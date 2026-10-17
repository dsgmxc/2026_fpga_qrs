# 2026-10-17 板上验证:扫压结果页 + 数据传输二维码

> 对应改动:LLM-070(参数页并入结果页、触摸读数、实测数据二维码)。
> 本目录只放**板上实测证据**,协议细节见 `docs/qr_transfer.md`。

## 一、本轮改了什么(上板前)

| 文件 | 改动 |
|---|---|
| `src/c_ui/ui/ui_demo.c` | 结果页摘要文案缩短(`每路点数 N · 有效曲线 x/3 · RG …`),原来 350 px 放不下被裁 |
| `src/c_ui/ui/ui_font_cn_20.c` | **重新生成**:补上缺失汉字(析/维/滑/住/左/右/服/访/静/解…) |
| `src/c_ui/ui/ui_font_cn_28.c` | 同上,符号表统一 |

### 字体问题的发现过程

第一轮截图里「分析电压」渲染成「分电压」、「实测数据二维码」会缺「维」字。
原因不是文案写错,而是 `ui_font_cn_20/28` 是**按固定符号表生成的子集字体**,
新文案里的字不在子集里 —— LVGL 缺字**不报错、直接渲染空白**。

修法:扫描 `ui/*.c`、`app/relay_view.c`、`app/pynq_protocol.c`、`app/cv_qr_live.c`
的全部字符串字面量(注释除外),与字体 cmap 比对,把缺少的字并进原符号表,
用同一份 `NotoSansCJKsc-Regular`(从系统 TTC 抽出 SC 面)、同参数重新生成:

```
--size <20|28> --bpp 4 --format lvgl -r 0x20-0x7E --symbols <661 字>
--no-compress --no-kerning --lv-include lvgl.h
```

结果:20 px 字体 605 → 670 个码位;28 px 同表。
以后新增中文文案,必须重跑这一步,否则照样静默缺字。

## 二、板上部署与构建

| 步骤 | 结果 |
|---|---|
| 上传 `ui/ui_demo.c`、`ui/ui_font_cn_20.c`、`ui/ui_font_cn_28.c` 到 `/home/xilinx/c_ui_src/ui/` | 回读 md5 与宿主逐一致 |
| 板上 `make -j2` | `✅ 编好了:c_ui` |
| `qrs-ui.service` | 平时 `disabled`;三次上电周期,每次只在开机后手动 `start` 一次 |

md5(宿主与板上一致):

```
22f6c2bbbdf920331170e7671afb2d0b  ui_font_cn_20.c
0a7e621fbb665a719c5f6657f81cde15  ui_font_cn_28.c
5692568e11444f81bba9514d62b6b672  ui_demo.c
```

## 三、三个上电周期(每次上电只加载一次位流)

| 周期 | 演示配置 | 抓到的内容 |
|---|---|---|
| 1 | `CV_UI_RESULT_TAB=2`(扫压结果)+ `CV_UI_DEMO_PROBE=10` | 发现缺字与摘要裁切(未留图,问题已修) |
| 2 | `CV_UI_RESULT_TAB=4`(数据传输) | `transfer_page.png` |
| 3 | `CV_UI_RESULT_TAB=2` + `CV_UI_DEMO_PROBE=10` | `result_page_probe10v.png` |

周期 2/3 均完成三回路 0.10→100 V 扫压(60 点/腿),日志:

```
[qr] ACV1 12 节点 / SCV1 141 点 / URL 1223 B /
     CRC acv1=0xa78b18cf scv1=0xaee88716 / 0.1000-100.0000 V
[qr] Rg 未知,回退 3.60 Ω(与前序一致)
```

原始日志行见 `board_qr_log.txt`。

## 四、二维码实测(关键证据)

对 `transfer_page.png` 用 **jsQR**(纯 JS 解码器)解码屏幕截图:

- 解码成功,**长度 1223 B**;
- 解码文本 md5 `e65c53432d5fec7d9640df2be8c87d70`;
- 与宿主单测生成的 `/tmp/qr_url.txt` **逐字节相同**(同 md5)。

解码文本存于 `qr_decoded_url.txt`(含完整 ACV1 + SCV1,可直接粘进浏览器验证)。

QR 版本较高(URL 1.2 kB),屏上模块约 2 px;截图原生分辨率可解,
**手机实拍尚未验证** —— 这是唯一还缺的一环。

## 五、界面截图

### result_page_probe10v.png

「结果汇总 → 扫压结果」:曲线 + 探针十字线(10 V 附近原始点 `#40/60`)、
读数行(VBIAS / Cs / Cp / D)、分析电压控件、6 张参数卡,一屏内互不重叠。

### transfer_page.png

「结果汇总 → 数据传输」:288 px 二维码 + 状态「数据就绪,可扫码传输」+
`ACV1 12 节点 · SCV1 141 点 · URL 1223 B` + `LIVE / MEASURED / PHYSICAL`。

## 六、用户实按确认

2026-10-17 用户操作后确认:

- **真实手指触摸**:按住/滑动曲线,读数正常跟手;
- **手机扫码实拍**:扫描「数据传输」页二维码正常。

⇒ 本批改动**全部验证项结案**。板子随后已关机(`shutdown -h now`)。
