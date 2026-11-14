# 架构说明

[项目首页](../README.md) · [API 说明](api.md) · [NuGet 与发布](nuget.md)

SeedMap 面向 .NET 10 消费端提供可复用的数据、计算、几何和图像缓冲区能力。UI 控件、窗口、主题、动画、输入事件、剪贴板和文件选择器由消费端实现。

## 模块职责

```text
消费端应用
  ├─ UI 状态、输入、图像对象、动画和窗口生命周期
  └─ RZeroF1.Minecraft.SeedMap
       ├─ Api：Native ABI、SafeHandle、版本 Profile 能力表和同步计算
       ├─ Terrain：类型化高度能力、单点和区域基础噪声列查询
       ├─ Engine：异步瓦片、取消、结构区域查询、附近群系搜索、LRU
       ├─ Camera：屏幕与世界坐标、视野、锚点缩放和采样步长
       ├─ TilePlanner：可见瓦片、预取范围和加载优先级
       ├─ Palette：群系颜色和 BGRA8 转换
       ├─ Geometry：距离、网格、区块边界和测量几何
       ├─ Export：离屏 BGRA 导出
       └─ Session：工作区、配置、历史、路径点和存储接口
            │
            ▼
       rzero_seedmap.dll
            │
            ▼
       固定修订版 Cubiomes C 源码
```

`World`、`Bounds`、群系与结构元数据、会话模型和渲染几何类型均定义在 `RZeroF1.Minecraft.SeedMap` 命名空间下，供不同消费端复用。`0.3.0` 保留既有调用方式，提供 89 个公开发行版本及三个仅供内部回归的历史 Native ID，并按功能报告验证状态。

## Native 调用与资源

`native/cubiomes/seedmap.c` 和 `seedmap.h` 定义 C ABI 适配层，固定版本的 Cubiomes 源码位于 `native/cubiomes/upstream/`。托管层首次读取能力表时验证 ABI 版本与 Cubiomes 修订号，不匹配时抛出异常。发行版本通过 `VersionCapability` 映射到可复用的 Biome/Structure profile；多个发行版本可以共享一个 profile，但能力状态和验证记录仍按发行版本保存。

历史发行版本使用独立稳定 ABI ID 100–145；其中 100–102 保留给内部精确回归，公开目录从 103 开始。现代发行版保留原有身份并新增精确分界 ID，适配层将发行 ID 映射到经验证的算法参数，ABI ID 不依赖 Cubiomes 枚举顺序。群系与结构 Profile 分开，1.4.2 神殿、1.14 前哨站、1.18 depth 和 1.21 强要塞偏好保留独立规则。Native 修订号包含本地修复标识，构建目标跟踪全部 Native C／H 输入。

Java 26.1、26.2、26.3 使用稳定发行 ID 149、150、151，分别路由 Biome、StructureSet 和 Terrain Profile。版本解析读取完整的 major／minor，保留旧版发行身份。26.3 群系查询保存原版 19 阶 RTree 的节点拓扑、子节点顺序及严格距离比较；`SampleBiome` 保留上次叶节点，`Generate` 仍独立于调用历史。StructureSet 使用各版已核验的权重、移除与重试；变体计算重新初始化自身 RNG，不共享群系采样状态。Abandoned Camp 只保存起点所需的数值、尺寸和谓词，不构建完整 pieces 或方块世界。

每个 Native generator 由 `SafeHandle` 管理，并由一个计算任务独占。同步调用方通过 `using` 释放 generator；异步任务在后台计算中创建并释放自己的 generator。Native DLL 通过 `DllImportSearchPath.AssemblyDirectory` 加载。

## 基础地形高度引擎

`native/cubiomes/terrain.c` 对默认主世界计算基础噪声列。`terrain_tables.h` 保存有限的数学运算图、样条控制点、噪声参数和种子派生常量；构建及运行无需原版 JAR、Mapping、Java、NBT 或 JSON 配置。实现复用 Cubiomes 的随机数和 Perlin 初始化，单独保留原版精度环绕、插值顺序、float 样条、洞穴与 Aquifer 判定。矿脉和 Surface Rules 对目标谓词没有新增高度贡献，未为此重建完整方块生成。

现有 Native handle 在首次高度或相关结构查询时创建 Terrain 状态。七个 Profile 分别覆盖四个既有高度档位及 26.1、26.2、26.3。每个 handle 独占全部可变状态；节点、流体和初步地表缓存容量固定，FlatCache 的失效代次随单 cell 原点变化，普通数学采样可复用。无进程级种子缓存，释放 handle 释放高度状态。

26.1／26.2 使用各自的密度计算图，保留乘法相对插值的位置。26.3 独立实现 float Perlin 梯度和 NoiseStack 权重累加，区分标量 XYZ 与体积 XZ／Y 点积，按原版 Z、X、累加 Y 的体积顺序插值。Aquifer 的 exclusion、preliminary surface、流体噪声与单列查询包络同样使用该版语义。`getBaseHeight`／`getBaseColumn` 在 material rules 之前读取基础列；Surface Rules、雕刻、装饰与结构地形变形不属于这些查询。

WORLD_SURFACE_WG 与 OCEAN_FLOOR_WG 返回 `getBaseHeight` 的首个空闲 Y，结构共用该引擎读取旋转四角高度、Jigsaw 投影或 Portal 的四角列占用分类。列分类只区分空气、固体、水、熔岩，不作为方块 ID。ABI 1 保留既有签名，并附加 `cm_terrain_column` 和 `cm_structure_variant`；后者支持逐变体点和实际加权选择，正式 region 查询复用同一路径。托管层用 SafeHandle 承接生命周期，区域高度查询以最多 64 点分段取消。

## 异步任务与缓存

`Engine` 绑定一个 `World`，为瓦片、结构和附近群系搜索共用两个并发工作槽。调用方取消令牌与引擎生命周期令牌连接，等待工作槽及分批计算时检查取消状态。

`TileAsync` 返回固定 `128 × 128` 的群系 ID 数组。瓦片起点为 `(TileKey.X * 128 * Step, TileKey.Z * 128 * Step)`；相邻瓦片使用同一采样网格。每个数组占用 `65_536` 字节，瓦片缓存按数组数据大小计入 48 MiB LRU 预算，最多容纳 768 个瓦片，不包含字典等管理开销。

不同发行版本即使算法等价，仍使用各自的 World 和 Engine 缓存；切换版本时创建新 Engine，不跨世界共享瓦片、结构和强要塞缓存。

结构区域缓存最多保存 8192 个查询结果，要塞位置在同一引擎内复用。结构查询按候选区域数量限制工作量，通过结果的 `Limited` 字段告知消费端。`Dispose()` 取消剩余工作并清空缓存，已返回的数组仍由消费端持有。

## 视口与图像呈现

`Camera` 与 `TilePlanner` 提供坐标变换、视野、预取和加载顺序计算，使用 SDK 自己的坐标与边界类型。宿主将结果接入自己的任务生命周期、视口状态和图像框架。

`Palette` 处理群系颜色选择和 BGRA8 写入，消费端负责把字节写入具体图像对象。`Export` 使用世界采样、颜色、网格、史莱姆区块、结构和路径点模型直接生成离屏像素，单次导出限制为 `16_777_216` 像素。

## 会话持久化

`Workspace` 保存多个 `Profile` 及当前配置 ID。`Session` 负责配置选择、历史排序和变更通知，通过 `ISessionStore` 读写状态。`FileSessionStore` 接受宿主指定的文件路径，应用可替换为其他存储实现。

## 平台与发布

当前 NuGet 包发布 `runtimes/win-x64/native/rzero_seedmap.dll`。源码项目只接受 `win-x64`，其他 Runtime Identifier 会导致构建失败。消费端的多平台工程应将 SeedMap 功能限制到 Windows x64 配置。

中英文 README、中文使用文档、XML API 注释、许可证和第三方来源信息随 NuGet 包一起分发。文档维护和打包流程见 [NuGet 与发布](nuget.md)。
