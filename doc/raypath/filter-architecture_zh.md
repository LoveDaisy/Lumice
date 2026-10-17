[English version](filter-architecture.md)

# Filter 子系统架构

本文档记录 Lumice filter 子系统的核心设计约束。
本文档是**规格说明**（spec）：所有实现必须与这里陈述的不变式对齐。
若实现细节与本文某条断言冲突，先修 doc（若 doc 有误）或修 code（若 code 偏移），两者不得静默分歧。

**目标读者**：负责实现涉及 filter 路由、晶体配置、多重散射传播或 C API 功能的贡献者，以及对照 Design A 做 code-review 的评审者。

---

## §1 绑定模型：Filter 属于 Scattering Entry

Filter 与晶体引用并列绑定到一个 **scattering entry**；crystal 与 filter 是两个独立身份轴：

```
entry  ──► crystal_id
       └─► optional filter_id
```

### JSON / 配置表示

`scene.scattering[].entries[].filter` 是语义绑定，不是单纯序列化细节。`ParseScatteringInfo`
逐 entry 解析晶体与可选 filter，组成一个 `ScatteringSetting`。两个 entry 可以引用同一晶体但使用
不同 filter，也可以只有一个带 filter；parser 不得按 crystal id 把这些绑定折叠。

### GUI 链接条目（Linked Entries）

当且仅当两个 entry 同时共享 `crystal_id` 和 `filter_id`，它们才是**链接的**。
GUI 在 ≥ 2 个 entry 构成此配对时显示 link 徽章。
链接组是**原子共享单元**：对任意成员的编辑必须在所有成员上可观察到。

编辑传播规则——自动传播 vs 需显式传播：

1. **晶体内容编辑**（在共享 pool slot 原地覆写）：自动传播。
   所有共享该 `crystal_id` 的 entry 通过 pool 间接引用自动感知更新。
2. **Filter 内容编辑**（在共享 pool slot 原地覆写）：自动传播。
   通过共享 `filter_id` 的 pool 机制，同 §1.1。
3. **Filter 添加**（`entry.filter_id: None → Some N`）：需显式传播。
   新 pool slot 默认只绑定到发起编辑的 entry；此前与该 entry 同处 `(cid, None)` 的链接兄弟条目也需将 `filter_id` 翻转为 N，否则链接组解体。
4. **Filter 移除**（`entry.filter_id: Some → None`）：需显式传播。
   链接兄弟条目也需清空 `filter_id`。

参见 `src/gui/gui_state.hpp` 的 `Linked group invariants` 权威注释块，以及
`src/gui/edit_modals.cpp` 的传播 owner `PropagateFilterIdToLinked`。

### 同一晶体，不同 Filter

core 配置允许两个 entry 复用同一 crystal id、同时引用不同 filter id（或其中一个没有 filter）。
它们共享晶体内容但不共享 filter 内容；由于 pair 不同，在 GUI 中也不构成 linked group。若晶体与
filter 的编辑都要彼此独立，使用 GUI 的「Unlink」或「Duplicate」分叉私有 pool slot；「Link to…」
则相反，会原子采用目标的 `(crystal_id, filter_id)` pair。

---

## §2 Filter 在 Simulation 中的角色（Design A）

Filter **参与 simulation 传播**。它们在 simulator 内部每个 ray-exit-crystal 决策点充当门控。

### 决策规则（每条 ray，每次 crystal 退出）

```
ray 退出晶体
    │
    ├─ filter 检查通过  ──►  ray 进入 outgoing / MS 传播
    │
    └─ filter 检查失败  ──►  ray 被 DROP（丢弃）
                             （既不作为 outgoing 发射，
                              也不传递到下一 MS 层）
```

`FilterSpec::Check(ray)` 在每个 outgoing-ray 候选点调用一次，使用当前 scattering entry 已解析的
filter。失败的 ray 不会出现在任何下游 buffer 中。

### 设计理由

将门控置于 simulator 端（而非 consumer / render 端）有两点好处：

1. **正确性**：filter 选择性在多重散射链的每一层都生效，
   物理正确的 ray 子集贯穿完整的晶体序列传播。
2. **性能**：高选择性 filter 在早期剪枝 ray 群体；剪枝效果在各层上叠乘（见 §3）。

---

## §3 多重散射（Multi-Scatter）语义

在多重散射（MS）配置中，每一层包含一个 `prob` 与一组 scattering entries。对被路由到 entry *i*
的 ray，决策三元组是 `(prob_layer, crystal_i, filter_i)`：

```
第 0 层：prob₀ × crystal₀ × filter₀
               │
         仅 filter₀ 通过的 ray
               │
第 1 层：prob₁ × crystal₁ × filter₁
               │
         仅 filter₁ 通过的 ray
               │
         ……
```

Filter 在 simulator 内部的**每个 crystal 退出点**进行评估。第 N 层当前 entry 的 filter 决定该
ray 是否有资格进入第 N+1 层。任意层 filter 失败都会丢弃该 ray：不再传播，也不进入输出 buffer。

Filter 选择性越高 → simulation 越快：幸存 ray 比例以 `∏ᵢ (pass_rateᵢ × probᵢ)` 的方式跨层叠乘剪枝。

---

## §4 Render 阶段：无 Filter 逻辑

Render / consumer 阶段**不执行任何 filter 操作**。

`RenderConsumer` 接收的所有 ray 在 simulator 内部已完成过滤。
Render 通道是纯投影和 XYZ 累积：

```
simulator 输出  →  RenderConsumer  →  投影  →  XYZ 累积
                                      （无 filter）
```

`RenderConfig` 不持有 filter 字段。此前的 `ms_filter_` 已在 219.4 恢复 Design A 时删除。
在 consumer 层应用 filter 是设计违规。

### 空批次契约（Empty-batch contract）

由于 filter 门控在 simulator 端执行，且可以合法地拒绝某个 `Consume()` 批次中的**全部** ray
（见 §7 分支表——低通过率 filter 出现「这一批零幸存」是正常的分布尾部事件，而非畸形状态），
`outgoing_d_` 与 `outgoing_w_` **可能同时为空而 `rays_` 非空**。`RenderConsumer` **必须容忍这一状态**：
当 `outgoing_w_.size() == 0` 时，拷贝、投影、累积循环全部退化为 no-op，`snapshot_intensity_` 保持为零。
consumer **不得**假设「`rays_` 非空 ⟹ `outgoing_*` 非空」；它唯一可依赖的尺寸不变式是
`sim_data.hpp` 记录的 `outgoing_d_.size() == 3 * outgoing_w_.size()` parallel-array 关系，
由 `RenderConsumer::Consume`（`src/server/render.cpp`）断言。

> 同一处曾有一条陈旧断言编码「`rays_` 非空 ⟹ `outgoing_w_` 非空」，导致只要某个过滤后的批次恰好
> 被清空，Debug 构建就会 `SIGABRT`。现已替换为上述尺寸自洽检查——因为它当年触发的「空批次」状态正是
> Design A 允许的。

---

## §5 数据流不变式

### 配置表示

每个 `SceneConfig.ms_.setting_[]` 都是一个 `ScatteringSetting`，同时携带已解析的 `crystal_`
与该 entry 自己的 `filter_`（`src/config/proj_config.hpp`）。源 JSON 复用 crystal id 不会合并这些
setting 或它们的 filter。

GUI 把 crystal 与 filter 放在两个独立 pool 中；`EntryCard` 同时保存两个 id。内容是否共享分别由
对应 id 决定，而 link 徽章及添加/移除传播使用完整 `(crystal_id, filter_id)` pair（§1）。

### Filter 检查入口

Simulator 中每个 outgoing-ray 候选均调用一次 `FilterSpec::Check(ray)`。
`FilterSpec::Create(config, crystal, axis_dist)` 由该 entry 的 filter config 与其引用的
crystal/axis 上下文构建 spec。

调用点位于 `src/core/simulator.cpp` 中的 `CollectData()` 函数——决策规则见 §2。

### RenderConfig 不变式

`RenderConfig` 不得持有 filter 字段。Filter 是纯 simulation 端关注点。

### EntryExit filter 的表达能力

`entry_exit` filter 有两个正交约束轴：

1. **结构轴**——entry face 与 exit face 各自可以是一个面号或 wildcard。
2. **长度轴**——并行 `RaypathRecorder::size_` 的可选最短/最长边界。

core schema 直接表达一个原子谓词：

```cpp
// src/config/filter_config.hpp
struct EntryExitFilterParam {
  std::optional<IdType> entry_;     // nullopt = wildcard
  std::optional<IdType> exit_;      // nullopt = wildcard
  size_t min_len_ = 1;              // path-length lower bound
  std::optional<size_t> max_len_;   // nullopt = no upper bound
};
```

`EntryExitSpec::Match`（`src/core/filter_spec.cpp`）先检查 recorder 长度，再按已设置的端点读取
recorder 的首/末面；两端都是 wildcard 时只由长度边界决定，`size == 0` 始终在读取端点前拒绝。

多值与组合语义位于 GUI → Scene 翻译层。GUI 把 filter 存成 sum-of-products 行：一行是多个
raypath / EntryExit factor 的 AND，多行互为 OR。EntryExit factor 使用
`entry:<faces> & exit:<faces> & len:<bounds>` 小语法；entry/exit 中逗号分隔的面号是该 factor
内部的 OR，空 entry 或 exit 是 wildcard。

`src/gui/file_io.cpp` 的 `ExpandSopToClauses` 展开 EntryExit 面号的笛卡尔积及各 factor 的备选，
`ExpandFilterToScene` 再把原子项注册成 simple filter，并在需要时注册一个 `ComplexFilter`
组合。由此 core 的 `EntryExitFilterParam` 始终只表示单 entry / 单 exit 原子谓词，而 GUI 可表达
raypath 与 EntryExit 的混合 AND 及多行 OR。编辑面位于 `src/gui/edit_modals.cpp`，语法的单一
owner 是 `src/gui/raypath_segments.hpp`。

JSON 保持向后兼容：旧的 `{"entry": 3, "exit": 5}` 仍解析为 entry=3、exit=5、
`min_len=1`、无上限；缺失或显式 null 的 entry/exit/max_len 映射为 `nullopt`。

## §5.1 设备侧 Filter Match（Metal 与 CUDA）

早期设计认为 filter 求值依赖 host 侧晶体配置，不能移到设备；隔离的 device-match parity 探针推翻了
这个前提。首个 Metal 实现把 raypath filter matching 的 `ReduceBuffer` 核移植到 MSL，并用全部真实 filter
类型对照 host：144 万次逐 ray 检查零 mismatch，且没有可测的寄存器压力代价。

当前共享路线以 `src/core/device_filter_desc.hpp` 为两种 GPU backend 的 host descriptor 权威：
Metal 由 `src/core/metal/lumice_trace.metal` 的 `DeviceFilterCheck` 求值；CUDA 读取相同字段，
由 `src/core/shared/filter_shared.h` 的共享 matcher 求值。CUDA 的 fused emit gate 在设备上检查，
final-layer host drain 仍调用 `FilterSpec::Check`；`test_cuda_filter_parity.py` 同时覆盖两条分支。
CPU 直接调用 `FilterSpec::Check`。

各 filter 类型的设备可行性：

| Filter 类型 | 设备 match 机制 |
|-------------|-----------------|
| `None` | 直接通过 |
| `Direction` | 立体角比较 |
| `Crystal` | crystal id 比较 |
| `Raypath` / `EntryExit` | `ReduceBuffer` + `GetFn` 表 + 字节比较 + 长度边界 |
| `Complex` | 上述原子谓词的布尔组合 |

关键分解是：orbit **构造**依赖 config，只在 host 每 session 做一次；逐 ray orbit **匹配**只是有界
整数核，可移植到设备。Metal 与 CUDA 的 fused device 路径都在 ray 离开晶体的 emit gate 执行
prob 决策与 filter 门控，filter-fail 不写 continue 或 exit 输出，从结构上保持 Design A 的跨 MS
终止语义；CUDA 的 final-layer host drain 是上文已点名的另一条执行臂。

---

## §6 历史脉络

### task-200（query-filter-uplift-v2，2026-05-18）——路由决策：已回退

**目标**：将 filter 从 simulator 端迁移到 consumer 端，使 filter 实时编辑无需重跑 simulation。

**实现方式**：`config_manager.cpp:194-225`（已在 task-revert-filter-to-simulator-side / 219.4 中删除）
新增了 post-parse 自动绑定，将 `scattering.entries[].filter` 复制到 `renderer.ms_filter_`，
激活了 `render.cpp` 中此前休眠的 `FilterRay` 路径。
在 simulator 中，filter 被降级为纯 MS-branch gate（filter 失败的 ray 仍作为 outgoing 发射）。

**发现的问题**（scrum-prob-zero-leak / #219，2026-05-22）：

1. 预期收益——"filter 编辑跳过 simulation"——在 GUI 中从未实现。
   修改 filter 仍会触发 `MarkFilterDirty → 全量 sim 重跑`。
2. task-200 代码路径发现三个实现 bug：
   - Post-parse 平铺操作在多层 MS 配置中丢失了 per-entry filter 绑定。
   - 晶体上下文在配置解析时被冻结，导致 live 晶体更新无法到达 consumer 端 filter。
   - 配置绑定器中的 chain-walk 边界条件在某些 MS 配置下导致索引越界。
3. Consumer 端过滤效率更低：filter 选择性不再在 simulation 期间剪枝 ray 群体，
   失去了多层叠乘剪枝的性能收益（§3）。

**决策**：回退到 Design A（task-revert-filter-to-simulator-side，219.4）。

### scrum-210（filter-architecture-refactor，2026-05-19）——保留

scrum-210 引入了 `FilterSpec` 多态接口以及基于 canonical-form / orbit 的 raypath 匹配方案。
这是**纯算法层改进**，与路由决策正交。
task-revert（219.4）迁移 filter 调用点（consumer → simulator），但不触动 `FilterSpec` 算法接口。

保留组件：`src/core/filter_spec.{hpp,cpp}`、simulator 中的 per-batch `specs_table` 调用、scrum-210 测试套件。

### scrum-prob-zero-leak / #219（2026-05-22）——Design A 基线

本 scrum 审计了 task-200 的各 bug，评估了回退方案，将 Design A 确定为权威路由模型。Off 模式重设计作为 backlog 条目跟踪。

### scrum-237（remove-anchor-lane，2026-05-28）——移除 anchor lane

移除 F1 anchor lane，回退到每帧基于可见 framebuffer 的 self-P99.5 EV 归一化。filter-fail 射线现在在 `CollectData` 中立即终止（Design A 语义），消除了 multi-scattering ~2× 性能税。ABI break：`LUMICE_RawXyzResult` 移除 `anchor_p995_y` 与 `anchor_snapshot_intensity` 字段。`test_partition_buffer_additivity` 测试已删除。

---

## §7 CollectData 分支表（Design A）

`src/core/simulator.cpp` 中的 `CollectData()` 对 outgoing 候选射线实现以下分支表：

| filter | prob | 结果 |
|--------|------|------|
| pass | pass | `IsContinue()` —— 下一次 MS 散射 |
| pass | fail | `IsOutgoing()` —— emit |
| fail | — | 射线终止（`w_` 置负；既不 outgoing 也不 continue） |

filter-fail 射线不携带任何特殊标记——复用 `w_ = -1.0f`（TIR sentinel）以从 `IsOutgoing()` / `IsContinue()` 中排除。它们仍保留在 `all_data` 中（语义类似 TIR），但永远到达不了 consumer 的 outgoing accumulator。

### 当前 relative exposure 锚点

Filter 终止会改变实际落地的 ray，但不会让 relative exposure 获得一套私有测光规则。当前 mono
relative scale 锚到场景天空：

```
scale = intensity_factor × TargetWhiteToLinear(135)
        / (axis_solid_angle × anchor_l99_sky)
```

`anchor_l99_sky` 是 session 固定全天等面积锚平面上的天空辐亮度 P99，与 lens、FOV、view、
`visible` 和输出分辨率无关；`axis_solid_angle` 把辐亮度换到当前 renderer 每像素积分辐亮度的单位。
server/CLI 公式由 `RenderConsumer::ExposureScale` 单一持有。GUI 从 `LUMICE_RawXyzResult` 读取这两个
字段，经 `LUMICE_ComputeEvAuto` 与 preview shader 的相对照度因子到达同一逐像素结果；两侧公式外形
不同，是因为 server buffer 与 GUI 源纹理的单位不同。

这是**当前契约**。§6 中的 self-P99.5 是 2026-05-28 移除 anchor lane 时引入、后来被替换的历史状态，
而不再作为现行规则。推导与迁移记录见 `doc/rendering/ev-pipeline-architecture.md` §2.8。

---

## §8 不变式自动化状态

已实施的每一条 seam 都有对应自动化：

- `test_simulator.cpp` 的 `CollectDataFilterDispatch` 用例钉住 CPU 三种分支，包括 prob 本会继续时
  filter-fail 仍必须终止。
- `test_render_consumer_empty_batch.cpp` 钉住下游契约：整批被过滤为空合法，
  `outgoing_d_` / `outgoing_w_` 尺寸不一致不合法。
- `test_device_filter_check_host.cpp`、`test_metal_filter_match_parity.mm` 与
  `test_cuda_filter_parity.py` 对照 host 语义检查设备 matcher 与 backend 路由；
  `test_ms_filter_leak.py` 是 filter-fail 不泄漏到后续 MS 层的全栈 regression sentinel。
- GUI 测试钉住共享 `(crystal_id, filter_id)` pair 的 entry 在 filter 添加/移除时原子传播。

这里有意**不存在**「同一 crystal id 的所有引用必须使用同一 filter」的 loader 断言：那会拒绝 §1
定义的合法 per-entry 状态。真正的义务是每个 entry 独立解析自己的 filter，以及凡被 UI 声明为编辑
linked pair 的操作，都要把 filter-id 变化传播到该 pair 的每个成员。

---

## 交叉引用

| 关注点 | 位置 |
|--------|------|
| 绑定模型、GUI linked-group 不变式 | `src/gui/gui_state.hpp` — `Linked group invariants`；`src/gui/edit_modals.cpp` — `PropagateFilterIdToLinked` |
| entry 级 crystal/filter 配置 | `src/config/proj_config.hpp` — `ScatteringSetting` |
| Simulator 端 filter 检查（Design A 门控） | `src/core/simulator.cpp` — `CollectData()` |
| 空批次 consumer 契约（Design A） | `src/server/render.cpp` — `RenderConsumer::Consume()` |
| FilterSpec 算法接口 | `src/core/filter_spec.hpp` |
| 设备 filter descriptor / matcher | `src/core/device_filter_desc.hpp`；`src/core/shared/filter_shared.h` |
| relative exposure 结果字段 | `src/include/lumice_engine.h` — `LUMICE_RawXyzResult.anchor_l99_sky` / `axis_solid_angle` |
| Filter JSON schema | `doc/configuration.md` |
| Raypath 语义、P/B/D filter 开关 | `doc/raypath/raypath-symmetry.md` |
| 自适应亮度 Off 模式、可加性 | `doc/rendering/adaptive-brightness.md` |
