[English version](architecture.md)

# 系统架构

本文描述 Lumice 当前的高层架构。完整 API 声明、配置字段和测试矩阵各有独立权威文档，本文不复制这些易漂移清单。

## 1. 系统形态

Lumice 在同一引擎之上提供两个应用壳：

- `Lumice`：命令行应用；
- `LumiceGUI`：交互式 GUI。

两者都只经 `src/include/` 下的公开 C 能力头使用引擎。GUI 不直接包含引擎的 `core/` 或 `config/` 头。场景提交到 server 后，渲染或分析 session 产生 batch，consumer 把 batch 约化为结果，调用方再获取一帧不可变发布结果。

```text
JSON / GUI 文档
        │
        ▼
公开 C 能力（`lumice_*.h`）
        │
        ▼
场景提交 ──► session 编排 ──► CPU worker 或单个 GPU engine
                                      │
                                      ▼
                                   SimData
                                      │
                 ┌────────────────────┼────────────────────┐
                 ▼                    ▼                    ▼
             渲染 / 锚点             统计             光路直方图
                 └────────────────────┼────────────────────┘
                                      ▼
                           不可变 `ResultFrame`
                                      │
                                      ▼
                              CLI / GUI / C 客户端
```

引擎一次持有一个已提交项目；一个项目含一个 scene 和任意多个 render 条目。

## 2. 受门禁约束的源码分层

`cmake/lumice_layers.cmake` 是引擎分层的单一真源。CMake 用它构建 foundation 对象集，`scripts/check_policies.py` 读取同一份 manifest，拒绝无归属文件和向上层 include。

从低到高：

| 层 | 主要职责 |
|---|---|
| `foundation` | 几何、光学、buffer、host/device 共用原语、基础设施与 base API |
| `analytic` | 对外 analytic kernel 及其 C bridge |
| `scene` | 配置模型、场景构建、scene/editor 能力 |
| `view` | 投影与 annotation view 构建 |
| `sim` | simulator、光线生成、CPU/Metal/CUDA trace backend |
| `render` | consumer、曝光、合成、结果类型 |
| `engine` | server 编排与 engine 能力 |
| `raypath` | 单光路分析与报告装配 |
| `capi` | 其余公开 C bridge 与打包 surface |

源码只能 include 本层或更低层。存量例外列在只减不增的 `cmake/lumice_layer_allowlist.txt`。

应用壳有意不进入该 manifest：

- `src/gui/` 只用引擎的 `lumice_*.h` 能力头，以及 `src/util/` 的纯无状态 helper；
- `src/launcher/` 是 Windows 引擎加载前的壳；
- `src/main.cpp` 是 CLI 壳。

## 3. 运行时 session

### 3.1 场景提交

`src/config/` 把配置 JSON 解析成 scene 模型。GUI 文档也先投影成同一公开 scene 能力再提交。`ServerImpl` 持有已提交场景、session epoch、worker 组、consumer 与发布结果。

普通 commit 可以复用兼容的引擎状态，但会开启新的渲染 epoch。`LUMICE_ContinueRender` 是另一种 append 操作：它在当前兼容累积上追加光线，不重置已有结果。

### 3.2 CPU 与 GPU 路由

`ResolveGpuRoute` 是 server 定型时的单一路由裁决。

- CPU 路使用按平台实测的自动 worker 数；调用方可显式指定，固定 seed 的运行按契约只用一个 worker。
- Metal 与 CUDA 走 GPU 单引擎路：由一个 simulator 驱动大 dispatch，而不是照抄 CPU 多 worker 管线。
- 光路分析 session 始终走 CPU。按 GPU 路定型的 server 常备一组 CPU 分析 worker；CPU server 复用普通 worker 组。

后端 seam 是 `TraceBackend`。host 拥有 scene/config 语义，backend 拥有设备执行。详细契约见 `backends/seam-design.md` 与 `backends/trace-backend-frame-lifecycle.md`。

### 3.3 Batch 生产与消费

worker 通过 server 队列发布 `SimData` batch。payload 会随路由成为 legacy 出射光线、worker-projected sidecar 或 device-fused plane。一个 server-owned session 下的 consumer 解释这些 payload：

- `RenderConsumer`：每个 render 的累积、投影、曝光、annotation 与可选 color-class lane；
- `AnchorConsumer`：每个渲染 session 一份的场景级 relative-exposure 锚；
- `StatsConsumer`：光线与几何统计；
- `RaypathHistogramConsumer`：分析 session 的完整光路能量聚合。

渲染 session 为每个 render 条目建立一个 `RenderConsumer`，另有一个 session 级 anchor consumer 与统计。分析 session 则以光路直方图 consumer 加统计替代渲染/锚点 consumer。

### 3.4 发布帧与完成判定

`DoSnapshot()` 发布新的不可变 `ResultFrame`，不会覆写已经交给读者的 buffer。C 调用方必须获取 `LUMICE_ResultFrame*`，读取所需结果族，再释放该帧；句柄就是读者对结果生命周期持有的真实份额。

仿真生命周期与结果排空是两个事实：

- `LUMICE_GetSimLifecycle` 报告生产处于 running、stopping 还是 complete；
- `LUMICE_GetDrainStatus` 报告本 epoch 产生的 batch 是否已全部到达 consumer；
- `LUMICE_AcquireResultFrame` 获取 render、raw XYZ、composite、stats 或 analysis 的同世代快照。

最终结果要求生产完成且 drain 完成。完整 C 契约见 `interfaces/c_api_zh.md` 与 `interfaces/capi-lifecycle-architecture.md`。

## 4. 几何、渲染与分析

### 几何与仿真

晶体是参数化的有限小族。平面、稳定面号槽位与闭式轮廓是主表达；需要多边形或三角形的消费者再由其派生。CPU 与 GPU 路共享面编号、光学原语、投影约定和 host 侧场景语义。

修改几何或谓词前先读 `geometry/crystal-geometry-representation.md`、`geometry/coordinate-convention_zh.md` 与 `geometry/numerical-robustness.md`。

### 渲染与曝光

渲染以线性 XYZ 累积。mono relative 曝光锚在固定全天等面积域上测量；absolute 曝光使用 emitted energy。显示转换、print、annotation 与 color-class 合成在累积后按 render 配置执行。

权威公式与 owner 见 `rendering/accumulator-consumer-architecture.md` 和 `rendering/ev-pipeline-architecture.md`。

### 光路产品

两种产品彼此不同：

- `analyze`：专用 CPU Monte Carlo session，按天空 ROI 排完整光路链的能量；
- `raypath`：由 `src/raypath/` 基于 analytic 能力实现的确定性单光路 fiber / feature 分析与 JSON 报告。

光路模块只向 engine 暴露已登记的公开头；`src/raypath/detail/` 保持内部实现。见 `raypath/raypath-analysis.md` 与 `raypath/raypath-cli-output.md`。

## 5. 公开 surface 与打包

项目没有 umbrella `lumice.h`。引擎能力拆为：

- `lumice_base.h`
- `lumice_scene.h`
- `lumice_render.h`
- `lumice_editor.h`
- `lumice_engine.h`
- `lumice_raypath.h`

analytic 计算能力使用 `lumice_analytic_core.h`；独立 analytic 库另以 `lumice_analytic.h` 提供库管理函数。

shared 构建产生三个不同库：

- `liblumice`：产品引擎能力；
- `liblumice_testapi`：同一引擎对象加测试专用 hook；
- `liblumice_analytic`：仅 foundation + analytic kernel（受支持的非 CUDA shared 构建）。

每个包的导出列表由自身头文件生成，并与实际二进制导出表对账。引擎与 analytic 库各自独立版本。见 `interfaces/api-layering-and-product-lines.md` 与 `interfaces/analytic-api.md`。

## 6. GUI 边界

`src/gui/` 是应用壳，不是第二份引擎实现。它负责：

- 交互式文档态与显示态；
- 经公开 C API 构建和提交 scene；
- 轮询 lifecycle、drain 与获取的结果帧；
- 把引擎的全天预览纹理重投影到交互式镜头；
- 将 `.lmc` 持久化为含 JSON 状态与可选预览 payload 的二进制容器；
- 通过另一份 GUI 状态投影导出 CLI-compatible config JSON。

仿真 commit 投影与 JSON export 投影在 GUI 显示语义要求处有意分叉。见 `gui/gui-state-governance.md` 与 `gui/gui-preview-lifecycle-architecture.md`。

## 7. 构建与验证边界

主要 target：

- `lumice` 与 `Lumice`：引擎库和 CLI；
- `BUILD_GUI=ON` 时的 `LumiceGUI` 与 `lumice_gui_obj`；
- 对应选项启用时的 unit、composition、parity、golden、GUI、E2E、performance 与 regression 测试。

使用 `scripts/build.sh` 与 `scripts/test.sh`，不要手工重建 target 组合。测试归属和执行范围以 `testing/testing-architecture_zh.md` 为准；贡献命令与源码边界见 `developer-guide_zh.md`。

## 相关文档

- [配置参考](configuration_zh.md)
- [开发指南](developer-guide_zh.md)
- [C API 参考](interfaces/c_api_zh.md)
- [后端 seam 设计](backends/seam-design.md)
- [累积器—消费者架构](rendering/accumulator-consumer-architecture.md)
- [GUI 状态治理](gui/gui-state-governance.md)
- [测试架构](testing/testing-architecture_zh.md)
