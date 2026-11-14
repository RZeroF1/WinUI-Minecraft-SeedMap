/* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * Copyright (c) 2026 RZeroF1.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */
#include "terrain.h"
#include "upstream/noise.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
typedef struct
{
    int op, a[4];
    double p[4];
} TerrainNode;
typedef struct
{
    float location, derivative;
    int value;
} TerrainSplinePoint;
typedef struct
{
    uint64_t lo, hi;
    int first, count;
    double amplitudes[16];
} TerrainNoiseSpec;
#include "terrain_tables.h"

#define TN (sizeof(terrain_nodes) / sizeof(*terrain_nodes))
#define NN (sizeof(terrain_noise_specs) / sizeof(*terrain_noise_specs))
typedef struct
{
    int x, y, z, mode;
    double value;
    unsigned used;
} Entry;
typedef struct
{
    int level, type;
} Fluid;
typedef struct
{
    int x, y, z;
    Fluid fluid;
    int used;
} FluidEntry;
typedef struct
{
    int x, z, y, used;
} SurfaceEntry;
/* All mutable noise and memoization state belongs to one generator. The DAG
 * and profile tables are immutable; no seed data is shared between threads.
 * mode=0 is an unwrapped density context, mode=1 is the single-cell NoiseChunk
 * context used by getBaseColumn/getBaseHeight. */
struct Terrain
{
    DoublePerlinNoise noise[NN];
    PerlinNoise oct[NN][32];
    PerlinNoise blended[40];
    Xoroshiro aquifer;
    Entry cache[TN][64];
    unsigned cacheEpoch;
    int originX, originZ, early, profile;
    FluidEntry fluids[256];
    SurfaceEntry surface[256];
    SurfaceEntry aquiferTop[64];
};
static double bounded(double n, double lo, double hi)
{
    return n < lo ? lo : n > hi ? hi : n;
}
static float flerp(float f, float a, float b)
{
    return a + f * (b - a);
}
static float flerp3(float x, float y, float z, const float v[8])
{
    return flerp(z, flerp(y, flerp(x, v[0], v[1]), flerp(x, v[2], v[3])),
                    flerp(y, flerp(x, v[4], v[5]), flerp(x, v[6], v[7])));
}
static int divfloor(int n, int d)
{
    int q = n / d;
    return q - (n % d < 0);
}
static double wrap(double x)
{
    return x - floor(x / 33554432.0 + 0.5) * 33554432.0;
}
static const int grad[16][3] = {
    {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
    {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}, {1, 1, 0}, {0, -1, 1}, {-1, 1, 0}, {0, -1, -1}};
static double smooth(double t)
{
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
static double dot(int i, double x, double y, double z)
{
    const int *g = grad[i & 15];
    return g[0] * x + g[1] * y + g[2] * z;
}
/* Exact modern Perlin path keeps vanilla's epsilon and negative yMax semantics. */
static double perlin(const PerlinNoise *p, double x, double y, double z, double ys, double ym)
{
    x += p->a;
    y += p->b;
    z += p->c;
    int ix = (int)floor(x), iy = (int)floor(y), iz = (int)floor(z);
    x -= floor(x);
    y -= floor(y);
    z -= floor(z);
    double uy = y;
    if (ys != 0)
    {
        double m = ym >= 0 && ym < y ? ym : y;
        y -= floor(m / ys + (double)1.0e-7f) * ys;
    }
    const uint8_t *d = p->d;
    int a = d[ix & 255], b = d[(ix + 1) & 255], aa = d[(a + iy) & 255], ab = d[(a + iy + 1) & 255],
        ba = d[(b + iy) & 255], bb = d[(b + iy + 1) & 255];
    double v000 = dot(d[(aa + iz) & 255], x, y, z), v100 = dot(d[(ba + iz) & 255], x - 1, y, z);
    double v010 = dot(d[(ab + iz) & 255], x, y - 1, z),
           v110 = dot(d[(bb + iz) & 255], x - 1, y - 1, z);
    double v001 = dot(d[(aa + iz + 1) & 255], x, y, z - 1),
           v101 = dot(d[(ba + iz + 1) & 255], x - 1, y, z - 1);
    double v011 = dot(d[(ab + iz + 1) & 255], x, y - 1, z - 1),
           v111 = dot(d[(bb + iz + 1) & 255], x - 1, y - 1, z - 1);
    return lerp3(smooth(x), smooth(uy), smooth(z), v000, v100, v010, v110, v001, v101, v011, v111);
}
static int octaves(OctaveNoise *o, PerlinNoise *p, Xoroshiro *r, const TerrainNoiseSpec *spec)
{
    uint64_t lo = xNextLong(r), hi = xNextLong(r);
    int n = 0;
    double lf = pow(2.0, spec->first),
           vf = pow(2.0, spec->count - 1) / (pow(2.0, spec->count) - 1.0);
    for (int i = 0; i < spec->count; i++, lf *= 2, vf /= 2)
    {
        if (spec->amplitudes[i] == 0)
            continue;
        int j = spec->first + i + 16;
        Xoroshiro pr = {lo ^ terrain_octave_hashes[j][0], hi ^ terrain_octave_hashes[j][1]};
        xPerlinInit(p + n, &pr);
        p[n].lacunarity = lf;
        /* Java evaluates amplitude * noise * valueFactor at each octave.
         * Keep the two factors separate so the compiler cannot pre-round
         * amplitude * valueFactor before the noise multiplication. */
        p[n].amplitude = spec->amplitudes[i];
        p[n].d2 = vf;
        n++;
    }
    o->octaves = p;
    o->octcnt = n;
    return n;
}
static double octsample(const OctaveNoise *o, double x, double y, double z)
{
    double v = 0;
    for (int i = 0; i < o->octcnt; i++)
    {
        const PerlinNoise *p = o->octaves + i;
        double f = p->lacunarity;
        v += p->amplitude * perlin(p, wrap(x * f), wrap(y * f), wrap(z * f), 0, 0) * p->d2;
    }
    return v;
}
/* 26.3 uses float Perlin gradients and noise stacks. Volume sampling factors
 * the XZ gradient before adding Y; scalar sampling uses the XYZ dot order. */
static float perlin263(const PerlinNoise *p, double x, double y, double z,
                       double smear, int volume)
{
    double originalY = y;
    x = wrap(x) + p->a;
    y = wrap(y) + p->b;
    z = wrap(z) + p->c;
    int ix = (int)floor(x), iy = (int)floor(y), iz = (int)floor(z);
    float fx = (float)(x - floor(x)), fz = (float)(z - floor(z));
    double relativeY = y - floor(y);
    float uy = (float)relativeY;
    if (smear != 0)
    {
        double limit = originalY >= 0 && originalY < relativeY ? originalY : relativeY;
        relativeY -= floor(limit / smear + (double)1.0e-7f) * smear;
    }
    float fy = (float)relativeY, values[8];
    const uint8_t *d = p->d;
    for (int i = 0; i < 8; i++)
    {
        int a = d[(ix + (i & 1)) & 255];
        int b = d[(a + iy + ((i >> 1) & 1)) & 255];
        const int *g = grad[d[(b + iz + ((i >> 2) & 1)) & 255] & 15];
        float xx = fx - (i & 1), yy = fy - ((i >> 1) & 1), zz = fz - ((i >> 2) & 1);
        values[i] = volume ? (g[0] * xx + g[2] * zz) + g[1] * yy
                           : (g[0] * xx + g[1] * yy) + g[2] * zz;
    }
    float ax = fx * fx * fx * (fx * (fx * 6.0f - 15.0f) + 10.0f);
    float ay = uy * uy * uy * (uy * (uy * 6.0f - 15.0f) + 10.0f);
    float az = fz * fz * fz * (fz * (fz * 6.0f - 15.0f) + 10.0f);
    return flerp3(ax, ay, az, values);
}
static float normal263(Terrain *t, int n, double x, double y, double z,
                       int volume, double xzScale, double yScale)
{
    const DoublePerlinNoise *noise = t->noise + n;
    float value = 0;
    for (int i = 0; i < noise->octA.octcnt; i++)
        for (int b = 0; b < 2; b++)
        {
            const PerlinNoise *p = (b ? noise->octB.octaves : noise->octA.octaves) + i;
            double frequency = p->lacunarity * (b ? 1.0181268882175227 : 1.0);
            double xx = volume ? x * (xzScale * frequency) : x * frequency;
            double yy = volume ? y * (yScale * frequency) : y * frequency;
            double zz = volume ? z * (xzScale * frequency) : z * frequency;
            value += terrain263_noise_weights[n][i] * perlin263(p, xx, yy, zz, 0, volume);
        }
    return value;
}
static double normal(Terrain *t, int n, double x, double y, double z)
{
    const DoublePerlinNoise *p = t->noise + n;
    double f = 1.0181268882175227;
    return (octsample(&p->octA, x, y, z) + octsample(&p->octB, x * f, y * f, z * f)) * p->amplitude;
}
static double base(Terrain *t, int x, int y, int z)
{
    if (t->profile == TERRAIN_263)
    {
        float main = 0, mn = 0, mx = 0;
        double f = 1, xs = 684.412 * 0.25, ys = 684.412 * 0.125;
        for (int i = 0; i < 8; i++, f /= 2)
            main += (float)(0.05 / f) *
                perlin263(t->blended + 32 + i, x * (xs / 80.0 * f),
                          y * (ys / 160.0 * f), z * (xs / 80.0 * f), ys * 8.0 / 160.0 * f, 1);
        float q = (float)bounded(main + 0.5f, 0, 1);
        f = 1;
        for (int i = 0; i < 16; i++, f /= 2)
        {
            float weight = (float)(1.0 / 65536.0 / f);
            if (q < 1)
                mn += weight * perlin263(t->blended + i, x * (xs * f), y * (ys * f),
                                         z * (xs * f), ys * 8.0 * f, 1);
            if (q > 0)
                mx += weight * perlin263(t->blended + 16 + i, x * (xs * f), y * (ys * f),
                                         z * (xs * f), ys * 8.0 * f, 1);
        }
        return q == 0 ? mn : q == 1 ? mx : flerp(q, mn, mx);
    }
    double xx = x * (684.412 * 0.25), yy = y * (684.412 * 0.125), zz = z * (684.412 * 0.25);
    double fx = xx / 80.0, fy = yy / 160.0, fz = zz / 80.0, smear = (684.412 * 0.125) * 8.0;
    if (t->profile == TERRAIN_1182 || t->profile == TERRAIN_118)
    {
        xx = divfloor(x, 4) * 684.412;
        yy = divfloor(y, 8) * 684.412;
        zz = divfloor(z, 4) * 684.412;
        fx = divfloor(x, 4) * (684.412 / 80.0);
        fy = divfloor(y, 8) * (684.412 / 160.0);
        fz = divfloor(z, 4) * (684.412 / 80.0);
        smear = 684.412;
    }
    double main = 0, mn = 0, mx = 0, f = 1;
    for (int i = 0; i < 8; i++, f /= 2)
        main += perlin(t->blended + 32 + i, wrap(fx * f), wrap(fy * f), wrap(fz * f),
                       smear / 160.0 * f, fy * f) /
                f;
    double q = (main / 10.0 + 1.0) / 2.0;
    f = 1;
    for (int i = 0; i < 16; i++, f /= 2)
    {
        double a = wrap(xx * f), b = wrap(yy * f), c = wrap(zz * f);
        if (q < 1)
            mn += perlin(t->blended + i, a, b, c, smear * f, yy * f) / f;
        if (q > 0)
            mx += perlin(t->blended + 16 + i, a, b, c, smear * f, yy * f) / f;
    }
    return clampedLerp(q, mn / 512.0, mx / 512.0) / 128.0;
}
static double eval(Terrain *t, int n, int x, int y, int z, int mode);
static double slide(double n, int y)
{
    int cell = y / 8 + 8;
    double v = clampedLerp((48 - cell - 8) / 2.0, -0.078125, n);
    return clampedLerp(cell / 3.0, 0.1171875, v);
}
/* 1.18/1.18.1 pre-DensityFunction NoiseSampler, including the old cave graph. */
static double earlyNoise(Terrain *t, int x, int y, int z, int skip)
{
    double off = eval(t, TL_OFFSET, x, 0, z, 0), factor = eval(t, TL_FACTOR, x, 0, z, 0),
           jagged = 0;
    if (!skip)
    {
        double jag = eval(t, TL_JAGGEDNESS, x, 0, z, 0);
        if (jag != 0)
        {
            double n = normal(t, TI_JAGGED, x * 1500.0, 0, z * 1500.0);
            jagged = n > 0 ? jag * n : jag / 2.0 * n;
        }
    }
    double slope = (1.0 - y / 128.0 + off + jagged) * factor;
    slope *= slope > 0 ? 4 : 1;
    slope += skip ? -0.703125 : base(t, x, y, z);
    double out = slope;
    if (!skip && slope >= -64)
    {
        double entrance = normal(t, TI_CAVE_ENTRANCE, x * .75, y * .5, z * .75) + .37 +
                          clampedLerp((y + 10) / 40.0, .3, 0);
        double rough = (.4 - fabs(normal(t, TI_SPAGHETTI_ROUGHNESS, x, y, z))) *
                       lerp((normal(t, TI_SPAGHETTI_ROUGHNESS_MODULATOR, x, y, z) + 1) * .5, 0, .1);
        double n = normal(t, TI_SPAGHETTI_3D_RARITY, x * 2.0, y, z * 2.0), rar = n < -.5  ? .75
                                                                                 : n < 0  ? 1
                                                                                 : n < .5 ? 1.5
                                                                                          : 2;
        double thick = lerp((normal(t, TI_SPAGHETTI_3D_THICKNESS, x, y, z) + 1) * .5, .065, .088);
        double sp3 =
            fmax(fabs(rar * normal(t, TI_SPAGHETTI_3D_1, x / rar, y / rar, z / rar)) - thick,
                 fabs(rar * normal(t, TI_SPAGHETTI_3D_2, x / rar, y / rar, z / rar)) - thick);
        double entrances = fmin(entrance, bounded(sp3, -1, 1) + rough);
        if (slope < 1.5625)
            out = fmin(slope, entrances * 5);
        else
        {
            double layer = normal(t, TI_CAVE_LAYER, x, y * 8.0, z);
            layer = layer * layer * 4;
            double cheese = bounded(normal(t, TI_CAVE_CHEESE, x, y / 1.5, z) + .27, -1, 1) +
                            clampedLerp((slope - 1.5625) * 1.28, .5, 0) + layer;
            n = normal(t, TI_SPAGHETTI_2D_MODULATOR, x * 2.0, y, z * 2.0);
            rar = n < -.75 ? .5 : n < -.5 ? .75 : n < .5 ? 1 : n < .75 ? 2 : 3;
            thick =
                lerp((normal(t, TI_SPAGHETTI_2D_THICKNESS, x * 2.0, y, z * 2.0) + 1) * .5, .6, 1.3);
            double elevation =
                lerp((normal(t, TI_SPAGHETTI_2D_ELEVATION, x, 0, z) + 1) * .5, -8, 8);
            double dist = fabs(elevation - y / 8.0) - thick;
            dist = dist * dist * dist;
            double sp2 = bounded(
                fmax(dist, fabs(rar * normal(t, TI_SPAGHETTI_2D, x / rar, y / rar, z / rar)) -
                               .083 * thick),
                -1, 1);
            double rare = lerp((normal(t, TI_PILLAR_RARENESS, x, y, z) + 1) * .5, 0, 2),
                   width = lerp((normal(t, TI_PILLAR_THICKNESS, x, y, z) + 1) * .5, 0, 1.1);
            double pillar =
                pow(width, 3) * (normal(t, TI_PILLAR, x * 25.0, y * .3, z * 25.0) * 2 - rare);
            if (pillar <= .03)
                pillar = -INFINITY;
            out = fmax(fmin(cheese, fmin(entrances, sp2 + rough)), pillar);
        }
    }
    return bounded(slide(out, y), -64, 64);
}
static double earlyNoodle(Terrain *t, int id, int x, int y, int z, double scale, int fallback)
{
    int bx = divfloor(x, 4) * 4, by = divfloor(y, 8) * 8, bz = divfloor(z, 4) * 4;
    double v[8];
    for (int j = 0; j < 8; j++)
    {
        int xx = bx + (j & 1) * 4, yy = by + ((j >> 1) & 1) * 8, zz = bz + ((j >> 2) & 1) * 4;
        v[j] = yy < -60 || yy > 320 ? fallback : normal(t, id, xx * scale, yy * scale, zz * scale);
    }
    return lerp3((y - by) / 8.0, (x - bx) / 4.0, (z - bz) / 4.0, v[0], v[2], v[1], v[3], v[4], v[6],
                 v[5], v[7]);
}
static float spline(Terrain *t, const TerrainNode *p, int x, int y, int z, int mode)
{
    float f = (float)eval(t, p->a[0], x, y, z, mode);
    const TerrainSplinePoint *pts = terrain_splines + p->a[1];
    int count = p->a[2], i = -1;
    while (i + 1 < count && f >= pts[i + 1].location)
        i++;
    if (i < 0 || i == count - 1)
    {
        if (i < 0)
            i = 0;
        float v = (float)eval(t, pts[i].value, x, y, z, mode);
        return pts[i].derivative == 0 ? v : v + pts[i].derivative * (f - pts[i].location);
    }
    float a = pts[i].location, b = pts[i + 1].location, u = (f - a) / (b - a);
    float va = (float)eval(t, pts[i].value, x, y, z, mode),
          vb = (float)eval(t, pts[i + 1].value, x, y, z, mode);
    float da = pts[i].derivative * (b - a) - (vb - va),
          db = -pts[i + 1].derivative * (b - a) + (vb - va);
    return flerp(u, va, vb) + u * (1.0f - u) * flerp(u, da, db);
}
static double eval(Terrain *t, int n, int x, int y, int z, int mode)
{
    const TerrainNode *p = terrain_nodes + n;
    double a, b, v;
    if (mode && p->op == TD_FLAT_CACHE)
    {
        int qx = divfloor(x, 4), qz = divfloor(z, 4);
        if (qx >= t->originX / 4 && qx <= t->originX / 4 + 1 && qz >= t->originZ / 4 &&
            qz <= t->originZ / 4 + 1)
        {
            x = qx * 4;
            y = 0;
            z = qz * 4;
        }
    }
    unsigned hash =
        (unsigned)x * 73428767u ^ (unsigned)y * 912271u ^ (unsigned)z * 438289u ^ (unsigned)mode;
    hash ^= hash >> 16;
    hash ^= hash >> 8;
    hash &= 63;
    Entry *e = &t->cache[n][hash];
    if (e->used == (mode ? t->cacheEpoch : 1) && e->x == x && e->y == y && e->z == z && e->mode == mode)
        return e->value;
#define E(k) eval(t, p->a[k], x, y, z, mode)
    switch (p->op)
    {
    case TD_CONSTANT:
        v = p->p[0];
        break;
    case TD_ADD:
        v = E(0) + E(1);
        break;
    case TD_SUB:
        v = E(0) - E(1);
        break;
    case TD_DIV:
        v = E(0) / E(1);
        break;
    case TD_NEGATE:
        v = -E(0);
        break;
    case TD_Y:
        v = y;
        break;
    case TD_LERP: {
        float alpha = (float)E(0);
        v = alpha == 0 ? E(1) : alpha == 1 ? E(2) : flerp(alpha, (float)E(1), (float)E(2));
        break;
    }
    case TD_GRADIENT26: {
        float factor = ((float)p->p[3] - (float)p->p[2]) / (float)(p->p[1] - p->p[0]);
        int yy = y < p->p[0] ? (int)p->p[0] : y > p->p[1] ? (int)p->p[1] : y;
        v = (float)p->p[2] + (float)(yy - (int)p->p[0]) * factor;
        break;
    }
    case TD_MUL:
        a = E(0);
        v = a == 0 ? 0 : a * E(1);
        break;
    case TD_MIN:
        a = E(0);
        b = E(1);
        v = fmin(a, b);
        break;
    case TD_MAX:
        a = E(0);
        b = E(1);
        v = fmax(a, b);
        break;
    case TD_ABS:
        v = fabs(E(0));
        break;
    case TD_SQUARE:
        a = E(0);
        v = a * a;
        break;
    case TD_CUBE:
        a = E(0);
        v = t->profile == TERRAIN_263 ? (float)((float)a * (float)a) * (float)a : a * a * a;
        break;
    case TD_HALF_NEGATIVE:
        a = E(0);
        v = a > 0 ? a : a * 0.5;
        break;
    case TD_QUARTER_NEGATIVE:
        a = E(0);
        v = a > 0 ? a : a * 0.25;
        break;
    case TD_SQUEEZE:
        a = bounded(E(0), -1, 1);
        v = t->profile == TERRAIN_263
                ? (float)a / 2.0f - ((float)((float)a * (float)a) * (float)a) / 24.0f
                : a / 2.0 - a * a * a / 24.0;
        break;
    case TD_CLAMP:
        v = bounded(E(0), t->profile == TERRAIN_263 ? (float)p->p[0] : p->p[0],
                         t->profile == TERRAIN_263 ? (float)p->p[1] : p->p[1]);
        break;
    case TD_RANGE_CHOICE:
        a = E(0);
        v = a >= (t->profile == TERRAIN_263 ? (float)p->p[0] : p->p[0]) &&
            a < (t->profile == TERRAIN_263 ? (float)p->p[1] : p->p[1]) ? E(1) : E(2);
        break;
    case TD_Y_CLAMPED_GRADIENT:
        v = clampedLerp(((double)y - p->p[0]) / (p->p[1] - p->p[0]), p->p[2], p->p[3]);
        break;
    case TD_NOISE:
        v = t->profile == TERRAIN_263
                ? normal263(t, p->a[3], mode ? x : x * p->p[0],
                            mode ? y : y * p->p[1], mode ? z : z * p->p[0],
                            mode != 0, p->p[0], p->p[1])
                : normal(t, p->a[3], x * p->p[0], y * p->p[1], z * p->p[0]);
        break;
    case TD_SHIFT_A:
        v = t->profile == TERRAIN_263
                ? normal263(t, p->a[3], mode ? x : x * 0.25, 0, mode ? z : z * 0.25,
                            mode != 0, 0.25, 0) * 4.0f
                : normal(t, p->a[3], x * 0.25, 0, z * 0.25) * 4;
        break;
    case TD_SHIFT_B:
        v = t->profile == TERRAIN_263
                ? normal263(t, p->a[3], mode ? z : z * 0.25, mode ? x : x * 0.25, 0,
                            mode != 0, 0.25, 0.25) * 4.0f
                : normal(t, p->a[3], z * 0.25, x * 0.25, 0) * 4;
        break;
    case TD_SHIFTED_NOISE:
        a = E(0);
        b = E(1);
        v = t->profile == TERRAIN_263
                ? normal263(t, p->a[3], x * p->p[0] + a, y * p->p[1] + b,
                            z * p->p[0] + E(2), 0, 1, 1)
                : normal(t, p->a[3], x * p->p[0] + a, y * p->p[1] + b, z * p->p[0] + E(2));
        break;
    case TD_WEIRD_SCALED_SAMPLER:
        a = E(0);
        if (p->p[0] == 1)
            b = a < -.5 ? .75 : a < 0 ? 1 : a < .5 ? 1.5 : 2;
        else
            b = a < -.75 ? .5 : a < -.5 ? .75 : a < .5 ? 1 : a < .75 ? 2 : 3;
        v = b * fabs(normal(t, p->a[3], x / b, y / b, z / b));
        break;
    case TD_BLEND_ALPHA:
        v = 1;
        break;
    case TD_BLEND_OFFSET:
        v = 0;
        break;
    case TD_OLD_BLENDED_NOISE:
        v = base(t, x, y, z);
        break;
    case TD_SPLINE:
        v = spline(t, p, x, y, z, mode);
        break;
    case TD_FLOAT_ADD:
        v = (float)((float)E(0) + (float)E(1));
        break;
    case TD_FOLDED_RIDGE: {
        float r = (float)E(0);
        v = -(fabsf(fabsf(r) - 0.6666667f) - 0.33333334f) * 3.0f;
        break;
    }
    case TD_SLIDE:
        v = slide(E(0), y);
        break;
    case TD_INVERT:
        v = 1.0 / E(0);
        break;
    case TD_FIND_TOP_SURFACE: {
        int top = (int)floor(E(1) / p->p[1]) * (int)p->p[1], bottom = (int)p->p[0];
        v = bottom;
        for (int yy = top; yy >= bottom; yy -= (int)p->p[1])
            if (eval(t, p->a[0], x, yy, z, 0) > 0)
            {
                v = yy;
                break;
            }
        break;
    }
    case TD_INTERPOLATED:
        if (t->profile == TERRAIN_263)
        {
            int sx = (int)p->p[0], sy = (int)p->p[1];
            int bx = divfloor(x, sx) * sx, by = divfloor(y, sy) * sy, bz = divfloor(z, sx) * sx;
            float val[8];
            for (int j = 0; j < 8; j++)
                val[j] = (float)eval(t, p->a[0], bx + (j & 1) * sx,
                                    by + ((j >> 1) & 1) * sy, bz + ((j >> 2) & 1) * sx, 2);
            /* BaseColumn's sampleVolume: interpolate Z, then X, then accumulate Y. */
            float az = (float)(z - bz) / sx, ax = (float)(x - bx) / sx;
            float lo = flerp(ax, flerp(az, val[0], val[4]), flerp(az, val[1], val[5]));
            float hi = flerp(ax, flerp(az, val[2], val[6]), flerp(az, val[3], val[7]));
            float step = (hi - lo) * (1.0f / sy), value = lo;
            for (int yy = 0; yy < y - by; yy++)
                value += step;
            v = value;
            break;
        }
        if (mode)
        {
            int bx = divfloor(x, 4) * 4, by = divfloor(y, 8) * 8, bz = divfloor(z, 4) * 4;
            double val[8];
            for (int j = 0; j < 8; j++)
                val[j] = eval(t, p->a[0], bx + (j & 1) * 4, by + ((j >> 1) & 1) * 8,
                              bz + ((j >> 2) & 1) * 4, 0);
            v = lerp3((x - bx) / 4.0, (y - by) / 8.0, (z - bz) / 4.0, val[0], val[1], val[2],
                      val[3], val[4], val[5], val[6], val[7]);
            break;
        }
    default:
        v = E(0);
        break;
    }
#undef E
    if (t->profile == TERRAIN_263)
        v = (float)v;
    e->used = mode ? t->cacheEpoch : 1;
    e->x = x;
    e->y = y;
    e->z = z;
    e->mode = mode;
    e->value = v;
    return v;
}
Terrain *terrain_create(uint64_t seed, int profile)
{
    if (profile < TERRAIN_119 || profile > TERRAIN_263)
        return NULL;
    Terrain *t = calloc(1, sizeof(*t));
    if (!t)
        return 0;
    Xoroshiro r;
    xSetSeed(&r, seed);
    uint64_t lo = xNextLong(&r), hi = xNextLong(&r);
    for (int i = 0; i < NN; i++)
    {
        const TerrainNoiseSpec *s = terrain_noise_specs + i;
        Xoroshiro nr = {lo ^ s->lo, hi ^ s->hi};
        int n = octaves(&t->noise[i].octA, t->oct[i], &nr, s);
        octaves(&t->noise[i].octB, t->oct[i] + n, &nr, s);
        int first = 0, last = s->count - 1;
        while (s->amplitudes[first] == 0)
            first++;
        while (s->amplitudes[last] == 0)
            last--;
        t->noise[i].amplitude = (1.0 / 6.0) / (0.1 * (1.0 + 1.0 / (last - first + 1)));
    }
    Xoroshiro br = {lo ^ 0x1ee555222ef96f14ULL, hi ^ 0xe2bedfdbebe43d33ULL};
    for (int i = 0; i < 40; i++)
        xPerlinInit(t->blended + i, &br);
    Xoroshiro ar = {lo ^ 0x7bb15cc403c6ace6ULL, hi ^ 0x0bdd56bc9d232691ULL};
    t->aquifer.lo = xNextLong(&ar);
    t->aquifer.hi = xNextLong(&ar);
    t->cacheEpoch = 1;
    t->profile = profile;
    t->early = profile == TERRAIN_1182 || profile == TERRAIN_118;
    return t;
}
void terrain_destroy(Terrain *t)
{
    free(t);
}
static int preliminary(Terrain *t, int x, int z)
{
    x = divfloor(x, 4) * 4;
    z = divfloor(z, 4) * 4;
    unsigned h = ((unsigned)x * 543217u ^ (unsigned)z * 812521u) & 255;
    SurfaceEntry *e = t->surface + h;
    if (e->used && e->x == x && e->z == z)
        return e->y;
    int y;
    if (t->profile == TERRAIN_1219 || t->profile >= TERRAIN_261)
    {
        int root = t->profile == TERRAIN_261 ? T261_PRELIMINARY_SURFACE_LEVEL
                 : t->profile == TERRAIN_262 ? T262_PRELIMINARY_SURFACE_LEVEL
                 : t->profile == TERRAIN_263 ? T263_SURFACE_LEVEL
                                            : TR_PRELIMINARY_SURFACE_LEVEL;
        y = (int)eval(t, root, x, 0, z, 0);
    }
    else if (t->profile == TERRAIN_118)
    {
        for (y = 320; y >= -64; y -= 8)
            if (earlyNoise(t, x, y, z, 1) > .390625)
                break;
    }
    else
        for (y = 320; y >= -64; y -= 8)
        {
            double d = eval(t,
                            t->profile == TERRAIN_1182 ? TL_INITIAL_DENSITY_WITHOUT_JAGGEDNESS
                                                       : TR_INITIAL_DENSITY_WITHOUT_JAGGEDNESS,
                            x, y, z, 0);
            if (t->profile == TERRAIN_1182)
                d = slide(bounded(d - 0.703125, -64, 64), y);
            if (d > 0.390625)
                break;
        }
    if (y < -64)
        y = 2147483647;
    e->x = x;
    e->z = z;
    e->y = y;
    e->used = 1;
    return y;
}
static Fluid global(int y)
{
    Fluid f = {y < -54 ? -54 : 63, y < -54 ? 3 : 2};
    return f;
}
static int fluidat(Fluid f, int y)
{
    return y < f.level ? f.type : 0;
}
static Fluid fluid(Terrain *t, int x, int y, int z)
{
    unsigned h = ((unsigned)x * 74237u ^ (unsigned)y * 78497u ^ (unsigned)z * 65713u) & 255;
    FluidEntry *entry = t->fluids + h;
    if (entry->used && entry->x == x && entry->y == y && entry->z == z)
        return entry->fluid;
    Fluid g = global(y), f = g;
    int surface = 2147483647, has = 0;
    static const int offsets[13][2] = {{0, 0},  {-2, -1}, {-1, -1}, {0, -1}, {1, -1},
                                       {-3, 0}, {-2, 0},  {-1, 0},  {1, 0},  {-2, 1},
                                       {-1, 1}, {0, 1},   {1, 1}};
    for (int oi = 0; oi < 13; oi++)
    {
        int i = t->profile == TERRAIN_118 ? (oi < 7 ? oi + 1 : oi == 7 ? 0 : oi) : oi;
        int xx = x + offsets[i][0] * 16, zz = z + offsets[i][1] * 16, s = preliminary(t, xx, zz),
            a = s + 8, center = i == 0;
        if (center && y - 12 > a)
            goto done;
        int near = y + 12 > a;
        Fluid sample = global(a);
        if ((near || center) && fluidat(sample, a) != 0)
        {
            if (center)
                has = 1;
            if (near)
            {
                f = sample;
                goto done;
            }
        }
        if (s < surface)
            surface = s;
    }
    double d1, d2;
    int depthRoot = t->profile == TERRAIN_261 ? T261_DEPTH
                  : t->profile == TERRAIN_262 ? T262_DEPTH : TR_DEPTH;
    if (t->profile == TERRAIN_263 ? eval(t, T263_EXCLUSION, x, y, z, 0) > 0
        : !t->early && eval(t, TR_EROSION, x, y, z, 0) < (double)-0.225f &&
          eval(t, depthRoot, x, y, z, 0) > (double)0.9f)
        d1 = d2 = -1;
    else
    {
        double m = has ? clampedLerp((surface + 8 - y) / 64.0, 1.0, 0.0) : 0;
        double noise = bounded(eval(t, TR_FLUID_LEVEL_FLOODEDNESS, x, y, z, 0), -1, 1);
        double upper = lerp((m - 1.0) / -1.0, -0.3, 0.8), lower = lerp((m - 1.0) / -1.0, -0.8, 0.4);
        d1 = noise - upper;
        d2 = noise - lower;
    }
    int randomizedLevel = -32512;
    if (d1 > 0)
        f.level = g.level;
    else if (d2 > 0)
    {
        int gx = divfloor(x, 16), gy = divfloor(y, 40), gz = divfloor(z, 16);
        double spread = t->profile == TERRAIN_118
                            ? normal(t, TI_AQUIFER_FLUID_LEVEL_SPREAD, gx, gy / 1.4, gz)
                            : eval(t, TR_FLUID_LEVEL_SPREAD, gx, gy, gz, 0);
        double n = t->profile == TERRAIN_263 ? (float)spread * 10.0f : spread * 10.0;
        int nlevel = gy * 40 + 20 + (int)floor(n / 3.0) * 3;
        randomizedLevel = nlevel;
        f.level = surface < nlevel ? surface : nlevel;
    }
    else
        f.level = -32512;
    /* Before 1.19, lava selection tests the randomized level before clamping
     * to the neighboring preliminary surface. Later profiles test the result. */
    int lavaLevel = t->early ? randomizedLevel : f.level;
    if (lavaLevel <= -10 && lavaLevel != -32512 && g.type != 3)
    {
        double n = eval(t, TR_LAVA, divfloor(x, 64), divfloor(y, 40), divfloor(z, 64), 0);
        if (fabs(n) > 0.3)
            f.type = 3;
    }
done:
    entry->used = 1;
    entry->x = x;
    entry->y = y;
    entry->z = z;
    entry->fluid = f;
    return f;
}
static double pressure(Terrain *t, int x, int y, int z, Fluid a, Fluid b, double *barrier)
{
    int ta = fluidat(a, y), tb = fluidat(b, y);
    if ((ta == 2 && tb == 3) || (ta == 3 && tb == 2))
        return 2;
    int delta = abs(a.level - b.level);
    if (delta == 0)
        return 0;
    double mid = 0.5 * (a.level + b.level), dy = y + 0.5 - mid, d = delta / 2.0 - fabs(dy), v;
    if (dy > 0)
        v = d > 0 ? d / 1.5 : d / 2.5;
    else
    {
        d = 3.0 + d;
        v = d > 0 ? d / 3.0 : d / 10.0;
    }
    double n = 0;
    if (v >= -2 && v <= 2)
    {
        if (isnan(*barrier))
            *barrier = eval(t, TR_BARRIER, x, y, z, 0);
        n = *barrier;
    }
    return 2.0 * (n + v);
}
/* 1.21.9+ bounds Aquifer sampling from all preliminary columns around its
 * owning chunk. This bound is immutable for a seed/profile/chunk and separate
 * from the current base-column FlatCache origin. */
static int aquifer_top(Terrain *t, int x, int z)
{
    int cx = divfloor(x, 16), cz = divfloor(z, 16);
    if (t->profile == TERRAIN_263)
    {
        cx = x;
        cz = z;
    }
    unsigned hash = ((unsigned)cx * 543217u ^ (unsigned)cz * 812521u) & 63;
    SurfaceEntry *e = t->aquiferTop + hash;
    if (e->used && e->x == cx && e->z == cz)
        return e->y;
    int top = -64;
    int minX = t->profile == TERRAIN_263 ? divfloor(divfloor(x - 5, 16) * 16, 4) * 4 : cx * 16 - 16;
    int minZ = t->profile == TERRAIN_263 ? divfloor(divfloor(z - 5, 16) * 16, 4) * 4 : cz * 16 - 16;
    int maxX = t->profile == TERRAIN_263 ? (divfloor(x - 5, 16) + 1) * 16 + 9 : cx * 16 + 25;
    int maxZ = t->profile == TERRAIN_263 ? (divfloor(z - 5, 16) + 1) * 16 + 9 : cz * 16 + 25;
    for (int zz = minZ; zz <= maxZ; zz += 4)
        for (int xx = minX; xx <= maxX; xx += 4)
        {
            int surface = preliminary(t, xx, zz);
            if (surface > top)
                top = surface;
        }
    e->x = cx;
    e->z = cz;
    e->y = (divfloor(top + 8 + 12, 12) + 1) * 12 + 10;
    e->used = 1;
    return e->y;
}
static int substance(Terrain *t, int x, int y, int z, double density)
{
    if (density > 0)
        return 1;
    if ((t->profile == TERRAIN_1219 || t->profile >= TERRAIN_261) && y > aquifer_top(t, x, z))
        return fluidat(global(y), y);
    if (y < -54)
        return 3;
    int gx = divfloor(x - 5, 16), gy = divfloor(y + 1, 12), gz = divfloor(z - 5, 16);
    int distances[3] = {2147483647, 2147483647, 2147483647}, positions[3][3] = {{0}};
    for (int ix = 0; ix <= 1; ix++)
        for (int iy = -1; iy <= 1; iy++)
            for (int iz = 0; iz <= 1; iz++)
            {
                int a = gx + ix, b = gy + iy, c = gz + iz;
                uint64_t s = (uint64_t)(int64_t)(int32_t)((uint32_t)a * 3129871u) ^
                             (uint64_t)(int64_t)c * 116129781ULL ^ (uint64_t)(int64_t)b;
                s = s * s * 42317861ULL + s * 11ULL;
                s = (uint64_t)((int64_t)s >> 16);
                Xoroshiro r = {s ^ t->aquifer.lo, t->aquifer.hi};
                int px = a * 16 + xNextInt(&r, 10), py = b * 12 + xNextInt(&r, 9),
                    pz = c * 16 + xNextInt(&r, 10);
                int dx = px - x, dy = py - y, dz = pz - z, d = dx * dx + dy * dy + dz * dz;
                for (int j = 0; j < 3; j++)
                    if (distances[j] >= d)
                    {
                        for (int k = 2; k > j; k--)
                        {
                            distances[k] = distances[k - 1];
                            memcpy(positions[k], positions[k - 1], sizeof(positions[k]));
                        }
                        distances[j] = d;
                        positions[j][0] = px;
                        positions[j][1] = py;
                        positions[j][2] = pz;
                        break;
                    }
            }
    Fluid f[3];
    f[0] = fluid(t, positions[0][0], positions[0][1], positions[0][2]);
    int result = fluidat(f[0], y);
    double sim01 = 1.0 - abs(distances[1] - distances[0]) / 25.0;
    if (t->profile == TERRAIN_118)
    {
        double sim02 = 1.0 - abs(distances[2] - distances[0]) / 25.0,
               sim12 = 1.0 - abs(distances[2] - distances[1]) / 25.0, v = 0, barrier = NAN;
        f[1] = fluid(t, positions[1][0], positions[1][1], positions[1][2]);
        f[2] = fluid(t, positions[2][0], positions[2][1], positions[2][2]);
        if (result == 2 && y - 1 < -54)
            v = 1;
        else if (sim01 > -1)
        {
            double p01 = pressure(t, x, y, z, f[0], f[1], &barrier) / 2,
                   p02 = pressure(t, x, y, z, f[0], f[2], &barrier) / 2,
                   p12 = pressure(t, x, y, z, f[1], f[2], &barrier) / 2;
            v = fmax(0, 2 * fmax(0, sim01) *
                            fmax(p01, fmax(p02 * fmax(0, sim02), p12 * fmax(0, sim12))));
        }
        return density + v <= 0 ? result : 1;
    }
    if (sim01 <= 0)
        return result;
    if (result == 2 && y - 1 < -54)
        return result;
    double barrier = NAN;
    f[1] = fluid(t, positions[1][0], positions[1][1], positions[1][2]);
    if (density + sim01 * pressure(t, x, y, z, f[0], f[1], &barrier) > 0)
        return 1;
    f[2] = fluid(t, positions[2][0], positions[2][1], positions[2][2]);
    double sim02 = 1.0 - abs(distances[2] - distances[0]) / 25.0,
           sim12 = 1.0 - abs(distances[2] - distances[1]) / 25.0;
    if (sim02 > 0 && density + sim01 * sim02 * pressure(t, x, y, z, f[0], f[2], &barrier) > 0)
        return 1;
    if (sim12 > 0 && density + sim01 * sim12 * pressure(t, x, y, z, f[1], f[2], &barrier) > 0)
        return 1;
    return result;
}
static int scan_column(Terrain *t, int x, int z, int type, int *column, int minY, int maxY)
{
    const int profile = t->profile;
    int ox = divfloor(x, 4) * 4, oz = divfloor(z, 4) * 4;
    if (ox != t->originX || oz != t->originZ)
    {
        /* Wrapped FlatCache results depend on this single-cell origin. */
        if (++t->cacheEpoch == 0)
        {
            memset(t->cache, 0, sizeof(t->cache));
            t->cacheEpoch = 1;
        }
    }
    t->originX = ox;
    t->originZ = oz;

    for (int y = maxY; y >= minY; y--)
    {
        double density;
        if (profile == TERRAIN_118)
        {
            int bx = divfloor(x, 4) * 4, by = divfloor(y, 8) * 8, bz = divfloor(z, 4) * 4;
            double v[8];
            for (int j = 0; j < 8; j++)
                v[j] = earlyNoise(t, bx + (j & 1) * 4, by + ((j >> 1) & 1) * 8,
                                  bz + ((j >> 2) & 1) * 4, 0);
            double raw = lerp3((y - by) / 8.0, (x - bx) / 4.0, (z - bz) / 4.0, v[0], v[2], v[1],
                               v[3], v[4], v[6], v[5], v[7]);
            density = bounded(raw * .64, -1, 1);
            density = density / 2 - density * density * density / 24;
            if (earlyNoodle(t, TI_NOODLE, x, y, z, 1, -1) >= 0)
            {
                double width = clampedLerp(
                    (earlyNoodle(t, TI_NOODLE_THICKNESS, x, y, z, 1, 0) + 1) / 2, .05, .1);
                double a =
                    fabs(1.5 * earlyNoodle(t, TI_NOODLE_RIDGE_A, x, y, z, 2.6666666666666665, 0)) -
                    width;
                double b =
                    fabs(1.5 * earlyNoodle(t, TI_NOODLE_RIDGE_B, x, y, z, 2.6666666666666665, 0)) -
                    width;
                density = fmin(density, fmax(a, b));
            }
            if (raw <= -64)
            {
                int block = fluidat(global(y), y);
                if (column)
                    column[y - minY] = block;
                else if (type == 0 ? block != 0 : block == 1)
                    return y + 1;
                continue;
            }
        }
        else
            density = eval(t, profile == TERRAIN_1182 ? TL_FINAL_DENSITY
                            : profile == TERRAIN_261 ? T261_FINAL_DENSITY
                            : profile == TERRAIN_262 ? T262_FINAL_DENSITY
                            : profile == TERRAIN_263 ? T263_FINAL_DENSITY
                                                    : TR_FINAL_DENSITY, x, y, z, 1);
        int block = substance(t, x, y, z, density);
        if (column)
            column[y - minY] = block;
        else if (type == 0 ? block != 0 : block == 1)
            return y + 1;
    }
    return -64;
}
int terrain_column(Terrain *t, int x, int z, int type, int *column)
{
    return scan_column(t, x, z, type, column, -64, 319);
}
void terrain_column_range(Terrain *t, int x, int z, int minY, int maxY, int *column)
{
    scan_column(t, x, z, 0, column, minY, maxY);
}
int terrain_height(Terrain *t, int x, int z, int type)
{
    return scan_column(t, x, z, type, NULL, -64, 319);
}
