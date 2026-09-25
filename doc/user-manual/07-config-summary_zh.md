[English version](07-config-summary.md)

# 配置总结

Summary 窗口把当前配置摊在一页上——晶体及其形状、朝向，太阳，光谱，镜头与视角，曝光——让别人看一眼截图就明白，不必打开程序或文件。它只展示配置：渲染结果图另走 Save → `Screenshot...` 导出；这一页也不是用来重新导入的——要给别人复现，分享 `.lmc` 或导出的 config JSON。

## 1. 在哪里

点顶栏的 **Summary** 按钮（文档图标），在 **Colors** 与 **Analysis** 旁边。它会在预览旁打开一个独立的非模态「Summary」窗口；再点一次按钮、或点窗口的 ×，即可关闭。换文档——New、Open、Import 或 Revert——也会把它关掉，因为这一页描述的是某一份文档。

窗口宽度固定为 1200 px，分左右两栏——左边是各设置组，右边是各层及其条目——因此一份常见的文档在 1280 × 900 的屏幕上一屏看全、无需滚动；高度随内容变化，到这个预算为止，只有特别长的文档才会滚动。它是只读的：页面上没有任何可编辑的东西。

## 2. 页上有什么

自上而下：

- **版本号**——画出这张图的 Lumice 版本。两张总结图有出入时，先看两边各是哪个版本画的，比较起来最省事。
- **Sun**——Altitude、Diameter、Spectrum（或自定义光谱的表）。
- **Simulation**——Rays(M)（以百万计，与滑条一致）、Max hits、Infinite rays。
- **Render**——Lens Type、FOV、相机的 Elevation / Azimuth / Roll、Visible 与 Front、Resolution、EV 与 EV Anchor、Mode（screen / print）、Show As（normal / channel_br，仅 Mode 为 Screen 时），以及底色——Screen 下是 Sky Color，Print 下是 Paper Color。
- **Settings**——主面板上没有控件、只能在 Settings 面板里改的少数设置（目前：ray allocation）。单独列出，是为了让你对着程序读这一页时知道该去哪里找它们。
- **Layer N**——一行标题写该层的多次散射概率与条目数，随后是两张表，每个条目一行，两张表都以条目在层内的序号（`#`）开头，所以一张表里的某一行能对上另一张表里的同一行：
  - **Crystals**——`#`、**Enabled** 与 **Weight**（卡片上的开关与比例）；**Crystal**：池编号、你起的名字、类型，拼法与晶体卡片和 Colors 窗口完全一致；**Zenith / Azimuth / Roll**：三个朝向分布完整印出，Zenith 前面带三个分布归类出的预设名（`Plate · G 0(1)`：Column / Plate / Parry / Lowitz / Random / Custom），所以调过 std 的预设（std 5 而不是 1 的 Column）也看得出来；**Filter**：按卡片的拼法（`3-5-1 In PBD`，或 `None`），后面跟 filter 的名字——多行 filter 与卡片一样只印首行加 `(+N more)`，要读全部行请开 Filter Editor。
  - **Shape**——`#`，然后是编辑器 Crystal 页的各列：棱柱为 Height，锥体为 Prism H / Upper H / Lower H / Upper A / Lower A，再是 Face 3–8。列是两种类型的并集，某一行用不到的列就留空。

  两张表下方有一行图例，写明分布字母的含义。

**分布记号。**凡是从分布里抽取的值——三个朝向角，以及你随机化了的任何形状参数——单元格写作 `字母 中心(展宽)`：字母表示分布类型（`G` Gauss、`U` Uniform、`Z` Zigzag、`L` Laplacian、`G*` Gauss (legacy)），括号前后的两个数就是面板控件上的那两个数——朝向的 Mean 与 Std / Range / Amplitude / Scale 输入框，形状参数的中心与展宽——不做任何换算：`U 0.900(0.100)` 是 Range 框写着 0.100 的均匀分布，即区间 [0.85, 0.95]，不是 ±0.100。两个简写：没有随机化的形状参数只印数字（`1.000`）；全周均匀的朝向——均值 0、范围 360，也就是 Random 预设的 azimuth 与 roll——折叠成单个 `U`。同步的形状参数在值后面标 `· sync N`。

标签用的是各面板自己的字——`Rays(M)`、`EV Anchor`、`Sky Color`——拿着截图对照程序时，每一行都能在面板上按同一个名字找到；唯一的例外是 `Visible`，View 面板上它是 *Visibility* 标题下三个没有单独标签的单选按钮。这一页也只印面板此刻显示或生效的字段：全天镜头下没有 FOV / Elevation / Azimuth / Roll / Visible / Front 这几行，正如面板上这几个滑条被置灰；开了 Infinite rays 就没有 Rays(M) 行；Render 组里只有当前 Mode 用到的那一种底色——Sky Color 或 Paper Color——不会两者都印。

页上的值是各面板呈现给你的值，不是内部交给仿真器的值：选了 `linear` 镜头就印 `linear`，尽管预览内部总是先仿真一张全天纹理再在屏幕上重投影。描述「你怎么看这个程序」而非「这个场景是什么」的设置——叠加线及其颜色、背景照片及其路径、面板布局、日志级别、哪些窗口开着——不在页上，所以截图里不会带出任何关于你这台机器的信息。

值的拼法沿用文档本身的：`fisheye_equal_area`、`relative`、`full`——与 `.lmc` 文件和 Settings 面板只读列所用的字一样。

## 3. 截图

没有图片导出按钮；用操作系统自带的截图工具框选这个窗口——macOS 上 ⌘⇧4 再按空格点窗口，Windows 上 Win+Shift+S，Linux 用桌面环境的等价快捷键。几层的文档在窗口里绰绰有余；文档长到页面需要滚动时，截两张或先精简文档——这一页会把所有层与条目都印出来，不论有多少。

要的是文字而不是图片时，版本行右端的 **Copy as text** 把这一页放进剪贴板：先是版本行，然后每个分组是标题加每字段一行 `标签<TAB>值`，然后每一层是标题加它的两张表（单元格以制表符分隔、表头行在前），最后是分布记法的图例。它与窗口画出来的是同一份页面模型，屏幕上的每一行就是剪贴板里的一行。

## 4. 另见

- 导出的 config JSON——用 CLI 能渲染的形式描述同一张图 → 顶栏 **Save** → `Config JSON...`（[gui-guide_zh.md](../gui-guide_zh.md)）
- 把当前设置存为个人默认值 → **Settings** 面板，它列出同一批设置行及其出厂值（[gui-guide_zh.md](../gui-guide_zh.md)）
