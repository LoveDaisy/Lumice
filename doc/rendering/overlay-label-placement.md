# Overlay Label Placement — curve-centric 设计与落地记录

> 状态：**as-built 设计记录**。curve-centric 模型于 2026-06-23 定案，commit `6844a5431`
> 首次实现，之后 placement 收敛到 core annotation 管线。下文的历史诊断与测量仍是现行机制的设计理由。

## 范围

坐标网格、地平线、太阳角距圈与视轴角距圈的 overlay 文字 label 放置。生产路径只有一个
placement owner：`src/core/annotation_overlay.cpp::ComputeAnchors`；GUI 通过
`LUMICE_ComputeAnnotationAnchors` 消费发布的锚点，再做外观转换、碰撞抑制与绘制。参考点 marker
复用同一套投影与可见性机制，但它们是点而不是曲线 label。Overlay **线**属于 renderer 的另一阶段。

## 实施前基线（boundary-centric 5-source）与缺陷

commit `6844a5431` 之前，GUI 侧 `ComputeOverlayLabels` 是 **boundary-centric**：沿若干边界曲线
采样，检测相邻样本间的网格值穿越，并在穿越处放置 label。它有五个 sample source：

1. `sample_viewport_edges` — 视口矩形四边（所有 lens）。
2. `sample_hemisphere_equator` — 赤道边界（`!full_sky` 且 visible∈{upper,lower}）。
3. `sample_front_great_circle` — 前半球大圆（`!full_sky` 且 front）。
4. `sample_interior_latitudes` — 纬度环 interior label（`!full_sky` 且 lens≠linear）。
5. `sample_sun_circle_interior` — 角距圈 interior label（`!full_sky` 且 show_sun_circles）。

**2026-06-23 的 512² 白盒探针（elev=20，visible=full）暴露 4 个独立缺口**：

| projection | 现状 | 根因 |
|------------|------|------|
| globe | 13 个 label 全是纬度、沿 0°经线弧，**零经度** | disc 在视口内 → 边采样无穿越；只有纬度专属 Source 4 兜底 |
| rectangular | 40 个全是纬度（左右边）、**零经度** | full_sky 只跑 Source 1；经线本应穿上下边，但上下边=极点方位奇异，被 `abs(az1-az0)<20` 守卫全拒 |
| dual_fisheye | **0 个 label** | full_sky 只跑 Source 1，两圆盘在方形视口内未命中有效穿越 |
| linear / fisheye | 覆盖够，但**边缘掠射处 label 重复成簇**（顶边 ~15 个重复） | 每个边采样穿越各产一个 label，掠射时一条线多次穿同一边 |

**病根 = boundary-centric**：沿边采样天然漏 disc 内曲线、被极点奇异坑、掠射处成簇。
单点补丁（给 globe 补一个对偶经度 sampler）只能解 1/4，且继续在 5-source 上堆特例。

## 设计：curve-centric 统一放置

反过来，以**曲线**为中心。对每条网格 level-set 曲线（altitude=const / azimuth=const /
sun_dist=const）：

1. 在世界系参数化 walk（扫描另一坐标，如 altitude 曲线扫 azimuth 0..360）。
2. forward 投影到屏幕像素。
3. 裁剪到**可见区域** = 视口矩形 ∩ 投影 disc 有效域 ∩ 半球（visible upper/lower/full + front）。
4. 求可见弧，**二选一 dispatch**：
   - **边界模式**（可见弧触可见区域边界 = 曲线穿出视野）：label 落边界交点。
     覆盖部分环 / 掠射 / 视口边裁剪等所有"穿出"情形。
   - **内部模式**（可见弧 = 完整曲线，没穿出）：label 落曲线 canonical 锚点，
     **锚点数/位置按曲线类型参数化**：
     - 角距环 → 4 个（绕 sun_dir 90° 间隔）
     - 纬圈 / 经线 / 地平线 → 1–2 个（canonical 参考位）

**统一概念**：`label = 网格曲线 ∩ 可见区域边界`；"可见区域边界" = 视口边 ∪ 投影 disc 轮廓
∪ 半球裁剪边。三者统一为同一"可见区域边界"，对应直觉上的三类锚点（特殊经纬值线 /
裁剪边 / 视口边）。

### 为什么一个机制同解 4 缺口

- globe / dual_fisheye 经度：经线 walk 会穿 disc 轮廓 → 边界模式产 label（原探针实测
  globe 经度 0→36/36，全有边界锚）。
- rectangular 经度：按经线**自身的 azimuth 值**在其视口边出口放 label，绕开"沿边采样
  在极点处算方位"的奇异。
- dual_fisheye：forward 投影后，曲线裁剪到两圆盘 → 有锚点。
- 成簇：每条可见弧只产一个边界锚；完整曲线只产该类型规定的少数锚点，不再是每个边采样
  穿越各产一个。

### 内部模式的一般性

"完整环在内"不止角距环：globe 正对极点（elev≈±90）时纬圈是 disc 内同心圆、地平线是
完整圈，同样触发内部模式。角距环的"4 label"只是它的内部锚点参数。现 Source 4/5 被
收编为两类曲线的内部模式，而非并列特例。

## 锚点取点规则（实施细化与复核修正）

边界模式"label 落边界交点"对**纬圈/角距环**直接取可见弧的 invis→vis 入口即可（这些
曲线的入口点彼此分离）。但**子午线（经度曲线）不能用通用入口锚点**：所有子午线都过极点，
而极点（alt=±90，az 无关 → `(0,0,±1)` 投影到同一像素）常落在可见区域边界上（globe /
dual_fisheye 的 disc 轮廓），导致每条子午线的入口都是那个极点汇聚点 → **全部经度 label
堆叠成一个像素**（draw 时 bbox 抑制后只剩 ~1 个）。这是 288.6 首版的退化缺陷，独立空间
分析（探针实测 globe 36 经度全在 (256,370)、dual_fisheye 全在 (384,256)）抓到。

**修正规则（owner 选定）**：经度 label 锚在子午线与**参考纬度环的交点**——优先地平线
alt=0（可见时），否则可见范围内最接近赤道的纬度（从 alt=0 样本向外搜首个可见样本）。
低纬度处子午线方位间距最大 → label 彼此分离。此规则**对所有 projection 统一应用**
（rectangular 等亦如此，更"在线上"），修复退化且不退化已正常的 lens。实测修复后 globe /
dual_fisheye 经度 x-span 114/256px → 227/384px，各点唯一。

## 当前实现

1. `src/core/annotation_overlay.cpp::ComputeAnchors` 构造一份投影/可见性 frame 并 walk 所有请求的
   曲线家族。所有 lens 都调用 `lm_proj::ProjectExitToPixel`；生产路径不再有 GUI 自己的 forward
   projection 或 curve walk。
2. `EmitCurveLabel` 在每条可见弧的 invis→vis 入口产一个锚。完全可见的纬圈或地平线退回一个
   canonical 可见样本；`WalkRingFamily` 为完全可见的太阳角距圈或视轴角距圈产至多四个 90° 间隔锚点。
3. 子午线使用上文 owner 选定的赤道优先规则，不走通用弧入口。
4. 当前曲线家族为 horizon、elevation、longitude、以太阳为中心的 angular-distance circle 和以
   lens center 为中心的 view-distance circle。具名天空 marker 作为点样本复用同一投影/可见性 frame。
5. GUI `annotation_anchors.cpp` 把 C API 结果转换成带类型的 anchor sets；`overlay_labels.cpp` 负责
   外观、视口 clamp 与碰撞抑制。因此 CLI 与 GUI 共享 placement 语义和 clamp 规则，但末端有意分叉：
   GUI 可抑制拥挤 label，CLI 绘制 core 交付的每一个 anchor。

## Screenshot 与导出边界

live preview 每帧发布一份 frame 参数与 label sets。**Screenshot** 从这份已发布状态出发，可通过一次性
选项删减家族，再把剩余 sets 交给 `RenderExportToRgba`。屏幕与 Screenshot 都到达
`RenderFrameContentToBoundFbo`；`test_gui_preview_export_parity.cpp` 从同一 snapshot 对两臂做 byte-exact
比较。该 gate 今天仍只在本地求值：`parity` tag 没有 CI job；其 1× test surface 也覆盖不了 Retina
DPI，显式 DPI 的单臂 export test 负责这条正交轴。

Dual Fisheye PNG 与 Equirectangular PNG 是 panorama export，不是 Screenshot 别名。它们调用
`RenderExportToRgba` 时不传 label sets，因此不含 overlay 文字。"导出包含 label"只对选择了 label
家族的 Screenshot 成立。

## 回归锚点

- Globe 与 rectangular 的经度 label、dual-fisheye 两个圆盘和掠射曲线去簇均由
  `test/unit-correctness/gui/test_overlay_labels.cpp` 覆盖。
- 完整角距圈的 90° 间隔锚和子午线赤道优先规则，是 `src/core/annotation_overlay.cpp` 内的结构性
  契约；修改时应保留曲线类型差异，而不是恢复 boundary sampler。
- 生产演进：curve-centric 初版 `6844a5431`；core annotation 地基 `5c678fa9b`；角距圈接入
  `92f75cf9a`；网格接入 `141526f73`；地平线接入并删除 GUI walk `0433e0dac`；preview/Screenshot
  共用 FBO 路径 `c8a364a7e`。

上文原始探针数字作为历史证据保留；其 ignored 工作文件不是当前契约的依赖。
