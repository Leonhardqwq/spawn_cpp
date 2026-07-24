# PVZ DOE 工作流

本目录把原来分散在 C++、notebook 和 Excel 中的流程合并为一个可重复执行的入口：

```text
DOE 设计
→ 在固定的 2^32 出怪编码环中选取连续种子片段
→ 让各片段在 level 上连续衔接
→ 导出模拟顺序并录入阳光响应
→ 拟合效应模型并生成报告
```

新增代码只负责工作流编排。PVZ 随机数、出怪、种子换算、搜索和阳光模型尽量直接复用项目根目录的 `inc/`：

- `inc/basic_setting.h`：`Scene`、`ZombieTypeList`、`SEED_STEP`、`inv`、`rng_seed`/`real_seed` 换算、权重等；
- `inc/quick_seed_finder.h`：原有 Quick KMP、评分、连续条件和 DOE 集合搜索；
- `inc/sun_model.hpp`：原有阳光模型及冲关模拟；
- `inc/json.hpp`、`inc/util.hpp`：JSON 和公共工具。

四个 `data/*.bin` 是现有固定只读数据。本工作流不会重建、迁移或修改它们。

## 文件结构

```text
pvz_doe/
├── README.md
├── main.cpp                  # inc 的薄 C++ 命令行封装
├── workflow.py              # 用户入口：DOE、选段、拟合和报告
├── pvz_doe.exe              # 编译产物，不提交
└── experiments/
    └── <experiment_id>/
        ├── experiment.json  # 实验的唯一配置
        ├── design_input.csv # 仅 design.method=import 时使用
        ├── observations.csv # 人工或外部模拟器填写的响应
        └── out/             # 可重新生成，禁止手工修改
            ├── design.csv
            ├── schedule.csv
            ├── model.json
            ├── report.html
            ├── evaluation.json
            └── trajectories.csv # 仅 trace 模拟时生成
```

只有 `experiment.json`、可选的 `design_input.csv` 和 `observations.csv` 是实验输入。`out/` 下的内容均由程序生成。

## 环境与编译

要求：

- 64 位 Windows；
- 支持 C++17 和线程库的编译器；
- Python 3.10 或更高版本；
- NumPy；
- SciPy 可选：安装后报告会给出精确的 t 检验 p 值，否则 p 值留空。

从项目根目录编译 C++ 内核：

```powershell
g++ -std=c++17 -O3 -pthread pvz_doe/main.cpp -o pvz_doe/pvz_doe.exe
```

安装 Python 依赖：

```powershell
python -m pip install numpy
```

可选：

```powershell
python -m pip install scipy
```

先运行最小自检：

```powershell
pvz_doe\pvz_doe.exe selftest
```

搜索时只加载当前场地的一个 16 GiB 文件，不会同时加载四个文件。应为进程留出足够的物理内存。

## 普通使用流程

### 1. 创建实验

```powershell
python pvz_doe/workflow.py init de_half DE `
  --display "DE 半花" `
  --short DE05 `
  --alias "DE半花" `
  --alias "半花DE"
```

场地只能是 `DE`、`NE`、`PE`、`RE`。命令创建：

```text
pvz_doe/experiments/de_half/experiment.json
```

之后直接编辑这个 JSON，设置因子、DOE 方法、模型项和模拟参数。

实验可通过稳定 `id`、简称或任一别名引用：

```powershell
python pvz_doe/workflow.py design de_half
python pvz_doe/workflow.py design DE05
python pvz_doe/workflow.py design "DE半花"
```

列出所有实验和称呼：

```powershell
python pvz_doe/workflow.py list
```

### 2. 单独检查 DOE

```powershell
python pvz_doe/workflow.py design DE05
```

该命令只生成 `out/design.csv`，并检查设计行唯一性与当前模型矩阵的秩，不扫描 16 GiB 数据。

### 3. 自动选取和拼接种子片段

```powershell
python pvz_doe/workflow.py prepare DE05
```

可指定 C++ 内核和线程数：

```powershell
python pvz_doe/workflow.py prepare DE05 `
  --engine pvz_doe/pvz_doe.exe `
  --threads 12
```

`prepare` 一次完成：

1. 生成或导入 DOE；
2. 将设计点投影为原有出怪 bit；
3. 调用 C++ QuickSet 搜索最长的不重复目标片段；
4. 删除已覆盖设计点并继续搜索，直到覆盖全部设计；
5. 为各片段分配连续的 level，并重新计算相应 `real_seed`；
6. 展开和复核每个样本；
7. 生成 `out/schedule.csv`；
8. 在不存在时创建 `observations.csv` 模板。

当前选段策略与旧流程一致，是确定性的贪心覆盖，不承诺得到全局最少片段数。

### 4. 运行外部战斗模拟并填写响应

按照 `out/schedule.csv` 的 `execution_order` 运行阵型模拟。每逢 `segment_id` 改变，就在对应的起始 `level` 切换为该段的 `real_seed`；level 不重置。

把结果填入实验根目录的 `observations.csv`。`prepare` 不会覆盖已有观测。

### 5. 拟合模型

```powershell
python pvz_doe/workflow.py analyze DE05
```

该命令按 `sample_id + sample_key` 校验并连接日程与观测，拟合配置中给定的 DOE 主效应和交互效应，生成：

- `out/model.json`：供 `SunModel` 继续计算的模型；
- `out/report.html`：实验配置、设计检查、观测和拟合结果。

### 6. 检查整个实验

```powershell
python pvz_doe/workflow.py check DE05
```

检查内容包括配置、DOE 秩与覆盖、片段顺序、出怪投影、种子换算、连续 level、观测引用以及 model/evaluation 的输入指纹。

### 7. 查询、独立搜索和阳光传播

查看指定 `real_seed` 在连续 level 下的出怪：

```powershell
python pvz_doe/workflow.py inspect DE05 0x6A25F176 --level 1000 --count 5
```

独立使用原有 Quick 搜索：

```powershell
# 精确编码序列
python pvz_doe/workflow.py search exact NE 100667 319801 393519 --threads 12

# 长度 50 的窗口；评分因子可写名称、简称或 bit
python pvz_doe/workflow.py search score NE 50 红眼=10 白眼=5 --threads 12

# 搜索满足包含/排除条件的连续区间；两个选项均可重复
python pvz_doe/workflow.py search run NE --require 0x40000 --exclude 0x40 --threads 12
```

三种搜索都支持 `--start-rng-seed`、`--find-len`、`--threads` 和 `--engine`。完整扫描的 `--find-len` 可写 `4294967296`。

`run` 中多个 `--require` 之间为 OR，多个 `--exclude` 之间也为 OR，两组结果再作 AND。它使用单线程 $O(n)$ 精确扫描：完整 $2^{32}$ 环会合并首尾，较短 `find_len` 按线性区间处理。

`analyze` 生成模型后，可以传播阳光模型：

```powershell
# Monte Carlo 破阵率
python pvz_doe/workflow.py simulate mc DE05 `
  --trials 10000 --level-num 5000 --sampler-seed 0x01352773

# 指定 real_seed 的逐 level 轨迹
python pvz_doe/workflow.py simulate trace DE05 0x6A25F176 `
  --level-start 1000 --level-num 5000

# 编码环上的最坏累计损失区间；调试时可限制扫描长度
python pvz_doe/workflow.py simulate worst DE05 --find-len 1000000
```

三种结果分别写入 `out/evaluation.json` 的 `mc`、`trace`、`worst` 字段；`trace` 还会生成 `out/trajectories.csv`。

`sampler_seed` 只用于复现 Monte Carlo 起点抽样，不是 PVZ 种子。MC 还返回有限 horizon 下的 `mean_survival_levels` 和 `censored_count`，最难样本的 `failure_step` 在未失败时为 `null`。`trace.failed` 在首次破阵后保持为真。`worst` 默认只对完整环使用 `circular=true`；调试子区间默认线性，结果同时给出 `circular` 和 `wraps`。

## `experiment.json`

下面展示分式析因配置。中文名、简称和多个称呼都可以直接编辑。

```json
{
  "schema_version": 1,
  "id": "de_half",
  "names": {
    "display": "DE 半花",
    "short": "DE05",
    "aliases": ["DE半花", "半花DE"]
  },
  "scene": "DE",
  "seed": {
    "user_id": 1,
    "mode_id": 13,
    "level_start": 1000
  },
  "factors": ["红眼", "橄榄", "冰车", "气球", "白眼", "舞王", "密度"],
  "density_threshold": 12000,
  "design": {
    "method": "fractional",
    "terms": [
      [],
      ["红眼"],
      ["橄榄"],
      ["冰车"],
      ["气球"],
      ["白眼"],
      ["舞王"],
      ["密度"],
      ["红眼", "橄榄"]
    ],
    "base_factors": ["红眼", "橄榄", "冰车", "气球"],
    "generators": {
      "白眼": {
        "product": ["红眼", "橄榄", "冰车"],
        "sign": 1
      },
      "舞王": {
        "product": ["红眼", "橄榄", "气球"],
        "sign": 1
      },
      "密度": {
        "product": ["红眼", "冰车", "气球"],
        "sign": -1
      }
    }
  },
  "simulation": {
    "sun_start": 9990,
    "sun_end": -500,
    "sun_cap": true,
    "bias": 0.0,
    "level_num": 5000,
    "sim_num": 10000,
    "sampler_seed": "0x01352773",
    "output_num": 20
  }
}
```

字段约束：

- `id` 是稳定目录名；修改显示名称时不要修改它；
- `names.display` 用于报告标题；
- `names.short` 和 `names.aliases` 都可用于命令行定位实验；
- `scene` 只决定 `data/<scene>.bin` 和场地出怪限制；MGE 等阵型名仍应使用实际数据场地 `DE`；
- `level_start` 是整场实验第一个样本的 level，而不是每个片段各自的起点；
- `factors` 可使用 `workflow.py` 中 `FACTOR_ALIASES` 登记的中文名、英文名或简称；
- `density_threshold` 只在这里定义，因子“密度”沿用原逻辑 `weight_has(code) < density_threshold`；
- `design.terms` 同时定义 DOE 关心的模型空间和最终回归项；
- `simulation` 保存阳光模型传播参数，不影响 DOE 选段；其中 `bias` 是每个 level 加到回归预测上的固定响应修正。

要增加或修改僵尸简称，编辑 `workflow.py` 开头的 `FACTOR_ALIASES`。整数 bit 仍由原有 `inc` 定义，不在实验配置中重复维护。

## 四种 DOE 方法

### `full`

```json
{
  "method": "full",
  "terms": [[], ["红眼"], ["橄榄"], ["红眼", "橄榄"]]
}
```

对 $k$ 个因子生成完整的 $2^k$ 个 $-1/+1$ 设计点。

### `fractional`

正规二水平分式析因设计。若基础列为 $A,B,C,D$，生成元为：

$$
E=ABC,\qquad F=ABD,
$$

则程序先生成基础列的 $2^4$ 行，再逐行计算：

$$
x_E=x_Ax_Bx_C,\qquad x_F=x_Ax_Bx_D.
$$

`sign=-1` 表示对该生成列整体取反。每个生成元的 `product` 只能引用 `base_factors`，避免递归和含义不确定的生成关系。

程序检查所配置模型项在该设计上是否满秩；当前不另行输出定义关系、分辨率或完整别名结构。

### `d_optimal`

```json
{
  "method": "d_optimal",
  "runs": 24,
  "random_seed": "0x01352773",
  "starts": 20,
  "max_iter": 100,
  "terms": [[], ["红眼"], ["橄榄"], ["红眼", "橄榄"]]
}
```

候选集为所列因子的完整二水平超立方体。程序用固定随机种子的多起点交换算法寻找较大的：

$$
\det(X^\mathsf{T}X).
$$

`terms` 决定 $X$ 的列，所以设计和最终拟合使用同一模型空间。该方法是确定可复现的启发式搜索，不宣称得到全局最优解。

### `import`

```json
{
  "method": "import",
  "input": "design_input.csv",
  "terms": [[], ["红眼"], ["橄榄"]]
}
```

`input` 省略时默认读取实验目录下的 `design_input.csv`：

```csv
design_id,红眼,橄榄,冰车
D001,-1,-1,1
D002,1,-1,-1
```

因子值只能是 `-1` 或 `1`，设计行和 `design_id` 必须唯一。该入口用于历史 DOE、外部软件设计和非正规分式设计。

## 模型项的写法

`design.terms` 使用因子名称列表：

```json
[
  [],
  ["红眼"],
  ["橄榄"],
  ["红眼", "橄榄"]
]
```

含义依次为：

$$
1,\qquad x_{\text{红眼}},\qquad x_{\text{橄榄}},\qquad
x_{\text{红眼}}x_{\text{橄榄}}.
$$

所有二值因子在 DOE 和回归中统一使用 `-1/+1`。模型为：

$$
Y=\sum_{T\in\mathcal T}\beta_T\prod_{f\in T}x_f+\varepsilon.
$$

空列表代表截距；一个因子代表主效应；两个因子代表二阶交互。

## 连续 level 的种子拼接

QuickSet 搜索只决定每个片段的起始 `rng_seed` 和长度，不为片段固定 `real_seed`。所有片段找到后，程序再按最终执行顺序连续分配 level。

设实验起始 level 为 $L_0$，第 $j$ 个片段长度为 $\ell_j$，搜索到的起始 `rng_seed` 为 $r_j$。该片段的起始 level 是：

$$
L_j=L_0+\sum_{q<j}\ell_q.
$$

随后直接按原有 `BasicSeedFinder::get_real_seed()` 计算：

$$
\operatorname{real\_seed}_j
=r_j-\mathrm{user\_id}-\mathrm{mode\_id}-101L_j
\pmod {2^{32}}.
$$

片段内偏移 $k$ 的样本为：

$$
\begin{aligned}
\mathrm{level}_{j,k}&=L_j+k,\\
\mathrm{rng\_seed}_{j,k}&=r_j+101k\pmod {2^{32}},\\
\mathrm{real\_seed}_{j,k}&=\operatorname{real\_seed}_j.
\end{aligned}
$$

因此，如果第一段使用 level 1000–1010，第二段必定从 level 1011 开始。片段边界只切换 `real_seed`，不会把 level 重置为 1000。

## 输出数据

所有种子值统一输出为固定 8 位的大写十六进制字符串，例如 `0x00000065`。这包括 `rng_seed`、`real_seed`、`start_rng_seed`、`end_rng_seed`、`mode2_seed`、`sampler_seed`、`random_seed` 和搜索结果的 `seeds`；`seed_count` 等计数仍是十进制整数。命令行与配置输入同时兼容这种十六进制写法和旧的十进制写法。

### `out/design.csv`

一行对应一个 DOE 设计点：

```csv
design_id,红眼,橄榄,冰车,气球,白眼,舞王,密度
D001,-1,-1,-1,-1,1,1,-1
```

`design_id` 是连接 DOE、执行顺序和响应的稳定键。下游不得依赖文件行号连接数据。

### `out/schedule.csv`

一行对应一个需要实际运行的样本，核心列为：

```csv
sample_id,sample_key,schedule_key,execution_order,design_id,segment_id,segment_offset,level,rng_seed,real_seed,bin_index,spawn_code,projected_code,weight
S001,7dd63b1456b9fb6c,...,0,D017,G001,0,1000,0x00000065,0xFFFE75CF,...,...,...,...
S002,d366a97c31de8137,...,1,D003,G001,1,1001,0x000000CA,0xFFFE75CF,...,...,...,...
S012,6cd7890c47f337f5,...,11,D004,G002,0,1011,0x12345678,0x1232C78B,...,...,...,...
```

`schedule_key` 绑定场地、种子上下文、因子、密度阈值和 DOE 设计行。`sample_id` 只是当前执行表中的短编号；`sample_key` 还绑定 `schedule_key`、种子、出怪编码和权重。因此重新 `prepare` 后即使仍叫 `S001`，旧观测也不会被静默接到新样本上。

程序自动验证：

- `level` 从 `level_start` 起逐行连续；
- 同一 `segment_id` 的 `real_seed` 不变；
- 同段 `rng_seed` 每行增加 101；
- `bin_index` 每行增加 1，并按 `uint32_t` 环绕；
- `projected_code` 与 `design_id` 对应的设计点一致；
- 每个 DOE 设计点恰好被覆盖一次。

### `observations.csv`

这是唯一需要人工或外部模拟器填写的响应文件：

```csv
observation_id,sample_id,sample_key,replicate,raw_response,adjustment,valid,source,note
O001,S001,7dd63b1456b9fb6c,1,-1200,250,1,sim-run-001,补南瓜修正
O002,S002,d366a97c31de8137,1,300,0,1,sim-run-001,
O003,S002,d366a97c31de8137,2,250,0,1,manual-repeat,
```

最终响应统一定义为：

$$
Y=\mathrm{raw\_response}+\mathrm{adjustment}.
$$

约定 $Y>0$ 表示该 level 净增加阳光。模拟器已经给出最终响应时，将 `adjustment` 填为 0。同一样本可用不同 `replicate` 记录重复试验；无效数据保留但令 `valid=0`。

不要手工修改 `sample_key`。若程序报告它与当前 schedule 不匹配，应重新导出或迁移观测，而不是把旧 key 改成新值。

### `out/model.json`

保存拟合系数、标准误、拟合指标，以及用于拒绝过期产物的 `analysis_key`/`model_key`。为直接复用 `inc/sun_model.hpp`，可执行部分沿用原有格式：

- 截距：`factor1=19, factor2=19`；
- 主效应：`factor1=<bit>, factor2=19`；
- 二阶交互：`factor1=<bit1>, factor2=<bit2>`；
- 密度因子：bit 20；
- `coef` 与两个 factor 数组逐项对应。

模型另存 `bias`，`SunModel` 对每个 level 使用：

$$
\widehat Y=\sum_i\mathrm{coef}_i\,x_{\mathrm{factor1}_i}x_{\mathrm{factor2}_i}+\mathrm{bias}.
$$

报告元数据不会参与 `SunModel` 计算，但用于追踪实验、样本数、秩、$R^2$、调整 $R^2$ 和 RMSE。

### `out/report.html`

自包含的只读结果页，汇总：

- 实验名称、场地和有效样本数；
- 有效观测、重复试验及逐样本拟合结果；
- 系数、标准误、拟合指标和可用时的 p 值；
- 实际值与预测值。

### `out/evaluation.json`

保存 `simulate` 的机器可读结果。同一 `model_key` 下按 `mc`、`trace`、`worst` 分开保存；模型不变时运行一种模式不会删除另外两种结果，重新拟合得到新模型后首次模拟会清除旧模型的结果。

### `out/trajectories.csv`

仅由 `simulate trace` 生成，一行对应一个 level：

```csv
real_seed,step,level,rng_seed,spawn_code,predicted_response,sun,failed
```

`evaluation.json` 只保存该轨迹的摘要和文件名，完整逐步数据放在此 CSV。

## C++ 内部接口

正常使用只运行 `workflow.py`。以下接口供工作流调用和排错：

```powershell
pvz_doe\pvz_doe.exe selftest
pvz_doe\pvz_doe.exe inspect SCENE REAL_SEED USER_ID MODE_ID LEVEL [COUNT]
pvz_doe\pvz_doe.exe search exact REQUEST.json
pvz_doe\pvz_doe.exe search score REQUEST.json
pvz_doe\pvz_doe.exe search run REQUEST.json
pvz_doe\pvz_doe.exe search set REQUEST.json
pvz_doe\pvz_doe.exe expand REQUEST.json
pvz_doe\pvz_doe.exe simulate CONFIG.json
pvz_doe\pvz_doe.exe simulate worst CONFIG.json
```

- `inspect` 查看指定 `real_seed` 在连续 level 下的 `rng_seed` 和出怪编码；
- `search exact|score|run` 分别实现精确序列、评分窗口和连续条件搜索；
- `search set` 调用原有 QuickSet，对当前剩余 DOE 点进行连续不重复集合搜索；
- `expand` 为选定片段连续分配 level，并生成完整样本行。
- `simulate CONFIG.json` 按 `SunConfig.mode=1|2|3` 执行 Monte Carlo、轨迹或最坏区间；`simulate worst` 强制使用最坏区间模式。

标准输出只写 JSON；诊断和错误写入标准错误；失败返回非零退出码。不要在 C++ 输出中混入供人阅读的提示，否则会破坏 Python 协议。

## 固定数据说明

场地与文件固定映射：

| 场地参数 | `Scene` | 数据文件 |
|---|---|---|
| `DE` | `DAY` | `data/DE.bin` |
| `NE` | `NIGHT` | `data/NE.bin` |
| `PE` | `POOL` | `data/PE.bin` |
| `RE` | `ROOF` | `data/RE.bin` |

每个文件包含 $2^{32}$ 个 little-endian `uint32_t`，总大小为 17,179,869,184 字节。数组定义为：

$$
A[i]=\operatorname{get\_list}\!\left(101i\bmod 2^{32}\right).
$$

由 `inc/basic_setting.h` 的 `inv=2083697005` 可得：

$$
\mathrm{bin\_index}=\mathrm{rng\_seed}\times\mathrm{inv}\pmod {2^{32}}.
$$

程序根据 `scene` 自动读取同名文件，只检查存在性和精确大小；配置中不允许另行指定路径。
