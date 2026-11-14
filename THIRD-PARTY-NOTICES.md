# Third-Party Notices

## Cubiomes

This project uses Cubiomes by Cubitect, pinned to revision `e61f90580cbdd883214a8054670dacae655e59c0`. Cubiomes provides Minecraft Java biome, structure, and random generation calculations. It retains its upstream MIT license and Cubitect copyright notice.

The pinned upstream sources are located in `native/cubiomes/upstream/`. The license, source information, and file hash manifest are located in `licenses/cubiomes/`.

The Java 26.x biome adaptations selectively use MIT-licensed changes from Cubiomes PR #166 at `53b86c8bee85806347d3051b2f43d7ca206f7aba` and reviewed version/range rules from xpple/cubiomes at `8c37ab26f2b47740839eb0045b34efc5141eb065`. The exact 26.3 climate topology is numerical parameter data with an independently written MIT-licensed search; the fork's six-order 26.3 tree is not distributed. Full provenance is recorded in `licenses/cubiomes/SOURCE.md`.

`native/cubiomes/seedmap.c` and `seedmap.h` are the Native C ABI adapter written by RZeroF1 for this project. The adapter defines boundaries, parameters, and resource lifetimes and is covered by the project's MPL 2.0 license.

`native/cubiomes/terrain.c`, `terrain.h`, and `terrain_tables.h` are the project's MPL 2.0 terrain-height implementation and finite mathematical profiles, written by RZeroF1. They reuse Cubiomes random-number and noise initialization functions. Minecraft client JARs, mappings, Java code, templates, textures, and verification tools are not included in the package.
