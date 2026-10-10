[English version](coordinate-convention.md)

# 坐标系与旋转约定

> **破坏性变更（v3）。** v3 版本重构了旋转链。来自旧版本、含 `crystal.axis.*` 字段的配置
> 渲染结果将出现方向偏移，可能需要按本文档下方的新约定（含 `azimuth` 符号约定与 `−180°` 链
> 偏移）重新编写。`filter.raypath` 与 `light.*` / `view.*` 不受影响。

本文档定义 Lumice 在晶体姿态、光源位置、相机视角等场景下使用的坐标系、轴约定与旋转链。所有数值示例默认以度为单位，弧度场景会显式注明。

## 1. 晶体局部坐标系

晶体局部坐标系随网格固定，跟随晶体一起旋转。约定为右手系：

- `N1`（1 号面，顶面基面）外法向 → 局部 `+z`（即 c-axis）
- `N3`（3 号面）外法向 → 局部 `+x`
- `N1 × N3` → 局部 `+y`（右手定则）

其余面由六棱柱对称性自然推出，详见 `src/core/crystal.cpp::FillHexFnMap` 中的法向表。

`Nx` 表示 `x` 号面的外法向单位向量（由晶体内部指向外部）。本文档中 c-axis 始终指 N1 方向。

## 2. 世界坐标系

世界坐标系固定且为右手系：

- `+z` 指向天顶
- `xy` 平面为水平面（halo 模拟中即为地平面）
- `+x` 是参考方位角方向（详见 §3）

世界坐标系是晶体姿态采样和相机视角共用的参考系。

## 3. 方位角约定

Lumice 使用**数学习惯**的方位角约定：

- `az = 0°` 对应世界 `+x`
- 方位角增加方向为绕 `+z` 自上而下俯视呈**逆时针**
- 与地理约定（北 = 0°、向东递增）相反

该约定统一适用于所有方位角字段：`crystal.axis.azimuth`、`scene.light_source.azimuth`、`render[].view.azimuth`。

## 4. 光源位置

光源方向由 `(altitude, azimuth)` 参数化：

- `altitude`（也称太阳高度角）∈ [0°, 90°]：0° 在地平线，90° 在天顶
- `azimuth` 遵循 §3 约定；`azimuth = 0°` 把光源置于 `+x` 方向
- `diameter` 控制日盘的角张度

`SampleRayDir`（在 `src/core/simulator.cpp` 中）发射光子方向**指向**观察者（即与太阳位置向量相反）；这是采样侧约定，对用户配置不可见。

## 5. 典型晶体姿态（世界坐标）

以下姿态是四个内置 axis preset 的特征姿态。每条描述固定**均值**朝向；实际采样朝向叠加 §7 中的 Gauss / Uniform 扰动。

### 5.1 Plate（片晶）

- `N1` 指向世界 `+z`（c-axis 垂直）
- 晶体绕 `N1`（即世界 `+z`）自由旋转

### 5.2 Column（柱晶）

- `N1` 位于 `xy` 平面（c-axis 水平）
- `N1` 绕世界 `+z` 旋转（由 `azimuth` 采样）
- 晶体绕自身 `N1` 自由旋转（由 `roll` 采样）

### 5.3 Parry

- `N3` 指向世界 `+z`
- 晶体绕 `N3`（即世界 `+z`，由 `azimuth` 采样）自由旋转；`roll` 锁定在 0° 附近以保持 `N3` 稳定向上

### 5.4 Lowitz

- `N3 × N1` 位于 `xy` 平面
- `N3 × N1` 绕世界 `+z` 旋转（由 `azimuth` 采样）
- `zenith` 带较大的 Gauss 扰动（默认 σ ≈ 40°），c-axis 围绕天顶大幅摆动——这一大 σ 是 Lowitz 视觉特征的来源，并非 chain 中的独立项

## 6. 旋转链

Lumice 对所有 preset 与自定义配置使用单一旋转链。给定一组采样 `(azimuth, zenith, roll)`（度），局部到世界的旋转矩阵为：

```
R(azimuth, zenith, roll) = Rz(azimuth − 180°) · Ry(−zenith) · Rz(roll)
```

链作用于局部向量时**自内向外**展开：

1. `Rz(roll)` 绕局部 c-axis（与世界 `+z` 重合于初始时刻）
2. `Ry(−zenith)` 绕世界 `−y`
3. `Rz(azimuth − 180°)` 绕世界 `+z`

`Rn(θ)` 是绕轴 `n` 旋转 `θ` 的标准右手旋转矩阵。

实现见 `lumice::BuildCrystalRotation(azimuth_rad, latitude_rad, roll_rad)`（`src/core/simulator.hpp`），其中 `latitude_rad = π/2 − zenith_rad`，对应内部球面采样使用的 latitude 约定。

## 7. Preset 默认采样参数

| Preset | zenith              | azimuth                | roll                    |
|--------|---------------------|------------------------|-------------------------|
| Plate  | Gauss(μ=0°, σ)      | Uniform [0°, 360°)     | Uniform [0°, 360°)      |
| Column | Gauss(μ=90°, σ)     | Uniform [0°, 360°)     | Uniform [0°, 360°)      |
| Parry  | Gauss(μ=90°, σ)     | Uniform [0°, 360°)     | Gauss(μ=0°, σ) locked   |
| Lowitz | Gauss(μ=0°, σ_L)    | Uniform [0°, 360°)     | Gauss(μ=0°, σ) locked   |

"locked" 表示 GUI 把分布类型固定为 Gauss，仅 σ 可由用户调整。默认 σ 取值参见 `src/gui/edit_modals.cpp::kAxisPresets`（当前 Plate / Column / Parry σ = 1°，Lowitz σ_L = 40°）。

各 preset 在运行时的视觉差异主要来自**分布形态**（Uniform vs locked Gauss、大 σ vs 小 σ），而非均值参数差异。Column 与 Parry 在默认下共享相同的 `(μ_az, μ_zenith, μ_roll)` 三元组，仅在 `roll` 采样方式上不同。

## 8. azimuth − 180° 偏移

azimuth 项的 `−180°` 偏移由局部坐标架选择 `N3 = +x` 决定。若不加偏移，Parry 默认 `(zenith = 90°, azimuth = 0°, roll = 0°)` 会把 `N3` 映射到世界 `−x` 而非 `+z`，与 §5.3 描述矛盾。

Parry 默认下的数值验证：

```
R = Rz(−180°) · Ry(−90°) · Rz(0°)

N1_world = R · (0, 0, 1)
         = Rz(−180°) · Ry(−90°) · (0, 0, 1)
         = Rz(−180°) · (−1, 0, 0)
         = (+1, 0, 0)             → +x

N3_world = R · (1, 0, 0)
         = Rz(−180°) · Ry(−90°) · (1, 0, 0)
         = Rz(−180°) · (0, 0, 1)
         = (0, 0, +1)             → +z   ✓
```

直观上，`−180°` 偏移把"配置中的 azimuth = 0°"对齐到"Parry 姿态下 N1 在 +x 侧"，符合用户在"`+x` 即光源方向"参考下配置晶体的自然预期。

## 9. 相机视角约定

相机坐标架独立于晶体坐标架，由 `render[].view` 下的 `(elevation, azimuth, roll)` 参数化。

### 9.1 朝向

相机在世界坐标下的 forward 方向为：

```
forward = (cos(elevation) · cos(azimuth),
           cos(elevation) · sin(azimuth),
           sin(elevation))
```

等价于：

- `elevation = 0°, azimuth = 0°` → forward = `+x`
- `elevation = 90°` → forward = `+z`（向上看）
- `elevation = 0°, azimuth = 90°` → forward = `+y`

实现见 `lumice::BuildViewMatrix`（`src/gui/preview_renderer.cpp` 及 core 中等价路径）。

### 9.2 方位与高度的符号约定

`view.azimuth` 遵循 §3 同一数学约定（自 `+z` 看 `+x` 起逆时针为正）。

`view.elevation`：正值表示向上看，`elevation = -10°` 表示稍微低于地平线方向。

### 9.3 Roll

`view.roll` 绕相机 forward 轴旋转。正值 roll 使得渲染图像在观察者眼中**逆时针**旋转（即相机局部 `+x` 朝局部 `+y` 旋转）。

### 9.4 与光源的关系

view 方位与光源方位采用**同一**方位角约定但相互独立。`light_source.azimuth = 0°`（光源在 `+x`）配合 `view.azimuth = 0°` 时，相机面向光源。

一个常见的 halo 截图约定是 `view.azimuth = 180°`，即相机背向光源、太阳在观察者身后；该约定与 §6 的 chain 约定完全独立。

### 9.5 字段语义不变

§6 chain 重构不影响 `view.*` 字段的取值与语义。已有 config / `.lmc` 文件中 `view.*` 字段保持渲染同样的相机取景。

### 9.6 等距柱状（`rectangular`）镜头：core 跟随姿态，GUI 刻意不跟随

owner 裁定，2026-09-02。两侧回答的是**两个不同的问题**，因此**都是对的**；本节存在的目的是让
下一位读者不必重推，也不要把其中一侧「修」成另一侧。

**core 侧**：等距柱状图跟随**完整**相机姿态——方位角**和**俯仰**和**滚转——走的是其余每种镜头
共用的同一个旋转。`BuildProjParams` 对**所有**镜头类型都用 `MakeCameraRotation(cfg)` 填
`ProjParams::rot`，`ProjectExitToPixel` 的 `kProjRectangular` 分支从单镜头族同一个相机系向量出发：

```
c = R^T · (−w)              // ApplyRotTranspose，与 linear/fisheye/globe 逐字相同
lon = atan2(−c.x, +c.z)     // RectangularForward(c.z, −c.x, −c.y)
lat = asin(−c.y)
```

**轴的分配值得单写一句**，因为「跟随姿态」有多个置换都满足，只有这一个保住了下面那条退化：
`+c.z` 是视轴，于是视轴落在图的正中；`−c.y` 是极轴——在 `Rz(−90°+roll)·Ry(90°−el)` 这半条链下，
相机局部 `+y` 指向世界天底，取负才把天顶放到图的上方；`−c.x` 是剩下的正交轴，**它的符号正是让
退化精确成立的那一位**。

2026-09-02 之前 core 把相机旋转压成一个标量 `az0 = atan2((R·ẑ)_y, (R·ẑ)_x)` 再从经度里减掉。
由于 `R·ẑ = (cos el·cos az, cos el·sin az, sin el)`，该标量恒等于 `az`——所以**改变俯仰或滚转
会得到逐比特相同的一帧**，这正是本次改动要修的。`az0` 字段已从 `ProjParams` 移除，无人读取。

**`el = roll = 0` 处的退化（为什么已发布的东西一张都没动）**：把 `el = 0, roll = 0` 代入
`R = Rz(az)·Ry(90°−el)·Rz(−90°+roll)`：

```
R = [[ sin az, 0, cos az],
     [−cos az, 0, sin az],
     [      0, −1,     0]]
```

于是取 `g = −w`：

```
c.x =  sin az·g_x − cos az·g_y
c.y = −g_z
c.z =  cos az·g_x + sin az·g_y
```

记 `g_x = ρ·cos φ`，`g_y = ρ·sin φ`：

```
lon = atan2(−c.x, c.z) = atan2(ρ·sin(φ−az), ρ·cos(φ−az)) = φ − az
lat = asin(−c.y)       = asin(g_z)
```

这与旧式**完全一致**（旧式 `RectangularForward(−w)` 给出 `lon₀ = atan2(g_y, g_x) = φ`、
`lat₀ = asin(g_z)`，再 `lon = lon₀ − az0`，而 `az0 = az`）。逐点相等由
`LmProj.RectangularAtZeroElevationAndRollReproducesTheAzimuthOnlyForm`
（`test/golden-analytic/core/test_projection.cpp`）断言——它把旧公式**逐字抄进测试**，
使这条等式在字段被删之后仍然可检验。

**唯一的例外是两个极点** `g = ±ẑ`：那里两侧 `atan2` 的两个参数同时为零，经度按等距柱状图的
本性就是任意的。旧式把极点放在 `−az` 列，新式放在视轴列；**行**（携带语义的那一半）不变。

由这条退化直接得到两个推论，无需额外论证：所有已发布的 `rectangular` config 渲染结果不变
（它们全都在 `el = roll = 0`）；GUI↔CLI 的导出对照也不需要新论证，因为 GUI 只会在零姿态下
导出这个镜头（见下）。

**GUI 侧**：预览保持一张**固定的全天纹理**，对它不做任何视图变换：`LensIsFullSky` 把
`rectangular` 与四种 dual-fisheye 归为同一类，`preview_renderer.cpp` 对该类设
`needs_view_transform = false`，而 `RenderPreviewPanel` **每一帧**都把该类的
`elevation / azimuth / roll` 钉为零。这是**职责划分而非遗漏**——GUI 侧所有相机姿态变换都是前端
的工作，在显示期通过重采样纹理完成；把姿态推进投影里等于做两遍。

由于那条每帧规则是**唯一的执行点**（`.lmc` 加载器、CLI JSON 导入器、个人默认值覆盖三者都是纯
转写，且四个 `renderer.*` 键全部是可作默认值的字段），它被从两个方向钉住：
`view_display_controls/an_arriving_document_cannot_smuggle_a_full_sky_pose` 覆盖三条到达路径
以及它们产出的导出 config，`view_display_controls/which_view_sliders_apply_depends_on_the_lens`
覆盖控件路径。

两侧的相互对照见
`VisibleMaskGuiParity.RectangularFollowsTheFullCameraPoseAndTheGuiIsAFixedTexture` 与
`AnnotationOverlayGuiParity.RectangularFollowsTheCameraPoseAndTheGuiDeliberatelyDoesNot`：
零姿态下精确一致，方位角下差一个纯水平位移，俯仰/滚转下差异出现纵向分量。

## 10. 持久化兼容

### 10.1 语义变化字段（破坏性）

- `crystal.axis.{zenith, azimuth, roll}`：§6 chain 改动后，相同数值会渲染出不同朝向。旧 `.lmc` / `config.json` 文件渲染结果会改变。

### 10.2 语义保持字段

- `filter.raypath`：面编号物理位置不变。`src/core/crystal.cpp` 中 `ref_norms[]` 未修改，`raypath = [3, 5]` 仍指向同一对物理面。
- `light.*`、`view.*`、`crystal.shape.*`、`filter.symmetry`：不变。

### 10.3 迁移策略

不提供自动迁移脚本。用户对照 §5 重新撰写 `crystal.axis.*` 字段。

- 确定性 axis 值（如 `axis.zenith = 90`），通常只需翻一个符号或 180°，肉眼对照渲染结果即可调整
- 概率分布（Gauss / Uniform）下分布形态不变，只是采样产生的渲染朝向变化

#### 朝向 delta 速查（确定性 axis）

下表对比同一 `(azimuth, zenith, roll)` 输入在新旧 chain 下 N1 / N3 的世界朝向，用于重写固定朝向配置时的快速核对。

| (azimuth°, zenith°, roll°) | 旧 N1 → world | 新 N1 → world | 旧 N3 → world | 新 N3 → world |
|----------------------------|---------------|---------------|---------------|---------------|
| (0, 0, 0)                  | +z            | +z            | +x            | −x            |
| (0, 90, 0)                 | +x            | +x            | −z            | +z            |
| (180, 90, 0)               | −x            | −x            | −z            | +z            |
| (90, 0, 0)                 | +z            | +z            | +y            | −y            |

观察规律：
- 当 `zenith = 0`（Plate 类）且 `roll = 0` 时，N3 沿 `±x` 翻转；若旧配置依赖 N3 指向 `+x`，新 chain 下需把 `azimuth` 加 180°
- 当 `zenith = 90`（Column / Parry）时，N3 在 `±z` 间翻转；如果旧配置想让"N3 朝天顶"（Parry 语义），新 chain 下在 `azimuth = 0°` 直接成立，而旧 chain 需额外调整
- 使用 `azimuth = Uniform[0°, 360°)` 全向采样的配置统计不变，不需要迁移

## 11. 验证

实现正确性由三层独立验证：

1. **数学层**：`test/test_simulator.cpp` 单元测试（`BuildCrystalRotation.CaseA_AzOffsetOnly` ... `CaseD_RollAroundCAxis`）以 4 条数学可分辨的输入分别探查 chain 各项
2. **结构层**：GUI 缩略图（`src/gui/thumbnail_cache.cpp`）展示 §5 中的典型 preset 姿态
3. **物理层**：E2E 与 GUI 参考图（`test/e2e/references/*.jpg` / `test/gui/references/*.png`）核验 halo pattern 在 chain 改动前后的稳定性。所有 axis 全方位均匀采样的配置在 chain 改动下统计不变，参考图无需重新生成

任意一层验证失败时，按以下优先级排查：

- `azimuth − 180°` 偏移符号（§8）
- 方位角符号约定（§3）
- 中层 `Ry(−zenith)` 符号（§6）

## 附录：chain 输出速查表

下表的 4 个用例数学可分辨，独立探查 chain 各项，对应 `test/test_simulator.cpp` 中的断言。

| Case | (az°, zenith°, roll°) | N1_world (= R · ê_z) | N3_world (= R · ê_x) | 验证目标             |
|------|------------------------|----------------------|----------------------|----------------------|
| A    | (0, 0, 0)              | +z                   | −x                   | az − 180° 偏移       |
| B    | (0, 90, 0)             | +x                   | +z                   | Ry(−zenith) 符号     |
| C    | (90, 90, 0)            | +y                   | +z                   | Rz(az − 180°) 非平凡 az |
| D    | (0, 0, 90)             | +z                   | −y                   | Rz(roll)             |

容差：`Dot3(N_actual, N_expected) > 1 − 1e-5`。

Preset 与 Case 的对应关系：

- Plate 默认（zenith = 0°）：与 Case A 等价（不计随机 roll / az）
- Parry 默认（zenith = 90°, roll ≈ 0°）：与 Case B 等价（不计随机 az）
- Column 默认（zenith = 90°, roll Uniform）：均值层与 Parry 相同；preset 差异在 `roll` **分布**上，不在 chain 输出上
- Lowitz 默认（zenith = 0° mean，大 σ）：均值层与 Plate 相同；c-axis 因大 σ 在天顶附近大幅摆动

## 13. 屏幕手性（渲染投影约定）

前面各节固定了**世界**坐标、azimuth 数学约定与相机 forward 方向，但**刻意没有**规定"azimuth 增大对应屏幕的哪一侧"。屏幕左右映射是一个**渲染呈现**层面的选择，只定义一次，并在所有渲染路径（CLI 直出图、GPU Metal/CUDA 后端、GUI 预览）上一致应用。core 模拟器（世界方向 + 旋转链）不受影响，仍遵 §3。

**约定：`右 = +az`。** 对面对太阳的 inside-out 镜头，azimuth 增大的天空特征在屏幕上**更偏右**。这是最符合用户习惯的方向，且 CLI 直出图与 GUI 预览**必须一致**（这正是在此统一约定的意义）。

- **单镜头族**（linear + 四种单 fisheye：equal-area / equidistant / stereographic / orthographic）：`+az → 屏幕右`。
- **globe**（option-B 外视角球面）：刻意**相反**，`+az → 屏幕左`。从球**外**看天球会相对 inside-out（裸眼）视角水平镜像，故 globe 与 inside-out 族相反侧是正确的，非 bug。
- **rectangular / 等距柱状**：`+az → 屏幕右`（等距柱状经度惯例），与单镜头族一致。

**实现**：屏幕 x 手性落在 `lm_proj::ProjectExitToPixel`（`src/core/shared/projection_shared.h`，legacy CPU / Metal / CUDA 三端共享的单一真源）的单镜头分支，故所有后端继承同一约定。GUI 的独立 forward 实现（`preview_renderer.cpp::ProjectWorldDirToScreen`、`overlay_labels.cpp::WorldDirToPixel`）本就产出相同的 `右 = +az`。

**回归守卫**：手性翻转对 forward/inverse 往返测试不可见（任何自洽约定下往返都恒闭合），故用**绝对屏幕左右符号**断言钉死：`test/golden-analytic/core/test_projection.cpp`（backend 绝对列 pin）+ `test/unit-correctness/gui/test_render_handedness_guard.cpp`（backend + 两条 GUI forward + 交互读回的跨实现对拍）。
## 14. 像素中心约定（前向分箱 ↔ 掩码 ↔ shader）

§13 固定的是「方向落在屏幕哪一侧」。本节固定它下面那个亚像素问题：**给定一个连续图像坐标，它是哪个像素？该像素的中心又在哪？** 这两半此前的答案相差恰好半个像素。

**约定：像素 `px` 覆盖连续区间 `[px - res/2, px + 1 - res/2)`，其中心在 `px + 0.5 - res/2`。** 图像关于画面中心对称，可寻址区间为 `[-res/2, res/2)`。

映射的两个方向都由这一句话推出，且各自只有一个 owner：

- **正向**（天空方向 → 像素索引）：`px = floor(v + res/2)`，落在 `lm_proj::ProjectExitToPixel`（`src/core/shared/projection_shared.h`）每个分支末尾的分箱处。`LM_FLOOR` / `LM_FN` 使该头文件被三后端共同编译，故 legacy CPU / Metal / CUDA 不可能在这件事上分歧。
- **反向**（像素索引 → 天空方向）：在 `px + 0.5 - res/2` 处取样，由渲染域掩码（`src/core/lens_proj_build.hpp` 的 `mask_detail::PixelToWorld` 及各族 helper）与 GUI 预览 shader 各自独立实现——后者的 `pos = v_ndc * u_resolution * 0.5` 就是同一条对称约定在 NDC 下的写法。

两者精确复合：把像素中心代入正向得 `floor(px + 0.5) == px`（对任意整数 `px` 恒成立），⇒ 往返回到出发的那个像素，不留任何余量。

**它取代了什么**：前向分箱此前是 `floor(v + res/2 + 0.5)`，可寻址区间成了 `[-res/2 - 0.5, res/2 - 0.5)`——不以画面中心对称，且与掩码 / shader 所设的约定差半个像素。它造成的后果全部是亚像素级、单独看没有一条像是错的，这正是它长期存活的原因：往返会滑一格；恰好落在成像圆边缘的方向可能一侧判进、另一侧判出；dual-fisheye 圆盘最外圈会被压成宽度只有正常像素千分之几的薄片。

**消费这条约定时**：core 给出的是**像素索引**，不是连续坐标。任何要拿 core 的答案与连续位置比较的地方——GUI 的 forward、反投影、overlay 锚点——必须先换算，取该像素的**中心**（图像空间的 `index + 0.5`）。拿裸索引去比连续坐标，量到的是截断而不是投影；在旧的 `+0.5` 分箱下这个类别错误被藏住了，因为那时索引恰好是连续坐标的 *round*，残差是对称的半像素。

**还有第三对配对：GUI 的源纹理 gather**。上面两条 bullet 是同一个映射在**渲染域**（正在产出的那一帧）上的两个方向。这条约定还有第三处承重点，且因为它既不是目标镜头的正向、也不是它的反向而特别容易被漏掉：GUI 预览重投影的是一张由同一套分箱产出的**全天源纹理**，所以 shader 读这张纹理时也必须按同一条约定读。这个读取点是 `src/gui/preview_renderer.cpp` 的 `dualFisheyeToUV`，规则是 `uv = pixel / tex_res`——就是这一个除法，**不加** `+ 0.5`。

原因是 GL 自身的纹理寻址本来就是按这套话说的：采样点落在 `uv * tex_res == px + 0.5` 时命中纹素 `px` 的中心，与上面那条约定是同一句话。所以把一个已经用 core 分箱约定表达的坐标直接交给 GL 就够了，先加半个纹素等于把这次居中**应用了两遍**。它造成的失效不是「读错格子」——它把每个片元挪到纹素角点上，于是每次采样都变成一次 2×2 双线性平均。整帧一律变软在没有对照物时是看不出来的，但 dual-fisheye 源纹理恰好提供了对照物：两个圆盘把 `y_norm` 映射到 `fx` 的符号相反，于是同一个像素偏移被映射成**相反的**天空方向，±5° 赤道重叠带里融合的是两片不同的天空，显形为地平线上的一条横向拖影带。提交 `4d9e3643` 写下那个 `+ 0.5` 时是对的——它配的是旧的 `floor(v + 0.5)` 分箱；提交 `f1bf1e9e` 换掉分箱、同步了上面那对渲染域配对，但没有同步这第三对，两侧由此失配。

**回归守卫**：`test/golden-analytic/core/test_visible_mask.cpp` 里两条对前向与掩码自带反向做**精确相等**往返的用例（`GlobeInverse.RoundTripsAgainstTheForwardGlobeBranch`、`RectangularInverse.RoundTripsAgainstTheForwardRectangularBranch`）。它们要求原样拿回出发像素、零容差——这正是半像素偏心一旦重现就会变红、而不会被某条容差静默吸收的原因。

第三对配对有自己的守卫，在 `test/gui/functional/test_preview_dual_fisheye_gather.cpp`。它不可能写成 C++ 往返——gather 活在 GLSL 里，而在测试里镜像一份该公式只会让断言自证自身。它改为直接跑真实 shader，并把源格式的 `r_scale` 置为 1：这使显示侧反投影与 gather 侧正投影成为同一投影、同一尺度，整条链塌缩成一个恒等映射——画布像素 `(col, row)` 必须读到源纹素 `(col, row)`。再以单纹素棋盘直接测这条约定：锐利即表示每个片元只读了一个纹素，塌成一片平坦中灰即表示它平均了一个 2×2 块。注意它的覆盖边界：它检测的是**所有像素共有的系统性偏移**，也就是约定失配本身的形态。一个把内容整体搬走却不使其糊化的投影错误（旋转、镜像圆盘）会保持棋盘锐利、在这里通过；那类问题由 `lens_proj` 参考图与 `test_visible_mask_gui_parity.cpp` 覆盖。

## 15. 方向向量语义（travel 与 position）

§2–§3 固定的是**位置类**量的世界系。本节是**方向向量**的唯一权威——即流经 C API 的单位
3 向量（`cone_center`、`incident_direction`、`target_direction`、`outgoing_direction`、
`LUMICE_UnprojectPixel` 的返回值），以及它们与用户在 config / CLI / GUI 取点读数里写的
天区点标签（`--center`、`--target`）的关系。

**两种单位向量，同一个系。** 都在 §2 世界系（+z 天顶、方位角按 §3）；差别只有一个取负：

- 天区点 `(alt, az)` 的**位置**——指向它的向量：
  `pos = (cos alt · cos az, cos alt · sin az, +sin alt)`；
- 从该点**来光**的 travel 方向——出射光线被显示在该点时携带的传播方向：
  `AltAzToDir(alt, az) = −pos = (−cos alt · cos az, −cos alt · sin az, −sin alt)`。

这同时调和了两句此前读起来矛盾的话：「zenith is z = −1」（`lumice_engine.h` 的方向契约）与
「+z 是天顶」（§2）。前者说的是 travel 方向——从天顶来的光竖直向下传播，`z = −1`；后者说的
是位置向量。同一系、两类量、差一个取负。具体地：`LUMICE_UnprojectPixel` 返回 travel 方向；
渲染的 `ProjectExitToPixel` 吃光线出射传播 `w`，落在 travel 方向为 `−w` 的那个像素上。

**显示规则。** 出射传播为 `w` 的光线显示在（能量累积在）它「所来之处」：
`position(display) = −w`，即标签 `DirToAltAz(w)`（`alt = asin(−z)`、
`az = atan2(y, x) − 180`——这里的 −180° 只是把 travel↔position 的取负抵消回去；与 §8
晶体链的 −180° 偏移是两回事）。因此偏离太阳入射方向 `δ` 的晕光显示在距太阳标签 `δ` 的
标签处——测试里所有 oracle 针对的正是这个用户可见几何。

**谁填什么。** `src/util/sky_direction.hpp`（`AltAzToDir` / `DirToAltAz`）是标签 ↔ travel
方向换位的唯一实现。每个 light-travel 约定消费者的填充规则：

| 字段 | 正确填充 | 取负后会查询 |
|---|---|---|
| `LUMICE_RaypathAnalysisRequest.cone_center`（CLI `--center`、GUI Point 模式） | `AltAzToDir(P)` | P 的对跖点 |
| 单光路 `target_direction`（CLI `raypath --target`） | `AltAzToDir(P)` | P 的对跖点 |
| `incident_direction`（`SunIncidentDirection`） | `AltAzToDir(sun)` | 反日点处的光源 |

为什么锥中心填「位置的取负」选中的恰是 P 本身：`ConeMembership` 拿中心与出射传播点积，
而显示在 P 的光线传播方向就是 `AltAzToDir(P)`——于是
`dot(AltAzToDir(P), w) = cos(angle(P, display(w)))`。同一恒等式使 `AltAzToDir(P)` 成为正确
的 fiber 目标：kernel 寻找传播方向等于所给向量的出射，而那些正是显示在 P 的出射。钉住
这件事的绝对位置测试（`test_cone_absolute_sky_position_at_non_zero_altitude`、
`test_cone_azimuth_is_not_mirrored_on_a_fixed_pose`、
`test_target_absolute_sky_position_at_non_zero_altitude`）用 22° 晕与手算几何当 oracle——
正因为自洽性断言在这些约定（包括错的那些）下全部通过。

**外部工具传位置向量时。** 自家 API 说位置向量的消费者（Lumice Integral 的
`sun_direction(alt, az)` 是 `pos` 而非 `−pos`）在填引擎方向字段前必须取负，读引擎 travel
方向当 bearing 用时必须把 180° 加回去。迄今大多数混淆来自：拿 LI 的方位角（位置 bearing，
LI kernel 的 target 是位置）与引擎 CLI 标签（也是位置 bearing）对比时，忘了两个 kernel 的
target **字段**分别落在 travel/position 取负的两侧——标签相同，交给 kernel 的向量种类不同。

## 16. 镜头落域（每 family 的全天空覆盖）

§13/§14 修的是方向**在画布上**落哪。本节修的是一切「图像 ↔ 方向域统计」互检底下的问题：
**每个镜头 family 究竟把哪些方向落到画布上**——因此一张图的能量份额在被拿去与全天空
数字（analysis 的 `total_energy`、链表份额、`mc_evidence`）比较之前，语义是什么。

**份额域声明。** analysis 运行（`Lumice analyze`、`--roi sky`）不投影、逐出射 segment
全计入：它的链份额活在**全天空域**。渲染帧只累计被镜头沉积进像素的 segment：它的份额
活在**镜头落域**。两者分母不同是**设计使然**；比较它们要么走同域互检（两侧同取 cone
ROI），要么带上下面各 family 的落域分数。

**每 family 落域**（均匀全天方向的落域分数）：

- **Rectangular**：1.0——`lon` 模 wrap、`|polar| < 1` 的方向 `lat` 全部留在画布内。
  精确极点（`|polar| == 1.0f`）坐在这张图的 f32 刀刃上：`py = floor(-asinf(polar)·scale + H/2)`
  在 `lat = ±π/2` 处灾难性相消，平台 `asinf(±1.0f)` 的末位 ulp 决定落最后一块画布行
  还是画布外一行（被帧侧 bounds check 静默丢弃）。Apple libm 把 π/2 向下舍、落界内；
  glibc x86_64/aarch64 与 MSVC 向上舍、丢弃——同一 fixture 跨 CI 实测（run
  38034154651：Ubuntu x86_64 / Ubuntu ARM64 / Windows MSVC 红、macOS 绿），并在
  macOS 上把 `lat` 朝 glibc 方向拨 1 ulp 复现了丢弃。离极点 δ 弧度的方向以 δ·`scale`
  像素的边距落盘，所以刀刃是输入方向的一个测度零集合，不是邻域效应（均匀球面集
  `|z| ≤ 0.9999975` 仍有 0.023 px 的行边距）。全天空 rect 帧与全天空
  analysis total 一致到 float 求和噪声（实测相对 4e-8；该残差是逐像素 double 累加
  在发布时一次性收窄到 float 的机制——`src/server/render.hpp`/`render.cpp`——能量
  恒等式测试的容差把它形式化），这使该 family 成为能量对账审计的校准臂。
- **Dual-fisheye**：方向→圆盘映射 1.0（每半球折到一个圆盘，`rho² = 1 − |z| ≤ 1` 严格
  成立），**除下述实测缺陷外**。
- **单鱼眼（fov 180）方形画布**：内切圆覆盖前半球（`rho² = 1 − cz ≤ 1`）。90° 之后
  （`cz < 0`、`rho > 1`）`kFisheyeEqualAreaMinCz = −1+1e-3`（对跖点数值下限）仍放行到
  天顶角 ≈ 177.4°——但放行不等于落盘。EAR 前向映射是方位保持的（像点半径
  `rho · short/2`），方形画布的 bounds check 会再裁一次方位：超盘的放行方向中只有
  对角扇区方位（`max(|cos φ|, |sin φ|) ≤ 1/rho`）落进画布，其余方位投出画布缘被
  静默丢弃。落盘方位份额从盘缘的全体方位（`rho = 1`）收缩到半对角处的零
  （`rho = √2`），所以该 family 的均匀全天落域分数严格小于 1.0——单鱼眼方形帧
  **不是**全天空校准臂。近盘缘带（`rho ≈ 1`、绝大多数方位落盘）上「一张图的角落
  是对侧视向的地平带」仍近似成立：仰视帧与俯视帧各自把这条带沉积一次，两图份额
  之和大于 1（参考场景实测 1.121）。这是**显示域语义**——每张图就是「这个
  视向的画布所见」——不是能量泄漏：单帧内没有像素重复收能，曝光锚也不读帧。

**已知缺陷（实测，撰写本文时未修）：dual-fisheye 折叠边界丢失。** travel 方向在
float32 里舍入到 `wx == −1.0f` 的 segment——地平太阳场景下的直穿类光路（平行面透射
与低偏向侧入射链；22° 晕本身偏 22°，不受影响）——投影为 `sx = +1.0f`、`z_hemi = 0`、
`x_norm = +1.0f`、`fy = cy + r = 128.0f`、`py = 128`，恰好画布外一行，被
`ProjectAndClassifyRay` 的 bounds check（`src/core/lens_proj_build.hpp`）静默丢弃。
参考场景上这删掉 5.98% 的 segment、18.9% 的帧能量（实测：帧 sumY 与 analysis total
之比 = 0.8107；均匀方向集实测恰为 1.0——所以任何用一般方向集的合成单测都看不见它）。
`analyze --roi frame` 逐位复现同一数字，因为两条路共享同一投影谓词。修复落地前，
**dual-fisheye 帧的总能量已知会按场景依赖的方式少计直穿类能量**，任何归一化常数都
吸收不了；与渲染臂分母比较的 `mc_evidence` 份额字段继承此注意事项（见
`doc/raypath-cli-output.md` §7）。

**互检规则。** 图像↔analysis 份额唯一合法的比较是**同域**：两侧同取 cone ROI
（analysis 的 `--roi cone`，与图像经同一投影的逆映射在锥内像素上的积分）。矩形
全天空帧是平凡的特例——它的落域**就是**全天空（上文 Rectangular 条），这正是它
能当校准臂的原因。对全部鱼眼族，即便上述缺陷修好，拿帧份额对比全天空 analysis
份额仍是范畴错误——单鱼眼的角落与方位裁剪语义单独就决定了这一点。
