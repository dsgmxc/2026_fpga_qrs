# 2026-10-06 · 「扫压·全部」三条曲线为何只看见一条 · **帧缓存像素实测存档**

> 🔍 **结论:三条 series 全都画了,但它们逐点重合,后画的 CRSS 完全覆盖前两条。**
> **根因不是绘图,是硬件 —— 本工程没有继电器(`relay_ctrl` 全工程不存在)。**
>
> 对应必做点 **M04**(实测输出与比对)/ **M13**(板上跑通);归入 **P4**(双通道 + 继电器 + 校准)的缺口登记;
> 交互记录见 `report/llm-log/LLM-049.md`。

---

## 一、被测对象 / 测点 / 仪器

| 项 | 值 |
|---|---|
| 被测对象 | 结果页「扫压结果」页签的 **C-V 曲线图**(`g_results_chart`) |
| **测点** | **无物理测点**。测的是 **DDR 里正在显示的那一帧**(物理基址 `0x38100000`) |
| 仪器 | **无**。`/dev/mem` 映射 + 宿主机 PIL 做 RGB565 直方图 |
| 屏 | ATK-MD0700R-1024600-IPS,1024×600 |
| 现场 | 用户在板上跑完一轮「扫压 · 回路 = **全部**」,并停在「扫压结果」页签 |

### 1.1 测量原理

`lcd_fb_reader` 按**物理地址**从 DDR 取行扫描出去 ⇒ 直接读同一块内存就能拿到**正在显示的那一帧**
(见 `LLM-047` 的 `fb_dump.py`)。把 PPM 逐像素**量化回 RGB565**,再对三个系列色做**精确**直方图计数 ——
不靠肉眼,也不依赖对"看起来像一条"的判断。

---

## 二、三个系列色的定义(从代码取,不是目测)

| path | 图例名 | RGB888(`ui_demo.c:244`) | RGB565 |
|---|---|---|---|
| 0 | CISS | `0xd65ab1` 品红 | **`0xD2D6`** |
| 1 | COSS | `0xe5225b` 红 | **`0xE10B`** |
| 2 | CRSS | `0x8c971d` 橄榄绿 | **`0x8CA3`** |

添加顺序 `g_sweep_series[0..2]`(`ui_demo.c:5583-5594`)= CISS → COSS → CRSS;
LVGL **后添加的 series 画在上层** ⇒ 若三条重合,可见的是 **path 2 = CRSS**。

---

## 三、原始实测数据

帧缓存 dump(2026-10-06,`0x38100000`,`/home/xilinx/fbB.ppm`,1024×600,312 种颜色),
宿主机量化回 RGB565 后精确计数:

| 系列 | 精确 RGB565 | 像素数 | 包围盒 | y 方向簇 |
|---|---|---|---|---|
| **CISS** | `0xD2D6` | **104** | x`[405,430]` y`[139,142]` = **26 × 4** | **1 簇 = 图例色块** |
| **COSS** | `0xE10B` | **104** | x`[535,560]` y`[139,142]` = **26 × 4** | **1 簇 = 图例色块** |
| **CRSS** | `0x8CA3` | **4291** | x`[90,983]` y`[139,425]` | **2 簇**:`y[139,142]`(自身图例块)+ `y[167,425]` x`[90,983]`(**曲线本体**) |

### 3.1 三条互相印证的自洽点

1. **104 = 26 × 4** —— 正是图例色块的控件尺寸(`lv_obj_set_size(line, 26, 4)`,`ui_demo.c`),
   所以 CISS / COSS 的 104 px **就是各一个图例色块,曲线上 0 像素**。
2. **两个图例块间距 = 130 px**(405 → 535),正是代码里的 `item_x = 380 + index * 130`。
   ⇒ 确认这三块确实是那一排图例,不是别处的巧合配色。
3. **CRSS 多出来的 4187 px**(4291 − 104)落在 y`[167,425]`、横跨 x`[90,983]` —— 就是**曲线本身**。

---

## 四、结论

- **三条 series 都在画**(`g_sweep_visible_mask = 0x07`),不是渲染故障。
- **曲线上只有 CRSS 的颜色** ⇒ 三条坐标**逐点相同、完全重合**,栈顶 = 最后添加的 CRSS。
- 三条相同的原因:**每个偏压点只做一次 ADC 采集**,`sweep_measure_point()` 把**同一个** `s_pair.cs_pf`
  填进三个 path;而真实 Ciss / Coss / Crss 需要**三种测量回路**(G-S 短接 / D-S / G-D)靠继电器**每点切换**。
- **`relay_ctrl` 全工程不存在**(四层零命中:交付仓库 `src/rtl/` grep `relay` = 0;
  `cv_sys.v` 里 `0x1C`/`relay` = 0;C 侧 `pynq_regs.h` 无 `0x1C`;
  Python 侧 `cv_regs.py` 的 `RELAY_CFG: 0x1C` 落在「**测量主链(PL 侧尚未实现)**」块)
  ⇒ **这是 P4 的硬件缺口,不是显示 bug**。
- ✅ **界面侧零改动**:`sweep_measure_point()` 的 `for path` 循环已写好,继电器落地后自动出三条不同的线。

---

## 五、复现命令

```bash
# 板上(用户先把界面停在「扫压结果」页签)
echo xilinx | sudo -S /usr/local/share/pynq-venv/bin/python3 \
    /home/xilinx/fb_dump.py 0x38100000 /home/xilinx/fbB.ppm
# 宿主机
python3 /tmp/bx.py -d /home/xilinx/fbB.ppm /tmp/fbB.ppm
python3 -c "
from PIL import Image; from collections import Counter
im=Image.open('/tmp/fbB.ppm').convert('RGB'); W,H=im.size; px=im.load()
f=lambda r,g,b:((r>>3)<<11)|((g>>2)<<5)|(b>>3)
c=Counter(f(*px[x,y]) for y in range(H) for x in range(W))
for k,v in {'CISS':(0xd6,0x5a,0xb1),'COSS':(0xe5,0x22,0x5b),'CRSS':(0x8c,0x97,0x1d)}.items():
    print(k, c.get(f(*v),0))"
```

## 六、附件

| 文件 | 内容 |
|---|---|
| `20261006_sweep_all_framebuffer.png` | 整帧(1024×600),停在该页签的实屏 |
| `20261006_sweep_all_curve_crop.png` | 图表区裁切(y 120–440),曲线与图例 |

---

## 七、⏸ 未做 / 遗留

- **没有**做图片的**人工目视**二次确认(结论完全由像素计数给出)——
  这是刻意的:像素计数比"看起来像一条"更可信。
- **没有**验证"继电器落地后确实会出三条" —— 该验证要等 `relay_ctrl` 写出来才做得了。
- 本轮**零代码改动**;`pynq_protocol.c` / `ui_demo.c` 一行未动。
