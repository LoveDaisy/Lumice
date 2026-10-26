[English version](c_api.md)

# C接口使用文档

本文档详细说明C接口API使用方法。

## 概述

Lumice 提供了完整的C接口，方便与其他语言集成。C接口封装了C++实现，提供了简洁的API。

### 头文件

Lumice 的 C API 按能力拆成六个头文件，每个能力一个；按用到的内容包含对应的头，没有伞头。

| 头文件 | 内容 |
|---|---|
| `lumice_base.h` | 符号可见性、不透明句柄、错误码、日志、产品版本；其余各头都包含它（ABI 版本在单独的叶子头 `lumice_api_version.h` 中——需显式包含，见「常量」） |
| `lumice_scene.h` | 场景描述值类型与 `LUMICE_Scene` 的构建 / JSON API |
| `lumice_render.h` | 渲染参数、标注、投影 |
| `lumice_editor.h` | 编辑器支撑：键名、合法性检查、晶体网格、光路文本校验、Miller 指数转换 |
| `lumice_engine.h` | 服务器生命周期、提交场景、结果帧、光路分析运行、后端选择 |
| `lumice_raypath.h` | 四类光路结果的 typed 读取面、workspace/warm/cancel 句柄与可选 JSON 输出 |

```c
#include "lumice_engine.h"  // 同时包含 lumice_base.h、lumice_render.h、lumice_scene.h
```

下文示例需要 `lumice_engine.h`（服务器、提交、结果）与 `lumice_scene.h`（构建场景），
前者已包含后者。

### 链接库

链接 `lumice` 静态库。

### 设计原则

- **统一错误码**：大多数 API 返回 `LUMICE_ErrorCode`，实际输出通过指针参数传递
- **数组+哨兵模式**：结果获取 API 使用固定大小数组 + 哨兵终止，无堆分配
- **零拷贝**：渲染结果的 `img_buffer` 直接指向内部缓冲区，无拷贝开销

## API参考

### 常量

```c
// lumice_api_version.h——叶子头；在使用该宏的文件里显式包含
//（lumice_base.h 刻意不包含它，原因见下）
#define LUMICE_API_VERSION 458        // ABI 版本，编码为 major*100 + minor（v4.58）

// lumice_base.h
#define LUMICE_MAX_RENDER_RESULTS 16  // 渲染结果数组最大容量
#define LUMICE_MAX_STATS_RESULTS 1    // 统计结果数组最大容量
```

`LUMICE_API_VERSION` 让调用方把编译时依赖的 ABI 钉死，在不匹配时编译期报错，而不是撞上结构体布局漂移导致的静默 UB：

```c
static_assert(LUMICE_API_VERSION >= 439, "Lumice header too old for this integration");
```

公开符号集增加或结构体布局发生 BREAKING 变更时都会 bump。`lumice_base.h` 刻意不包含
`lumice_api_version.h`：宏**值**变化会使其包含闭包内全部 TU 的编译缓存条目失效，把 define 挡在
`lumice_base.h` 的宽闭包之外，bump 就只需重编文本上使用该宏的文件。凡引用 `LUMICE_API_VERSION`
处请直接包含 `lumice_api_version.h`。

### 类型化光路结果

四个既有 `LUMICE_Analyze*` 入口保留；每个结果句柄持有一份不可变的 typed 值，`*ToJson`
只是从该值派生并缓存的可选表示。typed getter 不解析 JSON，也不启动物理计算。
`LUMICE_PathWorkspaceCreate` 在创建时捕获 Scene，四个同步 `LUMICE_PathWorkspaceQuery*` 入口
不再接受另一份活 Scene。Scene/Workspace 销毁或后续 query 不影响已经取得的结果。

`LUMICE_PathWarmDataFromSurvey` / `LUMICE_PathWarmDataFromPoint` 从结果提取独立持有的复用数据；
`LUMICE_PathWarmDataFromJson` 是旧 warm 文本的显式导入入口。新 query 对携带 warm 文本的请求
具名拒绝，文本与 typed warm 同时给出也不暗选其一。`LUMICE_PathCancelToken` 可从另一线程
单向请求取消。正数 `deadline_ms` 在入口解析为一次绝对截止，0 表示新路径不设截止；执行结局
区分协作式截止与显式取消。既有 Analyze 入口的预算语义不变。

四种结果各有 `GetInfo` 与按语义块命名的 indexed getter。调用方把输出记录的 `struct_size`
设为编译时的完整 extent：已发布的完整 extent 或更大的未来尾部可接受；未发布的中间尺寸与
过小尺寸被拒绝，未知尾部不写。可增长的复杂记录逐项读取，而不是把当前库的 struct stride
强加给旧调用方。同质标量数组与 UTF-8 `{data,length}` 字符串从结果只读借用，至该结果
Destroy 为止；字符串以 NUL 结尾，长度不含终止符。活结果上的只读 getter/`ToJson`
可以并发，但 Destroy 必须等待所有读取与借用指针使用结束。缺失值由 available/evidence
显式标记；非法索引与 variant kind 返回具名读取状态，不把默认零当结果。

**v4.54** 是纯追加。新增光路 survey——带 `struct_size` 头的 `LUMICE_PathSurveyRequest`、不透明的
`LUMICE_PathSurvey`、`LUMICE_AnalyzePathSurvey`、`LUMICE_PathSurveyToJson`（与
`LUMICE_SceneToJson` 同一缓冲区契约）、`LUMICE_PathSurveyDestroy`，以及
`LUMICE_PATH_SURVEY_MAX_BUDGET_MS`。survey 在代数层路由一个单层光路类：全体成员都带 kernel 的
rank-0 恒等证书时，整类坍缩为太阳处的一个 `source_image` 对象并早停；否则如实输出 T0 事实、
T1/T2 对象类目记为 unevaluated。JSON 是新的 schema 族 `lumice.path-survey`，schema_version 4
（`raypath-cli-output.md` §9）；与 schema-1/3 文档不共享字段。既有符号与结构体零变化。

**v4.53** 将无 target 的 `lumice.path-feature-report` 文档升级为 schema 3：结构对象、support、
features、未归因结构、MC 证据与 coverage 分层表达；schema 2 读者必须拒读 schema 3。请求结构体以及
v4.50 的 target/fiber/warm schema 1 接口不变。字段见 `raypath-cli-output.md` §7。

**v4.52** 在 `LUMICE_PathFeatureReportRequest` 尾部追加完整的预算、观察与层选择字段组；v4.51
前缀在新默认值下仍可使用。报告 JSON 升为 schema 2，记录实际产品输入与光谱、有界数值发现、
actual/candidate/unfinished 分离、partial work 和显式的不支持多晶体结果；其数值能力与 analytic API 7
共享。target/fiber/warm 的 schema 1 字段和计算均不变。

**v4.51** 是纯追加。新增不接受天空 target 的路径级 feature report：
`LUMICE_PathFeatureReportRequest`、不透明的 `LUMICE_PathFeatureReport`、
`LUMICE_AnalyzePathFeatureReport`、`LUMICE_PathFeatureReportToJson`、
`LUMICE_PathFeatureReportDestroy`，以及采样数、波长数和组合工作量上限。请求按具体物理 L2 成员与离散波长计算
有限晶体 `A*T`，结果使用独立的 `lumice.path-feature-report` schema；v4.50 的目标点请求、JSON 与
warm-start 合约没有增加字段。字段和 coverage 边界见 `raypath-cli-output.md` §7。

**v4.50** 是纯追加。新增单光路分析：带 `struct_size` 头的 `LUMICE_SinglePathRequest`、不透明的 `LUMICE_SinglePathResult`、`LUMICE_AnalyzeSinglePath(scene, request, &out, err_buf, err_size)`、`LUMICE_SinglePathResultToJson`（与 `LUMICE_SceneToJson` 同一缓冲区契约）、`LUMICE_SinglePathResultDestroy`，以及 `LUMICE_SINGLE_PATH_MAX_SAMPLE_COUNT`。结果经其 JSON 形式读取，字段见 `raypath-cli-output.md`。行为：`LUMICE_SetLogLevel(NULL, level)` 现在设置引擎全局日志级别（原为什么都不做），供不建 server 的调用方使用。

**v4.49** 是纯追加加一处行为恢复。新增 `LUMICE_CouldFilterMatchFace(crystal, face, symmetry)`——考虑 filter 自身 P/B/D 的 `LUMICE_CouldCrystalHaveFace`（没有面积的面仍可经由 filter 对称重新编号到的面匹配）——以及 `LUMICE_ExpandRaypathClass(crystal, faces, n, symmetry, semantics, out, &count)`，按两种含义之一（`LUMICE_SYMMETRY_SEMANTICS_LABEL` / `LUMICE_SYMMETRY_SEMANTICS_PHYSICAL`）给出一条光路在一组对称位下的等价类（至多 `LUMICE_MAX_RAYPATH_CLASS_MEMBERS` 个成员）。行为：**filter** 的 P/B/D 恢复为标号等价，与 v4.46 及以前相同——P、B 不论晶体形状与取向都生效，D 不变。下文 v4.47、v4.48 所述的收窄现在只适用于光路分析列表的分组；`LUMICE_GetCrystalSymmetry` 与 `LUMICE_IsPApplicable` / `LUMICE_IsBApplicable` 含义不变，服务于该分组与 filter 编辑器的提示（见 `raypath-symmetry.zh.md` §1.1）。没有结构体变化。

**最近一次这样的 break 是 v4.48。** `LUMICE_CrystalSymmetry` 在 `d_effective` 之后新增尾部字段 `p_effective`、`b_effective`（sizeof 16 → 24）；`LUMICE_GetCrystalSymmetry` 会写满整个结构体，调用方需重新编译。同时新增 `LUMICE_IsPApplicable(roll 类型, roll 全宽)` 与 `LUMICE_IsBApplicable(方位角类型, 方位角全宽, zenith 类型, zenith 中心, zenith 全宽)`——P、B 的取向分布一半（对应 `LUMICE_IsDApplicable`）。光路约化现在只在 roll 对 60° 平移不变（满圈 uniform）时应用 P，只在 zenith 关于 90° 对称且方位角满圈 uniform 时应用 B：Parry 弧不再把 3-5 与 4-6 并为一行，板状晶体不再把 1-3 与 2-3 并为一行。`p_effective` / `b_effective` 是这两个条件再与形状相交（`rotation_step < 6` / `horizontal_mirror`）。

**`p_effective` 的当前语义（不占版本号——零 ABI 变化，无结构体变化）：** 它现在报告分析分组自身的推导——保留旋转集（roll 周期性，加上「zenith 关于 0°/180° 对称且方位角满圈 uniform」时极点折回的 180° 子群，再与形状实有旋转相交）——而非 roll 探针再交 `rotation_step`。Lowitz 对齐（zenith 高斯中心在极点、roll 锁定）会把 `p_effective` 从 0 翻到 1：其 180° 子群在该处合并分析行。`LUMICE_IsPApplicable` 本身不变，仍回答 roll 单一问题；台账见 `lumice_api_version.h`。

**v4.46：** `LUMICE_RenderParam` 在 `globe_back_fade` 之后新增尾部字段 `display_mode`（`LUMICE_DISPLAY_MODE_NORMAL` / `LUMICE_DISPLAY_MODE_CHANNEL_BR`，sizeof 6456 → 6460），调用方需重新编译。JSON 键为 `render.display_mode`，取值 `"normal"` 或 `"channel_br"`。该字段引入 gamma 之后的 sRGB B − R 灰度诊断；在 `tone: print` 下无效果，开启时带色类配置的场景不产出光路染色合成图。零值即 normal，所以零初始化的结构体和不含该键的文档渲染结果不变。

**当前 `channel_br` 契约（后续语义修正，不改结构体布局）：** `clamp(0.5 + 2·(B − R), 0, 1)`
读取同一曝光和视角下、**未叠加天空底色的晕图** gamma 后 sRGB 通道，排除天空底色与背景照片，
也不在计算后加回底色；空天空为中灰。结果仍随曝光与裁剪变化，须在同一 EV 下比较；叠加层保留原色。
这不是对 v4.46 当时含天空输入语义的追溯改写。

**v4.45 就是一次这样的 break。** `LUMICE_RenderParam` 在 `view_dist_label` 之后追加 `float globe_back_fade`（sizeof 6452 → 6456），调用方需重新编译。它是 `globe` 镜头的背面渐隐范围——球的背面在轮廓之后多深的范围内仍然可见、随离相机的距离渐隐（JSON 键 `globe_back_fade`，见 `configuration.md`）。其余镜头忽略它；0（零初始化值与 JSON 默认值）只显示朝向相机的半球，即此前各版本的画面。没有任何移除或重排。

**v4.39 就是一次这样的 break。** 第五个注解 family——视场距离圆，即与相机**光轴**成等角距的圆，是 `angular_dist`（以太阳为参考）的以光轴为参考的孪生——让两个结构体在尾部增长，调用方需重新编译。`LUMICE_RenderParam` 在 `paper` 之后新增 `view_dist[]` / `view_dist_count` / `view_dist_line` / `view_dist_label`（sizeof 4904 → 6452），与 `angular_dist` 四字段逐项同形；`LUMICE_AnnotationRequest` 在 `marker_count` 之后新增 `view_dist_deg` / `view_dist_count`（128 → 144），其标签以 kind `LUMICE_ANNOTATION_VIEW_DIST`（4）返回。JSON 键为 `grid.view_dist`、`grid.view_dist_line`（默认 true）、`grid.view_dist_label`（默认 false），形态与 `angular_dist` 三键完全一致；缺键即 family 关闭，所以 v4.39 之前写下的文档渲染结果不变。没有任何移除或重排。有两点是**刻意不做**的：没有 `reference_dir_view`——圆心就是视图自己的前向（elevation / azimuth / roll），core 已经为前半球裁剪推导了它，请求本身已决定了圆心，再加一个方向字段只是它的第二份拷贝；且它结构上与 `lens_shift` 无关——移轴移动的是光轴的**像素**而非光轴，圆跟着光轴走。另一点与其他三个线开关相同：零初始化的 `LUMICE_RenderParam` 里 `view_dist_line` 读作 0，而 JSON 默认为 true，请显式设置或走 JSON。两个结构体现在都在头文件里带精确 sizeof 的 `static_assert`，下一次尾部追加若不同时递增版本号即为编译错误。

**v4.13 就是一次这样的 break。** `LUMICE_CrystalParam` 在结构体末尾新增了一个字段：`int sync_group[LUMICE_SHAPE_SCALAR_COUNT]`，同时新增了十个 `LUMICE_SHAPE_SCALAR_*` 索引常量。核心仿真器自 v4.12 起就已经能表达形状标量 sync group（同组的若干形状标量在一个晶体实例上共享一次随机抽样——完整语义见[`configuration.md` 的形状标量 Sync Group 一节](../configuration.md#shape-scalar-sync-groups)），但 `LUMICE_CrystalParam` 当时没有对应槽位，导致配置文件、GUI、以及任何调用方——三者都只能走这唯一一条通路——的声明会被静默丢在地上：核心侧永远收到"全部独立"，且没有任何警告。结构体布局发生了变化，调用方需要重新编译；但行为不变——零初始化的 `sync_group`（与此前一个被清零/`{}` 初始化的 `LUMICE_CrystalParam` 的状态相同）意味着每个标量都独立，与此前完全一致。`LUMICE_SHAPE_SCALAR_*` 按 **RNG 抽取顺序**索引 `sync_group[]`（prism：`HEIGHT` 之后是 `FACE_0..FACE_5`；pyramid：`UPPER_H` 之后是 `PRISM_H`、`LOWER_H`，再之后是 `FACE_0..FACE_5`），这个顺序刻意**不同于** `LUMICE_CrystalParam` 本身的字段声明顺序（`height` / `prism_h` / `upper_h` / `lower_h`——注意 `upper_h` 与 `prism_h` 相对抽取顺序是互换的）。一个组的 leader——即整个组的分布会被规范化到的那个成员——被定义为组内最小的 `LUMICE_SHAPE_SCALAR_*` 索引，所以这个顺序是有约束力的：若把它对齐到结构体字段顺序，会静默改变一个混合组里到底是哪个成员拥有分布。

**v4.12 也是一次 break。** 宽值结构体 `LUMICE_Config` 及所有只为喂养它而存在的东西被移除：`LUMICE_Config` 本身、三个提交入口（`LUMICE_CommitConfig` / `LUMICE_CommitConfigFromFile` / `LUMICE_CommitConfigStruct`）、`LUMICE_ParseConfigString` / `LUMICE_ParseConfigFile` / `LUMICE_ConfigToJson`、`LUMICE_ConfigCreateColorClasses` / `LUMICE_ConfigReleaseColorClasses` / `LUMICE_ConfigReleaseCompositions` 所有权辅助函数，以及 C++ RAII 头 `src/include/lumice_config_scope.hpp`。`LUMICE_Scene` 取代了它们全部。不提供任何 shim 或别名：调用已移除符号的代码会编译失败——这正是预期的迁移信号。

### 数据类型

#### LUMICE_Server

服务器句柄，不透明指针类型。

```c
typedef struct LUMICE_Server_ LUMICE_Server;
```

#### LUMICE_Scene

场景句柄，不透明指针类型。它是本 API 的配置容器，完整生命周期见 [配置](#配置)。

```c
typedef struct LUMICE_Scene_ LUMICE_Scene;
```

#### LUMICE_ErrorCode

错误码枚举。大多数 API 的返回值。

```c
typedef enum LUMICE_ErrorCode_ {
  LUMICE_OK = 0,             // 成功
  LUMICE_ERR_NULL_ARG,       // NULL 参数
  LUMICE_ERR_INVALID_JSON,   // JSON 格式错误
  LUMICE_ERR_INVALID_CONFIG, // 配置内容错误
  LUMICE_ERR_MISSING_FIELD,  // 缺少必填字段
  LUMICE_ERR_INVALID_VALUE,  // 字段值无效
  LUMICE_ERR_FILE_NOT_FOUND, // 文件不存在/无法打开
  LUMICE_ERR_SERVER,         // 服务器错误
  LUMICE_ERR_UNKNOWN,        // 未被以上类别覆盖的内部失败
} LUMICE_ErrorCode;
```

#### LUMICE_ServerState

服务器状态枚举。

```c
typedef enum LUMICE_ServerState_ {
  LUMICE_SERVER_IDLE,      // 空闲状态
  LUMICE_SERVER_RUNNING,   // 运行中
  LUMICE_SERVER_NOT_READY, // 未就绪
} LUMICE_ServerState;
```

#### LUMICE_LogLevel

日志级别枚举。

```c
typedef enum LUMICE_LogLevel_ {
  LUMICE_LOG_TRACE,    // 追踪
  LUMICE_LOG_DEBUG,    // 调试
  LUMICE_LOG_VERBOSE,  // 介于 DEBUG 与 INFO 之间的详细诊断
  LUMICE_LOG_INFO,     // 信息（默认）
  LUMICE_LOG_WARNING,  // 警告
  LUMICE_LOG_ERROR,    // 错误
  LUMICE_LOG_OFF,      // 关闭日志
} LUMICE_LogLevel;
```

#### LUMICE_RenderResult

渲染结果结构体。

```c
typedef struct LUMICE_RenderResult_ {
  int renderer_id;                   // 渲染器ID
  int img_width;                     // 图像宽度（像素）
  int img_height;                    // 图像高度（像素）
  const unsigned char* img_buffer;   // 图像数据缓冲区（RGB格式，只读）
} LUMICE_RenderResult;
```

**注意**：
- `img_buffer` 指向的图像数据由库内部管理，不需要手动释放
- `img_buffer` 是其来源 `LUMICE_ResultFrame` 的只读视图，有效期直到该帧被
  `LUMICE_ReleaseResultFrame()` 释放为止——持有帧本身就是保住这段像素的手段，
  另一个读者去取自己的帧不会让你手上的失效
- 如需数据活得比帧更久，请在释放前自行 `memcpy` 拷贝
- 图像数据格式为RGB，每个像素3个字节（R, G, B）
- 图像数据大小 = `img_width * img_height * 3` 字节
- 哨兵标识：`img_buffer == NULL`

#### LUMICE_StatsResult

统计结果结构体。

```c
typedef unsigned long long LUMICE_RayCount;

typedef struct LUMICE_StatsResult_ {
  LUMICE_RayCount ray_seg_num;      // 光线段数量
  LUMICE_RayCount sim_ray_num;      // 模拟光线数量
  LUMICE_RayCount crystal_num;      // 本次 run 实际采样出的不同晶体几何数（见“注意”）
  LUMICE_RayCount orientation_num;  // 本次 run 实际采样出的不同晶体取向数（见“注意”）
} LUMICE_StatsResult;
```

`LUMICE_RayCount` 在所有平台上都至少为 64 位，包括 `unsigned long` 只有 32 位的 Windows。

**注意**：
- 哨兵标识：全零（`sim_ray_num == 0`）
- `orientation_num` 将取向与几何分开计数。固定形状配随机轴时可以只有一个几何，却有大量取向；
  与 `crystal_num` 相同，它受 CPU/GPU 采样密度影响，不能作为跨后端 parity 断言。
- `crystal_num` 是**本次 run 实际采样出了几个不同的晶体几何**，不是构造了几个晶体对象。
  形状由固定数值给定时只会抽样一次、之后全程复用，所以形状不含随机分布的场景恰好报出
  它的（散射层 × entry）总数 —— 第 1 层 3 个 entry 加第 2 层 2 个就报 5，与 `ray_num`
  无关。给形状加上分布（`height` / `face_distance` 上的 `{"type": "gauss", ...}`）后，
  这个数随实际抽出的几何数增长 —— 这正是用来确认形状随机化是否真的生效的观察量。
- **不可跨后端比较**。CPU 路线逐 ray-group 抽新几何；GPU 路线在 K-shape 几何时钟关闭
  （默认关闭）时每 dispatch 每 (层, entry) 只抽一次。同一场景在不同后端上因此报出不同的
  数值：这个差距就是两条路线的采样密度差异本身，不是需要抹平的不一致。
- 与 `num_workers`、与 batch/dispatch 粒度都无关。场景中形状固定的那部分按 committed
  config 清点一次，不是每 worker 一次、也不是每 batch 一次，所以性能旋钮动不了它；
  随机抽出的那部分则逐次累加 —— 那些确实是不同的几何，累加正是想要的答案。
- 形状固定的 entry 即使没分到光线（例如 `crystal_proportion` 为 0）也计入。
  这个量描述的是场景，不是调度。

#### LUMICE_ServerConfig

服务器配置结构体，用于 `LUMICE_CreateServerEx()`。

```c
typedef struct LUMICE_ServerConfig_ {
  int num_workers;        // CPU worker 数。0 = 自动：Linux/macOS 取物理核心数，Windows
                          // 取完整逻辑核心数，并应用平台上限；具体单源为
                          // ServerImpl::AutomaticWorkerBaseAndCap()。显式正值原样采用。
  unsigned int sim_seed;  // 确定性 RNG 种子。0 = 随机（默认）。
                          // 非零时收缩为单 worker，以获得逐位稳定结果。
  int preferred_backend;  // LUMICE_BACKEND_CPU / LUMICE_BACKEND_METAL / LUMICE_BACKEND_CUDA
} LUMICE_ServerConfig;
```

**注意**：
- 零初始化的结构体（`= {0}`）等同于默认行为（自动 worker 数、随机种子、CPU 后端）
- `num_workers` 在 CPU 路线决定渲染 worker 数；在 Metal/CUDA 路线不改变单 GPU render engine，
  而是决定常驻 CPU analysis pool 的大小，即 `LUMICE_StartRaypathAnalysis` 使用的 worker 数
- 当 `sim_seed != 0` 时，服务器把 CPU worker 组（GPU 路线上的 analysis pool 亦同）收缩为 1，
  使固定调用序列可复现
- `preferred_backend` 在构造时选择路线；`LUMICE_TRACE_BACKEND` 开发环境覆盖仍具有更高优先级

### 服务器生命周期

#### LUMICE_CreateServer

创建服务器实例。

```c
LUMICE_Server* LUMICE_CreateServer(void);
```

**返回值**：
- 成功：返回服务器句柄指针
- 失败：返回 `NULL`

**注意**：
- 返回的句柄必须使用 `LUMICE_DestroyServer()` 释放

#### LUMICE_CreateServerEx

使用自定义配置创建服务器实例。

```c
LUMICE_Server* LUMICE_CreateServerEx(const LUMICE_ServerConfig* config);
```

**参数**：
- `config`: 指向 `LUMICE_ServerConfig` 结构体的指针；传 `NULL` 等同于 `LUMICE_CreateServer()`

**返回值**：
- 成功：返回服务器句柄指针
- 失败：返回 `NULL`

**注意**：
- 返回的句柄必须使用 `LUMICE_DestroyServer()` 释放
- 当需要设置确定性种子或控制线程数时，使用此函数替代 `LUMICE_CreateServer()`

#### LUMICE_DestroyServer

销毁服务器实例。

```c
void LUMICE_DestroyServer(LUMICE_Server* server);
```

**参数**：
- `server`: 服务器句柄指针，可以为 `NULL`（安全）

**注意**：
- 销毁服务器会停止所有正在进行的处理。
- `LUMICE_DestroyServer` 是生命周期屏障，不会替外部调用方做并发同步。调用前必须阻止新的
  server 句柄调用，并等待全部在途调用返回（例如 join 调用线程，或使用外部销毁锁）；返回后不得
  再使用该 server 句柄。
- 销毁前已取得的结果帧是独立句柄，仍可在 server 销毁后读取和释放，但须遵守下文每帧自己的
  生命周期规则。

### 日志

#### LUMICE_SetLogLevel

设置日志级别。

```c
void LUMICE_SetLogLevel(LUMICE_Server* server, LUMICE_LogLevel level);
```

### 配置

配置存放在 `LUMICE_Scene` 里——一个不透明、增量构建的句柄。配置 server 只有一条路：先构建或解析出一个场景，再提交它。

```
   LUMICE_SceneCreate + Add*/Set*  ─┐
                                    ├─→  LUMICE_Scene  ──→  LUMICE_CommitScene(server, scene)
   LUMICE_SceneFromJson{,File}     ─┘         │
                                              └──→  LUMICE_SceneToJson  （保存 / 查看）
```

编写（JSON 或程序化）与提交是刻意分离的：`SceneFromJson` 从不碰 server，`CommitScene` 也从不接受文档。句柄归调用方所有，可以反复提交，最终必须销毁。

#### 场景生命周期

```c
LUMICE_Scene* LUMICE_SceneCreate(void);
LUMICE_Scene* LUMICE_SceneClone(const LUMICE_Scene* scene);
void          LUMICE_SceneDestroy(LUMICE_Scene* scene);
```

- `SceneCreate` 返回空句柄，分配失败时返回 `NULL`。所有权归调用方，最终必须传给 `SceneDestroy`。
- `SceneClone` 深拷贝出一个完全独立的句柄（无别名）。这正是旧的宽值结构体语义真正买到的东西——原子的模态编辑 + Cancel——现在只需一次调用，而不是一次六位数字节的栈拷贝。每个句柄都要各自销毁。`scene` 为 `NULL` 或分配失败时返回 `NULL`。
- `SceneDestroy` 对 `NULL` 是安全空操作。每个句柄只能销毁一次，重复销毁是未定义行为（与 `LUMICE_DestroyServer` 同一契约）。

#### 构建场景：`LUMICE_SceneAdd*`

```c
LUMICE_ErrorCode LUMICE_SceneAddCrystal(LUMICE_Scene*, const LUMICE_CrystalParam*, int* out_id);
LUMICE_ErrorCode LUMICE_SceneAddFilter(LUMICE_Scene*, const LUMICE_FilterParam*, int* out_id);
LUMICE_ErrorCode LUMICE_SceneAddComplexFilter(LUMICE_Scene*, const LUMICE_FilterParam*,
                                              const LUMICE_ComplexComposition*, int* out_id);
LUMICE_ErrorCode LUMICE_SceneAddRenderer(LUMICE_Scene*, const LUMICE_RenderParam*, int* out_id);
LUMICE_ErrorCode LUMICE_SceneAddScatterLayer(LUMICE_Scene*, const LUMICE_ScatterLayer*, int* out_id);
LUMICE_ErrorCode LUMICE_SceneAddColorClass(LUMICE_Scene*, const LUMICE_ColorClass*, int* out_id);
```

每个 `Add*` 追加一个条目，并把它的 0 基序号 id（同类条目中按插入顺序的下标）写入 `*out_id`。

- **id 由场景分配，传入结构体上的 `.id` 字段一律被忽略。** 之后构造的交叉引用字段（`LUMICE_FilterParam.crystal_id`、`LUMICE_ScatterEntry.crystal_id` / `.filter_id`、composition 的 term id）**必须**使用返回的 `out_id`，不能用自己挑的 id。
- **所有输入立即深拷贝。** 传入的 leaf 结构体可以是栈上临时量，调用一返回就复用或丢弃——本 API 里没有任何"必须活过 commit"的生命周期推理。`SceneAddComplexFilter` 同样会拷贝 composition 的 `term_ids` / `term_counts` 堆数组，调用返回后即可释放它们。
- **逐调用校验。** 每个 `Add*` 在写入前校验这一个条目的形状与枚举；失败时场景保持完全不变（不留半写状态）。
- `SceneAddFilter` 只处理 SIMPLE 分支（`none` / `raypath` / `entry_exit` / `direction` / `crystal`）。`type == LUMICE_FILTER_TYPE_COMPLEX` 的 filter 会被拒绝——请用 `SceneAddComplexFilter`，它同时接收 filter 标识与其 composition，并忽略 `filter->type` / `filter->composition_index`。

**返回值**（全部 `Add*` 相同）：
- `LUMICE_OK`: 成功，已写入 `*out_id`
- `LUMICE_ERR_NULL_ARG`: `scene`、输入指针或 `out_id` 为 `NULL`
- `LUMICE_ERR_INVALID_CONFIG`: 条目非法（枚举错误、计数越界），或该类条目已达对应的 `LUMICE_MAX_CONFIG_*` 容量上限

#### 场景设置：`LUMICE_SceneSet*`

```c
LUMICE_ErrorCode LUMICE_SceneSetLightSource(LUMICE_Scene*, float sun_altitude, float sun_azimuth,
                                            float sun_diameter, const char* spectrum);
LUMICE_ErrorCode LUMICE_SceneSetCustomSpectrum(LUMICE_Scene*, const LUMICE_SpectrumEntry*, int count);
LUMICE_ErrorCode LUMICE_SceneSetSimParams(LUMICE_Scene*, int infinite, LUMICE_RayCount ray_num,
                                          int max_hits, int geom_clock);
LUMICE_ErrorCode LUMICE_SceneSetRayAllocation(LUMICE_Scene*, int mode);
LUMICE_ErrorCode LUMICE_SceneSetColorMode(LUMICE_Scene*, int raypath_color_mode);
```

每个 `Set*` 都是幂等的（后写覆盖先写）且可以任意顺序调用。`scene` 为 `NULL` 返回 `LUMICE_ERR_NULL_ARG`，值非法返回 `LUMICE_ERR_INVALID_CONFIG` / `LUMICE_ERR_INVALID_VALUE`。

`LUMICE_SceneSetRayAllocation`（v4.38）设置 `scene.ray_allocation`——`doc/configuration.md` 里的逐晶体光线分配模式。`mode` 取 `LUMICE_RAY_ALLOCATION_PROPORTIONAL`（0，落盘字符串 `"proportional"`：每个晶体按人口占比分到光线）或 `LUMICE_RAY_ALLOCATION_ADAPTIVE`（1，`"adaptive"`：按各晶体实测的逐光线能量方差分配——期望画面不变，噪声在晶体之间更均匀）。它有意不做成位置参数的 `LUMICE_SceneSetSimParams` 的一个参数。从不调用它的句柄不写这个键，commit 时由 core 默认值决定；`LUMICE_SceneFromJson` / `ToJson` 仍逐字保留文档自己的拼写。

光源与光谱存在交互，且设计上与调用顺序无关：离散光谱（`SetCustomSpectrum` 且 `count > 0`）优先于 `spectrum` 字符串，因此 `SetLightSource` **不会**覆盖已设置的离散光谱，两种调用顺序收敛到同一结果。`SetCustomSpectrum` 传 `count == 0` 清除离散光谱，回退到字符串（默认 `"D65"`）。

`SetSimParams` 接收 `infinite`（1 = 无限跑，0 = 跑满 `ray_num` 条光线后停）、`max_hits`，以及 `geom_clock`（几何重采样时钟 K；0 = 交由 core 推导默认值）。

#### 序列化：`LUMICE_SceneToJson` / `SceneFromJson` / `SceneFromJsonFile`

```c
LUMICE_ErrorCode LUMICE_SceneToJson(const LUMICE_Scene* scene, char* out_buf, size_t buf_size,
                                    size_t* out_len);
LUMICE_ErrorCode LUMICE_SceneFromJson(const char* json_str, LUMICE_Scene** out_scene);
LUMICE_ErrorCode LUMICE_SceneFromJsonFile(const char* filename, LUMICE_Scene** out_scene);
```

这是 JSON 编写侧，且完全不碰 `LUMICE_Server`：解析只产出句柄，仅此而已。往返无损——`SceneFromJson(SceneToJson(scene))` 与原场景语义等价。文档格式与 CLI 读取的格式相同，见 [配置文档](../configuration_zh.md)。

`SceneToJson` 使用 snprintf 风格的调用方缓冲区，不跨 ABI 分配内存：

- 传 `out_buf == NULL`（或 `buf_size == 0`）只查询长度、不写入。
- 缓冲区不足时输出被截断，但**始终**以 NUL 结尾。
- `out_len` 非 `NULL` 时总是写入完整（未截断）长度，因此 `*out_len >= buf_size` 即表示发生了截断。惯用两段式：先查询、按 `*out_len + 1` 分配、再调一次。
- `scene` 为 `NULL` 返回 `LUMICE_ERR_NULL_ARG`；场景无法序列化（例如某个 `Set*` 收到了非法 UTF-8 字符串）返回 `LUMICE_ERR_INVALID_CONFIG`。

`SceneFromJson` / `SceneFromJsonFile` 成功时向 `*out_scene` 写入一个全新句柄——所有权归调用方，最终必须 `SceneDestroy`。**任何失败路径上 `*out_scene` 都被置为 `NULL` 且不泄漏句柄**，因此调用方绝不会去销毁一个并未产出的句柄。

**返回值**：
- `LUMICE_OK`: 成功，`*out_scene` 持有新句柄
- `LUMICE_ERR_NULL_ARG`: `json_str` / `filename` 或 `out_scene` 为 `NULL`
- `LUMICE_ERR_INVALID_JSON`: 语法错误
- `LUMICE_ERR_MISSING_FIELD`: 缺少必需字段
- `LUMICE_ERR_INVALID_VALUE` / `LUMICE_ERR_INVALID_CONFIG`: 枚举或取值非法，或超出某个 `LUMICE_MAX_CONFIG_*` 软上限
- `LUMICE_ERR_FILE_NOT_FOUND`（仅 `SceneFromJsonFile`）: 文件无法打开

#### LUMICE_CommitScene

把场景提交给 server。这是本 API **唯一**的提交入口。

```c
LUMICE_ErrorCode LUMICE_CommitScene(LUMICE_Server* server, const LUMICE_Scene* scene,
                                    int* out_reused);
```

**参数**：
- `server`: 服务器句柄
- `scene`: 场景句柄。以 `const` 读取，**既不被消费也不被销毁**。server 不保留对它的引用，所有权仍在调用方手上，同一句柄可以反复提交（用 `Add*`/`Set*` 改一改再提交一次）。
- `out_reused`: 可选（可为 `NULL`）。非 `NULL` 时：本次提交复用了既有 consumer/renderer（渲染器布局未变）写 `1`，重建了写 `0`。

**返回值**：
- `LUMICE_OK`: 成功——server 立即停止当前任务并开始新任务
- `LUMICE_ERR_NULL_ARG`: `server` 或 `scene` 为 `NULL`
- `LUMICE_ERR_INVALID_CONFIG` / `LUMICE_ERR_MISSING_FIELD` / `LUMICE_ERR_INVALID_VALUE` / `LUMICE_ERR_INVALID_JSON`: core 拒绝了该配置
- `LUMICE_ERR_SERVER`: server 侧失败

任何错误路径上 `*out_reused` 都保持不变。注意这里不做整场景重校验：每个 `Add*`/`Set*` 都已校验过自己的输入，这里还能暴露的是 core 的跨字段 / 语义拒绝（例如 `max_hits` 越界）。

### 结果获取

结果从**结果帧**（result frame）读取——一份不可变的仿真快照，调用方持有它的一份生命周期。
取帧、读其四个视图中需要的若干个、释放：

```c
LUMICE_ResultFrame* frame = NULL;
if (LUMICE_AcquireResultFrame(server, &frame) == LUMICE_OK) {
    LUMICE_RenderResult renders[LUMICE_MAX_RENDER_RESULTS + 1];
    LUMICE_FrameGetRender(frame, renders, LUMICE_MAX_RENDER_RESULTS);
    // ... 使用 renders[i].img_buffer ...
    LUMICE_ReleaseResultFrame(frame);
}
```

**为什么是帧、而不是 server 上的 getter**：读取返回的缓冲区是库内存的视图。在旧的
server-taking getter 下，这块内存只在**下一次** getter 调用之前有效，于是一个读者的读取会
让另一个读者仍在使用的像素失效——那是 use-after-free，而不只是读到陈旧数据。帧给读者一份
真实的生命周期份额，像素一直有效到**该读者**释放**自己那一帧**为止。

模型本身还免费带来两条性质：
- **同世代由构造保证**：同一帧上的任意两次读取描述的是同一次快照，不再需要一个合并的
  「xyz + composite」入口去凑同世代。
- **帧之间互相独立**：取第二帧不影响第一帧，调用方想持有多少帧都可以——跨 server 销毁也成立。

四个读取函数保持了它们所替代的 getter 的数组+哨兵形态，只有第一个参数不同：
- `out` 数组大小至少为 `max_count + 1`（含哨兵位）
- 有效结果之后紧跟一个全零的哨兵条目
- 调用方循环到哨兵自然结束
- 帧或 `out` 为 `NULL` 时一律返回 `LUMICE_ERR_NULL_ARG`

#### LUMICE_AcquireResultFrame

```c
LUMICE_ErrorCode LUMICE_AcquireResultFrame(LUMICE_Server* server, LUMICE_ResultFrame** out_frame);
```

物化一次待处理的快照（与它替代的 getter 一致），然后向 `*out_frame` 写入一个新的帧句柄。
调用方拥有该句柄，**必须**最终传给 `LUMICE_ReleaseResultFrame`。

**返回值**：
- `LUMICE_OK`: 成功。`*out_frame` 永不为 `NULL`——即便在首次快照之前，此时帧只是读作
  「无结果」（每个 `LUMICE_FrameGet*` 写出各自的哨兵 / 全零结构体）
- `LUMICE_ERR_NULL_ARG`: `server` 或 `out_frame` 为 `NULL`

#### LUMICE_ReleaseResultFrame

```c
void LUMICE_ReleaseResultFrame(LUMICE_ResultFrame* frame);
```

NULL-safe no-op（与 `LUMICE_DestroyServer` 同契约）。每个句柄必须恰好释放一次；重复释放同一
句柄是未定义行为——这与 `LUMICE_DestroyServer` 以及本 API 中其他所有句柄一致，因此**刻意不设**
double-free 哨兵。同一帧句柄仍有线程在调用 `FrameGet*`，或仍在使用从它借出的指针时，不得释放
该句柄；等待这些操作全部结束后再释放一次。各次独立 acquire 得到的帧句柄不共享这层 wrapper
生命周期，可以彼此并发地读取或释放。

**忘记释放的失效模式**：只泄漏那一帧，仅此而已。它不会破坏内存、也不会影响任何其他读者，
因为帧是不可变的、且各自独立引用计数；而泄漏正是 ASan/LSan/valgrind 本来就会报的东西。
释放之后，从该帧读出的每个指针都是悬垂的——需要保留的内容请先拷贝。

#### LUMICE_FrameGetRender

mono / 全光谱 sRGB uint8 图像。

```c
LUMICE_ErrorCode LUMICE_FrameGetRender(const LUMICE_ResultFrame* frame, LUMICE_RenderResult* out, int max_count);
```

**哨兵**：末尾条目的 `img_buffer == NULL`。该路径上 `composite_p99_y` 保持为 0，应忽略。
`max_count` 推荐使用 `LUMICE_MAX_RENDER_RESULTS`。

#### LUMICE_FrameGetComposite

per-raypath 合成 sRGB 图像，每个染色 renderer 一张。

```c
LUMICE_ErrorCode LUMICE_FrameGetComposite(const LUMICE_ResultFrame* frame, LUMICE_RenderResult* out, int max_count);
```

**哨兵**：末尾条目的 `img_buffer == NULL`；未配置 `raypath_color` 时结果为空（`out[0]` 即哨兵），
mono 路径不受影响。`composite_p99_y` 在这条路径上有意义。

#### LUMICE_FrameGetRawXyz

原始 XYZ 浮点数据与强度标量。

```c
LUMICE_ErrorCode LUMICE_FrameGetRawXyz(const LUMICE_ResultFrame* frame, LUMICE_RawXyzResult* out, int max_count);
```

**哨兵**：末尾条目的 `xyz_buffer == NULL`。

#### LUMICE_FrameGetStats

仿真统计——单个值，因此没有 `max_count`。

```c
LUMICE_ErrorCode LUMICE_FrameGetStats(const LUMICE_ResultFrame* frame, LUMICE_StatsResult* out);
```

帧不携带统计数据时（例如取自首次快照之前）写出全零结构体。

### 状态与控制

#### LUMICE_QueryServerState

查询服务器状态。

```c
LUMICE_ErrorCode LUMICE_QueryServerState(LUMICE_Server* server, LUMICE_ServerState* out);
```

**参数**：
- `server`: 服务器句柄指针
- `out`: 输出状态指针

**返回值**：
- `LUMICE_OK`: 成功，状态写入 `*out`
- `LUMICE_ERR_NULL_ARG`: `server` 或 `out` 为 `NULL`

#### LUMICE_GetSimLifecycle

读取权威的三态生命周期、当前 epoch 与 session 类型；`LUMICE_QueryServerState` 只是它的二态投影。

```c
typedef enum LUMICE_SimLifecycle_ {
  LUMICE_LIFECYCLE_IDLE = 0,
  LUMICE_LIFECYCLE_RUNNING,
  LUMICE_LIFECYCLE_COMPLETED,
} LUMICE_SimLifecycle;

typedef enum LUMICE_SessionKind_ {
  LUMICE_SESSION_RENDER = 0,
  LUMICE_SESSION_ANALYSIS = 1,
} LUMICE_SessionKind;

typedef struct LUMICE_SimLifecycleResult_ {
  int lifecycle;
  unsigned long long epoch;
  int session_kind;
} LUMICE_SimLifecycleResult;

LUMICE_ErrorCode LUMICE_GetSimLifecycle(LUMICE_Server* server, LUMICE_SimLifecycleResult* out);
```

- `COMPLETED` 表示有限 run 的生产侧已自然完成；需要终值统计时仍须等待下面的 drain 相等判据
- 每次 `LUMICE_CommitScene`、`LUMICE_StartRaypathAnalysis` 或 `LUMICE_ContinueRender` 成功启动 run，
  `epoch` 都递增
- `session_kind` 只由 render commit 或 analysis start 改写；完成与 Stop 均保留它，因此停止后的
  analysis 仍读作 `ANALYSIS`，直到下一次 render commit
- 三个字段分别保持当前，但不构成彼此的单次原子快照

#### LUMICE_GetDrainStatus

读取消费端排空状态：当前世代的数据是否已被完全消费，即累计统计量是否已是终值。

```c
typedef struct LUMICE_DrainResult_ {
  unsigned long long drained_epoch;
  unsigned long long current_epoch;
} LUMICE_DrainResult;

LUMICE_ErrorCode LUMICE_GetDrainStatus(LUMICE_Server* server, LUMICE_DrainResult* out);
```

**参数**：
- `server`: 服务器句柄指针
- `out`: 输出 `{drained_epoch, current_epoch}` 的指针

**返回值**：
- `LUMICE_OK`: 成功
- `LUMICE_ERR_NULL_ARG`: `server` 或 `out` 为 `NULL`

**契约**：当且仅当 `drained_epoch == current_epoch` 时，当前世代已完全排空。
`current_epoch` 与 `LUMICE_GetSimLifecycle()` 报告的是同一个值，在本调用中一并采样，
调用方无需再发一次请求去比较。

**为什么等 `LUMICE_SERVER_IDLE` 不够**：`LUMICE_QueryServerState` /
`LUMICE_GetSimLifecycle` 的判据**全部在生产侧**——没有 simulator 在忙、没有待处理场景、
场景生成已结束，没有一条询问消费线程是否已把队列排空。统计量
（`LUMICE_FrameGetStats` → `crystal_num` / `orientation_num` / `sim_ray_num`）是快照时刻
冻结的累加值，因此在服务器首次报告 IDLE 的瞬间读取，可能拿到**部分总和**——实测
`orientation_num` 为 19616、期望 20000，恰好差整数个派发单位。任何需要终值的读者都必须
轮询本调用，而不是那个状态枚举。

**开销**：O(1) 原子读，与 `LUMICE_GetSimRayCount` 同一档——无快照、无渲染、无锁，
可以每轮轮询都调用。

**边界情况**：
- 无限运行（`ray_num` 无上限）永远不会排空，与其生命周期永远不会到达 `COMPLETED` 同形。
- `LUMICE_StopServer()` **不会**发布排空信号。停止会丢弃仍在排队的数据，因此"已停止"
  与"已排空"保持可区分，而不是被当作一次干净的完成上报。
- `drained_epoch` 单调且从不重置。新一次提交或 `LUMICE_ContinueRender` 会让
  `current_epoch` 越过它，相等判据自身就会读作“尚未排空”。

**典型用法**：

```c
LUMICE_DrainResult drain;
for (;;) {
    if (LUMICE_GetDrainStatus(server, &drain) != LUMICE_OK) { /* 错误处理 */ }
    if (drain.drained_epoch == drain.current_epoch) {
        break;  // 统计量已是终值，可以安全获取 result frame 并读取
    }
    sleep_ms(10);
}
```

#### LUMICE_StopServer

停止服务器。

```c
void LUMICE_StopServer(LUMICE_Server* server);
```

**参数**：
- `server`: 服务器句柄指针，可以为 `NULL`（安全）

**注意**：
- 停止后可以继续提交新配置
- 停止不会释放服务器资源，需要调用 `LUMICE_DestroyServer()` 释放
- 停止后的 render 保留累积结果，可由 `LUMICE_ContinueRender()` 继续追加

#### LUMICE_ContinueRender

不重置累积结果，为最近一次 render 追加新光线（v4.44）。

```c
LUMICE_ErrorCode LUMICE_ContinueRender(LUMICE_Server* server, int infinite,
                                       LUMICE_RayCount additional_ray_num);
```

- 只适用于已经提交过 render、且当前没有 run 正在执行的 render session；analysis session 必须先
  重新提交 render
- `additional_ray_num` 是跨波长的**增量**而非新总量；`infinite != 0` 时忽略该值并运行到 Stop
- 成功后保留此前图像、统计、`emitted_energy` 与曝光锚点，同时启动新 epoch；有限追加完成后仍须
  按 `LUMICE_GetDrainStatus` 等待终值
- 无可继续的 render、run 正在执行或完成批次未能在内部时限内排空时返回
  `LUMICE_ERR_SERVER`，且拒绝路径不改变现有状态

## 使用示例

### 基础示例

```c
#include "lumice_engine.h"
#include <stdio.h>

int main() {
    // 1. 创建服务器
    LUMICE_Server* server = LUMICE_CreateServer();
    if (!server) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }

    // 2. 把配置文件解析成场景句柄，提交，然后释放句柄。
    //    （server 不保留对 scene 的引用，所以 commit 一返回就可以销毁它。）
    LUMICE_Scene* scene = NULL;
    if (LUMICE_SceneFromJsonFile("config.json", &scene) != LUMICE_OK) {
        LUMICE_DestroyServer(server);
        return 1;
    }
    LUMICE_ErrorCode commit_err = LUMICE_CommitScene(server, scene, NULL);
    LUMICE_SceneDestroy(scene);
    if (commit_err != LUMICE_OK) {
        LUMICE_DestroyServer(server);
        return 1;
    }

    // 4. 轮询结果
    while (1) {
        usleep(1000000);  // 1 second

        LUMICE_DrainResult drain;
        if (LUMICE_GetDrainStatus(server, &drain) != LUMICE_OK) {
            break;  // 生产代码应处理错误
        }
        int done = drain.drained_epoch == drain.current_epoch;

        // 在 drain 采样之后取帧，保证 done 这一轮读到的是终值；
        // 下面的渲染结果与统计读自同一次快照
        LUMICE_ResultFrame* frame = NULL;
        if (LUMICE_AcquireResultFrame(server, &frame) == LUMICE_OK) {
            LUMICE_RenderResult renders[LUMICE_MAX_RENDER_RESULTS + 1];
            if (LUMICE_FrameGetRender(frame, renders, LUMICE_MAX_RENDER_RESULTS) == LUMICE_OK) {
                for (int i = 0; renders[i].img_buffer != NULL; i++) {
                    printf("Render[%02d]: %dx%d\n",
                           renders[i].renderer_id, renders[i].img_width, renders[i].img_height);
                }
            }

            LUMICE_StatsResult stats;
            if (LUMICE_FrameGetStats(frame, &stats) == LUMICE_OK) {
                printf("Stats: rays=%llu, crystals=%llu, orientations=%llu\n",
                       stats.sim_ray_num, stats.crystal_num, stats.orientation_num);
            }

            LUMICE_ReleaseResultFrame(frame);
        }

        if (done) {
            break;
        }
    }

    // 5. 销毁服务器
    LUMICE_DestroyServer(server);
    return 0;
}
```

### 完整示例（包含错误处理）

```c
#include "lumice_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <config.json>\n", argv[0]);
        return 1;
    }

    // 1. 创建服务器
    LUMICE_Server* server = LUMICE_CreateServer();
    if (!server) {
        fprintf(stderr, "Error: Failed to create server\n");
        return 1;
    }

    // 2. 设置日志级别（可选，默认 INFO）
    LUMICE_SetLogLevel(server, LUMICE_LOG_INFO);

    // 3. 加载配置：文件 -> 场景句柄 -> 提交 -> 释放句柄
    LUMICE_Scene* scene = NULL;
    LUMICE_ErrorCode err = LUMICE_SceneFromJsonFile(argv[1], &scene);
    if (err != LUMICE_OK) {
        fprintf(stderr, "Error: Failed to parse config (error code: %d)\n", err);
        LUMICE_DestroyServer(server);
        return 1;
    }
    err = LUMICE_CommitScene(server, scene, NULL);
    LUMICE_SceneDestroy(scene);   // 无论提交成功与否，句柄都归我们释放
    if (err != LUMICE_OK) {
        fprintf(stderr, "Error: Failed to commit config (error code: %d)\n", err);
        LUMICE_DestroyServer(server);
        return 1;
    }

    // 4. 轮询结果
    while (1) {
        usleep(1000000);

        LUMICE_DrainResult drain;
        if (LUMICE_GetDrainStatus(server, &drain) != LUMICE_OK) {
            fprintf(stderr, "Error: Failed to query drain status\n");
            break;
        }
        int done = drain.drained_epoch == drain.current_epoch;

        // 每次轮询在 drain 采样后取一帧，因此 done 这一轮是终值；
        // 在帧释放之前图像一直有效，无论其间别的线程读了什么。
        LUMICE_ResultFrame* frame = NULL;
        if (LUMICE_AcquireResultFrame(server, &frame) == LUMICE_OK) {
            LUMICE_RenderResult renders[LUMICE_MAX_RENDER_RESULTS + 1];
            if (LUMICE_FrameGetRender(frame, renders, LUMICE_MAX_RENDER_RESULTS) == LUMICE_OK) {
                for (int i = 0; renders[i].img_buffer != NULL; i++) {
                    printf("Render[%02d]: %dx%d, buffer=%p\n",
                           renders[i].renderer_id, renders[i].img_width, renders[i].img_height,
                           (const void*)renders[i].img_buffer);

                    // 保存图像（示例）
                    char filename[256];
                    snprintf(filename, sizeof(filename), "output_%d_%dx%d.raw",
                             renders[i].renderer_id, renders[i].img_width, renders[i].img_height);
                    FILE* img_file = fopen(filename, "wb");
                    if (img_file) {
                        size_t img_size = (size_t)renders[i].img_width * renders[i].img_height * 3;
                        fwrite(renders[i].img_buffer, 1, img_size, img_file);
                        fclose(img_file);
                    }
                }
            }

            LUMICE_StatsResult stats;
            if (LUMICE_FrameGetStats(frame, &stats) == LUMICE_OK) {
                printf("Stats: rays=%llu, segments=%llu, crystals=%llu, orientations=%llu\n",
                       stats.sim_ray_num, stats.ray_seg_num, stats.crystal_num, stats.orientation_num);
            }

            LUMICE_ReleaseResultFrame(frame);
        }

        if (done) {
            break;
        }
    }

    // 5. 清理
    LUMICE_DestroyServer(server);
    return 0;
}
```

## 错误处理

### LUMICE_ErrorCode 错误码

所有配置/查询/结果获取 API 返回 `LUMICE_ErrorCode`。最佳实践：

```c
LUMICE_Scene* scene = NULL;
LUMICE_ErrorCode err = LUMICE_SceneFromJsonFile("config.json", &scene);
if (err != LUMICE_OK) {
    switch (err) {
        case LUMICE_ERR_NULL_ARG:
            fprintf(stderr, "Null argument\n");
            break;
        case LUMICE_ERR_FILE_NOT_FOUND:
            fprintf(stderr, "Config file not found\n");
            break;
        case LUMICE_ERR_INVALID_JSON:
            fprintf(stderr, "Invalid JSON format\n");
            break;
        default:
            fprintf(stderr, "Error code: %d\n", err);
            break;
    }
}
```

### 错误处理最佳实践

1. **检查错误码**：所有返回 `LUMICE_ErrorCode` 的函数都应检查返回值
2. **检查创建结果**：`LUMICE_CreateServer()` 返回 `NULL` 表示失败
3. **资源清理**：确保在所有退出路径上调用 `LUMICE_DestroyServer()`

## 线程安全性

### Server 调用与帧读取有不同的生命周期

只读 server 轮询（`LUMICE_QueryServerState`、`LUMICE_GetSimLifecycle`、
`LUMICE_GetDrainStatus`、`LUMICE_GetSimRayCount`）与 `LUMICE_AcquireResultFrame` 可以在
server 句柄存活期间由多个线程调用。这些调用可以彼此并发，也可以与下文串行化的 run 启动/停止
操作并发；内部同步会在调用期间保护 server 状态，但**不会**延长 `LUMICE_Server*` wrapper 的
生命周期。

因此，`LUMICE_DestroyServer` 是单独的销毁屏障。调用前，owner 必须停止接受所有使用该 server
句柄的新调用，并通过 join 或外部同步等待全部在途调用返回。`DestroyServer` 不得与轮询、取帧、
修改或任何其他解引用同一 server 句柄的调用重叠；返回后任何 server 调用都不再合法。

每次成功的 `LUMICE_AcquireResultFrame` 都返回一个独立句柄，持有不可变发布帧的一份引用。已取得
的帧可以比 server 活得更久，彼此互不干扰。任意数量的线程都可以在同一存活帧上调用
`LUMICE_FrameGet*`，读侧无需加锁。`FrameGet*` 返回的指针（`img_buffer` / `xyz_buffer`）仍然是
该帧内部的借用，因此同一帧句柄上的全部 `FrameGet*` 调用与借出指针使用结束前，不得调用
`LUMICE_ReleaseResultFrame`。每个句柄只释放一次；各次独立 acquire 得到的句柄可以彼此并发地
读取或释放。

`LUMICE_FrameGetStats` 不属于指针借用族：它拷贝的是一个不含指针的**纯值**结构体，所以调用
返回后不会留下悬垂指针。它使用相同的取帧/释放包络，只是因为所有结果读取都来自帧。

### 需要串行化的 API

以下边界需要调用方串行化；这不表示存活 server 上已有文档说明的只读调用与每个非销毁 owner
操作之间都要相互排斥：
- `LUMICE_CreateServer()` / `LUMICE_CreateServerEx()`：首次创建时会注册进程级全局 logger sink，
  不保证并发创建安全。
- `LUMICE_CommitScene()` / `LUMICE_StartRaypathAnalysis()` / `LUMICE_ContinueRender()` /
  `LUMICE_StopServer()`：由一个 owner 线程串行执行 run 启动与停止操作。
- `LUMICE_DestroyServer()`：执行上文完整的 server 句柄销毁屏障。
- 对同一个 `LUMICE_Scene*` 句柄的修改（`LUMICE_SceneAdd*` / `LUMICE_SceneSet*` /
  `LUMICE_SceneDestroy`）：句柄内部没有加锁。不同句柄互相独立，两个线程各建各的场景是安全的。

### 多线程使用建议

1. **单服务器多线程**：指定一个 owner 线程串行执行 `LUMICE_CommitScene()` /
   `LUMICE_StartRaypathAnalysis()` / `LUMICE_ContinueRender()` / `LUMICE_StopServer()`。
   server 仍存活时，读线程可以并发轮询和取帧。销毁时先阻止新的 server 调用，并 join 或以其他
   方式等待全部在途 server 调用结束，再调用 `LUMICE_DestroyServer()`。
2. **跨销毁持有帧**：已经取得的帧可在 server 销毁后继续由读线程持有。释放某个帧句柄前，先
   等待该句柄的全部使用者结束；句柄仍被持有时，并发 `FrameGet*` 读取无需互斥锁。
3. **多服务器**：每个线程使用独立的服务器实例。

## 与其他语言集成

### Python集成（使用ctypes）

```python
import ctypes
import json

# 加载库
lib = ctypes.CDLL('./liblumice.so')  # Linux
# lib = ctypes.CDLL('./liblumice.dylib')  # macOS

# 定义类型
class LUMICE_RenderResult(ctypes.Structure):
    _fields_ = [
        ('renderer_id', ctypes.c_int),
        ('img_width', ctypes.c_int),
        ('img_height', ctypes.c_int),
        ('img_buffer', ctypes.POINTER(ctypes.c_ubyte)),
    ]

class LUMICE_StatsResult(ctypes.Structure):
    _fields_ = [
        ('ray_seg_num', ctypes.c_ulonglong),
        ('sim_ray_num', ctypes.c_ulonglong),
        ('crystal_num', ctypes.c_ulonglong),
        ('orientation_num', ctypes.c_ulonglong),
    ]

class LUMICE_DrainResult(ctypes.Structure):
    _fields_ = [
        ('drained_epoch', ctypes.c_ulonglong),
        ('current_epoch', ctypes.c_ulonglong),
    ]

LUMICE_MAX_RENDER_RESULTS = 16
LUMICE_MAX_STATS_RESULTS = 1

# 定义函数签名
lib.LUMICE_CreateServer.restype = ctypes.c_void_p
lib.LUMICE_DestroyServer.argtypes = [ctypes.c_void_p]
lib.LUMICE_SceneFromJsonFile.argtypes = [ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
lib.LUMICE_SceneFromJsonFile.restype = ctypes.c_int
lib.LUMICE_CommitScene.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
lib.LUMICE_CommitScene.restype = ctypes.c_int
lib.LUMICE_SceneDestroy.argtypes = [ctypes.c_void_p]
lib.LUMICE_SceneDestroy.restype = None
# 帧句柄是不透明的，绑定就只有 c_void_p——没有结构体布局需要镜像。
lib.LUMICE_AcquireResultFrame.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
lib.LUMICE_AcquireResultFrame.restype = ctypes.c_int
lib.LUMICE_ReleaseResultFrame.argtypes = [ctypes.c_void_p]
lib.LUMICE_ReleaseResultFrame.restype = None
lib.LUMICE_FrameGetRender.argtypes = [ctypes.c_void_p, ctypes.POINTER(LUMICE_RenderResult), ctypes.c_int]
lib.LUMICE_FrameGetRender.restype = ctypes.c_int
lib.LUMICE_FrameGetStats.argtypes = [ctypes.c_void_p, ctypes.POINTER(LUMICE_StatsResult)]
lib.LUMICE_FrameGetStats.restype = ctypes.c_int
lib.LUMICE_GetDrainStatus.argtypes = [ctypes.c_void_p, ctypes.POINTER(LUMICE_DrainResult)]
lib.LUMICE_GetDrainStatus.restype = ctypes.c_int
lib.LUMICE_StopServer.argtypes = [ctypes.c_void_p]

# 使用示例
def simulate(config_file):
    server = lib.LUMICE_CreateServer()
    if not server:
        raise RuntimeError("Failed to create server")

    try:
        scene = ctypes.c_void_p()
        err = lib.LUMICE_SceneFromJsonFile(config_file.encode('utf-8'), ctypes.byref(scene))
        if err != 0:
            raise RuntimeError(f"Failed to parse config (error: {err})")
        try:
            err = lib.LUMICE_CommitScene(server, scene, None)
            if err != 0:
                raise RuntimeError(f"Failed to commit config (error: {err})")
        finally:
            lib.LUMICE_SceneDestroy(scene)

        import time
        while True:
            time.sleep(1)

            drain = LUMICE_DrainResult()
            if lib.LUMICE_GetDrainStatus(server, ctypes.byref(drain)) != 0:
                raise RuntimeError("Failed to query drain status")
            done = drain.drained_epoch == drain.current_epoch

            # 在 drain 采样后取帧，保证 done 这一轮读到终值
            frame = ctypes.c_void_p()
            if lib.LUMICE_AcquireResultFrame(server, ctypes.byref(frame)) == 0:
                try:
                    renders = (LUMICE_RenderResult * (LUMICE_MAX_RENDER_RESULTS + 1))()
                    if lib.LUMICE_FrameGetRender(frame, renders, LUMICE_MAX_RENDER_RESULTS) == 0:
                        for r in renders:
                            if not r.img_buffer:
                                break
                            print(f"Render[{r.renderer_id:02d}]: {r.img_width}x{r.img_height}")
                finally:
                    lib.LUMICE_ReleaseResultFrame(frame)

            if done:
                break
    finally:
        lib.LUMICE_DestroyServer(server)
```

### Rust集成

```rust
use std::ffi::CString;
use std::os::raw::{c_char, c_int, c_uchar, c_ulonglong, c_void};

const LUMICE_MAX_RENDER_RESULTS: usize = 16;
const LUMICE_MAX_STATS_RESULTS: usize = 1;

#[repr(C)]
struct LUMICE_RenderResult {
    renderer_id: c_int,
    img_width: c_int,
    img_height: c_int,
    img_buffer: *const c_uchar,
}

#[repr(C)]
struct LUMICE_StatsResult {
    ray_seg_num: c_ulonglong,
    sim_ray_num: c_ulonglong,
    crystal_num: c_ulonglong,
    orientation_num: c_ulonglong,
}

#[link(name = "lumice")]
extern "C" {
    fn LUMICE_CreateServer() -> *mut c_void;
    fn LUMICE_DestroyServer(server: *mut c_void);
    fn LUMICE_SceneFromJsonFile(filename: *const c_char, out_scene: *mut *mut c_void) -> c_int;
    fn LUMICE_CommitScene(server: *mut c_void, scene: *const c_void, out_reused: *mut c_int) -> c_int;
    fn LUMICE_SceneDestroy(scene: *mut c_void);
    fn LUMICE_AcquireResultFrame(server: *mut c_void, out_frame: *mut *mut c_void) -> c_int;
    fn LUMICE_ReleaseResultFrame(frame: *mut c_void);
    fn LUMICE_FrameGetRender(frame: *const c_void, out: *mut LUMICE_RenderResult, max_count: c_int) -> c_int;
    fn LUMICE_FrameGetStats(frame: *const c_void, out: *mut LUMICE_StatsResult) -> c_int;
    fn LUMICE_QueryServerState(server: *mut c_void, out: *mut c_int) -> c_int;
    fn LUMICE_StopServer(server: *mut c_void);
}
```

## 常见问题

### Q1: 如何从文件读取配置？

**A**: 用 `LUMICE_SceneFromJsonFile()` 把文件解析成场景句柄，再用 `LUMICE_CommitScene()` 提交，最后 `LUMICE_SceneDestroy()` 释放句柄。若文档已在内存里，改用 `LUMICE_SceneFromJson()`，其余相同。解析与提交拆成两步是有意为之：解析完全不碰 server，因此可以在不打断正在跑的仿真的前提下校验或编辑配置。

### Q2: 图像数据格式是什么？

**A**: RGB格式，每个像素3个字节（R, G, B），按行存储。图像大小 = `img_width * img_height * 3` 字节。

### Q3: img_buffer 何时失效？

**A**: 直到你释放它所属的那一帧。`img_buffer` 是 `LUMICE_ResultFrame` 的只读视图，持有帧即保住像素——其他独立帧句柄的动作、以及新的快照，都无法在你持有期间让它失效。同一帧句柄的全部使用者结束后，才能执行它唯一一次 `LUMICE_ReleaseResultFrame()`；需要活得比帧更久的数据请提前 `memcpy`。

### Q4: 服务器可以同时处理多个配置吗？

**A**: 不可以。每次调用 `LUMICE_CommitScene()` 会停止当前任务并开始新任务。如需并行处理，应创建多个服务器实例。（同一个场景句柄可以提交给多个 server——commit 以 `const` 读取它且不保留引用。）

### Q5: 如何判断结果数组是否为空？

**A**: 检查数组第一个元素是否为哨兵。渲染与 composite 结果：`renders[0].img_buffer == NULL` 表示无结果；raw XYZ 为 `xyz[0].xyz_buffer == NULL`。`LUMICE_FrameGetStats` 写的是单个值而非数组，其「无结果」形态是全零结构体（`sim_ray_num == 0`）。

## 相关文档

- [配置文档](../configuration_zh.md) - 配置格式详细说明
- [系统架构文档](../architecture_zh.md) - 系统架构设计
- [开发指南](../developer-guide_zh.md) - 开发指南
- [文档索引](../README_zh.md) - 所有文档的导航
