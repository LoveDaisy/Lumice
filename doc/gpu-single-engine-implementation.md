# GPU 单引擎实现设计（seam-design §5 落地）

> 本文是 §5 单引擎 GPU simulator **重写**的实现设计与上下文锚，供后续 scrum 引用。
> 配套（读这些获取完整推理）：
> - `doc/seam-design.md` —— §5 目标架构蓝图（§3.6 原始之罪 / §4.5 filter 出口 / §5 统一 seam 形态）。
> - `doc/gpu-route-history.md` —— GPU 迁移 #250→268 全程回顾（§9 = 单引擎弧高潮）。
> - `doc/trace-backend-frame-lifecycle.md` —— Metal 帧生命周期 as-built（§8 = DR-3 波长）。
> - `doc/testing-architecture.md §4.2` —— parity metric-masks-bugs battery 方法论（cross-seed + 能量守恒 + golden 锚 + 人眼）。
> - `scratchpad/explore-gpu-single-engine/{SUMMARY,insights,experiments}.md` —— explore-266 de-risk 细节（gitignored，本文固化其 durable 结论）。

## 0. ⭐ As-Built 状态（scrum-267 + scrum-268 完成，2026-06-17）

> 本节为接手须知。两个 scrum 均已合 main；下方 §1-§7 保留设计推理原文。

> ⚠️ **吞吐数字 2026-06-19 受控重测后纠偏**（task-fix-throughput-bench-honesty）。
> 原表的 "CLI 9.5× / GUI 2.07× / 6× poller headroom" 三处均为测量假象，已替换为下方
> 实测 regime 表。纠偏详情与度量教训见本节末"度量纠偏"小节。

| 里程碑 | 数值 |
|--------|------|
| **CLI 引擎吞吐**（重场景，`--benchmark` setup-excluded，dispatch 32768，M2 Max，2026-06-19 复测） | `ms_multi_crystal_complex_filter` **8.1× legacy**；`ms_multi_crystal_filtered_bd` **10.1× legacy** |
| **GUI steady 吞吐**（真实 GUI regime，infinite + reconstruct 路径，dual_fisheye 512×256，M2 Max） | 重场景 **~9.5× legacy**（轻 1.8× / 中 2.2× / 最重棱锥 5.7×） |
| **GUI first_upload**（median，regime 相关） | 重场景 ~18ms / 中场景 ~71ms（均 < 150ms 冻结阈） |
| **引擎 vs GUI** | **同口径下 ≈ 1:1（无 headroom gap）**——引擎 8–10× ≈ GUI ~9.5×；旧"6× poller headroom"是假象 |
| **dispatch 甜点** | **32768**（backend-aware 默认；`LUMICE_DISPATCH_RAY_NUM`）。512/2048 仍饿死 GPU（0.2–0.8×），128 在大 ray_num 直接挂死 |
| **commit↔batch 解耦** | ✅ `LUMICE_DISPATCH_RAY_NUM` + `LUMICE_COMMIT_RAY_NUM` 双旋钮（concern #2 已解） |
| **parity matrix** | **10/10**（含 D65 illuminant + DR-3 波长） |
| **occupancy**（trace_layer_kernel PSO） | **640**（benign：R1 不触发，见 §6.1 最终裁决） |
| **PR** | #127（scrum-267）+ #129（scrum-268），均已合 main（2026-06-17） |
| **多 renderer 双 render 吞吐**（用户双 render 文档 fisheye_equidistant 1024² + dual_fisheye_equal_area 2048×1024，3 晶体 + 3 raypath filter，D65；drain-aligned，21 次交错采样取中位数，`test/performance/test_metal_multi_renderer_throughput.py`，Apple M 系列开发机，2026-09-16） | Metal 双 render **27.9M rays/s = 单 render 的 0.950 / 0.908**（闸 ≥ 0.85）；legacy CPU 同 config 6.7M ⇒ **4.2× legacy**（N 面累加之前该 config 整批回退，仅 1.06×）。一次 session 服务全部 `render[]`（≤ 4），exit tail 内逐 renderer 投影，无第二个 dispatch——`doc/seam-design.md` §4.2.1 |

**度量纠偏（task-fix-throughput-bench-honesty，2026-06-19）**——原 §0 三个数字为何错、现值为何可信：
- **"CLI 9.5× legacy"**：旧 `--benchmark` 把一次性 setup（server alloc + scene gen + 首 dispatch 延迟）与 100ms 轮询量化计入吞吐分母，对 0.2s 量级的快后端系统性低估（实测被压到 4.95×）。修复（计时起点改首次 `sim_ray_num>0` + 5ms 轮询）后重测 = 8.1×/10.1×。旧"9.5×"作为 ratio 巧合接近真值，但出处口径不可信。
- **"GUI 2.07× legacy"**：是 **task-272 修 complex-filter 导入前**测的——GUI 静默丢 filter → 跑无 culling 的重 workload → 被压低。修复后真实重场景 = ~9.5×。
- **"6× poller headroom（引擎 9.5× 在 GUI 仅兑现 2.07×，差 poller 20ms 整幅回读）"**：不存在。同口径下引擎 ≈ GUI（甚至 GUI steady 绝对值更高，因 `--benchmark` 旧口径低估引擎）。poller 整幅回读是**每次 commit 的延迟成本**（体现在 first_upload，全部 < 150ms），不是吞吐天花板（explore-271 E3 已证 poll 间隔不影响吞吐）。
- **错误 config 名**：原表 `ms3_multi_crystal_complex_filter` 不存在；真名为 `ms_multi_crystal_complex_filter`（无 `3`）。
- **度量教训**：吞吐对比必须 ① 排除 setup（rate = 稳态 active 窗口）；② 对齐 workload（filter culling 与否）与投影（GUI 强制 dual_fisheye_equal_area，见 `file_io.cpp` `SerializeCoreConfig`）；③ 只比同方法学内的 ratio-over-legacy，跨方法学相除无意义。原始数据：`scratchpad/task-fix-throughput-bench-honesty/data/`。

**已删除的遗留结构**（§5.1 reuse/discard 账本承诺的删法）：
- `CopyContSliceToRootBuf`（host-side 续传）→ 删，由 `transit_root_kernel` 取代。
- 12-worker queue-per-Simulator 编排 → 删，`server.cpp` 重构为单引擎。
- `LUMICE_BATCH_RAY_NUM` 双重身份 → 拆为 `LUMICE_DISPATCH_RAY_NUM` + `LUMICE_COMMIT_RAY_NUM`（BATCH 保留为 COMMIT 的 backward-compat fallback，已废弃）。
- ms_mode==1 "半续传"分支（写占位 out_p=centroid） → 重写为 device emit-gate。

**额外修复（correct-by-construction）**：
- +16% 多 MS filter 能量 bug（根因=host `CopyContSliceToRootBuf` 解耦 filter，随删除自愈）。
- server 构造函数潜伏 bug（legacy CPU 巨型未切块 consume 假象，268.6 白盒证伪后修）。

### 0.1 ⭐ CUDA backend as-built（全量多 CI + 诚实吞吐基线，2026-06-27，task-cuda-throughput-bench）

> CUDA 是 seam 的第二个真实消费者。scrum-295/296 落地单 CI MVP；本轮把它推到**与 legacy 完全对齐的全量多 CI**，并取到第一份**诚实**吞吐基线。设计蓝图细节见 `scratchpad/task-cuda-throughput-bench/MULTI_CI_DESIGN.md`（git-ignored），分支 `feat/cuda-multi-ci-correctness`。

- **多 CI（每层多晶体带 proportion）全量落地，对标 Metal 惰性-transit-per-CI 模型**（CUDA 此前是 single-CI MVP：BeginSession 只传 1 晶体、TraceLayer/Recombine/DrainExits 用 setting_[0]）。结构改动：
  - 几何 per-CI：`UploadCrystalGeometry`/`EnsureGeomCapacity`（grow-only），对标 Metal `UploadCrystal`。
  - cont 缓冲 ping-pong（`d_cont_*[2]`）；**transit 从 Recombine 移入 TraceLayer 惰性逐 CI**（读 cont[in_slot] 的 per-ci 切片过该 ci 晶体），Recombine 变薄（只 bump 层 + 返回 cont count）。
  - TraceLayer per-CI 循环：`PartitionCrystalRayNum` 连续切片 → 每 CI MakeCrystal + 几何上传 + gen(首层)/transit(续传) + trace，exit(crystal_id=ci tag)/cont[out_slot] atomic 累加。RNG flat-seed + 每 CI 推进 monotone gen/gate/transit counter（不相交 PCG 区间，单 CI 行为不变）。
  - final 层 DrainExits 按 `ExitRayRecord.crystal_id` 索引 per-CI FilterSpec/crystal；`EnsureContCapacity` 支持 3+ 层 cont/root grow（in_slot 不动，cont_cap_[2] 每槽）。
  - kernel emit-gate filter slot 修为 `ms_layer*max_ci + crystal_id`（原缺 crystal_id=单 CI MVP 残留）。
  - `ComputeExitCap/ComputeContCap` 去掉 64MiB silent-drop 硬顶，按解析上界 `n*(max_hits*2+4)` size（correctness > memory；OOM→优雅回退 legacy）。
- **验证（dev49/44 RTX 4060 Ti，全对 legacy）**：parity-cross-backend 11/11（单MS / ms_prob05 2层 / ms_multi_crystal 2层多CI ds_corr 0.913→0.9998 / **ms3_multi_crystal 3层多CI [4,3,2] 含 final** / filter / cross-seed 自洽）；e2e-correctness 15/15（CUDA 路由确认）；ctest golden-analytic 100%。**未修前 ms_multi_crystal 静默错（ds_corr 0.913，energy+cross-seed 都掩盖）——parity 漏因只测了 single-CI 的 ms_prob05。**
- **诚实吞吐基线（干净机 44-GPU，GPU 0%/CPU idle 99%）**：light_single_ms CUDA ≈ **0.10–0.12× legacy，flat（与 dispatch 无关）**；瓶颈 100% = **出射记录主机往返**（DrainExits D2H + 主机 filter + 下游 XYZ 累加，线性于 exit 数 ~54ns/exit），trace kernel 极小。
  - ⚠️ **推翻 scrum-296 Step D（49-GPU 旧单 CI 代码）"大 dispatch 1.6–2.2×"**——那是 exit 64MiB 硬顶丢弃 87% exit 的假象（drain 被封顶→虚高）。处理全部 exit 后 = ~0.1× flat。**"加大 dispatch 让 CUDA 赢"失效。**
  - 唯一有意义优化 = **device 侧 XYZ 累加**（不每 dispatch 把裸 records 过 PCIe+主机处理），非 dispatch/batch 调参。详见 `scratchpad/backlog.md` seam 条目（待单起 explore）。
  - 仪表瑕疵待修：多 CI 重写把 cudaEvent `ev_end_kernel_` 记录点挪到 per-CI 循环外，TraceLayer kernel 计时失真（≈0ms 假象）；profiling 前先修 event placement。

> ⚠️ **SUPERSEDED（2026-07-01，scrum-313 doc 审计）**——上面这份 "CUDA ≈ 0.10–0.12× legacy，flat，
> 唯一优化 = device 侧 XYZ 累加（待 explore）" 的 2026-06-27 基线**已被后续整条 arc 兑现+推翻**，仅作历史留存：
> - **device 侧 XYZ 累加已落地**（scrum-302 device-fused accumulation）——正是当时点名的"唯一有意义优化"，
>   消掉了裸 records 过 PCIe 的瓶颈。
> - **buffer-persist**（scrum-304.2，镜像 Metal Reset）消掉 per-batch alloc churn → 可比轻·单MS CUDA
>   **35–56M/s（6× legacy，≥25M 竞品线）**。
> - **dispatch 默认 262144 + exit-cap**（scrum-306.2）→ out-of-box **~114M/s**（= 134M intrinsic 的 85%）。
> - **第三时钟 readback 解耦**（scrum-312，seam-design §4.8）→ 真实 GUI 分辨率 2048×1024 下 CUDA 28→39M、
>   Metal 11→32M、1070Ti 12.5→33.5M。
>
> 当前 canonical 吞吐数字见 **`doc/performance-testing.md`「当前 canonical 吞吐结果」**（历史 per-run 详录
> 在 `scratchpad/perf-results-log.md`）。GPU 路线 #294→312 的完整演进见 `doc/gpu-route-history.md` Phase 10–11。
### 0.2 ⭐ 投影支持矩阵（scrum-315 投影全量对齐，2026-07-02）

> 此前 GPU 后端只在 device 内做 **2 种固定投影**（`rectangular` + `dual_fisheye_equal_area`）；其余镜头经 `IsCompatible` 返回 false → **静默回落 legacy CPU**（输出正确，只是吃不到 GPU 加速）。scrum-315 把 forward 投影统一进单一真源后，**全部 11 种投影都在三端渲染**。

`LensParam::LensType`（`src/config/render_config.hpp`）全集 = 11 种，现在**全部**在 legacy CPU / Metal / CUDA 上渲染，单源自 `src/core/shared/projection_shared.h::ProjectExitToPixel`（三端同源编译，parity 是结构性保证）：

| # | proj_type | 类型 | legacy CPU | Metal | CUDA |
|---|-----------|------|:---:|:---:|:---:|
| 0 | linear | 透视 | ✅ | ✅ | ✅ |
| 1 | fisheye_equal_area | 等积鱼眼 | ✅ | ✅ | ✅ |
| 2 | fisheye_equidistant | 等距鱼眼 | ✅ | ✅ | ✅ |
| 3 | fisheye_stereographic | 立体投影鱼眼 | ✅ | ✅ | ✅ |
| 4 | dual_fisheye_equal_area | 双等积鱼眼 | ✅ | ✅ | ✅ |
| 5 | dual_fisheye_equidistant | 双等距鱼眼 | ✅ | ✅ | ✅ |
| 6 | dual_fisheye_stereographic | 双立体鱼眼 | ✅ | ✅ | ✅ |
| 7 | rectangular | 等距柱状（全景） | ✅ | ✅ | ✅ |
| 8 | fisheye_orthographic | 正交鱼眼 | ✅ | ✅ | ✅ |
| 9 | dual_fisheye_orthographic | 双正交鱼眼 | ✅ | ✅ | ✅ |
| 10 | globe | 有限距离球面透视（Option B） | ✅ | ✅ | ✅ |

- **globe = 选项 B（真投影，非 orthographic 别名）**：有限距离球面透视，相机在 `(0,0,kGlobeCameraD)`，`kGlobeCameraD=4.0`，覆盖视轴 ±75.5° 而非满半球（与 orthographic 数学不同，仅 D→∞ 极限相等）。315.4 新增，legacy 此前也没有（仅 GUI 预览专用）。**core/shared 的 `kGlobeCameraD` 必须与 GUI `src/gui/gui_constants.hpp` 一致**（CLI↔GUI 观感一致契约；常数跨不过 C-API 边界，靠 shared 头 "must match" 注释锚 + golden-analytic round-trip 校验维持）。
- **GUI 显示重投影不在此矩阵内**：GUI 预览的 inverse 重采样（固定 dual-fisheye 全天图 → 任意视角显示）是 C-API 边界外的独立关注点，其镜头数学不进 `projection_shared.h`（`src/gui/` 不能 `#include` core 头 + GLSL 不能 include C++ 头）。渲染路径 = forward scatter；GUI 显示 = inverse 重采样，两者本质不同。
- **cross-backend parity 测试**：`test/parity-cross-backend/backend/test_{metal,cuda}_projection_parity.py`（共享 battery `test/e2e/_projection_battery.py`），每种投影一个 oracle=legacy 的对照，确认全部类型真走 GPU、不再 fallback。

## 1. 状态与目标（设计期原文）

> **接手注意**：本节描述 scrum-267/268 之前的出发状态（设计期），已是历史。
> as-built 见 §0。

- **当时**：Metal 是 GUI **opt-in**（`use_metal_backend` 复选框，默认关）；走 **12-worker + host-side MS 续传**（`metal_trace_backend.mm` kernel 写 continuation，host `CopyContSliceToRootBuf` 逐光线做 filter+prob+frame-transit）。这是 258-265 把已 de-risk 零件**焊进 legacy 结构**的产物，非 §5。
- **legacy CPU 始终是 GUI 默认实走路径 + 永久 ground truth**（perf 基线 + 正确性参照）。
- **目标**：实现 seam-design §5 —— 单引擎、三时钟解耦（几何采样 / trace 派发 / 图像回读各自频率）的 wavefront GPU simulator，替换 12-worker + host-side MS 编排。CPU 不重设计，留作可信 oracle。

## 2. explore-266 关键结论（固化）

1. **divergence 裁决 = 伪命题**。层内是**线性链**：每跳必有一支离开（收集进 buffer、不再参与本层）、一支留内继续（TIR 无人离开；weight 衰竭提前止）→ 一层出射 = **O(root × max_hits) 线性**（非递归树）→ 一线程一根光线寄存器内跑线性链 = megakernel 干净、divergence-free。跨 MS 层每条离开光线独立 prob 续传 → 几何增长（explore-257 amp 单层 4.81→两层 24.65）→ **wavefront across layers**。**§5 = 层内 megakernel + 层间 wavefront**。
2. **device 续传 filter 可行（实测 de-risk）**。raypath filter 唯一非平凡 per-ray 核 `detail::ReduceBuffer`（对称折叠）移植 MSL，对真实 host **1.44 亿检查 0 mismatch**、无寄存器压力、1.3-2.7G/s。**§4.5"对称折叠依赖晶体配置→filter 须留 consumer"是悲观误判**——它混淆了 orbit **构建**（config 依赖、host/每 session 一次）与 per-ray **匹配**（有界整数核、device 逐位可移植）。逐类型：None/Direction/Crystal=trivial；Raypath/EntryExit=ReduceBuffer+GetFn 表+memcmp+长度界；Complex=布尔组合——**全 device 可行**。
3. **吞吐（harvest 257/263/265，非重测）**。单个当前式 Metal 引擎仅 ~18K rays/s（< legacy 50K）；12-worker 把 host 串行 hop 并行到 12 核才达 225K（W8 饱和）。§5 单引擎要胜出须**双 lever 且都做满**：大 dispatch（摊薄 launch 延迟）+ device-resident 续传（消 host hop），两者正交 = 三时钟解耦。
4. **correctness gap（E5-E7，**已知、不单独修、由本重写顺带解决**）**。当前 Metal 多 MS + 读 recorder/对称的 filter 对 legacy **+16% 能量**；受控隔离矩阵定位到 host `CopyContSliceToRootBuf` 对续传光线的**解耦 filter 消费**（GetFn / orbit 构建 crystal-axis 的 hop_* slot 状态），非匹配逻辑（E4 逐位）、非单层、非多晶体、非 prob 机制。

## 3. 设计原则（硬约束）

- **legacy = ground truth；当前 Metal 代码至多作参考，不照搬**（owner 定调：可推翻重写，别过早陷入"修当前实现"的局部陷阱）。
  这条同时是 **GUI 工厂默认走 legacy CPU** 的第一条理由——两者是同一件事的两面。另外两条理由
  （用途分工：GPU 适合大光线量的高质量输出、CPU 适合拖滑杆式的快速调整；以及"不隐式翻转默认链路"
  的接口哲学）连同这条，一起落在 `src/gui/gui_state.hpp` 的 `use_gpu_backend` 字段注释里。
- **统计等价验收**（seam-design §3.6），度量用 **raw-XYZ parity**（`GetRawXyzResults` block-mean corr，避开 sRGB tonemap 失真；**2026-08-10 注**：该 getter 已于 v4.15 净删，今天的等价读法是 `LUMICE_AcquireResultFrame` + `LUMICE_FrameGetRawXyz`，度量本身不变），覆盖单/多 MS × 有/无 filter，对 legacy。
- **device 续传 gate 融进 kernel 逐跳 emit**（非"把 host hop 搬上 device"）：每 emit 一条离开光线（此刻 `path[]`=路径至今在寄存器）当场 device prob+filter → 续传写 continuation buffer / 输出写 exit buffer / fail 丢。三重收益：①逐位对齐 legacy `CollectData`（它就是逐跳对每条离开光线用"路径至今"判 filter，simulator.cpp:426）；②**天然修掉 §2.4 的 +16% bug**（其根因正是解耦的 host 重建；inline 用寄存器路径至今，单 MS exit 已证此 path 对 → correct-by-construction）；③E4 证融入零额外代价。
- **frame-transit 落点（C3，2026-06-14 精化）**：emit gate 只融 **prob + filter**——它们依赖"路径至今"，必须在离开那跳、寄存器里做。**frame-transit（重采下一层晶体取向 `InitRay_rot` + `ApplyInverse` + 重采入射点/面 `InitRay_p_fid`）不依赖路径、依赖\*下一层晶体几何\*，放到\*下一层 dispatch 的 kernel 入口\***（该 dispatch 已绑定自己那块晶体）。**为什么不塞进 emit gate**：多晶体场景（Scrum 1 验收 config 即 7 晶体）下，若 frame-transit 在 emit 那跳做，gate 就需要下一层全部晶体几何池 + per-ray 选择 = 把 concern #5 几何池提前拽进 Scrum 1，与"几何池留 Scrum 2"自相矛盾。切在 ingest 则：emit gate 保持 path-local、几何池干净留 Scrum 2、per-ray 下一层晶体路由变成层间 compaction 的一步（wavefront Recombine 本就该建模）。**此边界须在 Scrum 1 设计期钉死，否则实现中途会撞"多晶体 gate 做不了 device frame-transit"。**
- **几何单源**：晶体构造留 CPU（`MakeCrystal`），device 侧零几何代码（复杂稀少留 CPU/每 session，简单海量上 GPU/逐线程）。
- **架构为 CUDA 承接**：按离散内存语义设计，不焊统一内存假设（M2 上 host hop 廉价的结论不可外推 PCIe）。

## 4. 实现弧（2 scrum + 后续）

| 阶段 | 性格 | 交付 |
|------|------|------|
| **Scrum 1：device 续传引擎**（本文 §5） | 实现导向（设计已由 266 完成） | **正确性 + device-resident 续传机制**。交付物=raw-XYZ parity 对 legacy 恢复（+16% 消除）+ device 续传 gate 可独立切换运行。**不是吞吐**（见 C2）。 |
| **Scrum 2：单引擎编排**（本文 §6） | explore 先行（仍有设计不确定性） | 吞吐胜负手（大 dispatch + 单引擎 async + 释放 CPU 核）+ concern #2 解 + 几何池（concern #5） |

> **C1 clean-engine 形态（2026-06-14 owner 拍板）**：Scrum 1 **不是在 `metal_trace_backend.mm` 上原地做减法**，而是**新建一条干净的 §5 续传引擎路径**（device emit-gate kernel + device-resident 续传核），旧 Metal 路径降为**可切换的参考实现**，对 legacy 的 raw-XYZ parity 达标后**整体删除**旧 host-hop 路径。判定"是重写非补丁"的硬纪律：续传从 §5 emit-gate 原则重建、`CopyContSliceToRootBuf` 是\*删\*不是\*改\*、唯一验收门=对 legacy 的 raw-XYZ parity。缝契约 `trace_backend.hpp` 保留不动（它就是 §5 的 host/device 契约，见复用账本）。
>
> **C2 Scrum 1 验收是 correctness，不是吞吐（2026-06-14）**：266 E2/E3 已证——Lever B（device 续传）单独留在 12-worker 里，吞吐方向不明甚至可能退化（12 worker 原靠 12 核并行 host hop 拿到 225K；hop 移上 device 后 12 worker 争抢一块 GPU、host 核空出但 GPU 成唯一瓶颈）。吞吐胜负要"双 lever 且都做满"（Lever A 大 dispatch 在 Scrum 2）。故 **Scrum 1 验收 = raw-XYZ parity；吞吐至多做"无灾难性回退" sanity check，绝不设为 perf 门**。
| Phase 3：CUDA（dev49/RTX4060） | —— | 整个 GPU 迁移第二步，backlog 单独跟踪，**不在本 Metal 弧内** |

依赖：device gate 局部于 kernel + 续传 buffer，与"1 个还是 12 个引擎"正交 → Scrum 1 可独立先行；Scrum 2 在其上把编排塌成单引擎。

## 5. Scrum 1 范围：device 续传引擎

**做**：
- **emit gate**（融进 trace kernel 逐跳 emit）= device prob（PCG）+ device filter-match（ReduceBuffer 移植）。只融这两件——它们依赖"路径至今"（C3）。
- **frame-transit 在下一层 dispatch 的 kernel 入口**（device-root-gen 取向采样 + 入射点/面重采）——不在 emit gate（C3）。
- continuation 全程 device-resident（host 仅 counter + 重 dispatch），新引擎不含 host hop 的逐光线处理；旧 `CopyContSliceToRootBuf` 路径可切换保留，parity 达标后删（C1）。
- 多 MS filter 正确性 correct-by-construction（gate 内联 = legacy 语义）。
- **raw-XYZ parity 验收**（唯一硬门，C2）：单/多 MS × 有/无 filter（含 raypath PBD/BD + complex），对 legacy 恢复统计等价；用 owner 构造的 `ms3-multi-crystal-filter.json` 范式场景。建议按 E7 隔离梯子分级：先单晶体多 MS+filter（证 device gate 修掉续传 filter bug 的机制）→ 再多晶体（验 frame-transit 在 ingest 的多晶体路由）。
- golden-ray 解析锚（backlog:438）：给真 kernel 续传路径装确定性 golden 光线 + 解析期望，补"多反弹 trace loop / 面求交 / 续传"的 absolute 锚缺口（本轮 parity bug 暴露此盲区）。

### 5.1 复用 / 舍弃账本（钉死参考边界，防边写边被旧结构同化）

> 现有"设计"分两层：**缝契约**（`trace_backend.hpp`）= §5 蓝图本身，保留；**Metal 实现**（`metal_trace_backend.mm`）= 干净核 + 罪壳混合，罪壳重写。

**保留并复用（已 de-risk，誊抄进新引擎，非"原地留着"）：**
- **缝契约 `trace_backend.hpp`** — 不动。invariant 1-6（粗粒度融合 / 续传 device-resident 不透明 handle / 只 4B counter 过缝 / 出射世界系 / 离散内存语义）逐条=§5 的 host/device 契约；`TraceLayer`/`Recombine` 逐层模型=explore-266 把 §5 修正为"层间 wavefront"后的正确形态。
- trace kernel **内层 optics**（`mm:157-382`：`GetReflectRatio`/折射反射 TIR/面求交/`path[]` 记录）= #250 寄存器驻留核，逐字搬。
- exit-seam 出射记录写出（`mm:276-294`，ms_mode==0 分支）= 规范出口。
- `gen_root_kernel`（PCG device root-gen，scrum-260，`mm:688`）→ 复用为 frame-transit 取向采样。
- `exit_seam.hpp` schema / `ReadbackExitRays` / parity harness / `CpuTraceBackend` oracle / `ReduceBuffer` MSL spike（E4 `scratchpad/explore-gpu-single-engine/spike/`）。
- 机械管线（PSO / `Ensure*` buffer / `UploadCrystal`）= backend 脚手架非罪，**共享复用，不重写**。

**重写 / 删除（原始之罪，device 续传缺口）：**
- **`CopyContSliceToRootBuf`（`mm:1571-1737`）= 删除**，不修改。其 (A)frame-transit 迁到下一层 ingest、(B)filter/prob 迁到 emit gate。+16% bug 随之 correct-by-construction 消失。
- **trace kernel ms_mode==1 分支（`mm:217-261`）= 重写**：现写"半续传"（世界系 dir + 占位 `out_p=centroid` + 给 host 的 cont metadata），改为离开那跳用寄存器 `path[]` 当场 device prob+filter。
- **cont metadata 并行 buffer**（`cont_crystal_id`/`face_seq`）= 仅为喂 host hop 而存在 → device gate 上线后删。

**推迟到 Scrum 2（本 scrum 不碰）：** 12-worker→单引擎（缝之上 server.cpp，缝已支持）；per-batch 单晶体→几何池 per-ray 索引（§6 / concern #5）；commit↔batch 解耦（concern #2）；exit 分支 inline image 投影残留（image-seam，归 P3/CUDA）。

**验收**：raw-XYZ parity 对 legacy 在 filtered + multi-MS 场景恢复（当前 +16% 消除）；统计等价（§3.6）。

## 6. Scrum 2 范围：单引擎编排（explore 先行）

- explore 设计：单大 dispatch + async 引擎替 12-worker（12-worker 本质是延迟隐藏 hack，explore-263 证 W8 饱和）；**commit↔batch 解耦**（concern #2，backlog:565/571——`LUMICE_BATCH_RAY_NUM` 当前一身二职=GPU dispatch 粒度 + SimData 到达粒度，须解耦使 GUI 用大 GPU batch + 细 commit 响应）；GPU 大 dispatch 内部如何增量 drain/commit 给 consumer；CPU 核预算。
- **几何池 / concern #5**（晶体几何参数随机变动 → batch 内多晶体几何、per-ray 索引）在此评估或延后（§6 待定项，257 exp#5 证 memory divergence +20%@K1024 modest，control divergence 未测）。
- 再由 explore 结论动态生成实现子任务。

### 6.1 Scrum 1 结转的 Scrum 2 输入（267.2 fused-emit-gate 产出，2026-06-14）

- **⭐ R1 option B 最终裁决（scrum-268.6 实测，2026-06-17）**：267.2 的 emit gate 把 `trace_layer_kernel` PSO 的 `maxTotalThreadsPerThreadgroup` 从 1024 压到 704（再到 640，DR-3 波长后）。**Scrum 2 实测结果：R1 不触发（benign）。** 理由：重场景（`ms_multi_crystal_complex_filter` / `ms_multi_crystal_filtered_bd`）Metal 单引擎实测 CLI 吞吐 **8–10× legacy**（2026-06-19 复测，见 §0），GUI steady **~9.5× legacy**——occupancy 640 对吞吐无恶化，无需触发 option B（拆 filter-gate 为独立 wavefront dispatch）。occupancy 回归门固定在 640（当前实测基准），由 `TraceLayerKernelMaxThreadsForTest()` 守卫。**R1 option B 归 backlog（CUDA phase-2 开篇时重估，离散显存场景可能更敏感）。**
- **gate cleanup（Scrum 2 顺带）**：①`DeviceFilterCheck` 第 7 形参在 `kFilterMatchHelperSrc` 定义层仍名 `crystal_id`，正确实参是 `gate_slot`（= `ms_layer_idx*max_ci+crystal_id`）——建议改名 `orbit_slot` 使 API 自描述（当前靠调用点注释保护，不可扩展）；②`gate_seed` 在 `(ms_layer_idx=0,crystal_id=0)` 退化为 `gen_seed_`、与 gen_root PCG 种子重叠（corner case，M5 parity 未受影响）——加非零 XOR 偏置消除。

## 6.2 反漂移纪律（D1-D4，贯穿 §5 弧的实施纪律）

> 背景：seam-design 蓝图很早就讨论清楚，但 #250→265 多次翻车不是方向问题，而是**实施时贪图最小努力、被现有代码同化、走小步偏移**（§3.6 原始之罪的复发）。Scrum 1 没漂移，正因为有 **C1 硬规则**（删非改 / 干净引擎 / 唯一验收门）。把同款纪律一般化、前置固化为后续每个 scrum 的实施约束。依据 a04（系统完整性先于局部 / 举证责任在增加方 / 减法优先 / 过程信号是架构警报）+ a05（软约束必失效→固化为门禁）。

- **D1 验收门 pre-register**：每个 scrum 的数值验收门在 implementation 之前钉死进 `scrum.md`，门的测试 harness 作为头几个子任务先建。**禁止事后放宽门来迁就实现**——这正是 scrum-267 续传欠采样 bug 被"放宽 corr 阈值"掩盖的复发模式（[[feedback_gpu_parity_corr_masks_undersampling]]）。
- **D2 删非包 + 前置 reuse/discard 账本**：design explore 收敛时产出"复用/删除账本"（仿 §5.1），**点名要删除的 legacy 结构**，禁止"在旧结构外面再包一层适配器"（包不是删）。
- **D3 plan-review / code-review 必答三问（不答即 block）**：每个实现子任务的 plan 和 diff 显式回答——①推进了哪条 §5 不变量？②**删除**了什么（不只是加了什么）？③是否依赖 legacy 结构（12-worker / host-hop）？若是——是带 delete-ticket 的临时脚手架，还是漂移？挂在 scrum-drive 已强制的 review 门上。
- **D4 过程信号当架构警报（tripwire）**：**若某实现子任务的 diff 在 legacy 结构之上净增、且没删任何东西 → 停。** 净加 = 又在镜像 CPU 结构。这是停下来回看大目标的硬信号，不是继续走小步的理由。

> 各 scrum 的具体验收门数值是 scrum-specific，落在该 scrum 的 `scratchpad/scrum-*/scrum.md`，不进本 doc。

## 7. 验证策略（贯穿两 scrum）

- **ground truth = legacy CPU 渲染**（raw-XYZ 优先于 sRGB）。oracle 另可用 CpuTraceBackend 独立重实现（#253 范式）抓 kernel 数值/时序错。
- parity 全程统计级（累加图像 / raw-XYZ block-mean corr），非逐光线 identity（mt19937 流不可对齐，PCG 同分布）。
- perf 基线 = legacy CPU（GUI 实走路径），勿用 `LUMICE_TRACE_BACKEND=cpu_backend`（慢 2.5× 辅助产物）。
- **⚠️ parity metric-masks-bugs battery**：corr 单指标两次放过真 bug（267.3 欠采样 + 268.8 平坦谱静默）。所有 parity gate 必须组合：**cross-seed 自洽 + 能量守恒 + golden 绝对锚 + 人眼核查 + revert 反验**。详见 `doc/testing-architecture.md §4.2`（权威规范，不在此处重复）。

## 8. DR-3：per-ray 波长决策链（scrum-268.3/268.8）

> 波长设计经历了三轮决策（DR-1→DR-2→DR-3），最终落点与原计划不同。

**DR-1（原计划）**：per-threadgroup 波长——所有位于同一 threadgroup 的光线共享同一折射波长（MSL uniform + tg 内协作读）。

**DR-1 被推翻（owner 白盒 + runner 自阻塞双向发现）**：
- 多 MS 续传原子压缩光线数——光子的 `tg_id` 在层间变化（tg 内存活数不固定，续传打包重排），per-tg 波长使光子中途换折射率 → 路径计算错误，非方差问题。
- runner 自阻塞：尝试把 per-batch `wl` 穿过 exit seam 发现 per-batch wl 无法跟着续传存活到下一层。
- 两端独立发现合流，owner 拍板 DR-3。

**DR-2(B)（过渡）**：host 预采波长池 + 上传，device 仅读 index（零 device Sellmeier，满足 §3.7 合规）。

**DR-3（最终，as-built）**：波长是**每个光子的终生属性**（`wl_idx`，从 root-gen 到最终 consumer 贯穿全路径）：
- `gen_root_kernel` 按 `global_idx % M` 分配 `wl_idx`（均匀覆盖 WlPool）。
- trace kernel 按 `wl_idx` 从 WlPool 读折射率（寄存器驻留，零全局访问额外税）。
- emit gate 把 `wl_idx` 写进 `cont_wl_idx_out`。
- `transit_root_kernel` pass-through `cont_wl_idx_in → root_wl_idx_out`（见 `doc/trace-backend-frame-lifecycle.md §4.3/§8`）。
- consumer 用 per-ray CMF 权重积分 XYZ。

**静默失效抓捕**（硬门案例，see §2.4 正确性）：Step 9 gate 误放在 `use_backend` 分支，使 cpu_backend 路径 `curr_wl_` 保持 0（无池），产全黑图像——触发反静默回退硬门（`!per_ray_wl && !outgoing_d_.empty() && curr_wl_ < 1.0f → assert`）当场抓住。没有硬门，bug 静默产平坦谱（plausible，不崩）继续藏。

## 9. 单引擎 as-built：关键发现与弯路

> 接手：这些是 scrum-268 最值得学的教训，避免重做。

1. **"consumer-bound"误判**（268.6）：初测 GUI steady 81K vs legacy 729K → 误判 consumer 是瓶颈。实为：server bug（legacy 巨型未切块 consume）+ GUI 预算稀释。白盒证：benchmark consumer ~1.3µs/batch，健康 ~1.0µs/batch——GPU **非** consumer-bound，差距来源=poller 每 20ms 整幅图回读 + GPU 上传（非 consumer）。

2. **G2 子进程隔离 BLOCKER**（268.2）：in-process static cache 使 batch 变化无效，batch 不变性测试结论全错——改子进程隔离后才得到正确的"batch 有小幅真实依赖（cross corr 0.987-0.997 vs self-noise 0.9998，几何采样粒度效应）"。

3. **独立验证抓 runner 漏报回归**（268.7）：runner 未验证对 CLI legacy 单引擎的影响，owner 亲手发现 legacy 慢 6×（1-worker 编排=设计预期代价，非 regression）。

4. **~~吞吐天花板：poller 20ms 整幅回读~~（2026-06-19 推翻，见 §0 度量纠偏）**：原结论"引擎 9.5× 在 GUI 仅兑现 2.07×、差 poller"是测量假象——同口径下引擎（8–10×）≈ GUI（~9.5×），无 headroom gap。poller 整幅回读是 **per-commit 延迟成本**（first_upload < 150ms），不是吞吐天花板（explore-271 E3 实证 poll 间隔不影响吞吐）。partial-readback / async upload 若做，目标是降**交互延迟**（first_upload），非提吞吐。

## 10. XYZ 设备平面的 drain-window 累加精度（as-built，2026-09-19）

> 接手：CUDA 的像素平面在一个 drain 窗口内是一条 fp32 `atomicAdd` 长链；它的偏差曾被 legacy 侧同形的
> fp32 偏差抵消，legacy 改 double（PR #380）后单独显形。本节记录缺陷机制、四个候选修法的一手红绿数字、
> 选定形态与仍未闭合的吞吐代价。改 `AccumXyzToPixel` / `d_xyz_buf_` / `ReadbackXyzAccum` /
> `LUMICE_XYZ_DRAIN_BATCHES` 语义前先读。

### 10.1 缺陷机制

第三时钟（§0 / `Simulator::kDefaultXyzDrainBatches = 64`）让设备侧 W×H×3 像素平面跨整个 drain 窗口
持续存活，`EmitToDeviceXyz` 对它逐出射 `atomicAdd`。全天球场景每像素每窗口只有 ~16 次加法，无事；窄视场
热像素场景一个像素一个窗口能吃下上万次，fp32 和的 ulp 随和增长，每次加法舍入 ≤ 半 ulp，累计成**随窗口长度
走的舍入漂移**（符号会翻转，不是单向丢失）。这是 `d_landed_weight_` 那次「会话级单标量 −1.7%」缺陷
（`EmitToDeviceXyz` 上方注释）在像素级的同族——标量那次靠 per-warp 寄存器 + 每层折回 host double 修掉，
平面因体积是 W×H×3 不能照搬每层 D2H。

一手签名（home-wsl，base `6f38f1a7`，`test_cuda_energy_accounting_parity.py` 的
`R = Ysum / snapshot_intensity` 两账本比值，`parhelion` 10M rays，seed 42）：

| `LUMICE_XYZ_DRAIN_BATCHES` | 64（默认） | 16 | 1 |
|---|---|---|---|
| `R_cuda/R_legacy − 1` | **+0.4000%** | −0.1352% | −0.0078% |

parity battery 25 行里 4 行红（parhelion 三 seed +0.40%；`multi_lens` 三 renderer +0.52/+0.50/+0.32%），容差
0.1% 不放宽。Metal 同形路径（`lumice_trace.metal` 同一 `accum_shared.h`）**不受影响**：Metal 每 batch drain，
fp32 链最长一个 batch；且已有跨厂商证据——结构相同的单地址 GPU 原子归约，CUDA 与 Metal 的精度代价可差 50 倍
（标量那次 CUDA +1.74% vs Metal +0.035%；Metal 平面实测残差 ±0.02% 不随 N 增长），本仓不重新逆向 Apple 硬件。

### 10.2 四个候选的一手红绿矩阵

协议：`examples/bench_config.json`（单 renderer 2048×1024 rectangular 180°）改 `ray_num` 3e9，
`Lumice benchmark --backend cuda`，同一台机上 base 与候选二进制**交错 5 次**，全部 `rate_basis=steady`；
正确性 = 上表三档 + 全量 energy-ledger / multi-renderer 两个 parity 文件。home-wsl（RTX 5090 D，WSL2）。

| 候选 | 形态 | 正确性（parhelion 三档） | 吞吐 vs base | 判定 |
|---|---|---|---|---|
| A | `LUMICE_XYZ_DRAIN_BATCHES=1`（零代码，每 batch 同步 D2H 折回 host double） | −0.0078% 三档一致 | **−88%**（413 → 49.5 M/s） | 否决：正是第三时钟消掉的税 |
| C | 每 batch 一个 device kernel 把 fp32 平面折进 double 平面并清零 fp32 | −0.0078% 三档一致（fp32 链仍有一个 batch 长） | **−22.5%**（405 → 314） | 否决：每 batch +188 µs，全平面遍历串在 stream 关键路径上（下一 batch 的同步 H2D 要等它），外加 WSL2 每个 GPU 包 ~30–45 µs |
| **B** | `AccumXyzToPixel` 直接 `atomicAdd(double*)`（sm_60+ 原生），drain 时 device 转 fp32 staging 再 D2H | **1.000000** 全部行，三档 spread 0.0000% | **−13.9%**（409 → 352；home-win 原生 −17.8%，472 → 388）；1024×512 +5%、512×256 −3%（均在噪声内） | **选定**；代价是 2× 平面字节的占用效应（缩小平面即消失），不是 64-bit 原子本身 |
| D | fp32 主路 + 比值门控迁移：`old = atomicAdd(fp32)`，`old ≥ 128·w` 时 `atomicExch` 搬进 double 平面 | 1.000000，三档 spread 0.0000% | **−24.0%**（409 → 311） | 否决：每次命中要等一次带返回值的原子（ATOM 而非 RED），比 B 的占用效应还贵 |

C 与 D 的代码留在分支历史里只作证据（C：`c6eb47aa`/`9ff5d1dc`，D：`a00caa9f` + revert），树上只有 B。

### 10.3 选定形态（B）as-built

- `src/core/shared/accum_shared.h` CUDA 变体：`AccumXyzToPixel(double* xyz_buf, …)`，乘积仍是 float
  乘积（与 Metal / host 同舍入），只有运行和是 double。Metal / host 变体不动。
- `CudaTraceBackend::Impl`：`d_xyz_acc_`（double，原子目标）与 `d_xyz_buf_`（fp32，只做 D2H staging，
  kernel 从不写）成对；`EnsureXyzBuf` 同分支分配、同 shape 变化清零；`BeginSession` 不碰。
- `ReadbackXyzAccum`：先 `xyz_plane_to_float_kernel`（session stream 上，`acc → out` 转 float 并清零
  `acc`），再 `cudaDeviceSynchronize`，再逐 renderer D2H `d_xyz_buf_`——**D2H 字节数、host 侧
  `XyzImageData` / `SimData::xyz_pixel_data_` 类型一字不改**。转换 kernel 每 drain 一次（1e9 rays 约 60 次），
  不是每 batch。
- 显存：2048×1024 单 renderer 多 50 MB（double 平面）；`multi_lens` 四 renderer 各 1024×512 多 48 MB。
- 哨兵：`test_cuda_energy_accounting_parity.py::test_cuda_energy_ledger_independent_of_drain_window`——
  parhelion seed 42 三档 `R_cuda/R_legacy` 互差 ≤ 0.05%；换回 base 的 `liblumice_testapi.so` 读 spread 0.5353%
  红。⚠️ 与 battery 其余行一样，只在 CUDA 参照机上跑（CI 无 GPU）。

### 10.4 仍未闭合：吞吐代价（上抛 owner）

B 在生产口径（2048×1024）上 −13.9%（home-wsl，409 → 352 M/s）、**−17.8%**（home-win 原生，同协议同日
5 交错，472.5 → 388.4 M/s，两臂 CoV ≤ 2.7%）。四候选无一满足
「≤2%」，且失败机制各异（A：per-batch 同步 D2H；C：per-batch 全平面遍历 + GPU 包税；D：热路径等原子返回；
B：2× 平面占用）。没有第五个形态能同时做到「per-batch 零动作、热路径零等待、平面字节不翻倍」——三者正是
A/C、D、B 各自的代价源。剩下的选项由 owner 裁定：接受 B 的代价换精确的两账本；或退回 fp32 平面、把 battery
那 4 行判为已知偏差（⛔ 不是放宽 `_T_R_RATIO_TOL`，而是显式记录一个 +0.4% 的窗口长度依赖）。

同缺陷族、本次**未修**的已知风险：`d_anchor_buf_`（曝光锚点平面）与 `d_class_lane_buf_`（per-class Y-lane
平面）与 `d_xyz_buf_` 同样是跨整个 drain 窗口的逐出射 fp32 `atomicAdd`（`EmitToDeviceXyz` 里的
`AccumAnchorY` 与 `FanColorClassLanes`）。今天没有任何一手红态数据（没有测试读这两块平面的两账本），
按「举证责任在增加一方」不在本次交付内改；若将来证实可观测，修法就是本节 B 的同形——把那块平面的类型与
原子拓宽，drain 侧加同样的转换 kernel——不需要新的机制。



> ⚠️ 本节在 `main` 基线上补写，与分支 `fix/cuda-drain-window-fp32-plane`（PR #383）自己的 §10.1–§10.4
> （单场景 `examples/bench_config.json` 2048×1024、候选 A/C/B/D 的红绿矩阵）是**同一缺陷调查的两批
> 数据**，尚未合并去重——该分支合并时，此处内容应与其 §10.2/§10.4 合并为一节，而不是并列保留两份
> "候选对比表"。本节只补两件那次没测的事：① 候选 B 在 4 个 canonical 场景 + 完整分辨率扫描下的代价
> （那次只测了 2048×1024 一个分辨率点）；② 新增候选 **E**（`FoldDeviceXyzBatch` 每 8 batch 折一次，
> 而非候选 C 的每 batch 一次）的正确性与吞吐。E 与 A/B/C/D 一样是**探针**，树上不落地，代码只在
> 别处（生成 patch 的机制过程）保留，本节只留数字与结论。

### 10.5 候选 B/E 相对 `main`（无修复）的吞吐代价，按场景/分辨率

协议：`scripts/bench_throughput.py`，default dispatch，N≥5 交错（CoV>15% 按脚本自身判据升到
N=9）。`main` = `origin/main@6f38f1a7`（未修复的 fp32 平面，无 fold）；候选 B =
`origin/fix/cuda-drain-window-fp32-plane@2cdc353e`；候选 E = 在 `main` 上 cherry-pick 候选 C 的两个
提交（`c6eb47aa` `9ff5d1dc`）后把 fold 频率从"每 batch"改成"每 8 batch"（drain 前再补一次 fold，
避免尾部残留漏折）。

**4 场景默认分辨率矩阵**（`bench_light_single_ms`/`ms_multi_crystal` 的默认分辨率均为 512×256），
`cuda multi_median_rps`，vs `main` 的百分比；括号内为该格 CoV，均在 3–17% 区间，多数差值在噪声内：

| 场景 | home-wsl main | home-wsl B (Δ) | home-wsl E (Δ) |
|---|---|---|---|
| `bench_light_single_ms` | 251.6 M/s | 278.5 M/s (+10.7%, CoV 3–11%) | 301.7 M/s (+19.9%, CoV 8–11%) |
| `ms_multi_crystal` | 131.0 M/s | 126.9 M/s (−3.1%, CoV 3–7%) | 126.8 M/s (−3.2%, CoV 1–4%) |
| `ms_multi_crystal_complex_filter` | 228.7 M/s | 226.6 M/s (−0.9%, CoV 8–17%) | 248.0 M/s (+8.4%, CoV 10–11%) |
| `ms_multi_crystal_filtered_bd` | 207.5 M/s | 235.8 M/s (+13.7%, CoV 9–14%) | 221.5 M/s (+6.8%, CoV 10–13%) |

在 512×256 这个（较小的）默认分辨率上，B/E 相对 `main` 的差值全部落在测量噪声量级内——与 PR #383
§10.2 "512×256 −3%（噪声内）" 的结论一致。**分辨率变大之后代价才显形**（XYZ 平面字节数
`W×H×3×4`，512×256 仅 1.5 MB，2048×1024 是 24 MB）：

| 场景 @ 2048×1024 | home-wsl main | home-wsl B (Δ) | home-wsl E (Δ) | home-win main | home-win B (Δ) | home-win E (Δ) |
|---|---|---|---|---|---|---|
| `bench_light_single_ms` | 250.7 M/s | 216.4 M/s (**−13.7%**) | 223.2 M/s (**−11.0%**) | 347.6 M/s | 287.5 M/s (**−17.3%**) | 327.2 M/s (**−5.9%**) |
| `ms_multi_crystal` | 115.6 M/s | 103.4 M/s (**−10.5%**) | 103.6 M/s (**−10.4%**) | 153.8 M/s | 157.9 M/s (+2.7%) | 160.5 M/s (+4.3%) |

`bench_light_single_ms`（单晶体、无 filter，出射密度最高——每像素每 drain 窗口的 `atomicAdd` 次数最多）
在两台机器上都读到清楚的代价，且方向一致：**E 比 B 便宜**——home-wsl 省 2.7pp（−11.0% vs −13.7%），
home-win 省 11.4pp（−5.9% vs −17.3%）更明显。`ms_multi_crystal`（多晶体、per-ray 计算重、出射密度低）
在 home-wsl 上读到与 B 相近的代价，但在 home-win 上 B/E 反而比 `main` **快**——两台机器方向不一致，
按实测记录，不强行统一解释；候选的吞吐代价看起来主要由"每像素出射密度"而非"晶体数/场景复杂度"驱动，
这与 §10.1（缺陷机制：热像素场景一个像素一个窗口能吃下上万次 `atomicAdd`）的机制描述方向一致，但本次
测量样本量（每格 5–9 次交错）不足以把"E 比 B 更便宜"钉成跨场景通用结论——只在 `bench_light_single_ms`
这一光路密集场景上观察到。

### 10.6 候选 E 的正确性：drift 剂量-响应补上"8"这一档

PR #383 §10.1 的一手签名表（`parhelion` 10M rays，seed 42，未修复的 fp32 平面）给出
`LUMICE_XYZ_DRAIN_BATCHES` ∈ {64, 16, 1} 对应的 `R_cuda/R_legacy − 1` 为 **+0.400% / −0.135% /
−0.008%**——drift 随窗口长度走，因为窗口越长、fp32 `atomicAdd` 链越长。候选 E 把 fold 频率钉在
"每 8 batch"，所以无论 `LUMICE_XYZ_DRAIN_BATCHES` 设多大，fp32 链长都被**上限在 8**（drain 前补一次
fold 处理不足 8 的尾段）。用同一 `test_cuda_energy_accounting_parity.py::_r_ratio`（
`sum(flt_buf Y) / snapshot_intensity`）在候选 E 二进制上跑 `parhelion`，seed 42/43/44 ×
`LUMICE_XYZ_DRAIN_BATCHES` ∈ {64, 16, 4}（home-wsl，一手测量）：

| seed | drain=64 | drain=16 | drain=4 |
|---|---|---|---|
| 42 | +0.0452% | +0.0452% | −0.0390% |
| 43 | +0.0452% | +0.0452% | −0.0390% |
| 44 | +0.0449% | +0.0449% | −0.0391% |

三档在 64/16 之间完全一致（因为两者的 fp32 链长都被 8-batch fold 钉住，drain 窗口本身多长不再重要），
在 drain=4 时降到 −0.039%（此时 `xyz_win_.calls` 从未达到 8，drain 前的补 fold 把链长压到 4，与
`main` 表里 drain=4 一档"未实测"留白呼应）。**三档 |drift| 全部 ≤ 0.05%**，比 `_T_R_RATIO_TOL`
（0.1%，两后端互比的容差）还紧一半，也确认候选 E 没有把 B 已经解决的"精度依赖窗口长度"问题带回来
到会破坏跨后端 parity 的程度——但注意这不等于"drift 与窗口无关"（B 的性质）：跑
`test_cuda_energy_ledger_independent_of_drain_window`（该测试针对候选 B 的
`{1, 16, 64}` 三档、容差 0.05% 编写）在候选 E 上**预期性地红**——drain=1 读 −0.0078%（链长 1，
与 PR #383 表格 drain=1 一档吻合，因为 E 在 drain=1 时每次都补 fold，退化成候选 C 的每 batch 折）而
drain=16/64 读 +0.0452%（链长 8），spread 0.0530% 略超该测试为 B 钉的 0.05% 门槛——这是候选 E "有意
把链长钉在 8 而非 1"的直接体现，不是候选 E 实现的缺陷，只是它不满足一条**为候选 B 的形态量身写的**
不变量。

### 10.7 结论边界

本节只补数据，不裁定 575 的最终形态（候选取舍仍由 owner 在 PR #383 上下判断）；`bench_light_single_ms`
上 E 比 B 便宜、`ms_multi_crystal` 上两机方向不一致、E 的三档 drift 全部 ≤0.05% 但不满足 B 的
"drift 与窗口长度无关"这条更强不变量——这三点是本节交付的全部事实，取舍留给 owner。
