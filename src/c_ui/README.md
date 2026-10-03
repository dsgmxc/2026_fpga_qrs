# c_ui —— PYNQ 上的 LVGL 界面进程

把前序 STM32H7 工程的 LVGL 8.2 界面(`ui_demo.c`,6 个页面)搬到 Zynq-7020 的 PS 上:
**PS 渲染 RGB565 → DDR 双帧缓存,PL 的 `lcd_fb_reader` 按行读出去扫屏**;
触摸坐标从 PL 的 `gt911_ctrl` 寄存器里读。

> 这是显示/触摸子系统的 PS 侧那一半。PL 侧(时序、帧缓存读出、I2C 主机、GT911 控制)
> 在 `qrs.srcs/sources_1/new/` 里,规格见 `docs/display_subsystem.md`。

---

## 1. 它是什么 / 不是什么

| 是 | 不是 |
|---|---|
| 一个**独立进程**,被 `src/python/main.py` 拉起 | ❌ 不是 Python 里的一个函数 —— 界面与编排分成两个进程,边界最硬 |
| `ui_demo.c` 的**原样运行**(一行没改) | ❌ 不是重写的界面 |
| 只管**画界面 + 读触摸** | ❌ **不管测量** —— 所有 `DisplayProtocol_Request*` 是空实现,一律返回 `false` |

⚠️ **本轮的预期现象**:点任何测量按钮,状态栏会显示「UART7发送失败」。
这是**如实报告"指令没发出去"**,不是触屏坏了 —— 触屏好不好看 `tools/touch_probe.py`。
页面之间的**导航**不走这一层(纯 LVGL),不受影响。

---

## 2. 目录

```
src/c_ui/
  Makefile                      板上原生编译(-std=gnu11,见文件头)
  third_party/
    lvgl/                       ★ LVGL 8.2.0 **原样拷贝,一个字节没改**
    LICENSE_LVGL.txt            LVGL 的 MIT 许可证
  port/                         ★ 路径别名树 —— 让 ui_demo.c 的 include 能原样解析
    BSP/LCD/lcd_panel_config.h    (前序同名头,LTDC/PLL 宏已删)
    BSP/TOUCH/lcd_touch.h         (前序同类型定义,实现换成读 PL 寄存器)
    BSP/LVGL/lv_port_disp.h       ★ 与前序唯一接口差异:Init 收两个物理地址
    BSP/LVGL/lv_port_indev.h      (与前序逐字相同)
  ui/                           ★ 前序源码原样搬(md5 见 §5)
    ui_demo.c / ui_demo.h
    ui_font_cn_20.c / ui_font_cn_28.c
    sweep_interpolation.c/.h  engineering_parameters.c/.h  display_protocol.h
  app/                          ← 本工程新写的部分,只有这些
    lv_conf.h                   LVGL 配置(照抄前序,只改内存策略,见 §4)
    pynq_regs.h / pynq_regs.c   /dev/mem 映射 + AXI 寄存器读写(自带头)
    pynq_disp.c                 LVGL 显示驱动(direct_mode 双缓冲)
    pynq_touch.c                触摸数据生产者(读 0x68/0x6C/0x70)
    pynq_indev.c                LVGL 输入驱动(缓存转发)
    pynq_protocol_stub.c        DisplayProtocol_* 空实现
    main.c                      入口 + 主循环(只做装配)
```

📌 **后四个 `.c` 没有自己的 `.h`** —— 它们实现的都是**前序已有的接口**
(`port/BSP/LVGL/lv_port_disp.h`、`port/BSP/TOUCH/lcd_touch.h`、
`port/BSP/LVGL/lv_port_indev.h`、`ui/display_protocol.h`)。
**故意不再加一层头**:多一层转发头就多一份可能漂移的声明,而且会让
"与前序接口逐字相同"这件事变得不可核对了。

```
```

---

## 3. 编译

**在板上原生编译**(不在 PC 上交叉编译 —— 理由见 Makefile 文件头,风险 R4)。

```bash
# 板上第一次:
sudo apt install build-essential

cd src/c_ui
make check          # 工具链 + /dev/mem + STRICT_DEVMEM 一眼看完
make -j2            # A9 双核;别 -j8,内存扛不住
```

约 170 个 LVGL 源文件,板上编一次是分钟级,属一次性成本。

**`make` 用的是 `-std=gnu11`。** 写成 `-std=c11` 会报
`implicit declaration of 'clock_gettime' / 'nanosleep'` —— POSIX 接口要
`_POSIX_C_SOURCE`,而 `-std=gnu11` 会自动带上等价的 `_DEFAULT_SOURCE`。

---

## 4. 与 LVGL 官方文档不一样的地方(踩过的坑)

1. **⛔ 不用 LVGL 自带的 fbdev 后端。**
   官方文档会让人直接写 `/dev/fb0` —— 那条路**不做双缓冲、不等 VBlank**,
   屏上会有明显撕裂。本项目走**自定义 flush 回调**(`pynq_disp.c`),
   前序 `lv_port_disp.c` 就是这么做的,**照它,不照文档**。

2. **`lv_conf.h` 用 `LV_MEM_CUSTOM 1`(libc 的 malloc/free)。**
   前序是把 LVGL 的堆钉死在一块固定物理 RAM(裸机没有 malloc)。
   Linux 上没必要钉,也不会因为 `LV_MEM_SIZE` 估小了在运行时炸。

3. **`direct_mode = 1` + `full_refresh = 0`,且必须做「矩形同步」。**
   `full_refresh=0` 时 LVGL 只重画失效区域,其余像素指望缓冲里还是上一帧。
   可**换址之后新后台是更早的那一帧** ⇒ 不把本次失效矩形从新前台补到新后台,
   界面会一块块"退回两帧前"。见 `pynq_disp.c` 的 `sync_invalid_areas()`。
   这个 bug 只在局部刷新时显形,极难归因,别删。

4. **没有 DMA2D。**
   前序用 STM32 的 DMA2D 硬件搬矩形,PYNQ 的 PL 里**故意没做**矩形搬运器
   (理由写在 `pynq_disp.c` 文件头,是 B04「资源取舍论证」的素材)。
   所以矩形同步只有 CPU 逐行 memcpy 一条路,而映射是**非缓存**的
   ⇒ 首帧(全屏失效)会明显慢。**实测帧率必须回填**(风险 R2)。

5. **`LV_COLOR_16_SWAP` 保持 0。**
   只有"整屏颜色对、但每两个像素互换"时才改 1;那种情况的另一半证据是
   `tools/display_probe.py` 的彩条也糊。**两处必须同时判**,别只看一处就改。

---

## 5. 可复现性:前序文件是逐字拷贝

`ui/` 下的 **9 个文件**与前序 STM32 工程(`~/Desktop/qrs/LCD/XIP_Appli/Core/`)
**逐字节一致**,已用 md5 逐一核对过:

| 文件 | md5 | 前序位置 |
|---|---|---|
| `ui_demo.c` | `2eb34d4e65e46743d6f379ce557a1e34` | `Src/` |
| `ui_demo.h` | `fe9f689f6210978b6e46825c9fb708bb` | `Inc/` |
| `ui_font_cn_20.c` | `cb9f2b604b363fe5eb2accc9c573b754` | `Src/` |
| `ui_font_cn_28.c` | `cd1eb866e9bfc6f7112619400997130f` | `Src/` |
| `sweep_interpolation.c` | `29e9cedc9c1eb6dc0d9bee1a9932a839` | `Src/` |
| `sweep_interpolation.h` | `75e004f8689e4720a2a5a101330437a0` | `Inc/` |
| `engineering_parameters.c` | `9502dcd7a494a2e4215e3ff8d944f887` | `Src/` |
| `engineering_parameters.h` | `daf652b4e3b5ef7a0fa4431515ce3b4f` | `Inc/` |
| `display_protocol.h` | `4fcb9c6e8a00ef4951ac1f8923effea0` | `Inc/` |

```bash
# 与前序工程逐文件对比(应全部无输出 = 完全一致)
SRC=~/Desktop/qrs/LCD/XIP_Appli/Core
for f in ui/ui_demo.c ui/ui_font_cn_20.c ui/ui_font_cn_28.c \
         ui/sweep_interpolation.c ui/engineering_parameters.c; do
    diff -q "$f" "$SRC/Src/$(basename $f)"
done
for f in ui/ui_demo.h ui/sweep_interpolation.h ui/engineering_parameters.h \
         ui/display_protocol.h; do
    diff -q "$f" "$SRC/Inc/$(basename $f)"
done
```

**`ui_demo.c` 里一行都没改。** 它 `#include "BSP/LCD/lcd_panel_config.h"`
这类前序工程路径,靠 Makefile 的 `-Iport` 让 `port/BSP/...` 顶上去 ——
**不改源码、不加 `#ifdef`**。这样它仍可与前序工程逐字对照。

---

## 6. 运行

### 正常路径(推荐)

```bash
sudo python3 ../python/main.py
```

`main.py` 会:分配两块 64 KB 对齐的 DDR 帧缓存 → 写 `FB_BASE0/1` →
把**物理地址**用命令行传给本进程。**C 侧拿不到物理连续内存,只能由 Python 传。**

### 单独跑(排障用)

```bash
sudo ./c_ui --fb0 0x1F000000 --fb1 0x1F200000 --poll-ms 10
sudo ./c_ui --fb0 0x... --fb1 0x... --no-touch     # 没接触摸底板时
./c_ui --help
```

⚠️ 单独跑时这两块地址里的内容可能是**上电残留的随机数据** ——
屏上先显示厂商彩条,等第一帧画完才切到 DDR 源(见 `main.c` 第 7 步的注释)。

退出码:`0` 正常 / `1` 初始化失败 / `2` 参数错。

---

## 7. 上板前置检查(任一不过就停下,别往下走)

| 检查 | 命令 | 不过怎么办 |
|---|---|---|
| 位流是最新的 | `md5sum qrs.bit` 应为 `ad93f160ae0fd4c97da325faabe4b85a` | 重拷 `board/` |
| 板上能编 C | `make check` | `apt install build-essential` |
| **`/dev/mem` 能映射 RAM** | `grep STRICT_DEVMEM /boot/config-$(uname -r)` | 见风险 R1 |
| 触摸有外部上拉 | 万用表量 R19/P20 静态电平 ≈ 3.3 V | 无上拉则 I2C 必挂 |
| 是 root | `id -u` = 0 | `sudo` |

### 风险 R1:`CONFIG_STRICT_DEVMEM`

开着的话 `/dev/mem` **不能映射普通 RAM**,`cv_phys_map()` 返回 NULL 并打提示。
备选路线(任一):
- 内核 cmdline 加 `iomem=relaxed`(需改启动参数)
- 改用 `/dev/dma_heap/system` + `DMA_HEAP_IOCTL_ALLOC`(映射可缓存 ⇒ 要配 `DMA_BUF_IOCTL_SYNC`)
- 退回"Python 侧 numpy 绘制"的降级路线(界面能力大幅缩水,只作保底)

---

## 8. 已知的未验证项(如实列出,不要当已通过)

- ⚠️ **本进程的 x86 上编译已通过(0 warning),但从未在板上编译、从未运行过。**
  代码可编译 ≠ 功能可用。
- ⚠️ **整个显示/触摸通路从未上板** —— 见 `docs/display_subsystem.md` 的验证矩阵。
- ⚠️ **风险 R2**:非缓存映射下整屏同步的实际耗时未知。首帧 1.2 MB 全屏写可能吃掉
  17 ms 帧预算的一大半。**必须实测回填**,不够再考虑 PL 加矩形拷贝器或换 dma-heap。
- ⚠️ **`display_protocol` 未接通**,`Request*` 全部返回 `false`(见 §1)。
- ⚠️ **PL 只导出 2 个触点、且不含 `track_id`** ⇒ 三指以上或"哪根手指是哪根"的语义
  与前序 STM32 版**不同**(前序是读 GT911 的 track_id 的)。别当成等价实现。
- ⚠️ **`LCD_Touch_ResetController()` 返回 `LCD_TOUCH_ERROR_UNSUPPORTED`** ——
  复位时序在 PL 里,PS 没有任何寄存器能触发它。要支持得在 PL 加一个 CTRL 位。
- ⚠️ `err` 粘滞位回答的是"**开机以来曾出错**",不是"此刻在错"(RTL 只在重配 PL 时清)。
  本实现只报告一次然后正常读 —— 否则一次上电噪声会让触摸**永久**失效。

---

## 9. 板上验收步骤

| # | 验什么 | 怎么判 |
|---|---|---|
| 1 | PS↔PL 链路 | `main.py` 的 `link_check()` PASS |
| 2 | FCLK0 固化 | `Clocks.fclk0_mhz` 读回 = 50;记 `report/measurements/` |
| 3 | **显示通路 + 风险 D1** | `tools/display_probe.py`:彩条 → 帧缓存渐变图,肉眼可见切换。**D1 的首次实测** |
| 4 | 帧缓存不欠载 | 跑 60 s 后 `0x64[19] underrun` **必须为 0** |
| 5 | **触屏坐标** | `tools/touch_probe.py`:坐标落在 0~1023 / 0~599,抬起后 `count` 归 0 |
| 6 | **LVGL 首页** | 屏上出现首页 + 中文字形(不是方框) |
| 7 | **LVGL 触摸联动** | 点首页按钮,页面切走 |

第 6/7 条才是"界面初始化"的真正判据。**结果落 `report/measurements/`,不要预填。**
