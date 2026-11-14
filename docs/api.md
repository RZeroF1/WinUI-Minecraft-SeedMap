# API 说明

[项目首页](../README.md) · [架构说明](architecture.md) · [NuGet 与发布](nuget.md)

本文面向 `0.3.0` API，保留既有公共调用方式，新增 26.1、26.2、26.3 的独立发行入口和逐阶段能力。示例采用 `using SeedMap = RZeroF1.Minecraft.SeedMap;` 别名，所有 SDK 类型均通过 `SeedMap.` 访问，可用于启用隐式 `using` 的 .NET 10 项目。

## 世界与能力查询

```csharp
using SeedMap = RZeroF1.Minecraft.SeedMap;

var version = SeedMap.Api.Versions.Single(v => v.Key == "1.20.6");
var world = new SeedMap.World(
    Seed: SeedMap.Api.ParseSeed("123"),
    Version: version.Id,
    Dimension: 0,
    Y: 64);
var cancellationToken = CancellationToken.None;
```

后续代码段接续以上的 `world` 和 `cancellationToken`。版本 ID 来自 `Api.Versions`；它返回发行版本目录，补丁版本可以共享底层 Native Profile。维度使用主世界 `0`、下界 `-1`、末地 `1`，`Y` 表示采样高度。世界坐标以方块为单位，范围为 `[-30_000_000, 30_000_000)`；`Bounds` 包含最小边界，不包含最大边界。

| API | 用途 |
|---|---|
| `SeedMap.Api.Versions` | 查询 Native 实际支持的 Java 版本。 |
| `SeedMap.Api.VersionCapabilities` | 查询发行版本到内部 Profile 的映射、群系/结构/维度状态和验证等级。 |
| `SeedMap.Api.GetVersionCapability(releaseKey)` | 按发行版本读取能力描述；未知版本抛出参数异常。 |
| `SeedMap.Api.Biomes(version, dimension)` | 查询 DEFAULT 世界可生成的群系目录，含 ID、英文键名、中文名和颜色；不等同于原版完整注册表。 |
| `SeedMap.Api.Structures(version, dimension)` | 查询结构 ID、英文键名、名称和区域大小。 |
| `SeedMap.Api.ParseSeed(text)` | 去除首尾空白后解析十进制 `long`，其他非空文本按 Java `String.hashCode()` 计算。 |
| `SeedMap.Api.Create(world)` | 创建由 `SafeHandle` 管理的 Native generator，使用后释放。 |
| `SeedMap.Api.Generate(...)` | 同步生成指定矩形范围的群系 ID。 |
| `SeedMap.Api.SampleBiome(...)` | 按 generator 的顺序执行单点原版群系采样，复现现代 Climate 等距离选择中的上次命中状态。 |
| `SeedMap.Api.ResetBiomeSampling(...)` | 清除该 generator 的有序采样状态，使下一次采样从原版搜索根节点开始。 |
| `SeedMap.Api.GetTerrainCapability(releaseKey, dimension, heightmap)` | 查询指定版本、维度和高度类型的精确性及 Profile。 |
| `SeedMap.Api.SampleTerrainHeight(...)` | 计算一个方块列的原版基础高度。 |
| `SeedMap.Api.GenerateTerrainHeights(...)` | 可取消的区域高度查询，复用 generator 的噪声和有限缓存。 |
| `SeedMap.Api.QueryRegion(...)` | 查询指定结构区域，未找到时返回 `null`；Experimental 结构的 `null` 只表示当前 SDK 筛选未返回候选。 |
| `SeedMap.Api.Strongholds(...)` | 查询要塞位置。 |
| `SeedMap.Api.IsSlimeChunk(seed, chunkX, chunkZ)` | 按区块坐标查询史莱姆区块。 |

`VersionInfo` 表示 Native 能力表中的发行版本和整数 ID；`VersionCapability` 额外表示 `BiomeProfileKey`、`StructureProfileKey`、总体状态、群系状态、结构状态、维度状态和验证状态。发行版本可以共享群系 profile，同时保留独立结构 profile。`StructureCapability` 按实际结构分别报告 `RawPlacement`、`BiomePredicate`、`Candidates`、`GenerationPoint`、`Terrain` 和 `FullGeneration`。Mineshaft 的公开查询只返回 RNG placement，其 BiomePredicate 与 GenerationPoint 因不适用而标为 Unsupported；`FullGeneration` 对所有结构均为 Unsupported。1.18–1.21.11 的 Biome 与登记结构候选为 Supported；附加 Terrain 和 FullGeneration 仍单独报告。完整逐版清单见 [版本矩阵](versions.md)。

`StructureHit.Checks` 说明本次命中实际执行的检查：`BiomePredicate` 表示执行了结构群系谓词，`ApproximateTerrain` 表示使用近似地形高度，`ExactTerrain` 表示执行了已验证的基础噪声列高度／末地城高度检查。现代 Village、Woodland Mansion、Ruined Portal、Trail Ruins 和 26.3 Abandoned Camp 按实际 GenerationStub 的三维 quart 坐标筛选群系，并设置 BiomePredicate 和 ExactTerrain。返回坐标仍为候选区块方块原点；1.18–1.21.11 保留既有子类型查询规则，26.x 的 Village／主世界 Portal／Camp 使用 StructureSet 加权选择和失败重试。Mineshaft 和 26.x Nether Portal 为 placement-only；后者的 BiomePredicate、GenerationPoint 为 Unsupported。FullGeneration 始终为 Unsupported。

26.1、26.2、26.3 的稳定 Native ID 分别为 149、150、151，仍使用 ABI 1 和严格 Native／managed revision 握手。群系 ID 187 为 26.2 起的 `sulfur_caves`，188 为 26.3 起的 `dappled_forest`；`Api.Biomes` 提供各版名称和颜色。26.3 的结构 ID 25 为 `abandoned_camp`，每个区域边长 592 方块；其它版本不把它列入结构目录。

## 同步群系采样

```csharp
using var generator = SeedMap.Api.Create(world);
int[] samples = SeedMap.Api.Generate(
    generator, x: -64, z: -64, width: 32, height: 32, step: 4, y: world.Y);
```

`width`、`height` 是样本数量，`step` 是相邻样本间隔的方块数，可取 `1`、`4`、`16`、`64`、`256`。返回数组按行排列，索引为 `row * width + column`。同一个 generator 应由单个计算任务独占。旧版主世界 scale 1 为方块 Voronoi，4 为 RiverMix，16／64／256 分别为中间 Layer 输出，不能视为对 scale 1 的等距抽样；旧版群系忽略 Y。1.12.2 及以前的公开末地群系始终为 ID 9，内部岛屿分类不向群系目录暴露。

`SampleBiome` 与 `Generate` 使用相同的坐标、维度、步长和边界约定；它为单个 generator 保留 Climate 搜索叶节点，因此调用顺序与原版连续查询一致。状态跨该 generator 的 `SampleBiome` 调用保留，创建新 generator 或调用 `ResetBiomeSampling` 后清除。每个 generator 必须只由一个任务访问。`Generate` 的瓦片不读取或改变有序采样状态，结果仅由世界参数和瓦片坐标决定，适用于并行计算、缓存和相邻瓦片拼接；它不承诺复现任意原版有序查询历史中的 tie 选择。结构查询使用各结构自己的位置谓词和 terrain 阶段，不读写此 `SampleBiome` 状态；现代 Village、Woodland Mansion、Ruined Portal 和 Trail Ruins 的相关基础高度阶段为 Supported，Desert Pyramid、Jungle Temple 的附加近似地形筛选仍为 Experimental，FullGeneration 始终为 Unsupported。

## 精确基础地形高度

```csharp
using var heightGenerator = SeedMap.Api.Create(world);
var capability = SeedMap.Api.GetTerrainCapability(
    "1.20.6", dimension: 0, SeedMap.TerrainHeightmap.OceanFloorWorldGen);
int surfaceY = SeedMap.Api.SampleTerrainHeight(heightGenerator, x: 0, z: 0);
int floorY = SeedMap.Api.SampleTerrainHeight(
    heightGenerator, x: 0, z: 0, SeedMap.TerrainHeightmap.OceanFloorWorldGen);
int[] heights = SeedMap.Api.GenerateTerrainHeights(
    heightGenerator, x: -17, z: -5, width: 32, height: 16, step: 3,
    heightmap: SeedMap.TerrainHeightmap.WorldSurfaceWorldGen,
    cancellation: cancellationToken);
```

这两个 WorldGen 类型精确复现默认、未混合主世界 `NoiseBasedChunkGenerator.getBaseHeight`，支持所有登记的 1.18–1.21.11 发行版及 26.1、26.2、26.3。WORLD_SURFACE_WG 取最高非空气方块，包含水和熔岩；OCEAN_FLOOR_WG 取最高满足原版固体谓词的方块。返回该方块的 `Y + 1`，空列返回 `-64`。默认垂直范围为 `[-64, 320)`；例如 seed 0、1.20.6、(0,0) 的两个返回值为 63 和 56。`World.Y` 不影响高度结果。

基础列包含密度插值与 Aquifer 对空气、固体、水和熔岩的判定，不执行 Surface Rules、Carvers、Features、结构地形变形、升级区块混合或实际模板方块。WorldSurface、OceanFloor、MotionBlocking、MotionBlockingNoLeaves 对应后期区块语义，当前为 Unsupported；不能把对基础列使用同名谓词的高度当作实际完成生成后的 Heightmap。旧版本、下界、末地和自定义参数同样不在此高度 API 的 Exact 范围内。

`TerrainCapability` 报告 ReleaseKey、Dimension、Heightmap、Accuracy、ProfileKey、MinY 和 Height。Accuracy 的 Exact、Approximate、Experimental、Unsupported 相互区分；当前仅上述两个 WorldGen 类型在现代主世界为 Exact，不以 Cubiomes `mapApproxHeight` 替代。`ProfileKey` 区分四个既有高度档位，以及 `terrain-26.1`、`terrain-26.2`、`terrain-26.3`。版本切换不共享缓存，三版的新高度和结构查询均不读写 generator 的有序群系搜索状态。

区域结果按 `row * width + column` 排列。`step` 为 `1..30_000_000` 的整数方块间距，原点无需对齐；所有采样点须在世界边界内。每边最多 4096 点、总计最多 1,048,576 点，返回数组由调用方持有。托管层每最多 64 点检查取消，Native 不保留输出缓冲区。同一 generator 必须由一个任务独占，多个 generator 可以并行使用；噪声与有限缓存按 Seed、Profile、维度隔离，释放 generator 一并释放。高度与结构调用不改变 `SampleBiome` 的有序状态。

高度查询同步执行，密集大区域应在消费端后台任务中分批调用并缓存。批量入口复用噪声初始化与相邻节点采样，不承诺达到群系瓦片的吞吐量；取消在小段之间生效。

## 异步瓦片、结构与搜索

```csharp
using var engine = new SeedMap.Engine(world);
int[] tile = await engine.TileAsync(
    new SeedMap.Engine.TileKey(Step: 4, X: 0, Z: 0),
    cancellationToken);

var villages = SeedMap.Api.Structures(world.Version, world.Dimension)
    .Where(type => type.Key == "village")
    .ToArray();
var structures = await engine.QueryStructuresAsync(
    new SeedMap.Bounds(-2000, -2000, 2000, 2000),
    villages,
    cancellationToken);

int targetBiome = tile[0];
var location = await engine.FindBiomeAsync(
    targetBiome, centerX: 0, centerZ: 0, radius: 1024, ct: cancellationToken);
```

`Engine` 返回 `128 × 128` 群系 ID 瓦片，起点为 `(TileKey.X * 128 * Step, TileKey.Z * 128 * Step)`，负坐标同样按瓦片网格处理。引擎提供两个并发工作槽、取消、48 MiB LRU 瓦片缓存和结构区域缓存。缓存命中可能返回同一数组实例，消费端应只读使用；需要修改时先复制数组。

`QueryStructuresAsync` 返回 `Hits` 和 `Limited`。当某类结构的候选区域过多时，该类查询会被跳过并设置 `Limited=true`，消费端可以缩小查询范围或减少结构类型。`FindBiomeAsync` 按 32 方块网格向外搜索，并按单方块采样确认命中位置；它可能遗漏较小群系，返回 `null` 表示此次采样未找到结果。

释放 `Engine` 会取消其生命周期内仍在执行的工作并清空缓存。调用方应等待或处理已有任务的取消结果，再结束自己的视口或会话。

## 相机与瓦片计划

```csharp
var camera = new SeedMap.Camera(centerX: 0, centerZ: 0, pixelsPerBlock: .25);
camera.ZoomAt(1.25, anchorX: 600, anchorY: 380, viewportWidth: 1200, viewportHeight: 760);
var bounds = camera.VisibleBounds(viewportWidth: 1200, viewportHeight: 760);

int step = camera.SelectSamplingStep();
var visible = SeedMap.TilePlanner.Visible(bounds, step);
var wanted = SeedMap.TilePlanner.WithPrefetch(bounds, step, rings: 1);
var batch = SeedMap.TilePlanner.OrderForLoading(
    wanted, visible, camera.CenterX, camera.CenterZ, maximum: 2);
```

`ScreenToWorld` 和 `WorldToScreen` 在屏幕与世界坐标之间转换。`ZoomAt` 围绕屏幕锚点缩放；世界边界钳制可能限制相机移动。`SelectSamplingStep` 返回 `1`、`4`、`16`、`64`、`256` 中的一个值。瓦片计划优先安排可见瓦片，再按距视野中心的距离排序；消费端负责调度任务和创建图像对象。

## 颜色、几何与 BGRA

```csharp
var biomes = SeedMap.Api.Biomes(world.Version, world.Dimension)
    .ToDictionary(biome => biome.Id);
var selected = new HashSet<int> { 1, 4, 5 };
byte[] pixels = SeedMap.Palette.ToBgra(
    tile, biomes, selected, hideUnselected: false, darkTheme: true);

double distance = SeedMap.Geometry.Distance(0, 0, 300, 400);
SeedMap.Bounds chunk = SeedMap.Geometry.ChunkBounds(10, -4);
```

BGRA 缓冲区按行排列，每个像素四个字节，顺序为 B、G、R、A，当前转换结果的 Alpha 为 `255`。颜色值采用 `0xAARRGGBB`。`Palette.WriteBgra` 可以写入调用方提供的缓冲区；`Geometry.GridLines`、`Circle` 和 `ChunkBounds` 提供叠加层与测量所需的几何数据。

## 离屏导出

```csharp
uint[] colors = Enumerable.Range(0, 256)
    .Select(id => SeedMap.Palette.ResolveBiomeColor(id, biomes))
    .ToArray();
var profile = new SeedMap.Profile { Seed = world.Seed };
var image = await SeedMap.Export.RenderAsync(
    world,
    new SeedMap.Bounds(-1024, -1024, 1024, 1024),
    step: 4,
    colors: colors,
    profile: profile,
    ct: cancellationToken);
```

`Export.Image.Pixels` 是 BGRA8 缓冲区，尺寸由 `Width`、`Height` 给出，单次导出最多 `16_777_216` 像素。输出起点按采样步长向下对齐。`Profile` 提供网格、史莱姆区块、结构选择和路径点；`StructuresLimited` 表示部分结构查询因工作量上限被跳过。消费端负责将像素编码为 PNG 等文件格式。

## 会话与存储

`Session` 管理 `Workspace`、`Profile`、历史和路径点。应用先调用 `Load()`，通过 `Select()` 选择世界，通过 `Notify()` 通知配置变更，再按需要调用 `Save()`。SDK 不决定应用数据目录，宿主可提供 `FileSessionStore`，也可实现 `ISessionStore` 接入数据库、云同步或内存存储。
