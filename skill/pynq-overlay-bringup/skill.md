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

---

## 失效条件

- ⛔ **`sudo`(不带 `-E`)** —— 必报 `No Devices Found`。
- ⛔ **`.bit` 与 `.hwh` 不同基名** —— PYNQ 找不到 hwh,`ip_dict` 为空或直接报错。
- ⛔ **把 FCLK 设定写在 `Overlay()` 之前** —— 会被 `Overlay()` 复位掉,静默无效。
- ⛔ **多行 heredoc / 多行 `python3 -c` 粘进串口** —— 续行提示符注入缩进。
- ⛔ **把 `Clocks.fclk0_mhz` 的返回值当实测频率引用**。
- ⚠️ **`/sys/class/fpga_manager/fpga0/state` 不可作为"位流已进 PL"的证据** ——
  本项目实测**加载前后都读 `unknown`**,原因未查明。别拿它当判据。
- ⚠️ **Pynq 镜像里只有 `xilinx` 用户的密码**,root 密码没有官方记载 —— 别信社区传言。

## 已验证的效果

| 项 | 结果 | 证据 |
|---|---|---|
| PYNQ 2.7.0 在领航者 ZYNQ-7020 启动 | ✅ | 板上实跑 `pynq.__version__` → `2.7.0` |
| **无网线**加载 Overlay | ✅ | 串口控制台全程;官方指南 §2.3 六项启动先决条件中**没有网线** |
| `sudo` vs `sudo -E` 对照 | ✅ bare `sudo` → `No Devices Found`;`sudo -E` → 成功,**根因确证** | `LLM-022` |
| **Vivado 2025.2 的 `.hwh` ← PYNQ 2.7 解析** | ✅ **完全兼容**,逐字段一致 | `report/measurements/20260925_hwh_compat.md` |
| 自建 BD 的真 `.hwh` 被板上真解析器吃下 | ✅ `ip_dict` 地址、名字、`ADDR_WIDTH` 三项事前预测事后确认 | `report/measurements/20260929_scratch_loopback.md` |
| `bit`/`hwh` 改名 + `sudo -E` 全流程 | ✅ 板上 `VER 0x10000` / `S0 0x0` / `W1 0xdeadbeef` / `W2 0xa5a55a5a` | 同上 |
| FCLK0 开机默认值 | 🔴 **27.78 MHz**(低于面板窗口下限 40.8 MHz);`Clocks.fclk0_mhz = 50` 后 **50.000 MHz** | `report/measurements/20261003_fclk0_real_measured.md` |

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

> ⚠️ **本技能包的撰写纪律**:所有"已验证的效果"必须能追到一次真实的上板 / 仿真 / 报告。
> 追不到的写成"未验证",不写成"已验证"。
