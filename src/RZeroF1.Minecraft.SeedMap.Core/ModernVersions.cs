// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// Copyright (c) 2026 RZeroF1.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
namespace RZeroF1.Minecraft.SeedMap;

public static partial class Api
{
    private static VersionCapability ModernCapability(VersionInfo version)
    {
        string release = version.Key;
        var releaseVersion = Version.Parse(release);
        int minor = releaseVersion.Minor;
        bool java26 = releaseVersion.Major == 26;
        string nativeProfile = ReleaseToNative[release];
        string biomeProfile = release switch
        {
            "26.1" => "cubiomes-26.1",
            "26.2" => "cubiomes-26.2",
            "26.3" => "cubiomes-26.3-rtree19",
            "1.14" => "cubiomes-1.14.4",
            "1.21" or "1.21.1" => "cubiomes-1.21.3",
            "1.21.4" => "cubiomes-1.21-winter-drop",
            _ when minor == 21 && release is not ("1.21.2" or "1.21.3") => "cubiomes-1.21.5-pr162",
            _ => "cubiomes-" + nativeProfile
        };
        string structureProfile = release switch
        {
            "26.1" or "26.2" or "26.3" => "java-" + release + "-structure-sets",
            "1.14" => "cubiomes-1.14",
            "1.21" or "1.21.1" => "cubiomes-1.21.1",
            "1.21.5" or "1.21.6" or "1.21.7" or "1.21.8" => "cubiomes-1.21.5-1.21.8-strongholds",
            _ => biomeProfile
        };
        bool establishedModernQuery = releaseVersion.Major == 1 && minor <= 17;
        var structures = new[] { -1, 0, 1 }
            .SelectMany(dimension => Structures(version.Id, dimension))
            .Where(structure => structure.Id != 100)
            .Select(structure =>
            {
                bool exactBiomePredicate = structure.Id != 15 && !(java26 && structure.Id == 12);
                bool exactGenerationPoint = structure.Id != 15 && !(java26 && structure.Id == 12);
                VersionSupportStatus terrain = !establishedModernQuery && (structure.Id is 5 or 9 or 11 or 23 or 25)
                    ? VersionSupportStatus.Supported
                    : structure.Id is 1 or 2 or 9
                        ? VersionSupportStatus.Experimental
                        : structure.Id == 20
                            ? VersionSupportStatus.Supported
                            : VersionSupportStatus.Unsupported;
                var capability = new StructureCapability(structure.Id, structure.Key,
                    VersionSupportStatus.Supported,
                    terrain,
                    VersionSupportStatus.Unsupported)
                {
                    RawPlacement = VersionSupportStatus.Supported,
                    BiomePredicate = exactBiomePredicate ? VersionSupportStatus.Supported : VersionSupportStatus.Unsupported,
                    GenerationPoint = exactGenerationPoint ? VersionSupportStatus.Supported : VersionSupportStatus.Unsupported
                };
                return capability;
            })
            .ToArray();
        bool structuresSupported = structures.All(s => s.Candidates == VersionSupportStatus.Supported);
        bool biomesSupported = true;
        string notes = java26
            ? "精确发行 JAR 已对照三维度有序群系、结构 placement、128 强要塞、矿井及逐变体生成点；Village/主世界 Portal 使用原版 StructureSet 抽取与失败重试，26.3 新增 Abandoned Camp。默认主世界两种 WorldGen 基础高度使用独立 Terrain Profile；Nether Portal 保留 placement 候选，BiomePredicate/GenerationPoint 未支持；Desert Pyramid/Jungle Temple 附加 Terrain 仍为 Experimental，FullGeneration 为 Unsupported。"
            : establishedModernQuery
            ? "精确 JAR 的五尺度群系、三维度、128 强要塞、矿井 RNG 及已登记结构的位置谓词已独立对照；完整结构方块生成仍未声明。"
            : "27 个精确 JAR 的群系、三维度、128 强要塞、placement 与逐结构生成点已独立对照；默认主世界 WORLD_SURFACE_WG/OCEAN_FLOOR_WG 和 Village、Mansion、Ruined Portal、Trail Ruins 的基础高度判定已验证。Desert Pyramid、Jungle Temple 的附加 Terrain 检查仍为近似；完整结构方块生成不属于支持范围。";
        return new(release, biomeProfile, structureProfile, version.Id,
            structuresSupported ? VersionSupportStatus.Supported : VersionSupportStatus.Experimental,
            biomesSupported ? VersionSupportStatus.Supported : VersionSupportStatus.Experimental,
            structuresSupported ? VersionSupportStatus.Supported : VersionSupportStatus.Experimental,
            VersionSupportStatus.Supported,
            VersionVerificationStatus.GameJarAlgorithms, notes)
        {
            Strongholds = VersionSupportStatus.Supported,
            StructureCapabilities = Array.AsReadOnly(structures)
        };
    }
}
