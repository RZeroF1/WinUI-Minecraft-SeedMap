// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// Copyright (c) 2026 RZeroF1.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
using System.Runtime.InteropServices;

namespace RZeroF1.Minecraft.SeedMap;

/// <summary>原版高度查询的方块谓词；WorldGen 类型用于未雕刻、未装饰的基础噪声列。</summary>
public enum TerrainHeightmap
{
    WorldSurfaceWorldGen = 0,
    OceanFloorWorldGen = 1,
    WorldSurface = 2,
    OceanFloor = 3,
    MotionBlocking = 4,
    MotionBlockingNoLeaves = 5
}

/// <summary>高度计算的精确性等级。</summary>
public enum TerrainAccuracy
{
    Unsupported,
    Approximate,
    Experimental,
    Exact
}

/// <summary>某发行版、维度与高度类型的实际计算能力。</summary>
public sealed record TerrainCapability(string ReleaseKey, int Dimension, TerrainHeightmap Heightmap,
    TerrainAccuracy Accuracy, string ProfileKey, int MinY, int Height);

public static partial class Api
{
    [DllImport(Library, CallingConvention = CallingConvention.Cdecl), DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)]
    private static extern int cm_terrain_heights(Generator generator, int type, long x, long z,
        int width, int height, int step, [Out] int[] output, int capacity);

    /// <summary>查询默认、未混合主世界的基础高度能力；不包含 Surface Rules、雕刻、装饰或已有区块。</summary>
    public static TerrainCapability GetTerrainCapability(string releaseKey, int dimension, TerrainHeightmap heightmap)
    {
        var version = GetVersionCapability(releaseKey);
        bool supported = version.ReleaseKey.StartsWith("1.18", StringComparison.Ordinal) ||
            version.ReleaseKey.StartsWith("1.19", StringComparison.Ordinal) ||
            version.ReleaseKey.StartsWith("1.20", StringComparison.Ordinal) ||
            version.ReleaseKey.StartsWith("1.21", StringComparison.Ordinal) ||
            version.ReleaseKey is "26.1" or "26.2" or "26.3";
        supported &= dimension == 0 && heightmap is TerrainHeightmap.WorldSurfaceWorldGen or TerrainHeightmap.OceanFloorWorldGen;
        string profile = releaseKey switch
        {
            "26.1" or "26.2" or "26.3" => "terrain-" + releaseKey,
            "1.18" or "1.18.1" => "terrain-1.18",
            "1.18.2" => "terrain-1.18.2",
            "1.21.9" or "1.21.10" or "1.21.11" => "terrain-1.21.9",
            _ => "terrain-1.19"
        };
        return new(releaseKey, dimension, heightmap,
            supported ? TerrainAccuracy.Exact : TerrainAccuracy.Unsupported,
            supported ? profile : "", supported ? -64 : 0, supported ? 384 : 0);
    }

    /// <summary>计算原版 getBaseHeight：最高满足谓词方块的 Y+1；空列返回 -64。X/Z 为方块坐标。</summary>
    /// <remarks>仅支持现代默认主世界的 WorldGen 类型。Generator 必须独占访问；高度查询不改变有序群系状态。</remarks>
    public static int SampleTerrainHeight(Generator generator, long x, long z,
        TerrainHeightmap heightmap = TerrainHeightmap.WorldSurfaceWorldGen)
    {
        ArgumentNullException.ThrowIfNull(generator);
        var result = new int[1];
        Check(cm_terrain_heights(generator, (int)heightmap, x, z, 1, 1, 1, result, 1));
        return result[0];
    }

    /// <summary>批量计算基础高度，按 z*width+x 排列；step 是正整数方块间距，原点无需对齐。</summary>
    /// <remarks>以最多 64 点的同步小段执行，在各段之间响应取消；重用 Generator 的噪声与有限缓存。</remarks>
    public static int[] GenerateTerrainHeights(Generator generator, long x, long z, int width, int height,
        int step = 1, TerrainHeightmap heightmap = TerrainHeightmap.WorldSurfaceWorldGen,
        CancellationToken cancellation = default)
    {
        ArgumentNullException.ThrowIfNull(generator);
        cancellation.ThrowIfCancellationRequested();
        if (width <= 0 || height <= 0 || width > 4096 || height > 4096 || (long)width * height > 1_048_576)
            throw new ArgumentOutOfRangeException(nameof(width), "高度区域最多包含 1,048,576 个样本，每边最多 4096。");
        if (step <= 0 || step > WorldLimit) throw new ArgumentOutOfRangeException(nameof(step));
        if (x < -WorldLimit || z < -WorldLimit || x >= WorldLimit || z >= WorldLimit ||
            x + (long)(width - 1) * step >= WorldLimit || z + (long)(height - 1) * step >= WorldLimit)
            throw new ArgumentOutOfRangeException(nameof(x), "高度区域超出 Java 世界边界。");
        var result = new int[width * height];
        var strip = new int[Math.Min(64, width)];
        for (int row = 0; row < height; row++)
            for (int column = 0; column < width; column += strip.Length)
            {
                cancellation.ThrowIfCancellationRequested();
                int count = Math.Min(strip.Length, width - column);
                Check(cm_terrain_heights(generator, (int)heightmap, x + (long)column * step,
                    z + (long)row * step, count, 1, step, strip, strip.Length));
                Array.Copy(strip, 0, result, row * width + column, count);
            }
        cancellation.ThrowIfCancellationRequested();
        return result;
    }
}
