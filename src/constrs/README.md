# src/constrs —— 引脚约束

> ⚠️ **先读这一页再动这两个文件。**
> 本目录有**两个 `.xdc`,但工程只启用其中一个** —— 写错文件不会报错,只会让约束**安静地不生效**。

| 文件 | 工程内状态 | 约束的端口 | 内容是否当前 |
|---|---|---|---|
| **`led.xdc`** | ✅ **启用**(`UsedIn = synthesis + implementation`) | **34 个**:`lcd_rgb[23:0]` + `lcd_hs/vs/de/bl/clk/rst` + `touch_scl/sda/int/rst_n` | ✅ **当前有效** |
| `io.xdc` | ⛔ **禁用**(`UserDisabled = 1`) | 15 个:上面那 30 根 LCD 的**重复副本**,外加 9 个**旧顶层遗留端口** `clk_in1` / `rst_n` / `dac_cs` / `dac_mosi` / `dac_sclk` / `adc_sync` / `adc_sclk` / `adc_mosi` / `clk_65m` | ❌ **已过时** |

依据:`qrs.xpr` 里两个 `<File>` 节点的属性 —— `io.xdc` 带 `<Attr Name="UserDisabled" Val="1"/>`,
`led.xdc` 没有。2026-10-02 实测复现:综合与实现**只 Parsing 了 `led.xdc`**,全程没碰过 `io.xdc`。

---

## ⚠️ 为什么文件名会骗人

`led.xdc` 这个名字会让人以为它只管那颗 LED。**实际上它是全工程唯一启用的引脚约束文件**
—— 30 根 LCD 和 4 根触摸的球号**都放在它里面**。

## 🔴 踩过的坑:约束写在禁用文件里 ⇒ 位流阶段才炸

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
看起来"还没轮到你约束",而不是"你约束写丢了"。⇒ **改引脚之前,先确认改的是哪个文件**。

```tcl
# 查出工程里到底哪些约束文件是启用的
report_property [get_files *.xdc]
```

---

## 当前的内容划分

**`led.xdc`(生效)**

- 30 根 LCD 引脚(`lcd_rgb[23:0]` 24 根 + `lcd_hs` / `lcd_vs` / `lcd_de` / `lcd_bl` / `lcd_clk` / `lcd_rst` 6 根)
- 4 根 GT911 触摸(`touch_scl` / `touch_sda` / `touch_int` / `touch_rst_n`)

球号出处:厂商例程 `28_top_lcd_touch` 的 `top_lcd_touch.xdc`(#LCD / #LCD TOUCH 段),
**不是**从原理图查的 —— 但与板上已跑通的 `lcd_*` 那 30 行**同源同文件**,可信度高。
触摸 4 根**未加 `PULLUP` 属性** —— 厂商原设计也没加且能跑通,故推断排线上已有外部上拉;
本工程同样**故意不加**(加了会把手上的"外部上拉缺失"掩盖成"时好时坏")。

**`io.xdc`(禁用,保留备查)**

上半段是**旧顶层 `qrs.v` 的遗留约束** —— `clk_in1` / `rst_n` / `dac_*` / `adc_*` / `clk_65m`
这 9 个端口**在当前的 `system_wrapper` 里根本不存在**。它们记录的是测量主链
(DAC1220 偏置 / AD9833 激励 / AD9238 采样)的**球号规划**,这是别处没有的信息,所以保留。
其中 AD9833 三根线的球号旁边有原注:

> ⚠️ 这三个球号是照 `docs/board_pins.md` 的 J4 表挑的**空闲脚**,**不是从实际接线查来的**
> —— 上板前请对 J4 丝印核对一次。

📌 **当前顶层是 `system_wrapper`(由 `src/rtl/` 之上的 BD `system` 生成),不是 `qrs.v`。**
所以这两个文件里的端口与当前设计的对应关系是:**只有 `led.xdc` 那 34 个端口是对得上的。**

---

## ⚠️ 两个文件都没有 `create_clock`

实测 `grep -c create_clock` → 两个文件**都是 0**。

⇒ 当前设计**没有显式的时钟周期约束**。时序报告里的频率来自 Vivado 对
`processing_system7_0` 的 `FCLK_CLK0` 的**自动推导**,不是我们写进去的。
写 M03「时钟频率」一节时必须如实标注这一点,不得说成"我们约束了 50 MHz"。

---

## 与工程的关系(单向)

本目录是 **Vivado 工程 `qrs.srcs/constrs_1/new/` 的快照**。
**方向是单向的:工程 → 仓库。** 在工程里改了 `.xdc`,本目录**不会自动跟着变**,
需要手工重新拷贝。改动前请确认改的是**工程里那份**。
