# GPU 路线系统回顾（#250 → #312 + Phase 13/14，截至 2026-09-19）

> §一~§四 = Metal 单引擎弧（#250→268，原始回顾，截至 2026-06-13）；§五 Phase 10 = CUDA 第二步 + device-fused（#294→302）；§六 Phase 11 = CUDA 吞吐收口 + Windows 交付 + 第三时钟（#303→312）；§七 Phase 12 = 投影全量对齐；§八 Phase 13 = 多 renderer N 面累加；§九 Phase 14 = CUDA 空闲间隙的机制层否定结果（异步化第三次被拒，这次带机制）。

> 目的：把 GPU 迁移一路的决策演进、已积累数据、遗留项盘点清楚，作为重新深想的全局参照系。
> 触发：owner 反思"行动前想得不够深→地基不稳、结论来回摇摆、积累不成形"。

---

## 一、大方向决策演进（每步的决策 + 它修正了什么）

**Phase 0 — 为什么要 GPU（赌注前提）**
- beam tracing 复盘（03-29）：方差瓶颈=SO(3) 姿态采样 O(1/√N)，算法层无结构改进空间 → 提质唯一路径=加算力。
- #246 cpu-soa-refactor：量化内存墙（#248）——RaySeg 膨胀致 -29% 退化，SoA option D 落地（168→96B）。坐实"高并发 CPU 内存受限"。

**Phase 1 — GPU 赌注 de-risk（#250）**
- #250 spike：**GO**。M2 Max ~250× 全 sim、**计算受限**（28<<400 GB/s）、寄存器驻留绕开内存墙、**divergence 证伪**（无需 stream compaction）、原子累加仅 +7%。
- **06-04 关键 reframe**：把"backend 选型死结"框架松开——dev49 是 NVIDIA 不是 AMD。真实矩阵：M2(开发)=Metal、**dev49(bench)+Windows(用户)=CUDA**。覆盖三台只需 Metal+CUDA+CPU。**定两步走：Step1 Metal/M2，Step2 CUDA/dev49。**

**Phase 2 — 架构：缝（#251）**
- 决策：**薄 `TraceBackend` 缝 + N 份原生 kernel**（拒可移植 kernel 语言——会藏住寄存器驻留这个 250× 命根、又省不掉每平台 SDK/CI）。
- 设计铁律：缝粗到整条融合管线 / host 指针不穿缝（device-resident 不透明 handle，**从第 1 天按离散内存语义**）/ 层间重组=backend-local / C API 不变。
- de-risk：recorder 寄存器压力 ≤16%（唯一真风险，证伪）。

**Phase 3 — 正确性弯路（#252 取消 → #253）**
- #252 集成路线取消——根因=缝帧生命周期沉默（Metal 漏出射 `crystal_rot_.Apply` → 环塌成带）。**帧是错的。**
- #253 frame-correctness：修单/多 MS 帧、重做 parity harness（CpuTraceBackend 独立 oracle，弃 kernel-mirror 共享盲区）、1.35× 亮度裁为伪象、owner 人眼把关。corr 0.99/0.997。**任何 perf 工作前必须先有可信正确性。**

**Phase 4 — Metal 进 GUI（#254-255）**
- #254 Metal kernel + C API 开关 + GUI 复选框 + parity 0.9963。#255 日志统一。

**Phase 5 — GUI perf 转向：缝重设计（#256-259）**
- #256 metal-gui-perf：**关键发现**——GUI 慢根因=backend-seam 整幅回读税（每 batch O(W×H)），**非 Metal/GPU**。2048×1024 下 Metal 比 legacy 慢 9-57×；合批反超。当时判"GUI 是 GPU 的错误战场"。
- #257 seam 重设计：缝切在**出射光线**（buffer-egress 非 image），带宽省 266-1362×。路线图 P1 出口侧 / P2 入口侧 root-gen / P3 bulk device-fused。
- #258 exit-seam scrum：P1 转正为规范出口替代 image-seam。filter parity 真根因=BeginSession 每 SimBatch 重置 RNG（白盒命中）。
- #259：吞吐未回退。

**Phase 6 — 入口侧 GPU root-gen（#260-261）**
- #260 rootgen scrum：device root-gen 转正默认供给，单 worker 1.87×/多 ~2×，parity ≥0.99。
- #261：bench 确证 **b128 单晶体净亏 / 多晶体净赚**。← 第一个诚实的"非一致赢"信号。

**Phase 7 — Profile & 融合（#263-264）**
- #263 integrated-profile：摩擦=**GPU 命令缓冲往返延迟**（wait 89%）非计算。融合（跳 gen wait）2.41×。async 不值。多 worker=掩盖延迟非并行（单 GPU）。**gen regime 2D（大 batch device-gen 18.5M=2× 天花板）。concern #2 在此结晶。**
- #264 fusion：融合默认化。单晶体 b128 **2.40×（分母=融合前 Metal，非 legacy）**。

**Phase 8 — GUI 可行性现实核对（#265，本会话）**
- 裁决(A)：Metal 能胜 legacy 但**无全局单 batch**——轻场景要大/重场景要小（concern #2 跨 regime 坐实）。默认 b128 饿死 GPU。Metal 对重场景拖动痛点有效（4.5× 帧）。

**Phase 9 — §5 单引擎重写弧高潮（#266-268，2026-06-14/17）**
- **explore-266 de-risk**（2026-06-14）：三关键结论固化——①divergence=伪命题（层内线性链 megakernel，层间 wavefront）；②device 续传 filter 可行（E4：`ReduceBuffer` MSL，1.44M 0 mismatch，seam-design §4.5 悲观误判推翻）；③吞吐须双 lever（大 dispatch + device-resident 续传，正交）。定位 current Metal 多 MS+filter 对 legacy **+16% 能量 bug**（根因=host `CopyContSliceToRootBuf` 解耦 filter）。
- **scrum-267（Scrum 1，device 续传引擎）**：删 `CopyContSliceToRootBuf`，新建 `transit_root_kernel` device 路径；emit gate 内联 device filter-match（DeviceFilterCheck）；raw-XYZ parity 8/8 GREEN（+16% 自愈 correct-by-construction）；occupancy 704→640（DR-3 后，benign）。PR #127，合 main 2026-06-14。
- **scrum-268（Scrum 2，单引擎编排）**：核心结果（吞吐数字 2026-06-19 受控重测纠偏，见下）：
  - **CLI 引擎吞吐**（`--benchmark` setup-excluded，dispatch 32768，M2 Max，2026-06-19 复测）：`ms_multi_crystal_complex_filter` **8.1× legacy** / `ms_multi_crystal_filtered_bd` **10.1× legacy**。
  - **GUI steady 吞吐**（infinite + reconstruct，dual_fisheye）：重场景 **~9.5× legacy**（轻 1.8× / 中 2.2× / 最重 5.7×）。
  - **DR-3 波长 per-ray**：推翻 DR-1 per-tg（multi-MS 续传原子压缩使 tg_id 跨层失稳）→ host 预采 WlPool，光子终生携带 `wl_idx`（见 `doc/trace-backend-frame-lifecycle.md §8`）。
  - **concern #2 已解**：`LUMICE_BATCH_RAY_NUM` 拆为 `LUMICE_DISPATCH_RAY_NUM`（GPU dispatch 粒度）+ `LUMICE_COMMIT_RAY_NUM`（GUI commit 粒度）双旋钮（268.4）。
  - **+16% bug correct-by-construction 修复**：随 `CopyContSliceToRootBuf` 删除自愈，parity matrix 10/10。
  - PR #129，合 main 2026-06-17。
- **吞吐数字纠偏（task-fix-throughput-bench-honesty，2026-06-19）**：scrum-268 收尾时记的 "CLI 9.5× / GUI 2.07× / 6× poller headroom" 三处均为测量假象——① `--benchmark` 把 setup 计入分母低估快后端（修复后引擎 8–10×）；② GUI 2.07× 是 task-272 修 complex-filter 导入前的无 culling 数（修复后 ~9.5×）；③ 同口径下引擎 ≈ GUI，**无 poller headroom gap**（poller 整幅回读是 per-commit 延迟成本，非吞吐天花板，explore-271 E3 已证）。原始数据见 `scratchpad/task-fix-throughput-bench-honesty/data/`。

### 演进的元模式
1. **战略意图始终是"GPU 求吞吐、两步走"**，但 #256 起逐步滑向"在开发机 Mac GUI 上把 Metal 调好"。
2. **结论反复自我修正**：scrum-260"device-gen 恒默认"→#261"b128 净亏"→#263"regime 2D"→#264"2.40×(vs Metal)"→#265"Metal 输 GUI→b128 饿死→重场景 concern #2"。每步纠正前一步的过度外推，**基线/regime 不断漂移**。

---

## 二、已积累的数据（硬资产）与已探索方向

### 硬数据资产（可信、可复用）
| 数据 | 值 | 来源 | caveat |
|---|---|---|---|
| GPU trace 算力上限 | ~250× 全 sim，计算受限，register-resident | #250 | M2 统一内存 |
| divergence | 证伪（固定 max_hits，无变长路径） | #250 | — |
| 原子累加竞争 | +7%，竞争无关 | #250 | — |
| recorder 寄存器压力 | ≤16%（唯一真风险，证伪） | #251.1 | — |
| GPU-RNG gen | 171×，端到端恢复 32× | #251.5 | **统一内存，不可外推 CUDA** |
| 上传 | 非瓶颈（0.54× trace）；host-gen 才是（40.7×） | #251.5 | **统一内存 caveat** |
| 帧正确性 | 单/多 MS corr 0.99/0.997 vs CPU；1.35×=伪象 | #253 | — |
| exit-seam 带宽 | 省 266-1362× | #256-258 | — |
| device root-gen | 单 1.87×/多 ~2×，parity ≥0.99 | #260 | — |
| 摩擦真身 | GPU 命令缓冲往返**延迟**（wait 89%），非计算 | #263 | — |
| 多 worker | 掩盖延迟非并行（单 GPU，W6→8 持平） | #263 | — |
| gen regime 2D | 大 batch device-gen 18.5M=2× 天花板 | #263 | — |
| 融合 | 2.41×（vs 融合前 Metal） | #263-264 | 分母非 legacy |
| GUI 可行性 | 轻场景大 batch 2-5× 胜；重场景 concern #2 咬人 | #265 | — |
| device filter-match | 全类型（含 Raypath/Complex）1.44M 0 mismatch；seam §4.5 悲观判推翻 | #266 E4 | M2 统一内存 |
| CLI 引擎吞吐 | `--benchmark` setup-excluded + dispatch 32768 → **8.1× / 10.1× legacy**（complex_filter / filtered_bd） | #267-268, #273 复测 | M2 Max |
| GUI steady 吞吐 | infinite + reconstruct → 重场景 **~9.5× legacy**（轻 1.8× / 中 2.2× / 最重 5.7×） | #273 复测 | M2 Max |
| dispatch 甜点 | **32768**（backend-aware 默认）；512/2048 饿死 GPU（0.2–0.8×），128 大 ray_num 挂死 | #268.6, #273 | M2 Max |
| 引擎 vs GUI | 同口径 ≈ 1:1（**无 poller headroom gap**）；旧"6× headroom/2.07×/9.5×"为测量假象 | #273 | M2 Max |

### 已探索方向 & 裁决
- 可移植 kernel 语言 → **拒**（藏寄存器驻留杠杆）。
- async/双缓冲藏延迟 → **拒**（融合已 captures 大头，async 上界 3.07× 不值）。#263
- 多 worker 求 GPU 并行 → **证伪**（单 GPU）。#263
- 自适应 gen 策略 → **推迟**（依赖 commit↔batch 解耦；GUI 走大 batch 则 moot）。#263

---

## 三、遗留项盘点（现在看：仍有价值 / 已过时 / 已被取代）

| backlog 条目 | 现状判断 |
|---|---|
| **CUDA Step 2（离散显存重测 + master plan 第二步）** [425/120] | ⭐**最高战略价值，未动**。真实部署目标（Windows 用户 + dev49 bench 都是 NVIDIA）。所有 Metal"便宜"结论带统一内存 caveat，CUDA 才是真正受检处。seam 全部设计就是为它零上层改动。 |
| **commit↔batch 解耦（concern #2）** [565] | ✅**仍有价值，且已被 #265 跨 regime 证明必要**。启动条件（#264 落地）**已满足**。是 GUI 吞吐+响应的根治解。 |
| **P3 bulk device-fused** [557] | ✅仍有价值，但**属 CUDA/offline 吞吐**（非 GUI），已标 phase-2 CUDA 开篇。 |
| **golden 光线 absolute 锚测试** [438] | ✅仍有价值，尤其"CUDA backend 立项时一并做"——第二后端正需 absolute 锚分辨"kernel 错"vs"两后端一致都错"。 |
| **自适应 gen 策略** [579] | ⚠️**部分过时**。#265 显示重场景要小 batch，可能仍相关；但大半被 decouple 工作吸收，待 decouple 后重估。 |
| **Metal 收尾：默认 batch 调优** [544] | ⚠️**已被 #265 细化/取代**——不是简单"调高默认"，是 concern #2 / regime-aware 问题。 |
| Metal 收尾：sim_seed 文档 / rpath / single_ms_filter [544] | ✅小清理仍有效。 |
| **Metal device-RNG root supply** [410] | ❌**已被 scrum-260 实现/取代**，应关闭。 |
| roofline 内存天花板 [368] / fuse trace→consume [388] | ◽CPU 侧高端 perf，与 GPU 路线正交；仍有效但低优先（多数用户 8C16T 不撞墙）。 |

---

## 四、供讨论的深层张力（observation，非 prescription）

1. **战略漂移：是否在打磨 GPU 的最差战场？** GPU 结构优势在**大批量离线吞吐**（250× 计算受限、register-resident、b2048 18.5M）。而最近所有挣扎（concern #2、b128 饿死、多 worker 非并行、延迟受限）都是 GPU 在 GUI **交互 regime**里打它的**最差仗**。真正的奖品（CUDA 大批量、服务真实用户）一直没动。

2. **regime 结构早就在框架里。** backlog 120 master plan + #263 regime 2D 早把"大 batch 赢/小 batch 角落"画出来了。最近 b128输→调大赢→重场景又输的来回，本质是同一 regime 结构被反复**局部重新发现**——因为没回到全局参照系。正是 owner 说的"想得不够深"：已有全局框架没被复用。

3. **CUDA 是房间里的大象。** Metal phase-1 越打磨 GUI 细节，离 seam 真正受检点（离散显存、CUDA）越远。所有 unified-memory caveat 的结论都欠一次 CUDA 兑现。

4. **基线漂移=摇摆的机制根源。** 每个"×几"在不同分母下成立（融合前Metal/legacy CPU/CpuTraceBackend/host-gen），跨节点不一致→似是而非。已固化 `feedback_perf_baseline_is_legacy_cpu`，但历史结论需用统一基线重读。

---

## 五、Phase 10 — CUDA 第二步 + device-fused 消费(#294 → #302，2026-06-24 → 06-29)

> 接 §一 Phase 9 之后。这一段把 GPU 路线推进到「第二个真实消费者（离散显存 CUDA）」并把消费端（投影+filter+累加）从 host 搬上 device。**缝契约经 CUDA 兑现、零上层改动未返工——蓝图赌对了。**

**explore-294 → scrum-295（CUDA MVP）→ scrum-296（全量多 CI）**：CUDA backend 接入同一 `TraceBackend` 缝。单 MS 验缝 → 全量多 CI/多 MS/filter 对齐 legacy（parity 全绿）。波折：CUDA 自创 Möller-Trumbore 遍历复现了 task-275~278 已解决的绝对-ε 漏面（energy 0.735）→ 教训「几何遍历也要纳入共享核单源，别重新发明」。

**task-cuda-throughput-bench（#299，诚实吞吐基线）**：干净机实测 CUDA ≈ **0.10-0.12× legacy，flat with dispatch**；瓶颈 100% = **出射记录 per-exit PCIe 主机往返**（DrainExits D2H + host filter + host 累加，~54ns/exit）。**推翻 scrum-296 Step D「大 dispatch 1.6-2.2×」**（那是 64MiB exit 顶丢 87% exit 的假象）。结论：device 侧累加是唯一有意义的方向。

**第一性原理收敛（2026-06-28 owner 讨论）**：离散显存上唯一稀缺资源 = PCIe。数据按「是否必须跨 PCIe」分层（输入一次/在途永不/输出小图/富元数据默认永不）→ **纯融合 device 流水线**：emit gate 当场 prob+filter+固定投影+补偿累加进 device XYZ，**渲染图产物不物化出射记录**；富元数据降级为光路回溯的可选 tap。把蓝图 §4.4「两个出口」在离散显存上塌成「一条流水线、两个回读节奏」。详见 `seam-design.md` §4.8（离散显存 reframe）。

**scrum-302（device-fused accumulation）**：
- **S1（Metal）**：消费端上 device（emit gate 融投影+filter+补偿累加），砍出射 buffer，抽 option-b 共享核 `accum_shared.h`。inline 修 3 真 bug（mid-exit 漏 rectangular 分支 / server has_renderable 漏 xyz_pixel_data_ 致零图 / **最终层漏 prob draw → energy=1/(1-prob)**）。验收：parity 17/17 + e2e 15/15。意外：device-fused Metal vs 旧 Metal **2.9-4.1×**（消 host O(N) 投影）。PR 未提（合并在分支）。
- **S2（CUDA/dev49）**：CUDA 接共享核 + device 累加。inline 亲跑 dev49 修 2 bug（mid-exit 漏 device 累加 → energy=1/层数 / 测试 config 用不支持的 fisheye 投影）→ **parity 10/10 绿**。runner 三犯（被 kill / Mac 编不了 CUDA / 收窄 scope 误报 DONE）→ 巩固纪律「CUDA AC 真闭只能 scrum owner 在 dev49 亲跑」。

**⚠️ 未解的核心张力（scrum-302 S2 暴露，owner 定性）**：device-fused 去掉 roundtrip 后，CUDA 吞吐 **0.16-0.40× legacy**（16 线程 Zen5），随 max_hits 收窄不反超。owner 定性:**对一块 4060 Ti 根本不合理 = 设计/实现缺陷**（非"GPU 非天然胜"的可接受现实）。诊断：compute-bound（max_hits 反比）+ 低 max_hits 仍只 1.82M/s 疑 per-CI 串行 dispatch/launch 开销（单引擎大 dispatch 理想在 CUDA 多 CI 未兑现，§3.6 原始之罪复发）+ recorder local-mem。**#250「250×」spike 与 production 间有未追的退化**——这是下一个 explore 的核心(profiler-first，见 backlog)。

---

## 六、Phase 11 — CUDA 吞吐收口 + Windows 交付 + 第三时钟(#303 → #312，2026-06-29 → 07-01)

> 接 Phase 10。这一段把上面「未解的核心张力」（CUDA 0.16-0.40× "根本不合理"）**彻底收口**——真因大半是**测量假象**，剩下是几个真 lever；随后把 CUDA 从"能算对"推到"Windows 可交付"，最后补上蓝图 §4.8 第三时钟。**Phase 10 的悲观基线全部作废，别再引用。**

**explore-303 → scrum-304（吞吐收口第一刀）**：explore-303 一度归因"零 cudaStream + 每层 sync readback → GPU 1-2% 利用"，但 scrum-304 profiling 修正：真因 = **per-batch buffer 拆建 churn**（CUDA Reset 每 batch cudaFree+cudaFreeHost，占 mh15 wall 83%）。**buffer-persist**（`Reset(keep_persistent_buffers)`，镜像 Metal Reset）一刀 → 可比轻·单MS CUDA **35–56M/s（>25M 竞品线，>Mac Metal 28-30M）**，parity 10/10。**"0.16-0.40×" 定性为测量假象**（ad-hoc 非 idle-gate + 拿重场景 `ms_multi_crystal` 当可比口径）。

**scrum-306（dispatch + exit-cap，逼近 intrinsic）**：**async 前提被 profiling 推翻**（nsys 证 cudaEventSynchronize 仅 0.3% host time，stream deferral 值 <1%，增量 2 放弃）。真杠杆 = **默认 dispatch 32768→262144 + exit-cap**（CUDA `SupportsDeviceXyzAccum` 恒 true → trace kernel 从不写 `d_exit_`，这块死缓冲按 n×(2·max_hits+4) 膨到 GB→崩，capping 解锁大 dispatch）→ out-of-box **~114M/s（= 134M intrinsic 的 85%）**。几何池/persist = parity-clean 结构改进但**吞吐中性**（不得当加速源）。旁证：306.6 Metal 无便宜 kernel 杠杆（同 CUDA→算法级 backlog）；306.7 legacy 能量随 dispatch 波动证伪为 MC 方差非 bug。血泪：shared dev49 CPU 争用使 host-bound 吞吐 33M↔114M 同 binary 剧烈抖动，只信 per-run interleaved。

**explore-307 → task-308（77h 漏光，MSVC 专属正确性 bug）**：`RandomSample` 在 `curr_p==0.0`（MSVC `std::uniform_real_distribution` 可返精确 0）时 no-match→out 未写→保留调用方默认 tri_id=0→选背光面当入射→全反射异色高权重漏光。两平台插桩铁证（MSVC 2e9 抽样 115 次命中 / Mac 1e8 抽样 0 次）。与 GPU 吞吐正交，但同期落地。

**scrum-309 + scrum-310（CUDA Windows 交付）**：1070Ti/sm_61 parity 10/10 + 吞吐数据点 + GUI 跨平台"Use GPU"勾选（owner 眼验）；CI 门禁（windows-2022 pin，windows-2025=VS2026 拒 CUDA）+ 多 arch fatbin（PTX 61 floor + 75/86/89 real）+ 运行时 capability<sm_61 探测+优雅降级 + release 打包 cudart dll + CLI `--backend cuda`。**CUDA 在 Windows 真正可交付。** scrum-311 清理（CUDA 死代码 + CI Node24 + exit-seam crystal 计数）。

**scrum-312（第三时钟 readback 解耦，蓝图 §4.8 兑现）**：内测反馈"默认场景 GPU 慢"→真因 = 真实 GUI 分辨率 2048×1024 下 readback 焊死在 trace 时钟（每 SimBatch 一次 device XYZ 回读）。route B 把 readback 解耦到显示节奏第三时钟 → 2048×1024 增益：4060Ti(Ada) 28→39M(1.4×) / 1070Ti(Pascal) 12.5→33.5M(2.7×) / Metal 11→32.3M(~3×)，三机一致。**推翻"Metal 统一内存零收益"**（per-batch 全幅 24MB memset+memcpy 是真成本）。精度用 periodic-drain（float32+host Neumaier+稀 drain），**f64 淘汰**（48MB 溢 L2，0.6×）。parity CUDA 10/10×2arch + Metal 14/14。

**Phase 11 元模式**：Phase 10 的"根本不合理"张力，收口后大半是**测量方法学问题**（非 idle-gate / 错口径 / setup 计进分母 / drain 粒度失真），真机制 lever 只有三个（buffer-persist / dispatch / exit-cap / 第三时钟）。**教训固化**：GPU 吞吐 correctness 不可单一复现路径自证；shared 机只信 interleaved；profile 先于改代码（async 增量差点白建）。当前 canonical 吞吐见 `doc/performance-testing.md`；per-run 详录见 `scratchpad/perf-results-log.md`。**残余**：机制 C（in-kernel atomicAdd L2 溢出，高分辨率残税）+ kernel 算法级重构（SOL 逼近）归 backlog 远期。
## 七、Phase 12 — 投影全量对齐（scrum-315，2026-07-02）

> device-fused 消费端此前只在 GPU 内做**两种固定投影**（`rectangular` + `dual_fisheye_equal_area`）；其余镜头类型经 `IsCompatible` 返回 false → **静默回落 legacy CPU**（输出正确，只是吃不到 GPU 加速）。这是 **GPU 加速覆盖**缺口，不是正确性缺口。

**scrum-gpu-projection-parity（#315，worktree 隔离，5 步 + 1 chore）**：把 forward 投影（dir→pixel，含 pixel-layout / fov scale / lens_shift / dual-fisheye 双眼布局 / overlap / 可见范围裁剪）从三处各写一遍（legacy `lens_proj.hpp` / Metal `lumice_trace.metal` 两 block / CUDA `EmitGateProject`）统一进**单一真源** `src/core/shared/projection_shared.h::ProjectExitToPixel`。三端塌成对同一函数的薄封装 → 加投影≈免费，**cross-backend parity 是结构性保证**（同一份代码，host/Metal/CUDA 不可能漂移）。

- **315.1 explore**：de-risk pixel-layout 能否进 shared 并跨 host C++ / MSL / CUDA 同源编译（无 std 容器）→ 可行。
- **315.2**：建真·共享 `ProjectExitToPixel`（补 `LinearForward` + pixel-layout + dual + overlap），legacy CPU 改调它，**parity-neutral**（仍 10 种、不改行为，用现有 e2e/parity 把重构本身 de-risk）。
- **315.3**：全部 forward 类型接进 Metal（两 block）+ CUDA（`EmitGateProject`）走共享 dispatch；放宽 Metal/CUDA `IsCompatible` + 去 BeginSession assert，让 GPU 接受全部类型。
- **315.4**：新增 **globe** 为第 11 种可渲染投影（legacy 此前也没有，仅 GUI 预览专用）——**选项 B：有限距离球面透视**（`kGlobeCameraD=4.0`，覆盖视轴 ±75.5° 而非满半球，与 orthographic 数学不同，仅 D→∞ 极限相等）；必须与 GUI `gui_constants.hpp` `kGlobeCameraD` 一致（CLI↔GUI 观感一致契约，靠 shared 头 "must match" 注释锚 + golden-analytic round-trip 校验维持）。
- **315.5**：每种投影一个 cross-backend parity 测试（legacy=oracle）`test/parity-cross-backend/backend/test_{metal,cuda}_projection_parity.py`（共享 `test/e2e/_projection_battery.py`），确认全部类型真走 GPU、不再 fallback。

**结果**：**全部 11 种投影**（`linear` / `fisheye_{equal_area,equidistant,stereographic,orthographic}` / `dual_fisheye_{同四}` / `rectangular` / `globe`）现在都在 **legacy CPU + Metal + CUDA** 上渲染，单源自 `projection_shared.h`。GUI 显示重投影（inverse 重采样固定 dual-fisheye 全天图）是 C-API 边界外的独立关注点，不进 shared、不动（仅守 globe `kGlobeCameraD` 一致性；GUI 侧镜头数学多副本的隐性漂移已单记 backlog，后续单独 explore）。

## 八、Phase 13 — 多 renderer N 面累加 + 回退可见性（`941faebd` → PR #372，2026-09-16）

> 投影全量对齐之后，GPU 路剩下最后一条**按 config 形状回退**的门：`CanUseBackend` 的 `renders_.size() != 1`——而 GUI 导出的文档天然带两个 render（预览 + 导出投影），于是用户最常见的 config 在 Metal/CUDA 上整批回退 legacy（实测 5.1M rays/s，收益近乎不可见）。这一段把 seam 从"一个 renderer"改成"N 个 renderer"，两个 device 后端在 exit tail 内逐 renderer 投影到 N 张面（**一个 dispatch，不是 N 个 pass**），解除该门；并把剩余回退原因从 WARN 升成 C API / CLI / `[BENCHMARK]` 可见。as-built 见 `doc/seam-design.md` §4.2.1。

- **seam N 化 + Metal**（`941faebd` / `c26d5a80` / `ac8de513`）：`SessionSpec::renders` 携 N 个 `RenderConfig*`，`SimData` 三累加目标 N 化，`RenderConsumer` 按下标取面；Metal `KernelParams::renderers[4]` 定长描述符 + 三个累加绑定按偏移分片。顺手修掉一个真缺陷：逐 hit 索引原子加 `landed_weight[r]` 被判非 SIMD-uniform，落地权重系统性偏低 1.66%——改寄存器累加 + 每 SIMD-group 逐 renderer 归约。
- **CUDA 同形**（`2517aeaa`）：`EmitToDeviceXyz` 循环 N 个 `RendererPlaneDesc`（per-Impl 设备缓冲 `d_renderers_`），`landed_acc[4]` 逐 renderer 独立 warp 归约——⛔ 合并成一次 shuffle 的错误常规 corr/energy 全绿，只有双账本比值能抓。
- **固定资产 + 可见性**（本 Phase 收口）：逐 renderer parity（`test_{metal,cuda}_multi_renderer_parity.py`，含双账本检查 `sum(Y 面 i)/snapshot_intensity[i]`）、双 render 吞吐闸（`test_{metal_multi_renderer,cuda}_throughput.py`，21 次交错采样取中位数 ≥ 单 render 0.85）；`CanUseBackend` 三条前置门拒绝时翻转 `backend_active_`/`active_backend_`，CLI `Stats:` 行与 `[BENCHMARK]` JSON 新增 `backend` / `fell_back`；`capi_runner` 的路由判定改读 `LUMICE_GetActiveBackend` / `LUMICE_GetBackendFallbackFlag`。

**结果**：用户双 render 文档 Metal **27.9M rays/s = 单 render 的 0.950 / 0.908，4.2× legacy**（改前回退态 1.06×）；CUDA（RTX 5090 D）双 render **282.8M rays/s = 单 render 的 0.906 / 0.992，21.1× legacy**（参照机不锁频，单样本 CoV 0.13–0.16，故闸取 21 次交错采样的中位数比）。`IsCompatible` 两个后端均无条件 true，renderer 上限（4）与 C API 解析期上限相同 ⇒ **今天没有任何能到达 simulator 的 config 会在 GPU 路上按 config 形状回退**；剩余回退只有设备/PSO 中途失败，且现在对用户可见。

## 九、Phase 14 — CUDA 每层空闲间隙：异步化第三次被拒，这次带机制（`fe621b8c` / `0b830f3e` → revert `d3810020`，2026-09-19）

> 前置量尺：home-win 原生（RTX 5090 D，WDDM）1e9-ray 生产规模下，CUDA 的 `1/X`（wall ÷ nsys `cuda_gpu_kern_sum` 硬件计时的 kernel 真实执行时间）= **2.26**——GPU 有一半以上的 wall 时间没有 kernel 在跑。随后一次 1e7-ray 的 nsys CorrId 对齐把每层收尾拆成两层：`exit_count`/`cont_count` 那条 4B 读回的阻塞 ≈ 该层两个 kernel 的真实执行时间，是主机据此给 `DrainExits` 定 size、给下一层分区的**真实控制流依赖**，不可异步化；其后的 `landed_weight` 读回 + `cudaMemset` 清零 + `DrainExits` 计数器重置一串小调用发生在 GPU 已空闲之后，加总 ≈202.9 µs/周期，被估为"每次调用各付一次与字节数无关的 WDDM 派发税"，异步排队 + 一次 `cudaEventSynchronize` 即可回收，估算修复后 1/X≈1.60。**这一估算在生产规模被一手数据证伪**，两版实现都已 revert，树与基线零 diff；两个 commit 留在分支历史只作证据。

**两版实现与同 session 交替三跑的结果**（同日基线复现 1/X = 2.263，与 2.26 立项线一致）：

| 臂 | 做法 | wall（3 跑均值） | kernel 硬件计时 | 1/X |
|---|---|---|---|---|
| base（`f6d797da`） | — | 1.790 s | 791.18 ms | **2.263** |
| v1（`fe621b8c`） | `landed_weight` 读回/清零原地改 `cudaMemcpyAsync`/`cudaMemsetAsync` 到 `stream_` + pinned host 缓冲 + 专用 event 一次 `cudaEventSynchronize`；`DrainExits` 计数器重置改 `cudaMemsetAsync` | 1.802 s | 789.28 ms | **2.283**（中性，−0.7% 在噪声内） |
| v2（`0b830f3e`） | 在 v1 上把 `landed_weight` 的 async D2H + memset **前移到 `exit_count` 阻塞读回之前**，复用既有 `ev_end_d2h_` 等待（`alloc_tally` 同款先例） | 2.005 s | 789.74 ms | **2.539**（慢 11.5%，三次一致） |

**机制（来自 nsys `cuda_api_sum` + `cuda_gpu_trace` 逐周期时间线，非推测）**：

1. **小阻塞拷贝的成本不是"派发税"，是一趟 WDDM submit→complete 往返**。v1 把 `cudaMemcpy` 从 11655 次/1373 ms 降到 7839 次/1204 ms（−170 ms，正是被拿掉的 `landed_weight` 阻塞拷贝），但 `cudaEventSynchronize` 从 3816 次/31.7 ms 涨到 7632 次/251 ms（+219 ms）。等待没有消失，只是换了名字：async 排队后再 `cudaEventSynchronize` 付的是同一趟往返。**只有"不等"才能省掉它**，而 `landed_weight` 在这里是要被消费的。
2. **⛔ `exit_count` 读回之前不得插任何 GPU 包**。GPU 时间线上 base 每周期 `K_trace` 结束 @211 µs → `exit_count` D2H 执行 @251 µs（40 µs 延迟）；v2 变成 `K_trace` 结束 @211 → lw D2H @252 → lw memset @280 → `exit_count` D2H @327。前移的两个微型 GPU 包各占 **~28–47 µs 的 GPU 时间线，与字节数无关**（WDDM 对每个 DMA/memset 包的调度延迟），全部串在那条真实控制流依赖之前；省下的 host 侧一趟小拷贝往返（`alloc_tally` 注释里既有实测 ≈20 µs）远小于新增的 ~75 µs 关键路径 ⇒ 净 +54–56 µs/周期（2.005 − 1.790 = 0.215 s ÷ 3816 周期，对得上）。这是硬约束，后续任何"合并收尾"方案先过这一条。
3. **nsys 把 host API 时长放大约 2×，且小场景每次调用比生产规模高 3–5×**。1e7-ray profiled 口径读到 memset 70.2 µs、eventRecord 48.8 µs、第二个 memcpy 69–85 µs（加总即 202.9 µs）；1e9 生产规模的 capture 里同类是 memset 15.2 / eventRecord 12.8 / launch 16.7 µs，而且 profiling 本身把每周期从 469 µs（未 profiling）拉到 845 µs。**那 202.9 µs 是 profiler 放大后的小场景 host API 时长，不是可回收的真实间隙。** 未 profiling 的真实非 kernel 间隙 = 469 − 207 ≈ **262 µs/周期**，其中 `landed_weight` 那趟往返只占 ~20 µs（≈4%）。方法学教训与 Phase 11 同族：用 profiled 口径的绝对 µs 数去估未 profiling 的收益，尺子本身先没过审（a16/a42）。
4. **D2H 按字节分桶（7815 条，三臂分布完全一致）**：7693 条 ≈0 B（每周期 `exit_count` 4 B + `landed_weight` 16 B，GPU 侧 0.4 µs）+ 61 条 8.389 MB + 61 条 25.166 MB（每 ~64 batch 一次 `ReadbackXyzAccum` 窗口 drain：25.166 MB = 2048×1024×3 float 的 xyz 面，8.389 MB 是同次 drain 的伴随面；GPU 侧 560 / 1900 µs 每次）。**不存在其他尺寸** ⇒ `DrainExits` 的 exit-record 大数组拷贝在 device-fused XYZ（S2）路上**一次也没发生**：kernel 不写 `d_exit_`，`h_exit_count_`==0 直接早返回，v1 改的那行在生产路径根本不执行。这不是小场景假象而是 S2 路的结构性事实（Phase 11 的 exit-cap 已说过 trace kernel 从不写 `d_exit_`，这里是它在 D2H 侧的镜像）。
5. **顺带发现，未动**：那 122 次窗口 drain 阻塞拷贝合计 **≈150 ms/1e9 rays ≈ 8.4% wall**，比本次瞄准的 `landed_weight` 往返（~4%）大一倍，是当前数据里最大的单一非 kernel 项；它在 `ReadbackXyzAccum`（第三时钟，Phase 11 末尾引入），与本次改的函数异根因，只登记不并入。
6. **正确性尺子**：CUDA 路上固定 seed、同一二进制跑两次，输出 `.npy` 的 MD5 就不同（float `atomicAdd` 顺序不确定），所以**逐位哈希不是 CUDA 的回归尺子**，parity battery 才是——两版都过 22/25，3 红为 `parhelion` +0.400% 的既存红且基线同机逐位复现；`CudaKShapeFilterParity`（消费 `exit_w_sum`=`landed_weight`，读回失序会直接红）两版皆绿。

**结果与裁决**：AC「1/X ≤ 1.74」两版均未达标，按 a46 零收益代码不保留（v1 仅剩"显式 event 优于隐式 NULL-stream 顺序"一条理由，那是另一个题）。**这是异步化方向在 GPU 路上第三次被拒**——#263 拒"async/双缓冲藏延迟"（上界不值）、Phase 11 吞吐收口拒"stream deferral"（`cudaEventSynchronize` 仅 0.3%）、本次拒"合并收尾读回"（机制：往返不可免 + 微型包排在控制流依赖前必亏）；三次的共同教训是**异步化只对"不需要等的东西"有效，而 GPU 路上每层收尾要等的东西就是控制流本身**。剩下的 262 µs/周期间隙留给后续一次零代码的模型检验（每周期 ~10 次 host 提交 × 每包 ~30–45 µs 是否成立；不成立就不该再在这条线上花参照机时间），候选杠杆按本次数据排序：① `ReadbackXyzAccum` 窗口 drain 异步双缓冲/降频（8.4%）；② 4 个纯诊断的 timing `cudaEventRecord` 在非 debug 路径关掉；③ 层起始的 `d_exit_count_`/`d_cont_count_` memset 折进 `gen_root_kernel`；④ `exit_count` + `landed_weight` 合并成一个连续 device 缓冲一次 D2H（消掉 ~20 µs 往返而不加包）。
