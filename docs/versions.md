# Minecraft Java 发行版本与验证状态

当前公开源码目录包含 89 个发行版本条目：1.13.2–1.21.11 的 43 个发行版、1.4.2–1.12.2 的 43 个发行版和 26.1、26.2、26.3。1.2.5、1.3.1、1.3.2 仍保留精确 Native ID、研究资料和本地回归入口，但暂不出现在 `Api.Versions` 与公开能力目录。各发行版分别使用精确原版 JAR 验证；消费端运行 SDK 无需 Java 或游戏 JAR。

`Supported` 针对对应能力与 DEFAULT 世界参数；结构能力按 RawPlacement、BiomePredicate、Candidates、GenerationPoint、Terrain 和 FullGeneration 阶段分别记录。`Experimental` 表示精确 JAR 对照已发现阶段限制，或该阶段尚无生产等价实现。`Api.Biomes` 是 DEFAULT 可生成群系目录，省略原版注册表中未用于该世界类型的条目。原版完整地形、结构方块落地、升级世界和自定义生成参数不由候选等级覆盖。`StructureHit.Checks` 只报告本次 Native 查询实际执行的阶段检查，未设置的标志不代表该阶段已验证。1.2.5、1.3.1、1.3.2 的强要塞在独立种子基准通过，任意 IntCache 历史状态仍标为 `Experimental`。

## 26.1–26.3

| 发行版本 | 稳定 Native ID | Biome Profile | 群系／三维度 | 结构候选／Stronghold | 主世界两种 WorldGen 高度 |
| --- | ---: | --- | --- | --- | --- |
| 26.1 | 149 | cubiomes-26.1 | Supported | Supported | Exact：terrain-26.1 |
| 26.2 | 150 | cubiomes-26.2 | Supported，新增 sulfur_caves（187） | Supported | Exact：terrain-26.2 |
| 26.3 | 151 | cubiomes-26.3-rtree19 | Supported，新增 dappled_forest（188） | Supported，新增 Abandoned Camp（25） | Exact：terrain-26.3 |

三个版本分别完成生产 DLL 对照：每版 36,000 个三维度 quart／block 新样本和 18,432 个历史样本；两种 WorldGen 高度各 3,760 个结果及 3,760 个完整基础列；RawPlacement、128 Stronghold 和 Mineshaft 使用各版原版输入单独回放。26.3 保留原版 8,018 个节点、7,594 个叶节点和最多 19 子节点的 RTree，搜索顺序与严格距离比较共同决定 tie，不能由参数 bounds 或六阶重排表替代。

26.x 的 Structure Profile 为 `java-<发行版>-structure-sets`。Village 和主世界 Ruined Portal 按原版 StructureSet 顺序和权重抽取，失败后移除该项并继续；每个变体重新建立自身的 generation RNG。26.3 的零 air-pocket 概率跳过随机调用。查询结果仍为候选区块的方块坐标。26.3 Camp 的 18 个 variants、十种等权起始帐篷、旋转、Java 负数中心取整、Jigsaw 高度投影和 biome predicate 均纳入生产路径；54 个原有有效点及负例、18 个覆盖全部 variants 的新增有效区域和最终加权选择已分别对照。

| 26.x 结构阶段 | RawPlacement | BiomePredicate | Candidates | GenerationPoint | Terrain | FullGeneration |
| --- | --- | --- | --- | --- | --- | --- |
| Village／主世界 Ruined Portal | Supported | Supported | Supported | Supported | Supported | Unsupported |
| Abandoned Camp（仅 26.3） | Supported | Supported | Supported | Supported | Supported：起点投影 | Unsupported |
| Mansion／Trail Ruins | Supported | Supported | Supported | Supported | Supported | Unsupported |
| Desert Pyramid／Jungle Temple | Supported | Supported | Supported | Supported | Experimental：附加近似筛选 | Unsupported |
| Mineshaft | Supported | Unsupported | Supported：placement-only | Unsupported | Unsupported | Unsupported |
| Nether Ruined Portal | Supported | Unsupported | Supported：placement-only | Unsupported | Unsupported | Unsupported |

26.x 的其它结构按 `StructureCapabilities` 中具体阶段读取。三版 Exact 均限定默认、未混合主世界的 `WORLD_SURFACE_WG` 和 `OCEAN_FLOOR_WG`，垂直范围 `[-64,320)`。26.1／26.2 分别保存密度计算图；26.2 的乘法和插值位置变化、26.3 的 float Perlin／NoiseStack、体积插值、Aquifer exclusion 与单列查询范围均保留版本边界。26.3 另以 28,416 个最终密度值和六组各 444 个标量值完成 float 位模式对照。Nether／End 的完整高度 API、后期 Heightmap 和完整方块世界保持 Unsupported。

## 1.13.2–1.21.11

| 发行版本 | 稳定 Native ID | 群系 | 结构候选 | 维度 | 强要塞 | 原版验证 |
| --- | ---: | --- | --- | --- | --- | --- |
| 1.13.2 | 16 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.14 | 147 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.14.1 | 17 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.14.2 | 17 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.14.3 | 17 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.14.4 | 17 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.15 | 18 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.15.1 | 18 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.15.2 | 18 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.16 | 19 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.16.1 | 19 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.16.2 | 20 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.16.3 | 20 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.16.4 | 20 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.16.5 | 20 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.17.1 | 21 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.18 | 148 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.18.1 | 148 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.18.2 | 22 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.19 | 23 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.19.1 | 23 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.19.2 | 23 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.19.3 | 24 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.19.4 | 24 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.20 | 25 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.20.1 | 25 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.20.2 | 25 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.20.3 | 25 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.20.4 | 25 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.20.5 | 25 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.20.6 | 25 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21 | 26 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.1 | 26 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.2 | 27 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.3 | 27 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.4 | 28 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.5 | 146 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.6 | 146 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.7 | 146 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.8 | 146 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.9 | 29 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.10 | 29 | Supported | Supported | Supported | Supported | GameJarAlgorithms |
| 1.21.11 | 29 | Supported | Supported | Supported | Supported | GameJarAlgorithms |

1.18–1.21.11 的 `Api.SampleBiome` 已通过每个精确 JAR 234,864 个有效样本、2,646 个边界拒绝的有序回放，27 版共 6,341,328 个有效样本且零差异。原版 `Climate.ParameterList` 会把上次命中的叶节点用于等距离搜索；该状态在连续使用的 biome source／sampler 实例内延续，`SampleBiome` 将它保存在独占 generator 上，`ResetBiomeSampling` 清除上次命中状态。`Api.Generate` 则生成由世界参数和范围决定的独立、确定性瓦片，缓存和瓦片拼接不依赖生成顺序；因此它不声称复现任意原版有序查询历史中的 tie 选择。结构查询不共享此有序采样状态，另按每种结构的 biome、生成点与地形阶段验证。现代 27 版的 RawPlacement、Stronghold、Mineshaft 与登记结构的候选／GenerationPoint 已逐版对照。Village、Woodland Mansion、Ruined Portal、Trail Ruins 使用精确基础高度和 GenerationStub 的三维群系谓词，其相关 Terrain 阶段为 Supported；FullGeneration 不随之晋级。

## 现代基础高度与结构阶段

| 精确高度 Profile | 实际验证发行版本 | 主世界 WORLD_SURFACE_WG／OCEAN_FLOOR_WG | 默认垂直范围 |
| --- | --- | --- | --- |
| terrain-1.18 | 1.18、1.18.1 | Exact | [-64, 320) |
| terrain-1.18.2 | 1.18.2 | Exact | [-64, 320) |
| terrain-1.19 | 1.19、1.19.1、1.19.2、1.19.3、1.19.4、1.20、1.20.1、1.20.2、1.20.3、1.20.4、1.20.5、1.20.6、1.21、1.21.1、1.21.2、1.21.3、1.21.4、1.21.5、1.21.6、1.21.7、1.21.8 | Exact | [-64, 320) |
| terrain-1.21.9 | 1.21.9、1.21.10、1.21.11 | Exact | [-64, 320) |

高度以原版 `getBaseHeight` 为合同，每版两个类型共 756 个严格整数高度样本，包含不同种子、正负坐标、世界边界和跨 cell／chunk 采样。1.18、1.18.2、1.19、1.20.6、1.21.11 另有完整基础列状态对照。返回最高满足谓词方块的 Y+1；最低空列为 -64。能力只适用于默认、未混合主世界的基础噪声列，旧版、其它维度和后期 Heightmap 类型为 Unsupported。`mapApproxHeight` 仍为 Approximate，未用于这些 Exact 查询。

| 现代结构 | RawPlacement | BiomePredicate | Candidate | GenerationPoint | Terrain | FullGeneration |
| --- | --- | --- | --- | --- | --- | --- |
| Village | Supported | Supported | Supported | Supported | Supported：Jigsaw 高度投影 | Unsupported |
| Woodland Mansion | Supported | Supported | Supported | Supported | Supported：旋转四角最低高度 | Unsupported |
| Ruined Portal（主世界） | Supported | Supported | Supported | Supported | Supported：模板变换、列谓词与 suitable Y | Unsupported |
| Trail Ruins（1.20 起） | Supported | Supported | Supported | Supported | Supported：起始高度与投影 | Unsupported |
| Desert Pyramid／Jungle Temple | Supported | Supported | Supported | Supported | Experimental：额外近似筛选 | Unsupported |
| Mineshaft | Supported | Unsupported：不适用 | Supported：placement-only | Unsupported：不查询 stub | Unsupported | Unsupported |

Terrain 的 Supported 仅覆盖表中已复现的基础高度／生成点规则，不能解释为完成方块落地或结构地形变形。WorldSurface、OceanFloor、MotionBlocking、MotionBlockingNoLeaves 是后期区块语义，目前为 Unsupported；原版对基础列应用同名谓词的 `getBaseHeight` 结果不代表完整生成后的 Heightmap。

## 历史公开版本

| 发行版本 | 稳定 Native ID | 群系／维度 | 结构候选 | 强要塞 | 完整方块生成 |
| --- | --- | --- | --- | --- | --- |
| 1.4.2 | 103 | Supported | Supported | Supported | Unsupported |
| 1.4.4 | 104 | Supported | Supported | Supported | Unsupported |
| 1.4.5 | 105 | Supported | Supported | Supported | Unsupported |
| 1.4.6 | 106 | Supported | Supported | Supported | Unsupported |
| 1.4.7 | 107 | Supported | Supported | Supported | Unsupported |
| 1.5.1 | 108 | Supported | Supported | Supported | Unsupported |
| 1.5.2 | 109 | Supported | Supported | Supported | Unsupported |
| 1.6.1 | 110 | Supported | Supported | Supported | Unsupported |
| 1.6.2 | 111 | Supported | Supported | Supported | Unsupported |
| 1.6.4 | 112 | Supported | Supported | Supported | Unsupported |
| 1.7.2 | 113 | Supported | Supported | Supported | Unsupported |
| 1.7.3 | 114 | Supported | Supported | Supported | Unsupported |
| 1.7.4 | 115 | Supported | Supported | Supported | Unsupported |
| 1.7.5 | 116 | Supported | Supported | Supported | Unsupported |
| 1.7.6 | 117 | Supported | Supported | Supported | Unsupported |
| 1.7.7 | 118 | Supported | Supported | Supported | Unsupported |
| 1.7.8 | 119 | Supported | Supported | Supported | Unsupported |
| 1.7.9 | 120 | Supported | Supported | Supported | Unsupported |
| 1.7.10 | 121 | Supported | Supported | Supported | Unsupported |
| 1.8 | 122 | Supported | Supported | Supported | Unsupported |
| 1.8.1 | 123 | Supported | Supported | Supported | Unsupported |
| 1.8.2 | 124 | Supported | Supported | Supported | Unsupported |
| 1.8.3 | 125 | Supported | Supported | Supported | Unsupported |
| 1.8.4 | 126 | Supported | Supported | Supported | Unsupported |
| 1.8.5 | 127 | Supported | Supported | Supported | Unsupported |
| 1.8.6 | 128 | Supported | Supported | Supported | Unsupported |
| 1.8.7 | 129 | Supported | Supported | Supported | Unsupported |
| 1.8.8 | 130 | Supported | Supported | Supported | Unsupported |
| 1.8.9 | 131 | Supported | Supported | Supported | Unsupported |
| 1.9 | 132 | Supported | Supported | Supported | Unsupported |
| 1.9.1 | 133 | Supported | Supported | Supported | Unsupported |
| 1.9.2 | 134 | Supported | Supported | Supported | Unsupported |
| 1.9.3 | 135 | Supported | Supported | Supported | Unsupported |
| 1.9.4 | 136 | Supported | Supported | Supported | Unsupported |
| 1.10 | 137 | Supported | Supported | Supported | Unsupported |
| 1.10.1 | 138 | Supported | Supported | Supported | Unsupported |
| 1.10.2 | 139 | Supported | Supported | Supported | Unsupported |
| 1.11 | 140 | Supported | Supported | Supported | Unsupported |
| 1.11.1 | 141 | Supported | Supported | Supported | Unsupported |
| 1.11.2 | 142 | Supported | Supported | Supported | Unsupported |
| 1.12 | 143 | Supported | Supported | Supported | Unsupported |
| 1.12.1 | 144 | Supported | Supported | Supported | Unsupported |
| 1.12.2 | 145 | Supported | Supported | Supported | Unsupported |

## 实际结构能力

每版只列出原版存在的结构：公开版本全部有矿井、村庄、Nether Fortress 和强要塞；内部保留的 1.2.5、1.3.1、1.3.2 也有独立结构回归。神殿从内部研究范围的 1.3.1 起验证，女巫小屋从 1.4.2 起验证，Monument 从 1.8 起验证，Igloo／End City 从 1.9 起验证，Mansion 从 1.11 起验证。`StructureCapabilities` 按实际结构列出 RawPlacement、BiomePredicate、Candidates、GenerationPoint、Terrain 和 FullGeneration。Mineshaft 的公开查询是 placement-only，其 BiomePredicate 与 GenerationPoint 为 Unsupported。1.13.2–1.17.1 的登记结构候选与位置谓词在精确 JAR 样本中匹配；其中精确 1.14 以及 1.14.1–1.14.4 的 Ocean Ruin、Shipwreck、Village 对照未复现旧报告的大批差异。旧 `3943/829` 统计来自研究工具对稳定 Native ID 的 profile 映射错误，已作废。现代 Desert Pyramid 和 Jungle Temple 的候选与生成点已对照；两者的附加近似 Terrain 阶段保留 Experimental。现代 Village、Mansion、Ruined Portal 与 Trail Ruins 的候选／生成点和相关基础高度已达到 Supported；通用谓词按 generation stub 实际 `(x,y,z)` 调用 `getNoiseBiome`。FullGeneration 始终为 Unsupported。

End City 的 1.9–1.10.2 位置谓词没有高度过滤；1.11–1.12.2 复现原版 ChunkPrimer 索引和旋转 seed 的行为。仅这一位置谓词所需的高度检查被验证；完整末地地形及末地城方块放置仍未声明支持。Mansion 的支持同样只到 canSpawn 群系及位置谓词。

## 算法 Profile

群系实现分为四组：1.2.5；1.3.1–1.6.4；1.7.2–1.8.9 与 1.11–1.12.2；1.9–1.10.2。1.9／1.10 的 birch mutation 中间层与前后版本不同，不能合并。结构另选 Profile：1.4.2 的 quart 判定与组件分类、1.8 的 Monument、1.11 开始的末地城高度规则均有独立边界。

Profile 共享不合并发行身份。46 个历史发行版本分别保留稳定 Native ID、验证等级和独立研究／回归身份；其中 43 个进入公开发行目录，100–102 仅供内部研究和回归使用。`BiomeProfileKey`、`StructureProfileKey` 是算法标识，不用作发行版本名称或跨世界缓存键。

## 使用能力目录

```csharp
var capability = RZeroF1.Minecraft.SeedMap.Api.GetVersionCapability("1.12.2");
Console.WriteLine(capability.Verification); // GameJarAlgorithms
Console.WriteLine(capability.Strongholds);
foreach (var structure in capability.StructureCapabilities)
    Console.WriteLine($"{structure.Key}: candidate={structure.Candidates}, full={structure.FullGeneration}");
```

范围外的正式 1.0.0／1.1.0、Bedrock、自定义世界参数和混合版本存档没有在本轮新增。原版资源、映射和研究工具保留为内部验证材料，不随源码包或 NuGet 分发。
