[English version](03-cli-quickstart.md)

# CLI 快速上手

本章介绍如何在命令行下跑 Lumice — 适合批处理、CI、无显示器的服务器、可复现的 recipe。

> **前置**：`build/cmake_install/static/Lumice` 已构建。如果还没有，参见 [`01-install_zh.md`](01-install_zh.md)。

## 1. 最简调用

最少参数：

```bash
./build/cmake_install/static/Lumice -f examples/config_example.json
```

这会把 JPEG 输出写到当前工作目录。指定输出目录：

```bash
./build/cmake_install/static/Lumice -f examples/config_example.json -o /tmp/lumice-out
```

![Lumice 启动](../figs/cli_screenshot_01.jpg)

## 2. 看输出

跑完后输出目录里会有"每个 render 条目一张图"。内置示例定义了 4 个渲染条目，因此会得到 4 张图：

| 文件 | 镜头 / 视图 | 说明 |
|------|-------------|------|
| `img_01.jpg` | 等积双鱼眼，全天空 | 默认的"全天看光晕"视图 |
| `img_02.jpg` | 线性镜头，较窄视场 | 接近"普通相机去畸变"的视图 |
| `img_03.jpg` | 等距鱼眼 | 适合按角度量取光晕半径 |
| `img_04.jpg` | 立体投影鱼眼 | 靠近地平线区域圆形保持得好 |

![示例输出 1](../figs/example_img_01.jpg)
![示例输出 2](../figs/example_img_02.jpg)
![示例输出 3](../figs/example_img_03.jpg)
![示例输出 4](../figs/example_img_04.jpg)

控制台还会打印一段 `Stats:` 总结（光线数、耗时、按波长累积等）。想要复现某次跑出的图，把这段保存下来即可。

两条流是有意分开的：产品输出——这里的 `Saved:` / `Stats:`、`benchmark` 的 `[BENCHMARK]` JSON、`analyze` 的 CSV——走 **stdout**；所有诊断日志（`-v` 打开的和引擎自己的消息）走 **stderr**。所以 `Lumice -f config.json -o out 2>/dev/null` 只打印产品行；想把凭据和日志放进同一个文件，用 `> run.log 2>&1`。CLI 与 GUI 程序自己的日志行（`GUI_LOG_*`）都走 stderr——GUI 的 stdout 上没有产品输出要跟日志行抢占。

## 3. Verbose 与 Debug 模式

```bash
./build/cmake_install/static/Lumice -f config.json -v   # trace 级日志
./build/cmake_install/static/Lumice -f config.json -d   # debug 级日志
```

`-v` 大致是"按 batch 显示生成进度"；`-d` 额外输出 RNG seed、散射层调度等诊断信息。先用 `-v`，只在追 bug 时再上 `-d`。

## 4. 完整 flag 一览

CLI 有四个子命令：`render`、`benchmark`、`analyze` 与 `raypath`。`render` 是默认值：`Lumice -f config.json` 与 `Lumice render -f config.json` 是同一条命令，所以上面所有示例跑的都是 render。每个子命令只接受自己的选项；`Lumice <子命令> -h` 打印该子命令的帮助页。

`Lumice -h` 打印的完整列表（事实锚：`./build/cmake_install/static/Lumice -h`）：

```text
Usage: ./build/cmake_install/static/Lumice [render] -f <config_file> [options]
       ./build/cmake_install/static/Lumice benchmark -f <config_file> [options]
       ./build/cmake_install/static/Lumice analyze -f <config_file> [options]
       ./build/cmake_install/static/Lumice raypath -f <config_file> --crystal <id> --path <faces> --target <alt>,<az>
       ./build/cmake_install/static/Lumice --version
       ./build/cmake_install/static/Lumice <subcommand> -h

Lumice — simulate ice halos by tracing rays through ice crystals.

Subcommands:
  render             Simulate and write halo images. This is the default when
                     no subcommand is given: `./build/cmake_install/static/Lumice -f ...` is a render.
  benchmark          Run a throughput benchmark and print [BENCHMARK] JSON
                     (`./build/cmake_install/static/Lumice benchmark -h` for its options)
  analyze            List the raypath chains that light a region of the sky, as CSV
                     (`./build/cmake_install/static/Lumice analyze -h` for its options)
  raypath            Analyse one raypath over one sky point: its fiber of crystal
                     poses and its sun-direction sphere, as JSON
                     (`./build/cmake_install/static/Lumice raypath -h` for its options)

Options for render (the default subcommand):
  -f <file>          Specify the configuration file (required)
  -o <dir>           Output directory for rendered images (default: current directory)
  --format <fmt>     Output image format: jpg or png (default: jpg)
  --quality <1-100>  JPEG quality (default: 95, ignored for PNG)
  --seed <N>         Fix the simulation's random seed (a positive integer) so two runs
                     of the same config are the same run; this also sizes the pool to
                     one worker (a seeded run is single-threaded by contract).
                     Default: random.
  --backend <name>   Trace backend: auto, cpu, metal, or cuda (default: auto).
                     'auto' and 'cpu' both select the CPU route today; 'metal'
                     falls back to CPU if unavailable. The LUMICE_TRACE_BACKEND
                     env var, if set, still overrides this (debug/CI only).
  --workers <N>      Number of CPU simulation worker threads (default: automatic —
                     one per physical core on Linux/macOS, one per logical core on
                     Windows, each capped at a measured per-platform ceiling; an
                     explicit N is never capped).
                     Machine-dependent, so it is a command-line switch rather than
                     a config-file field: a config travels between machines and a
                     worker count should not travel with it. Ignored on a GPU route
                     (single engine).
  -v                 Verbose output (trace level logging)
  -d                 Debug output (debug level logging)
  -h, --help         Show this help message and exit

Examples:
  ./build/cmake_install/static/Lumice -f config.json
  ./build/cmake_install/static/Lumice -f config.json -o /tmp/output
  ./build/cmake_install/static/Lumice -f config.json --format png
  ./build/cmake_install/static/Lumice -f config.json --quality 80
  ./build/cmake_install/static/Lumice -f config.json --seed 7
  ./build/cmake_install/static/Lumice -f config.json --backend metal
  ./build/cmake_install/static/Lumice -f config.json --workers 4
  ./build/cmake_install/static/Lumice -f config.json -v
  ./build/cmake_install/static/Lumice benchmark -f examples/bench_config.json
  ./build/cmake_install/static/Lumice analyze -f config.json --roi cone --center 43,0 --radius 2
  ./build/cmake_install/static/Lumice raypath -f config.json --crystal 1 --path 3-5 --target 20,25
```

`Lumice benchmark -h`：

```text
Usage: ./build/cmake_install/static/Lumice benchmark -f <config_file> [options]

Run a throughput benchmark and print [BENCHMARK] JSON. The legacy CPU route
runs a dual pass (single-worker + multi-worker → per-core and parallel-
efficiency data); a GPU route is single-engine, so it runs one steady pass
only (single/multi would not be parallel). The worker counts are part of the
measurement methodology, which is why there is no --workers here; nothing is
written to disk, which is why there is no -o.

Options:
  -f <file>          Specify the configuration file (required)
  --backend <name>   Trace backend: auto, cpu, metal, or cuda (default: auto).
                     'auto' and 'cpu' both select the CPU route today; 'metal'
                     falls back to CPU if unavailable. The LUMICE_TRACE_BACKEND
                     env var, if set, still overrides this (debug/CI only).
  -v                 Verbose output (trace level logging)
  -d                 Debug output (debug level logging)
  -h, --help         Show this help message and exit

Examples:
  ./build/cmake_install/static/Lumice benchmark -f examples/bench_config.json
  ./build/cmake_install/static/Lumice benchmark -f examples/bench_config.json --backend metal
```

`Lumice analyze -h`——把 GUI「Raypath Analysis」窗口做的光路分析（结果怎么读见 [`06-raypath-analysis_zh.md`](06-raypath-analysis_zh.md)）搬到命令行，输出与该窗口「Export CSV」相同的 CSV：

```text
Usage: ./build/cmake_install/static/Lumice analyze -f <config_file> [options]

Trace the scene and list the raypath chains that delivered energy into a region
of the sky, most energetic first, as CSV — the same file the GUI's Raypath
Analysis window exports. The config is the scene; the question asked of it is
given by the options below and never read from the config. The analysis always
traces on the CPU (the chain record exists on that route only).

Output: the CSV goes to stdout, or to --csv <path> instead (never both). A `#`
head names the region, the symmetry, the ray total and the export time; then
one row per chain: Raypath, Energy (% of the total), Cumulative %, +/- (the
row's 1/sqrt(count) relative noise, in %). Progress goes to stderr, one line per
second. Ctrl-C ends the run early and still writes the result accumulated so far
(exit 0) — which is how a scene whose ray_num is "infinite" is meant to be run.
With --csv the file is rewritten atomically every second, so it is complete at
any moment it is read.

Options:
  -f <file>          Specify the configuration file (required)
  --roi <region>     Which rays count: sky (every outgoing ray; default), frame (the
                     rays that land inside one of the config's render[] frames), or
                     cone (the rays within --radius of --center).
  --render-id <id>   frame only: the render[] entry whose lens / view / visible /
                     front / resolution define the frame (default: the first entry).
  --center <alt>,<az>
                     cone only (required): the cone's centre as the altitude and
                     azimuth, in degrees, of the sky point — azimuth measured as the
                     sun's is, so the sun sits at --center <sun_altitude>,0.
  --radius <deg>     cone only (required): the cone's angular radius in degrees.
  --symmetry <spec>  Merge chains that are the same path up to crystal symmetry when
                     listing: any combination of P, B, D (case-insensitive) or
                     `none` (default: PBD). Changes the grouping, never the totals.
  --rays <N>         This run's ray budget, total across wavelengths; N may carry a
                     K, M or G suffix (e.g. 20M). Default: the scene's own ray_num,
                     including "infinite".
  --chain-capacity <N>
                     How many distinct (unreduced) raypath chains the record keeps
                     exact; chains past it fall into the `other (not recorded)` row
                     and the head's record_full_hits counts how often. A plain
                     integer in [1, 1048576], no suffix. Default: 16384, which holds
                     the multi-scatter scenes it was measured on; a single-crystal
                     --symmetry none listing of every long path can need more (a
                     hexagonal prism's 8-face set alone is ~29k chains). Costs
                     memory: about workers x N x 220 bytes on the trace side, plus
                     roughly as much again once for the listing — so 32768 on 10
                     workers is ~140 MB and the maximum is several GB; --workers
                     above the automatic count multiplies the first term.
  --seed <N>         Fix the simulation's random seed (a positive integer) so two
                     runs of one question are the same run; this also sizes the pool
                     to one worker (a seeded run is single-threaded by contract).
                     Default: random.
  --csv <path>       Write the CSV to this file instead of stdout.
  --backend <name>   Trace backend: auto, cpu, metal, or cuda (default: auto).
                     'auto' and 'cpu' both select the CPU route today; 'metal'
                     falls back to CPU if unavailable. The LUMICE_TRACE_BACKEND
                     env var, if set, still overrides this (debug/CI only).
  --workers <N>      Number of CPU simulation worker threads (default: automatic —
                     one per physical core on Linux/macOS, one per logical core on
                     Windows, each capped at a measured per-platform ceiling; an
                     explicit N is never capped).
                     Machine-dependent, so it is a command-line switch rather than
                     a config-file field: a config travels between machines and a
                     worker count should not travel with it. Ignored on a GPU route
                     (single engine).
  -v                 Verbose output (trace level logging)
  -d                 Debug output (debug level logging)
  -h, --help         Show this help message and exit

Examples:
  ./build/cmake_install/static/Lumice analyze -f config.json
  ./build/cmake_install/static/Lumice analyze -f config.json --roi cone --center 43,0 --radius 2
  ./build/cmake_install/static/Lumice analyze -f config.json --roi frame --render-id 1 --csv frame.csv
  ./build/cmake_install/static/Lumice analyze -f config.json --symmetry none --rays 5M --seed 7
```

`Lumice raypath -h`——接在 `analyze` **之后**的一步：从它的列表里选定一条光路后，只针对这一条问下去。给它一个晶体条目上的一条单层光路和一个天空点，它写出一份 JSON：使太阳落到该点的晶体姿态纤维的各个分量（每个分量带逐点详情），以及这条光路在整个太阳方向球上的偏折角。字段含义见 [`../raypath-cli-output.md`](../raypath-cli-output.md)。选项：

| 选项 | 含义 |
|---|---|
| `-f <file>` | 场景 config（必需）。晶体取名义形状（各形状分布的中心），太阳当作点光源。 |
| `--crystal <id>` | 晶体条目的 config id（必需）。 |
| `--path <faces>` | 光路，写法同 `analyze` 的输出，如 `3-5`、`3-6-4-8`（必需）。多层链会被拒绝。 |
| `--target <alt>,<az>` | 天空点，单位度，方位角按太阳的方式量（必需；与 `analyze --center` 同约定）。 |
| `--wavelength <nm>` | 取值 [350, 900]；默认取 config 的（仅当它恰好一个波长），否则 550。 |
| `--events <N>` | 分量搜索的种子事件数（不是追迹的光线数）；可带 `K`/`M`，上限 100M，默认 1M。事件越多越能找到小分量。 |
| `--grid <rows>` | 太阳方向网格的纬向行数（经向为其两倍），0–720，默认 90；0 表示不输出。 |
| `--warm <file>` | 此前的一份输出；其种子作为搜索起点，使已找到的分量不丢。 |
| `-o <path>` | 把 JSON 写到该文件而不是 stdout。 |

输出是确定性的（没有 `--seed` 与 `--workers`）。进度是 stderr 上的两行；Ctrl-C 直接结束、不写任何东西。

两组示例，都可从仓库根目录原样运行（晶体 1 是 `examples/config_example.json` 里高 1.2 的棱柱，晶体 6 是其中高 0.3 的片晶，太阳高度 20°）：

```bash
# 棱柱光路 × 一个天空点：一个闭合分量。
./build/cmake_install/static/Lumice raypath -f examples/config_example.json --crystal 1 --path 3-5 --target 20,25 -o r.json

# 晶体形状起作用的光路：片晶上的 3-6-4-8。
./build/cmake_install/static/Lumice raypath -f examples/config_example.json --crystal 6 --path 3-6-4-8 --target 20,120 -o r.json
./build/cmake_install/static/Lumice raypath -f examples/config_example.json --crystal 6 --path 3-6-4-8 --target 20,140 -o r.json
```

第一条得到 `outcome: "discovered"` 与一个 `closed` 分量。后两条是退化情形，值得读一遍。`3-6-4-8` 是绕 c 轴的一次转动，所以它的偏折角只取决于太阳在晶体系中的纬度：等值线是一条纬线圈，而纤维是它的一段**圆弧**，不是闭环。片晶上两次运行都返回 `components: []`，靠 `reach.target_in_range` 区分：`20,120`（偏折角 108.9°）时为 `true`——纤维存在，只是这么矮的晶体没有任何光线沿它通过（同一目标点上，高 1.2 的棱柱即晶体 1 能找到两段圆弧）；`20,140`（偏折角 124.0°，超过这条光路 120° 的最大值）时为 `false`：这类晶体无论取什么姿态都不能把太阳送到那里。出射方向与姿态根本无关的光路（片晶的 `1-2`）则返回 `outcome: "point_mass"`。三种空结果的完整读法见 [`../raypath-cli-output.md`](../raypath-cli-output.md) §3.9 的表。

要点：

- `-f` 是唯一必需 flag。不带它会以非零退出并打印 usage 提示。
- `--format png` 切到无损 PNG，此时 `--quality` 被忽略。
- `--backend <name>` 只是**请求**一个 trace 后端；这次运行实际拿到了什么，看 `Stats:` 行：`backend=<cpu|metal|cuda>` 是运行真正跑在哪个后端上，`fell_back=true` 表示 GPU 后端曾拿到、随后丢失或被拒绝（运行中途设备出错，或 config 是它服务不了的），其余部分改走了 CPU 路——原因是 stderr 上的一条 `WARN`。本构建或本机根本提供不了的后端（比如在 Mac 上 `--backend cuda`）不算回退：运行从一开始就按 CPU 路配置，读到的是 `backend=cpu, fell_back=false`，外加启动时的一条警告。`benchmark` 子命令的 `[BENCHMARK]` JSON 带同样的两个字段。多渲染器 config（GUI 导出的文档带两个）在 GPU 路上作为一个 session 运行——见 [`../configuration_zh.md`](../configuration_zh.md#多个渲染器与-gpu-路)。
- `--workers <N>` 覆盖自动 worker 数（Linux/macOS 上每个物理核一个、Windows 上每个逻辑核一个，各有一个按平台实测的上限；该上限只作用于自动值，你显式给出的 `N` 永远不受它约束）。它是命令行开关而不是 config 字段，是有意的：worker 数描述的是**机器**，而 config 文件会在机器之间流转。非法值（`0` / 负数 / 非数字）以非零退出，不静默回退到默认值。
- `Lumice analyze -f <config>` 不渲染，而是向场景提一个问题：哪些光路链把能量送进了天空的某个区域，结果是 stdout 上的 CSV（或 `--csv <path>` 写文件）。config 只是场景；区域、行合并所用的对称性、光线预算与随机种子全是选项，从不读 config——所以脚本可以拿同一份 config 问多个问题，加 `--seed` 即可复现任何一次。`ray_num` 为 `"infinite"` 的场景会一直跑到 Ctrl-C，仍然写出结果；带 `--csv` 时文件每秒原子重写，任何时刻读到的都是完整文件。进度行走 stderr，stdout 只有 CSV。
- `Lumice raypath -f <config> --crystal <id> --path <faces> --target <alt>,<az>` 针对单条光路而不是列表提问：先从 `analyze` 的输出里选一条光路，再问它在某个天空点上对应哪些晶体姿态。结果是 stdout 上的 JSON（或 `-o <path>`，原子写入）；它是确定性的，同一个问题跑两次输出逐字节相同。
- `Lumice benchmark -f <config>` 用于性能回归测试 — 详见 [`../performance-testing_zh.md`](../performance-testing_zh.md)，**不是**普通模拟用法；它只接受 `-f`、`--backend`、`-v`、`-d`、`-h`（没有 `-o`：它不写文件；没有 `--workers`：worker 数就是测量口径本身）。原来的 `--benchmark` 旗现在会报错并给出指向这里的迁移提示。

## 5. 性能预期

Lumice 按波长追踪光线。对于离散波长 spectrum（典型场景：`light_source.spectrum: [{wavelength, weight}, ...]`），`ray_num` 是**所有波长加起来的总数**（换元的精确规则见 [`../configuration.md`](../configuration.md)），工作量随这个总数变化，不是随单波长数变化。示例配置的 `ray_num` 是 `4.5e8`（`450000000`）——9 段波长 spectrum 的总数，折合每段约 5×10⁷ 条光线。

新手首跑建议：

- 想几秒看到结果？把 `ray_num` 降到 `1e6`，spectrum 改成单波长（`[{"wavelength": 550, "weight": 1.0}]`）。
- 想出版级清晰度？把 `ray_num` 保持在 `4.5e8` 以上 + 完整 9 段 spectrum，预期在现代多核笔记本上约 2 分钟。

`ray_num` × batch × wavelength 的精确关系，以及更深入的性能调优，见 [`05-faq_zh.md`](05-faq_zh.md) "ray_num × wavelength 语义" 和 [`../performance-testing_zh.md`](../performance-testing_zh.md)。

## 延伸阅读

- 试现成 recipe → [`04-recipes_zh.md`](04-recipes_zh.md)
- FAQ、默认值、GUI 与 JSON 差异 → [`05-faq_zh.md`](05-faq_zh.md)
- 完整 schema → [`../configuration_zh.md`](../configuration_zh.md)
