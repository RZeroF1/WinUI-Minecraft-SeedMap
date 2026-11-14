/* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * Copyright (c) 2026 RZeroF1.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * https://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <stdint.h>

/* Default, unblended Overworld base-noise columns only. One owner per instance.
 * Height types: 0=WORLD_SURFACE_WG, 1=OCEAN_FLOOR_WG. Results are first free Y.
 * Internal occupancy classes are 0=air,1=solid,2=water,3=lava, not block IDs. */
enum
{
    TERRAIN_119 = 0, /* 1.19-1.21.8 */
    TERRAIN_1182 = 1,
    TERRAIN_1219 = 2, /* 1.21.9-1.21.11 */
    TERRAIN_118 = 3,  /* 1.18-1.18.1 */
    TERRAIN_261 = 4,
    TERRAIN_262 = 5,
    TERRAIN_263 = 6
};
typedef struct Terrain Terrain;
Terrain *terrain_create(uint64_t seed, int profile);
void terrain_destroy(Terrain *terrain);
int terrain_height(Terrain *terrain, int x, int z, int type);
int terrain_column(Terrain *terrain, int x, int z, int type, int *column);
/* Internal inclusive interval, within [-64,319], indexed from minY. */
void terrain_column_range(Terrain *terrain, int x, int z, int minY, int maxY, int *column);
