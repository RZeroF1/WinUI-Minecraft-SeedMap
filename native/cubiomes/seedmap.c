/* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * Copyright (c) 2026 RZeroF1.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#include "seedmap.h"
#include "terrain.h"
#include "upstream/finders.h"
#include "upstream/util.h"
#include <stdlib.h>
#include <string.h>

static const cm_version versions[] = {
    {100,7,"1.2.5"},
    {101,7,"1.3.1"},
    {102,7,"1.3.2"},
    {103,7,"1.4.2"},
    {104,7,"1.4.4"},
    {105,7,"1.4.5"},
    {106,7,"1.4.6"},
    {107,7,"1.4.7"},
    {108,7,"1.5.1"},
    {109,7,"1.5.2"},
    {110,7,"1.6.1"},
    {111,7,"1.6.2"},
    {112,7,"1.6.4"},
    {113,7,"1.7.2"},
    {114,7,"1.7.3"},
    {115,7,"1.7.4"},
    {116,7,"1.7.5"},
    {117,7,"1.7.6"},
    {118,7,"1.7.7"},
    {119,7,"1.7.8"},
    {120,7,"1.7.9"},
    {121,7,"1.7.10"},
    {122,7,"1.8"},
    {123,7,"1.8.1"},
    {124,7,"1.8.2"},
    {125,7,"1.8.3"},
    {126,7,"1.8.4"},
    {127,7,"1.8.5"},
    {128,7,"1.8.6"},
    {129,7,"1.8.7"},
    {130,7,"1.8.8"},
    {131,7,"1.8.9"},
    {132,7,"1.9"},
    {133,7,"1.9.1"},
    {134,7,"1.9.2"},
    {135,7,"1.9.3"},
    {136,7,"1.9.4"},
    {137,7,"1.10"},
    {138,7,"1.10.1"},
    {139,7,"1.10.2"},
    {140,7,"1.11"},
    {141,7,"1.11.1"},
    {142,7,"1.11.2"},
    {143,7,"1.12"},
    {144,7,"1.12.1"},
    {145,7,"1.12.2"},
    {146,7,"1.21.5-1.21.8-strongholds"},
    {147,7,"1.14"},
    {148,7,"1.18"},
    {149,7,"26.1"}, {150,7,"26.2"}, {151,7,"26.3"},
    {MC_1_21_5,7,"1.21.5"}, {MC_1_21_11,7,"1.21.11"}, {MC_1_21_WD,7,"1.21.4"},
    {MC_1_21_3,7,"1.21.3"}, {MC_1_21_1,7,"1.21.1"},
    {MC_1_20_6,7,"1.20.6"}, {MC_1_19_4,7,"1.19.4"},
    {MC_1_19_2,7,"1.19.2"}, {MC_1_18_2,7,"1.18.2"},
    {MC_1_17_1,7,"1.17.1"}, {MC_1_16_5,7,"1.16.5"},
    {MC_1_16_1,7,"1.16.1"}, {MC_1_15_2,7,"1.15.2"},
    {MC_1_14_4,7,"1.14.4"}, {MC_1_13_2,7,"1.13.2"}
};
/* Stable ABI release IDs are independent of Cubiomes enum values. */
static const int legacy_profiles[] = { MC_1_2, MC_1_3, MC_1_3, MC_1_4, MC_1_4, MC_1_4, MC_1_4, MC_1_4, MC_1_5, MC_1_5, MC_1_6, MC_1_6, MC_1_6, MC_1_7, MC_1_7, MC_1_7, MC_1_7, MC_1_7, MC_1_7, MC_1_7, MC_1_7, MC_1_7, MC_1_8, MC_1_8, MC_1_8, MC_1_8, MC_1_8, MC_1_8, MC_1_8, MC_1_8, MC_1_8, MC_1_8, MC_1_9, MC_1_9, MC_1_9, MC_1_9, MC_1_9, MC_1_10, MC_1_10, MC_1_10, MC_1_11, MC_1_11, MC_1_11, MC_1_12, MC_1_12, MC_1_12 };
static int profile_version(int id) {
    if (id == 149) return MC_26_1;
    if (id == 150) return MC_26_2;
    if (id == 151) return MC_26_3;
    if (id == 146) return MC_1_21_5;
    if (id == 147) return MC_1_14;
    if (id == 148) return MC_1_18;
    return id >= 100 && id < 146 ? legacy_profiles[id-100] : id;
}
enum { RELEASE_1_4_2 = 103 };
typedef struct {
    Generator generator; /* First member preserves the existing internal casts. */
    Terrain *terrain;
    int terrain_profile;
} SeedMapHandle;
static uint32_t profile_flags(int id) {
    if (id == RELEASE_1_4_2) return LEGACY_TEMPLE_142;
    if (id == 146) return STRONGHOLD_NO_CHERRY;
    if (id == 147) return OUTPOST_NO_SNOW;
    if (id == 148) return UNCLAMPED_DEPTH | EARLY_118_STRUCTURES;
    return 0;
}
static int structure_config(int type,int mc,uint32_t flags,StructureConfig *config) {
    if(!getStructureConfig(type,mc,config)) return 0;
    if(type==Ruined_Portal_N && (flags & EARLY_118_STRUCTURES)) {
        // Cubiomes' MC_1_18 aliases 1.18.2. The exact release profile retains
        // the earlier Nether-only 25/10 random-spread settings.
        config->regionSize=25;
        config->chunkRange=15;
    }
    return 1;
}
static int valid_version(int mc) {
    for (int i=0;i<(int)(sizeof(versions)/sizeof(*versions));i++)
        if (versions[i].id==mc) return 1;
    return 0;
}
static int valid_type(int type) {
    switch (type) {
    case Desert_Pyramid: case Jungle_Temple: case Swamp_Hut: case Igloo:
    case Village: case Ocean_Ruin: case Shipwreck: case Monument: case Mansion:
    case Outpost: case Ruined_Portal: case Ruined_Portal_N: case Ancient_City:
    case Treasure: case Mineshaft: case Fortress: case Bastion: case End_City:
    case Trail_Ruins: case Trial_Chambers: case Abandoned_Camp: return 1;
    default: return 0;
    }
}
static int64_t floor_div(int64_t a, int b) {
    int64_t q=a/b, r=a%b; return q-(r<0);
}
int cm_abi_version(void) { return 1; }
const char *cm_source_revision(void) { return "ca9fcb4e2b4f29a88b15a19fe1f00a73bf503ec2+legacy46.1+modern43.3+terrain27.1+java26.1"; }
int cm_versions(cm_version *out, int capacity) {
    int n=(int)(sizeof(versions)/sizeof(*versions));
    if (!out) return n;
    if (capacity<n) return CM_CAPACITY;
    memcpy(out,versions,sizeof(versions)); return n;
}
int cm_structures(int mc, int dim, cm_structure *out, int capacity) {
    if (!valid_version(mc) || dim < -1 || dim > 1) return CM_UNSUPPORTED;
    uint32_t flags=profile_flags(mc);
    mc=profile_version(mc);
    int n=0;
    for(int type=0;type<FEATURE_NUM;type++) {
        StructureConfig c;
        if (!valid_type(type)) continue;
        if (type==Mineshaft) {
            if(dim!=DIM_OVERWORLD) continue;
            c.dim=DIM_OVERWORLD; c.regionSize=1;
        } else if(!structure_config(type,mc,flags,&c) || c.dim!=dim) continue;
        if (out) {
            if(n>=capacity) return CM_CAPACITY;
            out[n]=(cm_structure){type,dim,16*c.regionSize,1,struct2str(type)};
        }
        n++;
    }
    if(dim==DIM_OVERWORLD) {
        if(out) {
            if(n>=capacity) return CM_CAPACITY;
            out[n]=(cm_structure){100,dim,0,1,"stronghold"};
        }
        n++;
    }
    return n;
}
const char *cm_biome_name(int mc,int dim,int id) {
    if(!valid_version(mc)) return NULL;
    mc=profile_version(mc);
    if(mc<=MC_1_12 && dim==DIM_END && id!=the_end) return NULL;
    if(id<0 || id>255 || !biomeExists(mc,id) ||
        getDimension(id)!=dim || (dim==0 && !isOverworld(mc,id))) return NULL;
    return biome2str(mc,id);
}
uint32_t cm_biome_color(int id) {
    unsigned char colors[256][3];
    initBiomeColors(colors);
    if(id<0 || id>255) return 0xff555555;
    return 0xff000000u | (colors[id][0]<<16) | (colors[id][1]<<8) | colors[id][2];
}
int cm_create(int mc,uint64_t seed,int dim,void **out) {
    if(!out) return CM_INVALID;
    *out = NULL;
    if (!valid_version(mc) || dim < -1 || dim > 1)
        return CM_UNSUPPORTED;
    SeedMapHandle *owner = (SeedMapHandle *)calloc(1, sizeof(SeedMapHandle));
    if (!owner)
        return CM_MEMORY;
    Generator *g = &owner->generator;
    owner->terrain_profile = mc == 149 ? TERRAIN_261
                             : mc == 150 ? TERRAIN_262
                             : mc == 151 ? TERRAIN_263
                             : mc == 148         ? TERRAIN_118
                             : mc == MC_1_18_2 ? TERRAIN_1182
                             : mc == MC_1_21_5 ? TERRAIN_1219
                                               : TERRAIN_119;
    uint32_t flags = profile_flags(mc);
    setupGenerator(g, profile_version(mc), flags);
    applySeed(g, dim, seed);
    *out = g;
    return CM_OK;
}
void cm_destroy(void *handle)
{
    if (handle)
        terrain_destroy(((SeedMapHandle *)handle)->terrain);
    free(handle);
}
static int ensure_terrain(SeedMapHandle *owner)
{
    if (owner->generator.dim != DIM_OVERWORLD || owner->generator.mc < MC_1_18)
        return CM_UNSUPPORTED;
    if (!owner->terrain)
        owner->terrain = terrain_create(owner->generator.seed, owner->terrain_profile);
    return owner->terrain ? CM_OK : CM_MEMORY;
}
int cm_terrain_heights(void *handle, int type, int64_t x, int64_t z, int w, int h, int step,
                       int32_t *out, int capacity)
{
    SeedMapHandle *owner = (SeedMapHandle *)handle;
    if (!owner || !out || w <= 0 || h <= 0 || w > 4096 || h > 4096 || (int64_t)w * h > 4096 ||
        capacity < (int64_t)w * h)
        return CM_INVALID;
    if (type < 0 || type > 1)
        return CM_UNSUPPORTED;
    if (step <= 0 || step > 30000000 || x < -30000000 || z < -30000000 || x >= 30000000 ||
        z >= 30000000 || x + (int64_t)(w - 1) * step >= 30000000 ||
        z + (int64_t)(h - 1) * step >= 30000000)
        return CM_RANGE;
    int rc = ensure_terrain(owner);
    if (rc < 0)
        return rc;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            out[j * w + i] = terrain_height(owner->terrain, (int)(x + (int64_t)i * step),
                                            (int)(z + (int64_t)j * step), type);
    return CM_OK;
}
static int check_biome_range(int64_t x, int64_t z, int w, int h, int step, int y)
{
    if (step != 1 && step != 4 && step != 16 && step != 64 && step != 256)
        return CM_UNSUPPORTED;
    if(x%step || z%step || x < -30000000 || z < -30000000 || x>=30000000 || z>=30000000 ||
       x+(int64_t)(w-1)*step>=30000000 || z+(int64_t)(h-1)*step>=30000000 || y < -64 || y>320)
        return CM_RANGE;
    return CM_OK;
}
int cm_terrain_column(void *handle, int x, int z, int32_t *out, int capacity)
{
    SeedMapHandle *owner = (SeedMapHandle *)handle;
    if (!owner || !out)
        return CM_INVALID;
    if (capacity < 384)
        return CM_CAPACITY;
    if (x < -30000000 || z < -30000000 || x >= 30000000 || z >= 30000000)
        return CM_RANGE;
    int rc = ensure_terrain(owner);
    if (rc < 0)
        return rc;
    terrain_column_range(owner->terrain, x, z, -64, 319, out);
    return CM_OK;
}
int cm_biomes(void *handle,int64_t x,int64_t z,int w,int h,int step,int y,int32_t *out,int capacity) {
    Generator *g=(Generator*)handle;
    if(!g || !out || w<=0 || h<=0 || w>512 || h>512 || capacity<w*h) return CM_INVALID;
    int check=check_biome_range(x,z,w,h,step,y);
    if(check!=CM_OK) return check;
    Range r={step,(int)(x/step),(int)(z/step),w,h,
             step==1?y:(int)floor_div(y,4),1};
    int *cache=allocCache(g,r);
    if(!cache) return CM_MEMORY;
    int rc=genBiomes(g,cache,r);
    if(!rc) {
        // Pre-1.13 End terrain categories are internal; vanilla exposes ID 9.
        if(g->mc<=MC_1_12 && g->dim==DIM_END)
            for(int i=0;i<w*h;i++) out[i]=the_end;
        else memcpy(out,cache,(size_t)w*h*sizeof(int32_t));
    }
    free(cache);
    return rc?CM_GENERATION:CM_OK;
}
int cm_biome_sample(void *handle,int64_t x,int64_t z,int step,int y,
                    uint64_t *search_state,int32_t *out) {
    Generator *g=(Generator*)handle;
    if(!g || !search_state || !out) return CM_INVALID;
    int check=check_biome_range(x,z,1,1,step,y);
    if(check!=CM_OK) return check;
    if(g->dim==DIM_END || g->mc<MC_1_18)
        return cm_biomes(handle,x,z,1,1,step,y,out,1);
    int qx,qy,qz;
    if(step==1)
        voronoiAccess3D(g->sha,(int)x,y,(int)z,&qx,&qy,&qz);
    else {
        int offset=g->dim==DIM_OVERWORLD && step>4?step/2:0;
        qx=(int)floor_div(x+offset,4);
        qy=(int)floor_div(y,4);
        qz=(int)floor_div(z+offset,4);
    }
    // Vanilla's RTree retains the previous nearest leaf on an equal-distance
    // search. The explicit state makes this ordered operation independent of
    // deterministic, order-independent map tiles.
    if(g->dim==DIM_NETHER)
        *out=getNetherBiomeNoise(&g->nn,qx,qz,search_state);
    else
        *out=sampleBiomeNoise(&g->bn,NULL,qx,qy,qz,search_state,0);
    return CM_OK;
}
/* GenerationStub position predicates use noise-biome quart coordinates,
 * independently of the caller's ordered biome-query history. */
static int modern_generation_point(SeedMapHandle *owner, int type, int x, int z)
{
    Generator *g = &owner->generator;
    int rc = ensure_terrain(owner);
    if (rc < 0)
        return rc;
    if (type == Mansion)
    {
        uint64_t rng = chunkGenerateRnd(g->seed, x >> 4, z >> 4);
        int rotation = nextInt(&rng, 4), dx = (rotation == 1 || rotation == 2) ? -5 : 5,
            dz = rotation >= 2 ? -5 : 5;
        int px = (x >> 4) * 16 + 7, pz = (z >> 4) * 16 + 7;
        int y = terrain_height(owner->terrain, px, pz, 0) - 1;
        int h = terrain_height(owner->terrain, px + dx, pz, 0) - 1;
        if (h < y)
            y = h;
        h = terrain_height(owner->terrain, px, pz + dz, 0) - 1;
        if (h < y)
            y = h;
        h = terrain_height(owner->terrain, px + dx, pz + dz, 0) - 1;
        if (h < y)
            y = h;
        if (y < 60)
            return 0;
        int biome = getBiomeAt(g, 4, px >> 2, y >> 2, pz >> 2);
        return isViableFeatureBiome(g->mc, Mansion, biome);
    }
    if (type == Trail_Ruins || type == Village)
    {
        static const int kinds[] = {plains, desert, savanna, taiga, snowy_tundra};
        int count = type == Village ? 5 : 1;
        for (int i = 0; i < count; i++)
        {
            StructureVariant sv;
            if (!getVariant(&sv, type, g->mc, g->seed, x, z, type == Village ? kinds[i] : -1))
                continue;
            int px = (((x >> 4) * 32 + 2 * sv.x + sv.sx - 1) / 2),
                pz = (((z >> 4) * 32 + 2 * sv.z + sv.sz - 1) / 2);
            int y = terrain_height(owner->terrain, px, pz, 0) + (type == Trail_Ruins ? -15 : 0);
            int biome = getBiomeAt(g, 4, px >> 2, y >> 2, pz >> 2);
            if (type == Trail_Ruins ? isViableFeatureBiome(g->mc, type, biome)
                                    : biome == kinds[i] || (biome == meadow && kinds[i] == plains))
                return 1;
        }
        return 0;
    }
    return CM_UNSUPPORTED;
}
static int portal_variant_point(SeedMapHandle *owner, int x, int z, int requested_kind,
                                cm_generation_point *out)
{
    /* Default ruined_portal template dimensions, in numbered then giant order.
     * The six rows are the standard/desert/jungle/swamp/mountain/ocean setups.
     * Only dimensions and biome membership are needed, not template blocks. */
    static const int sizes[13][3] = {
        {6, 10, 6}, {9, 12, 9}, {8, 9, 9},   {8, 9, 9},    {10, 10, 7},  {5, 7, 7},   {9, 7, 9},
        {14, 9, 9}, {10, 8, 9}, {12, 8, 10}, {11, 17, 16}, {11, 16, 16}, {16, 16, 16}};
    static const uint64_t masks[6][4] = {{0x000000096c0158b2ULL, 0, 0x0404c00108001012ULL, 0},
                                         {0x4ULL, 0, 0, 0},
                                         {0x0000000000a00000ULL, 0, 0x0000010000000000ULL, 0},
                                         {0x40ULL, 0, 0x0100000000000000ULL, 0},
                                         {0x0000007402000008ULL, 0, 0x027a002800000008ULL, 0},
                                         {0x0007700001000401ULL, 0, 0, 0}};
    Generator *g = &owner->generator;
    int rc = ensure_terrain(owner);
    if (rc < 0)
        return rc;
    int ox = (x >> 4) * 16, oz = (z >> 4) * 16;
    for (int kind = requested_kind; kind <= requested_kind; kind++)
    {
        uint64_t rng = chunkGenerateRnd(g->seed, x >> 4, z >> 4);
        int air = 0;
        int placement = 0; /* 0=surface,1=underground,2=mountain,3=buried,4=ocean floor */
        if (kind == 0 || kind == 4)
        {
            int below = nextFloat(&rng) < 0.5f;
            placement = below ? (kind == 0 ? 1 : 2) : 0;
            air = below;
            if (!below && g->mc < MC_26_3)
                air = nextFloat(&rng) < 0.5f; /* air-pocket sampling is skipped at probability 1. */
        }
        else if (kind == 1)
            placement = 3;
        else if (kind == 2)
        {
            if (g->mc < MC_26_3)
                air = nextFloat(&rng) < 0.5f;
        }
        else
            placement = 4;
        int giant = nextFloat(&rng) < 0.05f;
        int template = giant ? 10 + nextInt(&rng, 3) : nextInt(&rng, 10);
        int rotation = nextInt(&rng, 4), mirror = nextFloat(&rng) >= 0.5f;
        int sx = sizes[template][0], sy = sizes[template][1], sz = sizes[template][2], px = sx / 2,
            pz = sz / 2;
        int minx = 2147483647, maxx = -2147483647, minz = 2147483647, maxz = -2147483647;
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++)
            {
                int xx = a * (sx - 1), zz = b * (sz - 1), rx, rz;
                if (mirror)
                    xx = -xx;
                switch (rotation)
                {
                case 0:
                    rx = xx;
                    rz = zz;
                    break;
                case 1:
                    rx = px + pz - zz;
                    rz = pz - px + xx;
                    break;
                case 2:
                    rx = 2 * px - xx;
                    rz = 2 * pz - zz;
                    break;
                default:
                    rx = px - pz + zz;
                    rz = px + pz - xx;
                    break;
                }
                if (rx < minx)
                    minx = rx;
                if (rx > maxx)
                    maxx = rx;
                if (rz < minz)
                    minz = rz;
                if (rz > maxz)
                    maxz = rz;
            }
        minx += ox;
        maxx += ox;
        minz += oz;
        maxz += oz;
        /* BoundingBox center rounds differently from Jigsaw integer division. */
        int cx = minx + (maxx - minx + 1) / 2, cz = minz + (maxz - minz + 1) / 2;
        int map = placement == 4 ? 1 : 0, y = terrain_height(owner->terrain, cx, cz, map) - 1;
        if (placement == 1 || placement == 2)
        {
            int upper = y - sy, lower = placement == 1 ? -49 : 70;
            y = lower < upper ? lower + nextInt(&rng, upper - lower + 1) : upper;
        }
        else if (placement == 3)
            y = y - sy + 2 + nextInt(&rng, 7);
        /* findSuitableY needs occupancy below overhangs, not just top heights.
         * Lazily evaluate 16 levels of each base column until three qualify. */
        while (y > -49)
        {
            int columns[4][16], top = y, bottom = y - 15;
            if (bottom < -48)
                bottom = -48;
            terrain_column_range(owner->terrain, minx, minz, bottom, top, columns[0]);
            terrain_column_range(owner->terrain, maxx, minz, bottom, top, columns[1]);
            terrain_column_range(owner->terrain, minx, maxz, bottom, top, columns[2]);
            terrain_column_range(owner->terrain, maxx, maxz, bottom, top, columns[3]);
            int found = 0;
            for (; y >= bottom; y--)
            {
                int count = 0;
                for (int i = 0; i < 4; i++)
                {
                    int b = columns[i][y - bottom];
                    if (map ? b == 1 : b != 0)
                        count++;
                }
                if (count >= 3)
                {
                    found = 1;
                    break;
                }
            }
            if (found)
                break;
        }
        int biome = getBiomeAt(g, 4, ox >> 2, y >> 2, oz >> 2);
        if (out)
            *out = (cm_generation_point){ox,y,oz,biome,kind,template,rotation,mirror,placement,air};
        int standard = kind == 0 &&
            ((biome == sulfur_caves && g->mc >= MC_26_2) ||
             (biome == dappled_forest && g->mc >= MC_26_3));
        if (biome >= 0 && biome < 256 &&
            (standard || (masks[kind][biome >> 6] & (1ULL << (biome & 63)))))
            return 1;
    }
    return 0;
}

static int portal_generation_point(SeedMapHandle *owner, int x, int z)
{
    for (int kind = 0; kind < 6; kind++)
    {
        int rc = portal_variant_point(owner, x, z, kind, NULL);
        if (rc != 0)
            return rc;
    }
    return 0;
}
static int structure_set_generation_point(SeedMapHandle *owner, int type, int x, int z,
                                          cm_generation_point *out);

int cm_structure_variant(void *handle, int type, int x, int z, int variant,
                         cm_generation_point *out)
{
    SeedMapHandle *owner = (SeedMapHandle *)handle;
    if (!owner || !out)
        return CM_INVALID;
    Generator *g = &owner->generator;
    if (g->mc < MC_26_1 || g->dim != DIM_OVERWORLD)
        return CM_UNSUPPORTED;
    if (x < -30000000 || z < -30000000 || x >= 30000000 || z >= 30000000)
        return CM_RANGE;
    int count = type == Village ? 5 : type == Ruined_Portal ? 6 :
                type == Abandoned_Camp && g->mc >= MC_26_3 ? 18 : 0;
    if (!count)
        return CM_UNSUPPORTED;
    if (variant < -1 || variant >= count)
        return CM_INVALID;
    int rc = ensure_terrain(owner);
    if (rc < 0)
        return rc;
    if (variant == -1)
        return structure_set_generation_point(owner, type, x, z, out);
    if (type == Ruined_Portal)
        return portal_variant_point(owner, x, z, variant, out);
    int px, pz, expected, start, rotation;
    if (type == Village)
    {
        /* StructureSet order, not biome-ID order. Each attempt owns its RNG. */
        static const int biomes[] = {plains,desert,savanna,snowy_tundra,taiga};
        StructureVariant sv;
        expected = biomes[variant];
        if (!getVariant(&sv, Village, g->mc, g->seed, x, z, expected))
            return CM_GENERATION;
        px = (((x >> 4) * 32 + 2 * sv.x + sv.sx - 1) / 2);
        pz = (((z >> 4) * 32 + 2 * sv.z + sv.sz - 1) / 2);
        start = sv.start;
        rotation = sv.rotation;
    }
    else
    {
        /* Java 26.3 abandoned_camp set and all 18 start pools. Each pool has
         * ten equally weighted tents; templates 3/8 are 6x8, others 8x8.
         * No named starting jigsaw; rigid projection and zero dimension
         * padding. GroundLevelDelta affects piece Y, not GenerationStub Y. */
        static const int biomes[] = {
            bamboo_jungle,birch_forest,cherry_grove,dappled_forest,flower_forest,
            forest,meadow,giant_tree_taiga,tall_birch_forest,giant_spruce_taiga,
            pale_garden,savanna,snowy_taiga,jungle_edge,swamp,taiga,
            wooded_mountains,wooded_badlands_plateau
        };
        expected = biomes[variant];
        uint64_t rng = chunkGenerateRnd(g->seed, x >> 4, z >> 4);
        rotation = nextInt(&rng, 4);
        start = nextInt(&rng, 10);
        int sx = start == 2 || start == 7 ? 6 : 8, sz = 8;
        int dx = 0, dz = 0;
        switch (rotation)
        {
        case 1: dx = 1 - sz; { int tmp = sx; sx = sz; sz = tmp; } break;
        case 2: dx = 1 - sx; dz = 1 - sz; break;
        case 3: dz = 1 - sx; { int tmp = sx; sx = sz; sz = tmp; } break;
        }
        /* Match Java truncation toward zero on negative odd sums. */
        px = (((x >> 4) * 32 + 2 * dx + sx - 1) / 2);
        pz = (((z >> 4) * 32 + 2 * dz + sz - 1) / 2);
    }
    int y = terrain_height(owner->terrain, px, pz, 0);
    int biome = getBiomeAt(g, 4, px >> 2, y >> 2, pz >> 2);
    *out = (cm_generation_point){px,y,pz,biome,variant,start,rotation,0,0,0};
    return biome == expected || (type == Village && expected == plains && biome == meadow);
}

static int structure_set_generation_point(SeedMapHandle *owner, int type, int x, int z,
                                          cm_generation_point *out)
{
    /* Registered 26.x sets have equal unit weights. Draw without replacement
     * and stop at the first valid attempt, exactly as ChunkGenerator does.
     * The shared Portal set contains the Nether entry, which cannot satisfy
     * an Overworld biome predicate but must still consume the selection draw. */
    int count = type == Village ? 5 : type == Ruined_Portal ? 7 : 18;
    int active[18], remaining = count;
    for (int i = 0; i < count; i++)
        active[i] = 1;
    uint64_t rng = chunkGenerateRnd(owner->generator.seed, x >> 4, z >> 4);
    while (remaining)
    {
        int draw = nextInt(&rng, remaining), selected = 0;
        for (; selected < count; selected++)
            if (active[selected] && draw-- == 0)
                break;
        active[selected] = 0;
        remaining--;
        if (type == Ruined_Portal && selected == 6)
            continue;
        cm_generation_point point;
        int rc = cm_structure_variant(owner, type, x, z, selected, &point);
        if (rc > 0 && out)
            *out = point;
        if (rc != 0)
            return rc;
    }
    return 0;
}

int cm_structure_region(void *handle, int type, int rx, int rz, cm_hit *out)
{
    Generator *g = (Generator *)handle;
    if (!g || !out)
        return CM_INVALID;
    if (!valid_type(type))
        return CM_UNSUPPORTED;
    Pos pos;
    int flags = 0;
    if (type == Mineshaft)
    {
        flags = 0; /* Placement only: mine shafts have no biome viability test. */
        if (g->dim != 0)
            return CM_UNSUPPORTED;
        if (rx < -1875000 || rz < -1875000 || rx >= 1875000 || rz >= 1875000)
            return CM_RANGE;
        /* The implementation uses inclusive end chunk coordinates. */
        if (!getMineshafts(g->mc, g->seed, rx, rz, rx, rz, &pos, 1))
            return 0;
        /* getMineshafts outputs block coordinates. */
    }
    else
    {
        StructureConfig c;
        if (!structure_config(type, g->mc, g->flags, &c) || c.dim != g->dim)
            return CM_UNSUPPORTED;
        int region = 16 * c.regionSize;
        if ((int64_t)rx * region < -30000000 - region || (int64_t)rx * region >= 30000000 ||
            (int64_t)rz * region < -30000000 - region || (int64_t)rz * region >= 30000000)
            return CM_RANGE;
        // Raw placement for the early exact Nether profile still uses
        // generator version MC_1_18 (the upstream enum's 1.18.2 alias).
        if (type == Ruined_Portal_N && (g->flags & EARLY_118_STRUCTURES))
            pos = getFeaturePos(c, g->seed, rx, rz);
        else if (!getStructurePos(type, g->mc, g->seed, rx, rz, &pos))
            return 0;
        /* A border-intersecting region can place its candidate outside the
         * world. Preserve the absent-candidate result before point sampling. */
        if (pos.x < -30000000 || pos.z < -30000000 || pos.x >= 30000000 || pos.z >= 30000000)
            return 0;
        if (g->mc >= MC_1_18 && g->dim == DIM_OVERWORLD &&
            (type == Mansion || type == Village || type == Trail_Ruins || type == Ruined_Portal ||
             type == Abandoned_Camp))
        {
            int rc = g->mc >= MC_26_1 &&
                         (type == Village || type == Ruined_Portal || type == Abandoned_Camp)
                         ? structure_set_generation_point((SeedMapHandle *)handle, type, pos.x, pos.z, NULL)
                         : type == Ruined_Portal
                         ? portal_generation_point((SeedMapHandle *)handle, pos.x, pos.z)
                         : modern_generation_point((SeedMapHandle *)handle, type, pos.x, pos.z);
            if (rc <= 0)
                return rc;
            flags |= 1 | 4;
        }
        else
        {
            if (!isViableStructurePos(type, g, pos.x, pos.z, 0))
                return 0;
            // Legacy/Nether portals retain their existing placement-only contract.
            if (type != Ruined_Portal && type != Ruined_Portal_N)
                flags |= 1;
            if (!isViableStructureTerrain(type, g, pos.x, pos.z))
                return 0;
            if (g->mc >= MC_1_18 &&
                (type == Desert_Pyramid || type == Jungle_Temple || type == Mansion))
                flags |= 2;
            // The vanilla End City height predicate was added in 1.11.
            if (type == End_City && g->mc >= MC_1_11)
            {
                SurfaceNoise sn;
                initSurfaceNoise(&sn, DIM_END, g->seed);
                if (!isViableEndCityTerrain(g, &sn, pos.x, pos.z))
                    return 0;
                flags |= 4;
            }
        }
    }
    if (pos.x < -30000000 || pos.z < -30000000 || pos.x >= 30000000 || pos.z >= 30000000)
        return 0;
    *out = (cm_hit){type, pos.x, pos.z, flags};
    return 1;
}
int cm_strongholds(void *handle, cm_hit *out, int capacity)
{
    Generator *g = (Generator *)handle;
    if (!g || !out)
        return CM_INVALID;
    if (g->dim != DIM_OVERWORLD)
        return CM_UNSUPPORTED;
    if(capacity<128) return CM_CAPACITY;
    StrongholdIter sh;
    initFirstStronghold(&sh,g->mc,g->seed);
    int n=0, more;
    int max_count = g->mc < MC_1_9 ? 3 : 128;
    do {
        more=nextStronghold(&sh,g);
        out[n++]=(cm_hit){100,sh.pos.x,sh.pos.z,1};
    } while(more>0 && n<max_count);
    return n;
}
int cm_slime(uint64_t seed,int cx,int cz) {
    if(cx < -1875000 || cz < -1875000 || cx>=1875000 || cz>=1875000) return CM_RANGE;
    return isSlimeChunk(seed,cx,cz);
}
