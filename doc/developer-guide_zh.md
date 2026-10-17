[English version](developer-guide.md)

# 开发指南

本文是贡献者的实操入口。架构裁决在各专题文档；全仓工程规则在 `AGENTS.md`；分支、Pull Request 与发布流程以 `CONTRIBUTING.md` 为准。

## 1. 前置依赖

Lumice 是 C++17 + CMake + Ninja 项目。常规开发机需要：

- Apple Clang 14+、GCC 11+ 或 MSVC 2022；
- CMake 3.14+；
- Ninja 1.10+；
- Python 3.9+（每次构建都用来嵌入资产，E2E 工具也依赖它）；
- Linux GUI 构建另需 OpenGL、X11 与 GTK 3 开发包。

依赖由 CPM.cmake 拉取，默认按机器缓存在 `$HOME/.cache/lumice-cpm`；`CPM_SOURCE_CACHE` 可覆盖该位置。构建树与安装树仍按 worktree 和库 flavor 分开。

## 2. 构建命令与产物目录

使用仓库脚本：

```bash
./scripts/build.sh -j release       # static CLI / library
./scripts/build.sh -gj release      # 再构建 GUI
./scripts/build.sh -tj release      # 构建测试并运行 CTest correctness labels
./scripts/build.sh -gtj release     # 再运行 gui_test；需要显示服务器
./scripts/build.sh -bj release      # 再构建 benchmark
./scripts/build.sh -sj release      # shared libraries
```

选项可组合：

- `-t`、`-g`、`-b` 决定构建什么；
- 只有 `-s` 改变库 flavor（static → shared）；
- `-k` 删除所选 flavor 的 CMake build/install 树，但保留 compiler output 与机器级 CPM cache；
- `-x` 删除仓库内全部 `build/` 产物，但仍保留机器级 CPM cache。

路径：

| 用途 | 路径 |
|---|---|
| CMake build tree | `build/cmake_build/<static|shared>/` |
| install tree | `build/cmake_install/<static|shared>/` |
| compiler output | `build/<BUILD_TYPE>/<static|shared>/{bin,lib}/` |
| CLI | `build/cmake_install/static/Lumice` |
| GUI | 经过 `-g` 构建后的 `build/cmake_install/static/LumiceGUI` |

完整当前选项以 `./scripts/build.sh -h` 为准，不要把本文 flag 再复制成另一份规格。

## 3. 源码组织与边界

引擎层次只在 `cmake/lumice_layers.cmake` 定义一次：

```text
foundation → analytic → scene → view → sim → render → engine → raypath → capi
```

`src/` 下每个引擎文件都归该 manifest 所有，只能 include 本层或更低层。`scripts/check_policies.py` 拒绝无归属文件、失效 manifest 项和新增向上依赖。

主要目录：

| 目录 | 职责 |
|---|---|
| `src/config/` | JSON 解析与 scene/config 数据 |
| `src/core/` | 几何、光学、simulator、buffer、投影与 backend |
| `src/server/` | session 编排、consumer、结果与 C bridge |
| `src/raypath/` | 光路报告与单光路分析 |
| `src/analytic/` | 对外 analytic 能力 |
| `src/include/` | 公开 C 能力头 |
| `src/gui/` | 交互式应用壳 |
| `src/util/` | 基础设施与纯共享 helper |
| `test/` | C++ / Python 验证层 |

### 公开 API 边界

项目没有 umbrella header。按使用的能力分别 include：

```text
lumice_base.h  lumice_scene.h  lumice_render.h
lumice_editor.h  lumice_engine.h  lumice_raypath.h
```

GUI 可以 include 这些引擎能力头，但不得 include `core/`、`config/` 或 analytic family 的公开头。只有纯、无状态且不携带仿真/配置语义的共用 helper 才放进 `src/util/`。

`src/raypath/detail/` 只供 raypath 模块及其测试使用；engine-facing 光路头以已登记模块 surface 为准。

## 4. 风格与诊断

遵守 `.clang-format` 与 `.clang-tidy`。

命名：

- 类型：`CamelCase`；
- 函数：`CamelCase`；
- 变量：`lower_case`；
- 私有成员：`lower_case_`；
- 常量：`kCamelCase`；
- namespace：`lumice`。

优先 `const`、`constexpr`、智能指针、`nullptr` 与 `override`。禁止裸 `new`/`delete` 和 `using namespace`。

提交前运行：

```bash
./scripts/format.sh
./scripts/format.sh --check
```

### 日志

引擎与 server 诊断使用 `src/util/logger.hpp`：

```cpp
ILOG_INFO(logger_, "Committed epoch {}", epoch);
LOG_WARNING("Configuration uses a deprecated field");
```

GUI 诊断使用 `src/gui/gui_logger.hpp` 的 `GUI_LOG_*`。不要在 `src/` 新增 `printf`、`fprintf`、`std::cout` 或 `std::cerr`；它们绕过统一 sink，在 Windows GUI 中还可能完全不可见。具名例外只有 `src/main.cpp` 的 CLI 产品输出、`src/util/fatal.hpp` 的 abort 前 owner，以及引擎加载前的 Windows launcher。

CLI 产品输出走 stdout，诊断日志走 stderr。

## 5. 添加或修改代码

修改子系统前先从 `doc/README_zh.md` 找到对应设计文档。尤其是：

- 几何或谓词：`geometry/crystal-geometry-representation.md`、`geometry/numerical-robustness.md`；
- GPU 路：`backends/seam-design.md` 与 backend lifecycle 文档；
- 曝光/显示：`rendering/ev-pipeline-architecture.md`；
- GUI 状态：`gui/gui-state-governance.md`；
- 光路分析：`raypath/raypath-analysis.md`、`raypath/raypath-analysis-panel.md`；
- 测试：`testing/testing-architecture_zh.md`。

新增引擎源码文件：

1. 选择语义所属的最低层；
2. 在同一变更中加入 `cmake/lumice_layers.cmake`；
3. 保持 include 方向合法；
4. 通过既有 capability 暴露；若确需新 surface，先明确 API 设计；
5. 添加 oracle 与行为匹配的最小测试。

修改公开 C API：

1. 把声明放入拥有该操作的 capability header；
2. 使用该头对应的 export macro；
3. 在匹配的 `src/server/c_api_<capability>.cpp` bridge 实现；
4. 按兼容性规则更新 API version 与记录；
5. 验证生成的 export surface 和外部消费者契约。

不要在 GUI、CLI、engine 各抄一份同一语义；把权威实现放到合法的最低共享层，各壳只消费它。

### 环境变量

新增 `std::getenv` 前先应用 `env-var-policy.md`。用户可见行为应进入 CLI/config/API，不应藏在机器本地 env 中。运行时 `LUMICE_*` 读取统一位于 `src/util/env_knobs.cpp`，且必须登记并在启动时记录。

## 6. 测试

先构建测试，再按决策需要选择 scope：

```bash
LUMICE_SKIP_GUI_TESTS=1 ./scripts/build.sh -gtj release
./scripts/test.sh quick   # 验证刚做的改动
./scripts/test.sh full    # 更宽本地 sweep，含 GUI real-timing pool
./scripts/test.sh pr      # PR 前形态，含 shared-lib slow E2E
```

Python 直接入口：

```bash
pytest -v                 # fast 子集，与 CI 一致
pytest -v -m ''           # 完整 E2E 集
pytest -v -m slow         # slow；先运行 ./scripts/build.sh -sj release
```

命令退出码才是 pass/fail 判据。没有 `pipefail` 时不要把测试命令 pipe 给 `tail`；构建失败后也不得用旧二进制的结果推断新代码通过。

### 新测试放在哪里

先按验证目的分类：

| 命题 | 位置 / target |
|---|---|
| 一个纯 unit | `test/unit-correctness/` |
| 两个以上 unit 协作、无需 live frame/input | `test/composition-correctness/` |
| backend 等价 | `test/parity-cross-backend/` |
| 闭式或 analytic oracle | `test/golden-analytic/` |
| CLI / full-stack 行为 | `test/e2e-correctness/` |
| 吞吐闸 | `test/performance/` |
| rendered frame 或合成 GUI input | `test/gui/` / `gui_test` |
| 某个已逃逸缺陷 | `test/regression-sentinel/` |

对 `src/gui/`：

- 一个 unit 的纯逻辑：`gui_unit_test`（header-only 则可进 `unit_correctness_test`）；
- 多个 GUI unit 协作、无需 frame：`composition_correctness_test`；
- 需要 rendered frame 或 `ImGuiTestContext`：`gui_test`。

不要把一次失败输出直接覆盖 reference image。按 `testing/testing-architecture_zh.md` 中登记的 regeneration workflow 与 full-suite 采样规则重拍。

## 7. Policy 检查

修改结构或边界时运行 whole-tree policy：

```bash
python3 scripts/check_policies.py
```

pre-commit 会运行格式检查与 diff-scoped gates，也可直接调用：

```bash
python3 scripts/check_new_refs.py --staged
python3 scripts/check_new_gui_tests.py --staged
python3 scripts/check_loop_fatal_asserts.py --staged
```

用 `./scripts/install-hooks.sh` 一次性安装仓库 hook。不要绕过失败 hook，应修正根因。

## 8. 调试与性能分析

用 `./scripts/build.sh -gj debug` 构建 debug flavor。可执行文件安装到 `build/cmake_install/static/`，compiler output 保留在 `build/Debug/static/`。

优先步骤：

- CLI 用 `-v` 或 `-d`，日志在 stderr；
- 间歇性问题先画完整数据流，对 silent return 加可观测性，再改行为；
- parity 红先逐阶段比两条路，定位第一个分叉点；
- GPU 改动按 reference machine 协议验证，不从没有该 backend 的 host 外推；
- 按平台和缺陷形态使用 ASan/UBSan、LLDB/GDB、Instruments 或 `perf`。

性能结论必须来自重新构建的二进制、正确性 sanity gate、空闲参照机和 `testing/performance-testing_zh.md` 指定的 baseline。

## 9. 文档与贡献

本地生成 Doxygen：

```bash
doxygen .doxygen-config
```

输出在 `doc/api/html/`；生成文档不替代稳定 capability 文档。

开 PR 前：

1. 运行针对性检查；改动触及 simulator、C API 或 shared-library 路时运行 `./scripts/test.sh pr`；
2. 检查完整 diff 与 staged set；
3. 永久 prose 不引用 ignored working notes；
4. 按 `CONTRIBUTING.md` 执行分支、PR 与发布流程。

## 相关文档

- [系统架构](architecture_zh.md)
- [配置参考](configuration_zh.md)
- [测试架构](testing/testing-architecture_zh.md)
- [C API 参考](interfaces/c_api_zh.md)
- [环境变量策略](env-var-policy.md)
- [贡献指南](../CONTRIBUTING.md)
