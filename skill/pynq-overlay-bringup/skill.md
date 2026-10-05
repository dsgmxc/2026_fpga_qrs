# pynq-overlay-bringup

> **通用 PYNQ 技能**(面向 PYNQ 框架,**不与任何具体题目绑定** —— 换题目、换板卡仍可用)
> 适用对象:任何「Vivado 出位流 → PYNQ 加载 Overlay → PS 访问 PL 寄存器」的流程。

## 适用范围

| 项 | 值 |
|---|---|
| 框架 | PYNQ 2.7.0(本项目实测版本) |
| 板卡 | 任意 Zynq-7000 / Zynq UltraScale+ 且已装 PYNQ 镜像 |
| 工具 | Vivado 2025.2 / 2020.2(**两个版本导出的 `.hwh` 实测互相兼容**,见 §已验证的效果) |
| 场景 | 第一次把自建 BD 的位流加载进 PYNQ;或者加载「看起来成功了但访问不到」 |

**不适用**:裸机(Xilinx SDK/Vitis)、PetaLinux 用户态、以及不用 PYNQ 的 `/dev/mem` 直操场景。

---

## 使用方法

### 0. 前置:确认没有网线也能干

启动本身**不依赖以太网**;网线只服务于 Jupyter 浏览器访问。
**串口控制台 + SD 卡 boot 分区(FAT)做离线文件中转**,即可完成全部开发。

### 0.1 🔴 先确认 PL 里有没有位流 —— 读未配置的 AXI 会**挂死整机**

**PL 上电时很可能是空的。** 本项目实测:板子的 `BOOT.BIN` **不含位流**,
所以从加电到第一次 `Overlay()` 之间,PL 里一个逻辑都没有。

此时**读它的 AXI 窗口不会"读不到",而是把 CPU 卡死**:

```
PL 未配置 ⇒ GP0 读 0x43C00000 时 AXI 事务永远等不到从机应答
          ⇒ CPU 卡在总线上 ⇒ 整机挂死,**只能断电**   (GP 口没有超时机制)
```

**症状很有辨识度**:`mmap` 成功、程序打了第一行日志、然后**静默**,
`ping` 100% 丢包,SSH 断。**不是"寄存器读出来是 0",是主机停摆。**

**判据(必须先判,再动手)**:

| 检查 | 命令 | 期望 |
|---|---|---|
| 启动镜像含不含位流 | `grep -c $'\xaa\x99\x55\x66' /boot/BOOT.BIN` | `0` ⇒ **不含,PL 上电为空** |
| 内核加载过吗 | `dmesg \| grep -i fpga` | 只有 `registered`,**无加载记录** |
| **驱动记账** | `cat /sys/class/fpga_manager/fpga0/state` | `unknown` ⇒ **没加载**(加载后变 `operating`) |
| **DONE 位** | `devcfg INT_STS`(`0xF8007000`+`0x0C`)bit2 | `0` ⇒ **未配置** |

> ⚠️ **判 DONE 只能看 `INT_STS` bit2** —— 见 §失效条件里的那条更正,
> **`STATUS`(0x14)里没有 PCFG_DONE 这一位**。

**由此得到两条硬规则:**

1. 🔴 **一次上电只加载一次位流。** 若 PL 里已有活跃的 AXI 主机(比如帧缓存读控制器),
   再做第二次 `Overlay()` / fabric 复位会**锁死 HP 口连带 CPU**。要重载就断电重来。
2. 🔴 **寄存器访问必须排在唯一那次 `Overlay()` 之后。** 在此之前一切对
   PL 地址窗口的读写(含 `mmap` 后第一次读)**都是挂机操作**。

### 1. 串口控制台

```bash
# 板载 CH340(USB ID 1a86:7523)。若 /dev/ttyUSB0 迟迟不出现:
lsusb | grep 1a86          # 设备在,但可能没有驱动绑定
# 常见元凶:brltty 盲文服务,udev 规则直接抢走 CH340
grep -n '1a86/7523' /usr/lib/udev/rules.d/85-brltty.rules
sudo apt remove -s brltty  # 先模拟,确认无依赖级联
sudo apt remove brltty
sudo udevadm control --reload-rules && sudo udevadm trigger
# ⚠️ 事后绝不要跑 apt autoremove —— brltty 挂在 ubuntu-desktop-minimal 的 Recommends 链上
```

**串口命令纪律:一行、自闭合、不依赖续行。**
串口终端(minicom/tio)在输入未闭合时会进入续行模式,提示符变 `>` ——
而**提示符后面的前导空格会被一起送进 Python**,粘贴多行 `python3 -c "…"` 必得
`IndentationError: unexpected indent`。这不是板子的问题。

### 2. 拷文件:`bit` 与 `hwh` 必须同基名

```bash
# Vivado 产出的名字常常不同:system_wrapper.bit + system.hwh
# PYNQ 按基名配对 ⇒ 拷贝时顺手改名
scp system_wrapper.bit  xilinx@<board>:~/qrs.bit
scp system.hwh          xilinx@<board>:~/qrs.hwh     # ← 同一基名
```

`.xsa` 里的那份 hwh 叫 `system.hwh`(不是 `system_wrapper.hwh`),直接用它**不会匹配**。

### 3. 加载:必须 `sudo -E`,且用 venv 全路径 python

```bash
sudo -E /usr/local/share/pynq-venv/bin/python3 -c "from pynq import Overlay; ol=Overlay('/home/xilinx/qrs.bit'); print(ol.ip_dict)"
```

三条都是必需的:

| 项 | 不加会怎样 |
|---|---|
| `sudo` | `OSError: Root permissions required.`(要写 `/dev/mem`) |
| `-E` | `RuntimeError: No Devices Found`(**sudo 默认清环境变量,PYNQ 的设备探测依赖它们**) |
| venv 全路径 | 非交互环境 cwd 不是 `/home/xilinx` 时 import 不到 pynq |

**`No Devices Found` 的根因是 `XILINX_XRT` 没被导出**(定义在 `/etc/profile.d/xrt_setup.sh`)。
`-E` 只是「保留当前环境」这个手段;**如果调用方本身就没有这个变量,`-E` 也救不了你** ——
`sudo -S`(从管道读密码)**不加载 `profile.d`**,此时必须显式补一行:

```bash
export XILINX_XRT=/usr        # ← 缺它必报 No Devices Found,与 -E 无关
sudo -E /usr/local/share/pynq-venv/bin/python3 -c "from pynq import Overlay; …"
```

⇒ **见到 `No Devices Found` 先查 `echo $XILINX_XRT`,不要只顾着加 `-E`。**

### 4. 自检:三条不能省

```python
ol.ip_dict                                   # ① IP 在不在、地址对不对
from pynq import Clocks; print(Clocks.fclk0_mhz)   # ② PL 实际跑多快
# ③ 读写回环:必须写两笔不同的值
ol.<name>.read(0x4C); ol.<name>.write(0x4C, 0xDEADBEEF); ol.<name>.read(0x4C)
ol.<name>.write(0x4C, 0xA5A55A5A); ol.<name>.read(0x4C)   # ← 换值第二笔
```

③ 的**第二笔是必须的**:只写一笔测不出"读通路恒返回常数"的假通过。

### 5. 🔴 时钟设定必须在 `Overlay()` 之后

```python
ol = Overlay('qrs.bit')     # ← 这一步会把 FPGA0_CLK_CTRL 整个复位回 BOOT.BIN 的值
from pynq import Clocks
Clocks.fclk0_mhz = 50       # ← 必须在这之后
```

**`Overlay()` 只加载 `.bit`,不跑 `ps7_init`。**
BD 里写的 FCLK 配置在运行时被完全忽略,实际值由板子启动镜像的 `BOOT.BIN` 决定。
本项目 BD 写 50 MHz、开机实测 **27.78 MHz**,设定后才是 50.000 MHz。

⚠️ **`Clocks.fclk0_mhz` 的读回值不等于真值** —— 它按**假定**的 IO PLL 频率做除法。
要拿真值必须用独立时基(见 `skill/measurement-instrument-can-be-wrong/`)。

### 5.1 ⚠️ 手写 SLCR 改频时,`DIV0` 与 `DIV1` **必须同写**

不用 `Clocks` 而直接写 `SLCR.FPGA0_CLK_CTRL`(`0xF8000170`)时:

```
FCLK0 = IO_PLL / (DIV0 ?: 1) / (DIV1 ?: 1)        # 字段值为 0 表示旁路(÷1)
         DIV0 = bits[13:8]      DIV1 = bits[25:20]
```

**只改 `DIV0` 是个静默的错** —— 本项目实测:想设 50 MHz 只写了 `DIV0=20`,
而 `DIV1` 仍留着启动镜像的 6 ⇒ 实得 `1000/20/6 = 8.33 MHz`,**寄存器读回、代码、日志全都正常,
只有屏幕不对**。要靠**独立时基**才抓得到(数场频得 9.657 Hz,而应得 58.6)。

```python
# 正确写法:两个字段一次写全
wr32(0xF8000170, (old & ~((0x3F << 8) | (0x3F << 20))) | (20 << 8) | (1 << 20))
#                 ⇒ 0x00101400  (IO PLL 1000 / DIV0 20 / DIV1 1 = 50.000 MHz)
```

📌 **`Clocks.fclk0_mhz = 50` 之所以没问题,是因为 PYNQ 内部顺手把 `DIV1` 设成了 1** ——
**照抄它的"结果"而不是调用它,就会漏。**

---

## 失效条件

- ⛔ **`sudo`(不带 `-E`)** —— 必报 `No Devices Found`。
- ⛔ **`.bit` 与 `.hwh` 不同基名** —— PYNQ 找不到 hwh,`ip_dict` 为空或直接报错。
- ⛔ **把 FCLK 设定写在 `Overlay()` 之前** —— 会被 `Overlay()` 复位掉,静默无效。
- ⛔ **多行 heredoc / 多行 `python3 -c` 粘进串口** —— 续行提示符注入缩进。
- ⛔ **把 `Clocks.fclk0_mhz` 的返回值当实测频率引用**。
- ✅ **`/sys/class/fpga_manager/fpga0/state` 是可用的判据**(2026-10-05 更正)——

  | 时刻 | `state` | `dmesg \| grep -i fpga` |
  |---|---|---|
  | 开机、未加载 | `unknown` | 只有 `registered` / `probed` |
  | **`Overlay()` 之后** | **`operating`** | 多一行 **`writing qrs.bin to Xilinx Zynq FPGA Manager`** |

  本项目实测两条同时翻转,且 dmesg 那行的时间戳正好落在 `Overlay()` 调用的那一刻
  ⇒ **PYNQ 的 `Overlay()` 走的正是 fpga_manager 框架**,这个 `state` 是**驱动自己的记账**,
  比读 devcfg 更直接。
  > 🔴 **更正说明**:本条此前写「加载前后都读 `unknown`」,是**错的** ——
  > 当时没有在「加载后」真的量一次就下了结论。**别把"我没测出来"写成"它不可用"。**
- 🔴 **`devcfg STATUS`(0x14)里没有 `PCFG_DONE` 这一位** —— 别凭印象套用。
  权威位定义(**出处**:`xdevcfg_hw.h`,devcfg_v3_9 driver):

  | 寄存器 | 偏移 | 位 | 宏 |
  |---|---|---|---|
  | `STATUS` | `0x14` | bit4 | `XDCFG_STATUS_PCFG_INIT_MASK = 0x10`(INIT_B 拉高,**只说明 PL 就绪**) |
  | `STATUS` | `0x14` | **bit5** | **无定义(保留位)** ← 读到 1 **毫无含义** |
  | `INT_STS` | `0x0C` | **bit2** | `XDCFG_IXR_PCFG_DONE_MASK = 0x04` ← **DONE 在这里** |

  📌 这条是**踩坑换来的**:模型曾据 `STATUS` bit5 = 1 断言「PL 已配置」,
  并由此推出「PL 里残留上一轮位流」—— **整条推理链不成立**。
  **判据类结论先回本机安装里查头文件,不要凭记忆。**
- ⛔ **手写 `SLCR.FPGA0_CLK_CTRL` 时只写 `DIV0`** —— **静默错频**,见 §5.1。
- ⚠️ **root 下 `import pynq` 可能崩在 `pkg_resources`** ——
  本机实测 `UnicodeDecodeError`,源是某 `dist-packages/*.egg-info/PKG-INFO` 含非 UTF-8 字节。
  走服务那套环境(`VIRTUAL_ENV` / `PATH` 齐全)可避开;或不 import pynq、直接 `/dev/mem` 操作。
- ⚠️ **Pynq 镜像里只有 `xilinx` 用户的密码**,root 密码没有官方记载 —— 别信社区传言。

## 已验证的效果

| 项 | 结果 | 证据 |
|---|---|---|
| PYNQ 2.7.0 在领航者 ZYNQ-7020 启动 | ✅ | 板上实跑 `pynq.__version__` → `2.7.0` |
| **无网线**加载 Overlay | ✅ | 串口控制台全程;官方指南 §2.3 六项启动先决条件中**没有网线** |
| `sudo` vs `sudo -E` 对照 | ✅ bare `sudo` → `No Devices Found`;`sudo -E` → 成功,**根因确证** | `LLM-022` |
| 🔴 **`XILINX_XRT` 是 `No Devices Found` 的真根因** | ✅ `sudo -S` 下显式 `export XILINX_XRT=/usr` 后一次通过 | `LLM-042` |
| 🔴 **PL 上电为空 + 读其 AXI = 挂死 CPU** | ✅ **3 次断电坐实**;四条独立证据(`BOOT.BIN` 无同步字 / `dmesg` 无加载 / `state=unknown` / `INT_STS` DONE=0) | `LLM-042`、`report/measurements/20261005_dac_dds_ui_endtoend.md` §二 |
| 🔴 **`DIV1` 漏写导致静默错频** | ✅ 只写 `DIV0=20` 得 8.33 MHz(应 50);场频实测 9.657 Hz 才抓到;同写 ⇒ `0x00101400` = 50.0000 MHz | `LLM-042`、同上 §3.2 |
| **Vivado 2025.2 的 `.hwh` ← PYNQ 2.7 解析** | ✅ **完全兼容**,逐字段一致 | `report/measurements/20260925_hwh_compat.md` |
| 自建 BD 的真 `.hwh` 被板上真解析器吃下 | ✅ `ip_dict` 地址、名字、`ADDR_WIDTH` 三项事前预测事后确认 | `report/measurements/20260929_scratch_loopback.md` |
| `bit`/`hwh` 改名 + `sudo -E` 全流程 | ✅ 板上 `VER 0x10000` / `S0 0x0` / `W1 0xdeadbeef` / `W2 0xa5a55a5a` | 同上 |
| FCLK0 开机默认值 | 🔴 **27.78 MHz**(低于面板窗口下限 40.8 MHz);`Clocks.fclk0_mhz = 50` 后 **50.000 MHz** | `report/measurements/20261003_fclk0_real_measured.md` |
| 手写 SLCR 改频 + **独立时基复测** | ✅ `FPGA0_CLK_CTRL = 0x00101400` ⇒ 50.0000 MHz;帧率法复测 50.0170 MHz(**差 0.03%**) | `LLM-042` |
| ✅ **`fpga_manager/state` 可作加载判据**(旧结论"github 加载前后都 unknown") | 🔴 **推翻并更正** —— 开机 `unknown` → `Overlay()` 后 **`operating`**;dmesg 同步多出 `writing qrs.bin …`,时间戳吻合 | `LLM-042`(收尾复查) |

**兼容性验证的手法本身也值得复用**:不要停在"把两份 hwh 用肉眼/脚本比字段"(间接证据),
而是**把板上真正在跑的那个解析器取出来,喂真文件给它跑**:

```bash
debugfs -R 'cat /home/xilinx/pynq/pl_server/hwh_parser.py' ZYNQ-7020-2.7.0.img
# 免 root、免挂载,直接从 ext4 镜像里取出解析器(md5 841ea7686cc3d1fee8c61281db466f5c)
```

对照样本用**厂商同形示例**(内容几乎一致、只是工具版本不同) ⇒ **同内容异版本,最干净的 A/B**。

## 来源

1. **`LLM-022`(2026-09-25)** —— 无网线跑通 PYNQ。模型给了一条**粘不进串口终端的多行命令**,
   用户粘贴后得到 `IndentationError`。⇒ 「面向串口的命令必须单行自闭合」。
   同轮:`sudo` → `sudo -E` 的对照实验,以及**面对"root 密码是多少"没有编答案**。
2. **`LLM-023`(2026-09-25)** —— hwh 版本兼容性验证。探针 2 的 `ip_dict` **是空的**,
   在"验证兼容性"的上下文里很容易被读成"不支持"(那就得出完全相反的结论)。
   模型回头查空的来源,发现**是探针设计的问题不是版本问题**,并补了负对照(探针 3)才下结论。
3. **`LLM-027` / `20260929_scratch_loopback.md`(2026-09-29)** —— 真件上板。
   `.bit`/`.hwh` 改名、`sudo -E`、换值第二笔,三个动作都在这里定型。
4. **`LLM-034` / `20261003_fclk0_real_measured.md`(2026-10-03)** —— `Overlay()` 不跑 `ps7_init`、
   改频必须在 `Overlay()` 之后,这两条是**实测复现**出来的,不是从文档读来的。
5. **`LLM-042` / `20261005_dac_dds_ui_endtoend.md`(2026-10-05)** —— 本技能包扩出 §0.1 与 §5.1。
   ① **三次挂机**换来「PL 上电为空 + 读 AXI 挂 CPU」这条硬规则(与 §失效条件里的两条)。
   ② 🔴 **模型自己编了一个寄存器位** —— 凭印象把 `STATUS` bit5 当 `PCFG_DONE`,
      据此断言「PL 已配置」「残留上一轮位流」,还**反过来质疑用户没真正断电**。
      回查 `xdevcfg_hw.h` 才发现 `STATUS` 根本没这一位。**推论链整条作废,质问撤回。**
      ⇒ 这条列入 **§失效条件**与**来源**,因为它比"记不清"更危险:它会导致**指向用户的错误结论**。
   ③ **`DIV1` 漏写** —— 自认设了 50 MHz,靠数场频 9.657 Hz(应 58.6)才发现;
      ⇒ 再次印证「**写进寄存器 ≠ 生效**,必须独立复测」,并入 §5.1。
   ④ **两个环境坑**被证明会伪装成硬件故障:`XILINX_XRT` 未导出、root 下 `pkg_resources` 非 UTF-8 崩溃。
   ⑤ ✅ **收尾时又推翻了自己的一条旧结论** —— 本技能包原先写「`fpga_manager/state`
      加载前后都读 `unknown`,不可作判据」。收尾做只读复查时实测:开机 `unknown` /
      `Overlay()` 之后 **`operating`**,且 `dmesg` 同时多出一行
      `writing qrs.bin to Xilinx Zynq FPGA Manager`(时间戳正好是 `Overlay()` 那一刻)。
      ⇒ **它是可用判据**,已改写入 §0.1 与 §失效条件。
      📌 **教训:旧记录里"我测过、不行"的结论,也要能追到那一次测量** ——
      追不到就很可能是**没测就下了结论**(本例正是)。已并入 §失效条件的更正说明。

> ⚠️ **本技能包的撰写纪律**:所有"已验证的效果"必须能追到一次真实的上板 / 仿真 / 报告。
> 追不到的写成"未验证",不写成"已验证"。
> **反面结论(「某判据不可用」)同样要追到出处** —— 见 §失效条件里 `INT_STS` 那条。
