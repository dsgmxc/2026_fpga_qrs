# src/constrs —— 引脚约束

| 文件 | 工程内状态 | 约束的端口 | 内容是否当前 |
|---|---|---|---|
| **`led.xdc`** | ✅ **启用**(`UsedIn = synthesis + implementation`) | **40 个**:`lcd_rgb[23:0]` + `lcd_hs/vs/de/bl/clk/rst` + `touch_scl/sda/int/rst_n` + `dac_sclk/cs/mosi` + `dds_sclk/sync/data` | ✅ **当前有效** |

> ⚠️ **文件名会骗人。** `led.xdc` 这个名字会让人以为它只管那颗 LED。**实际上它是全工程唯一的
> 引脚约束文件** —— 30 根 LCD + 4 根触摸 + 6 根 DAC/DDS 的球号**都放在它里面**。
> 📌 **改引脚之前,先确认改的是哪个文件。**

球号出处:

| 端口组 | 出处 | 可信度 |
|---|---|---|
| `lcd_*` 30 根 + `touch_*` 4 根 | 厂商例程 `28_top_lcd_touch` 的 `top_lcd_touch.xdc`(#LCD / #LCD TOUCH 段) | 与板上已跑通的 `lcd_*` **同源同文件** |
| `dac_sclk=W15` / `dac_cs=W13` / `dac_mosi=Y14` | **用户确认的实际接线**(2026-10-05) | 实际接线 |
| `dds_sclk=T5` / `dds_sync=U7` / `dds_data=T11` | 2026-10-05 新增,取自 `docs/board_pins.md` 的空闲脚 | ⚠️ **照空闲脚挑的,未经接线核对** |

触摸 4 根**未加 `PULLUP` 属性** —— 厂商原设计也没加且能跑通,故推断排线上已有外部上拉;
本工程同样**故意不加**(加了会把手上的"外部上拉缺失"掩盖成"时好时坏")。

---

## ⚠️ 本文件没有 `create_clock`

实测 `grep -c create_clock led.xdc` → **0**。

⇒ 当前设计**没有显式的时钟周期约束**。时序报告里的频率来自 Vivado 对
`processing_system7_0` 的 `FCLK_CLK0` 的**自动推导**,不是我们写进去的。
写 M03「时钟频率」一节时必须如实标注这一点,不得说成"我们约束了 50 MHz"。

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
