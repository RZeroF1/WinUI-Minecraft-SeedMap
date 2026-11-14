/* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * Copyright (c) 2026 RZeroF1.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <stdint.h>

#ifdef _WIN32
#define CM_API __declspec(dllexport)
#else
#define CM_API __attribute__((visibility("default")))
#endif

/*
 * RZeroF1 seed map ABI 1. All functions are cdecl. Handles own a Generator;
 * never use a handle concurrently, and destroy it after all calls finish.
 * No caller buffer is retained. UTF-8 names are static, process-lifetime strings.
 * Coordinates are blocks, X=east, Z=south, bounds are [min,max).
 * Tiles use aligned block origins, step={1,4,16,64,256}, row-major int32 IDs.
 * Y is always blocks (the wrapper performs Cubiomes' vertical scale conversion).
 * World bounds are [-30,000,000,30,000,000); calls outside fail with CM_RANGE.
 * Ordered cm_biome_sample additionally takes caller-owned search state. Calls
 * are synchronous and bounded; cancellation belongs between calls/tiles.
 */
enum { CM_OK=0, CM_INVALID=-1, CM_UNSUPPORTED=-2, CM_RANGE=-3,
       CM_MEMORY=-4, CM_GENERATION=-5, CM_CAPACITY=-6 };
typedef struct { int32_t id, dimensions; const char *key; } cm_version;
typedef struct { int32_t id, dimension, region_blocks, flags; const char *key; } cm_structure;
typedef struct { int32_t type, x, z, flags; } cm_hit;
typedef struct {
    int32_t x, y, z, biome, variant, start, rotation, mirror, placement, air_pocket;
} cm_generation_point;

CM_API int cm_abi_version(void);
CM_API const char *cm_source_revision(void);
CM_API int cm_versions(cm_version *out, int capacity);
CM_API int cm_structures(int version, int dimension, cm_structure *out, int capacity);
CM_API const char *cm_biome_name(int version, int dimension, int id);
CM_API uint32_t cm_biome_color(int id);
CM_API int cm_create(int version, uint64_t seed, int dimension, void **out);
CM_API void cm_destroy(void *handle);
CM_API int cm_biomes(void *handle, int64_t x, int64_t z, int width, int height,
                     int step, int y, int32_t *out, int capacity);
/* Ordered vanilla biome-source sampling. Initialize search_state to zero and
 * retain it only for this handle's sequence. A failed call leaves it unchanged.
 * cm_biomes remains independent of search_state and of previous queries. */
CM_API int cm_biome_sample(void *handle, int64_t x, int64_t z, int step, int y,
                          uint64_t *search_state, int32_t *out);
/* Default Overworld getBaseHeight, before surface rules, carving or decoration.
 * type=0 WORLD_SURFACE_WG, type=1 OCEAN_FLOOR_WG. Highest qualifying block Y+1,
 * or -64 for an empty column. Coordinates are blocks; output is row-major.
 * Bounded synchronous strips (at most 4096 samples); cancel between calls. */
CM_API int cm_terrain_heights(void *handle, int type, int64_t x, int64_t z,
                             int width, int height, int step, int32_t *out, int capacity);
/* Default base-noise column, indexed from -64 through 319. Occupancy classes
 * 0=air, 1=solid, 2=water, 3=lava. No game block IDs, carving or decoration. */
CM_API int cm_terrain_column(void *handle, int x, int z, int32_t *out, int capacity);
/* One placement region per call; mine shafts use 16x16 block regions.
 * Strongholds use cm_strongholds. Result flags: 1=biome predicate checked,
 * 2=approximate terrain checked, 4=exact base-height/End City terrain checked. A
 * missing flag means that stage was not performed. No flag implies
 * block-world certainty. */
CM_API int cm_structure_region(void *handle, int type, int region_x, int region_z, cm_hit *out);
/* Default Overworld configured variant GenerationPoint and biome predicate.
 * Java 26.x Village (0..4), Portal (0..5), 26.3 Camp (0..17); indices follow
 * the release StructureSet order. variant=-1 selects the first valid weighted
 * attempt, using the same removal/retry path as cm_structure_region; in this
 * mode out is populated only on success. x/z identify the placement chunk.
 * Returns 1 if valid, 0 if the biome predicate rejects the populated raw point,
 * or a negative error. Does not change ordered biome sampling state or build
 * structure pieces. cm_structure_region uses this same path with weighted
 * selection and removal/retry. Existing cm_hit coordinates remain candidates. */
CM_API int cm_structure_variant(void *handle, int type, int x, int z, int variant,
                                cm_generation_point *out);
CM_API int cm_strongholds(void *handle, cm_hit *out, int capacity);
CM_API int cm_slime(uint64_t seed, int chunk_x, int chunk_z);
