# src/constrs —— 引脚约束

| 文件 | 工程内状态 | 约束的端口 | 内容是否当前 |
|---|---|---|---|
| **`led.xdc`** | ✅ **启用**(`UsedIn = synthesis + implementation`) | **56 个**:`lcd_rgb[23:0]` + `lcd_hs/vs/de/bl/clk/rst` + `touch_scl/sda/int/rst_n` + `dac_sclk/cs/mosi` + `dds_sclk/sync/data` + **`adc_clk` / `adc_oe_n` / `adc_pdn` / `adc_otr` / `adc_d[11:0]`(16 个,2026-10-06 加)** | ✅ **当前有效** |

> ⚠️ **文件名会骗人。** `led.xdc` 这个名字会让人以为它只管那颗 LED。**实际上它是全工程唯一的
> 引脚约束文件** —— 30 根 LCD + 4 根触摸 + 6 根 DAC/DDS + **16 根 AD9238** 的球号**都放在它里面**。
> 📌 **改引脚之前,先确认改的是哪个文件。**

球号出处:

| 端口组 | 出处 | 可信度 |
|---|---|---|
| `lcd_*` 30 根 + `touch_*` 4 根 | 厂商例程 `28_top_lcd_touch` 的 `top_lcd_touch.xdc`(#LCD / #LCD TOUCH 段) | 与板上已跑通的 `lcd_*` **同源同文件** |
| `dac_sclk=W15` / `dac_cs=W13` / `dac_mosi=Y14` | **用户确认的实际接线**(2026-10-05) | 实际接线 |
| `dds_sclk=T5` / `dds_sync=U7` / `dds_data=T11` | 2026-10-05 新增,取自 `docs/board_pins.md` 的空闲脚 | ⚠️ **照空闲脚挑的,未经接线核对** |
| `adc_*` 16 根 | **用户在板上实际接线后报出的名单**(2026-10-06) | 实际接线;⚠️ **D0/D11 的顺序没有第二条证据**(见下) |

> ⚠️ **AD9238 那 16 根里,`adc_d[0]`(LSB)与 `adc_d[11]`(MSB)的顺序只有一处出处** ——
> 用户报出的名单,按 `D0..D11` 顺序对应 `W11 Y11 Y12 Y13 V10 V11 W9 W10 Y8 Y9 Y6 Y7`。
> 若实际接反,**症状是采样值位序整体倒置**(正弦会变成锯齿状的怪波形),一眼能看出来。
> 这条是**上板第一件要确认的事**,已写进 `led.xdc` 正文。
> 📌 用户第一次报的名单里 `y11` 与 `w6` 各出现过两次(与模块排针只有 16 个信号对不上),
> 核对后换成了现在这版无重复的 —— **"接线的名单"和"引脚的名单"一样需要跟实物对一遍**。

触摸 4 根**未加 `PULLUP` 属性** —— 厂商原设计也没加且能跑通,故推断排线上已有外部上拉;
本工程同样**故意不加**(加了会把手上的"外部上拉缺失"掩盖成"时好时坏")。

---

## ⚠️ 本文件没有 `create_clock`(但 2026-10-06 起有一条 CDC 约束)

实测 `grep -c create_clock led.xdc` → **1**,但那**唯一一处是注释**(说明为什么不能再写
`create_clock`),**实际约束语句是 0 条**。

⇒ 当前设计**没有显式的时钟周期约束**。时序报告里的频率来自 Vivado 对
`processing_system7_0` 的 `FCLK_CLK0` 的**自动推导**,不是我们写进去的。
写 M03「时钟频率」一节时必须如实标注这一点,不得说成"我们约束了 50 MHz"。

🆕 **但 2026-10-06 加了本工程第一条实质时序约束**(随 AD9238 采集通路):

```tcl
set_clock_groups -asynchronous -group {clk_fpga_0 clk_fb_raw} -group {clk_adc_raw}
```

时钟名**不是猜的**,是 `report_clocks` 读出来的:`clk_fpga_0` = 20.000 ns(PS FCLK0 =
50 MHz)、`clk_fb_raw` = 20.000 ns(MMCM `CLKFBOUT`)、`clk_adc_raw` = 7.692 ns(MMCM
`CLKOUT0` = 1300/10 = **130 MHz**)。

**为什么可以声明异步(要紧处)**:两域同源,但域间**每一条**通路都只有两类 —— **同步器**
(第一级按设计就是要踩亚稳态,拿普通建立/保持去卡它**一定报违规**)与**双口 BRAM**
(存储器内部路径不做用户时序)。声明异步 = 明确告诉工具这两组之间不按周期对齐来卡,
MTBF 交给 `adc_capture.v` 里的 `ASYNC_REG` + 布局保证。

> ⚠️ **FCLK0 若停在 27.78 MHz(板上 `BOOT.BIN` 的默认),MMCM 锁不住 ⇒ `clk_adc_raw`
> 这个时钟对象根本不会生成 ⇒ 上面那条约束会报 `clock not found`。**
> 那是**预期行为**(说明 `0x84[0]` 还没放行),不是约束写错了 —— 排障时别在这上面绕。

⚠️ **`adc_d[*]` / `adc_otr` 的输入时序本版有意不约束**,原因如实记在 `led.xdc` 正文:
采样时钟周期由运行时寄存器 `P` 决定(`P=13` → 100 ns、`P=2` → 15.4 ns),**一个静态 XDC
写不下所有档位**;且当前数据线是杜邦线,`tPD` 离散度 ~4 ns 本身就盖过 65 MHz 的眼宽,
此时给精确 `set_input_delay` 只是**伪造精度**。
⇒ 实现报告里这几个端口会以 `no_input_delay` 出现在未约束清单中 —— **已知且有意**,不是漏了。

---

## 🔴 历史陷阱:约束写在**被禁用**的约束文件里 ⇒ 位流阶段才炸

> 这条坑对应的文件 `io.xdc` **已于 2026-10-05 删除**(内容已并入 `led.xdc`;
> 归档保留在**工作区** `docs/_retired/io.xdc.disabled-20261005.bak`,不入本仓库)。
> **保留下面的经过,是因为坑本身和具体文件无关** —— 只要工程里存在"不生效的约束文件",
> 同样的失败就会重演。

2026-10-02,4 条触摸约束先加进了 `io.xdc`(当时以为那是"引脚约束文件")。
综合、实现**全程零报错**,一路跑到 `write_bitstream` 才在 DRC 处失败:

```
ERROR: [DRC NSTD-1] Unspecified I/O Standard: 4 out of 44 logical ports use
       I/O standard(s) that are not specified. ...
ERROR: [DRC UCIO-1] Unconstrained Logical Port: 4 out of 44 logical ports have
       no user-assigned PACKAGE_PIN. ...
       Problem ports: touch_scl, touch_sda, touch_int, touch_rst_n
```

**为什么难发现**:被禁用的约束文件**不报错、不警告**,只是不参与。综合报告里那些端口
看起来"还没轮到你约束",而不是"你约束写丢了"。

```tcl
# 查出工程里到底哪些约束文件是启用的
report_property [get_files *.xdc]
```

📌 **推广**:任何"读一份配置、但那份配置可能没被启用"的场景都同型 ——
**先确认它真的被读了,再讨论里面写了什么。**
(同一族的问题另有 `qrs.xpr` 里一条 `<MsgRule>` 把 `CRITICAL WARNING` 整体降级成 `INFO`,
见 `llm-log/LLM-032.md`。)

---

## 与工程的关系(单向)

本目录是 **Vivado 工程 `qrs.srcs/constrs_1/new/` 的快照**。
**方向是单向的:工程 → 仓库。** 在工程里改了 `.xdc`,本目录**不会自动跟着变**,
需要手工重新拷贝。改动前请确认改的是**工程里那份**。
