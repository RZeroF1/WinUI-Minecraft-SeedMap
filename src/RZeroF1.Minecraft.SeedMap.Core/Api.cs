// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// Copyright (c) 2026 RZeroF1.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
using System.Globalization;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace RZeroF1.Minecraft.SeedMap;

public sealed record VersionInfo(int Id, string Key, int Dimensions);
public enum VersionSupportStatus { Unsupported, NativeSmokeOnly, Experimental, Supported }
public enum VersionVerificationStatus { None, NativeSmoke, GameJarBiomesAndDimensions, GameJarAlgorithms }
[Flags]
public enum StructureHitChecks
{
    None = 0,
    BiomePredicate = 1,
    ApproximateTerrain = 2,
    ExactTerrain = 4
}
public sealed record StructureCapability(int Id, string Key, VersionSupportStatus Candidates,
    VersionSupportStatus Terrain, VersionSupportStatus FullGeneration)
{
    public VersionSupportStatus RawPlacement { get; init; } = VersionSupportStatus.Unsupported;
    public VersionSupportStatus BiomePredicate { get; init; } = VersionSupportStatus.Unsupported;
    public VersionSupportStatus GenerationPoint { get; init; } = VersionSupportStatus.Unsupported;
}
public sealed record VersionCapability(
    string ReleaseKey,
    string BiomeProfileKey,
    string StructureProfileKey,
    int NativeId,
    VersionSupportStatus Overall,
    VersionSupportStatus Biomes,
    VersionSupportStatus Structures,
    VersionSupportStatus Dimensions,
    VersionVerificationStatus Verification,
    string Notes)
{
    public VersionSupportStatus Strongholds { get; init; } = VersionSupportStatus.Unsupported;
    public IReadOnlyList<StructureCapability> StructureCapabilities { get; init; } = Array.Empty<StructureCapability>();
}
public sealed record BiomeInfo(int Id, string Key, string Name, uint Color);
public sealed record StructureInfo(int Id, string Key, string Name, int RegionBlocks);
public readonly record struct World(long Seed, int Version, int Dimension, int Y);
public readonly record struct Bounds(int MinX, int MinZ, int MaxX, int MaxZ)
{
    public bool Contains(int x, int z) => x >= MinX && x < MaxX && z >= MinZ && z < MaxZ;
}
/// <summary>表示结构区域查询返回的候选坐标和本次实际执行的阶段检查。</summary>
public sealed record StructureHit(int Type, int X, int Z, int Flags)
{
    /// <summary>将 Native ABI 标志转换为可读的阶段检查集合。</summary>
    public StructureHitChecks Checks => (StructureHitChecks)Flags;
}

/// <summary>提供类型化、带版本校验的 Cubiomes ABI 调用；每个计算任务独占自己的 Native generator。</summary>
public static partial class Api
{
    private const string Library = "rzero_seedmap";
    public const int WorldLimit = 30_000_000;
    public const int AbiVersion = 1;
    public const string Revision = "ca9fcb4e2b4f29a88b15a19fe1f00a73bf503ec2+legacy46.1+modern43.3+terrain27.1+java26.1";
    private static readonly Lazy<VersionInfo[]> NativeCapabilities = new(InitializeNative);
    private static readonly Lazy<VersionInfo[]> Capabilities = new(InitializeReleaseCatalog);
    private static readonly Lazy<VersionCapability[]> VersionCapabilityTable = new(InitializeVersionCapabilities);

    private static readonly IReadOnlyDictionary<string, string> ReleaseToNative =
        new Dictionary<string, string>(StringComparer.Ordinal)
        {
            ["1.13.2"] = "1.13.2",
            ["1.14"] = "1.14", ["1.14.1"] = "1.14.4", ["1.14.2"] = "1.14.4",
            ["1.14.3"] = "1.14.4", ["1.14.4"] = "1.14.4",
            ["1.15"] = "1.15.2", ["1.15.1"] = "1.15.2", ["1.15.2"] = "1.15.2",
            ["1.16"] = "1.16.1", ["1.16.1"] = "1.16.1",
            ["1.16.2"] = "1.16.5", ["1.16.3"] = "1.16.5", ["1.16.4"] = "1.16.5", ["1.16.5"] = "1.16.5",
            ["1.17.1"] = "1.17.1",
            ["1.18"] = "1.18", ["1.18.1"] = "1.18", ["1.18.2"] = "1.18.2",
            ["1.19"] = "1.19.2", ["1.19.1"] = "1.19.2", ["1.19.2"] = "1.19.2",
            ["1.19.3"] = "1.19.4", ["1.19.4"] = "1.19.4",
            ["1.20"] = "1.20.6", ["1.20.1"] = "1.20.6", ["1.20.2"] = "1.20.6",
            ["1.20.3"] = "1.20.6", ["1.20.4"] = "1.20.6", ["1.20.5"] = "1.20.6", ["1.20.6"] = "1.20.6",
            ["1.21"] = "1.21.1", ["1.21.1"] = "1.21.1", ["1.21.2"] = "1.21.3", ["1.21.3"] = "1.21.3",
            ["1.21.4"] = "1.21.4", ["1.21.5"] = "1.21.5-1.21.8-strongholds", ["1.21.6"] = "1.21.5-1.21.8-strongholds",
            ["1.21.7"] = "1.21.5-1.21.8-strongholds", ["1.21.8"] = "1.21.5-1.21.8-strongholds", ["1.21.9"] = "1.21.5",
            ["1.21.10"] = "1.21.5", ["1.21.11"] = "1.21.11",
            ["26.1"] = "26.1", ["26.2"] = "26.2", ["26.3"] = "26.3",
        };

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeVersion { public int Id, Dimensions; public IntPtr Key; }
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeStructure { public int Id, Dimension, Region, Flags; public IntPtr Key; }
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeHit { public int Type, X, Z, Flags; }

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_abi_version();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern IntPtr cm_source_revision();
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_versions([Out] NativeVersion[]? output, int capacity);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_structures(int version, int dimension, [Out] NativeStructure[]? output, int capacity);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern IntPtr cm_biome_name(int version, int dimension, int id);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern uint cm_biome_color(int id);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_create(int version, ulong seed, int dimension, out IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern void cm_destroy(IntPtr handle);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_biomes(Generator handle, long x, long z, int w, int h, int step, int y, [Out] int[] output, int capacity);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_biome_sample(Generator handle, long x, long z, int step, int y, ref ulong searchState, out int biome);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_structure_region(Generator handle, int type, int rx, int rz, out NativeHit hit);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_strongholds(Generator handle, [Out] NativeHit[] output, int capacity);
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)] private static extern int cm_slime(ulong seed, int chunkX, int chunkZ);

    public static IReadOnlyList<VersionInfo> Versions => Capabilities.Value;
    public static IReadOnlyList<VersionCapability> VersionCapabilities => VersionCapabilityTable.Value;

    public static VersionCapability GetVersionCapability(string releaseKey)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(releaseKey);
        return VersionCapabilities.SingleOrDefault(v => string.Equals(v.ReleaseKey, releaseKey, StringComparison.Ordinal))
            ?? throw new ArgumentException($"未注册的 Minecraft 版本：{releaseKey}。", nameof(releaseKey));
    }

    private static VersionInfo[] InitializeNative()
    {
        if (cm_abi_version() != AbiVersion || Marshal.PtrToStringUTF8(cm_source_revision()) != Revision)
            throw new InvalidOperationException("种子地图组件版本不匹配，请重新构建或安装。");
        var items = new NativeVersion[cm_versions(null, 0)];
        Check(cm_versions(items, items.Length));
        return items.Select(v => new VersionInfo(v.Id, Marshal.PtrToStringUTF8(v.Key)!, v.Dimensions)).ToArray();
    }

    private static VersionInfo[] InitializeReleaseCatalog()
    {
        var native = NativeCapabilities.Value.ToDictionary(v => v.Key, StringComparer.Ordinal);
        return ReleaseToNative.Select(pair =>
        {
            if (!native.TryGetValue(pair.Value, out var profile))
                throw new InvalidOperationException($"发行版本 {pair.Key} 引用的 Native profile {pair.Value} 不存在。");
            return new VersionInfo(profile.Id, pair.Key, profile.Dimensions);
        }).Concat(LegacyReleases.Select(release =>
        {
            if (!native.TryGetValue(release, out var profile))
                throw new InvalidOperationException($"发行版本 {release} 缺少 Native 入口。");
            return new VersionInfo(profile.Id, release, profile.Dimensions);
        })).ToArray();
    }

    private static VersionCapability[] InitializeVersionCapabilities()
    {
        return Versions.Select(version =>
        {
            if (version.Id is >= 100 and < 146) return LegacyCapability(version);
            return ModernCapability(version);
        }).ToArray();
    }

    public static long ParseSeed(string text)
    {
        text = text.Trim();
        if (text.Length == 0) throw new ArgumentException("请输入种子。");
        if (long.TryParse(text, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out long value)) return value;
        // Java String.hashCode: UTF-16 code units, signed int, sign extended to long.
        int hash = 0;
        foreach (char ch in text) hash = unchecked(hash * 31 + ch);
        return hash;
    }

    public static int FloorDiv(int value, int divisor) => (int)Math.Floor(value / (double)divisor);
    public static double ClampCoordinate(double value) => Math.Clamp(double.IsFinite(value) ? value : 0, -WorldLimit + 1, WorldLimit - 1);

    public static Generator Create(World world)
    {
        _ = Versions;
        Check(cm_create(world.Version, unchecked((ulong)world.Seed), world.Dimension, out var handle));
        return new Generator(handle);
    }

    public static int[] Generate(Generator generator, long x, long z, int width, int height, int step, int y)
    {
        var ids = new int[checked(width * height)];
        Check(cm_biomes(generator, x, z, width, height, step, y, ids, ids.Length));
        return ids;
    }

    /// <summary>按原版 BiomeSource 的调用顺序采样；现代主世界及下界的等距离 climate 选择会保留本 Generator 上次命中的叶节点。</summary>
    /// <remarks>状态保存在这个独占的 Generator 上，跨 SampleBiome 调用有效，重建或重置 Generator 后清除。Generate 不读取或修改该状态，因此地图瓦片适合独立计算和缓存，但不复现任意有序查询历史。</remarks>
    public static int SampleBiome(Generator generator, long x, long z, int step, int y)
    {
        ArgumentNullException.ThrowIfNull(generator);
        Check(cm_biome_sample(generator, x, z, step, y, ref generator.BiomeSearchState, out int biome));
        return biome;
    }

    /// <summary>清除有序群系采样的上次命中状态，使下一次 SampleBiome 从原版搜索树根开始查找。</summary>
    public static void ResetBiomeSampling(Generator generator)
    {
        ArgumentNullException.ThrowIfNull(generator);
        ObjectDisposedException.ThrowIf(generator.IsClosed, generator);
        generator.BiomeSearchState = 0;
    }

    public static BiomeInfo[] Biomes(int version, int dimension)
    {
        _ = Versions;
        var list = new List<BiomeInfo>();
        for (int id = 0; id < 256; id++)
        {
            string? key = Marshal.PtrToStringUTF8(cm_biome_name(version, dimension, id));
            if (key is not null) list.Add(new(id, key, BiomeName(key), cm_biome_color(id)));
        }
        return list.ToArray();
    }

    public static StructureInfo[] Structures(int version, int dimension)
    {
        _ = Versions;
        var items = new NativeStructure[32];
        int count = cm_structures(version, dimension, items, items.Length);
        Check(count);
        return items.Take(count).Select(s =>
        {
            var key = Marshal.PtrToStringUTF8(s.Key)!;
            return new StructureInfo(s.Id, key, StructureName(key), s.Region);
        }).ToArray();
    }

    /// <summary>按 placement、已实现的 biome predicate 和地形筛选查询一个结构区域。</summary>
    /// <remarks>
    /// Experimental 结构的 <see langword="null"/> 只表示当前 SDK 算法未返回候选，不能作为原版必定不会生成的结论。
    /// 命中项的 <see cref="StructureHit.Checks"/> 标明实际执行的检查；ApproximateTerrain 表示使用近似高度筛选。
    /// </remarks>
    public static StructureHit? QueryRegion(Generator generator, int type, int rx, int rz)
    {
        int rc = cm_structure_region(generator, type, rx, rz, out var hit);
        Check(rc);
        return rc == 0 ? null : new(hit.Type, hit.X, hit.Z, hit.Flags);
    }

    public static StructureHit[] Strongholds(Generator generator)
    {
        var hits = new NativeHit[128];
        int count = cm_strongholds(generator, hits, hits.Length);
        Check(count);
        return hits.Take(count).Select(h => new StructureHit(h.Type, h.X, h.Z, h.Flags)).ToArray();
    }

    public static bool IsSlimeChunk(long seed, int x, int z)
    {
        _ = Versions;
        int result = cm_slime(unchecked((ulong)seed), x, z);
        Check(result);
        return result != 0;
    }

    internal static void Destroy(IntPtr handle) => cm_destroy(handle);

    private static void Check(int code)
    {
        if (code >= 0) return;
        throw new InvalidOperationException(code switch
        {
            -1 => "地图参数无效。",
            -2 => "此版本或维度不支持该功能。",
            -3 => "坐标超出 Java 世界边界。",
            -4 => "地图组件内存分配失败。",
            -6 => "地图组件缓冲区不足。",
            _ => "地图生成失败。"
        });
    }

    private static string StructureName(string key) => key switch
    {
        "desert_pyramid" => "沙漠神殿",
        "jungle_pyramid" or "jungle_temple" => "丛林神庙",
        "swamp_hut" => "沼泽小屋",
        "igloo" => "雪屋",
        "village" => "村庄",
        "ocean_ruin" => "海底废墟",
        "shipwreck" => "沉船",
        "monument" => "海底神殿",
        "mansion" => "林地府邸",
        "pillager_outpost" or "outpost" => "掠夺者前哨站",
        "ruined_portal" or "ruined_portal_n" or "ruined_portal_nether" => "废弃传送门",
        "ancient_city" => "远古城市",
        "buried_treasure" or "treasure" => "埋藏的宝藏",
        "mineshaft" => "废弃矿井",
        "fortress" => "下界要塞",
        "bastion_remnant" or "bastion" => "堡垒遗迹",
        "end_city" => "末地城",
        "trail_ruins" => "古迹废墟",
        "trial_chambers" => "试炼密室",
        "stronghold" => "要塞",
        "abandoned_camp" => "废弃营地",
        _ => key.Replace('_', ' ')
    };

    private static string BiomeName(string key) => key switch
    {
        "plains" => "平原",
        "sunflower_plains" => "向日葵平原",
        "desert" => "沙漠",
        "mountains" => "山地",
        "mountain_edge" => "山地边缘",
        "forest" => "森林",
        "sulfur_caves" => "硫磺洞穴",
        "dappled_forest" => "斑驳森林",
        "flower_forest" => "繁花森林",
        "birch_forest" => "桦木森林",
        "dark_forest" => "黑森林",
        "taiga" => "针叶林",
        "swamp" => "沼泽",
        "mangrove_swamp" => "红树林沼泽",
        "river" => "河流",
        "frozen_river" => "冻河",
        "ocean" => "海洋",
        "deep_ocean" => "深海",
        "warm_ocean" => "暖水海洋",
        "lukewarm_ocean" => "温水海洋",
        "cold_ocean" => "冷水海洋",
        "frozen_ocean" => "冻洋",
        "deep_lukewarm_ocean" => "温水深海",
        "deep_cold_ocean" => "冷水深海",
        "deep_frozen_ocean" => "冰冻深海",
        "beach" => "沙滩",
        "mushroom_field_shore" => "蘑菇岛岸",
        "desert_hills" => "沙漠丘陵",
        "wooded_hills" => "森林丘陵",
        "taiga_hills" => "针叶林丘陵",
        "jungle_hills" => "丛林丘陵",
        "jungle_edge" => "丛林边缘",
        "snowy_beach" => "积雪沙滩",
        "stony_shore" => "石岸",
        "stone_shore" => "石岸",
        "jungle" => "丛林",
        "sparse_jungle" => "稀疏丛林",
        "bamboo_jungle" => "竹林",
        "savanna" => "热带草原",
        "savanna_plateau" => "热带高原",
        "badlands" => "恶地",
        "eroded_badlands" => "风蚀恶地",
        "wooded_badlands" => "疏林恶地",
        "wooded_badlands_plateau" => "疏林恶地高原",
        "badlands_plateau" => "恶地高原",
        "mushroom_fields" => "蘑菇岛",
        "snowy_tundra" => "积雪的冻原",
        "snowy_plains" => "雪原",
        "snowy_mountains" => "积雪山地",
        "snowy_taiga" => "积雪针叶林",
        "snowy_taiga_hills" => "积雪针叶林丘陵",
        "ice_spikes" => "冰刺平原",
        "birch_forest_hills" => "桦木森林丘陵",
        "meadow" => "草甸",
        "grove" => "雪林",
        "snowy_slopes" => "积雪山坡",
        "jagged_peaks" => "尖峭山峰",
        "frozen_peaks" => "冰封山峰",
        "stony_peaks" => "裸岩山峰",
        "cherry_grove" => "樱花树林",
        "pale_garden" => "苍白之园",
        "deep_dark" => "深暗之域",
        "lush_caves" => "繁茂洞穴",
        "dripstone_caves" => "溶洞",
        "giant_tree_taiga" => "巨型针叶林",
        "giant_tree_taiga_hills" => "巨型针叶林丘陵",
        "wooded_mountains" => "森林山地",
        "desert_lakes" => "沙漠湖泊",
        "gravelly_mountains" => "沙砾山地",
        "taiga_mountains" => "针叶林山地",
        "swamp_hills" => "沼泽丘陵",
        "modified_jungle" => "丛林变种",
        "modified_jungle_edge" => "丛林边缘变种",
        "tall_birch_forest" => "高大桦木森林",
        "tall_birch_hills" => "高大桦木丘陵",
        "dark_forest_hills" => "黑森林丘陵",
        "snowy_taiga_mountains" => "积雪的针叶林山地",
        "giant_spruce_taiga" => "巨型云杉针叶林",
        "giant_spruce_taiga_hills" => "巨型云杉针叶林丘陵",
        "modified_gravelly_mountains" => "沙砾山地变种",
        "shattered_savanna" => "破碎的热带草原",
        "shattered_savanna_plateau" => "破碎的热带高原",
        "modified_wooded_badlands_plateau" => "疏林恶地高原变种",
        "modified_badlands_plateau" => "恶地高原变种",
        "bamboo_jungle_hills" => "竹林丘陵",
        "old_growth_pine_taiga" => "原始松木针叶林",
        "old_growth_spruce_taiga" => "原始云杉针叶林",
        "old_growth_birch_forest" => "原始桦木森林",
        "windswept_hills" => "风袭丘陵",
        "windswept_forest" => "风袭森林",
        "windswept_gravelly_hills" => "风袭沙砾丘陵",
        "windswept_savanna" => "风袭草原",
        "nether_wastes" => "下界荒地",
        "soul_sand_valley" => "灵魂沙峡谷",
        "crimson_forest" => "绯红森林",
        "warped_forest" => "诡异森林",
        "basalt_deltas" => "玄武岩三角洲",
        "the_end" => "末地",
        "small_end_islands" => "末地小型岛屿",
        "end_midlands" => "末地内陆",
        "end_highlands" => "末地高地",
        "end_barrens" => "末地荒地",
        _ => key.Replace('_', ' ')
    };
}

public sealed class Generator : SafeHandleZeroOrMinusOneIsInvalid
{
    internal ulong BiomeSearchState;

    internal Generator(IntPtr value) : base(true) => SetHandle(value);

    protected override bool ReleaseHandle()
    {
        Api.Destroy(handle);
        return true;
    }
}
