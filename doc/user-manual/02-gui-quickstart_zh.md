[English version](02-gui-quickstart.md)

# GUI 快速上手

本章带你在 Lumice GUI 中完成首次交互式模拟：启动应用、加载内置示例、跑一次模拟、读懂预览。看完你会知道点哪些面板、各浮窗是什么含义。

> **前置**：构建产物含 GUI（`./scripts/build.sh -j release` 会同时产出 `build/cmake_install/static/LumiceGUI`）。

## 1. 启动

```bash
./build/cmake_install/static/LumiceGUI
```

首次启动后主窗口为空载状态：

![LumiceGUI 默认空白状态](../figs/gui_screenshot_default.jpg)

## 2. 主窗口结构

下图为主界面 6 区域的标注图，编号在本手册中保持一致。

![GUI 布局标注图（中文）](../figs/gui_screenshot_example_07.jpg)

| 编号 | 区域 | 用途 |
|------|------|------|
| 1 | 顶部栏（Top Bar） | 打开/保存项目、Run/Stop/Continue 模拟、状态徽标 |
| 2 | 左侧面板（Left Panel） | 晶体列表、光源、散射、渲染配置卡片 |
| 3 | 晶体预览（Crystal Preview） | 当前晶体的 3D 预览（线框 / 隐藏线 / 透视 / 着色）|
| 4 | 渲染预览（Render Preview） | 边跑边累积的光晕图像 |
| 5 | 浮动镜头条（Floating Lens Bar） | 在多种镜头投影间快速切换（线性、等距鱼眼、等积鱼眼……）|
| 6 | 状态栏（Status Bar） | 当前光线数、耗时、日志严重度计数 |

> 区域名按"功能"取，不按视觉位置 — UI 颜色 / 像素位置可能在版本间漂移，但这 6 个功能稳定。

## 3. 加载内置示例

`File ▶ Open` 选择 `examples/config_example.json`。左侧面板会被填充示例的 4 个晶体、光源、散射层和 1 个渲染条目：

![加载示例](../figs/gui_screenshot_example_01.jpg)

晶体预览展示当前选中的晶体；点击列表里其他晶体即可切换预览对象。

## 4. 跑模拟、读预览

点击顶部栏的 **Run**。渲染预览开始实时累积光线：

![带网格 overlay 的渲染预览](../figs/gui_screenshot_example_02.jpg)

模拟运行中：

- 状态栏显示当前光线数与已耗时；
- 浮动镜头条可以**不停机**切换镜头投影 — 同一份光线数据被实时重投影；
- 上图的网格 overlay 帮你在预览里读角度，而且随文档一起走：它的开关、颜色、角度列表会存进 `.lmc`，导出的 JSON 里以 `render[].grid` 携带，CLI 同样会把它画到输出图上。
- 角距离圆（**Overlay ▶ Angular Distance from... ▶ Sun / Lens Center**，每行末尾的 **⋯** 按钮打开角度编辑器）可以用指点代替敲数字：按编辑器里 **+** 旁边的十字准星按钮，再把鼠标移到预览上。一个以该族中心为圆心的圆会跟着光标走，旁边写着它的半径；**左键**把这个半径加进列表（不取整，列表按 0.1° 显示），**Esc** 或**右键**取消、不改列表。光标落在不是天空的地方——镜头圆外、被 **Visibility**（Upper / Lower）或 **Front** 设置裁掉的部分——不显示圆，点击也不生效，重新瞄准即可。拾取期间拖动和滚轮都不会转动视角。按下按钮会顺带打开该行的 **Line** 开关（取消后保持打开）。已经画出的圆目前还不能直接拖动；要改就在编辑器里删掉再重新拾取。

提前停止？点顶部栏的 **Stop**，部分结果会保留在屏幕上。

跑完了（或者中途停了）还嫌噪声大？点 **Run** 旁边的 **Continue**。它会再追踪 Rays(M) 栏里那么多的光线
（勾选了无限光线时则一直跑到你点 Stop），并叠加到屏幕上已有的画面里——已经追踪的光线一条都不浪费：光线计数
从原来的数接着往上涨，亮度也不会跳变。**Run** 则总是从零开始重跑。两次之间可以改 Rays(M)——它决定下一次
Continue 追加多少——曝光、叠加线、视角也都可以随便调。其它改动（晶体、太阳、滤镜……）意味着画面已经不对应
当前配置，Continue 会变灰，直到你重新 Run 或 Revert 这些改动；鼠标悬停在按钮上可以看到变灰的原因。

### 这里偏蓝还是偏红？——Channel B−R 显示

预览区右上角的 **Normal | Channel B-R** 开关（平时半透明，鼠标悬停时变实；Print 模式下变灰——那里没有独立的红、蓝通道可减）把预览变成一张颜色诊断图：每个像素显示正常画面的蓝通道减红通道（显示用的 sRGB 值，gamma 之后——与在图像编辑软件的通道计算里做减法相同），以灰度表示。中灰表示没有差别；比中灰亮偏蓝，比中灰暗偏红。纯蓝读作白、纯红读作黑，任何中性像素——包括空天空和镜头圆之外——都读作中灰。切回 **Normal** 即恢复正常画面；两个方向都不会重跑模拟。

读数前要知道三件事：

- **值随 EV 变化。** B、R 是曝光后的显示通道，调高曝光会让同一个晕离中灰更远。
- **触顶后差值被压平。** 红或蓝到达满亮度后就不再增长，所以画面最亮处的差值会被压缩。
- **所以比较不同位置时要在同一 EV 下看**——不同曝光下的两个读数不可比。

叠加层（网格、圆、标记、标签、镜头边框）以各自原色画在诊断图之上。诊断图开启时，背景照片与光路染色的 Colored 合成图会被隐藏（它们的设置保留不变）；在 **Mode ▶ Print** 下该选项变灰：print 没有独立的红、蓝通道可减。截图导出的就是屏幕上的画面，导出的 config 带 `"display_mode": "channel_br"`，CLI 渲染出同样的灰度图。

## 5. 从零搭一个新 entry

不想直接打开示例，想自己搭一个光晕 recipe？最短路径：

1. **File ▶ New** 新建空项目。
2. 左侧面板 **Crystal** 卡片点 **Edit**（或 **+ Add**）打开晶体编辑器：

   ![晶体编辑对话框](../figs/gui_edit_crystal.jpg)

   选个预设（例如 **Hexagonal Prism**），如有需要可调 `height`，确认。（其它形状字段如 `face_distance` schema 中接受但当前引擎尚未实现 — 详见 [`05-faq_zh.md`](05-faq_zh.md) §4。）
3. 打开 **Light Source**，设 `azimuth`（太阳方位角）和 `altitude`（太阳高度角）。
4. 打开 **Render**，挑镜头、分辨率、视场角 — 浮动镜头条之后还能切换，所以默认值就行。
5. 点 **Run**。

> 📷 待补：Crystal Tab 整体截图（已登记到 `progress.md` 占位锚清单；closeout 阶段会汇总进 SUMMARY.md "待补充清单"）。

## 6. 保存与重载

`File ▶ Save As` 写出 `.lmc` 文件（其实是 JSON，CLI 也能直接吃）。在 GUI 里重新打开它会还原**晶体 / 光源 / 渲染**数据、镜头投影与 overlay 设置，但**不**还原纯观察态（例如晶体预览样式）。差异完整列表见 [`05-faq_zh.md`](05-faq_zh.md) "GUI 与 JSON 能力差异"。

## 延伸阅读

- 同一份 `.lmc` 用 CLI 跑一遍 → [`03-cli-quickstart_zh.md`](03-cli-quickstart_zh.md)
- 用现成 recipe 复现经典光晕 → [`04-recipes_zh.md`](04-recipes_zh.md)
- 查一道晕是哪些光路做出来的，并排除某一条 → [`06-raypath-analysis_zh.md`](06-raypath-analysis_zh.md)
- 把自己的配置截成一张图分享给别人 → [`07-config-summary_zh.md`](07-config-summary_zh.md)
- 完整 GUI 面板参考 → [`../gui-guide_zh.md`](../gui-guide_zh.md)
- 全部字段名与类型 → [`../configuration_zh.md`](../configuration_zh.md)
