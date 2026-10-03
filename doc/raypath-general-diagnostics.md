# 通用 Raypath 诊断契约

Raypath 诊断以当前场景的实际映射和测度为对象，不以路径、晶体或姿态的名称决定是否分析。固定科学案例可作为条件 oracle 和回归，但不是生产支持集合。

## 输入与测度

对实际晶体实例 `g`、具体物理成员 `p`、波长 `λ`、太阳盘方向 `s` 和姿态 `R∈SO(3)`，一个光滑路径分支定义方向映射 `F_{g,p,λ,s}`。其有效域由所有折射和入射 gate 的严格可行性决定。有限晶体入口面积、逐接口 Fresnel 权重、形状 ensemble、姿态分布、太阳盘、光谱和成员选择共同组成显式场景测度；亮度是该加权测度经 `F` 的推送。一面成员表示首面外反射；两面及以上成员表示入射透射、内部反射和出射透射组成的链。两者是同一场景测度中的物理分支，不由固定面号、形状或姿态白名单区分。

内部反射的 TIR 判别式使 Fresnel 权重非光滑，却不使部分反射离开有效域。它因此是独立的光学候选，不能被当作路径不可行或域边界。

多层路径须由逐层条件转移和权重组成：后一层的入射方向来自前一层输出，每层使用该层真实分布。单层方向映射不能替代整链模型。

### 场景测度的可计算分解

场景全局变量为 `g=(s0, λ, sun_node_id, spectrum_node_id)`。太阳盘方向 `s0` 和波长 `λ` 在整条复合路径只生成一次；其权重 `w_sun*w_spectrum` 也只计一次。固定 `g` 后，传播方向满足

`d0=s0, d_l=F_l(d_{l-1}, λ, shape_l, pose_l, member_l)`。

第 `l` 层的条件核只承载该层的晶体份额、shape/pose 分布、具体成员、有限入口和逐接口光学权重。前一层的 `outgoing_direction` 必须逐值成为后一层的 `incident_direction`；源太阳方向只作为 provenance 保留，不能重新充当后续入射。离散谱、太阳盘、层内连续分布和原子都保留各自的质量，失败分支不重新归一成成功分支。

姿态的三个产品分布按实际球面积分语义抽样，支持切维数由随机变量本身决定，不由 `random`、`plate` 等名称或 Gaussian 数量推断。shape 的适用 scalar 共享产品的 `sync_group` 潜变量：同组复用同一个原始 draw，height 再做绝对值 fold，face distance 保持有符号；固定 scalar 是同一生成流程的零维退化输入。每层之间的 shape 与 pose 独立，除非产品配置未来显式声明跨层关联。

成员选择有四种明确形式：具体 L2 face sequence、按本层实际 shape/axis 的 physical P/B/D gate 展开后的全集、该全集上的逐层显式 mask，以及逐条列出的多层 member chain。具体和显式 chain 均按 face sequence 本身标识并逐值使用，不经过 P/B/D 扩展；P/B/D gate 只定义等价成员展开，不是任意合法 face sequence 的可达性白名单。显式 chain 因而能选择同入口面但内部序列不同的成员，也能表达非笛卡尔积的多层组合。label/L1 等价不参与这一选择。层内晶体质量是该层所有正 `crystal_proportion` 的总和作分母、同一 `crystal_id` 的全部 entry 各自贡献质量的混合，不能用首个匹配 entry 代表整组；每个 linked entry 仍独立执行产品 `FilterSpec` 的 raypath、entry/exit、direction、crystal、compound、action 和 symmetry 语义。filter 拒绝把对应 entry 的接受质量置零但不重新归一，其后整层无剩余质量时终止该 chain。跨层链还乘以前置层的 continuation probability 和末层的 exit probability。

每层成员序列必须含 `1..kMaxHits` 个 face，空序列非法。一面成员使用实际 face polygon 的投影面积和外介质到晶体的 Fresnel 反射；两面及以上成员保留透射链语义。晶体种类不允许的 face，以及在该 shape 分布全部支持上都不可能出现的 face，在积分前作为非法输入拒绝；只在部分随机 shape 上消失的 face 仍是合法 ensemble 输入，并在对应实际 draw 上记为 `physically_unreachable`。固定 filter 的严格零证书只能在该行每层实际 sampled shape 都已通过 `BuildFaceNormals` 与通用诊断路径解析后使用，因此非法或本次 draw 缺面的成员不能借 filter 早退伪装成 `zero_weight`。

场景 spectrum 模式直接使用离散节点及其原始权重；illuminant 模式对产品的 `[380,780)` 均匀波长测度做分层节点积分，并在每个节点乘同一个 `GetIlluminantSpd` 权威函数。显式 diagnostic spectrum 是独立模式，不改写场景观测谱。零权重节点保留为 `zero_weight` 行。太阳直径为零时是中心方向质量为 1 的原子；非零时是产品球冠上的单位概率测度，各节点质量之和仍为 1。太阳直径改变方向分布而不隐式改变源总能量；另行需要 radiance/solid-angle 模式时必须由独立输入明确给出，不能借用默认场景语义。

每个唯一生成 latent 分别记录 base measure（原子计数、单位区间、Lebesgue 或 Bernoulli 计数）、重放坐标、proposal/target 密度或质量、映射 Jacobian 和数值状态。同步 shape scalar 共享首见 leader 的分布与 latent；height 的绝对值 fold、latitude LUT 的 inverse-CDF 与 flip 分支、GaussianLegacy 极点 fold、azimuth/roll 耦合和 degree→radian 映射都在同一记录中。姿态局部结构是 field 使用的 `(longitude, latitude, roll)` 到行主序旋转矩阵的解析 `3×9` 微分，支持 rank 由实际生成坐标的 SO(3) 切映射求得；极点处 longitude 与 roll 的同向生成元按精确图结构合并，若正展宽 latitude 的 float 样本恰落在奇异图上则 rank 为 `-1`（局部不可用），不能误报成严格低维支持。

每条测度行分别保存全局权重、`joint_sample_mass=1/N`、与之分离的连续/离散 joint proposal/target、每层条件质量、raw analytic `A_path`、当前 sampled shape 的总表面积 `S_total`、产品入口因子 `2*A_path/S_total`、逐接口 Fresnel 结果、实际 shape/pose/member 和来源 id。proposal/target 的联合积同时保存 log-density；importance 按逐因子 log-ratio 累积，而不是先算两个可能下溢的乘积再相除。每个线性 joint 值及总体量都携带 `available`、`exact_zero`、`underflow`、`overflow` 或 `invalid` 状态：下溢的线性密度可由有限 log-density 区分于真实零，非有限贡献、累计量或误差则使结果成为 `numerical_incomplete`，不得以 JSON `null` 配合 `confirmed` 掩盖。默认总体量逐层使用 `(2*A_path/S_total)*T` 与上述质量相乘，匹配产品 CPU/CUDA/Metal 共用的 projected-area entry normalization；它是无量纲 native measure 乘原始场景谱权重。raw `A_path` 和 `S_total` 以引擎原生几何的长度平方单位单独保留（正六棱柱面距为1时边长为1/2；转换至 LI 的 `a=1` 面积均需乘4，而 `2*A_path/S_total` 不变），不是 `m²` 或 `sr`，也不能把 raw LI 面积矩当成产品已归一亮度。联合样本、太阳节点和 illuminant 节点分别给误差估计；field 自身的导数 availability 和 margin 状态仍是局部数值证据，两者不得混写。一个有限太阳或 illuminant 只有一个节点时，结果明确为 `numerical_incomplete`。

行级 `status` 保留全局源或逐层条件质量的 `zero_weight`，`evaluation_status` 独立保留 field/生成数值失败；全零晶体层、所选晶体零份额、filter 拒绝后的零接受份额以及零 continuation/exit 质量都是合法零测度，不是无效配置或物理不可达。严格零与严格正性由完整的源／选择／filter／continuation／exit 因子账本判定，不依赖 field 求值推进到哪一层，也不依赖线性浮点乘积能否表示；全正因子的乘积下溢属于 `numerical_incomplete`，不是 `zero_weight`。结果级计数同时汇总测度与求值状态，因此零质量行即使保留了失败的 field 诊断，也不会把严格零测度升级成非零或未完成测度。若 filter 的接受结果依赖连续方向或对称展开，有限随机节点全部被拒绝仍只能得到 `numerical_incomplete`；只有接受性在剩余支持上恒定并已完整判定，才可据此声明严格 `zero_weight`。同理，连续支持的有限随机样本全部未命中不能证明全局不可达，只有完整原子枚举才可声明 `physically_unreachable`。积分流式处理全部行，JSON 只保存确定性 bottom-k hash 代表样本；内部消费者通过同 seed 重放或逐行 visitor 读取完整场，不得把代表样本当作全集。

## 候选与证据

自动发现至少检查下列来源：

- 实际支持上的映射临界或秩损失；
- 有效域边界、角点和多个约束的交汇；
- 每一个内部接口的 TIR/Fresnel kink；
- 原子或严格低维姿态支持，以及有限宽分布形成的集中；
- 平滑加权亮度的极值/脊线和跨波长的空间变化。

点质量要求某个天空方向的整个逆像具有正的加权测度：`ν({y}) = ∫_{F⁻¹({y})} A·T dμ > 0`。输入原子或正测度的常向分支可以满足这一条件；孤立临界点的局部 `rank=0` 本身不能证明存在点质量。对光滑支持的常秩分支，受限像的局部维数等于该秩；零测度临界集的像是焦散候选，不应被当作整个推送测度的点／线支持。严格约束和有限展宽必须分开报告：后者只在给定分辨率和不确定度下可以称为集中。

每个结果携带天空位置或范围、具体成员/接口/波长、活动约束、局部导数或两侧权重、分辨率和数值状态。可用状态包括 `confirmed`、`candidate`、`not_detected_at_resolution`、`numerical_incomplete`、`physically_unreachable` 和 `not_supported`。没有记录不是“没有特征”的证据。

有限候选池的完成、disk 条件下的拓扑证书或特殊几何的闭式公式，只在其明确前提下提供局部保证或加速；它们不证明所有输入或所有连通分量均已发现。

### 当前实现边界

产品 adapter 在 report 的 schema 3 路径中通过 `SceneMeasure` 的完整 visitor 构造版本化
support batch；`visited_row_count` 在任何物化上限判断之前递增，因此 JSON 保存的代表行不参与发现。
batch 保留实际 member chain、层/接口、谱/太阳节点、support 坐标、命名 margin、权重和完整链输出方向。
adapter 对每个物理活动坐标从同一 provenance 分支生成 lower/centre/upper 局部胞元，通过
`SceneMeasure` 的生产几何、filter 和原生权重 ledger 重求值；这些探针行显式不累加天空测度。
因此邻接关系来自真实局部胞元，而不是 visitor 遍历顺序。多层方向导数通过同一行状态上的双尺度扰动
传播完整 `outgoing→incident` 链，对 shape、pose、太阳和谱的所有活动坐标计算二维天空切映射的正则秩与受限秩；
三维嵌入矩阵的恒零行列式不作为临界证据。

analytic kernel 对每个命名约束检查活动点和邻接边上的两侧符号，分别产生 support、TIR、filter
和 weight 机制；多个同点活动约束另外产生 corner。没有同分支 callback 细化的交叉只保留
`candidate`。输入原子按质量合并；严格低维支持、有限宽支持和孤立 rank-0 临界保持不同机制，
后者不会升级成点质量。场景亮度由统一等面积天空网格边缘化，输出质量、每球面度密度、梯度、
Hessian 特征值、分辨率与粗细误差；极大值和脊线只有在两级网格一致时才确认。输入按稳定
`sample_id` 顺序累积，因此 visitor 分块和访问顺序不改变浮点求和顺序。

schema 3 的 report 只序列化上述 analytic 结果，不再调用固定 reference prism、固定 face sequence
或预设太阳侧的探测器。一面外反射、复合多层、非参考 shape 和未点名路径进入同一流程。冻结的
schema 1 请求仍保留历史 fixture 输出作为 ABI 兼容前缀；该兼容层不决定 schema 3 是否运行发现。

## 共享数值边界

独立 analytic 库提供可批量消费的数值，不导出产品 report。API version 11 的
`LUMICE_ANALYTIC_DiscoverFeatures` 接收 `struct_size`/stride 版本化的 support rows、constraints、
topology edges 和同步 callback，并返回 candidates、mechanism records 与 sky field；所有输出和字符串
由单一 result storage 持有并通过 `LUMICE_ANALYTIC_ReleaseFeatureDiscoveryResult` 释放。通用发现所需的行数据包括具体几何/成员/波长/姿态、方向与坐标约定、有限入口测度、逐接口 `T/R`、命名的有效域和 TIR margin、必要的一阶/二阶切导数和折射率导数，以及逐行数值状态。
版本 2 support batch 将坐标维度改为动态，追加了 `accumulates_measure` 和显式局部胞元轴；版本 1 的原始前缀仍可读，其 16 维上限不扩展到新版本。

只加载 `liblumice_analytic` 的 ctypes consumer 用独立等面积求和和约束根 oracle 验证数组布局、
stride/version/pointer 错误、回调借用期、释放幂等、访问顺序和并发调用。产品 JSON 不是这一数值契约的替代品。

## 验收

科学验收应在未点名路径、非参考形状及实际姿态、展宽、太阳和光谱变化下运行同一算法。每种机制应配隔离对照，只改变正在解释的因子；颜色结论还须有空间配对、单色和无边反例。固定场景的绿灯、JSON 字段存在或总量积分收敛本身都不构成通用发现完成。
