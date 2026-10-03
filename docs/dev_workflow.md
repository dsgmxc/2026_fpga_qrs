# 开发流程 —— 从改代码到板上看到结果

> 本文是**操作手册**,不是设计文档。每一步都是可以照着敲的。
> 凡是**实测过**的写法,标 🟢;凡是从手册/指南抄的,标出处;**未验证的标 ⚠️**。
>
> 相关:上板实测存档见 `report/measurements/`,寄存器表见 `report/source_design_checklist.md §3`。

---

## 0. 两条通道,各管什么

| 通道 | 物理连接 | 用途 | 状态 |
|---|---|---|---|
| **串口控制台** | USB 线 → 板载 CH340 | **启动日志 + 登录 + 敲命令**。板子没网也能用 | 🟢 已验证 |
| **直连网线** | PC 网口 ↔ 板子网口 | **`scp` 传文件** + Jupyter 网页 | 🟢 已验证 |

> 两条都要接。**串口是命脉** —— 板子起不来时只有它能看到原因;网线只是让传文件方便。

---

## 1. 接线与上电

### 1.1 上电前必须确认

| 项 | 要求 | 怎么查 |
|---|---|---|
| **SD 卡** | 已插入,烧好 PYNQ 2.7.0 镜像 | 板上 SD 槽 |
| **启动跳帽 P5** | 拨到 **SD 卡启动** | 对照《领航者ZYNQ之PYNQ开发指南_V3.2》p22 |
| USB 线 | 板子 USB-UART 口 ↔ PC | — |
| 网线 | 板子网口 ↔ PC `enp2s0` | — |

### 1.2 上电顺序 🟢

```
① 先插 USB 串口线   ← 必须在通电【之前】
② 打开串口终端       ← 先开好,才能看到启动全过程
③ 最后给板子上电
```

**为什么必须先开串口再上电**:PYNQ 启动只有十几秒,中途报错(找不到 SD、内核 panic、
`BOOT.BIN` 不匹配)只在这段时间打印。先上电再开串口 = 前面全看不见。

> ⏸ **模拟前端的上电顺序尚未决策**(D21)。已知矛盾:AD9833 初始化完约 **200.8 ms** 就在加激励,
> 而 DAC1220 要 **~800 ms** 才可用 —— 中间这段时间偏置未定义。
> **当前做数字部分验证时不接模拟板**;要接模拟前端时先解决 D21,别照着本文上电顺序接。

### 1.3 串口登录 🟢

```bash
picocom -b 115200 /dev/ttyUSB0
```

| 参数 | 值 | 出处 |
|---|---|---|
| 波特率 | **115200** | 指南 p22 |
| 数据位/校验/停止位 | **8N1** | 指南 p22 |
| 流控 | **无** | 指南 p22 |
| 用户名 / 密码 | `xilinx` / `xilinx` | 指南 p22 |

**退出 picocom**:先按 `Ctrl-A`,**松开**,再按 `Ctrl-X`。
⚠️ 是两个连续的组合键,不是同时按、也不是只按 `Ctrl-X`。

> **前置检查**:如果 `/dev/ttyUSB0` 不存在,先确认 CH340 驱动是否被占用:
> ```bash
> lsusb                            # 期望看到 1a86:7523
> ls /sys/bus/usb/drivers/ch341/   # 期望该接口已绑定
> ```
> 若设备在但驱动未绑定,检查是否装了 **`brltty`**(其 udev 规则会抢走 CH340)。
> 卸载前先 `sudo apt remove -s brltty` 模拟一次,确认不级联卸载其他包;
> **卸载后不要运行 `apt autoremove`**(brltty 挂在 `ubuntu-desktop-minimal` 的 Recommends 链上)。

### 1.4 掉电与重启 🔴

**掉电前必须先在系统里关机** —— PYNQ 的根文件系统在 **SD 卡**上,直接断电有损坏风险。

```
① 串口控制台里执行:
       sudo shutdown -h now
② 等控制台停止输出(收尾会打印停机信息)
③ 确认板子已停机后,再切电源开关 / 拔电源
```

⚠️ **Zynq 不会自己断电。** `shutdown -h now` 只让 Linux 停机,**板子仍然通电** ——
**必须手动切开关**,否则等于没关。

**重启**:

```bash
sudo reboot
```

⚠️ 系统卡死(串口无响应)时用板上的**复位按键**。
复位键位置请对照板子实物,**本文未记录**。

> 📌 **每次上电前的检查项见 §1.1** —— 尤其是 SD 卡与启动跳帽 P5。

---

## 2. 网络通道(scp 传文件)

### 2.1 地址分配 🟢

| 端 | 接口 | 地址 | 谁来配 |
|---|---|---|---|
| PC | `enp2s0` | **192.168.2.100/24** | NM 连接 `pynq-direct`(已建好) |
| 板子 | `eth0:1` | **192.168.2.99/24** | 镜像自带静态,不用管 |

`pynq-direct` 的关键设置(已生效,列出来备查):

```
connection.interface-name : enp2s0
ipv4.method              : manual
ipv4.addresses           : 192.168.2.100/24
ipv4.never-default       : 是     ← 保住 WiFi 默认路由,别删这条
ipv6.method              : disabled
```

> ⚠️ **`ipv4.never-default yes` 不能少**。没有它,插上网线后默认路由会被抢到没有网关的
> 2.x 网段上,整机断网(症状:浏览器打不开,`ping 8.8.8.8` 不通)。

### 2.2 连通性检查 🟢

```bash
ping -c 2 192.168.2.99
```

**但 ping 通不等于链路对** —— 板子可能在自己 ping 自己。补两条坐实:

```bash
ip r get 192.168.2.99     # 期望:dev enp2s0(而不是 wlp4s0)
ip neigh | grep 192.168.2.99   # 期望 lladdr 00:0a:35:00:22:01,与板子 ifconfig 的 ether 一致
```

### 2.3 Jupyter(可选)

浏览器打开 `http://192.168.2.99:9090`,密码 `xilinx`。

> ⚠️ Jupyter 是**探索工具,不是交付物**。notebook 住在板子 `/home/xilinx/jupyter_notebooks/`,
> **不在 git 里**。调通的东西必须落成 `src/python/*.py` 才算数。

---

## 3. 程序上板运行

### 3.1 板子的 Python 在哪 🟢

```bash
sudo -E /usr/local/share/pynq-venv/bin/python3 <脚本>
```

| 要点 | 说明 |
|---|---|
| **必须 `sudo -E`** | bare `sudo` 会清掉环境变量 → `RuntimeError: No Devices Found`。`-E` 保留环境 |
| **必须用 venv 里的 python3** | 系统 python3 里没有 `pynq` |
| 板上 Python 版本 | **3.8.2** |

> ⚠️ **板上是 Python 3.8,不是 3.10+**。写代码时避开这些 3.9+ 才有的东西:
> `dict | dict` 合并运算符、`list[int]` 内置泛型注解、`match` 语句。
> 犯了也不致命——板上报 `SyntaxError`,改一行就好——但**别在电脑上测通了就以为板上能跑**。
> 电脑上的 Python 版本比板上新,**电脑过 ≠ 板上过**。

### 3.2 传文件 🟢

```bash
# PS 侧脚本
scp src/python/main.py  xilinx@192.168.2.99:/home/xilinx/

# 上板产物(两个文件必须一起传、必须同目录、必须主干同名)
scp board/qrs.bit board/qrs.hwh  xilinx@192.168.2.99:/home/xilinx/
```

> ⚠️ **`.bit` 与 `.hwh` 主干必须相同**。PYNQ 的 `Overlay()` 靠这个配对找硬件描述;
> 名字对不上会加载失败或拿不到 `ip_dict`。
> **仓库里这两个文件已经叫 `qrs.*` 了**(对齐 `src/python/config.py` 的
> `OVERLAY_BIT = "/home/xilinx/qrs.bit"`),照抄上面命令即可。
> ⚠️ Vivado 导出的原件叫 `system_wrapper.bit` / `system.hwh`,**主干不同,不能直接拷上去**。

### 3.3 跑

```bash
sudo -E /usr/local/share/pynq-venv/bin/python3 main.py
```

---

## 4. 完整改动循环

改一行 RTL 到屏幕上看到效果,走这一圈:

```
① 改 RTL / 改 Python          ← Vivado 工程 + src/python/
② Vivado:综合 → 实现 → 生成比特流
③ Vivado:导出硬件(system.hwh;若 BD 没变则与上次相同)
④ scp 到板子                  ← 见 §3.2
⑤ 串口上跑,肉眼/打印确认      ← 见 §3.3
⑥ 结果存档 → report/measurements/
```

### 判断"要不要重跑综合"

| 改了什么 | 要重综合吗 |
|---|---|
| RTL 内部逻辑(端口没变) | ✅ 要 —— 比特流变了,`.hwh` **不变** |
| 顶层端口增删 | ✅ 要 —— **且必须重导 `.hwh`**,BD 要重新生成 |
| 只改 PS 侧 Python | ❌ 不用 —— 直接 `scp` 脚本 |

> 🔑 **`.hwh` 变没变,看 md5 最快**:
> ```bash
> md5sum <工程>/qrs.gen/.../hw_handoff/system.hwh  board/qrs.hwh
> ```
> 一致 ⇒ BD 没动,只传 `.bit` 就行。

---

## 5. 当前地址与寄存器速查

| 项 | 值 |
|---|---|
| AXI 基址 | **`0x43C00000`**,窗口 4 KB |
| `0x48` VERSION | RO,期望 `0x0001_0000` |
| `0x4C` SCRATCH | RW,回环自检用 |
| `0x50` LCD_CTRL | RW,`[0]` 0=厂商彩条 / 1=PS 填充 |
| `0x54` LCD_FILL | RW,`[23:0]` RGB888 |

> 地址是**软硬件契约**。PS 侧读的地址与 BD Address Editor 里分配的不一致,
> 结果是**事务失败**(读到垃圾 / DECERR / 挂死),不是"访问到了别的外设"。

### ⚠️ FCLK0:设计值 50 MHz,实测 41.6667 MHz

**2026-10-01 板上实测**:

```python
from pynq import Clocks
print(Clocks.fclk0_mhz)      # → 41.666667
```

| 项 | 设计值(BD) | **实测值** |
|---|---|---|
| FCLK0 | `PCW_ACT_FPGA0_PERIPHERAL_FREQMHZ = 50.000000` | **41.666667 MHz** |

**根因**:`Overlay()` **只加载 `.bit`,不跑 `ps7_init`** ⇒ FCLK0 的实际值由**板子启动镜像的
`BOOT.BIN`** 决定,**BD 里的 PS 时钟配置在运行时被完全忽略**。

**改到 50 MHz**(已在板上验证有效,**但必须在 `Overlay()` 之前**):

```python
from pynq import Clocks
Clocks.fclk0_mhz = 50          # ← 放在 Overlay() 之前,让 PL 首拍就在 50 MHz
ol = Overlay("/home/xilinx/qrs.bit")
```

判据:改完屏仍正常显示(已测)。

> 🔑 **为什么必须显式设**:不设的话,时钟是"BOOT.BIN 恰好配了什么"的**继承值** ——
> 换一块板、换一版镜像就变了。显式设定后它是**本设计自己保证的常量**。
>
> 📌 **本条同时是一条复现纪律**:报告与文档中引用时钟时,
> **必须区分"设计值"与"实测值"**,不得写成"clk_axi = 50 MHz(实测)"。

---

*最后更新:2026-10-01*
