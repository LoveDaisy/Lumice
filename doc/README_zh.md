[English version](README.md)

# 文档索引

Lumice 文档按受众与主题组织。产品使用从用户手册开始；参与开发先读架构与开发指南；深入具体子系统时进入对应专题目录。

## 从这里开始

### 用户

- [用户手册](user-manual/00-overview_zh.md)：安装、GUI 与 CLI 快速上手、配方、FAQ、光路分析和配置摘要
- [配置参考](configuration_zh.md)：JSON 配置字段、默认值与校验规则
- [GUI 指南](gui/gui-guide_zh.md)：应用工作流、预览与 `.lmc` 项目文件
- [C API 参考](interfaces/c_api_zh.md)：引擎 API 用法、错误、所有权与集成

### 开发者

- [系统架构](architecture_zh.md)：模块、依赖与数据流
- [开发指南](developer-guide_zh.md)：环境配置、编码约定、测试与调试
- [测试架构](testing/testing-architecture_zh.md)：测试分层、归属、执行范围与比较标尺
- [环境变量策略](env-var-policy.md)：环境开关的决策闸与单一 owner
- [贡献指南](../CONTRIBUTING.md)：分支、Pull Request、发布与贡献流程

## 专题文档

### 几何

坐标约定、晶体表达、取向采样、数值谓词与几何随机化行为：

- [坐标约定](geometry/coordinate-convention_zh.md)（[English](geometry/coordinate-convention.md)）
- [晶体几何表达](geometry/crystal-geometry-representation.md)
- [晶体取向采样](geometry/crystal-orientation-sampling_zh.md)（[English](geometry/crystal-orientation-sampling.md)）
- [几何随机化性能](geometry/geometry-randomization-perf.md)
- [几何随机化价值与测量](geometry/geometry-randomization-value-and-measurement.md)
- [近极面积测度采样](geometry/near-pole-area-measure-sampling.md)
- [数值鲁棒性](geometry/numerical-robustness.md)

### GUI

GUI 使用、状态归属、生命周期、布局历史与视觉语言：

- [GUI 指南](gui/gui-guide_zh.md)（[English](gui/gui-guide.md)）
- [自定义光谱与光路染色](gui/gui-custom-spectrum-and-raypath-color.md)
- [GUI 预览生命周期架构](gui/gui-preview-lifecycle-architecture.md)
- [GUI 状态治理](gui/gui-state-governance.md)
- [GUI 视觉语言](gui/gui-visual-language.md)
- [GUI 布局架构与处置记录](gui/gui-layout-architecture.md)

### 光路

光路表达、过滤与对称、分析产品、报告 schema 与诊断研究：

- [光路与光段架构](raypath/raypath-rayseg-architecture.md)
- [Filter 架构](raypath/filter-architecture_zh.md)（[English](raypath/filter-architecture.md)）
- [光路对称](raypath/raypath-symmetry.zh.md)（[English](raypath/raypath-symmetry.md)）
- [光路分析](raypath/raypath-analysis.md)
- [光路分析面板](raypath/raypath-analysis-panel.md)
- [光路 CLI 输出](raypath/raypath-cli-output.md)
- [Schema 3 分层设计](raypath/raypath-schema3-layered-design.md)
- [特征诊断验收](raypath/raypath-feature-diagnostic-acceptance.md)
- [特征诊断发现研究](raypath/feature-diagnostic-discovery-research.md)

### 渲染

累积与消费、曝光、投影注解，以及屏幕/印刷显示模型：

- [累积器—消费者架构](rendering/accumulator-consumer-architecture.md)
- [自适应亮度](rendering/adaptive-brightness.zh.md)（[English](rendering/adaptive-brightness.md)）
- [EV 管线架构](rendering/ev-pipeline-architecture.md)
- [Overlay 标签放置](rendering/overlay-label-placement.md)
- [Print 模式减色墨](rendering/print-mode-subtractive-ink.md)

### 后端

Host/device seam 与已验证的 GPU 执行架构：

- [TraceBackend seam 设计](backends/seam-design.md)
- [Trace backend 帧生命周期](backends/trace-backend-frame-lifecycle.md)
- [GPU 单引擎实现](backends/gpu-single-engine-implementation.md)
- [GPU 路线历史](backends/gpu-route-history.md)

### 测试

测试组织、性能测量、验证机器与远端验证流程：

- [测试架构](testing/testing-architecture_zh.md)（[English](testing/testing-architecture.md)）
- [性能测试](testing/performance-testing_zh.md)（[English](testing/performance-testing.md)）
- [验证机器](testing/machines.md)
- [远端 CUDA 构建与测试](testing/gpu-remote-cuda-build-testing.md)
- [Windows 远端测试](testing/windows-remote-testing_zh.md)（[English](testing/windows-remote-testing.md)）
- [XYZ 统计工具](testing/xyz-stats-tool.md)

### 接口

公开 C 接口、生命周期规则、打包边界与产品线决策：

- [C API 参考](interfaces/c_api_zh.md)（[English](interfaces/c_api.md)）
- [C API 生命周期架构](interfaces/capi-lifecycle-architecture.md)
- [Analytic API](interfaces/analytic-api.md)
- [API 分层与产品线](interfaces/api-layering-and-product-lines.md)

## 其他文档集合

- [`user-manual/`](user-manual/00-overview_zh.md)：面向任务的产品文档
- `research/`：保留原有状态与范围的研究记录
  - [逆向渲染研究](research/inverse-rendering_zh.md)（[English](research/inverse-rendering.md)）
  - [光束追迹研究](research/beam-tracing_zh.md)（[English](research/beam-tracing.md)）
- [`cases/`](cases/raypath-analysis-cases.md)：已登记的光路分析测验场景语料
- [`prototypes/`](prototypes/analyze-workspace.html)：交互式设计原型
- [`api/`](api/html/)：Doxygen 输出；用 `doxygen .doxygen-config` 在本地生成
- [`figs/`](figs/)：文档共用图片

## 根目录文档

`doc/` 根目录只保留入口与跨主题参考：

- [文档索引](README_zh.md)（[English](README.md)）
- [系统架构](architecture_zh.md)（[English](architecture.md)）
- [配置参考](configuration_zh.md)（[English](configuration.md)）
- [开发指南](developer-guide_zh.md)（[English](developer-guide.md)）
- [环境变量策略](env-var-policy.md)

## 语言与生成文档

英文是默认语言。存在中文伙伴的文档使用 `_zh.md` 或 `.zh.md` 后缀；并非每篇专题设计记录都有双语版本。源码注释和生成的 API 文档使用英文。

在仓库根目录生成 Doxygen 文档：

```bash
doxygen .doxygen-config
```

生成结果仍位于 `doc/api/html/`。

## 项目链接

- [项目英文 README](../README.md)
- [项目中文 README](../README_zh.md)
- [配置示例](../examples/config_example.json)
