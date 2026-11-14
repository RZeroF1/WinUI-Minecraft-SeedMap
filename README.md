# Minecraft-SeedUniverse

[简体中文](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/README.md) | [English](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/README.en.md)

Minecraft-SeedUniverse 是面向 Minecraft Java Edition 世界生成计算与种子地图的通用 .NET 10 SDK，以 `RZeroF1.Minecraft.SeedMap` NuGet 包提供，基于固定修订版 Cubiomes，提供群系采样、异步地图瓦片、结构查询、精确基础地形高度、附近群系搜索、坐标与相机数学、颜色转换、几何辅助和离屏 BGRA 导出。

SDK 提供数据、计算和图像缓冲区。消费端可以使用 WinUI、WPF、Avalonia、WebView、控制台或服务端程序，自行实现交互和图像呈现。

## 文档

| 文档 | 内容 |
|---|---|
| [API 说明](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/docs/api.md) | 世界模型、群系与结构查询、瓦片、相机、颜色和导出示例。 |
| [架构说明](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/docs/architecture.md) | 模块职责、Native 资源、异步任务和缓存生命周期。 |
| [NuGet 与发布](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/docs/nuget.md) | 安装、本地打包、独立包消费验证和发布流程。 |

## 当前能力

- 包 ID：`RZeroF1.Minecraft.SeedMap`
- 当前源码版本：`0.3.0`（本地未发布修改）
- 目标框架：`.NET 10`
- 原生运行时：Windows `win-x64`
- Minecraft Java 公开发行版本目录：包含 `1.4.2`–`1.12.2` 的 43 个历史发行版、`1.13.2`–`1.21.11` 的 43 个现代条目及 `26.1`、`26.2`、`26.3`，共 89 项；`1.2.5`、`1.3.1`、`1.3.2` 的精确 Native ID 与回归入口保留在内部；逐版能力见 [版本矩阵](docs/versions.md)
- 原版验证：89 个公开发行版及内部保留的三个历史发行版都有独立精确游戏 JAR Oracle 对照；能力按版本、结构和阶段登记，完整结构方块生成不属于支持范围
- 现代版本状态：1.18–1.21.11 的群系已通过每版 234,864 个有效样本的有序原版回放，27 版零差异并标为 Supported；结构仍按具体类型和验证阶段区分
- Java 26.x：三个独立 Native ID，支持三维度群系；26.2 加入 `sulfur_caves`，26.3 加入 `dappled_forest` 并保留原版 19 阶 RTree 的搜索顺序与等距离选择；Village／主世界 Portal 使用 StructureSet 加权重试，26.3 新增 18 类 Abandoned Camp 起点查询
- 地形高度：1.18–1.21.11 及 26.1–26.3 默认主世界的 `WORLD_SURFACE_WG` 与 `OCEAN_FLOOR_WG` 提供 Exact 单点和区域查询；26.x 各自使用独立 Profile，26.3 复现 float 密度与体积插值；高度为原版 `getBaseHeight` 的首个空闲 Y，不包含雕刻、装饰和完整区块生成
- 现代结构：Village、Woodland Mansion、Ruined Portal、Trail Ruins 使用实际生成点高度和三维群系谓词，Candidate、GenerationPoint 与相关 Terrain 阶段为 Supported；完整结构方块生成仍为 Unsupported
- 维度：主世界 `0`、下界 `-1`、末地 `1`
- 采样步长：`1`、`4`、`16`、`64`、`256` 个方块
- Cubiomes 修订版：基于 `e61f90580cbdd883214a8054670dacae655e59c0`，应用 PR #162 `ca9fcb4e2b4f29a88b15a19fe1f00a73bf503ec2` 与 `legacy46.1+modern43.3+terrain27.1+java26.1` 本地版本修复；选择性移植的来源见随包 Source Manifest

```text
消费端 UI / 图像框架
        │
        ▼
RZeroF1.Minecraft.SeedMap
  世界计算 · 瓦片 · 结构 · 坐标 · BGRA · 导出
        │
        ▼
rzero_seedmap.dll
        │
        ▼
固定修订版 Cubiomes
```

## 安装

使用当前源码构建的本地 `0.3.0` 包时，在 .NET 10 项目中添加包引用，并明确使用 Windows x64：

```xml
<PropertyGroup>
  <TargetFramework>net10.0</TargetFramework>
  <RuntimeIdentifier>win-x64</RuntimeIdentifier>
</PropertyGroup>

<ItemGroup>
  <PackageReference Include="RZeroF1.Minecraft.SeedMap" Version="0.3.0" />
</ItemGroup>
```

公开版本见 [NuGet](https://www.nuget.org/packages/RZeroF1.Minecraft.SeedMap)，源码和版本记录见 [GitHub](https://github.com/RZeroF1/Minecraft-SeedUniverse/releases)。尚未发布的源码版本可按[本地打包流程](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/docs/nuget.md#本地构建与打包)验证。

## 快速开始

```csharp
using SeedMap = RZeroF1.Minecraft.SeedMap;

var version = SeedMap.Api.Versions.Single(v => v.Key == "1.20.6");
var world = new SeedMap.World(123, version.Id, 0, 64);
var cancellationToken = CancellationToken.None;

using var engine = new SeedMap.Engine(world);
int[] ids = await engine.TileAsync(
    new SeedMap.Engine.TileKey(Step: 4, X: 0, Z: 0),
    cancellationToken);

var biomes = SeedMap.Api.Biomes(version.Id, world.Dimension)
    .ToDictionary(b => b.Id);
byte[] bgra = SeedMap.Palette.ToBgra(ids, biomes);
Console.WriteLine($"{SeedMap.Engine.TileSize} × {SeedMap.Engine.TileSize}, {bgra.Length} bytes");
```

示例可用于启用隐式 `using` 的 .NET 10 控制台项目。瓦片大小为 `128 × 128`，`Step: 4` 表示每个样本间隔四个方块；瓦片 `(0, 0)` 对应世界坐标 `[0, 512) × [0, 512)`。返回的 BGRA8 缓冲区按行排列，每个像素四个字节。

`Engine` 提供两个并发工作槽、可取消请求和 48 MiB LRU 瓦片缓存，每个后台计算任务独立创建并释放 Native generator。`Export.RenderAsync` 可以生成离屏像素，消费端可使用自己的 PNG 编码器保存图像。

## 基础地形高度

```csharp
using var generator = SeedMap.Api.Create(world);
int surfaceY = SeedMap.Api.SampleTerrainHeight(generator, x: 0, z: 0);
int oceanFloorY = SeedMap.Api.SampleTerrainHeight(
    generator, x: 0, z: 0, SeedMap.TerrainHeightmap.OceanFloorWorldGen);
int[] heights = SeedMap.Api.GenerateTerrainHeights(
    generator, x: -16, z: -16, width: 32, height: 32, step: 1,
    cancellation: cancellationToken);
```

区域结果按 `row * width + column` 排列，原点无需与采样步长对齐。Generator 独占自己的噪声和有限缓存，可复用来计算相邻区域。高度计算成本高于群系采样，适合结构判定、局部剖面和按需区域分析；大范围高度地图应使用后台分批计算并缓存结果。完整语义见 [API 说明](docs/api.md)。

## 支持范围

当前包提供 Windows x64 原生资产，面向原版 Minecraft Java 默认世界计算。`Api.VersionCapabilities` 按结构阶段登记能力，`Api.GetTerrainCapability` 按版本、维度和 Heightmap 类型报告 Exact、Approximate、Experimental 或 Unsupported。高度引擎覆盖未混合默认主世界的基础噪声列；后期 Heightmap、自定义世界生成、Minecraft Bedrock、混合版本存档和完整方块世界生成不在当前支持范围内。26.x Nether Portal 仅提供 placement 候选，其 BiomePredicate 和 GenerationPoint 为 Unsupported。

## 构建与打包

源码构建需要 Windows、.NET 10 SDK 和 Visual Studio C++ Build Tools。在仓库根目录执行：

```powershell
dotnet build src/RZeroF1.Minecraft.SeedMap.Core/RZeroF1.Minecraft.SeedMap.Core.csproj -c Release -r win-x64
dotnet pack src/RZeroF1.Minecraft.SeedMap.Core/RZeroF1.Minecraft.SeedMap.Core.csproj -c Release -r win-x64
```

包内容、许可证和构建输出可通过 `scripts/Test-ArtifactIsolation.ps1` 检查，详见 [NuGet 文档](docs/nuget.md)。

## 许可证

RZeroF1 原创代码采用 MPL 2.0，见 [LICENSE](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/LICENSE)。Cubiomes 上游代码保留 MIT 许可证和 Cubitect 的版权声明，见[第三方声明](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/THIRD-PARTY-NOTICES.md)和[来源说明](https://github.com/RZeroF1/Minecraft-SeedUniverse/blob/main/licenses/cubiomes/SOURCE.md)。许可证和第三方来源文档使用英文。
