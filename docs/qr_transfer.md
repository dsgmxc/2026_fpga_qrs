# 实测数据二维码(结果汇总 → 数据传输)

> 状态:**代码完成、宿主单测与板上截图均通过(2026-10-17)**;
> 屏上二维码截图可被 jsQR 解码,1223 B 与宿主生成的 URL 逐字节一致(md5 `e65c5343…`)。
> ⛔ 尚未验证:**真实手指触摸**、**手机实拍扫码**。
> 协议**照搬前序工程** `~/Desktop/qrs`,不自行发明格式。
> 网页解码页:`https://fox4327.github.io/ai-cv2spice-web/`

---

## 一、URL 形式

```
https://fox4327.github.io/ai-cv2spice-web/#m=<ACV1>&c=<SCV1>
```

| 段 | 必选 | 内容 | 编码器 |
|---|---|---|---|
| `#m=` ACV1 | 是 | LTspice 建模载荷:12~13 个电压节点 + Qgs/Qgd/Qds 电荷增量 | `app/ai_cv2spice_acv1.c`(前序逐字移植) |
| `&c=` SCV1 | 否 | 完整 C-V 曲线:三条曲线重采样到网页隐式网格 | `app/ai_cv_sweep_payload.c`(前序逐字移植) |
| 组装 | — | 公共有效区间、ACV1 节点、SCV1 重采样、URL 拼接 | `app/cv_qr_payload.c` |
| 生命周期 | — | 三路都完整才生成;失败清空并打 `[qr]` 日志 | `app/cv_qr_live.c` |
| UI | — | 「结果汇总 → 数据传输」页 | `ui/ui_demo.c` |

实测 URL 长度约 **1.2 kB**(12 节点 / 141 点),缓冲 3072 B。

## 二、ACV1 元数据(本项目取值)

| 字段 | 值 |
|---|---|
| flags | `MEASURED_PHYSICAL`(bit2 = 4) |
| device | `LIVE` |
| part number | `MEASURED_MOSFET` |
| manufacturer | `Measured` |
| frequency | `MeasureConfig_GetDdsHz()`(默认 1 MHz) |
| excitation Vpp | `1.0` |
| Rg | `UI_Demo_GetLatestResistance()`;无效时回退 **3.6 Ω**(与前序一致,日志会注明) |
| 电压节点 | 0.1 / 0.494 / 1.176 / 2.379 / 4.559 / 7.627 / 10 / 13.11 / 20.78 / 33.84 / 56.62 / 100 V |

节点截到三条曲线的公共终止电压;若终止电压不等于列表末项,补一个终止节点。
节点数至少 2、最多 13。电荷增量按 `[0]=Ciss−Crss`、`[1]=Crss`、`[2]=Coss−Crss`
做梯形积分;0 V → 首个实测点按首点值恒定。

> ⚠️ 前序的 `1e-8 nC` 下限在 Float16 下会变成 0。本项目改为 **`1e-6 nC`**
> (Float16 次正规数可表示),否则小电容点的 ΔQ 全被压成 0。

## 三、SCV1 隐式电压网格(必须与网页逐位一致)

网页解码器不传电压表,而是**按 start/stop 重建**网格,规则是:

1. `[0] = 0`;
2. 对 `i = 1..60`,若 `i * 20/60 <= stop` 则追加;
3. 若 `stop > 20`,追加整数 `21 .. floor(stop)`;
4. 若末项与 `stop` 相差 > `1e-4` 再补 `stop`;
5. 最后把 `[0]` 替换成 `max(start, 0.1)`。

`start` / `stop` 取三条曲线公共有效区间的交集。`stop = 100 V` 时网格为 **141 点**;
`stop <= 20 V` 时 61 点。

C 侧不自行挑点,而是按上述规则生成同一组浮点值,再对每条曲线做 log-log 插值
(`ui/sweep_interpolation.c`)采样。**start/stop 先按 float32 取整再参与生成**,
保证与解码器读到的 float32 值一致。

每条曲线的 12-bit 量化范围取该曲线有效点的 `log10(C)` 最小/最大。

## 四、UI 行为

| 状态 | 传输页显示 |
|---|---|
| 没有完整数据 | 状态行 `等待三回路扫压`(橙),载荷行 `ACV1 -- · SCV1 -- · URL --`,`DEMO / NO DATA`;二维码仍显示,指向静态页 |
| 三路扫压完成 | 状态行 `数据就绪,可扫码传输`(绿),载荷行 `ACV1 12 节点 · SCV1 141 点 · URL 1223 B`,`LIVE / MEASURED / PHYSICAL` |
| QR 编码失败 | 隐藏二维码,状态行红色 `QR RENDER ERROR` |

- 二维码在结果页刷新时按脏位重画(`g_transfer_qr_dirty`),不是每 80 ms 重排。
- 扫压开始 / 复位 → `CV_QR_Live_Reset()` 清空载荷并回到 DEMO。
- 扫压结束(完成或停止)→ `CV_QR_Live_OnSweepComplete()`;**只有三路都有数据**才生成。

## 五、验证

### 5.1 宿主单测

```bash
cd src/c_ui
gcc -std=gnu11 -O2 -Wall -Wextra -DLV_CONF_INCLUDE_SIMPLE \
    -Iapp -Iui -Iport -Ithird_party/lvgl \
    tests/qr_payload_test.c \
    app/ai_cv2spice_acv1.c app/ai_cv_sweep_payload.c \
    app/cv_qr_payload.c ui/sweep_interpolation.c \
    -o /tmp/qr_payload_test -lm
/tmp/qr_payload_test     # 期望最后一行 PASS (0 failure(s))
```

### 5.2 与网页解码器交叉验证

宿主单测会写 `/tmp/qr_url.txt`。用前序工程自带的 JS 解码器逐段核:

```bash
node /tmp/verify_qr.mjs   # 期望 PASS
```

断言内容:ACV1 flags/元数据/节点数、36 个 ΔQ > 0、SCV1 141 点、
`voltageV[]` 与独立重建的隐式网格逐点一致(容差 1e-4)、
0.1 / 1 / 10 / 100 V 四点 `csPf` 相对误差 < 0.5%。

### 5.3 板上

1. 上传 `app/ai_cv2spice_acv1.{c,h}`、`app/ai_cv_sweep_payload.{c,h}`、
   `app/cv_qr_payload.{c,h}`、`app/cv_qr_live.{c,h}`、`ui/ui_demo.c/.h`、
   `app/pynq_protocol.c` 到 `/home/xilinx/c_ui_src/`,**回读 md5**;
2. 板上 `make -j2`、软重启;
3. systemd drop-in:`Environment=CV_UI_DEMO_PAGE=4 CV_UI_RESULT_TAB=4`
   (自动跑 0.1→100 V 三路扫压后进「数据传输」页);
4. `tools/fb_dump.py` 截图,核 `[qr] ACV1 ... / SCV1 ...` 日志;
5. 第二个上电周期改 `CV_UI_RESULT_TAB=2`,并用 `CV_UI_DEMO_PROBE=10`
   抓「扫压结果」页的探针读数截图(仅截图用,正常运行不设)。

**实跑结果(2026-10-17,3 个上电周期)**:

- 板上 `make -j2` 成功;上传文件回读 md5 与宿主一致;
- `[qr] ACV1 12 节点 / SCV1 141 点 / URL 1223 B / CRC acv1=0xa78b18cf scv1=0xaee88716`;
- 对屏上截图(288 px QR)用 jsQR 解码:**成功,1223 B**,
  md5 与宿主 `/tmp/qr_url.txt` 同为 **`e65c53432d5fec7d9640df2be8c87d70`**;
- 证据:`report/measurements/20261017_ui_result_transfer/`。

> ⚠️ **中文字体是子集字体**:`ui_font_cn_20/28.c` 按固定符号表生成,
> 新文案缺字会**静默渲染为空白**。本次因此补了 65 个码位(605 → 670),
> 涵盖 `析/维/滑/住/左/右/服/访/静/解` 等。以后改中文文案必须重新扫描字符串
> 并重跑 `lv_font_conv`(参数见本目录证据 README)。

## 六、已知限制

- **依赖外部静态页**:手机必须能访问 `fox4327.github.io`;数据本身只在 URL 片段里,
  不经过任何服务器。
- **只有三路都完整才生成**;单路或中止扫压继续显示 DEMO 状态。
- SCV1 点数由终止电压决定(100 V → 141 点),不能自行改点。
- 结果页图表不做捏合缩放;缩放仍只在扫压页,视口变化会同步到结果页。
- 前序的 AI 质量门控、预处理、模型推理**一律未移植**,只复用编码与 URL 形式。
