# 项目说明

## PVZ 游戏机制参考

分析出怪、随机数、种子、场景限制等真实游戏机制时，按以下顺序查询：

1. 游戏反编译源码（主要事实来源）：
   `D:\PVZ\research\self\coding\code\refer\PlantsVsZombies-decompilation`
2. PVZ 模拟器实现（用于核对可运行逻辑）：
   `D:\PVZ\research\self\coding\code\refer\pvz-emulator-examples\lib`

如果 notebook、模拟器与反编译源码存在差异，以反编译源码所体现的原版游戏行为为准，并在结论中说明差异。

## 固定出怪编码数据

`data/DE.bin`、`data/NE.bin`、`data/PE.bin`、`data/RE.bin` 是固定只读数据，分别对应 `DAY`、`NIGHT`、`POOL`、`ROOF`。每个文件包含恰好 \(2^{32}\) 个 little-endian `uint32_t`：

```text
A[i] = ZombieTypeList(scene).get_list(uint32_t(i * SEED_STEP))
```

其中 `SEED_STEP = 101`，`inv = 2083697005`，因此：

```text
index    = uint32_t(rng_seed * inv)
rng_seed = uint32_t(index * SEED_STEP)
```

不要修改或重新生成这些文件，也不要为其引入版本迁移逻辑。程序只需根据场地读取同名文件，并检查文件大小为 `2^32 * sizeof(uint32_t)`。

所有对外种子值使用固定 8 位的大写十六进制字符串 `0xXXXXXXXX`；输入同时兼容该格式和旧的十进制格式。`seed_count` 等计数不是种子值，仍使用十进制整数。
