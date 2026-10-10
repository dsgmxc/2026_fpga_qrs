# BD / SmartConnect 核对 —— `0x1C` 写入异常根因排查(第 3 组)

*2026-10-10 · 只读核对,未动任何源文件、未跑 Vivado*

## 0. 本次要回答什么

前两组已经把异常**坐实**、把范围**缩到 `cv_sys` 之外**
(`20261010_relay_cfg_write_anomaly.md` §7):

| 已排除 | 依据 |
|---|---|
| 源 RTL | `cv_axi_lite_slave.v:714` `relay_set_r <= 1'b1` 只在写当拍为高,一个 `do_write` 一拍 |
| 综合后网表 | `relay_set_r` 只有 1 个 FF,其 D 锥含 `~bvalid_r` |
| PS 脚本写法 | 换独立进程、换写入值、换绕法,现象不变 |

**剩下来只有一条**:一次 PS 写 → 两次 AXI 事务。本次核对 **BD 里有没有能"一变二"的机制**。

---

## 1. BD 结构

`qrs.srcs/sources_1/bd/system/system.bd`(1579 行)只有 4 个 block:
`processing_system7_0` / `axi_smc` / `rst_ps7_0_50M` / `cv_sys_0`。

```
processing_system7_0/M_AXI_GP0  →  axi_smc/S00_AXI
axi_smc/M00_AXI                 →  cv_sys_0/S_AXI          (AXI4Lite, ADDR 8, DATA 32)
cv_sys_0/M_AXI (AXI3, 64 bit)   →  ps7/S_AXI_HP0           ← 读 DDR 帧缓存,另一条路
```

**`axi_smc` 上只有 1 个主 1 个从**(`NUM_SI=1` / `NUM_MI=1`)。两侧同频
(`FCLK_CLK0` 50 MHz)⇒ 接口上**没有 CDC**。

---

## 2. SmartConnect 内部(展开后的 `bd_44e3.bd`)

`STRATEGY=AUTOMATIC`,展开出 5 个 `sc_node` + 两个转换器 + MMU + 事务调节器。
把"能把一笔变成多笔"的四类机制逐个对过:

| 机制 | 关键参数(实际解析值) | 判定 |
|---|---|---|
| **位宽转换**(会把 1 拍拆成多拍) | SI/MI 都是 `WDATA_WIDTH=32`;每个 `sc_node` `MAX_PAYLD_BYTES=4` / `S00_NUM_BYTES=4` / `M00_NUM_BYTES=4` | ❌ 无转换,没东西可拆 |
| **跨时钟域**(同步器会重采样) | `NUM_CLKS=1`,两侧都是 `FCLK_CLK0` 50 MHz | ❌ 不存在 |
| **MMU 多次查表 / 多段重映射** | `NUM_SEG=1`,`SEG000_BASE_ADDR=0x43C00000`,`SEG000_SIZE=12`(4 K),`MSC000_ROUTE=0b111`(不重映射) | ❌ 单段、单次查表 |
| **事务调节器 / 仲裁重发** | `SINGLE_ISSUING=1`,`NUM_WRITE_OUTSTANDING=8` | ❌ 只限流,不复制 |

唯一能改**事务条数**的是 **AXI3 → AXI4-Lite 协议转换**(`m00_exit`:`HAS_BURST=0`、
`M_MAX_BURST_LENGTH=0`,`AXILITE_CONV=1`)。AXI4-Lite 不能突发,所以转换器
**必须**把一笔突发拆成逐拍的单次写。**但 A9 发出的一次 32 位 store 本身就是 1 拍**,
拆无可拆 ⇒ 这一层同样不能把 1 变成 2。

> ⚠️ **不确定性(不掩盖)**:以上是**从生成的配置读出来的,不是仿真**。
> Vivado 的 SmartConnect 是加密 IP,**无法仅凭配置证明内部没有重发路径**。
> 要彻底排除,只能对 SmartConnect 做仿真,或在 `S_AXI` 写通道挂 ILA(要重建位流)。
> 这是本次不做的事。

---

## 3. 🆕 附带发现:地址窗口 4 K,从设备只译 8 位 —— 全程混叠

| 口径 | 值 | 出处 |
|---|---|---|
| BD 地址映射给 `cv_sys_0` 的范围 | **4 K** | `system.bd:1557-1559`;`system.hwh` 也是 `0x43C00000–0x43C00FFF` |
| SmartConnect MMU 段大小 | 4 K(`SEG000_SIZE=12`) | `bd_44e3.bd` |
| **从设备实际译码宽度** | **8 位(256 B)** | `cv_axi_lite_slave.v:130` `C_S_AXI_ADDR_WIDTH = 8` |
| SmartConnect 出口宽度 | **8** | `m00_sc2axi` / `m00_exit` 的 `AXI_ADDR_WIDTH=8` |

⇒ **地址位 `[11:8]` 被静默丢弃**:整个 `0x43C00100–0x43C00FFF` 全部混叠到
`0x43C00000–0x43C000FF`。

**当前无害**(寄存器最高只到 `0x98`),但这是一条**真实的潜在隐患**,两个后果:

1. **交付文档里"4 K 窗口"的说法是错的** —— 实际可用窗口只有 256 B
   (`0x00–0xFF`),是它的 1/16。`cv_regs.py` 文件头写的"地址窗口 = 0x00 ~ 0xFC"
   才是对的;要改的是 BD 地址映射那一栏,或把 `ADDR_WIDTH` 提到 12。
2. **`0x1C` 的"地址绑定"不是全局保证** —— 在低 8 位内成立
   (`awaddr_r[7:2]` 分别是 `6'h06`/`6'h07`,两个脉冲出自两个独立 FF,无串扰),
   但 `0x43C0011C`、`0x43C0021C` … 都会打到同一格。
   ⇒ 排查时"我只写了 0x1C"这句话,**只在地址是 `0x43C0001C` 时才成立**。

> 📌 这不是本次异常的成因(没有代码会去写 `0x43C001xx`),但**必须记下来** ——
> 它决定了"地址绑定"这条推理能用多强。

---

## 4. 顺带核对的:`cv_sys_0` module_ref 缓存

`system.bd` 里 `cv_sys_0/S_AXI` 声明 `PROTOCOL=AXI4LITE, ADDR_WIDTH=8, DATA_WIDTH=32`,
缓存 XCI `system_cv_sys_0_5.xci` 同值,源码 `cv_sys.v:65-66` 也同值 ⇒ **三处一致,无漂移**。

⚠️ 但注意 `ADDR_WIDTH` 那一栏**恰好等于从设备的 8**;`system.bd` 只把它当成
`cv_sys_0` 自己的端口宽度,并没拿它去约束 BD 地址映射的范围 —— 这就是第 3 节那个
4 K / 256 B 落差的来源。

另有一份旧的实例缓存 `system_cv_sys_0_0/`(10-06),接口参数与 `_0_5` 逐字节相同,
BD 引用的是 `_0_5`。**未被引用、无害。**

---

## 5. 结论

1. **BD 里没有"一写变两写"的机制。** 四类结构逐一排除(§2)。
   剩下唯一没排除的,是加密 IP 内部的、配置上看不见的重发路径 —— **无法从配置判断**。
2. **历史实测全部来自临时脚本的裸 `/dev/mem` mmap,从没走过 PYNQ 的 `MMIO.write`。**
   而这两条路虽然都落到 `/dev/mem`,**映射属性由各自的开法决定**,不能互相代表。
   ⇒ **下一次上板第一件事就是把两种后端各跑一遍**(§6)。
3. **新发现一条潜在隐患**(§3):4 K 映射 vs 8 位译码 ⇒ `0x100–0xFFF` 全程混叠。

### 下一步(按信息量排)—— ✅ 已于 2026-10-10 当晚结案

| # | 做法 | 能分开什么 | 代价 | 结局 |
|---|---|---|---|---|
| **①** | `tools/relay_rw.py` 不加参数(vs `--raw`)各跑一遍 | 两个后端**都**复现 ⇒ PS/Zynq 层面的真问题;只有 `--raw` 复现 ⇒ 临时脚本的 mmap 问题 | 零 | ✅ **跑了,但结论比这个二选一更细** —— 两个后端**都**正常,异常的只有 `struct.pack_into` 这一种写法 |
| ② | `S_AXI` 写通道挂 ILA,看 `awvalid/awaddr/aw_hs/w_hs/do_write/relay_set_r` | 直接看是两笔 AXI 还是一笔两脉冲 | 重建位流 + 一次上电 | 🚫 **不需要了** —— 根因已在 PS 侧用零成本手段坐实 |
| ③ | 对 SmartConnect 做 AXI VIP 仿真,灌一笔写 | 加密 IP 内部有没有重发 | 搭一套仿真环境(最贵) | 🚫 **不需要了**,同上 |

**结案结论(2026-10-10 当晚,完整证据见 `20261010_relay_cfg_write_anomaly.md` §10):**

> 病根在 PS 侧写通路的**调用方式**:`struct.pack_into` 对一个 Python `mmap` 会发出
> **两笔** AXI 事务(先写 `0x00000000`,再写载荷),而 `pynq.MMIO`(numpy 整字存)
> 与 `mm[off:off+4] = ...`(切片赋值)**都只发一笔**。
> ⇒ `relay_ctrl` 一笔事务一拍脉冲,第一笔 `0x00`(白名单里的 `COSS_DISCHARGE`)
> 被受理并锁 40 ms busy,第二笔(真值)撞 `ERR_BUSY` ⇒ 硬件停在 `0x00`。
> **SmartConnect 与 PL 全部无辜** —— 本文档"不在 `cv_sys` 里"的判断是对的,
> 只是当时把范围划在了"PS 写通路",而真正的分界线在**更靠前的一行 Python 调用**。

⚠️ **本文件 §4 第 2 条("两条路的映射属性由各自的开法决定,不能互相代表")是错的** ——
   实测两者都是 `/dev/mem` + `O_SYNC`(`pynq/pl_server/embedded_device.py:523`)。
   差别不在映射属性,在**写下去的位宽/笔数**。这条错判曾把排查方向引向 ILA。
