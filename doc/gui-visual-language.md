# 设计：GUI 视觉语言（排版 / 色彩 / 尺寸节奏 / 信息形态）

> 状态：blueprint，已由可运行原型逐项验证（2026-08-13）；**§4 的定案已落地并留在 `main` 上**（PR #271）。
> 改动 GUI 外观、面板排版、控件形态之前先读。
>
> ⚠️ **2026-08-26 更正时态**：与本文并行的面板形态重排（`doc/gui-layout-architecture.md`）曾经实施，
> 随后被小范围内测否决并从 `main` 回退，见该文 §8。今天 `main` 上是**老 shell + 本文的视觉语言**。
> 本文凡提到「docking 迁移」处一律读作「将来任何一次面板重排」，而不是一件在途的事。
> ⚠️ **2026-08-26 再更正**：上一版此处写「内测反馈是聚合的，不构成外观已被接受的证据」。更细粒度的反馈随后到手，该说法作废——**配色被接受**（用户对配色的接受范围很宽），被拒的是形态。⇒ 本文的视觉语言层留在 `main` 上现在有直接证据支持。
> ⚠️ 但**别把这份接受扩大解释**：被问到的是**配色**。§4 的其余条目（正文字体、尺寸节奏、语义色）未被单独问及，它们留在 `main` 上仍然只是「没有被反对」，不是「已获用户验收」。
>
> 关联：`doc/gui-state-governance.md`（状态转换治理，与本文正交——那份管"值怎么变"，本文管"怎么显示"）、`doc/testing-architecture.md` §4.6（视觉回归参考图的重拍成本）。

## 0. 为什么有这份文档

起因是两个并行的诉求：一是 backlog 里的长期功能（光路成分分析、可调背景颜色）在当前 GUI 上"能做但别扭"；二是外观上的"工业风、粗糙"感。

第二条通常会被当成主观品味而无法讨论。本文的作用是把它拆成**可指认、可测量、可验证**的若干条，并记录哪些方向被原型证伪了——因为那几条正是最容易被重新提出的。

## 1. 诊断：外观从未被设计过

两行代码是全部证据：

- `src/gui/font_init.hpp` — `io.Fonts->AddFontDefault()`，即 ImGui 内置的 ProggyClean 13px 像素字体。**从未选过正文字体。**
- `src/gui/main.cpp` — `ImGui::StyleColorsDark()`，**调色板零定制**。全树对 style 的改动只有十余处局部 `PushStyleVar`（边框粗细、padding）。

⇒ 所谓"粗糙"不是设计做坏了，是**没有设计**。这把问题从品味之争挪回工程动作。

**同时存在一个结构缺口**：`src/gui/main.cpp` 与 `test/gui/test_gui_main.cpp` **各自重复**了一遍 `StyleColorsDark()` + `LoadFontAtlas()`。这意味着 `gui_test` 截出的图与真 app 的外观只是**碰巧**一致，而非在构造上一致。任何视觉工作的第一步都应是把这条缝合上（单一 owner），否则截图不构成对产品外观的证据。

## 2. 核心结论：精致感 = 秩序，不是留白

**秩序 = 重复 + 对齐 + 量化。** 三者都与"给多少空间"无关。

反直觉但决定性的一条：**密度不是精致的敌人，不一致才是**。作为对照的参考应用（卫星态势感知类）信息密度**高于**本应用，却明显更有条理，其手段全是重复类的——列表行等高、成组按钮等尺寸、几十行共享同一条竖直参考线、间距只有很少几种取值、分组靠**边界**（面板边缘、表头）而非拉大空隙来划分。

具体到本应用，破坏秩序的机制按可指认程度排序：

1. **行高不统一** —— slider 行 / combo 行 / checkbox 行 / radio 行各有高度，竖直方向没有节拍。
2. **标签的 x 位置不一致** —— slider 行的标签落在右侧固定列上（`kLabelColWidth`），而 `ImGui::Checkbox` 把标题紧贴在方框之后，两者相差约 170px。眼睛沿着一条竖线往下读，每遇一个复选框就断一次。
3. **间距取值种类过多** —— `SeparatorText` 前后、`CollapsingHeader` 前后、行间、组间各一套，且没有共同基数。
4. **子标题粒度过细** —— Display 组内曾有三个 `SeparatorText` 管六行内容（一个标题管两行）。标题在这个粒度上消耗的注意力多于它组织的。
5. **一值两控件** —— `[slider][input]` 并排，同样信息量约两倍的盒子。这是**真实的易用性 affordance**（粗调 + 精确输入），不是错误；但 `DragFloat`（ctrl+click 键入）一个控件即可给到同样两件事。
6. **大片空区** —— 左面板约 70% 为空，预览区静止时为空。**这条不是样式问题，是信息架构问题**，只能靠"有东西可显示"解决，见 §6。

## 3. 色彩：色相与面积是两个独立旋钮

这一节的存在是因为**混淆这两个旋钮导致了一次完整的错误往返**，而该错误在纯目视下无法察觉。

从截图逐点采样（右面板同一位置，色相/饱和度）：

| 变体 | 输入框底色 | 分组标题条 | 面板底色 |
|---|---|---|---|
| 现状（`StyleColorsDark`） | 216° / **59%** | 212° / **65%** | 0° / **0%**（纯中性） |
| "低饱和蓝"第一版 | 218° / 19% | 222° / 25% | 225° / 17% |
| "加强蓝"第二版 | 223° / 30% | 218° / 43% | 220° / 35% |
| 定案 | 218° / 53% | 214° / 50% | 225° / 20% |

**结论：本应用现状本来就是一个高饱和蓝主题。** 前两版以"增加蓝色倾向"为名，实际把蓝**减掉了 2–3 倍**。真正的问题从来不是"要加多少蓝"，而是"**现状的蓝要保留多少**"。

而且注意现状的面板底色是**纯中性**：今天的结构已经是"中性底 + 蓝色元件"。所以问题不在色相，在**面积**——一条通栏实心的高饱和标题条，加上每个 slider 上都有的蓝色滑块。

定案的做法因此是：**保留接近现状的色相与饱和度在小元件上，削减被填充的面积**——分组标题条从实心条改为低透明度染色（alpha 0.16），slider 滑块静止时中性、仅在拖动中显示强调色。

⚠️ **纪律：色彩判断必须采样测量，不得凭目视断言。** 上述往返中，"看起来不够蓝"这个判断由两个人分别做出且一致，但它是错的——因为参照物（现状）比两个候选都蓝得多，而低亮度区的色相差异在目视下不可靠。

## 4. 已验证的定案

以下每条都由可运行原型截图验证过，数值可直接采用。

**4.1 正文字体** —— 换用比例字体（原型用 Roboto Medium 15px，另试过 Karla 16 / Droid Sans 15）。**这是单条改动中观感变化最大的一项**，且顺带改善了屏幕上的字形渲染质量。✅ 已定案并落地：Roboto Medium 15px，见 §7。

> **15px 是逻辑像素，物理尺寸 = 15 × `ui_scale`。** 本节及 §4.2 的全部数值都是 1x 设计基准（96 dpi，
> 也是 `gui_test` 参考图的拍摄尺度），不是跨平台不变的物理量。`ui_scale = 显示器内容缩放 × 用户倍率`，
> 单一 owner 是 `src/gui/theme.{hpp,cpp}`：`ApplyVisualLanguage(io, layout_scale, raster_density)`
> 每次从 `ImGuiStyle()` 基线重派生 style（`ScaleAllSizes`）、按 `round(15 × layout_scale)` 重建 atlas，
> 其余所有屏幕像素字面量在使用处经 `UiPx()`/`UiPxI()` 取值。两个参数按平台拆分（策略在 `main.cpp`
> 的 `ResolveUiScaleParams`，机制在 `theme.cpp`）：Windows/Linux 的 GLFW 窗口坐标就是物理像素，
> `layout_scale = monitor × multiplier`、`raster_density = 1`；macOS 的窗口坐标是逻辑点、OS 已经把
> 布局放大过，`layout_scale = multiplier`、`raster_density = monitor`——后者只提高 atlas 栅格化密度
> （`ImFontConfig::RasterizerDensity`）不改任何度量，Retina 上字形变清晰而窗口尺寸不变（已真机核对）。
> 三份状态各有一个 owner、不重复：`theme.cpp` 的 TU-local `g_layout_scale`（生效值，`CurrentUiScale()`
> 读它）；`app.cpp` 的 `g_ui_scale_multiplier`（用户倍率，`user_defaults.json` 的 `app.ui_scale_multiplier`
> 在窗口创建前由 `LoadUiScaleMultiplierAtStartup` 读入，Settings 面板经 `SetUiScaleMultiplierImmediate`
> 改写）；`main.cpp` 的 `g_monitor_scale_x/y`（`glfwGetWindowContentScale` 的最近一次读数，跨屏回调只改
> 它并置 `g_ui_scale_dirty`）。任一输入变化都走同一条路：下一帧顶部 `RebuildForUiScale` 重跑
> `ApplyVisualLanguage` → 重传字体纹理 → `PlanWindowSizeForScale` 抬高窗口下限并在必要时放大窗口。
> `UiPx()` 出口没有机械门禁（新写一个裸像素字面量不会被 checker 拦下），靠 code review 惯例维持，
> 这是已知取舍。

**4.2 尺寸节奏（量化）** —— 全部取值为 4 的倍数；关键在于**行距比现状更紧而非更松**，以抵消更大字号：

```
WindowPadding (8,6)  FramePadding (6,3)  CellPadding (4,2)
ItemSpacing (8,3)    ItemInnerSpacing (4,3)  IndentSpacing 16
ScrollbarSize 10     GrabMinSize 8
FrameBorderSize 0    WindowBorderSize/ChildBorderSize/PopupBorderSize 1
FrameRounding 2  GrabRounding 2  TabRounding 2  ChildRounding 3
ScrollbarRounding 3  WindowRounding 4  PopupRounding 4
```

两处值得单独说明：

- **`FrameBorderSize = 0`** —— 现状每个输入框 / 下拉 / 按钮都自画一圈边框，一列八个设置读起来是十六个盒子。层级改由间距节奏承担。
- **圆角收小** —— 大圆角在小行高上会吃掉承载对齐的那个角，反而更散。

实测收益：同一面板在字号更大的前提下，可见内容反而**多于**现状（现状 Overlay 组被截断，量化后连末尾的半径行都可见）。

**4.3 调色板** —— 见 §3 定案列。核心两条规则：**强调色不做大面积常驻填充，且滑条静止态不上色**；分组标题是**染色**不是实心条。

⚠️ 这条规则曾被写成"强调色只在**交互进行中**出现"，那个措辞是错的，且会主动否掉正确答案。`src/gui/theme.cpp` 的 `ApplyPalette` 自己就把 accent 给了**静止的**勾选态（`c[ImGuiCol_CheckMark] = p.accent`），而紧随其后那段注释解释的其实是**滑条**——"Grab neutral at rest, accent only while dragging"，理由是滑条是面板里重复次数最多的控件，它的静止色调定全局基调。所以规则真正管的是**面积**与**滑条**，不是"任何交互态都不许常驻上色"：勾选态本身就是内容（"这一项被选中"），涂色是内容的一部分，不是装饰性强调。同理，卡片的关联边框（`src/gui/panels.cpp` 的 active / co-shared 两态）、状态栏的 `Simulating...`、LINK 徽标都用 accent 而不违反这条——它们都是细线或短文本，且都在陈述"此刻是这个"。

**4.4 重复记录 → 表格** —— 形态错配的判据是：**若干条目共享同一记录结构**（同样的字段集），却用竖直堆叠 + 每条重复标签来表达。Overlay 的四个辅助线正是如此（颜色 / 线 / 文字标签 / 透明度，外加仅标记点具备的像素半径）。

表格化后 4 项 × 2 行 + 四次重复的 "Alpha" 标签 → **4 行 + 1 个表头**。附带两项收益：

- **空格子即信息** —— 一眼可见只有标记点没有文字标签、且只有它有半径。
- **不等宽字段进折叠行** —— 少数条目独有的额外操作（角度编辑器、像素半径）收到行末小箭头之后。这既避免了为少数派开一整列，又把那一列的宽度还给名称列，**恰好解决了名称被裁切的问题**。

**4.5 门禁关系：合并控件而非装饰它** —— `Infinite rays` 是 `Rays(M)` 的门禁（`src/gui/field_editor_registry.cpp` 中该字段的 applicability 谓词），但这层从属在视觉上完全不可见。

- ❌ **缩进被门禁的行** —— 给关系画装饰，实测观感不佳。
- ✅ **把开关合并进滑条最右端的档位** —— 关系直接消失，控件减少一个（减法优先）。

⚠️ 合并时必须守住一条：**`infinite` 不是"一个很大的数"，它改变的是终止语义**，与"一亿条光线"是质的不同而非量的不同。因此最右端应是一个显示文字（"until stopped"）而非数字的**独立档位**，且数值输入框必须保留——档位吞掉了最大有限值，精确的最大值必须仍可键入。这是该方案唯一的真实代价，由输入框兑付。

**4.6 子标题粒度** —— 一个 `SeparatorText` 至少要管得起自己占的那一行。Display 组由三个子标题压缩为一个，并把修饰性控件（横竖版切换）并回它所修饰的那一行（该控件原本独占一行，与它修饰的 Preset 被拆开）。

**4.7 左右面板宽度** —— 左面板 400 → 380。左面板承载的信息远少于右面板，等宽以上会读作失衡。

**4.8 顶栏：分组、量化与 Continue 的动作色**（as-built，2026-09-26）—— 顶栏（`RenderTopBar`，`src/gui/app_panels.cpp`）
原先按书写顺序机械排列，用裸字形 `"|"` 分组。现在的规则：

- **分组**（左到右）：左面板折叠 ｜ 执行组（Run/Stop、Continue）｜ 文件组（New/Open/Save）｜
  功能组（Colors/Analysis/Summary + Colored 复选框 + 告警 pip）｜ Settings …… ⚠ + Revert（右对齐）· 右面板折叠（贴右缘）。
- **组边界 = 主题化竖线**：`ToolbarGroupSeparator()`（`SeparatorEx(Vertical)`，颜色取 `ImGuiCol_Separator`，随色盘走）。
  两侧各一个 `ItemSpacing.x`，于是**组间距 = 2 × 组内间距 + 1px 竖线**——节奏只有 `ItemSpacing.x` 这一个来源，没有新的像素常量。
- **尺寸量化**：全栏按钮同一帧高（Revert 由 `SmallButton` 改为 `Button`，原先矮 6px）；**组内同宽**——
  执行组取 Run/Stop/Stopping.../Continue 四个标签的最大宽度，文件组取 New/Open/Save 的最大宽度（`ToolbarGroupButtonWidth()`，
  一处实现两处调用）。功能组与 Settings **不**拉齐：它们各开一个不同的窗口，拉齐只制造与内容无关的留白。
- **窄窗口预算**：在 `kMinWindowWidth`（1024）下、最宽内容（有色类 ⇒ `Full Spectrum` 复选框 + pip）实测需 1004px，余量 20px；
  改造前为 997px。⚠️ 曾试过组间距取 1.5 × `ItemSpacing.x`，需 1036px，**溢出 12px**——所以本栏没有更宽组间距的空间，
  下次往顶栏加东西前先看这个余量。由 `shell_chrome/the_top_bar_fits_at_the_minimum_window_width` 钉住，无降级分支（不需要）。
- **常隐藏的状态提示住在栏尾的空白里**：⚠ + Revert 仍是常驻提交、未修改时 alpha=0 + `BeginDisabled` 隐藏（「不跳动」约束：
  它出现/消失时任何按钮都不许平移，由 `shell_chrome/toggling_modified_moves_no_top_bar_button` 钉住）。它原本紧跟 Continue，
  而文档大多数时候是未修改的，于是那块隐藏矩形在栏里最密的一段常驻成一个洞。现在它右对齐贴在右面板折叠钮左侧——
  Settings 与右折叠钮之间在最小窗宽以上本来就是空白，隐藏态藏进的是本就空着的地方；右对齐而非紧跟 Settings，
  是为了读作栏尾的「状态角」而不是 Settings 组的一员（文档状态不属于偏好设置），也让它的 x 不随 Colored 复选框的有无而变。
  总宽预算不变（同一组控件只是换了位置，窄窗口仍需 1004px）。⭐可迁移判据：**常驻占位、但多数时间隐藏的控件，放在布局本就空闲的
  位置，而不是操作流中间**——占位的代价只有在它占的是本来有用的地方时才付。

**Continue 的动作色**：`semantic_colors.hpp` 的 `PushContinueButtonStyle()`。它**不是** good/warning/destructive 任何一档
（不对内容做判断），也**不是** accent（accent 表示交互状态；accent 染色的 Colors 按钮读作「已配置/被选中」）——它是
**动作身份色**：让「在当前画面上继续加光线」不会被读成同组的 Run（绿，从零开始）或 Stop（红）。候选均实拍截图比较
（启用/禁用两态），按与同栏四种按钮色（Run 绿 120°、Stop 红 0°、默认按钮蓝 218°、accent 212°）的距离裁定：

| 候选 | Normal RGB | 色相 | 与最近邻的色相差 | 与最近邻的 RGB 距离 | 裁定 |
|---|---|---|---|---|---|
| **A 紫罗兰** | (0.38, 0.24, 0.58) | 265° | 47°（默认蓝） | 0.41（默认蓝） | ✅ 采用 |
| B 青 | (0.10, 0.42, 0.46) | 187° | 25°（accent） | 0.31（默认蓝） | ❌ 与蓝色按钮族同族，读作「又一个普通按钮」 |
| C 梅红 | (0.52, 0.20, 0.42) | 319° | 41°（Stop 红） | **0.27**（Stop 红） | ❌ 低于 0.3 下限，且与 Stop 同栏同槽相邻 |

禁用态不另设颜色：`BeginDisabled` 的 `DisabledAlpha` 对它与其它按钮一视同仁地压暗（实拍像素 (97,61,148) → (65,43,97)）。
「与同栏四色的 RGB 距离 ≥ 0.3」由 `gui_unit_test` 的 `ContinueButtonColour.IsFarFromEveryButtonColourItSharesTheBarWith`
机械钉住（0.3 复用 `theme.cpp` 对照色盘的既有下限，非新阈值）。它与 Run 的绿一样是写死的具名常量、不随色盘走——
值是**相对**那几个同样写死的语义色选出来的，只让它随色盘漂移反而会破坏这组相对关系；换肤时应与 Run/Stop 一并重新决定。

## 5. 被证伪的方向（勿重提）

- **均匀加大间距 / 圆角** —— 直觉上"更透气"，实测读作**空旷**，反而降低精致感。这是 §2 那条结论的反面证据。
- **把所有标签移进统一的右侧标签列** —— 竖线确实接上了，但复选框与它自己的标题被拉开约 230px，**邻近性被牺牲**，已看不出哪个框对应哪个标签。对齐与邻近都是排版原则，该做法用一个换了另一个。若要两全，正解是**前置标签**（标签在左、控件在右）：竖线是标签的左边缘（连续），每个控件紧跟自己的标签（邻近）。代价是 combo / checkbox / radio 每一类都要包一层，属调用点级工作量。
- **去色中性化** —— 见 §3，基于对现状色彩的误判。

## 6. 与面板重排的顺序约束

（原标题「与 docking 迁移的顺序约束」。那次迁移已实施并被回退，见 `doc/gui-layout-architecture.md` §8；
本节约束对**将来任何一次重排**依然成立，且已由那一轮实施反向印证：视觉语言确实可以正交先行并单独留下。）

两件事处在**同一层**（面板 shell 的排布），因此顺序不是自由的：

- **纯中央的视觉语言**（字体、调色板、尺寸节奏）与 docking **正交**，可独立先行。
- **控件形态与信息形态**（前置标签、表格化、控件合并、分组重排）与 docking 迁移**撞层**——迁移本身就要重写面板 shell。分两次做等于同一批代码改两遍。
- §2 第 6 条（大片空区）**只能**由 docking + 新功能解决，样式层无解。

另有一条来自表格化的具体证据：表格形态所需的宽度**超过 300px 固定面板所能给的**（原型中名称列被裁，只能靠折叠行腾挪化解）。这为"面板可调宽度"提供了一条具体理由，而非"更自由"这类模糊好处。

## 7. 已知遗留

- ~~**正文字体未定案**~~ —— **已定案并落地**：Roboto Medium 15px。分发按当时判断比照 FontAwesome，走构建期嵌入（`scripts/embed_binary.py` 生成 `roboto_medium_embed.cpp`，声明头 `src/gui/roboto_medium_embed.h`），运行期不读文件；字体与 style 的初始化收敛到单一 owner `src/gui/theme.{hpp,cpp}`（此前 `src/gui/main.cpp` 与 `test/gui/test_gui_main.cpp` 各写一遍，截图与真 app 只是碰巧一致——见 §1）。
- ~~**颜色存在第二个 owner**~~ —— **已收口**：good / warning / destructive 三档语义色现由 `src/gui/semantic_colors.hpp` 单一定义，21 处调用点（绿色 Run 按钮、锈色 Resolution 输入框、Stop 按钮、日志级别色、filter 编辑器的三态校验底色等）改为引用具名函数。每档提供两种消费形态——`*TextColor()` 亮色用于文本/图标/边框，`*FillColor(alpha)` 哑光用于 FrameBg/CellBg 背景染色，alpha 由调用点给（各调用点的强调程度本就有意不同）。按钮三态只在存在消费者的档位提供：good 在同一头文件，destructive 仍是 `src/gui/destructive_style.hpp` 的 `PushDestructiveStyle`/`PopDestructiveStyle`（12 处配对调用的既有实现，一档一形态只留一个 owner），warning 无按钮消费者故不预先实现。
  - 每个 canonical 取值都锚定收编前**已存在**的字面量（复用次数最多的那个），不是新拍的折中色——于是 5 处近重复琥珀、3 处近重复红折叠后只有几个百分点的通道位移，而参考图覆盖到的场景**逐像素不变**。
  - 三类颜色**不**在此收编，判据是"颜色是否表达产品对内容的判断"：**强调色**（hover/active/选中高亮）表达交互状态，与语义色取值必须分开，否则"正在被操作"会读成"有风险"；**数据色**（用户在 Overlay 面板配的线色、由组号派生的 sync-group 色板）由用户或索引决定；**画布内容色**（画在预览图 / 3D 渲染之上的标注、以及渲染自身的底色）不携带判断。
  - 状态栏的 `Simulating...` / `Stopping...` / `Done` 也不并入：它们是进度/信息指示。把"任何非稳态"塞进 warning 会把该档从"内容需要你处理"稀释掉，反而淹没 Resolution 这种真正会触发重跑的提示。它们的归宿是下面那条"中性刻度"。
- **调用点颜色是否跟着色盘走：判据与豁免清单** —— 上一条解决的是"同一语义的颜色有几个 owner"；这一条解决的是另一个正交问题：一个调用点自己挑的颜色，**换一套色盘时跟不跟着变**。跟不着变的即为泄漏，必须改为读色槽（`ImGui::GetStyleColorVec4` / `ImGui::GetColorU32`）或读 `lumice::gui::AccentColor()`。

  判据本身可机械检验，不必靠人工点检：`src/gui/theme_test_hooks.hpp` 暴露一个 test-only 的对照色盘（`ApplyContrastPaletteForTest`，取值与生产色盘每个字段的欧氏距离 ≥ 0.54），`gui_test --theme-palette contrast` 用它重跑 `theme_scan` 类别的场景，`scripts/scan_theme_leaks.py` 逐像素比较两套色盘下的同一批截图并高亮**两次色值相同**的像素。它产出的是**候选**不是判决——豁免类颜色本来就应当不变——但"哪些像素没跟着色盘动"这个问题从此有机械答案。

  **豁免只有三类，其余一律视为泄漏**（面板铬件——缩略图边框、日志正文档位、按钮/复选框染色——一律不豁免）：

  | 可豁免 | 判据 | 实例 |
  |---|---|---|
  | **数据色** | 取值由**用户或索引**决定，不由主题决定 | `panels.cpp` 的 `kSyncGroupPalette` 六色板；Overlay 面板里用户配的辅助线色 |
  | **画布内容色** | 画在**预览图 / 3D 渲染**之上或之下，对比对象是**图像**而不是面板 | `overlay_labels.cpp` 的标签黑底、`face_number_overlay.cpp` 的面号描边；以及 `crystal_renderer.cpp` 渲染晶体缩略图/预览时自己的 `glClearColor` 底色 |
  | **内容语义色** | 携带"这个值健康 / 需要注意 / 危险"的产品判断，与皮肤无关（正如 OS 深浅色切换通常不会改变错误红的色相） | `semantic_colors.hpp` 的 good / warning / destructive 三档，含绿色 Run 按钮与 `destructive_style.hpp` 的按钮三态 |

  ⚠️ **"结构色"这个旧类别已废止**。它曾被用来豁免占位边框、标签底衬一类颜色，在本判据下站不住：缩略图的占位块与边框正是靠它免检，而它们换肤时会原样留在原地。今天它们读 `ImGuiCol_FrameBg` / `ImGuiCol_Border`。仍然合法的只是它当年混进去的另外两样：**全透明**的 hover 触发区（`ImVec4(0,0,0,0)`，alpha=0 画不出像素，谈不上跟不跟色盘）与**由填充色派生**的对比文字（`panels.cpp` 里按 sync-group 填充色的 luma 在黑/白之间取值——它的输入是数据色，派生规则本身不是颜色选择）。

  **不带判断的进度/状态文字取"中性刻度"**：主题给的三档是 `ImGuiCol_Text`（静息）/ `ImGuiCol_TextDisabled`（暗淡）/ accent（此刻正在发生）。状态栏据此为 `Simulating...` = accent、`Stopping...` = `TextDisabled`（同一帧里 Stop 按钮本就是灰掉且禁用的）、`Done` = `Text`；日志面板的 trace/debug = `TextDisabled`、默认档 = `Text`。带判断的两态仍走语义色（`Ready` = good，`Modified` = warning）。

  **顶栏那两处自造蓝一并归 accent**，但它们的语义**并不同类**，这一点必须记下来，否则这次合并会变成一个没人记得的隐性决定：

  - Composite 勾选框的勾选态（`app_panels.cpp`，条件是 `composite_now`）是**同一个控件的不同状态**。主题本来就把 accent 花在静止的勾选态上（`theme.cpp` 的 `c[ImGuiCol_CheckMark] = p.accent`），所以这处属既有做法。
  - 顶栏 Colors 按钮的三态（条件是 `ShouldTintColorsButton`，即文档里配了色类）是**内容状态投影到控件上的徽章**——按钮在两种情况下行为完全相同，染色只是在窗口打开**之前**就告诉你染色配没配。

  两者仍都进 accent，理由是**举证责任**而不是语义相同：为徽章单开一档 `Palette` 角色，今天只有**一个**调用点撑着。⭐ **触发条件登记：出现第二个"内容非空 → 给控件染色"的徽章式用法时**，才是把它从 accent 里拆出来、单立一档 `Palette` 角色的时机。

  ⭐ **另一条边界，同样先登记不动手**：`crystal_renderer.cpp` 的画布底色被豁免为画布内容色，但它是**整块可见面积**。若将来真的出第二套皮肤（尤其是浅色），这块底色与画在它上面的线框色**必须作为同一个画布问题一起重新决定**，不能只改其中一个——单独改任何一侧都会破坏它们之间的对比关系。
- **一值两控件仍是主流行式** —— 表格单元格内已验证单控件（`DragFloat`）可行，但推广属控件形态层，见 §6。
- **左面板空区** —— 见 §2 第 6 条。
- **标签放置有两种形态、各有规则，不再追求统一为一种**——表格形态（多行同构、每行一个对象，如 Overlays 表）用**前置标签**
  （名称列在左、控件在右、固定宽度右对齐列）；单个控件（slider / combo / checkbox / radio）用 ImGui **尾随标签**，
  其中读 `CalcItemWidth()` 的一族（slider / combo）由 `LabelColumnGapX()` + `kLabelColWidth` 对齐在同一竖线，checkbox / radio
  贴控件、不参与该对齐族。新增控件按此二选一，**不得长出第三种**。「全部统一为前置」已裁定不单独做：用户对位置比对外观敏感
  （§5 / `gui-layout-architecture.md` §8 的内测证据），单独做付两次习惯成本；留待下一次形态重排时一并做（backlog §1 #31）。

## 8. 验证方法

视觉改动的验证回路已存在，成本很低：

- `gui_test` 的 `capture_harness/fullframe` 用例抓取 `ResetTestState()` 之后的整帧默认界面，且 `SavePng` 在 PSNR 比对**之前**执行 —— 主题一改必然比对失败，但**图照样产出**。配 `--export-dir` + `--keep-export-png` 即可批量取图。
- **必须保留一个控制项**：未改动的基线变体应当**通过**现有 PSNR 门禁。它一旦不通过，说明改动动了不该动的像素，此时所有对比失效。原型全过程中该控制项持续为绿。
- ⚠️ **测试绿灯对未走到的路径零覆盖**。原型中曾出现一次 `Missing EndGroup()`（右面板整个渲染成 ImGui 断言框），而当时 287/287 gui_test 全绿——因为测试跑的是基线主题，新路径根本没有被执行。**抓到它的是截图，不是测试。**
- **参考图重拍是有税的**：任何主题改动都会使 on-screen 的视觉回归参考图（`modal_layout`、`defaults_panel_layout`、`capture_harness`）失效并需重拍；`lens_proj` 走离屏 FBO、不含 chrome，不受影响。⇒ **视觉方案应一次定稿再落地，不要连续多次微调**，否则每次都要付一遍重拍成本。

## 9. 窗口尺寸策略：四档表

GUI 里每个窗口「能不能被用户拉、拉哪个轴、出现时多大、下次打开还记不记得」曾是每个窗口一套：
Settings 每次打开都重置到默认尺寸（`ImGuiCond_Appearing`），Summary 完全不可拉
（`AlwaysAutoResize | NoResize`），Edit Entry 又是第三种。这一节把它收成四档，每个窗口归到一档；
新窗口先在这张表里选档，再写代码。

### 9.1 四档与判据

| 档位 | 含义 | 判据 |
|---|---|---|
| **固定** | 位置、尺寸都不可动 | 主 shell 的一部分。改它就是重排，属 `gui-layout-architecture.md` §8 的禁区 |
| **自动贴合** | 随内容变；用户不可拉 | 几个固定控件，或行数天然很少（确认对话框、Custom Spectrum 这一类） |
| **半可变** | 一轴固定、另一轴用户可拉；出现时先贴合内容；会话内记住用户拉过的尺寸 | 内容是沿**一个轴**增长的行列表 |
| **自由** | 两轴都可拉 | 内容是二维的（列表 + 预览、多列并排的工作窗口） |

「会话内记住」的含义与边界：同一次进程里关掉再打开，尺寸是用户上次拉到的值；进程重启后回到默认。
跨重启记忆属 app 偏好命名空间（`gui-state-governance.md` §8 的命名空间 ③），不在这张表里，
也不由窗口自己实现。

### 9.2 逐窗口归档

| 窗口 | 落点 | 档位 | 备注 |
|---|---|---|---|
| TopBar / StatusBar / Left / Right / Preview | `app_panels.cpp` | 固定 | 每帧由 app 重钉；§8 禁区 |
| Log 面板 | `app_panels.cpp` | 固定（250px） | owner：本来与主窗口同宽，暂不动 |
| Edit Entry | `edit_modals.cpp` | 半可变（宽固定、高可拉） | Compact / Expanded 两档，宽按各自形态钉死（min == max）；高度与 Summary 共用 `secondary_window_sizing.hpp` 的「跟随内容直到用户拖动」状态机。与 Settings/Summary 不同的一点：窗口里**有一个可伸缩 pane**（Compact 的 tab 区、Expanded 的两列）吸收高度差，其余部分（预览、共享行、底栏）不变 ⇒ 任意高度下操作行都在窗口内、窗口自身不滚；工作区矮于内容时（如 1366×768）pane 自动让出溢出量，不拖也不滚。高度上限取窗口**所在显示器**的工作区（不用只认主 viewport 的 `ClampedSecondaryWindowMaxHeight`）；UI scale 变化那一帧回到跟随内容 |
| Custom Spectrum / 5 个确认对话框 | `edit_modals.cpp` / `app_panels.cpp` | 自动贴合 | `AlwaysAutoResize`，不动 |
| Settings | `defaults_panel.cpp` | 半可变 | 宽钉 760；默认 760×584 不变，故 `defaults_panel_layout` 参考图不重拍 |
| Summary | `config_summary_window.cpp` | 半可变 | 宽钉 1200；出现时贴合内容、上限 min(900, 工作区)，之后高可拉到工作区 |
| Raypath Analysis / Colors | `analysis_panel.cpp` / `color_window.cpp` | 自由 | `ImGuiCond_FirstUseEver` 给默认尺寸，之后 ImGui 自己记 |

### 9.3 与 `gui-layout-architecture.md` §8 的边界：可拉尺寸不是重排

一个窗口**在自己的矩形内**可以被拖多大，是这个窗口自己的几何策略；§8 否决的是把窗口/面板挪到
shell 的哪个区域、要不要接入 docking。两件事互不影响：将来无论重排走哪个方向，这张四档表都不需要
跟着改，反过来这张表也不构成重启重排的理由。

### 9.4 实现配方（半可变）

ImGui 对「半可变」的表达只有一句：`SetNextWindowSizeConstraints(ImVec2(W, h_min), ImVec2(W, h_max))`——
固定轴 min = max，浮动轴给 [下限, 上限]。⛔ 不要发明自定义 resize 手柄。上限的裁剪算式
（工作区高度减一圈边距，再与窗口自己的硬上限取小）只有一个实现，`src/gui/secondary_window_sizing.hpp`
的 `ClampedSecondaryWindowMaxHeight()`，两个半可变窗口都调它。

默认尺寸怎么给，取决于内容随不随文档变：

- **内容不随文档变**（Settings：表格在内部滚动，窗口外框是常量）：`SetNextWindowSize(默认, ImGuiCond_FirstUseEver)`。
  这个 cond 只在窗口生命周期里第一次出现时生效一次，之后 ImGui 自己按窗口记住 `Size`
  （`io.IniFilename == nullptr` 时不落盘，正好是「会话内」）。⚠️ 不是 `ImGuiCond_Appearing`——
  那个每次重开都生效，用户拉过的尺寸就丢了，正是修掉的那个 bug。
- **内容随文档变**（Summary：行数是文档的函数）：`AlwaysAutoResize` 用不了——ImGui 里这个 flag 蕴含
  `NoResize`（resize 边框的命中测试在 `NoResize` **或** `AlwaysAutoResize` 任一为真时跳过），设了它用户
  就永远拉不动。改为每帧手动做同一件事：窗口尚未被用户接管时，把上一帧量到的内容高度
  （`ImGuiWindow::DC.IdealMaxPos − DC.CursorStartPos`，与 ImGui 自己的 `ContentSizeIdeal` 同一算式）
  加上 padding 与标题栏，经 `SetNextWindowSize(…, ImGuiCond_Always)` 请求为本帧高度；用户一拖，下一帧
  读回的实际 `Size.y` 与上次请求值对不上，据此判定「已接管」、停止再请求。这是配方的特化——
  「贴合内容直到用户第一次插手」——不是另一套规则。

两种窗口都要给 `gui_test` 一条复位口（`ResetXxxTestState()`，挂进 `ResetTestState()`）：
ImGui 记住的尺寸跨用例存活，不复位就是「隔离单跑绿、全量跑红」家族的又一个字段变体（Pos / Scroll /
InputText 缓冲之外的第四个：Size）。
