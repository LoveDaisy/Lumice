# 设计讨论：C API 的层次混杂与产品线扩张方向

> 性质：**设计讨论记录**，不是定稿规范，也不是产品路线图。记录一次 owner 与 assistant 的架构讨论：
> 起点是"要不要把 core 和 gui 拆成两个仓库"，结论是这个问题问错了层次，真正的发现是
> **今天的 C API 把三个不同高度的东西混在了一个平面上**。
> 关联：`doc/c_api.md`（如何*使用* API）、`doc/capi-lifecycle-architecture.md`（C API 的内部不变量与生命周期状态机）、
> `doc/architecture.md`（as-built 三层：Config / Server / Core）、`doc/seam-design.md`（host/device seam，另一条正交的缝）。

---

## 0. 缘起

当前代码基本分为 core 与 gui 两部分，且边界是真实的：`src/gui/` 对 `core/`、`config/` 的
`#include` 数为 **0**，产品可执行文件只链接 C API 库（`src/gui/CMakeLists.txt:125`），
并由 `scripts/check_policies.py` 的 API boundary 规则加门禁。于是自然会想到下一步：
把 core 拆成独立仓库，让它更专注仿真本身与效率，前端（GUI / CLI / 其他程序）随便换。

讨论的结论是：**这个动作解决不了它想解决的问题**，因为想要的解耦性质已经成立；
而当扩张方向被摊开之后，暴露出来的是另一个更深的问题——今天叫 "core" 的东西，
以及暴露它的 C API，其实是三个高度压在一起的产物。当前只做冰晕仿真这一条产品线时，
这个混杂完全看不出来；一旦出现第二条产品线，它会立刻变成阻塞点。

---

## 1. 核心发现（一句话）

**当前 C API 不是"光线追踪引擎 API"，而是"冰晕仿真应用 API"，中间还掺进了一批为
GUI 编辑器服务的 schema 助手函数。三个高度共用一个平面，没有任何标记区分。**

---

## 2. 证据：三个高度，一个平面

> 本节及 §8 引用的 `lumice.h` 与行号是 2026-08 的证据快照——该头文件此后已拆为六个能力头并删除（§9），行号不再对应任何文件。

| 高度 | 性质 | C API 中的代表（`src/include/lumice.h`） |
|---|---|---|
| **L0 引擎** | 与冰晕无关的通用能力：几何、求交、折射、采样、后端调度 | *C API 中没有独立出口*——只能透过 L1 间接触及 |
| **L1 冰晕域模型** | 晶体族、光路、朝向分布、天空角空间投影、filter | `LUMICE_SceneAddCrystal`:717、`LUMICE_SceneSetLightSource`:744、`LUMICE_SceneAddScatterLayer`:730、`LUMICE_SceneAddFilter`:721 |
| **L2 编辑器支撑** | 服务于"有一个图形界面在编辑这份配置"这件事，与仿真执行无关 | `LUMICE_ShapeScalarSyncKeyName`:1136、`LUMICE_AxisScalarKeyName`:1170、`LUMICE_ShapeIndicesKeyName`:1152、`LUMICE_ShapeWedgeAngleKeyName`:1146、`LUMICE_IsLegalFace`:1113、`LUMICE_IsShapeScalarApplicable`:1123、`LUMICE_ValidateRaypathText`:1186、`LUMICE_GetCrystalMesh`:1091、`LUMICE_MaxFov`:1209 |

L2 那一组最能说明问题：其中四个函数的返回值是 **JSON 键名字符串**。它们存在的唯一理由，
是让编辑器与序列化格式共享单一权威（见 PR #230）——这是一个好设计，但它是
"编辑器与配置格式之间的契约"，不是"引擎对调用者的契约"。今天它和
`LUMICE_CommitScene` 并排躺在同一个头文件的同一个平面上。

源码侧同样是混的。`src/core/` 目录里 `geo3d` / `geo3d_closedform` / `math` / `optics`
偏 L0，而 `crystal` / `raypath` / `lat_lut` / `scatter_accum` / `projection` / `filter_spec`
是 L1；它们共处一个目录、编进同一个 object library、通过同一个 API 暴露。

### 2.1 一个具体的窄化例子：光源

`LUMICE_SceneSetLightSource(scene, sun_altitude, sun_azimuth, sun_diameter, spectrum)`
（`src/include/lumice.h:744`）——光源在 API 层就是"方向 + 角直径"，**没有位置**。
这不是疏忽，而是无限远光源假设的正确表达；但它把这个假设固化在了对外契约上，
而不是固化在 L1 内部。

顺带一个易撞名的历史包袱：`LUMICE_SceneSetSimParams(scene, int infinite, ...)`
（`:747`）里的 `infinite` 指的是**光线数量**无限（`ray_num: "infinite"`，
见 `src/server/c_api_scene.cpp` 的 `LUMICE_SceneSetSimParams` 与 `src/server/c_api_scene_internal.hpp` 的 `ConfigScratch::infinite`），
与光源距离无关。将来若要在 API 上表达光源距离，这个词位已被占用。

---

## 3. 三个扩张方向，各自压在哪道缝

owner 提出的三个扩张方向，压力点各不相同，**没有一条落在 core|gui 这道缝上**：

| 方向 | 真正的压力点 | 与仓库拓扑的关系 |
|---|---|---|
| **① 更多终端形态**（如纯网页 / 浏览器沙盒） | 可移植性与阻塞模型：线程模型、GPU 后端替换、文件 IO、C API 是否有隐含同步阻塞语义 | 无关。这是**构建目标**问题。现有的 accumulator-consumer + poller 拉取式取结果本来就是非阻塞形态，对沙盒环境是有利的地基 |
| **② 更多产品线**（如有限距离光源 / 专项光路分析） | **L0 与 L1 的分层**：产品线专属逻辑要长在通用引擎之上，而不是长在冰晕域模型之上 | 无关，且**方向相反**：需要的是从 L1 下面切出 L0，不是把 L0+L1+L2 整包搬到另一个仓 |
| **③ 正式发布动态库给第三方** | 契约与兼容政策：semver、ABI 稳定性承诺、符号可见性、install/export、废弃流程、契约测试 | 无关。单仓发布库是主流做法 |

---

## 4. 顺序约束（本文最有行动价值的一条）

> **方向 ② 决定 API 应该长什么样；方向 ③ 一旦发生就把 API 冻住。**

因此"先把动态库正式发布，再慢慢想产品线"是把顺序做反了：那会把
"冰晕应用 API + 编辑器键名助手"这个当前形态锁给第三方，而它恰好是方向 ② 最需要重切的部分。
这与既有纪律一致——没有正式发版就不要提前背兼容包袱。

同理，**拆仓也是一种提前冻结**：跨仓的 API 比仓内的 API 贵一个数量级去改。
在还不知道 L0/L1 该切在哪之前拆仓，等于用最贵的方式把当前这道缝（L1|L2）宣布为最终答案。

---

## 5. 为什么"拆成两个仓库"不是答案

1. **想买的性质已经成立**。GUI 侧零 core/config include、只链 C API、并有门禁保障。
   拆仓是用贵得多的机制去换一个已经成立的性质。
2. **跨树改动占比很高，且不是脏耦合**。截至 2026-08-01 的近 12 个月，已合并的代码 PR 中
   core-only 71 个、gui-only 50 个、**两边都动 71 个（192 个中占 37%）**。
   看这些 PR 的主题（自定义光谱、光路染色、filter 表达力提升、Scene 句柄化、auto EV）就知道，
   它们是"core 长出一个能力 → 必须在 UI 上有出口才算交付"的功能纵切，是产品形态决定的。
   拆仓会把这 37% 变成两仓 lockstep：发版、bump 依赖、两次 review、两次 CI，
   并且 `git bisect` 跨不过那道缝——而本项目大量依赖跨层白盒定位。
3. **发布形态是单一产品**。`.github/workflows/release.yml` 一次产出 CLI 与 GUI，同 tag 同签名，
   目前不存在需要独立节奏的 core 消费者。

### 5.1 唯一一处边界确实没守住（可独立行动）—— **2026-08-06 已收口，本节保留为记录**

原始诊断（2026-08-01）：
`test/gui/CMakeLists.txt:54`：`gui_test` 链接 `lumice_obj`（core 内部对象库）而非 `lumice`；
`test/gui/functional/test_render_handedness_guard.cpp:41-46` 直接 include 了 6 个 `core/`、`config/` 头。

产品遵守 C API，测试不遵守。后果是：**今天并不知道 GUI 侧能否只靠 C API 活下来**，
测试替产品兜住了缺口。

**✅ include 违规已消解（2026-08-03，commit `1526696b`）**：该 guard 已迁至
`test/unit-correctness/gui/`（target `gui_unit_test`），`test/gui/` 下不再有任何
`core/`、`config/` 直连。

选 `gui_unit_test` 而非 parity 层，是因为它是全仓**唯一同时链 `lumice_gui_obj` + `lumice_obj`**
的 target——该 guard 比较的正是 backend 正向投影 vs 两条 GUI 正向投影，
parity 层链不到 GUI 侧符号，做不了这次比较。它现在是一个**有名字的显式跨层 oracle**
（文件头写明了这一点），即本节原先给出的两个选项中的第一个。

**⏳ 剩余部分（2026-08-01 记）**：`gui_test`（以及新增的 `gui_unit_test`）仍链 `lumice_obj`
而非 `lumice`。这条**链接边**才是本节的真正剩余内容，与 include 违规是两回事，未随迁移消解——
故「GUI 侧能否只靠 C API 活下来」当时**仍无证据**。

**✅ 已收口（2026-08-06，`chore/micro-debt-sweep` 动作 8；2026-08-10 复核）——上一段的结论已被反转**：
`gui_test` 现在只链 `lumice_gui_obj` + `lumice`（`test/gui/CMakeLists.txt:73`），编得过、跑得绿。
⇒ **这是「GUI 侧能只靠 C API 活下来」的第一份正面证据**：全仓最大的那个 GUI 测试 target
不再需要 core 内部对象库。上一段"仍无证据"的判断到此为止，别再据它下结论。

剩下的两处链 `lumice_obj` 是**有名字的显式豁免**，不是未经检视的默认——两处 CMake 注释都写明了这一点：

| target | 位置 | 豁免理由 |
|---|---|---|
| `gui_unit_test` | `test/CMakeLists.txt:264` / `:281` | 全仓唯一同时链 `lumice_gui_obj` + `lumice_obj`，`test_render_handedness_guard.cpp` 这类**显式跨层 oracle**（比较 backend 正向投影 vs 两条 GUI 正向投影）必须同时看到两侧符号 |
| `composition_correctness_test` | `test/CMakeLists.txt:337` / `:341` | `composition-correctness` 层（跨 ≥2 个单元、但不需要真实帧的命题）的 target，链接形态与 `gui_unit_test` 相同，理由同上 |

⇒ 本节的诊断（产品遵守 C API、测试不遵守）已从"一处未守住的边界"变成"两处具名跨层 oracle"。
⛔ 看见 `lumice_obj` 不等于发现了新缺口——先读那两处注释。

---

## 6. 建议的动作序列

### 现在就做（成本低、不可后悔）

1. **在头文件里把三个高度显式分区**：仿真执行（Server / Scene / Commit / `LUMICE_ResultFrame`
   一族 —— 本文写作时这里列的是 `Get*Results`，那六个 getter 已于 v4.15 净删）
   与域 schema、编辑器支撑（`*KeyName`、`IsLegalFace`、`ValidateRaypathText`、
   `GetCrystalMesh`、`MaxFov`）分节标注。价值不在美观，在于**将来发布时可以选择只发布哪一层**。
2. **暂不正式发布动态库**；若要发布，明确标注 experimental、不承诺 ABI。
   ⚠️ **本条已被 §8「2026-09-28 更新」取代**——触发信号已响，见 §8。
3. **加一个"第三方消费者"CI 小样**：只用 install 出来的头文件 + 库编译一个几十行的 C 程序跑一次仿真。
   它同时是方向 ① 与方向 ③ 的探针，成本比拆仓低两个数量级；它红了说明 API 表达力不足
   （PR #224 的句柄化过程中已两次暴露此类潜伏缺陷）。

### 值得各花一次探索（不紧急，但正是"不能等需求到眼前"的那类）

4. **产品线触点勘察**：不写产品代码，只回答"要支持一条假设不同的产品线，
   core 的触点有多少、分布在哪"。产出是一份触点清单，它会直接指出 L0/L1 的缝该切在哪。
   估算 LOC 之前先数触点——这类估算容易高估 2–3 倍。
5. **沙盒/网页环境可行性勘察**：只测 core 能否在目标工具链下编出来并跑通一次仿真，不做 UI。
   一次性暴露线程、阻塞、文件 IO 的真实边界。

### 拆仓的触发条件（写清楚，以便将来机械判断）

- 出现**真实的外部 core 消费者**，且其发布节奏与本产品不同步；
- core 与前端需要**不同的授权 / 开源策略**；
- **第二个人或团队**长期只负责其中一侧。

并且：当 L0 引擎层真的被切出来、并有了自己稳定的 API 时，**该拆的是 L0，不是 core|gui**；
到那一步，拆不拆已退化为一个无关紧要的打包决定。

---

## 7. 诚实边界

- **产品线的具体设计不在本文范围内**。owner 对新产品线（如街灯等有限距离光源场景）已有远比本文深入的构想，
  并指出 **core 侧可复用的部分比本文推断的多得多**。本文只记录 API 分层这一侧的分析，
  不对产品线的技术形态下任何结论；§2.1 的光源例子仅用于说明"假设被固化在对外契约上"这一现象，
  不构成对该产品线实现路径的判断。
- **本文的分层命名（L0/L1/L2）是分析工具，不是既有代码结构的命名**。目前代码中不存在这三层的物理边界，
  这正是本文要指出的事情。
- **§6 的动作序列是建议，未经立项评审**。其中第 1、2、3 项是低成本且可逆的；第 4、5 项是探索性质，
  其价值取决于产品线方向的确定程度，应由 owner 决定时机。第 2 项「暂不正式发布动态库」已被
  §8 取代——见该节。
- §5 的 37% 数字来自对合并提交的机械分类（按 PR 的 diff 是否同时触及 `src/gui/` 与
  `src/{core,config,server,util,include}/`），口径为"已合并的代码 PR"，不含纯文档/测试/CI 的 PR。

---

## 8. 2026-09-28 更新：第二条产品线与第一个外部消费者

**触发信号已响**：本议题记录的两个触发信号——「准备正式发布动态库」与「第二条产品线立项」——
在光路分析升级为 Lumice 第二产品核心（`raypath-analysis.md` §5.1.8）后同时出现：Lumice Integral
（LI）将经 Python binding 使用 Lumice 共享库里的成熟算法模块（第一个候选：单光路反解 / fiber
行走），是 Lumice 共享库的第一个外部消费者。§6「拆仓的触发条件」里「真实的外部 core 消费者」
一条在字面上也被满足，但本次只发布一个新的窄接口、不拆仓，owner 也未就拆仓作任何决定——
按 §6 同节的后半句，即便将来要拆，该拆的也是 L0 而不是 core|gui。

**§4 的顺序约束第一次被满足**：§4 的顺序约束是「方向 ② 决定 API 应该长什么样；方向 ③ 一旦发生
就把 API 冻住」——此前不满足是因为产品线方向未定稿。现在光路分析的产品形态已定稿
（`raypath-analysis.md` §5.1.8），第一个外部消费者也已明确：先定产品线形状，再发布冻结 API，
顺序第一次被走对。

**发布的是一个新的窄接口，不是现在的 `lumice.h`**：依据是 `LUMICE_API_VERSION` 全部 23 次
不兼容变更的逐条归类——L0 引擎层 7 周只变 1 次、L1 场景描述自 7/24 起静止，全部近期变动集中在
L1 的渲染与标注（11 天 13 次，几乎都落在 `LUMICE_RenderParam`）；LI 需要的几何、光路、逆向求解
都在稳定的一侧，渲染、标注、编辑器支撑一个都用不到。

**形态照搬已跑通的做法**：同一份 `lumice_obj` 对象 + 独立头文件 + 独立前缀（由
`scripts/check_policies.py` 的 `no-test-symbol-in-src` 类规则机械守住）+ 单独的库——测试侧的
`lumice_testapi`（`CMakeLists.txt:1002`、`test/support/lumice_test_api.h`）已经是这个形态的先例，
新窄接口照搬其形状，而不是从零设计。

**共享判据**：哪些语义进这个新接口、哪些两边各留一份互相校验，判据是「稳定，且不是两边互相
校验的对象」——完整推理与正反证据见 `raypath-analysis.md` §5.1.6「仓库定位」，本文不复制。

**时序**：基础设施先做（新头文件与前缀门禁、`LUMICE_API` 导出宏替代
`WINDOWS_EXPORT_ALL_SYMBOLS`、库默认 console sink 的宿主接管、semver / ABI 与废弃政策、
install / export 打包、LI 侧 binding 冒烟测试兼作「第三方消费者」样例）；共享库的实际内容随
第一个成熟算法模块一起进去，不做没有内容、只为发布而发布的库。第一个候选模块是单光路反解 +
fiber 行走——语义稳定、体量小。这部分已立项为本仓一个 scrum（尚未推进）。

---

## 9. 按能力划分库与头文件（as-built，2026-09-30）

§2 批评的「三个高度，一个平面」现在有了结构性的回应。本节记录落地后的真实状态；§4 的顺序
约束（先定产品线形状，再冻结 API）没有变，被松动的只有「一个平面」这一半。

### 9.1 两个正交维度

- **代码怎么切**：按层与能力切。层次的单一声明是 `cmake/lumice_layers.cmake`（foundation →
  analytic → scene → view → sim → render → engine → raypath → capi 九层，由低到高），
  `scripts/check_policies.py` 的 `layer-inversion` 规则拒绝任何指向更高层的 `#include`、
  清单不认识的 `src/` 文件、以及指向不存在文件的清单项；存量逆层边只剩
  `cmake/lumice_layer_allowlist.txt` 里的记录，该文件只减不增。
- **打包成哪些二进制**：由外部约束决定，不由能力划分决定。当前两条：① 对外分发——LI 需要
  `liblumice_analytic` 作 shared lib；② 运行时 ISA 分派——linux-x64 / windows-x64 发布版引擎
  是 shared lib 且编 baseline 与 x86-64-v3 两份，macOS 与 linux-arm64 整体 static。
  两条约束都不要求「一个能力一个动态库」，反而反对它：ISA 分派要求引擎是一整份可整体替换的
  二进制，一进程一引擎（§9.4）要求 foundation 的单例只存在一份。

### 9.2 头文件：六个能力头，没有伞头

原 `lumice.h`（3098 行、84 个函数）已按功能拆开并**删除**，不存在收拢它们的头；每个
includer 只包含自己用到的能力头。这个头集合不对外发布，所以拆分不构成 API 冻结。

| 头 | 承载 | 对 §2 的回应 |
|---|---|---|
| `lumice_base.h` | 符号可见性宏、`LUMICE_API_VERSION` 及其变更史、不透明句柄、错误码、日志、产品版本 | 其余各头都包含它，它不包含任何一个 |
| `lumice_scene.h` | 场景描述值类型与 `LUMICE_Scene` 的构建 / JSON API | L1 冰晕域模型 |
| `lumice_render.h` | 渲染参数、标注、投影 | L1 中变动最快的一块（§8 的 11 天 13 次），现在单独成头 |
| `lumice_editor.h` | 编辑器支撑：键名、合法性、mesh、`LUMICE_ValidateRaypathText` 等 | L2 与仿真执行分开 |
| `lumice_engine.h` | 服务器生命周期、提交场景、结果帧、光路分析运行、后端选择 | 引擎的执行面 |
| `lumice_raypath.h` | 单光路分析入口，结果为不透明句柄 + JSON | 第二产品核心的接入面 |

依赖只有一个方向：`base` ← `scene` ← `render` ← `engine`，`editor` 与 `raypath` 各自向下。
每个头都有「单独编译、C 与 C++ 各一次」的自含性测试。`src/gui/` 只许包含这些引擎能力头
（`gui-api-boundary` 规则），白名单由 `cmake/export_surfaces.cmake` 推导，不另写一份。

L0 引擎（几何、求交、折射、采样）仍然**没有**独立的 C 出口：它现在以 `foundation` 层的
形式存在于代码里（`liblumice_analytic` 只链它加解析核），对外则经 `lumice_analytic_core.h`
的计算函数暴露。§2 表中「L0 无独立出口」这一格因此从「没有」变成「只有解析这一条窄出口」，
这是有意的：L0 里被外部消费者需要的部分，就是解析核。

### 9.3 C 桥按头拆开，桥属于 capi 层

原 `src/server/c_api.cpp`（4679 行）已按头拆为 `c_api_{scene,editor,crystal_mesh,render,engine,raypath}.cpp`
六个桥并删除，内部头也按层拆开。`c_api_raypath.cpp` 原名 `c_api_single_path.cpp`，曾编进
`lumice_raypath_obj` 却依赖 `server/` 的内部类型，现在归位 raypath 层；`ParseConfigManager`
只剩一个名字，`MapErrorCode` 改为 `lumice::capi::ToCApiErrorCode`（inline、不再外部链接）。

### 9.4 三个打包各自导出什么

导出集合的单一声明是 `cmake/export_surfaces.cmake`，导出表由 `scripts/gen_export_list.py`
从各库的头文件生成，`scripts/check_export_surface.py` 是「导出表 == 头文件声明」的唯一对账者，
测试（`test_export_symbol_scope.py`）与 release 共用它；release 对 Linux 与 Windows 的 ISA 双份
引擎逐文件核对。

| 打包 | 链接 | 导出（函数个数） |
|---|---|---|
| `liblumice`（Windows 为 `lumice-engine.<tier>.dll`） | `lumice_obj` + foundation + raypath + 解析核 | 六个能力头 + `lumice_analytic_core.h`（93） |
| `liblumice_testapi` | 同上，同一批对象 | 引擎全集 + `LUMICE_TEST_*` 钩子（95） |
| `liblumice_analytic` | 只有 foundation + 解析核 | `lumice_analytic.h` 加 `lumice_analytic_core.h`（11） |

要点：

- **解析核是 Lumice 自己的产品能力**，引擎打包导出它的计算函数，壳可以直接使用；
  `liblumice_analytic` 只是给外部的分发通道。⛔ 不在引擎头里再包一套同语义的函数（同一语义
  只允许一个权威实现）。
- **库管理函数属于打包而不属于能力**：`LUMICE_ANALYTIC_GetApiVersion` / `SetLogCallback`
  （含加载时摘除 console sink）只在 `liblumice_analytic` 导出；在引擎里，日志与版本就是
  `lumice_base.h` 的 `LUMICE_SetLogCallback` / `LUMICE_GetVersionString`。这一分离靠
  「core 头 / 管理头」两个头实现，consumer 的 `#include <lumice_analytic.h>` 与所有符号、
  布局不变，所以 `LUMICE_ANALYTIC_API_VERSION` 没有 bump。
- **一进程一引擎不变**：同一进程不得同时加载引擎与 `liblumice_analytic`（两份含 foundation
  的打包会有两份 logger 等单例）；测试进程也只加载三库之一，
  `test/e2e/capi_runner.py::lib_candidates` 是唯一权威。

### 9.5 结果形态与版本号：两条已定结论

- **结果的对外形态 = JSON 文档 + `schema_version`**（例：`LUMICE_SinglePathResultToJson`，
  字段参考 `raypath-cli-output.md`）。它与 §2 批评的「返回配置键名的函数」不是一回事：
  后者是编辑器与序列化格式之间的契约，前者是引擎对调用者的结果契约，字段只追加，
  不改 ABI。类型化读取器可以以后追加，不与 JSON 冲突。
- **`LUMICE_API_VERSION` 与 `LUMICE_ANALYTIC_API_VERSION` 不统一**：它们量的是不同的东西
  （引擎 ABI 的变动速率比解析接口快一个数量级，见 `analytic-api.md` §8.1），一个消费者
  不应把另一个的变动读成自己的版本变化。

### 9.6 仍未做的

各能力层成为独立的 OBJECT 库（scene / view / sim / render / engine）尚未做：目前 `lumice_obj`
仍是一整块，层次靠清单与 `layer-inversion` 规则守护，而不是靠 CMake 依赖。它排在构建侧改动
合入之后；在那之前，「层」是被检查的声明，不是被链接器强制的边界。
