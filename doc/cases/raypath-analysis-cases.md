# 光路分析测验场景语料（raypath analysis case corpus）

> 状态：**场景索引**（2026-10-06 建立；2026-10-14 人工讨论终态轮：C01–C16 全部处置完毕——C04 绑定更正、
> C07/C08 归组与溯源、C13 能力更正、C16 机制终态——并新增 C17/C18（3157/357 两格，owner 授权增补）。
> 各条「终态补充/增补」段即为该轮结论；人工讨论完成不等于产品全 PASS，未做 owner 独立目视的条目不标人工验收）。「光路分析功能」的测验场景语料的统一登记处：后续相关任务
> （验收电池、普适性测量、GUI 案例矩阵、缺陷回归）从这里取场景，新发现的典型光路在此汇总。
> 它不是一级设计文档——机制与路线图在 [`raypath-analysis.md`](../raypath-analysis.md)，字段契约在
> [`raypath-cli-output.md`](../raypath-cli-output.md)，本文只回答「测什么场景、每个场景考察什么、
> 已知事实是什么」。

## 0. 使用规则

- 一条 = 一个登记场景，编号只增不复用。后续任务引用场景用编号（C01–C18…），不复制正文。
- **场景语料 ≠ 验收 fixture**：fixture 是钉死数值的测试资产（committed config / 断言 / 参考图），
  语料是场景索引。两者互相指认但不合并——一个场景可以还没有 fixture（其「已锚定资产」列为空）。
- 条目被后续证据推翻时**标注处置**（作废 / 被谁取代），不删除。

## 1. 约定（读条目前必读）

- **面号**：prism 基面 1/2、侧面 3–8（相邻侧面 60°）；pyramid 用其自身编号空间。GUI 晶体 id（0–4）
  与 config/CSV id（1–5）差一；present-face id（0–5）≠ raypath 面号（1–8）。
- **角度**：δ = display 散角（与太阳 label 的角距）。方向向量语义（travel vs position、display 规则、
  外部工具换算）以 [`coordinate-convention.md`](../coordinate-convention.md) §12 为唯一权威；跨
  Lumice Integral 对照时其角度在 heading 空间（LI az = label az + 180°，LI scatter θ = 180° − δ）。
- **对表域**：定量对照一律线性域（专用分析运行 CSV、`ev_mode: absolute` + `--format npy` 导出），
  不与 tone map 后的 8-bit 渲染比。
- **标号变体**：拆变体用 `--symmetry none`；L1 标号等价 vs L2 物理等价的语义见
  [`raypath-symmetry.md`](../raypath-symmetry.md)。

## 2. 能力考察维度（条目「考察点」的词表）

| 维 | 问的问题 |
|---|---|
| 位置 | 特征（边/峰/带）的天空位置是否与 MC 一致 |
| 色散 | 特征位置随折射率 n（波长）的移动 |
| 色偏 | 同一天空位置上不同波长（或不同链）的线性强度比 |
| 归属 | 该特征由哪些链 / 标号变体承载、各占多少 |
| 机制 | 特征的物理机制（几何截止 / TIR 截止 / 权重折线 / 族坍缩…）及证据形态 |
| 支撑 | 链的天空支撑区间（δ 域）与跨链互补 |
| 空态/退化 | 无特征、不可达、秩 0、纬线不变、族坍缩等非寻常终态的诚实表达 |
| 收敛 | 数值方法（fiber 行走、场方程、局部搜索）在该路径族上的可用性 |

## 3. 语料条目

| # | 链 / 格 | 场景 | 考察点 | 已知事实（锚点 / 方法） | 已锚定资产 |
|---|---|---|---|---|---|
| C01 | `3-5`（random 正六棱柱） | 22° halo 基线 | 位置 / 色散 / 收敛 | 最小偏折色散内缘：n=1.307/1.317 → δ=21.612°/22.371°（闭式 `2·asin(n/2)−60°`，e2e 以 1e-10 钉住）；D65 首个能量像素 21.532°、红端在内侧；定向族纤维交点性质见 `raypath-analysis.md` §5.1.8 核验注；**份额对表事实（2026-10-10）**：PBD `3-5` 全天份额 21.8117% ↔ filter 臂帧 sumY 和吻合（差 +0.04%，cone 同域互检的先例）；`analyze --roi frame`（dual）与渲染帧 sumY 逐位一致、（rect）= 全部 segment——帧域判定两路同源；全天空 rect 帧与 analysis total 一致到 4e-8 相对（校准臂）；镜头落域语义与 dual 折叠边界丢失（2026-10-10 已修复：切点集归边 bin，dual 帧总能量回归全天空恒等）见 `doc/coordinate-convention.md` §13；**终态补充（2026-10-14 人工讨论）**：22° 内缘是边（最小偏折色散边界），不是自动等于本格的聚光峰——边的位置正确不蕴含峰形已被解释 | config `test/e2e/configs/raypath_feature_random_regular.json`；e2e `test_raypath_feature_report_cli.py` |
| C02 | `3-1-5`（random，h/a=2） | 一条路径双特征 | 位置 / 机制 / 色偏 | **太阳侧**最小偏折色散内缘：实测红 21.686°/蓝 22.298°（与 C01 内缘同位——跨基面中转不改净几何，但峰形不同：C01 内缘峰 5.98 vs 本格 21.5–25° 区间仍升、峰仅 0.0308，**未证明本格自身有聚光峰**）；**反日侧** TIR 权重折线蓝带：δ=127.451–143.365°、kink 131.030°、blue/red 0.859–2.543、R=1 反事实降至 0.7–1.2（Fresnel 权重导数折断机制：两侧路径仍有效、能量连续，非能量跳变/出射门）；schema2 下 TIR 以 conditional candidate（判别式 ≈0）表达、不自动断言可见蓝带。**终态补充（2026-10-14 人工讨论）**：太阳侧普通色散内缘、反日侧宽 TIR 倾向区与软暗洞是三个独立对象，须分别说明，不得互相替代；等价 uniform 参数的识别不一致已有探针证据，但未证明 MC 存在显著分布错误——登记为生产消费缺口，非引擎缺陷裁定 | configs `raypath_feature_random_315_{render,focused_render}.json`；e2e `test_315_report_keeps_conditional_tir_distinct_from_observed_colour`；`raypath-feature-diagnostic-acceptance.md` |
| C03 | `3-5-6-7` | 内反射 fiber | 收敛 / 空态 | fiber↔水平集对应逐点成立（\|R·u+s\|≤3e-15、\|out−T\|≤4.5e-12）；开弧端点为有效域边界事件（`tir_boundary` / `path_infeasible`）；**终态补充（2026-10-14 人工讨论）**：约 141.916° 处有结构边界；姿态分量之间的连接与光能不成简单倍数——完整的 ρAT/J 联合解释保留，不得把 `D_P` 临界值 / 分量数直接称作天空可见亮度特征 | `raypath-analysis.md` §5.1.8 核验注 |
| C04 | `3-1-4-5` / `3-4-1-5` | 含反射开弧一致性 / 反射顺序等价 | 空态 / 收敛 | **处置更正（2026-10-14，owner 确认收口）**：编号保留，原「fiber 为多分量 arc；`TopologyEscape` 变体无完整对照（开放）」的错误绑定撤销——最初拆条时把本格误绑到未指定的 escape 变体上。本格考察开弧一致性与反射顺序等价：LI `test_topology_escape_fiber_support.py` 的 18 例 fiber/store 替代对照已核验含内反射等值线↔fiber（δ69/101.7 各 1 arc），后续薄颈审计修复 3567，其完整 partition 核验即 C03——本格与 C03 只差反射顺序变体。原始 escape 三路径是 3567/3157/357，其中 3157/357 另立 C17/C18 | —（绑定更正后待重新锚定；LI 对照存在，本仓 fixture 未钉） |
| C05 | `4-8-7-5`（beta 场景 C2，TIR 重 4 面） | 真实场景内反射链 | 收敛 / 支撑 | display 支撑 δ∈[0.54°,120.00°]（低 δ 薄尾 → δ119 堆积 8.79% → 刀刃截止；引擎锥扫描 × LI 解析测度互证，2026-10）；δ=120 缘对 n 位移 <0.05°（几何截止，无色）；**schema2 report actual 全空**：默认预算「种子未解局部场方程 / 双前缀不收敛」（min ESS 24<32），16× 预算后变为 unavailable / no_support_at_iterate / iteration_limit——复杂路径 actual 发现类失败的首个已知成员（beta 用户实测发现，2026-10-05）。**schema3 报告面（2026-10-09）**：partition complete [0°, 50.161740°, 120°]（walk ok/closed，无 escape），每端点带 onset（slab_axis / boundary_extremum / corner / slab_circle），对象全 computed——v2 的 actual 失败路径已被几何层结构性替代 | config `test/e2e/configs/raypath_feature_beta_prism.json`（β 几何直接构造，partition 与取向分布无关）；e2e `test_raypath_feature_schema3_cases.py` |
| C06 | C3 基面族：`4-8-1-7-5` 字面 + `(4,8,2,7,5)` + `(5,7,1,8,4)`（8 PBD 变体） | 反日侧承载 | 归属 / 机制 / 色散 / 支撑 | display 支撑 δ∈[120.00°,179.69°]，与 C05 在 δ=120° 圈**精确互补**；δ≈150 洞缘 = 基面反射 TIR 截止能量边，**LI 已闭式指认**：基面 TIR kink = 常值闭式圆 **149.247°@550nm**（148.645°@700 / 150.496°@400；LI wave-3 第一批，LI 仓 PR #49），越靠反日侧亚临界越深、Fresnel 反射率崩塌（引擎侧 ray 数升而能量崩互证），8 变体近乎均分承载（9.7–15.1%，锥 per-chain `--symmetry none`）；Δδ_edge(400−700 nm)=**+1.851°(LI 闭式圆)** / +1.87°(LI 早先数值锚，与闭式差 0.02°) / +2.00°(引擎单波长 npy)，蓝更深，与 2–3° 观测色过渡带同量级同方向（足以解释色宽）；洞内有亚临界残余微光（T 尾巴）；色序蓝内→绿→红外。**schema3 报告面（2026-10-09）**：partition [120°, 151.667407°, 180°]（与 C05 的 120° 精确互补已 e2e 钉），常值圆 149.246752°@550 以 `constant_curves[].d_p_rad` 发射，色散经 400/550/700 三单波长 e2e 钉（150.495979/149.246752/148.645350，Δ=1.8506） | config `test/e2e/configs/raypath_feature_beta_prism.json`（5 面请求含 (4,8,2,7,5) 兄弟行）；e2e `test_raypath_feature_schema3_cases.py`（互补 + 常值圆 + 色散）；单测 `test_support_block.cpp` |
| C07 | `(8,7,1,4,5)` | 太阳侧 | 归属负对照 | **溯源与终态（2026-10-14）**：编号保留，错误变体来源已查清——2026-10-05 旧 `li_probe4.py` 手填 variants 把 `8-7-1-4-5` 误标为 `4-8-1-7-5`/PBD 的 k4；现行 LI `pbd_orbit` 确认 `8-7-1-4-5` 不在该轨道中，原 beta filter401 为 `3-5-1-6-8`/PBD，保存的 10 份 MC CSV 也无该类能量行——上游变体误构造 + 证据层误判，无法确定当时想写的正确编号，勿未经 owner 裁定换成近似编号或宣称正常太阳侧晕。**本轮对账**：同 beta 形状下 10M MC 全黑；C++ 原函数逐 8 成员的 100k 方向样本在 height 2/3 均有光学有效点但 A 全零；LI 独立展开显示入口与展开出口共面且多边形分离——支持当前 shape 有限走廊空。历史 `delta_hist` 的 pose_count 来自方向光学 valid、未以正 A·T 筛选，报告另有 `corner_not_simple` 边界拒答——unknown 不能误写成无光证书。对账时不得引用被归档的人工现场材料作长期锚 | —（机制表述已锚，尚无永久 fixture） |
| C08 | `(7,5,4,8)` | 太阳侧 | 归属负对照 | **归组更正（2026-10-14）**：`7-5-4-8` 确为四面 `4-8-7-5` 的 P3 变体——本格作为 **C05 四面族的归属负对照**成立，但旧「k0+3」标签暗示它是 `4-8-1-7-5` 变体，不得沿用。本轮独立 report 与 C05 得到相同四 L2 成员，PBD 场景已有覆盖；支撑记录 δ∈[0.11°,120°] 中旧 0.11° 下限只是采样值，**不作为硬边界** | —（归组事实已锚：`reports` 层 equivalence 审计与 batch 登记存在，committed fixture 未钉） |
| C09 | `13-15-26-28`（锥晶） | 有限晶体门空态 | 空态 | 无限晶体 U_P 在 δ≈98–120° 有等值线、有限晶体 3e6 事件无 fiber（轮廓积分 `gated_out`）——「有等值线」与「有限晶体有亮度」分层；`reach=true` + 空分量的三分语义见 `raypath-cli-output.md` §3.9。**schema3 报告面（2026-10-09）**：partition [37.334°, 155.73°, 177.229°] 覆盖语料带；声明 wobbling 轴下 kind-1 对象 computed+partial（restricted 轨道部分点亮、`sampled_exhaustive` 确定性）——**unlit 判据本身由单测钉住**（全暗 pinned member 的活体 feedstock），本链生产面未复现 unlit（语料 3e6 无 fiber 是固定 pose target-mode 事实，与声明分布下部分点亮不矛盾）。**终态补证（2026-10-14）**：原空态场景是 LI 非对称 pyramid（prism_h0.5/upper0.25/lower0.6、上下 Miller 101/203），与 committed cone fixture 的对称 pyramid 不同——后者 partially lit 不是前者有限门的证据。LI 独立重算三分支撑：109.588° 两弧 `gated_out` true/value 0；120° 两弧 `gated_out` false/value 3.1e-12（panel 耗尽，非收敛精确量）；134.7° 闭环 2.25e-6；生产核心在 LI 363 个 109.588 节点上光学 valid/T>0 而 A 全 0。旧「δ≈120.3°」是有限 store 采样首个点、非精确截止，「[98°,120.3°) 全零」的范围概括应按三分支撑读。⚠️ unlit 机制单测 `C09UnlitShapeOnAFullyDarkPinnedMember` 实际用菱形片晶 `2-4-5-1` 替代锚——不要声称该单测钉住了本锥晶 | config `test/e2e/configs/raypath_feature_cone.json`；e2e `test_raypath_feature_schema3_cases.py`；unlit 机制单测 `test_structure_enumeration.cpp`（替代锚为菱形片晶） |
| C10 | `3-6-4-8`（plate / 纬线不变） | 退化 + h 依赖空态 | 空态 / 支撑 | 等值线 = 纬线圈（行内 D 极差 ≤1e-11 rad）、纤维 = 有效域截断弧（每圈 ~36% 经度有效，非闭环）；h≥0.8 两段 arc / h≤0.5 百 M events 零分量——「几何可达 vs 有限晶体无放行」区分的压力格。⭐ **schema3 report-route 红态：已诊断并收口（2026-10-09）**：`--report` 对本链（h=0.3 与 h=0.8 两变体）枚举腿曾 >420 s 不终止——诊断结论 = **非无界循环，是多项式爆炸**：`Diagnose` 色散腿的 kink step-1 行走在纬线退化族上产出 818 弧 / 78 万点（marched 种子去重带 ~1° 对「同一段纬线弧被数百个种子重复行走」不设防），特征统计 `ShiftOf`（红蓝曲线逐点最近邻配对）O(B×R)≈6.2×10¹¹ 点积 ≈ 数小时/成员；partition / boundary 行走本身 0.0 s 完成，「无孤立根 → 无界迭代」假设证伪。收口 = 三层声明式有界化（不碰退化语义）：枚举腿 300 s anti-hang deadline（`budgets.enumeration.hang_cap_ms/truncated/truncation_note`，防挂起是健壮性下限非质量旋钮、不与 `--budget-ms` 耦合）+ 内核 shift 配对成本守卫（超界曲线对 declared not-assessed，`coverage_complete` false + notes 具名，永不半算）+ 对象 carry 上限（稠曲线体 `walk_truncated`+`walk_s` 0.0+空 u）。**终态（两变体 rc=0，~6.5 s）**：kind-1=`escaped`（`slab_crease_touching`，partition 对该退化族自身的裁定）、稠 kind-3=`walk_truncated` 空曲线体、budgets 如实上报；target-mode 锚与单测零改动。**高度口径补证（2026-10-14）**：目标 alt20/az120 的 δ=108.937304°，h0.3 `reach=true` 空；h0.8 两 arc、126 点仅 15 点正 A·T；16384 理想 spin 采样 h0.3/.5 A 为 0、h0.65 已正 A——「h≥0.8 有 / h≤0.5 无」只能作为已测样例，**不是 0.8 精确临界高度**。方向公式纬线圈、光学有效弧、有限 A·T 支持三层需分清。另：GUI 实际观察版新增 zenith std0.3/roll uniform360，PBD 物理 L2 从 fixed 的 2 成员变 12、新 D65 预算耗尽——不能与旧 fixed 截断状态混写 | `raypath-analysis.md` §5.1.8 退化注；e2e `test_raypath_feature_schema3_cases.py`（C10 两变体格）；doc `raypath-cli-output.md` §7 有界化段 |
| C11 | `1-4-5-2`（plate，太阳高 20°，目标 (20°,120°)） | 120° 幻日主路径 | 机制（族坍缩）/ 收敛 | fiber 纬度 20.000°、有效弧 60°、逐点 \|out−T\|≤3.8e-12；同配置 10M MC 目标锥内 73.4%、同高度 ±2° 方位外为 0；目标球面偏折 **108.937°**——120° 是相对方位差而非偏折角（「方位差 ≠ δ」的钉子格）；整族姿态坍缩为天空一点（u 球纬线圈与纤维重合）。**schema3 报告面（2026-10-09）**：族 shared 支撑 [0°,120.000°]（gauss 天顶变体下 `kind_1_restricted` 对象 computed+partial，lit_fraction=1/6（=有效弧 60°/360° 族轨道占比，e2e 钉）、corroboration=observed、720 点 u 轨道随对象携带（108.937° 本身仍锚在 contour docking 单测——对象无 sky_position 面）。**语义更正（2026-10-14）**：e2e 里 support 上限 120 注释写成「方位差语义」是**错的**——该 support 是全偏折场散角 δ 的 [0°,120°]；sun20 像的方位差 120° 与散角 108.937° 是另两个量（108.937° 数值断言本身正确）。⚠️ 长期文档按此读；e2e 测试注释的修正属代码范围、不在本语料 chore 内。**归组（owner 提出）**：C11 作为已有 120° 幻日组的变体登记，与 C10/C12 同族机制覆盖；历史编号与独立成功回归锚保留，owner 未做独立 GUI 验收，不标人工验收 | config `raypath_feature_plate_target.json`（同 config 含 C14）+ `raypath_feature_plate_gauss.json`（schema3 报告面变体）；e2e `test_raypath_feature_schema3_cases.py` |
| C12 | 菱形板 `[1.5,1,1,1.5,1,1]`（h/a=2、太阳高 9°）的 120° 白/蓝色偏类 | 同一天空点、不同链、不同色 | 色偏 / 归属 / lit_members | **tint 三口径并存**（口径未统一前不互比）：①**LI analytic 权威**（LI #51 纤维求积口径，schema3 验收接线值）——蓝 **≈1.525** / 白 **≈1.010**；②**平坦场 mono-λ 对 MC 口径** t(450,650)（t=(Yλ₁,链/Yλ₁,full)/(Yλ₂,链/Yλ₂,full) 于 550 峰位）——蓝 **≈1.33** / 白 **0.97–0.99**（`1-3-4-2` 0.975、lit `4-8-7-5` 0.989）；t 随波长对 1.11–1.33 变化；③**schema1 固定探测器口径**（历史）：蓝 **1.528**、白 ≈1.02；**旧值 1.632 的口径已裁定（2026-10-14 补证）**：生产 analytic 独立网格复现——字面蓝 1352 成员（L2 对）比 **1.632366**、其他 L1 标号兄弟（如 `1-4-8-2`）**1.503153**、L1 每目标积分总和 **1.525349**——1.632 是字面成员口径、1.525 是类汇总口径，两者都真、不矛盾；文档区分 L1 标号兄弟与真正 L2 对称成员，旧「待 LI 裁定后废弃」不再适用（保留数字与裁定记录，语料规则）。**蓝 1352 类** `1-3-5-2` / `1-3-7-2`：PBD 隔离下两成员**字节同**（同轨道同 ray 集）——「成员口径差异」钉死一半，PBD 隔离下两成员是同族；**白 1342 类** `1-3-4-2` / `1-3-8-2`（tint 数值见上方三口径）；**白 `3-5-6-8`**：≈1.03（口径③历史）且**字面代表在该晶体不可行**，实际点亮成员为 `4-8-7-5` / `5-7-8-4` / `7-5-4-8` / `8-4-5-7`（= C05/C06 族——菱形板 120° 白类物理上由该族承载）；位置：相对太阳方位 ±120°、球面角距 117.5998°；tint 由**逐成员逐波长正 A·T 汇总**决定，不可从几何重合或字面代表推出。⚠️ schema2 已移除 v1 的 per-member brightness 行——**报告侧色偏表达是已知缺口**，现测量路径 = 口径②的 mono-λ 隔离渲染对（absolute EV + npy）。**schema3 报告面（2026-10-09）**：kind-1 对象的 `chromatic` 腿可达并 assessed（verdict + 阈值参数随身，fixed 与 gauss 天顶两形态都达），**`tint` 块仍未发射**（口径①的 1.010/1.525 单测锚在 fiber-quadrature / contour-docking，生产装配链未把这些数值接进对象 verdict——缺口从「无色偏表达」收窄为「色偏表达缺 tint 块」）。**TIR 反事实补证（2026-10-14）**：严格保持 λ 各自的 A / 入出透射 / 姿态 / 方向、只撤内部 R——蓝整类比降为 **0.952624**，白 1342 几乎不变（1.009806→1.008513）——确认近 TIR 内部 Fresnel 权重是蓝增强来源 | config `raypath_feature_rhombic_plate.json`；`raypath_feature_rhombic_plate_gauss.json`（schema3 变体）；e2e `test_raypath_feature_schema3_cases.py`；`raypath-feature-diagnostic-acceptance.md`（历史 fixture 数值与哈希）；tint 单测锚 `test_fiber_quadrature.cpp` / `test_contour_contract_docking.cpp` |
| C13 | `1-6-2`（Parry） | 非片晶族坍缩 | 机制 / 空态 | **能力更正（2026-10-14，撤旧句）**：原「`family_pinned` 判据只覆盖 plate/Lowitz，非片晶坍缩未全覆盖（开放）」不再成立——Parry pinned 识别**已不缺**：`analytic::FamilyAxis`/`FamilyPinned` 使用一般族轴，持久锚 = 单测 `test_dp_focus.cpp::FamilyPinnedCoversParry` 与 `SubsunDConstantOnCirclesAboutBodyX`；LI `test_conventions.py` 核对 roll0 时 162 有效/132 空、roll180 标签互换，`test_folding`/`test_focusing` 已覆盖 body-x 圆，LI `fe0c796` 文档说明 Parry。折叠矩阵 S_x 整族坍缩的本族方向 = 日下点 alt−20/az0、δ40、无位置色散。**本轮场景事实（CLI 4.7.3-dev，sun20、规则 h1、zenith90±0.3/roll0±0.3）**：162/261 两 L2 成员 shared、δ0..180 完整 partition、`kind_1_restricted` computed/partial、720 节点、轨道 lit_fraction 0.2778、corroboration observed——总 outcome 仍 partial，不能写全 report 验收通过。**L2 语义收口（owner）**：本 Parry roll 受限、L2 不具 P 对称；162/261 的 BD 成员与报告一致，宽标签 PBD 图里的额外弧是其他侧面链/弱弧，不能记成 L2 漏报或对称实现缺陷（主链人工场景采用 BD 仅基面对称对，勿把宽标签类与物理 L2 混同） | `raypath-analysis.md` §5.1.8 LI 对照注；`test_dp_focus.cpp` 两持久锚；LI `fe0c796` |
| C14 | `1-2`（plate） | rank 0 | 空态 | D≡0、`point_mass`、落点=太阳位置、`target_separation_deg` 正确——不可复用普通空态文案。**终态补充（2026-10-14）**：平行基面零楔下方向恒等、无位置色散，特征即太阳原像；生产 rank0 原语存在，但 report 专门终态的接线尚缺——既不能说「缺所有原语」，也不能说「产品已自动早停」 | config `raypath_feature_plate_target.json`；`raypath-cli-output.md` §3.10 |
| C15 | `1-3-6-2` | 恒等型秩 0 | 空态 | 相对平行侧面成对复合为恒等（六棱柱长度 ≤4 共 16 条秩 0 之代表）。**终态补充（2026-10-14）**：含两次反射的方向仍恒等，但 R² 近 TIR 的选择性造成整体偏蓝——actual D65/日盘积分 B/R ≈ **1.69**、两 seed 生产 rawXYZ ≈ **1.65/1.71**、仅撤内部 R ≈ **0.97**。几何 trivial ≠ 权重/颜色 trivial；与 C12 同为反射权重蓝增强，但方向机制与聚合口径不同，数值不混用 | — |
| C16 | 定向 `3-5`（column / Parry / Lowitz） | 交点 / 相切 / 整段重合 / 亮弧机制 | 收敛 / 机制 | u 球投影带仅**必要非充分**（柱晶 5 目标 3 假阳性）——判据必须在 pose 空间。**判据更正（2026-10-14）**：旧「两个近并根 = 相切」「Parry 用最小距离」降级为历史方法记录——旧 (40,30) 有两个很近的真实根、旧 Parry 离散最小距离 0.71°，都不可当精确相切或宽容差证书；550/sun20/规则 h1 点源 scratch 约束求解定位 column 真切点 **(40, 29.680390583)**（双侧 g>0 且二次增长、A 0.08547、完整 SO3 方向 Jacobian rank2，仅受限 column 天空映射出现 fold）与 Parry 精确族见证目标 **(40, 29.835010151)**（A 正）；当前 CLI 点列在真切点 g_min 仍 2.44e-5，纯变号 / 采样最小值会漏切触。这些是 scratch 数值补证，尚无持久 fixture，不能写成生产 report 已逐态输出。**亮弧机制（2026-10-14）**：受限 column 2D 映射的 ordinary fold 临界像给上/下切弧亮缘——完整 SO3 方向映射在选定截线仍 rank2，不能把全 `D_P` 临界结构直接改称切弧；Parry 理想族仅自由 az、正 AT 分支天空速度非零，为常规 1D 像、线通量按 pAT/速度加权——两者展宽机制不同；生产原语完整 fiber 上的 ρAT/J 积分复现两峰，AT 为常数仍有峰而 Haar 密度无对应两峰。report 自动轨迹/亮度峰线功能**未交付**。**整段重合更正（2026-10-14，撤旧句）**：「35 全局无整段重合」撤回——侧面链守恒 (q−s)·R e_z=0，太阳关于水平面镜像 q 使 Δq 竖直，整个目标 fiber 必有 Rzz0；sun20/550 的 target(−20,0) 一 closed 145 点全部正 A 且 Rzz≈1e-15。一条 1D 子集汇聚 ≠ 整个 2D 族 pinned/Dirac 质量，也不等同下切弧接 22° 圈最高点 −1.916°。**Lowitz 三类终态（2026-10-14，owner 确认）**：当前 Gaussian zenith0/std40 在生产 pole fold 中 az/roll 同时 +π、两分支等权，actual joint 保留 **P3（180°）**；实际三类 = 上 35/37/68/64、下 46/86/53/73、环 48/84/57/75——当前六 L2 产品 row 过细；生产 P gating 布尔只整组启/禁 60° 循环、尚未表达 180° 子群，六 row 问题**未修代码**（诚实登记缺口；修补须独立正式任务，不借文档 chore 改 gating）。术语：P3 是既有晶体 P 的幂、非新增固有对称；跨极点折回是同一 R 的坐标重写——用「当前取向分布保留的既有 P3 等价」。旧六分图只画 roll0 sheet、非 actual GUI 完整测度，需勘误；三类 10M/all20M 生产 MC 与 4 LMC 核对完成，owner 确认三类显示无误。三报告真实 L2 分别 column 12、Parry/Lowitz 各 35 与 37 两成员；GUI 明确 OR 这些成员，literal 35 仅诊断对照；report 仍 partial 且无 budget 耗尽，kind1 对象不是目标族交点标签，无 kind1_restricted（35 wedge60 不 pinned）；三组主要弧的具体代表链待对齐，不把当前 row 数当弧形数。**schema3 报告面（2026-10-09，target-free 侧）**：column config（天顶 gauss 90°±1）下 3-5 链 kind-1 对象 computed+partial（`sampled_exhaustive`）、s6_junction 在场；δ 轴 partition 与取向分布无关（与 C01 随机族同值——同晶体同链的纯几何事实）。三态（交点/相切/重合）仍是 target-mode fiber 语义，报告面未逐态表达 | config `test/e2e/configs/raypath_feature_column.json`；e2e `test_raypath_feature_schema3_cases.py`；持久锚 `test_dp_focus.cpp`（族轴机制） |
| C17 | `3-1-5-7`（开弧族，escape 三路径之一） | 反日亮度聚集机制 / 开弧 | 机制 / 色偏 / 空态 | **增补（2026-10-14，owner 终态）**：反日侧亮度聚集、0.5° 太阳尺度盘形核心与外围宽弥散；太阳角距 120–125° 已较弱、略蓝。机制 = 两次反射的 π 旋转共轭 q=2(a·s)a−s，其 normalJ = 4cos(δ/2) 趋向反日 0——解释聚集；**不是**整个取向族 pinned 的均匀太阳副像。目标事实（550nm）：110/150 分别 1 arc/2 arc，179 两 arc 且有正 A/T。色偏：actual D65/日盘区域积分反日 1° B/R ≈ **1.33**、仅撤内部 R ≈ **1.03**——支持近 TIR/Fresnel 色偏。诚实边界：`slab_crease_touching`/partial 未修，不由可见性宣称 partition 完备。口径注意：LI canonical n=1.31 与 Lumice 生产 550nm 是两个口径，对照须注明。⚠️ 编号历史：本链曾被 2026-10-05 旧 probe 误标变体（见 C07 溯源），本条为正式登记，与 C04 的 escape 三路径记载衔接 | 单测 `test_support_block.cpp::SupportBlock.RefusedPartitionIsAFailClosedRow`（crease touching 拒绝行）、`test_geometry_source.cpp::EscapeFormCarriesSlugAsDataWithNoIntervals` / `BucketDerivationIsInvariantToTheEscapeRegimeSentinel`（escape walk + G3 负对照）；人工 GUI 观察未做持久 fixture |
| C18 | `3-5-7`（开弧族，escape 三路径之一） | 反日宽弥散 / 支撑 | 机制 / 支撑 / 色偏 | **增补（2026-10-14，owner 终态）**：此前「中心盘形/中心强/95° 弱尾/略蓝」的旧句因 owner 曾误开 LMC 而明确撤回；有效观察 = 反日较暗**无盘像**、向外宽弥散渐亮、约 85° 太阳侧更亮且更颗粒、过渡非硬边、约 62° 弱尾。机制 = 镜面共轭 normalJ = 4sin(δ/2) 在反日非零（与 C17 的 4cos(δ/2) 对偶）；90° 一 arc / 140° 一 closed 皆有正 A；120° 光学开闭门不自动形成天空锐环。权重：两 actual source scrambles 支持宽亮域；550nm TIR 能量份额在 70–80/80–85/85–90 区间约 **94/57/4%**——权重过渡解释。诚实边界：weighted split 实现支持噪声机制方向，但未做 pixel 方差/Σw²/ESS，不标噪声定量认证；`slab_crease_not_carried`/partial 仍在，CLI 小检查为 not_carried 且有正 A/T | `test_geometry_source.cpp`（not_carried 拒绝形态同族）；人工 GUI 观察未做持久 fixture |

## 4. beta 场景与 `.lmc` 转换（C05/C06 的复现方法）

真实 beta 场景的 `.lmc` 在仓外。复现路径 = `.lmc` → config 转换，要点：GuiState JSON 紧随 44 字节
header（magic/version/flags/json_offset/json_size/tex_offset/tex_size）；filter `type:"sop"` 的
`summands[]` 每个字符串内还有 `;` 分隔的备选路径，必须再拆；root 的 `render` 段不能省（缺它 =
`LUMICE_ERR_MISSING_FIELD`）；`spectrum:"D65"` 字符串直接可用。转换正确性用 analyze 锥能量份额
交叉验证（与 GUI 收敛值比对排序与成员）。C05/C06 的支撑区间与变体份额事实由引擎锥扫描
（`--symmetry none`）× LI 解析测度互证建立（2026-10），**未钉 committed fixture**——钉 fixture 时
在条目「已锚定资产」补记。

## 5. 新增条目规则

一条光路值得登记，当且仅当：

1. 有明确考察点（§2 词表至少一维），且满足其一：机制有教学/验收价值、暴露过缺陷（回归价值）、
   或覆盖词表空档（新形状 / 新取向族 / 新空态形态）；
2. 带可复现配置或方法（committed config 优先；仓外场景给转换方法）；
3. 事实带锚点（tracked 文档 / config / 测试 / PR）或可复现方法；两者都给不了的写「未核验」。

字段与格式照抄 §3 表头；「已知事实」内区分**已核验**与**开放**；编号只增不复用。

> 历史注：本索引建立（2026-10-06）前，案例讨论分散在 `raypath-analysis.md` §5.1.8、
> `raypath-feature-diagnostic-acceptance.md` 与多份工作记录中；收拢时以 tracked 锚点为准重核，
> 无法锚定的结论显式标注。行内 C05/C06 的**变体份额**数字（9.7–15.1%）仍属未钉 fixture 的后一类
> （方法可复现，锚点待 fixture）；C05/C06 的支撑区间、120° 互补与洞缘常值圆/色散已于 2026-10-09
> 落成 committed fixture（`raypath_feature_beta_prism.json` + schema3 e2e），不再是缺口。
