#!/usr/bin/env python3
"""
tools/pl_state.py —— 「PL 现在配没配置?」的**零风险**探针

为什么需要它:板上 `BOOT.BIN` **不含位流**(见 dev_handbook 红线 ⑤)。上电后 PL 是空的,
此时**任何**对 `0x43C00000` 的 AXI 读都会把 CPU 挂在总线上 —— 2026-10-02 就是这么
把板子写挂死过一次,只能断电。

`/sys/class/fpga_manager/fpga0/state` **答不了**这个问题 —— PYNQ 走
`/dev/xdevcfg` / XRT,不经过 fpga_manager,那个文件恒为 `unknown`(2026-10-15 实测)。

⇒ 改读 **devcfg 的 STATUS 寄存器**(`0xF8007014`)。它是 **PS 内部**外设,
   PL 空着也读得动,**不会挂总线**。

────────────────────────────────────────────────────────────────────────────
🔴 **2026-10-15 更正 —— 位定义以厂商头文件为准,不靠自己背**

    第一手来源:`<Vivado>/data/embeddedsw/XilinxProcessorIPLib/drivers/devcfg_v3_9/
              src/xdevcfg_hw.h`

🔴 **而且「哪个位才真的区分配置与否」也不能靠背 —— 2026-10-15 在同一块板上
   加载位流前后各读一次,实测校准:**

| 位 | 测试名 | 空 PL | **已配置** | 结论 |
|---|---|---|---|---|
| 5 `PSS_CFG_RESET_B` | 1 | 1 | **不区分** | 它描述的是 PL 的复位/供电状态,与"配没配"无关 |
| 9 `PSS_GPWRDWN_B` | 1 | 1 | **不区分** | 同上 |
| **10 `PSS_FST_CFG_B`** | 0 | **1** | ★ **区分** | "第一次 PL 配置完成" |
| INT_STS(0x0C) bit2 | 0 | **1** | ★ **区分** | `PCFG_DONE` 中断(粘滞)。整字 `0x00020000` → `0x00020004` |

✅ **两列都实测了**(2026-10-15 补):空 PL 那一列原先只记了 `STATUS`,
`INT_STS` 是**当天晚些时候冷启动后补读的** —— `INT_STS = 0x00020000`,bit2 = 0。
⇒ `INT_STS` bit2 与 `STATUS` bit10 **都**是已验证的判据(代码里 `pl_loaded()` 取两者的或)。

⇒ 判据取 **bit10 `FST_CFG_B`**。它回答的是「**本周期加载过位流没有**」——
   正是我们要问的那个问题(重配置只走 PCAP,只会让它从 0 变 1)。

⚠️ 位 5 常被记成 "PCFG_DONE",那是错的。本仓库 2026-10-03 写下的
   `tools/step1_plstate.py` 就是这么标的 —— 它在**空 PL 上也会报 DONE=1**,
   等于永远说"已配置"。已就地改正。

| 位 | 掩码 | 名字(Xilinx 原文) | 含义 |
|---|---|---|---|
| 4 | `0x00000010` | `XDCFG_STATUS_PCFG_INIT_MASK` | FPGA Init Status(PL 已初始化,**可以**接收位流) |
| 5 | `0x00000020` | `XDCFG_STATUS_PSS_CFG_RESET_B` | **PL config reset status** —— 1 = PL 处于配置复位 ⇒ **没配置** |
| 6 | `0x00000040` | `XDCFG_STATUS_ILLEGAL_APB_ACCESS_MASK` | 非法 APB 访问(粘滞) |
| 7 | `0x00000080` | `XDCFG_STATUS_SECURE_RST_MASK` | 安全复位 |
| 8 | `0x00000100` | `XDCFG_STATUS_PSS_GTS_CFG_B` | 配置期间三态 IO |
| 9 | `0x00000200` | `XDCFG_STATUS_PSS_GPWRDWN_B` | **Global power down** —— 1 = PL 掉电 ⇒ **没配置** |
| 10 | `0x00000400` | `XDCFG_STATUS_PSS_FST_CFG_B` | **第一次 PL 配置完成** |
| 11 | `0x00000800` | `XDCFG_STATUS_PSS_GTS_USR_B` | HIZ 期间三态 IO |

⚠️ **位 5 常被记成 "PCFG_DONE",那是错的。** 本仓库 2026-10-03 写下的
   `tools/step1_plstate.py` 就是这么标的的 —— 它会把**空 PL 报成「已配置 ✅」**
   (实测:空 PL 的 STATUS = `0x40000A30`,位 5 = 1)。已就地改正。
   `PCFG_DONE` 那个信号在 **INT_STS(0x0C)bit 2**(`XDCFG_IXR_PCFG_DONE_MASK = 0x04`),
   是**中断状态**位,不是 STATUS 的状态位。

判据(2026-10-15 实测校准,**两种状态各读一次**):

    空 PL    : STATUS = 0x40000A30  INT_STS = 0x00020000
               → CFG_RESET_B=1、GPWRDWN_B=1、FST_CFG_B=0、PCFG_DONE=0
    已配置 PL: STATUS = 0x50000F30  INT_STS = 0x00020004
               → CFG_RESET_B=1、GPWRDWN_B=1、FST_CFG_B=1、PCFG_DONE=1
                 ⚠️ 位 5、位 9 **两种状态都是 1** —— 拿它们当判据必错。

⇒ 只有 **bit10(FST_CFG_B)** 与 **INT_STS bit2(PCFG_DONE)** 区分。
"""

import os
import struct
import sys

DEVCFG_BASE = 0xF8007000
STATUS_OFF   = 0x14
INT_STS_OFF  = 0x0C

PCFG_INIT     = 0x00000010      # bit4
CFG_RESET_B   = 0x00000020      # bit5  ★ 1 = 没配置
ILLEGAL_APB   = 0x00000040      # bit6  粘滞
SECURE_RST    = 0x00000080      # bit7
GTS_CFG_B     = 0x00000100      # bit8
GPWRDWN_B     = 0x00000200      # bit9  ★ 1 = PL 掉电 = 没配置
FST_CFG_B     = 0x00000400      # bit10 ★ 1 = 第一次配置完成
GTS_USR_B     = 0x00000800      # bit11
INT_PCFG_DONE = 0x00000004      # INT_STS bit2


def devcfg_read():
    """读 devcfg 的 (STATUS, INT_STS)。**只读 PS 内部,不碰 PL。**

    别处要判断"能不能碰 AXI"就调这个,不要再抄一遍位定义 ——
    `tools/bias_mmap.py` 就是这么用的。
    """
    import mmap

    fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
    try:
        m = mmap.mmap(fd, 0x1000, flags=mmap.MAP_SHARED,
                      prot=mmap.PROT_READ, offset=DEVCFG_BASE)
        try:
            return (struct.unpack_from("<I", m, STATUS_OFF)[0],
                    struct.unpack_from("<I", m, INT_STS_OFF)[0])
        finally:
            m.close()
    finally:
        os.close(fd)


def pl_loaded():
    """本周期加载过位流没有?(bit10 `PSS_FST_CFG_B` 或 INT_STS `PCFG_DONE`)

    ⚠️ 判据的来历见文件头那张实测表 —— 位 5 是**不能**用的。
    """
    st, ix = devcfg_read()
    return bool((st & FST_CFG_B) or (ix & INT_PCFG_DONE))


def main():
    try:
        st, ix = devcfg_read()
    except PermissionError:
        print("✗ 打不开 /dev/mem —— 要 root(板上 `sudo -E python3 …`)", file=sys.stderr)
        return 2

    def b(mask):
        return (st & mask) >> (mask.bit_length() - 1)

    print(f"devcfg STATUS  0x{DEVCFG_BASE + STATUS_OFF:08X} = 0x{st:08X}")
    print(f"  PCFG_INIT    (bit4 ) = {b(PCFG_INIT)}     1 = PL 已初始化,可以接收位流")
    print(f"  CFG_RESET_B  (bit5 ) = {b(CFG_RESET_B)}     ┐ 这两个位加载位流前后**都是 1**,")
    print(f"  GPWRDWN_B    (bit9 ) = {b(GPWRDWN_B)}     ┘ 不区分配置与否(2026-10-15 实测)")
    print(f"  ILLEGAL_APB  (bit6 ) = {b(ILLEGAL_APB)}     1 = 有过非法 APB 访问(粘滞)")
    print(f"  FST_CFG_B    (bit10) = {b(FST_CFG_B)}     ★ 1 = 本周期加载过位流")
    print(f"devcfg INT_STS 0x{DEVCFG_BASE + INT_STS_OFF:08X} = 0x{ix:08X}   "
          f"PCFG_DONE(bit2) = {(ix & INT_PCFG_DONE) >> 2}  ★ 同上")

    configured = pl_loaded()
    print()
    if configured:
        print("✅ 本周期加载过位流 —— 0x43C00000 可读(mmap 路径 / Overlay 都可)")
    else:
        print("⛔ PL 是空的(本周期没加载过位流)—— **别碰 0x43C00000**,"
              "读了会把 CPU 挂在 AXI 上。")
        print("   先 `Overlay('qrs.bit')` 配一次(⚠️ 一次上电只允许一次)。")
    return 0 if configured else 1


if __name__ == "__main__":
    raise SystemExit(main())
