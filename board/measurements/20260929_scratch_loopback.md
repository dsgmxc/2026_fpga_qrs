# 2026-09-29 · `SCRATCH` 读写回环 · 板上实测存档

> **P2 判据本体** —— 「`SCRATCH` 读写回环通过」。
> 这是全项目**第一次**由**自写的 RTL**(而非厂商 IP)提供 PS 可访问的 AXI 从机并跑通。
> 对应必做点 **M13**(板上跑通 + 实测输出)。

---

## 一、被测对象 / 测点

| 项 | 值 |
|---|---|
| **被测对象** | `cv_axi_lite_slave`(用户 Vivado 工程里的自写 AXI4-Lite 从机,225 行,`srcs/sources_1/new/cv_axi_lite_slave.v`) |
| **测点** | AXI-Lite 寄存器空间 **没有物理测点** —— 本轮测的是**寄存器读写语义**,不是引脚电平 |
| **基址** | `0x43C0_0000`,窗口 4 KB(`range = 4096`) |
| **实测寄存器** | `0x48 VERSION`(RO)、`0x4C SCRATCH`(RW) |
| **本轮未实现** | 其余 18 个寄存器(`case` 里没有 → 读返回 `0x0`);`WSTRB` 未实现(只支持整字写,已知限制) |
| **本轮未测** | 未实现地址的读行为、WSTRB 行为、复位后时序、背靠背连续访问 |

⚠️ **本次不涉及任何物理仪器** —— 读数是 `MMIO.read()` 的返回值,**量具就是 AXI 总线本身**。
`report/measurements/README.md` 里"仪器 / 量程"那一栏本轮记 **N/A**(无万用表、无示波器参与)。

## 二、所烧位流版本(**排除"烧的是旧程序"**)

| 项 | 值 |
|---|---|
| 文件 | `qrs.runs/impl_1/system_wrapper.bit` |
| 大小 | **4 045 690 B** |
| **md5** | **`caa12ac5130ae45cdb0e21c7a0af1500`** |
| 生成时间 | **2026-09-29 23:00:09** |
| 同批导出 `.xsa` | `qrs/system_wrapper.xsa`,345 061 B,md5 `da8c04a7e91b73d94301b10d0ead82ea`,23:00:42 |

> ⚠️ **md5 是本次新记的,不是烧录当时当场算的** —— 板子上那份 `.bit` 与本地这份
> **没有做过逐字节比对**。`scp` 之后文件未再改动(本地时间戳停在 23:00:09,早于上板操作),
> 但**严格说这只能证明"本地这份没变"**,不能证明"板上那份与它相同"。
> 见 `README.md` 那条:「时间戳法只能排除忘了重跑综合」。

## 三、烧录时间与板型

| 项 | 值 |
|---|---|
| 烧录时间 | 2026-09-29 **23:00 之后**(用户执行,无精确时刻记录) |
| 板型 | **正点原子领航者 ZYNQ-7020**(xc7z020clg400-2) |
| PYNQ | **2.7.0** |
| 加载方式 | `Overlay('/home/xilinx/system_wrapper.bit')`,**只加载 `.bit`,不跑 `ps7_init`** |
| 上位机 | `scp` 经直连网线(`192.168.2.100` ↔ `192.168.2.99`),板端 `sudo -E …/pynq-venv/bin/python3` |

⚠️ **`.hwh` 必须与 `.bit` 同基名** —— 拷贝时用 `scp …/system.hwh 板:~/system_wrapper.hwh`
**在拷贝的同时改名**(`.xsa` 里那份叫 `system.hwh`,会不匹配)。

## 四、预期值 vs 实测值(M04 要的比对数据)

| # | 操作 | 预期 | **实测** | 判定 |
|---|---|---|---|---|
| 1 | `read(0x48)` VERSION | `0x0001_0000`(`VERSION_CONST`) | **`0x10000`** | ✅ |
| 2 | `read(0x4C)` SCRATCH(上电初值) | `0x0000_0000` | **`0x0`** | ✅ |
| 3 | `write(0x4C, 0xDEADBEEF)` → `read(0x4C)` | `0xDEADBEEF` | **`0xdeadbeef`** | ✅ |
| 4 | `write(0x4C, 0xA5A55A5A)` → `read(0x4C)` | `0xA5A55A5A` | **`0xa5a55a5a`** | ✅ |

### 🔑 为什么第 4 笔是必须的

**只写一笔测不出"恒返回常数"的假通过。** 如果 `rdata` 被硬接成固定值、或读通路压根没连,
一笔写就回读不出区别;而**换一个值再回读**,要求寄存器真的**存住了最后一笔写**。
第 3→4 笔构成 A/B,把"读到的是上一次写入值"这件事钉死。

## 五、`ol.ip_dict` 实测(PYNQ 真解析器读自建 BD 的 `.hwh`)

```python
'cv_axi_lite_slave_0': {
    'fullpath': 'cv_axi_lite_slave_0',
    'type': 'xilinx.com:module_ref:cv_axi_lite_slave:1.0',
    'bdtype': None,
    'addr_range': 4096,
    'phys_addr': 1136656384,          # = 0x43C00000
    'mem_id': 'S_AXI',
    'memtype': 'REGISTER',
    'interrupts': {},
    'parameters': {
        'C_S_AXI_ADDR_WIDTH': '7',
        'C_S_AXI_DATA_WIDTH': '32',
        'VERSION_CONST': '0x00010000',
        'Component_Name': 'system_cv_axi_lite_slave_0_0',
        'C_BASEADDR': '0x43C00000',
        'C_HIGHADDR': '0x43C00FFF',
    },
    'registers': {},                  # 空 —— 见下
    'driver': <class 'pynq.overlay.DefaultIP'>,
}
```

三条**事前预测、事后实测确认**的点:

| 预测(依据) | 实测 | |
|---|---|---|
| 名字是**裸实例名**,不带 `/S_AXI` 后缀<br>(`.hwh` 里 `BDTYPE` 只出现在嵌套 BD 上) | `'fullpath': 'cv_axi_lite_slave_0'` | ✅ |
| `ADDR_WIDTH` 由 `X_INTERFACE_PARAMETER` 决定,值为 **7** | `'C_S_AXI_ADDR_WIDTH': '7'` | ✅ |
| `registers` 为空字典<br>(`.hwh` 里该 ADDRESSBLOCK 的 `ACCESS=""`,不像 `axi_gpio` 那样是 `read-write`) | `'registers': {}` | ✅ |

> **`registers: {}` 的影响**:对**裸 `MMIO.read/write` 无影响**(地址走 `MEMRANGE`,那条是对的)。
> 只有在依赖 PYNQ 自动生成寄存器驱动的场景下才有差别 —— 本项目不用那条路。
> `driver = DefaultIP` 是**正确**的(它不是任何 IP 驱动的目标)。

## 六、原始终端输出(用户执行,原文照录)

```
xilinx@pynq:~$ sudo -E /usr/local/share/pynq-venv/bin/python3 -c "from pynq import Overlay; …
[sudo] password for xilinx:
{'axi_gpio_0': {...}, 'cv_axi_lite_slave_0': {...}, 'processing_system7_0': {...}}
VER  0x10000
S0   0x0
W1   0xdeadbeef
W2   0xa5a55a5a
xilinx@pynq:~$
```

（`ip_dict` 全文因过长在此折叠,关键字段见 §五;原始整段在当日会话记录里。）

## 七、同批次工程侧核实(非板上证据,供互相印证)

| 项 | 值 | 来源 |
|---|---|---|
| 综合/实现 | WNS **+13.943 ns** / TNS 0.000 / 失败端点 **0** / `All user specified timing constraints are met.` | `impl_1/system_wrapper_timing_summary_routed.rpt` |
| DRC | **0 error** | `impl_1/system_wrapper_drc_routed.rpt` |
| 资源 | LUT **684**(1.29%)、FF **878**(0.83%)、BRAM 0、DSP 0 | `impl_1/system_wrapper_utilization_placed.rpt` |
| 前一版对比 | 上一版(仅 `axi_gpio`)LUT 509 / FF 579 ⇒ **+175 / +299** | 同上,两版口径一致(**均不含 ILA**) |
| `.hwh` | 154 543 B;`MEMRANGE … BASEVALUE="0x43C00000" INSTANCE="cv_axi_lite_slave_0"` | `qrs.gen/…/hw_handoff/system.hwh` |
| BD 连线 | `axi_smc/M01_AXI → cv_axi_lite_slave_0/S_AXI`;`s_axi_aclk ← FCLK_CLK0`;`s_axi_aresetn ← peripheral_aresetn` | `srcs/sources_1/bd/system/system.bd` |

## 八、本轮**没有**证明的事(不得越界引用)

- ❌ **未证明板上 FCLK0 的实际频率是 50 MHz。** `Overlay()` 不跑 `ps7_init`,
  FCLK0 由板子启动镜像的 `BOOT.BIN` 决定。全部文档现按 50 MHz 记,依据是 **BD 配置**,
  **不是频率实测**。一行可证:`from pynq import Clocks; Clocks.fclk0_mhz` —— **尚未跑**。
- ❌ **未做功能仿真。** `cv_axi_lite_slave.v` 只过了 `xvlog`(rc=0)与 `xelab -debug typical`(rc=0),
  **零激励、零波形**。本轮上板跑通的是**两条通路**(VERSION 读 / SCRATCH 读写),
  **不是全部 20 个地址的行为**,更不是 AXI 协议合规。
- ❌ **未测**未实现地址的读、`WSTRB`、复位行为、背靠背访问、AW/W 乱序到达。
- ❌ **未测**并发/压力 —— 全程只有 4 次单笔访问。

## 九、结论

🟢 **P2 判据「`SCRATCH` 读写回环通过」—— 达成**(2026-09-29)。

P2 三项分解:

| 分解项 | 状态 |
|---|---|
| 板子跑起 PYNQ | ✅ 2026-09-25 |
| PS↔PL 通路闭环 | ✅ 2026-09-29(`axi_gpio`,H15 LED 点亮) |
| **`SCRATCH` 读写回环** | ✅ **2026-09-29(本文件)** |

⚠️ **口径纪律**:上一轮的 `axi_gpio` 回环**不得**与本次混称;`axi_gpio` 那次是**厂商 IP**,
本次才是**自写 RTL**。报告引用时按本文件写。
