# PVZ 循环编码最短唯一长度

计算循环编码数组中任意两个起点的最大公共前缀 `max_lcp`，并输出：

```text
min_length = max_lcp + 1
```

## 编译

需要 64 位、支持 C++17 的编译器。进入本目录后执行：

```powershell
g++ -std=c++17 -O3 -march=native main.cpp -o lcp.exe
```

## 运行

```text
lcp <DE|RE|NE|PE>
```

例如：

```powershell
.\lcp.exe DE
.\lcp.exe RE
.\lcp.exe NE
.\lcp.exe PE
```

## 参数

- `DE`：白天，对应 `DE.bin`。
- `NE`：黑夜，对应 `NE.bin`。
- `PE`：泳池，对应 `PE.bin`。
- `RE`：屋顶，对应 `RE.bin`。

程序不接受数据文件路径参数，而是自动读取仓库 `data` 目录中与参数同名的文件。从 `LCP` 目录启动时使用 `../data/<类型>.bin`，从仓库根目录启动时使用 `data/<类型>.bin`。启动后会静默执行内置的小规模正确性检查。

正式源文件必须恰好包含 `2^32` 个连续的 `uint32_t`，总大小为 17,179,869,184 字节，无文件头。第 `i` 项即 `A[i]`；生成数据时对应的实际 RNG seed 为 `i * 0x65 mod 2^32`。

## 输出

- `length`、`classes`、`seconds`：当前倍增长度、不同窗口类别数和本轮耗时。
- `lcp_seconds`：循环 LCP 计算耗时。
- `max_lcp`：不同起点间的最大公共前缀长度。
- `min_length`：所求最短唯一定位长度，即 `max_lcp + 1`。
- `witness_indices`：达到 `max_lcp` 的一对起点。
- `unique_power_of_two`：倍增阶段首次使所有窗口唯一的二次幂长度。

全量运行约需 64 GiB 内存外加进程开销，建议使用明显多于 64 GiB 的可用内存。
