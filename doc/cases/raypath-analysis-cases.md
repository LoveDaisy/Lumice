# 光路分析测验场景语料（raypath analysis case corpus）

> 状态：**场景索引**（2026-10-06 建立）。「光路分析功能」的测验场景语料的统一登记处：后续相关任务
> （验收电池、普适性测量、GUI 案例矩阵、缺陷回归）从这里取场景，新发现的典型光路在此汇总。
> 它不是一级设计文档——机制与路线图在 [`raypath-analysis.md`](../raypath-analysis.md)，字段契约在
> [`raypath-cli-output.md`](../raypath-cli-output.md)，本文只回答「测什么场景、每个场景考察什么、
> 已知事实是什么」。

## 0. 使用规则

- 一条 = 一个登记场景，编号只增不复用。后续任务引用场景用编号（C01–C16…），不复制正文。
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
| C01 | `3-5`（random 正六棱柱） | 22° halo 基线 | 位置 / 色散 / 收敛 | 最小偏折色散内缘：n=1.307/1.317 → δ=21.612°/22.371°（闭式 `2·asin(n/2)−60°`，e2e 以 1e-10 钉住）；D65 首个能量像素 21.532°、红端在内侧；定向族纤维交点性质见 `raypath-analysis.md` §5.1.8 核验注 | config `test/e2e/configs/raypath_feature_random_regular.json`；e2e `test_raypath_feature_report_cli.py` |
| C02 | `3-1-5`（random，h/a=2） | 一条路径双特征 | 位置 / 机制 / 色偏 | **太阳侧**最小偏折色散内缘：实测红 21.686°/蓝 22.298°（与 C01 内缘同位——跨基面中转不改净几何，但峰形不同：C01 内缘峰 5.98 vs 本格 21.5–25° 区间仍升、峰仅 0.0308，**未证明本格自身有聚光峰**）；**反日侧** TIR 权重折线蓝带：δ=127.451–143.365°、kink 131.030°、blue/red 0.859–2.543、R=1 反事实降至 0.7–1.2（Fresnel 权重导数折断机制：两侧路径仍有效、能量连续，非能量跳变/出射门）；schema2 下 TIR 以 conditional candidate（判别式 ≈0）表达、不自动断言可见蓝带 | configs `raypath_feature_random_315_{render,focused_render}.json`；e2e `test_315_report_keeps_conditional_tir_distinct_from_observed_colour`；`raypath-feature-diagnostic-acceptance.md` |
| C03 | `3-5-6-7` | 内反射 fiber | 收敛 / 空态 | fiber↔水平集对应逐点成立（\|R·u+s\|≤3e-15、\|out−T\|≤4.5e-12）；开弧端点为有效域边界事件（`tir_boundary` / `path_infeasible`） | `raypath-analysis.md` §5.1.8 核验注 |
| C04 | `3-1-4-5` / `3-4-1-5` | 含反射开弧 / 多分量 | 空态 / 收敛 | fiber 为多分量 arc；`TopologyEscape` 变体**无完整对照（开放）** | —（部分核验，未锚定） |
| C05 | `4-8-7-5`（beta 场景 C2，TIR 重 4 面） | 真实场景内反射链 | 收敛 / 支撑 | display 支撑 δ∈[0.54°,120.00°]（低 δ 薄尾 → δ119 堆积 8.79% → 刀刃截止；引擎锥扫描 × LI 解析测度互证，2026-10）；δ=120 缘对 n 位移 <0.05°（几何截止，无色）；**schema2 report actual 全空**：默认预算「种子未解局部场方程 / 双前缀不收敛」（min ESS 24<32），16× 预算后变为 unavailable / no_support_at_iterate / iteration_limit——复杂路径 actual 发现类失败的首个已知成员（beta 用户实测发现，2026-10-05） | —（beta `.lmc` 在仓外；复现方法见 §4） |
| C06 | C3 基面族：`4-8-1-7-5` 字面 + `(4,8,2,7,5)` + `(5,7,1,8,4)`（8 PBD 变体） | 反日侧承载 | 归属 / 机制 / 色散 / 支撑 | display 支撑 δ∈[120.00°,179.69°]，与 C05 在 δ=120° 圈**精确互补**；δ≈150 洞缘 = 基面反射 TIR 截止能量边（越靠反日侧亚临界越深、Fresnel 反射率崩塌；引擎侧 ray 数升而能量崩互证），8 变体近乎均分承载（9.7–15.1%，锥 per-chain `--symmetry none`）；Δδ_edge(400−700 nm)=+1.87°(LI) / +2.00°(引擎单波长 npy)，蓝更深，与 2–3° 观测色过渡带同量级同方向（足以解释色宽）；洞内有亚临界残余微光（T 尾巴）；色序蓝内→绿→红外 | —（同 §4） |
| C07 | `(8,7,1,4,5)` | 太阳侧 | 归属负对照 | 支撑 δ∈[0°,121.2°]（太阳侧）——不可能承载反日侧特征；用于防错误归属 | — |
| C08 | `(7,5,4,8)` | 太阳侧 | 归属负对照 | 支撑 δ∈[0.11°,120°]（太阳侧） | — |
| C09 | `13-15-26-28`（锥晶） | 有限晶体门空态 | 空态 | 无限晶体 U_P 在 δ≈98–120° 有等值线、有限晶体 3e6 事件无 fiber（轮廓积分 `gated_out`）——「有等值线」与「有限晶体有亮度」分层；`reach=true` + 空分量的三分语义见 `raypath-cli-output.md` §3.9 | `raypath-analysis.md` §5.1.8 核验注 |
| C10 | `3-6-4-8`（plate / 纬线不变） | 退化 + h 依赖空态 | 空态 / 支撑 | 等值线 = 纬线圈（行内 D 极差 ≤1e-11 rad）、纤维 = 有效域截断弧（每圈 ~36% 经度有效，非闭环）；h≥0.8 两段 arc / h≤0.5 百 M events 零分量——「几何可达 vs 有限晶体无放行」区分的压力格 | `raypath-analysis.md` §5.1.8 退化注 |
| C11 | `1-4-5-2`（plate，太阳高 20°，目标 (20°,120°)） | 120° 幻日主路径 | 机制（族坍缩）/ 收敛 | fiber 纬度 20.000°、有效弧 60°、逐点 \|out−T\|≤3.8e-12；同配置 10M MC 目标锥内 73.4%、同高度 ±2° 方位外为 0；目标球面偏折 **108.937°**——120° 是相对方位差而非偏折角（「方位差 ≠ δ」的钉子格）；整族姿态坍缩为天空一点（u 球纬线圈与纤维重合） | config `raypath_feature_plate_target.json`（同 config 含 C14） |
| C12 | 菱形板 `[1.5,1,1,1.5,1,1]`（h/a=2、太阳高 9°）的 120° 白/蓝色偏类 | 同一天空点、不同链、不同色 | 色偏 / 归属 / lit_members | **蓝 1352 类** `1-3-5-2` / `1-3-7-2`：blue/red ≈1.6（两次独立测量 1.528 / 1.632，成员与波长口径差异未裁定——本格要钉的量）；**白 1342 类** `1-3-4-2` / `1-3-8-2`：≈1.02；**白 `3-5-6-8`**：≈1.03 且**字面代表在该晶体不可行**，实际点亮成员为 `4-8-7-5` / `5-7-8-4` / `7-5-4-8` / `8-4-5-7`（= C05/C06 族——菱形板 120° 白类物理上由该族承载）；位置：相对太阳方位 ±120°、球面角距 117.5998°；tint 由**逐成员逐波长正 A·T 汇总**决定，不可从几何重合或字面代表推出。⚠️ schema2 已移除 v1 的 per-member brightness 行——**报告侧色偏表达是已知缺口**，现测量路径 = mono-λ 隔离渲染对（absolute EV + npy） | config `raypath_feature_rhombic_plate.json`；`raypath-feature-diagnostic-acceptance.md`（历史 fixture 数值与哈希） |
| C13 | `1-6-2`（Parry） | 非片晶族坍缩 | 机制 / 空态 | 折叠矩阵 S_x 整族坍缩；`family_pinned` 判据只覆盖 plate/Lowitz，非片晶坍缩未全覆盖（开放） | `raypath-analysis.md` §5.1.8 LI 对照注 |
| C14 | `1-2`（plate） | rank 0 | 空态 | D≡0、`point_mass`、落点=太阳位置、`target_separation_deg` 正确——不可复用普通空态文案 | config `raypath_feature_plate_target.json`；`raypath-cli-output.md` §3.10 |
| C15 | `1-3-6-2` | 恒等型秩 0 | 空态 | 相对平行侧面成对复合为恒等（六棱柱长度 ≤4 共 16 条秩 0 之代表） | — |
| C16 | 定向 `3-5`（column / Parry / Lowitz） | 交点 / 相切 / 整段重合 | 收敛 / 机制 | 柱晶 / Lowitz 以 pose 约束沿 fiber 的符号变号给离散交点、两个近并根 = 相切、Parry 用最小距离；u 球投影带仅**必要非充分**（柱晶 5 目标 3 假阳性）——判据必须在 pose 空间 | — |

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
> 无法锚定的结论显式标注。行内 C05/C06 的支撑/色散数字当前属于后一类（方法可复现，锚点待 fixture）。
