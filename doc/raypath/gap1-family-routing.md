# 缺口①：测度→族路由 — 设计文档

> 状态：**已提升设计文档**（2026-10-20，随 715.1 受限族 rank 原语落地提升；原 2026-10-18 设计笔记的三轮
> 收敛记录与 §7 裁定记录保留原文）。rank 原语的规格权威即本文 §5：实现 =
> `src/analytic/family_rank.hpp`（Lumice-born，in-process；登记 =
> `doc/interfaces/analytic-api.md` §9 item 17）。设计上游的三个输入均为当时的工作笔记，此处改记为
> 机制与永久锚：两通道原则（结构路由与权重积分正交，消费面见 `raypath-analysis.md` §5.1 的
> survey 管线）；缺口清单（C01–C18 场景索引 = `doc/cases/raypath-analysis-cases.md`）；C10 探针事实
> （`Lumice raypath survey` v1（PR #508，main `01ed3796`）实测 C10：12 成员全部 `rank_zero=false`、
> mechanism=`jacobian+dimension_collapse`——证实全 SO3 秩判据不识别族上坍缩）。

## 1. 标签无关（owner 确认，2026-10-18）

- config/C API **不携带** plate/column/parry/lowitz 标签——`axis` 块只有每维
  `{type: fixed|uniform| gauss, mean, std}`（uniform 的 mean/std=区间中点/全宽）+ 两个缺省语义
  （`axis` 缺省=单姿态；azimuth/roll 缺省=自由均匀）。类型标签是 GUI 下拉框预设，不进序列化
  （688/689 两轮定下的既有事实）。引擎侧路由结构性无此输入，「标签理想值」方案不存在。
- kernel ABI 族枚举是**形状选择器**，标签语义为空（头文件原文：plate≡column、parry≡lowitz 只差
  LI 默认均值，C ABI 不应用默认值）——传形状+实际参数，无「归型取理想值」步骤。
- 「plate 标签 + random 参数」按参数路由（broad → 满维机器），与渲染实际行为一致；survey 报参数
  含义而非下拉框名义，对标签-参数打架的用户反而是正确诊断。
- 标签唯一合法残留 = **显示糖**（GUI 提示、输出描述词），derived description，永不反向参与路由。

## 2. 集中度三分：fixed / narrow / wide（owner 提出，2026-10-18；本文形式化）

路由说**集中度语言**，不说分布类型语言。每维从分布只提取三个量：**中心**（mean/中点）、**宽度**
（σ 或区间宽）、**有界性**（支撑是否紧）。分布类型新增不改变路由词汇——覆盖扩展性的机制在此。

- **fixed（宽=0）**：绝对分类；第 0 级即冻结，测度支撑降维。含 `axis` 缺省单姿态（第 0 级=0D，
  固定变换源像；权重单姿态 `EvaluatePath` 直算，不经 band-sum）。
- **narrow**：冻结候选（按宽度从紧到松排序）；是否真冻结由**逐对象存活筛**裁定。
- **wide**：永不冻结；full-uniform 与大 σ gauss 结构上同族（冻结型相同 ⇒ 映射结构相同），
  差别由权重通道携带。

**narrow/wide 是「维 × 对象」的属性，不是维的全局属性**：首刀按宽度排序，筛后同一维对不同对象可
异判（C16 形状实测：zenith 40° 冻结的像方 blur ≈10.4°——对 Lowitz fold 的结构截面（1.0° 地板）
判 wide、筛杀；对全天 δ 域结构（自身延展 ≈46°）判 narrow、存活）。

**有界性角色分离**：宽度进冻结阶梯；紧支撑进 `support_set` 的「密度支撑尾」边界标签。同一参数
两个用途，不混。

## 3. 阶梯（两通道原则的缺口①落地）

1. 第 0 级 = 实际支撑（fixed 维已冻结）→ 拥有 `support_set`（可达性不因集中而坍缩）；
2. 集中维按宽度从紧到松为冻结候选；uniform-full 与 azimuth 永不入选；第 k 级 = 冻结最紧 k 个，
   冻结值 = 参数中心，其余维携带实际 (中心, 宽度)；
3. 每级算 rank + 候选对象（坍缩点 / 受限 fold / 常规像）；T0 代数层（恒等证书）在阶梯之外不动；
4. 逐对象存活筛（structure proposes, weight disposes）：冻结宽度 × 映射局部敏感度 → 像方模糊
   vs 对象自身角尺度；通过者进 survey（带级标签），band-sum 地图为升级复核。

语料标定（场景定义与终态 = `doc/cases/raypath-analysis-cases.md`）：

| case | 测度 | 生效级 | 结果 |
|---|---|---|---|
| C10/C12 | zenith narrow(0.3°)、roll wide | 冻 zenith | 自旋族 rank0 → ±120° 坍缩点；第 0 级 support_set 并报 |
| C16 Lowitz | zenith 宽(40°)、roll narrow(0.3°) | 冻 roll | {zenith 实宽携带, az} 二维族 → Lowitz fold；再冻 zenith 被筛杀 |
| C14/C15 | —— | T0 代数层 | 阶梯之外（恒等证书先行） |

## 4. 覆盖矩阵与 v1 域（owner 确认：v1 不求覆盖面，留扩展机制）

两张矩阵分开：

- **结构侧**（阶梯/对象）：只需每维 (中心, 宽度, 有界) → 对分布类型几乎全覆盖；真正限制 =
  kernel 受限族入口（§5）。
- **权重/支撑侧**（band-sum 实际测度积分、支撑边界）：限 kernel 三形状（Haar / zenith-N ×
  roll-U / zenith-N × roll-N，az 恒均匀）。域外（az 集中、uniform zenith——含 full-uniform 也
  非 Haar、有界 uniform 的紧支撑语义）v1 = `refused(测度形状不在路由域)` 带具名原因。近似
  （匹配宽度 gauss 替代有界 uniform）= 将来的显式决定，不默认偷换（支撑边语义不同）。

扩 kernel 纪律：engine-kernel 语义对齐（先例：kernel zenith gauss 按 sinθ 归一与引擎采样一致）。

## 5. rank 原语（唯一 kernel 侧新算；已落地）

「指定自由维集合的族上 rank + 临界结构」：现有 `FiberResult.singular_values` 是全 SO3 基、
`TraceRestrictedFamilyCurve` 盖 1D 级、二维受限族无现成入口。形状裁决（2026-10-20 实现定案）：
**新 kernel 入口、in-process**（`src/analytic/family_rank.hpp`）——组合式（族上采样姿态 + 对自由维
有限差分）在 fold 边缘（σ→0 判据区间）失效且必与 1D 入口分叉；Jet 精确微分经生产 Euler 图表
R(a,θ,r)=Rz(a−π)Ry(−θ)Rz(r) 与 1D 语义同层互证；不暴露 C ABI、不 bump
`LUMICE_ANALYTIC_API_VERSION`（无 cross-ABI 消费者不发布冻结表面），v9 追加路径备而不用。

## 6. 口径正交

级选择（冻结型）与 L2 合并（C16 P3-not-P60、C14 {1-2} ensemble 对称）为两个独立判定，各从实际
测度来，互不推导。

## 7. 裁定记录

- **owner 已确认**：标签无关；集中度三分方向；v1 域边界 refused 处置；覆盖扩展走集中度词汇。
- **默认采用**（未反对即成立，异议随时翻）：逐对象筛后全报带级标签（非只报最深级）；局部敏感度
  筛为默认、band-sum 升级复核；口径正交。
- **owner 裁定（2026-10-19）——缺口① kernel 新算的出生地 = Lumice-born**。真身已核实收窄为「二维
  受限族的 rank + 临界结构入口」：1D family pinning 双边闭环（LI `focusing.py` `family_pinned`
  2026-09-29 立、10-07 `bc62a55` 泛化到 support axis，本仓 `dp_focus.cpp` `FamilyAxis` 已跟到泛化版，
  parity 锁定）。处置：规格权威 = Lumice 文档（本笔记随缺口①切片提升进 `doc/raypath/`）；
  `analytic-api.md` 登记为 Lumice-born 条目。LI 侧文章真需要时（约三四篇之后）可**临时并行** JAX 版，
  按 `doc/raypath/raypath-analysis.md` §5.1.6 原语层逻辑互为校验对象——届时两条纪律：parity fixture
  命名须标口径（L1/L2 教训：两个都合理但不同的实现被当同一件事对照 → 假红/假绿）；LI 版从 Lumice
  规格推导/对照，不独立重推。终态收敛按共享判据：LI 经 binding 消费 `liblumice_analytic`、JAX 版
  退役。发布前两仓并行本就是 de facto 状态；wave-2 hold 悬账随 analytic v9 +1，维持不动
  （2026-10-07 裁定不因新条目重开）。
