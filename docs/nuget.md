# NuGet 与发布

[项目首页](../README.md) · [API 说明](api.md) · [架构说明](architecture.md)

## 包信息

| 项目 | 内容 |
|---|---|
| 包 ID | `RZeroF1.Minecraft.SeedMap` |
| 当前源码版本 | `0.3.0`（本地未发布修改） |
| 目标框架 | `net10.0` |
| 原生运行时 | `win-x64` |
| 项目许可证 | MPL-2.0 |
| Cubiomes 许可证 | MIT，保留 Cubitect 的版权声明 |
| 源码仓库 | [RZeroF1/Minecraft-SeedUniverse](https://github.com/RZeroF1/Minecraft-SeedUniverse) |

NuGet 包的已发布版本以 [nuget.org](https://www.nuget.org/packages/RZeroF1.Minecraft.SeedMap) 为准，源码标签和更新说明见 [GitHub Releases](https://github.com/RZeroF1/Minecraft-SeedUniverse/releases)。

## 安装

当前源码构建的本地包使用版本 `0.3.0`；消费端使用 .NET 10 和 Windows x64，并固定包版本：

```xml
<PropertyGroup>
  <TargetFramework>net10.0</TargetFramework>
  <RuntimeIdentifier>win-x64</RuntimeIdentifier>
</PropertyGroup>

<ItemGroup>
  <PackageReference Include="RZeroF1.Minecraft.SeedMap" Version="0.3.0" />
</ItemGroup>
```

尚未发布的源码版本可使用本地打包流程验证。使用已发布包不需要安装 Visual Studio C++ Build Tools。

## 包内容

包中包含托管程序集、XML API 注释、Windows x64 原生后端、中英文 README、中文使用文档和英文许可证与来源信息：

```text
lib/net10.0/
    RZeroF1.Minecraft.SeedMap.dll
    RZeroF1.Minecraft.SeedMap.xml
runtimes/win-x64/native/
    rzero_seedmap.dll
README.md
README.en.md
docs/
    api.md
    architecture.md
    nuget.md
    versions.md
LICENSE
THIRD-PARTY-NOTICES.md
licenses/cubiomes/
    LICENSE
    SOURCE.md
    manifest.json
```

`.snupkg` 另行提供 Portable PDB 和 SourceLink 信息。公开文档按明确文件清单打包；新增使用文档时同步更新项目文件和 CI 中的产物检查。

## 本地构建与打包

源码构建需要 Windows、.NET 10 SDK 和 Visual Studio C++ Build Tools。在仓库根目录执行：

```powershell
dotnet build src/RZeroF1.Minecraft.SeedMap.Core/RZeroF1.Minecraft.SeedMap.Core.csproj -c Release -r win-x64
dotnet pack src/RZeroF1.Minecraft.SeedMap.Core/RZeroF1.Minecraft.SeedMap.Core.csproj -c Release -r win-x64
```

包输出到 `artifacts/packages/`。构建过程调用本地 Native 构建脚本，使用固定的 Cubiomes 源码；指定 `win-x86`、`win-arm64` 或其他运行时会失败。

`global.json` 固定 .NET 10 SDK major，并允许使用已安装的更新 feature band；安装更高 major 的 SDK 不会改变本项目的构建工具选择。CI 使用同一约定。

当前公开源码包含 89 个发行版本条目：43 个现代发行版、43 个公开历史发行版及 26.1、26.2、26.3。1.2.5、1.3.1、1.3.2 的 Native ID 与回归实现保留在源码和本地研究环境中，不进入 `Api.Versions` 的公开目录。逐版状态见 [版本矩阵](versions.md)。原版 JAR、Mapping、Oracle 与实验数据不进入包；消费端运行无需 Java。

版本能力由包内 `Api.VersionCapabilities` 返回。实验性 Profile 会随 Native 资产编译，消费端应按各功能的能力状态和已记录限制决定是否启用。

默认主世界的精确基础高度通过 `Api.GetTerrainCapability` 查询。1.18–1.21.11 及 26.1–26.3 的 WORLD_SURFACE_WG 和 OCEAN_FLOOR_WG 数学 Profile 随 Native DLL 编译，消费端运行不需要 Java、游戏 JAR 或额外配置文件；后期区块 Heightmap 不属于这些基础高度能力。单点、区域查询与取消示例见 [API 说明](api.md)。

## 包内容检查

打包后检查公开文件清单与构建输出：

```powershell
./scripts/Test-ArtifactIsolation.ps1 -CheckTracked -PackageDirectory artifacts/packages -OutputDirectory src/RZeroF1.Minecraft.SeedMap.Core/bin/Release/net10.0/win-x64
```

目录输入会读取当前 SDK 项目中的 `PackageVersion`，只检查本次版本对应的 `.nupkg` 和 `.snupkg`；不会枚举历史包。也可以直接传入单个当前包，例如 `-PackagePath artifacts/packages/RZeroF1.Minecraft.SeedMap.0.3.0.nupkg`。需要检查其他版本时显式传入 `-PackageVersion`。

独立消费验证应在单独的 .NET 10 Windows x64 项目中通过 `PackageReference` 还原实际 `.nupkg`，核对版本能力、Native 修订号和实际加载路径。重复打包同一版本进行本地开发验证时，应换用新的包缓存目录；准备发布的产物从最终提交重新构建。

## 发布流程

1. 同步项目版本、中英文 README、使用文档和包更新说明，完成构建与本地验证后提交源码。
2. 从最终提交的干净源码重新构建 `.nupkg` 和 `.snupkg`，核对版本、文档、Native 资产、许可证、仓库地址，以及 `.nuspec` 和 PDB SourceLink 中记录的提交 SHA。
3. 运行独立包消费者，确认实际包还原和 Native DLL 加载成功。仓库 CI 会检查包内容、版本、仓库元数据和当前提交的一致性。
4. 获得发布授权后，将已验证的产物上传到 NuGet，为同一提交创建版本标签和 GitHub Release，并附上对应产物。
5. 发布后从 NuGet 下载实际包，重新核对包内容与提交信息，并检查 GitHub 标签、Release 和 NuGet 页面。

已发布包使用唯一版本号，后续更新递增版本。更新说明面向使用者描述实际变化，构建、测试和包校验结果记录在提交或 PR 中。
