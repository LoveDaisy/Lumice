# 性能测试指南

本指南涵盖 Lumice 三个层次的性能测试，从纯管线基准测试到真实 GUI 交互测试。

所有命令假设工作目录为项目根目录。

> **范围**：本指南保留稳定的操作手册 + **当前 canonical** 吞吐参考。历史 per-run 实测详录
> （带日期的表、原始 reps、各 effort 的方法论）放在 git-ignored 的 `scratchpad/perf-results-log.md`
> ——新 per-run 数字追加那里，只有成为新 canonical 锚时才提升进本文档。远程机器上的 CUDA build +
> parity/正确性验证是另一件事——见 [`gpu-remote-cuda-build-testing.md`](gpu-remote-cuda-build-testing.md)
> （⛔ 该 recipe 的两台机器已退役、命令不可执行——先读那份文件顶部的横幅）。
> 下面哪些吞吐数字仍可复现，按 host 块标在「canonical 吞吐结果」一节里；
> **把任何数字当判据之前先读那个状态。**

## ⚠️ 合成 CPU 负载发生器：清理不能只靠 `trap ... EXIT`

鲁棒性/性能测试有时会起一个后台忙等进程（如 `yes > /dev/null &`）来合成 CPU 争用。仅用
`trap cleanup EXIT` 去 kill 那个 PID 并不可靠——同一次调查中复现两次；此前一个正是这种模式的
泄漏实例曾无人察觉地跑了 20 多小时，吃掉一台 12 核机器的绝大部分算力。

- 当 shell 是被 `SIGKILL`（或任何它捕获不了的信号）强制终止、而非正常退出时，`EXIT` trap
  不会执行——例如发起进程/会话本身被强行 kill 掉的场景。此时忙等子进程会被重新挂到 init 下，
  继续无限期运行。
- 一旦负载发生器进程以这种方式泄漏，从后台任务或嵌套 subshell 里执行 `kill` 并不可靠——不总是
  生效。只有对确切 PID 做**前台** `kill` 才能可靠终止它。

**正确形态（三者缺一不可）：**
1. **把生成的 PID 写入文件**，不只是存 shell 变量——文件能在发起它的 shell 之外存活，
   便于另一个 shell 在异常退出后找到并清理。
2. **前台 kill**，绝不从后台任务或嵌套 shell 循环里 kill。
3. **kill 之后用 `pgrep` 核验**（如 `pgrep -x yes`）——不核验自身结果的清理步骤等于没清理。

在共享机器上，只清理自己 PID 文件所列的进程——绝不按进程名广播 kill。

## 日志级别

CLI 基准测试和 GUI 性能测试均支持日志级别选项。
**建议在两个级别下各运行一次**以获取完整数据：

| 级别 | 用途 | 开销 | 额外输出 |
|------|------|------|----------|
| **info**（默认） | 精确吞吐量数据 | 可忽略 | PERF 摘要 + Consume 剖析 |
| **verbose** | GUI/Poller 循环细节 | 低（~2-5%） | 每周期 staging、upload、质量门控决策 |
| **debug** | Consume 每批次分解 | macOS ~8%，**Windows ~54-62%** | ConsumeData 每批次锁/消费计时 |

**注意**：Windows debug 日志开销显著（吞吐量下降 2-3 倍）。
- 使用 **info** 级别数据进行吞吐量比较
- 使用 **debug** 级别数据分析 Consume 内部（比率可靠，绝对值偏高）

## 关键指标

两个目标存在张力——通过以下指标量化：

| 目标 | 指标 | 来源 | 描述 |
|------|------|------|------|
| **响应性** | `first_upload`（ms） | 日志分析（`analyze_perf_log.py`） | Commit → 首次纹理上传成功；越低越好 |
| **渲染质量** | `upload_rays` 值 + CV | 日志分析（`analyze_perf_log.py`） | 每次上传的光线数；绝对值越高、CV 越低越好 |

**注意**：低 CV 不代表高质量——1K rays 配低 CV 仍然很差。绝对值和稳定性都重要。

辅助指标：

| 指标 | 来源 | 描述 |
|------|------|------|
| `rays/sec` | CLI 基准 / GUI 性能测试 | 管线吞吐量 |
| `upload_ratio` | GUI 性能测试 | uploads/restarts，纹理交付率 |
| `texture FPS` | GUI 性能测试 steady_state | 稳态纹理刷新率 |
| `Consume profile` | CLI -v / GUI --log-level debug | 每批次 filter/proj/accum 分解 |

> ⚠️ 英文版这里还有一整节**尚未翻译**的内容（`## ⚠️ Two ways \`--benchmark\` silently reports
> a number that answers a different question`，含 §A 单线程/多线程口径混淆、§B GPU 短窗测的是
> 时钟升频瞬态、§C GPU 冷启动初始化被折进分母，以及五条防御性规则——含本任务新测得的
> "GPU 上 `ray_num` 必须超过 drain 量子" 这条机制）。这不是本次改动引入的缺口——该节在中文版
> 里从一开始就不存在（不是"被删掉的规则 4/5"，是整节从未译过）；需要时请直接读英文版
> `performance-testing.md` 对应小节。补齐翻译不在本任务范围内。

## ⚠️ 本地构建不是出货的那个二进制：`-march=native` 在这里默认开、在别处一律关

这一条讲的不是 `Lumice benchmark` 报的是*哪个*数字，而是这个数字出自*哪个二进制*。它已经害过一次
跨平台结论——那条结论被写下来、被当作待办流传了一段时间，才查出成因。

**机制。** `LUMICE_ISA_LEVEL`（`CMakeLists.txt:50`）是唯一一个说明「Release 构建编译到哪一档
ISA」的变量，取四个值：

| `LUMICE_ISA_LEVEL` | GCC/Clang/clang-cl Release 下的 flag | 谁在用 |
|---|---|---|
| `native` | `-march=native` | **本地默认**——`scripts/build.sh` 什么都不传，所以每次本地构建吃的都是它；开发机就该为自己编译 |
| `baseline` | 无（x86-64-v1，任何 x86_64 CPU 都能跑的地板） | `.github/workflows/ci.yml` 里每一处 configure，以及除下面两个第二变体之外的每一个 release 包 |
| `x86-64-v3` | `-march=x86-64-v3`（AVX2+FMA） | Windows x64 release 的第二个**引擎**构建，由 **clang-cl** 编译：`release.yml` 把引擎库构建两遍（baseline 用 MSVC cl.exe、v3 用 clang-cl），打成两份内部 DLL；外壳可执行文件是另一趟单独的、只编一次的、永远基线的构建 |
| `x86-64-v4` | `-march=x86-64-v4`（AVX-512） | Linux x64 release 的第二个**引擎**构建：`release.yml` 把引擎库构建两遍，打成两份内部 `.so`；外壳可执行文件是另一趟单独的、只编一次的、永远基线的构建 |

flag 经同一个共享 CMake 函数 `lumice_apply_isa_march()` 挂在 `lumice_obj` 上，所以 CLI 和 GUI
**链接同一份引擎构建、因而同一档**——这管的只是引擎库；在两个 release 平台上，外壳可执行文件本身
都只编一次、永远基线（Windows 上这一点是结构性的，因为真正的 MSVC cl.exe 完全不读这个 flag；运行时
究竟加载哪一档引擎，见下文）。真正的 MSVC cl.exe 完全不读这个变量——该函数只在 GCC/Clang 分支与 clang-cl 分支被
调用——所以用 cl.exe 做的 Windows 构建（本地 `win_build.cmd` 默认、CI 的每一个 Windows job）
**结构上**没有等价物可开，永远是基线。**clang-cl 是例外**，也是 Windows release 有第二个变体的
全部原因：CMake 对它报告 `MSVC=TRUE`（它驱动 MSVC ABI 与 `/` 风格 flag），但它是一个认 `-march`
的 Clang 前端；`CMakeLists.txt` 把这一点一次性判成 `LUMICE_CLANG_CL`，走同一个函数。Windows
参照机实测（Zen 5、clang-cl 20.1.0、CUDA-off 臂）：clang-cl 在 `x86-64-v3` 上是真 MSVC release
构建的 **2.26×（W=1）/ 2.28×（W=4）**，`x86-64-v4` 与 `-mprefer-vector-width=512` 再加不上去
（2.25×/2.29×），不带 `-march` 的 clang-cl 是 1.01×——收益来自 flag 而非换编译器，且 LLVM 在 AVX2
就全部拿到，而 GCC（见下）在 AVX-512 之前一分不拿。这就是两个平台的第二变体档位不同的原因。

**Windows release 拿它做什么。** `windows-x64` 的 zip 每个入口只装一份外壳可执行文件
（`Lumice.exe`、`LumiceGUI.exe`；只编一次，用 MSVC cl.exe，结构上永远基线），外加两份内部引擎
DLL：`lumice-engine.baseline.dll`（cl.exe）与 `lumice-engine.x86-64-v3.dll`（clang-cl）。外壳
启动时读 CPUID 判整个 x86-64-v3 特性级别外加 XGETBV（OS 必须保存 YMM 状态——OSXSAVE 先确认再执行
XGETBV，否则那条指令是 `#UD`；这段探测逻辑逐字沿用自下文提到的、已退役的进程级 launcher 形态），
按自己所在目录的绝对路径 delay-load 对应那份 DLL，绝不经 PATH 或当前目录——同名 DLL 放在这两处
都不会被选中。`--isa=baseline` / `--isa=x86-64-v3` 可在任何 `LUMICE_*` 调用发生前强制其一。这是
单进程模型：不再有一个独立的 launcher 进程 `CreateProcess` 起 sidecar 再转发退出码，那是下文对比
的已退役形态。引擎 DLL 是内部实现细节——它的 ABI 不是稳定契约，不是独立的受支持接口，
`lumice.h` 不随它出货；因为外壳自身的编译期档位永远是基线、说明不了运行时到底加载了哪份引擎，
本节 benchmark 报的 `"isa"` 字段（见下方规则 3）改由 `LUMICE_GetEngineIsaLevel()` C API 在运行时
询问*那份被加载的引擎*来回答，而不是读外壳自己编译期的宏。
在与 release 等价的 CUDA-on 构建上，v3 变体在 ms1 W=1 下量到基线的 **2.23×**（1.701 vs 0.761 M
rays/s，CoV 0.72% / 0.33%，5 次交错重复、经已退役的进程级 launcher 形态的 `--isa=` 覆盖；
Windows 参照机，Zen 5，2026-09-11）——与上面那个 CUDA-off 探针数字相差 1.3%，但那是另一条臂，
不得拿来代替它。**改成外壳+引擎 DLL 形态后重测**（净机窗口，Windows 参照机，2026-09-17）：DLL
边界本身不掉速（v3 引擎做成 DLL vs 同代码静态链进一个 exe：single 100.4%、multi 97.6%，即噪声
量级）；相对纯静态基线构建，出货的外壳 + v3 DLL 是 **single 2.311×**（与上面的旧锚一致）**/
multi 1.562×**（不一致——上面那条旧锚从未在 1–4 worker 之外测过，从没测过这台机器 `benchmark`
自动选择的满 16 核 multi-worker 档位，所以这个差距是新测出来的，不是 DLL 拆分引入的回归）。
**这个 multi 数字随后已被更新，而且它依赖 worker 数**（Windows 参照机，2026-09-18，`render` 显式
`--workers`，三个 worker 侧投影场景——2048×1024 单次散射 / 512×256 / 彩色 fisheye 多晶体——5 次
交错重复，CoV ≤2.35%；场景集与方法都不同于上面那条单一 canonical 场景的 `benchmark` 趟，是口径
换了，不是同一实验重跑）：在 16 worker（即 `benchmark` `multi` 趟的档位）下，v3 引擎是基线引擎的
**1.774× / 1.780× / 1.908×**；在 32 worker——Windows 现在出厂的自动 worker 数——下是
**1.141× / 1.296× / 1.510×**。比值随 W 收窄，是因为快的那条臂先触顶：基线引擎靠 SMT 一路涨到 32
线程（相对自己 10 worker 的数字 1.75×–2.12×），v3 引擎只涨 1.11×–1.56×。引用时必须带上 W；
对这台机器，一个不带 W 的「multi 比值」是没定义清楚的数字。
同一次复测还发现了另一项与 DLL 本身有关的代价：baseline 引擎 DLL 比旧的纯静态 baseline exe
**慢 10–14%**（single 86.1%、multi 93.4%）——`WINDOWS_EXPORT_ALL_SYMBOLS` 生成的 `.def` 导出表
与 cl.exe 的 `/GL` 全程序优化不兼容，所以 cl.exe 编的 baseline DLL 丢掉了旧的纯静态 baseline exe
曾有的链接期优化；clang-cl 编的 v3 DLL 不受影响，仍保有它的 thin-LTO。

**Linux release 拿它做什么。** `linux-x64` 的 tarball 每个入口只装一份外壳（`Lumice`、
`LumiceGUI`；只编一次，基线），外加 `lib/` 下两份内部引擎共享库：`lib/liblumice.so`（baseline）
与 `lib/glibc-hwcaps/x86-64-v4/liblumice.so`（v4）——SONAME 相同、文件名相同，只是目录不同。
**没有任何应用代码在两者间做选择**：这是 glibc 自带的动态链接器 hwcaps 机制（glibc ≥ 2.33），
CPU 与 glibc 都够格时选中 `glibc-hwcaps/x86-64-v4/` 那份，否则静默落回 `lib/liblumice.so`。
这取代了已退役的、按入口各装一份的进程级 launcher 及其 `execv` 到 sidecar 的模型，也取代了
`--isa=` 覆盖——**Linux 没有 `--isa=` 的对应物**；面向 CPU 的测试覆盖手段是 glibc 自己的
`GLIBC_TUNABLES=glibc.cpu.hwcaps=-<feature>`（一个 CPU *特性*名，例如 `-AVX512F`——这个 tunable
不解析 `-x86-64-v4` 这类档位字符串）。「老发行版落回基线」这个说法有一个前提要说清楚：这个回落
要求二进制本身先能*加载*，而本仓库自己的构建地板（`ubuntu-24.04`，符号版本 `GLIBC_2.38`）本来就
已经超过 hwcaps 机制自己要求的 glibc 2.33，所以在本仓库自己出的 release 二进制上，「旧 glibc」
这条回落分支目前实际不可达——足够老的发行版会在符号版本检查阶段就拒绝加载这个二进制，根本走不到
hwcaps 探测（已用 `debian:buster`、`bookworm` 容器镜像核实）。今天真正可达的回落是「CPU 驱动」
而非「glibc 驱动」。
本仓库实测（Zen 5 / GCC 13.3，进程级 launcher 形态、每档一份完整重复 exe），v4 变体在 1–4
worker 下是基线的 1.9–2.3×，且**收益全部来自 AVX-512**——`x86-64-v2` 与 `-v3` 都量到 1.00×——
所以恰好是两个变体而不是一个阶梯。**改成外壳+共享库形态后重测**（净机窗口，Linux 参照机——
WSL2，glibc 2.39，2026-09-17）：single-worker 静态基线 0.685 M rays/s vs 外壳 + v4 共享库
1.393 M rays/s ⇒ **2.032×**（三臂交错 ×5，CoV ≤0.4%）；共享库边界本身不掉速（外壳+v4 vs 同一份
引擎静态链接：0.996×，即噪声量级）。这个场景的 multi-worker 只量到 ≈1.13×，不计入这条比较——
满核跑在这台硬件上无论哪一档都远早于单 worker 的 ISA 增益就先撞到吞吐天花板，这是硬件上限，
不是库边界代价。

**后果。** 从本地非 MSVC 构建取的吞吐数字，相对出货的基线二进制系统性偏乐观（就是上面那个
1.9–2.3×）。更糟的是：拿两个默认设置的本地构建做 Windows vs 非 Windows 的
A/B，比的是 native-ISA 二进制对基线-ISA 二进制，而**任何地方都不会给你任何警告**——两边都构建
成功、都打印出看起来合理的数字，而两者之比里有一部分只是编译开关的产物，与你想量的东西无关。

**规则。**

1. **任何要离开你这台机器的数字**——跨平台对照、写进 issue / 文档 / 评审里的数字——
   **必须用 `-DLUMICE_ISA_LEVEL=baseline` 取**。那才是 CI 用的配置，也是唯一跨平台可比的那一档。
   在 `x86-64-v4` 上取的数字是关于 Linux v4 变体的合法数字，但必须写明；`x86-64-v3` 与 Windows
   clang-cl 变体同理，且还要写明是 clang-cl（cl.exe 构建产不出这一档，所以键值本身已经隐含了
   编译器）。
2. **日常本机迭代可以继续吃 `native` 默认值。** 这不是在禁止 `-march=native`，而是在划清「这个数字
   能走多远」这条线。自己改动的前后 A/B，只要两臂在同一台机器、同一档设置上取，不受影响。
3. **读 `isa` 键，别指望记得住。** 每一行 `[BENCHMARK]` 都带 `"isa": "native"`、
   `"isa": "baseline"`、`"isa": "x86-64-v3"` 或 `"isa": "x86-64-v4"`（`src/main.cpp` 的
   `RunBenchmarkPass`，取自 `LUMICE_GetEngineIsaLevel()` C API——由*引擎*库在运行时用自己的
   `LUMICE_ISA_LEVEL_STR` 回答；`lumice_apply_isa_march()` 用与 `-march` flag 本身相同的条件解析
   该宏，所以只要回答的那份引擎构建没有真正应用 flag——包括 Debug 构建，以及每一个 cl.exe 编的
   引擎——这个键就读 `baseline`）。这是刻意设计成由引擎回答，而不是外壳读自己编译期的宏：在上面
   Windows / Linux 两种 release 形态下，外壳可执行文件永远编在基线档，若读外壳自己的宏，即便实际
   加载的是更高档的引擎库，也只会报 `baseline`；直接问那份被加载的引擎，才能让这个键跨过这道拆分
   仍然正确。于是一份旧 log 自己就能回答「这是哪个构建取的」，
   不需要还留着当时的 configure 输出。`baseline` 是可比的那一档；两行数字只有在 `isa` 相同时
   才允许互相比较。
4. **留意 configure 那一行。** 每次 `cmake` configure 都会打印一行 `-- LUMICE_ISA_LEVEL=...`
   状态，说明这棵构建树在哪一档。


## 测量纪律：一条性能结论需要哪些机器

**CPU 路线：结论需要两个 OS，不是一个。** 任何 CPU 路线的吞吐结论、最优 worker 数、或任何
A/B 结果，都必须同时有 **Windows 与 WSL2 两个参照角色的一手数据**（写角色名，不写主机名——见
`machines.md`），并且要么同时给出 Mac 数字，要么说明为什么没有。这不是为谨慎而谨慎：同一份代码
已经在这两个 OS 上测出过两次相反的结论。这不是偶发的巧合——把 legacy CPU 路线的逐 ray 投影从单一
consumer 线程搬到 simulator worker 上（`accumulator-consumer-architecture.md` §1.1）改变了这条
路线并行瓶颈所在的位置，而两个 OS 对这次转移的反应并不一致：本文上面的 ISA 一节里，Windows
参照机的 multi-worker 增益量到 **1.77×–1.91×**（随场景而异，16 worker；在它 32 worker 的自动
默认档位下是 1.14×–1.51×），而 Linux/WSL2 在同一类对照下只有 **~1.13×**，且
Linux 一侧被明确标注为「远早于单 worker 的 ISA 增益就先撞到吞吐天花板」——是同一种分叉形状，不是
局限于某一次测量的巧合。只在一个 OS 上取的数字、外推到另一个 OS，是把猜测打扮成结果。

**原生 Linux 没有任何一手数据——要说清楚，不要含糊过去。** 本仓库里所有「Linux」数字都来自
WSL2，而 WSL2 有它自己已经量到的、不同于原生 Linux 的行为：`futex`/`sys` 时间随 worker 数上升；
`dxgkrnl` 半虚拟化把 CUDA context 建立放大约 2.5×。任何按 OS 分档的常数，只要「Linux」那一格
实际测自 WSL2，就必须在该格或其说明里写明，不能只散落在别处的正文里。

**GPU 路线：`nvidia-smi` utilization 只是观测项，从不是吞吐判据。** 它量的是「设备上有某个
kernel 在跑」占墙钟时间的比例——不是 SM 占用率，也不是离 kernel-bound 上限还有多远。本仓库已经
在把它当判据这件事上立错过两次 scrum：清零掉曾占去 96% host API 时间的 host 侧 churn 之后，
utilization 数字纹丝不动——因为它本就看不见占用率或 host 侧停顿，只能看见「有没有东西被调度」。
GPU 吞吐唯一的判据是 `[BENCHMARK]` 行的 `rays_per_sec`，以及（kernel-bound 上限尺子落地之后）
生产吞吐相对那个上限的比值。

**参照机互斥。** Windows 与 WSL2 两个参照角色是同一台物理机；一侧在跑 bench 时另一侧仍在活动
会同时污染两边的数字。`machines.md` 已有这条，本段只指回去，不复述机制。

**观测通道不得挂在被测线程的临界路径上。** 一行 DEBUG 日志或一个 Python 日志回调曾经把
`DoSnapshot` 的墙钟时间直接拖长一倍——观测这个动作本身改变了被观测的数字。新增任何插桩都要绕开
临界路径（采样、独立线程、事后计数），而不是挂在它上面。


## GPU 占用率天花板：寄存器压力是真实代价，不是编译器冗余

`trace_single_ms_kernel`（`src/core/backend/cuda_trace_backend.cu`）在生产多 arch fatbin 上编译
出 **181 寄存器/线程**。按它 256 线程的 block（`kTraceBlockSize`）算，256 × 181 = 46,336，占 SM
65,536 个 32 位寄存器的大半——放得下一个 block，放不下两个——所以 `ncu` 报 `Block Limit
Registers = 1`，Windows CUDA 参照角色（Blackwell sm_120）上实测 Achieved Occupancy
**16.14%–16.28%**。封顶的是寄存器，不是 shared memory。那个显而易见的问题——这 181 个里有没有
编译器可以被说服让出来的冗余？把寄存器上限砍半让占用率翻倍，吞吐会不会跟着涨？——已经量过，
两问的答案都是否。数字记在这里，是为了这个方向不被人从头再试一遍。

唯一低风险的杠杆是在 kernel 签名上加 `__launch_bounds__(256, 2)`：不改任何逻辑，只告诉 `ptxas`
每个 SM 要塞下两个 block，于是寄存器被封在 65,536 / (256 × 2) = 128。它在编译期完全兑现了承诺，
在运行期输了：

| 指标 | baseline | `__launch_bounds__(256, 2)` | 变化 |
|---|---|---|---|
| 寄存器/线程（sm_120，对构建产物 `cuobjdump -res-usage` 读数） | 181 | 128 | −29.3% |
| Block Limit Registers | 1 | 2 | +1 |
| 理论占用率 | 16.67% | 33.33% | 翻倍 |
| Achieved Occupancy（`ncu`，Windows 参照角色） | 16.14%–16.28% | 31.54% | 如预测翻倍 |
| Spill stores / loads（`-Xptxas -v`，sm_120） | 0 B / 0 B | 12 B / 16 B | 极小 |
| **kernel 自身 Duration**（`ncu --set basic`，5e6-ray 配置，N=4 对 N=3，两臂 stdev < 1 µs） | **163.85 µs** | **179.67 µs** | **+9.65%——变慢** |
| Compute (SM) Throughput | 10.63% | 9.92% | 下降 |
| Memory Throughput | 27.94% | 25.81% | 下降 |
| 端到端 `rays_per_sec`（1e9-ray 生产配置，WSL2 参照角色，每臂 N=6，CoV 7.1%） | 423.07 M | 427.11 M | +0.95%，噪声内 |

两条机制结论可以带出这个 kernel 之外：

1. **0 spill 的基线说明寄存器数就是 kernel 的真实工作集，不是冗余。** 不加约束时 `ptxas` 给每个
   活跃值都选了寄存器而非 local memory，一个字节都没溢出；根本不存在可供 `__launch_bounds__`
   「释放」的浪费分配。它能做的只有强行封顶、人为制造 spill——而哪怕小到 12 B / 16 B（三四个
   32 位值）的 spill，在这里也吃掉了 9.65% 的 kernel 时间。寄存器数由 kernel 的逻辑复杂度决定；
   要降它得改逻辑，不是改编译器的主意。
2. **占用率翻倍不等于吞吐提升，除非 kernel 真的受占用率约束——而这由吞吐类计数器裁定，不由
   占用率数字本身裁定。** 16% 占用率下这个 kernel 的 Compute (SM) Throughput 只有 10.63%、Memory
   Throughput 只有 27.94%——都远未打满——说明它并不缺驻留 warp 来隐藏延迟；它的上限在单线程内的
   依赖链和分支/访存模式里。所以驻留翻倍没有换来任何可以抵消 spill 代价的东西，两项吞吐计数器
   反而*双双下降*。从 Achieved Occupancy 数字下任何结论之前先看 Compute/Memory Throughput：低占用率
   配低吞吐计数器是延迟链型 kernel，不是缺驻留的 kernel。

对本仓库测量方式的两条推论。只看端到端 A/B（+0.95%，CoV 7.1%）会被读成「被 host 侧开销掩盖了」——
隔离的 `ncu` Duration 给出的是更强的结论：kernel 自身变慢了；两种「看不出收益」的表面现象机制
完全不同，只有隔离测量能把它们分开。而寄存器数必须从构建产物上读（`cuobjdump -res-usage`），不能
从一次绿色重建推断：这次测量里一份被改写过的源文件带着比既有 `.obj` 还旧的修改时间，`ninja` 以
exit 0 跳过了重编，第一轮 profiling 把 baseline 二进制的 181 寄存器当成了 bounded 版本的读数——
「改了输入、输出没变」是唯一的信号，是产物层读数抓住了它。

裁决：**不采纳。** 寄存器压力留在 181 / 16% 占用率，是决定，不是遗漏。把 kernel 拆成更小的多个
pass 没有做原型：既然只加 bounds 的版本已是净亏，拆分在单引擎大 dispatch 设计下只会在同样的 spill
经济账上再叠加真实的跨 kernel 同步开销，只可能更差。这个 kernel 设备利用率低的*另一个*已测成因——
多次散射层之间的 host 侧空闲间隙，以及为什么它的异步读回被否决——是另一种机制、另一份记录：
`gpu-route-history.md` Phase 14（§九）。


## 1. CLI 管线基准测试

不含 GUI、VSync 或显示开销的纯管线吞吐量测试。

### 配置

使用 `examples/bench_config.json`：1 晶体，1 渲染器，D65 光谱，10M 光线，max_hits=8。

### `benchmark` 子命令

`Lumice benchmark -f <config>` 运行双模式基准测试（原 `--benchmark` 旗已删除，遇到会报错并给出迁移提示；
子命令只接受 `-f`、`--backend`、`-v`、`-d`、`-h`——不写文件所以没有 `-o`，worker 数是测量口径本身所以没有 `--workers`）：先用单 worker 测量单核效率，再用全 worker 测量
并行吞吐量。输出两行 JSON：

```
[BENCHMARK] {"mode": "single", "workers": 1, "cores": 8, "rays": 2000000, "wall_sec": 8.51, "setup_sec": 0.01, "active_sec": 8.5, "rays_per_sec": 235294.1, "rate_basis": "steady", "isa": "native"}
[BENCHMARK] {"mode": "multi", "workers": 8, "cores": 8, "rays": 10000000, "wall_sec": 0.6, "setup_sec": 0.02, "active_sec": 0.58, "rays_per_sec": 17241379.3, "rate_basis": "steady", "isa": "native"}
```

**`rays` 是被追迹的光线数**，所有路线的吞吐分子都是它。每条路线都追迹发给它的每一条光线——
投影面积入射因子是权重（`lm_pcg::entry_weight`，`src/core/shared/pcg_shared.h`），不是丢弃——
所以它同时也是发放数，即 `ray_num` 与所有面向用户的计数（`LUMICE_GetSimRayCount`、GUI 的
"Total rays"、`Stats: sim_rays`）用的那个数。一条在追迹前丢弃光线的路线会打破这个恒等式，
它的 `rays_per_sec` 会相对其他路线虚高（被丢弃的光线几乎零成本）：这样的路线在这里必须只计
它实际追迹的光线。

`rays_per_sec` 是 `active_sec`（从首条光线追踪到 IDLE 的窗口）上的**稳态追踪率**，
**不是** `rays / wall_sec`——但这只在 `rate_basis` 为 `steady` 时成立，而这正是
`rate_basis` 这个字段存在的意义。两个退化档（`wall_fallback` / `active_short`）都退回
`rays / wall_sec`，此时该字段是含 setup 的**下界**，分母也不是 `active_sec`。
`setup_sec`（server alloc + 场景生成 + 首 dispatch 延迟）只在 `steady` 档从分母剔除；
`rate_basis` 记录产出该率的路径。这条 setup-剔除规则
对整个 run 只 ~0.2s 的快后端很重要——折进 setup 会把它们的 rays_per_sec 压低 >30%。

**两套独立的 `rate_basis` 阶梯**（消费者/gate 按走的哪条路径判定，不按全集字符串相等判）：
- **finite `ray_num`**（legacy CPU 趟，及任何 finite-config `benchmark` 运行）：`steady` /
  `active_short` / `wall_fallback`（上面的 honesty-fix 阶梯）。
- **`ray_num="infinite"`**（GPU 趟，task-gpu-bench-drain-aligned-rate）：`drain_aligned`（恰好
  测了 N 个整 drain 窗口）或 `too_few_drains`（未凑满 N drain 就退出——异常/不可信）。见下方
  drain-count-driven canonical。两套阶梯不共用同一字符串空间。

**术语**："workers"指 simulator 线程（执行光线追踪的线程）。每个 server 实例还有 2 个
内部线程（场景生成 + 数据消费），总线程数 = workers + 2。

**并行扩展效率** = `multi_rps / (single_rps × workers)`。接近 1.0 表示良好扩展；
偏低说明存在锁竞争、内存带宽饱和或调度开销。**仅对 legacy CPU 路线有意义**——见下方 GPU caveat。

> **⚠️ GPU 后端是单引擎——不存在 "single" vs "multi" 并行。** GPU 路线（Metal / CUDA）无条件
> `worker_count=1`（`server.cpp:284`）；只有 legacy CPU 路线是真多 worker（默认
> Linux/macOS 上 `worker_count = min(PhysicalCoreCount(), 10)`、Windows 上
> `LogicalCoreCount()`——两者都是 `ServerImpl::AutomaticWorkerBaseAndCap()` 返回的一对值，即按平台
> 分档——依据是各平台生产实际加载的引擎下实测的最优 worker 数：Windows
> 参照机（clang-cl x86-64-v3 引擎）自动值卡在 10 会比现在出厂的 32 worker 慢 1.07×–1.58×，同一台
> 机器的 WSL2 侧在 glibc-hwcaps 自动选中的 x86-64-v4 引擎下最优点恰好就是 10（16 worker 吞吐减半）；
> ⚠️ 那格「Linux」数据来自 WSL2 代理，不是原生 Linux；`benchmark` 的 `multi` 趟显式请求满物理核，
> 因此不受该规则约束——它量的是满核并行效率，不再等于出厂默认会跑出来的吞吐，且两者的大小关系随
> 平台而异：多核 Linux/macOS 上 `multi` 比默认跑更多 worker，SMT 的 Windows 上反而更少）。既然 GPU 的 "single" 与 "multi" 趟都跑在同一个单引擎（只差暖机+光线数、
> 非并行），**`Lumice benchmark` 对 GPU 路线塌成 ONE 稳态趟**（label `mode="multi"`）、跳过暖机趟；
> legacy CPU 路线保留真双趟。路线检测是 env-aware 的（`LUMICE_WillUseGpuRoute` 认 `LUMICE_TRACE_BACKEND`，
> 故 env 选的 GPU run 也塌）。读 GPU 结果时：
> - GPU 的 `[BENCHMARK]` / `bench_throughput.py` 行**只有 `multi`**（`single`/`single_rps` 缺失
>   =预期、非 INCOMPLETE）。`multi` 数是稳态代表值。
> - **`efficiency` 对 GPU 不适用**（无 single/parallel 对；`explore-306.1` E1 `worker_count=1` 铁证）。
>   CI summary 的 efficiency 列仅对 legacy CPU 有意义。
> - 对旧数据：2026-07 前带 `single` 列的 GPU 表是旧双趟（single = 2M 光线暖机 run），那些 GPU `single`
>   数从来不是"单核"指标。

benchmark 模式的行为差异：
- **双趟运行**：依次创建两个独立 server 实例，指定不同 worker 数
  （单 worker pass 为 1，多 worker pass 为 `hardware_concurrency()`）
- **单 worker pass 光线数减少**：2M（而非配置原始值），以限制 CI 耗时
- **不写图片**：跳过 `SaveRenderResults`，`wall_sec` 纯反映模拟耗时
- **5ms 轮询间隔**（默认 1s）：把 IDLE 检测量化误差压到几毫秒（曾是 100ms，那单独就能给快 run 的
  wall time 加上一整个轮询间隔）

### Benchmark 场景注册表（canonical 吞吐场景）

> **吞吐比较的单一真源。** 永远引用本表里的真实 config——绝不臆造场景名。（本表存在是因为文档
> 曾引用不存在的 `ms3_multi_crystal_complex_filter` 并对标无法复现的数字；见
> task-fix-throughput-bench-honesty。）`scripts/bench_throughput.py` 跑 Metal-可比子集；两者保持同步。

测量口径：**引擎** = `Lumice benchmark` multi pass，setup-剔除稳态率；**GUI** =
`gui_test perf_test` steady_state，无限 budget，reconstruct 路径。基线分母永远是 **legacy CPU**
（GUI 真实路径）——绝不用 `cpu_backend`。下方 `Metal vs legacy` 比值是 M2 Max、2026-06-19 回归锚
（`scratchpad/task-fix-throughput-bench-honesty/data/`）；任何吞吐改动前后须同会话重测。

| config（真实文件） | regime | rays / MS / filter | lens / Metal 可比 | 角色 | Metal vs legacy（锚） |
|---|---|---|---|---|---|
| `bench_light_single_ms.json` | 轻·单MS · 512×256 | 10M / 1 / 无 | dual_fisheye_EA ✅ | 轻场景吞吐基准（bench 专用，勿因 e2e 改动）；**512×256 落 GPU L2，系统性高估 GPU 优势**；真实分辨率用 `--res-sweep`（见"分辨率是一等吞吐维度"） | 引擎 ~1.7× / GUI ~1.8× |
| `ms_multi_crystal.json` | 中·无filter | 2M / 2 / 无 | dual_fisheye_EA ✅ | 无 culling 中等基准；`--res-sweep` 第二代表场景（看场景依赖） | 引擎 ~2.0× / GUI ~2.2× |
| `ms_multi_crystal_complex_filter.json` | 重·标准 | 2M / 2 / complex | dual_fisheye_EA ✅ | **G1 + G4 主基准** | 引擎 ~8.1× / GUI ~9.5× |
| `ms_multi_crystal_filtered_bd.json` | 重·bd | 2M / 2 / bd | dual_fisheye_EA ✅ | G1 第二基准 | 引擎 ~10.1× |
| `ms3_mixed_pyramid_heavy.json` | 最重·棱锥 | 5M / 3 / 4×raypath | dual_fisheye_EA ✅ | register-pressure 上界 | 引擎 **~5.5×**（M2 Max，2026-06-24；GUI 互证 ~5.7×）。注：legacy single pass ~274s 超出 `bench_throughput.py` 的 `RUN_TIMEOUT_SEC`，故该场景仍从自动跑中排除——基线靠手动大 timeout 单测取得 |
| `halo_22.json` | 轻·单MS | 10M / 1 / 无 | **fisheye_EA（单）→ CLI Metal 回退** | **e2e 资产，勿改**；legacy-only 轻基准 | N/A（Metal 不兼容此投影；轻·Metal 用 `bench_light_single_ms`） |

注意：
- **上表 4 个 light/mid/heavy 主基准（bench_light / ms_multi / complex_filter / filtered_bd）都是
  512×256**（`ms3_mixed_pyramid_heavy` 例外，为 2048×1024）。512×256 的 XYZ 累加 buffer（W×H×3
  float = 1.5MB）落在典型 GPU 的 L2 内，**系统性高估 GPU 吞吐**——真实 GUI 默认渲染 2048×1024
  （16× 像素，24MB buffer，越 L2）。**跨分辨率数字不可比；报吞吐必须带分辨率**。真实分辨率吞吐用
  `bench_throughput.py --res-sweep`（分辨率轴，默认 dispatch，隔离单变量）——详见下方"分辨率是一等
  吞吐维度"。
- dispatch 甜点是**后端 + 分辨率依赖**：Metal 32768 / CUDA 262144 是 **512×256** 下的甜点；**分辨率
  升高时 CUDA 最优 dispatch 显著上移**（2048×1024 下 ~786K–2M，因 per-batch readback 摊薄）。小
  dispatch 饿死 GPU（512/2048 = 0.2–0.8× legacy）。
- `bench_throughput.py` 每 run 把 `ray_num` override 到大值（temp config，不动 committed 文件），
  使稳态窗口足够长而稳定。
- **⚠️ 第三时钟 drain 路径曾逼出 `multi_wall` workaround（scrum-312）——现对 GPU 由 drain-count-driven
  `drain_aligned` 取代**（task-gpu-bench-drain-aligned-rate，2026-07-02）。根因：`multi_med`
  （steady `rays_per_sec`）的窗口以 `sim_ray_num` 为界，而它只在第三时钟 drain 时前进（每 64×dispatch
  rays；CUDA 默认 262144 → 16.8M rays/drain）。旧 bench `ray_num=20M` 只 ~1.19 drain → `sim_ray_num`
  近 0 → 大部分 tracing 误算 setup → **5× 假低**（explore-315：wall 报 24.7M，真 plateau 130M）。旧修
  = `multi_wall = rays/wall_sec`；新修 = GPU 设 `ray_num="infinite"` 直接测整数个 *drain 窗口*
  （`drain_aligned`），既免 setup 又 drain-粒度精确。**GPU 行现重新用 `rays_per_sec`（`multi_med`）报诚实
  稳态率**；`multi_wall` 只对 finite（legacy）趟有意义，`drain_aligned` GPU 行忽略之。per-batch legacy
  不受影响（finite `steady` 阶梯）。

#### 分辨率是一等吞吐维度（device-fused XYZ 累加 → cost 随 buffer vs GPU L2）

> **报任何 GPU 吞吐数字必须带渲染分辨率；跨分辨率不可比。** 这不是二阶细节——它常主导轻场景的 GPU
> 吞吐（轻场景 trace 便宜，累加/回读占大头）。

机制：device-fused XYZ 累加（scrum-302）把每条出射光线在 **trace kernel 内** `atomicAdd` 进
W×H×3 float 图像 buffer（12 B/px）。cost 随 buffer 相对 GPU L2 变化：

- buffer ≤ L2（512×256 = 1.5MB，落多数 GPU 的 ~2MB L2）→ 累加走 cache，快。
- buffer ≫ L2（2048×1024 = 24MB）→ 每次 atomicAdd 打 DRAM，DRAM 带宽绑定，慢。膝在 L2 边界
  （~512→768），越 L2 后随分辨率平滑衰减（非二元断崖）。

CUDA 路线还有 **per-batch 同步 readback** 税（`ReadbackXyzAccum` 每 SimBatch 一次
`cudaDeviceSynchronize` + 阻塞 24MB PCIe D2H + memset），scrum-312 已把它解耦到显示节奏（"第三时钟"，
`seam-design.md` §4.8）——见下方 canonical 结果。Metal 无此税（`StorageModeShared` 统一内存 + 延迟等）。

**流程要求**：

1. GPU 吞吐用 `bench_throughput.py --res-sweep` 扫多档分辨率（默认 `256×128 … 2048×1024` 六档，2:1，
   跨 L2 膝两侧），而非只报单点。脚本每档在 temp config 里 override `render[].resolution`（committed
   文件不动，同 ray_num override 机制）。默认扫 `bench_light_single_ms`（轻，累加/回读主导）+
   `ms_multi_crystal`（稍重，看场景依赖），`--res-list` / `--res-configs` 可覆盖。至少覆盖 **512×256
   （引擎天花板 / L2-resident）+ 2048×1024（GUI 真实体验）** 两点。
2. 与竞品 / 硬件能力对标（下方 25M bar）时**对齐分辨率**——我方历史 512×256 数字是 L2-resident 上界，
   非用户体验。
3. 数字入表**必标分辨率**（现有表默认 512×256，除 `ms3_mixed_pyramid_heavy`）。sweep 的**曲线数据不入
   committed 文件**（按机器/会话变），入表的是按 N≥5 CoV 协议正式产出的 canonical 点。

#### 验收标尺：硬件能力，不是"× legacy CPU"

> **`× legacy CPU` 比值是地板，不是成功标尺。** 打过 legacy 只是必要门槛，绝非目标——GPU 后端可以
> "× legacy 赢"却只用 1–2% 利用率（scrum-304 就踩过这坑：一个报成"~1.7× legacy 16 核"的 CUDA 数其实
> 是重于可比口径的场景上饿着的 GPU）。GPU 后端真正的标尺是**这块卡的硬件能力**，锚到可比工作负载 +
> 外部参照。

**已注册硬件能力目标**（绝对、场景锚——判 GPU 吞吐时引用这个，而非"× legacy"）：

| 场景 | 可比负载 | 硬件能力目标 | 状态 | 来源 |
|---|---|---|---|---|
| `bench_light_single_ms`（轻·单MS） | 单晶 + 单 MS + 无续传 | **4060 Ti ≥ 25M rays/s** | ⚠️ **2026-08-11 起不可直接检验**——见下 | 竞品实测——轻·单MS 是 apples-to-apples 对比 |

> **⚠️ 25M bar 为何当前不可检验，以及三条合法出路。**
> 这个 bar 锚定在一块具体的卡（RTX 4060 Ti）上，而那块卡已不可得（见"canonical 吞吐结果"下的
> 机器状态表）。**一个无法据以测量的 bar 不是"弱一点的 bar"，而是根本不是 bar**，且比没有 bar 更糟
> ——它读起来仍像一道在生效的闸。不要再引用它宣称某次 run 达标或未达标。
>
> 三条合法出路（由优到次）：
> 1. **在手头这块卡上重测竞品**：同硬件、同可比场景、写明分辨率，把测得的数字注册为新 bar。
>    只有这条能真正恢复一把硬件能力标尺。
> 2. **降级为「无 bar」**：GPU 验收退回 profiler 落地的机制论证（时间花哪了 / GPU 是否饿着）。
>    更弱，但诚实。
> 3. **保留为历史 floor 并明示不可检验**——即当前采用的止血写法，直到 ① 做到为止。
>
> **⛔ 禁止用 TFLOPs / 显存带宽比值把 25M/s「折算」到新卡。** 两条独立理由，任一条即足够：
> - 规格比值是**拿代理量顶替目标量**。它只在吞吐受算力或带宽限制的区间里与吞吐同向，而这恰恰**不是**
>   bar 工作的区间——判据活在边缘，那正是这层耦合最先断裂的地方。
> - 本仓 CUDA 路径**实测是主机往返 bound**、不是 compute bound：主导成本是出射 seam 的 D2H readback
>   与命令往返（这正是第三时钟要解的机制，见下表）。受主机往返限制的路径根本不随新卡的 FLOPs 缩放，
>   折算出的数字连"错得方向一致"都做不到。
>
> 折算出来的 bar 是一个穿着阈值外衣的伪造测量值。要么去测（①），要么就说没有 bar（②）。

- 永远拿**可比**场景对标外部目标——别拿重场景（多晶/多 MS，如 `ms_multi_crystal`）的数去对标单晶单散射。
  逐光线工作量差一个数量级。
- GPU 数远低于硬件能力目标时，**无论对 legacy 比值多少都不算成功**；查利用率（CUDA 可用 nsys active%，
  见下）+ 机制，别收尾。
- 到不了目标，交付物必须是 **profiler 落地的机制解释**（时间花哪了），不是"GPU 非天然胜"的黑盒话术。
- **⚠️ 分辨率对齐**：25M bar 的竞品渲染分辨率未知；我方 `bench_light_single_ms` 是 **512×256
  （L2-resident，上界）**。真实 GUI 默认 2048×1024 下同卡同场景吞吐掉 3.6–5×（见"分辨率是一等吞吐
  维度"）。判定是否达标前必须**对齐分辨率**——别拿 512×256 的 L2-resident 数字宣称达 bar。用
  `bench_throughput.py --res-sweep` 取真实分辨率对标点。

### canonical 吞吐结果（逐机器状态：LIVE / ARCHIVED）

> 当前权威参考数字。**跨硬件数字不可比**——只在同一 host 块内读。历史 per-run 详录（带日期的表、原始
> reps、各 effort 方法论）在 `scratchpad/perf-results-log.md`。GPU 稳态率 = `drain_aligned`
> `rays_per_sec`（见下）；较旧的 scrum-312 res-sweep 块读 `multi_wall`。
>
> ⚠️ **「权威」不再等于「可复现」**：下面每个 host 块都标了 **LIVE** 或 **ARCHIVED**——
> 把任何数字当判据之前先读那个标记。

#### ⚠️ 机器状态——下面哪些数字今天还复现得出来

下面每一个数字**在测的当时都是真的**，且**一个都没有被改动**。变的不是数字，而是产出它的那台机器
还在不在。把任何一行当回归判据之前，先读它的状态：

| host 块 | 状态 | 含义 |
|---|---|---|
| **Mac**（Apple Silicon；Metal + legacy CPU） | **LIVE** | 机器仍在手上 → 可复现、可作回归判据 |
| **home-wsl**（Linux/WSL2；RTX 5090 D Blackwell + legacy CPU） | **LIVE** | 机器仍在手上 → 可复现、可作回归判据 |
| **home-win**（Windows 原生；RTX 5090 D Blackwell + legacy CPU） | **LIVE** | 机器仍在手上 → 可复现、可作回归判据 |
| **dev49**（Linux；RTX 4060 Ti Ada + legacy CPU） | **ARCHIVED — 2026-08-11 起硬件不可得** | 冻结记录；不可重跑，不可用于门控 |
| **win-builder**（Windows；GTX 1070 Ti Pascal） | **ARCHIVED — 2026-08-11 起硬件不可得** | 冻结记录；不可重跑，不可用于门控 |

两个容易被漏掉的推论：

- **现在有 LIVE 的 NVIDIA 列了**（2026-09-19/20 测量）：替代机（`home-win` / `home-wsl`，同一台物理机，
  RTX 5090 D 32GB / Blackwell sm_120）已按 N≥5 CoV 协议测过——见下方两张表。此前它一个数都没测过；
  dev49/win-builder 列仍然归档（硬件已不在），不被 5090D 数字取代——只在同一 host 块内读，不要跨代对比。
- **不要再用 TFLOPs、显存带宽或任何规格比值把已归档（或 5090D）数字缩放着填进新列**；那是造数据、
  不是测量（为何在本仓这种缩放尤其无效，见上文 bar 说明）。一列只能靠在真有的硬件上按 N≥5 CoV 协议
  重跑 `bench_throughput.py` 来（重新）填满。

#### drain-count-driven canonical · default dispatch · config 默认分辨率 · `drain_aligned` `rays_per_sec` · 2026-07-02

**背景**（drain 对齐计时 + **render-per-poll 修复**，`ee98065a`）：GPU `benchmark`
设 `ray_num="infinite"`，恰好测 N=10 个整 drain 窗口（`rate_basis="drain_aligned"`），修复 drain-量化
假低（explore-315：旧 finite 20M 下 CUDA 只 ~1.19 drain → 5× 假低）。下表是各 config **默认分辨率**下的
诚实稳态 `rays_per_sec`（**不是** scrum-312 分辨率 sweep——两块是不同 metric/轴，不可逐行比较）。每格 N
reps，>15% CoV → N=9 escalation。

> **task-317 重新 canonical 化**：下表 dev49 CUDA + legacy 列由 **render-per-poll 修复后**（commit
> `ee98065a`）重生成。修复前 `--benchmark` poll 循环每次迭代触发全图 sRGB 渲染，饿死 drain 窗口关闭，
> CUDA 上更让无界会话跑过 32-bit device PCG ray-index cap → legacy fallback → pass 永不终止。修复后 CUDA
> infinite ~1.6s 关闭（8 reps，CoV 0.1–1.8%）。这些干净 dev49 值取代修复前"首次 run"数（更噪/部分被占）。
> **win-builder 列已用 fixed-binary 重跑**，与修复前孤儿清理 run 基本一致（≤1.5% 差，在 CoV 内）——证实
> 修复对"未灾难性挂起"的机器（1070Ti render tax 非灾难性）不改变测量值，且 dev49 那 ~10% 差是旧 run 噪声非
> 系统性效应。

| config | dev49 4060Ti CUDA (vs legacy) **[ARCHIVED]** | win-builder 1070Ti CUDA **[ARCHIVED]** | Mac Metal ⚠️(近似) **[LIVE]** | dev49 legacy CPU (5M) **[ARCHIVED]** | home-wsl 5090D CUDA **[LIVE]** | home-wsl legacy CPU (5M) **[LIVE]** | home-win 5090D CUDA **[LIVE]** | home-win legacy CPU (5M) **[LIVE]** |
|---|---|---|---|---|---|---|---|---|
| `bench_light_single_ms` | **130.5 M/s** (12.5×, CoV 0.2%) | 80.7 M/s (0.4%) | ~69 M/s (CoV 11%) | 10.45 M/s | 251.6 M/s (52.1×, CoV 10.8%) | 4.82 M/s (CoV 2.1%) | 437.7 M/s (45.0×, CoV 4.6%) | 9.72 M/s (CoV 1.3%) |
| `ms_multi_crystal` | 22.2 M/s (12.7×, 0.1%) | 13.2 M/s (0.4%) | ~16.7 M/s (8.3%) | 1.74 M/s | 131.0 M/s (33.6×, CoV 6.8%) | 3.90 M/s (CoV 4.4%) | 184.6 M/s (101.4×, CoV 3.2%) | 1.82 M/s (CoV 0.1%) |
| `ms_multi_crystal_complex_filter` | 371.6 M/s (56.9×, 0.8%) | 112.4 M/s (0.6%) | ~24.6 M/s (18% 热) | 6.53 M/s | 228.7 M/s (39.3×, CoV 17.1%) | 5.82 M/s (CoV 13.3%) | 439.7 M/s (46.8×, CoV 6.0%) | 9.39 M/s (CoV 0.9%) |
| `ms_multi_crystal_filtered_bd` | 591.2 M/s (89.6×, 1.8%) | 158.8 M/s (0.8%) | ~26.7 M/s (8.4%) | 6.60 M/s | 207.5 M/s (32.3×, CoV 11.8%) | 6.41 M/s (CoV 8.5%) | 499.3 M/s (46.8×, CoV 2.5%) | 10.68 M/s (CoV 0.5%) |

**home-wsl/home-win 5090D 数据来源**（测量于 2026-09-19/20）：base `origin/main@6f38f1a7`，default
dispatch，N≥5 交错（CoV>15% 按脚本自身的判据升到 N=9——home-wsl 上 `ms_multi_crystal_complex_filter`
升级后仍读 17.1%，判 HIGH_COV_THERMAL，按实测值保留而非丢弃）。两台机是同一台物理机（WSL2 vs
Windows 原生，同 CPU/GPU，见 `doc/machines.md`）——它们的 legacy CPU 列相差幅度超出单纯 WSL
虚拟化开销能解释的范围（`ms_multi_crystal`：home-win legacy 1.82 M/s vs home-wsl legacy
3.90 M/s，即该 config 下原生反而更慢），按实测记录，不做调和。

**2026-09-24 复测，未变**（Mac、home-wsl、home-win；`origin/main@b6ed96be` 对其上叠加投影面积入射
权重改动的分支，两臂都重建，交错且轮换先后顺序，每臂 3–8 次、每次为脚本自身 N≥5 的中位）：每格分支/main
比值 0.97–1.09，legacy 与 GPU 皆然（Mac Metal 在主机有其他负载时测得，散布 0.99–1.20——没有回退，也不算提升），因此上表数值一个未改；`rays` 在每条路线上都是追迹数
且等于发放数（见上方 `[BENCHMARK]` 说明）。这次复测暴露一条测量状态事实：**home-win** 上 legacy
`ms_multi_crystal` 与 `ms_multi_crystal_complex_filter` 两格（1.82 / 9.39 M/s）只在机器闲置后的
**第一次**运行能复现；此后任一臂的每次运行都读 **1.62 / 8.64 M/s**（CoV 0.1–0.3%），两臂同样低
11% / 8%。另两格 legacy 与全部 CUDA 格首次与之后读数相同。与热机状态下的 home-win legacy 数字比较时，
先丢弃第一次运行，或轮换两臂顺序。

**要点**：
- **5× 假低已修**：`bench_light_single_ms` 4060Ti 读 **130.5 M/s** @0.2% CoV，命中 explore-315 独立实测
  plateau（400M-ray wall = 130.2 M/s）。这才是诚实 CUDA 稳态率；旧 finite-20M bench 读 24.7 M/s（5× 假低）。
- **infinite `--benchmark` 不再挂**（task-317）：读 `sim_ray_num` 改用便宜 O(1) `LUMICE_GetSimRayCount`
  替代触发渲染的 `LUMICE_GetStatsResults`，去掉饿死窗口关闭的 per-poll sRGB 渲染。dev49 CUDA infinite：
  3/3 reps RC=0 ~1.6s。
- **锁频桌面（CUDA）是权威 canonical**：4060Ti 与 1070Ti CoV 0.1–2.5%。drain-count-driven 在 Ada/Pascal(sm_61) 一致。
- **⚠️ Mac Metal 是 phase-1 近似**，非 canonical。Mac *笔记本* Metal 吞吐由热/GPU-boost 主导，跨 run 摆动
  ~2×（CoV 8–28%）；任何 N 都稳不住（N-sweep 证 CoV 不随 N 单调降——超 ~N=10 热漂移反使方差回增）。上表 Metal
  是单 run 中位数，只作数量级参考。
- **关于旧「4060Ti@512×256 = 110 M/s」（scrum-312 res-sweep `multi_wall`）**——非矛盾：那是*固定 512×256*
  的 `multi_wall`，与此处 `drain_aligned` 默认分辨率率是不同 metric+分辨率。drain-count-driven 130.4 M/s
  是诚实稳态率、取代 finite-bench 数作为 GPU 率基准；下方 res-sweep 块保留其分辨率依赖分析价值。
- 正确性（本次纯测量改动不影响）：CUDA parity 10/10 @4060Ti + 10/10 @1070Ti/sm_61；Metal parity 完好。
  Mac G1 gate `test_metal_throughput.py` 仍绿。

#### scrum-312 第三时钟 canonical · `--res-sweep` · `multi_wall` · 2026-07-01

**背景**：readback 从 trace 时钟解耦到显示节奏第三时钟（seam-design §4.8）。真实 GUI 分辨率
2048×1024 下 readback/per-batch-copy 税被摊薄。`bench_light_single_ms`（轻·单MS，L2/readback 主导），
per-resolution `multi_wall`：

| host / backend | 256×128 | 512×256 | 1024×512 | 1536×768 | **2048×1024** |
|---|---|---|---|---|---|
| dev49 RTX 4060Ti (Ada) CUDA **[ARCHIVED]** | 116 M/s | 110 M/s | 92 M/s | 65 M/s | **39.2 M/s** |
| win-builder GTX 1070Ti (Pascal) CUDA **[ARCHIVED]** | 77 M/s | 69 M/s | 45 M/s | 40 M/s | **33.5 M/s** |
| Mac M-series Metal **[LIVE]** | 28.1 M/s | 30.3 M/s | 31.2 M/s | 35.1 M/s | **32.3 M/s** |
| home-wsl RTX 5090D (Blackwell) CUDA **[LIVE]** | 250.1 M/s | 254.9 M/s | 278.4 M/s | 253.5 M/s | **250.7 M/s** |
| home-win RTX 5090D (Blackwell) CUDA **[LIVE]** | 391.0 M/s | 409.2 M/s | 397.7 M/s | 402.9 M/s | **347.6 M/s** |
| dev49 legacy CPU (baseline) **[ARCHIVED]** | 9.0 M/s | 8.8 M/s | 8.4 M/s | 7.7 M/s | 6.9 M/s |
| Mac legacy CPU (baseline) **[LIVE]** | 5.1 M/s | 4.8 M/s | 4.7 M/s | 4.8 M/s | 4.7 M/s |
| home-wsl legacy CPU (baseline) **[LIVE]** | 4.19 M/s | 4.13 M/s | 4.03 M/s | 4.17 M/s | 4.07 M/s |
| home-win legacy CPU (baseline) **[LIVE]** | 9.63 M/s | 9.65 M/s | 9.71 M/s | 9.63 M/s | 9.69 M/s |

**5090D 读平了，不是拐点**（测量于 2026-09-19/20，`bench_light_single_ms`，base
`origin/main@6f38f1a7`，N≥5，CoV 4–14%）：与 Ada/Pascal 两行不同，home-wsl 与 home-win 上 CUDA 吞吐
在 256×128→2048×1024 这整个区间内都不随分辨率下降——2048×1024 的 XYZ 平面（24 MB）仍装得进
Blackwell 更大的 L2，本表要展示的拐点在这张卡上出现在扫描区间之外，而不是区间之内。这两行 5090D
数据应读作"本区间未观察到拐点"，不是 Ada/Pascal 拐点数值的替代。

**第三时钟在 2048×1024 的增益**（vs per-batch drain 旧值，interleaved 同 binary 隔离 drain cadence）：

| host / backend | 旧（per-batch） | 第三时钟 | 增益 | 机制 |
|---|---|---|---|---|
| 4060Ti (Ada, 32MB L2) CUDA **[ARCHIVED]** | 28 M/s | 39 M/s | **1.4×** | Ada L2 大→旧值已 L2-resident，税主要是 per-batch D2H readback |
| 1070Ti (Pascal, 2MB L2) CUDA **[ARCHIVED]** | 12.5 M/s | 33.5 M/s | **2.7×** | 兼有 L2 溢出 + readback 税；第三时钟消 readback（L2 残留） |
| M-series Metal（统一内存） **[LIVE]** | 11 M/s | 32.3 M/s | **~3×** | 无 PCIe readback；per-batch 24MB memset+memcpy(×76 batch≈3.6GB) 被摊薄 |

> **⚠️ L2 尺寸这条机制论证仍然成立，但产出它的实验已不可重做。**「L2 越小、第三时钟增益越大」这个
> 结论建立在 Ada vs Pascal 的对照上（32MB vs 2MB L2），而这两块卡**都已归档**。现在手上只有一块
> N 卡，故这个两点对照**无法重跑**——换新硬件也不行（对照需要的是两块 L2 差异大的卡，不是一块新卡）。
> 请把该结论当作**基于已退役硬件的证据**：可以拿来做机制推理，但不可作为待复验的测量、也不可用于门控。

**要点**：
- **三种硬件（CUDA-Ada / CUDA-Pascal / Metal）一致确证第三时钟在真实 GUI 分辨率显著提速**；增益随
  "旧值里 per-batch readback/copy 占比"放大。
- **Metal 曲线近平**（28→35 M/s 跨 6 档），第三时钟使 Metal 分辨率-鲁棒（旧路径每 batch 全幅
  memset+memcpy 在高分辨率是真成本，非仅 CUDA readback）。**推翻早先"Metal 统一内存零收益"判断**。
- 正确性：CUDA parity 10/10 @4060Ti + 10/10 @1070Ti/sm_61；Metal parity 14/14（含 batch-invariance
  = drain-cadence 独立性）。
- win-builder 1070Ti 未列 vs-legacy（CPU 弱，比值虚高，读绝对值）；`multi_med` 列在第三时钟下失真已弃用
  （用 `multi_wall`）。
- 重场景 `ms_multi_crystal`（trace 主导，readback 占比小）增益温和：4060Ti 2048 = 14.9 M/s、1070Ti =
  7.3 M/s、Metal ≈ 17 M/s@256（Mac 热噪大，N=9 后仍抖，需稳定机复测 canonical）。

### macOS

```bash
# 构建
./scripts/build.sh -j release

# benchmark 模式（推荐——结构化输出，不写图片）
./build/cmake_install/static/Lumice benchmark -f examples/bench_config.json

# 手动模式（info 级别——带图片输出）
time ./build/cmake_install/static/Lumice -f examples/bench_config.json -o /tmp 2>&1 \
  | grep -E "Consume profile|Stats:"

# 手动模式（debug 级别——Consume 分解）
time ./build/cmake_install/static/Lumice -f examples/bench_config.json -v -o /tmp 2>&1 \
  | grep -E "Consume profile|Stats:"
```

关键输出：
- `benchmark`：两行 `[BENCHMARK]` JSON（单 worker + 多 worker），含单核和并行吞吐量数据
- 手动模式：`Consume profile` 行 + `time` 墙钟时间 → 吞吐量 = 10M / 墙钟秒数

### Windows

通过 CI 构建，然后传输二进制：

```bash
# 下载 CI 产物
gh run download <RUN_ID> --name gui-test-windows-msvc --dir /tmp/ci-win

# 传输二进制和配置到 Windows 机器
scp /tmp/ci-win/bin/Lumice.exe <windows-host>:<path>/
scp examples/bench_config.json <windows-host>:<path>/

# benchmark 模式（SSH / PowerShell）
.\Lumice.exe benchmark -f bench_config.json

# 手动模式（PowerShell 墙钟时间）
Measure-Command { .\Lumice.exe -f bench_config.json -o . 2>&1 | Out-Null } | Select-Object TotalSeconds
```

### Linux / CUDA（NVIDIA bench 机）

> ⛔ **本小节的命令按原样已不可执行（2026-08-11 起）**：它们钉死在 `dev49` 上（路径、docker 镜像、
> `/work` 挂载），而那台机器已永久不可访问。**方法本身仍然正确**，这也是保留本节的理由——用 committed
> harness `scripts/bench_throughput.py`，经 `LUMICE_BENCH_*` env 覆盖驱动，同一会话跑 `legacy,cuda`，
> 读 median + CoV。死掉的只是那些主机相关的字面量。可执行形态要等替代机装好 CUDA 工具链后才写得出来。

CUDA 吞吐曾在 dev49（RTX 4060 Ti，Linux，CUDA docker）测。两机完整 build + parity/正确性 recipe
（源码同步、docker/BuildTools 工具链、`LUMICE_HAS_CUDA` un-skip 闸、parity battery）见
[`gpu-remote-cuda-build-testing.md`](gpu-remote-cuda-build-testing.md)。**禁止每个 task 自己写 bench
脚本**——用 committed harness `scripts/bench_throughput.py`（已支持 CUDA env 覆盖、跑 canonical 场景集
含可比的 `bench_light_single_ms`、确认 GPU 路由、median+CoV）。

```bash
# dev49 CUDA docker 内（先 build -DLUMICE_CUDA_ENABLED=ON -DBUILD_SHARED_LIBS=ON）
LUMICE_BENCH_BIN=/work/build/Release/shared/bin/Lumice \
LUMICE_BENCH_LIBDIR=/work/build/Release/shared/lib \
LUMICE_BENCH_BACKENDS=legacy,cuda \
  python3 scripts/bench_throughput.py
# 只跑可比轻场景（对标 25M/s）：LUMICE_BENCH_CONFIGS=bench_light_single_ms
# dev49 共享机：严格吞吐须 idle 窗口；用完立刻清自己进程，别占机。
```

> ⚠️ **上面那条 idle-gate 的前提是「共享机」，而替代机不是共享的。**「抢空闲窗口、别杀别人的进程」
> 是 `dev49` 这台机器的属性，不是 benchmark 的属性。为独占机重新推导测量协议不在本次范围内——
> 在此标注，免得有人把共享机规矩当成普适规则搬过去。

#### GPU active% 诊断（仅 CUDA——Metal/Mac 无同口径 CLI 路径）

单看吞吐数字无法判断 GPU 是被喂饱还是饿着。CUDA 主机上把 bench 配一个 `nsys` capture 读 GPU 利用率
——这是区分"真赢"与"GPU 空转还打过 CPU 的假赢"的诊断。⚠️ 下一句的 de-risk 说的是**已退役**的 dev49；
profiler 工具链必须在替代机上重新装起来，这个诊断才重新可用——**技术手段**可迁移，
**已装好并验证过**这个状态不可迁移。
profiler 已在 dev49 de-risk（nsys 2023.4.4 +
ncu 2024.1.1；安装 + 用法见 `explore-cuda-step2-derisk/TOOLCHAIN.md`）。active% **非硬性普适门**——
macOS 上 Metal 无同口径 CLI active% 路径，故跨平台验收判据仍是上方绝对硬件能力目标；active% 是 CUDA 侧
佐证"为何这个数字高或低"的诊断。

```bash
# GPU active% = (sum of cuda_gpu_kern_sum) / wall，取自一次代表性 run 的 nsys timeline。
# 低 active%（如 1–2%）=> GPU 饿着 => 吞吐数字是 host-bound，非硬件能力结果。
nsys profile --trace=cuda --stats=true -o /tmp/rep <Lumice ...>   # 见 TOOLCHAIN.md
```

### CI 自动化基准测试

每次 push 会在所有 4 个 CI 平台（Ubuntu x64/ARM、macOS ARM、Windows MSVC）上运行 CLI
benchmark。结果汇总到 workflow run 页面的 summary 表格（`$GITHUB_STEP_SUMMARY`）。
详见 `.github/workflows/ci.yml`。

summary 表包含硬件上下文和双模式结果：

| 列 | 说明 |
|----|------|
| CPU | CPU 型号（运行时自动检测） |
| Cores | 逻辑核心数（`hardware_concurrency()`） |
| Workers | 多 worker pass 使用的 simulator 线程数 |
| Single rps | 单 worker rays/sec（单核效率） |
| Multi rps | 多 worker rays/sec（并行吞吐量） |
| Efficiency | `multi_rps / (single_rps × workers)`——并行扩展效率 |

**注意**：`Single rps` 是跨平台比较最有意义的指标，因为它自然包含了 IPC、
缓存层次和内存带宽差异，无需 GHz 归一化。`Efficiency` 揭示各平台特有的扩展瓶颈。

### 历史趋势

push 到 `main` 的 benchmark 结果会通过
[github-action-benchmark](https://github.com/benchmark-action/github-action-benchmark)
自动存储到 `gh-pages` 分支。Chart.js 仪表盘地址：

**https://lovedaisy.github.io/ice_halo_sim/bench/**

仪表盘追踪 12 条时间序列（4 平台 × 3 指标）：

| 指标 | 单位 | 说明 |
|------|------|------|
| `<Platform> / Single` | rays/sec | 单 worker 吞吐量（单核效率） |
| `<Platform> / Multi` | rays/sec | 多 worker 吞吐量（并行管线） |
| `<Platform> / Efficiency` | % | `multi_rps / (single_rps × workers) × 100`——自参照并行扩展指标 |

**如何解读图表**：
- Single/Multi rps **突然下降**可能是代码回归，也可能是 CI runner 硬件变更（查看 tooltip 中的 CPU 型号）
- **Efficiency** 对 runner 硬件变化免疫——Efficiency 下降可靠地表示并行扩展回归（如锁竞争加剧）
- 告警阈值设为 200%（性能下降超过 50% 才触发 commit 评论）

**已知限制**：
- 仅存储 `main` 分支 push 的历史；功能分支 benchmark 仅在 per-run summary 表中显示
- CI runner 硬件可能不经通知就更换，导致绝对 rps 指标出现阶跃变化
- 告警阈值是全局的（所有指标共享 200%）；Efficiency 回归低于阈值时需人工巡检图表

### 运行时调优旋钮

| 环境变量 | 默认 | 说明 |
|----------|------|------|
| `LUMICE_DISPATCH_RAY_NUM` | **262144**（CUDA）/ **32768**（Metal）/ 128（CPU） | task-268.4 旋钮；GPU 每-dispatch 网格大小。scrum-268.6 定 Metal 默认 32768；scrum-306.2 定 CUDA 默认 262144（都是实测甜点）。摊薄 GPU kernel 启动 + per-batch host 开销；小 dispatch 饿死 GPU（512/2048 = 0.2–0.8× legacy，128 在大 ray_num 下 hang）。设为 2 的幂对齐。server 启动时读取，运行中改无效。与 `LUMICE_COMMIT_RAY_NUM` 独立——喂大 GPU 而不牺牲 GUI 节奏。**分辨率依赖**：262144/32768 甜点测于 **512×256**；分辨率升高最优上移（2048×1024 下 CUDA ~786K–2M，实测 262144→~1M = 2.25×），因 per-batch readback buffer 16× 大、需更大 dispatch 摊薄。分辨率变时重标（见"分辨率是一等吞吐维度"）。 |
| `LUMICE_COMMIT_RAY_NUM` | 128 | task-268.4：`ConsumeData` 内 SimData-to-consumer 提交粒度。更小提交 = 更细 GUI 快照节奏（与 dispatch 大小无关）。仅 backend exit-seam 路径（legacy CPU SimData 绕过 chunker）。**⚠️ GPU device-fused 路线（Metal/CUDA，`SupportsDeviceXyzAccum()`==true）上是 no-op**：该路径产出 `xyz_pixel_data_` 而非 `outgoing_d_`，commit chunker（`server.cpp:809`）被跳过；device readback 频率实由 `LUMICE_DISPATCH_RAY_NUM`（每 SimBatch 一次 `ReadbackXyzAccum`）决定，**非本旋钮**。（曾误用本旋钮做"readback 无关"判据，无效。） |
| `LUMICE_BATCH_RAY_NUM` | （已废弃） | 仅作 `LUMICE_COMMIT_RAY_NUM` 的向后兼容回退。task-268.4 前它同时兼作 dispatch 和 commit 粒度。scrum-268：DISPATCH 拆分是 GPU 吞吐的主驱动；现在设 `LUMICE_BATCH_RAY_NUM` 只控 commit 节奏、不控 GPU dispatch 大小。优先用上面两个拆分 env。 |
| `LUMICE_TRACE_BACKEND` | 未设（legacy CPU） | trace 后端选择：未设 = legacy CPU；`metal` = Metal GPU 后端；`cuda` = CUDA GPU 后端；`cpu_backend` = SoA CPU 后端。 |
| `LUMICE_DISABLE_DEVICE_GEN` | 未设（device-gen 开） | 逃生舱：强制 GPU 后端走 **host** root-ray 生成。device root-gen（GPU PCG root-ray 供给）在合格层（single-crystal-per-ci，`tri_count ≤ 64`）是**默认**。仅在镜像 host `std::mt19937` 流的严格-一致 parity 测试里设 `1`（device PCG 流无法与之对齐）。每个后端实例构造时读一次。 |

> #### ⚠️ `LUMICE_DISPATCH_RAY_NUM` 是 GPU 专属旋钮——比较中绝不施给 legacy（scrum-306.1/306.4）
>
> `LUMICE_DISPATCH_RAY_NUM` 定 GPU 引擎的 per-dispatch 网格。**CUDA 总能量 dispatch-不变**
> （实测：ΣY = 261.29 M ±0.001% 跨 dispatch ∈ {128, 8192, 32768, 131072}，`dual_fisheye_ref`）——
> 即 GPU 结果在任何 dispatch 下都正确。**legacy（CPU）总能量 dispatch-不不变**：同旋钮（override
> legacy 的 `kDefaultRayNum`=128）使 legacy ΣY 波动 **−5%..+13%**——这是 per-batch 波长采样的
> Monte-Carlo 方差，**非 bug**（默认 128 收敛正确；见 per-run log 里 scrum-306.7）。
>
> **须避免的历史误诊**：为探 CUDA 而全局设 `LUMICE_DISPATCH_RAY_NUM=131072`，使
> `test_cuda_single_ms_no_filter_parity` 报 `energy_ratio=0.8922`——曾被**误归为"CUDA
> exit-cap/cont-cap 静默丢能量"**（原 scrum-304.3 backlog 条）。这不是 CUDA bug：旋钮漏进 legacy
> 参考 run、膨胀了**分母**（legacy_Y），`261.29/292.87 = 0.892`。parity harness 现对 `legacy` 后端
> 剥掉 `LUMICE_DISPATCH_RAY_NUM`（`test/e2e/capi_runner.py`，scrum-306.4），使 oracle 停在 canonical
> 默认，`energy_ratio` 只反映 GPU 后端正确性（复验：@131072 0.8922 FAIL → 1.0063 PASS）。
> `bench_throughput.py` 已把 legacy 排除出 dispatch sweep（`DISPATCH_PLAN["legacy"]=[None]`）。
>
> **过程教训**：304.3 的正确性断言只以 backlog 一行存在、无保留脚本 → 一个错诊断（CUDA）传播且无法
> 对齐。正确性断言必须落进 tracked 文档 + 可复现 recipe。复现：容器 `pip install pytest numpy`，
> `LUMICE_HAS_CUDA=1`，然后
> `LUMICE_DISPATCH_RAY_NUM=131072 pytest -m slow test/parity-cross-backend/backend/test_cuda_exit_seam_parity.py::test_cuda_single_ms_no_filter_parity`；
> 逐 dispatch ΣY 拆分用 `bench_work/harness2.cpp`（dev49）。

#### legacy CPU 批大小（`kDefaultRayNum`=128）：绑定约束是随场景变化的天花板，不是批条数

上面那条旋钮定的是 GPU dispatch 网格；但在 legacy CPU 路径上，同一个 `LUMICE_DISPATCH_RAY_NUM`
会 override `kDefaultRayNum`（`src/server/server.cpp:142`，消费点在 `server.cpp:1620`），**确实**
改变每个 SimBatch 交给 worker 池的光线条数。在两颗 CPU（一台 16 核 x86 + 一台 12 核 Apple M2 Max）
× 两个场景族（一个轻量单晶体单次散射场景、一个重的多晶体多次散射场景）上把该轴从 128 扫到 8192，
**没有找到一个对两个场景都好的批大小**——因为绑定约束是一个**交接率天花板，而这个天花板本身随
场景变化，两个场景之间相差 4.8×**：

- 轻场景在默认批（128）上就已达峰值 **51.8 k batch/s**——顶在天花板上。把它的批抬到 256 使交接率
  降到 28.0 k，于是同时买到 **+16.2% 峰值吞吐**与更平的 worker 数曲线（worker 数 ≥ 8 区间的
  最优/最差比 1.59× → 1.24×）。这是**双轴严格占优**，所以 128 并不是这个场景的最优。
- 重场景峰值只有 **10.8 k batch/s**——在测过的每一档都远低于天花板。加大它的批只买到延迟与不均衡，
  吞吐单调变差：256 档 −15.2%、512 档 −24.2%、8192 档 −34.5%。这个场景上 128 **就是**最优，
  且它落在扫描的下边界上。
- 在 M2 Max 上整条轴对两个场景都几乎不起作用（轻场景 −2.6%…+6.8%、重场景 −5.9%…+2.0%），且与
  x86 机器不同，它的 worker 数曲线在默认批上就已接近平（1.07× / 1.26×，对比 1.59× / 1.41×）。
  这台机器上批大小这个问题本来就接近不存在——反过来说，只在 Mac 上扫一遍就断言「这条轴是死的」
  不成立。

**为什么 `kDefaultRayNum` 保持 128。** 改成 256 是拿轻场景的 +16% 去换重场景的 −15%，不是净胜，
只是换一个被偏袒的场景。这个常数还兼任提交粒度的默认值（`kCommitCap =
env::CommitRayNum(logger_, kDefaultRayNum)`，`src/server/server.cpp:1324`），抬高它会连带把 GUI
快照节奏变粗：它的射程比纯 CPU 吞吐更宽。（补充指针，非本次扫描的结论：worker 数是与批大小
独立的另一条轴，其默认值单独由 `ServerImpl::AutomaticWorkerBaseAndCap()` 封顶，`src/server/server.cpp:269`。）

**批大小有一个 <40 光线的硬地板，且失效形态是崩溃而不是变慢。** `LUMICE_DISPATCH_RAY_NUM` ≤ 32
在轻场景族上确定性地崩在 `RayBuffer::DupOverflowSlot` 内（`src/config/sim_data.cpp:158`），
两个平台都复现：x86/Linux 上 8/16/24/32 档每一次 run 都 SIGSEGV，而 40/48/64/96/128 档全绿；
arm64/macOS 上 16 与 32 档 SIGSEGV、8 档 abort，同样 ≥ 40 正常。重场景在同样的批大小下跑得好好的
（8/16/32/64 全绿），所以这个地板同样是场景相关的。今天的用户暴露面为零（默认就是 128，
也没有任何产物发更小的值），但任何「下调批大小」的方案都被它挡住，而一次走到 40 以下的扫描会直接
崩掉而不是给出一个数。

**GPU device root-gen（scrum-260）**：GPU 后端上，root 光线（取向 / 方向 / 入射点）经 counter-based
PCG 流 `(gen_seed, gen_ray_base + tid)` 在 device 上生成，替代 host 预生成 + 上传。这是默认路径；对
legacy 的统计等价性由 slow-e2e parity harness 验证（`ds_corr ≥ 0.99`）。device-gen ON/OFF 吞吐刻画
（Metal，phase-1）——默认 batch 下 device-gen 对单晶单散射是净亏、对多晶多 worker 是净赚，大 batch 才
有强赢——记录在 `scratchpad/perf-results-log.md`。

## 2. GUI 性能测试（隐藏窗口，无 VSync）

由 ImGui Test Engine 驱动的自动化 GUI 测试。使用隐藏窗口和 `swapInterval(0)`，
因此**不反映真实显示/VSync 开销**。

默认应用 16ms 帧率限制（匹配真实应用的 `kTargetFrameTimeMs` 回退），以产生现实基线指标。
使用 `--no-frame-limit` 禁用以进行原始无限 FPS 比较。

两个测试场景：
- **steady_state**：2 秒累积——测量 rays/sec + 纹理 FPS
- **slider_drag**：5 秒交替参数变化——测量 rays/sec、帧数、restart 次数与上传次数（first_upload 延迟与每次上传光线数 CV 来自日志分析，不由该场景产出）

### 诊断标志

| 标志 | 默认 | 描述 |
|------|------|------|
| `--visible` | 关 | 显示 GLFW 窗口（用于 display/DWM 测试） |
| `--vsync` | 关 | 启用 VSync（隐含 `--visible`） |
| `--frame-limit` | 开 | 应用 16ms sleep 帧率限制 |
| `--no-frame-limit` | — | 禁用帧率限制以测试原始吞吐量 |
| `--main-loop-commit` | 关 | 在主循环中调用 `CommitConfig`（忠实模拟真实应用） |
| `--log-panel` | 关 | 测试期间显示日志面板 |
| `--dorun-delay <ms>` | 0 | 为 `DoRun` 添加人为延迟（模拟慢环境） |
| `--skip-calibration` | 关 | 跳过启动时质量阈值校准 |

### macOS

```bash
# 构建（需要 -gt 生成 GUI 测试目标）
./scripts/build.sh -gtj release

# 仅运行性能测试（PERF 输出与服务器日志都在 stderr）
./build/Release/static/bin/gui_test --filter perf_test \
  > /tmp/perf_stdout.txt 2>/tmp/perf_stderr.txt
grep "\[PERF\]" /tmp/perf_stderr.txt

# debug 级别（增加 ConsumeData 每批次 + Consume 剖析）
./build/Release/static/bin/gui_test --filter perf_test --log-level debug \
  > /tmp/perf_stdout_debug.txt 2>/tmp/perf_stderr_debug.txt
grep "Consume profile" /tmp/perf_stderr_debug.txt
```

**注意**：PERF 结果与引擎的服务器日志都在 **stderr**（引擎的 console sink 写到 stderr；stdout 只有测试
框架自己的汇总），一份 `2>` 捕获两者都有，`[PERF]` / `Consume profile` 两个 grep 读同一个文件。

### Windows

```bash
# 从 CI 产物传输 GUI 测试二进制
scp /tmp/ci-win/bin/gui_test.exe <windows-host>:<path>/

# 运行（PowerShell，必须使用 --filter 避免非性能测试崩溃）
$proc = Start-Process -FilePath ".\gui_test.exe" `
  -ArgumentList "-nopause","--filter","perf_test" `
  -NoNewWindow -Wait `
  -RedirectStandardOutput "perf_stdout.txt" `
  -RedirectStandardError "perf_stderr.txt" `
  -PassThru
Write-Host "Exit code: $($proc.ExitCode)"

# 查看结果
Get-Content perf_stderr.txt
```

**已知问题**：在 Windows SSH 下运行完整 GUI 测试套件会导致 ACCESS_VIOLATION。
使用 `--filter perf_test` 仅运行性能测试。

### 完整流程

每个平台运行两次：
1. 不设 `--log-level`（默认 info）——用于精确吞吐量数据
2. 设置 `--log-level debug`——用于 Consume 剖析分解

## 3. GUI 手动测试（可见窗口，VSync）

反映真实用户体验。需要带可见窗口的显示环境。

### 步骤

1. 启动 GUI 应用（macOS：双击 .app，Windows：直接运行 .exe）
2. 加载配置，点击 Run
3. **稳态测试**：等待约 5 秒累积，观察 rays/sec
4. **拖拽测试**：持续拖拽晶体高度滑动条约 10 秒
5. 启用文件日志（GUI Log 面板 → Enable File Log）
6. 使用分析脚本分析日志文件

### Windows 远程测试

无需物理接触即可测试 Windows，使用 watcher 远程工作流。
参见 [Windows 远程测试指南](windows-remote-testing_zh.md) 了解设置说明。

```bash
# 在 Windows 上运行带 VSync 的 GUI 性能测试
./scripts/win_remote_test.sh /tmp/ci-win/bin/gui_test.exe \
  --filter perf_test --vsync --log-level verbose
```

### 对比

| 条件 | GUI 性能测试 | 手动测试 |
|------|-------------|---------|
| 窗口 | 隐藏（默认）/ 可见（`--visible`） | 可见 |
| VSync | 关（默认）/ 开（`--vsync`） | 开（系统默认） |
| 帧率限制 | 开（默认）/ 关（`--no-frame-limit`） | 开 |
| 输入 | 自动化（ImGui Test Engine） | 手动（滑动条拖拽） |
| 可重复性 | 高 | 低（人为差异） |
| 反映真实体验 | 部分（配合 `--visible --vsync`） | 是 |

## 4. 日志分析脚本

`scripts/analyze_perf_log.py` 解析 GUI/Poller 日志文件，检测操作阶段
（STEADY / DRAG / PAUSE），并报告每阶段性能指标。

```bash
# 文本输出（比较多个文件）
python scripts/analyze_perf_log.py log1.log log2.log

# 带可视化（PNG 图表）
python scripts/analyze_perf_log.py -p log1.log log2.log

# 过滤时间范围（从日志开始的秒数）
python scripts/analyze_perf_log.py --from 2.0 --to 8.0 log.log

# 输出图表到指定目录
python scripts/analyze_perf_log.py -p -o /tmp/perf_plots log.log
```

主要功能：
- 阶段检测：STEADY（硬件吞吐量）、DRAG（交互响应性）、PAUSE（恢复）
- 每周期指标：首次上传延迟、上传光线数、commit 延迟
- 首次上传分解：commit 延迟 + 门控等待
- 多文件时的对比表格和 CDF 图表
