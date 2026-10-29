# 大模型协作记录索引

> 本目录是「大模型协作与技能包」评分项(15%)的**交互记录**部分。
> 由 Claude 自动维护,用户无需操心。
>
> **记录纪律(红线)**:只记真实发生的事。指南要求每项写明"已验证的效果"与"从哪些失败中总结得出",
> 编造的记录经不起追问。

---

## 记录一览

| 编号 | 日期 | 模型 | 任务 | 结果 | 沉淀到 |
|---|---|---|---|---|---|
| [LLM-001](LLM-001.md) | 2026-09-12 | Claude | 首批 RTL 模块设计(NCO / AD9833 SPI / 继电器) | ⚠️ 部分可用 | `skill/golden-model-first/`(待提炼) |
| [LLM-002](LLM-002.md) | 2026-09-12 | Claude | 仿真必要性之辩 + **代码分工纠正** | ⚠️ 部分可用 | 报告 §5 协作模式判断 |
| [LLM-003](LLM-003.md) | 2026-09-13 | Claude | 工程审查 + **把推断说成赛题规定被用户戳穿** | ⚠️ 部分可用 | 候选 `skill/llm-claim-provenance/` |
| [LLM-004](LLM-004.md) | 2026-09-13 | Claude | `spi_master.v` 两轮 RTL 审查(**改了没改对 / 改 A 引入 B**) | ✅ **已结案**(见记录内 09-15 更新) | 候选 `skill/verilog-review-checklist/` |
| [LLM-005](LLM-005.md) | 2026-09-15 | Claude | SPI 主机验证收口 + DAC1220 架构方案 + 首版 `dac_1220.v` 审查 | ⚠️ 部分可用(**审查结论未验证**) | `skill/verilog-review-checklist/` + `skill/spi-tb-patterns/` |
| [LLM-006](LLM-006.md) | 2026-09-16 | Claude | 读 DAC1220 手册解除阻塞项 + **发现厂商例程两处缺陷** | ⚠️ 部分可用(**结论仅有手册出处,未实测**) | 候选 `skill/vendor-example-is-not-spec/` + `skill/datasheet-to-rtl-constants/` |
| [LLM-007](LLM-007.md) | 2026-09-16 | Claude | DAC1220 复位+校准收口:命令边界 / CS 合法性 / **一次 §6.0 授权代改** | ⚠️ 部分可用(**代码零运行,全为静态推演**) | 候选 `skill/one-signal-one-meaning/` + `skill/spi-master-slave-handshake-checklist/` + `skill/loop-bound-change-safety/` + `skill/cs-is-optional-in-spi/` |
| [LLM-008](LLM-008.md) | 2026-09-18 | Claude | 正常工作状态(NORMAL)时序规格 + **四轮「指出→修正→复核」** | ⚠️ 部分可用(**四版代码均零运行**) | 新候选 `skill/intent-vs-code-mismatch/` + 扩展 `skill/spi-master-slave-handshake-checklist/` |
| [LLM-009](LLM-009.md) | 2026-09-18 | Claude | `dac_1220.v` **首次跑起来**:code↔电压换算 + 测试台审查 + **触发死锁修复与实测坐实** | ✅ **直接可用**(有 xvlog/xelab 日志 + VCD 波形) | 新候选 `skill/testbench-timebase-must-match-dut/` + `skill/verify-the-verifier/` |
| [LLM-010](LLM-010.md) | 2026-09-19 | Claude | **DIR 字节切片错**(对照手册+厂商例程查出)+ 测试台改为自检型 | ✅ **直接可用**(21 项 PASS + 变异测试 4 FAIL 原始日志) | 新候选 `skill/judge-source-must-be-independent/` + 扩展 `skill/vendor-example-is-not-spec/` |
| [LLM-011](LLM-011.md) | 2026-09-19 | Claude | 顶层 `qrs.v` **最简上板例化**(端口方向 / `code` 初值 / `set_in` 单次脉冲) | ✅ **直接可用**(elaboration 零错 + 顶层仿真实测 9 字节) | 扩展 `skill/verify-the-verifier/` + 新候选 `skill/stale-artifact-silent-reuse/` |
| [LLM-012](LLM-012.md) | 2026-09-19 | Claude | **上板输出 0 V 排障** —— 根因是烧了旧程序;**模型一度误判引脚**;同日追加:三点电压实测 + `locked` 代写 + `create_clock` 释义 | ⚠️ 部分可用(**引脚核对对、根因判断错**;追加部分可用) | 新候选 `skill/board-pin-lookup/` + 扩展 `skill/verify-the-verifier/` |
| [LLM-013](LLM-013.md) | 2026-09-20 | Claude | AD9833 上板前规格核定(参数 / 4 帧序列 / 复位流程)+ **模型把一处 RTL 改动判成「零风险」被用户一句反问推翻** | ⚠️ 部分可用(**规格可用,风险判断先错后对**;零运行) | 新候选 `skill/shared-module-change-blast-radius/` + 扩展 `skill/vendor-example-is-not-spec/` |
| [LLM-014](LLM-014.md) | 2026-09-20 | Claude | **首次授权代写 `adc_9833.v`**(分帧修 init + 补 NORMAL 态)+ **模型报假 FAIL、还差点改坏项目自己的常数** | ✅ **直接可用**(10 帧全 PASS + 2 组变异测试原始日志) | 新候选 `skill/measurement-instrument-can-be-wrong/` + `skill/edge-latch-outside-fsm/` |
| [LLM-015](LLM-015.md) | 2026-09-20 | Claude | **代写 `tb_adc_9833.v`**(47 项自检 + 4 变异体)+ **实测出 t8 超手册上限 25 ns、并发现 xsim 的 `%s` 编码陷阱** | ⚠️ 部分可用(**测试台可用且已归档;但 t8 判定未结案**) | 新候选 `skill/xsim-printf-non-ascii/` + `skill/datasheet-bound-must-be-checked-against-intent/` |
| [LLM-016](LLM-016.md) | 2026-09-20 | Claude | **把 `adc_9833` 最简接入 `qrs.v`**(端口 / 例化 / XDC 共 3 处)+ **顶层实测 4 帧全对,并第 6 次栽在"量具"上** | ⚠️ 部分可用(**顶层仿真实测通过;但引脚未经接线核对、未综合、未上板**) | 新候选 `skill/verilog-integer-defaults-to-x/` |
| [LLM-017](LLM-017.md) | 2026-09-20 | Claude | **AD9833 上板首测「500 mV 直流」排障** —— 供电 / `CLK` vs `SCLK` 语义 / 输出频率核定 | ⚠️ 部分可用(**规格核定可用;故障根因未定位,零测量**) | 新候选 `skill/board-pin-electrical-constraints/` + `skill/clock-domain-is-not-clock-frequency/` |
| [LLM-018](LLM-018.md) | 2026-09-21 | Claude | **AD9833 上板跑通,代码本来就没问题** —— 根因是**跳线帽没接板载晶振**(用户查出);改前(1 MHz)/改后(1 kHz)两版位流**在模块 SMA 口都测过**;官方 C 例程逐帧对照;**决策 D10 被证伪** | ⚠️ 部分可用(**对照与核验可用;模型零上板操作,根因由用户定位;测点/版本后由用户补答,上板读数仍无存档**) | 新候选 `skill/zero-cost-elimination-first/` + `skill/static-artifact-reading-expires/` + `skill/test-constant-must-be-reclaimed/` + 扩展 `skill/vendor-example-is-not-spec/` |
| [LLM-019](LLM-019.md) | 2026-09-22 | Claude | **AD9238 采集模块「算作写完」**(用户决定不仿真不上板);模型核出**厂商手册与厂商例程对同一个转换关系给出两套矛盾答案** | ⚠️ 部分可用(**移植核对与矛盾发现可用;哪套公式对,模型不判、用户未表态;零运行**) | 候选 `skill/vendor-docs-can-contradict-each-other/`(可与 `skill/vendor-example-is-not-spec/` 合并) |
| [LLM-020](LLM-020.md) | 2026-09-22 | Claude | **交付仓库建立** —— 回一手出处重核提交要求(3.3.3.2 / 3.3.3.3 / 3.3.5.1 / 3.3.5.4);首次提交前命名审计与两条事前拦截;**模型"合并两个目录"的建议被用户否决并撤回** | ⚠️ 部分可用(**要求核对与拦截可用;`docs/` 目录未经用户明确表态;仓库仍是占位,`LICENSE`/`skill/README.md` 为 0 字节**) | 候选 `skill/repo-first-commit-audit/` + `skill/glob-ignore-swallows-evidence/` + 扩展 `skill/zero-cost-elimination-first/` |
| [LLM-021](LLM-021.md) | 2026-09-25 | Claude | **LCD 彩条上板跑通** —— 显示/触摸方案定案(PL 侧时序 + 曲线缓存 + GT911 IIC);例程 GBK 编码修复;删 3 个冗余模块;最简例化 + 30 根 XDC + 综合通过;**模型"`.xpr` 残留引用已清零"的结论是错的,靠再跑一次综合才暴露** | ⚠️ 部分可用(**方案与例化可用、彩条已出;但引脚/编码/综合三项已实测,彩条照片已拍且顺序核对正确,**但照片未拷入工作区 ⇒ 归档仍未闭环**;模型一处结论错误由工具而非自查发现**) | 新候选 `skill/vivado-cli-verify-after-edit/` + `skill/gbk-example-porting/` + `skill/refuse-unfounded-conclusion/` |
| [LLM-022](LLM-022.md) | 2026-09-25 | Claude | **无网线跑通 PYNQ 2.7** —— 串口控制台路径打通;`brltty` 抢 CH340 定位;`OSError: Root permissions required` 与 `No Devices Found` 排障;**模型给了一条粘不进串口终端的多行命令** | ⚠️ 部分可用(**PYNQ 已跑通且 `ip_dict` 实测打印;但模型一处命令形式出错、`fpga_manager state` 仍读 `unknown` 未解释、串口输出无截图存档**) | 候选 `skill/pynq-overlay-bringup/` + `skill/serial-console-command-hygiene/` + 扩展 `skill/refuse-unfounded-conclusion/` |
| [LLM-023](LLM-023.md) | 2026-09-25 | Claude | **P2 头号风险解除** —— 拿板上**真解析器**跑 Vivado **2025.2** 真 hwh,与 2020.2 厂商同形示例对照;**探针 `ip_dict` 为空时模型没有读成"不兼容",追根因 + 补负对照才下结论**;顺带查实「无 AXI 的 RTL 模块在 PYNQ 里无对象可访问」 | ✅ **直接可用**(真件对真件,md5 与复现命令已归档;2 条边界明确标注未验) | 候选 `skill/pynq-overlay-bringup/` + `skill/control-group-before-conclusion/` + 扩展 `skill/refuse-unfounded-conclusion/` |
| [LLM-024](LLM-024.md) | 2026-09-26 | Claude | **串口控制台登录 + 电脑↔板子直连网线打通** —— `picocom` + `pynq-direct` 静态 IP;用 **ARP 里 MAC 与板子 `ifconfig` 的 `ether` 逐位一致**坐实"包真的到了板子";`_xsrf` 报错定位为标签页过期;**模型 3 处失误:指令没写执行位置(用户在板子上 ping 自己)、两机同名连接害用户删了配置、nmcli 校验顺序写错** | ⚠️ 部分可用(**链路已通且 Jupyter 登录成功;但 3 处失误均由用户实际执行才暴露**) | 扩展 `skill/serial-console-command-hygiene/` + 新候选 `skill/ping-is-not-link-proof/` + `skill/same-name-across-hosts/` |
| [LLM-025](LLM-025.md) | 2026-09-26 | Claude | **PS 侧开发流程定案**(写电脑/跑板子、不拷库、**拒绝装 3.8 与拒绝升级板子的 Python**)+ 工具链(VS Code 插件、多根工作区)+ **继电器架构确认 PS→PL**(三条文档出处 + D17 边界规则的调和);顺带**实测出交付仓库 `src/rtl/` 仍为空** | ⚠️ 部分可用(**全部为设计与文档核对,零落地验证**;继电器引脚位置明确打了问号) | 候选 `skill/avoid-version-parity-trap/` + 扩展 `skill/no-all-negative-from-shallow-search/` |

| [LLM-026](LLM-026.md) | 2026-09-29 | Claude | **第一次完整 PS↔PL 联动跑通** —— 自建 BD(PS7 + `axi_smc` + `rst` + `axi_gpio`)从零到**板上 LED 点亮**(`before: 0` → `write(1,1)` → `after: 1`);实测验掉「`X_INTERFACE_PARAMETER` 是时钟被识别的必要条件」(根因是**模型自己的探针只对一个变体跑了 automation**)、「`GPIO_0` 会展开成 3 个端口并卡 DRC」(Vivado 自动插 IOBUF,**实测 1 个端口**)、「22 端口不通」(实测 sshd 正常);顺带查实 **`BDTYPE` 只出现在层次化模块上** ⇒ PYNQ 里名字是裸实例名 | ⚠️ 部分可用(**联动已板上实测跑通、时序 WNS +11.544 ns;但用的是现成 `axi_gpio`,`cv_axi_lite_slave.v` 仍一行未写 ⇒ P2 判据「SCRATCH 回环」未达成;板上输出与 LED 照片**未存进 `report/measurements/`**;模型三处结论被实测推翻**) | 候选 `skill/probe-design-flaw-masquerades-as-conclusion/` + `skill/bd-interface-map-is-not-wrapper-port/` + 扩展 `skill/pynq-overlay-bringup/` + `skill/no-all-negative-from-shallow-search/` |

| [LLM-027](LLM-027.md) | 2026-09-29 | Claude | **自写 AXI4-Lite 从机首次上板跑通 —— P2 判据达成**。用户以「`cv_axi_lite_slave.v` 为什么不现在开始写这个」纠正模型的**排序错误**(模型拿 P4 的继电器引脚去 block P2 的判据);用户授权代写后挂进 BD、生成位流,板上四项全对:`VER 0x10000` / `S0 0x0` / `W1 deadbeef` / **`W2 a5a55a5a`(换值第二笔,排除"恒返回常数")**;顺带板上 `ip_dict` 实测确认三条事前预测(裸实例名 / `ADDR_WIDTH=7` / `registers` 为空);改地址时**先查 mtime 判定"BD 未保存"**而非怀疑用户;发现 `clk_axi` 文档写 100 而 BD 是 50 ⇒ 同步 4 份文档,**并回查标出 D20 一条理由以此为前提、已随降频失效** | ✅ **直接可用**(**有上板实测存档** `report/measurements/20260929_scratch_loopback.md` + 位流 md5;仅 `xvlog`/`xelab` 语法过,**零功能仿真**;**"通路通"未推出"频率对"**已显式标注) | 新候选 `skill/stage-dependency-direction/` + `skill/pynq-register-map-pattern/`(**沿用 `skill/README.md` 已列的 B1 计划项名,本次首次有板上实测可填**)+ 扩展 `skill/measurement-instrument-can-be-wrong/` |
| [LLM-028](LLM-028.md) | 2026-10-01 | Claude | **`cv_sys` 加 PS 控屏寄存器 + 上板验证;顺带实测出 FCLK0 偏离设计值 16.7%**。`LCD_CTRL`(0x50)/`LCD_FILL`(0x54)两个寄存器 + 像素源 mux;**关键设计选择:彩条做成 `LCD_CTRL` 的复位默认值** —— 直接回应用户"能不能跳过彩条验证",把那次回归变成自动保留。板上:绿→红→蓝→彩条四次全对,RGB 位序同时验证。**FCLK0 实测 41.6667 MHz,而 BD 写的是 50.000000** —— 根因是 `Overlay()` 不跑 `ps7_init`,BD 的 PS 配置在运行时被完全忽略;经寄存器前后对比(`0x00600600`→`0x00101e00`)+ 三条独立事实反推出 **IO PLL = 1500 MHz**,确认 41.6667 是真时钟且 PYNQ 可在运行时改到精确的 50 MHz。⚠️ 本轮**错误密度最高**:7 处错误中 6 处是模型自己的,含**隐式声明 bug(靠 `xvlog` 抓到,非目测)**、**按错寄存器位定义解码(套了 `UART_CLK_CTRL` 的布局)**、**用未验证的 IO PLL=1000 常数推出错误结论**、**把工作区 README 当仓库 README 编出虚构漂移**、**编造板上 Python 版本** | ⚠️ 部分可用(**成果有上板实测存档 `report/measurements/20261001_lcd_ps_ctrl_and_fclk0.md`;但模型判断错误密集,且"频率"结论只有寄存器解码、无独立时基测量**) | 新候选 `skill/verilog-declaration-order/` + `skill/register-bitfield-from-datasheet/` + `skill/derive-constants-from-multiple-points/` + `skill/cwd-reset-breaks-relative-paths/` + 扩展 `skill/no-all-negative-from-shallow-search/` |
| [LLM-029](LLM-029.md) | 2026-10-02 | Claude | **界面路线修订 —— 从「曲线缓存」改为「DDR 双帧 + PL 自研 AXI 读主机」**。用户要求「全功能实现」,模型读前序 STM32 项目的 LCD 部分后发现:**前序 UI 是 6 页面 LVGL**(`ui_demo.c` 5367 行 + 中文字库 15088 行),而 **D22 的「曲线缓存 + `y == curve[x]` 描点」在范围上承载不了**。🔴 **本次最有价值的一条是模型对 D22 的重新推导**:D22 的**算术没错**(14.75 Mb 确实装不进 4.9 Mb BRAM,今天依然成立),**错在隐含前提**(假设"界面只需一条曲线",这个前提**从未被写下也从未被验证**)与**漏算 DDR**(拿"BRAM 装不下"否掉帧缓存,可帧缓存本来就不该放 BRAM)⇒ 新增 **D24** 修订、D22 原行一字未删。**修正是执行手段**:新增 **D25**(界面全功能复刻)、**D26**(UI = 横跨 P3–P6 的独立工作流);新建 `docs/display_subsystem.md`(11 节,含 7 个新寄存器 / 风险 D1–D6 / PS 移植策略);改动四份文档。**另有两处模型自查纠正**:① 寄存器余量先估成"剩 12 个",**去数 `case` 分支后实际剩 10 个**(22 已用);② 由此**撤回了"`ADDR_WIDTH` 7→8"的倾向** —— 7 是已实测通过的边界,升位要动 `.xci` 的 `port_maps` 快照与 `Refresh Module Reference`,**是有代价的动作**。顺带办结陈欠 `AD9226` → **`AD9238`**(四份文档) | ⚠️ 部分可用(**规格与取舍论证可用,每个关键数字都给了推导;但本轮零代码 / 零仿真 / 零上板** —— 7 个寄存器**尚未写进 RTL**,`lcd_fb_reader`/`lcd_vblank_ctrl` **一行未写**,风险 **D1–D6 全部未实测**;带宽与时序余量均为**算术非实测**,已在记录内显式标注) | 新候选 `skill/hidden-premise-in-optimization/` + `skill/verified-boundary-is-costly-to-move/` + 扩展 `skill/no-all-negative-from-shallow-search/`(**同名目录并存时先看时间戳**)+ 扩展 `skill/zero-cost-elimination-first/`(**要进交付物的数字先回源数一遍**) |
| [LLM-030](LLM-030.md) | 2026-10-02 | Claude | **PL 侧显示通路 Chunk A —— `lcd_timing.v` 新建 + 7 个显示寄存器落地**。用户明确授权「按照你推荐的方式对 pl 端修改」(§6.0 例外),模型动手改 RTL:新建自研 `lcd_timing.v`(取代厂商 `lcd_driver` 的时序角色,输出级搬到 `cv_sys` 五行 assign)、`cv_axi_lite_slave.v` 加 7 个寄存器 `0x58`~`0x70` 共 11 个、`cv_sys.v` 换件并补端口。**两条读源码的发现**:厂商 `pixel_ypos <= v_cnt + 11'b1 - ...` 有 **off-by-one**(y ∈ 1..600,画彩条看不出、帧缓存取行会整体错一行);厂商 HS/VS **恒高**(纯 DE 模式,故本设计默认保持恒高、真实脉冲作参数备选而非默认)。**🔴 本次最有价值的一条是模型自己写的 bug**:`x_win` 上界误用 `H_ACT_END`(1184)而非 `H_REQ_END`(1182)⇒ **每行多发 2 个像素**(`px_x` 跑到 1025)。**它不会让屏幕坏**(多出的像素落在 DE 外被丢弃,彩条照旧正常),要到 Chunk B 帧缓存取行时才会以"右边缘 2 列串到下一行"发作 —— **靠临时 testbench 实跑抓到**(`[XY ERR] got (1024,0)`),非目测。另自查出**临时 TB 第一版断言本身有误**(复位在帧中间,却按整数帧断言)⇒ 确立"测试台报 FAIL 先怀疑测试台" | ⚠️ 部分可用(**`xvlog` 零 error/warning;`xelab -R` 实跑 2 帧 PASS** —— `de_req`=1228800=2×1024×600、坐标序列错误 0、`de_act` 相位错误 0、`line_start`=1200、行周期 1344 clk、帧周期 635 行、末行 y=599;**但零上板**,M13 要求的板上实测未做,`sts_*` 全接 0、`de_req`/`vblank`/`line_start`/`frame_start` 本批无消费者 ⇒ 综合会报 "no load"(**预期**,已在代码内写明)) | 新候选 `skill/right-constant-wrong-place/` + `skill/dont-fake-away-warnings/` + 扩展 `skill/verify-claims-not-intuition/`(**测试台报 FAIL 先怀疑测试台**)+ 扩展 `skill/verified-boundary-is-costly-to-move/`(**守着边界要把边界说出来**) |
| [LLM-031](LLM-031.md) | 2026-10-02 | Claude | **PL 侧显示通路 Chunk B —— `lcd_fb_reader` 代写 + BD 接 HP0 + 模型自己跑出上板位流**。用户授权「都按照你推荐的方案修改,然后自己用 vivado 跑出上板需要的文件,完成 ps 程序设计以外的其他工作」⇒ 模型推荐 **HP0(64 bit 直连 DDR)** 而非 GP0(32 bit 经 `axi_smc`),理由:一行 2 KB,64 bit 取一行 256 拍 vs 32 bit 512 拍,而行周期只有 1344 clk;**HP 被 DMA 占用的代价在本项目为零**(PS 侧不走 DMA)。新写 `lcd_fb_reader.v`(行取数 FSM + 乒乓行缓冲 + 预取 + AXI 读主机),`cv_sys.v` 加 41 根 `M_AXI` 端口,BD 开 HP0 64 bit 并接上。**三条纠错轨迹**:① `update_module_reference` 在 2025.2 批量模式下**四种写法全静默失败**(rc=1、错误信息为空),真因是 `qrs.gen/.../mref/cv_sys/component.xml` 是**上次开 BD 时的快照**(10月1 17:04)⇒ 缓存过期;**症状是"没有症状"**;② `ERROR: [BD 41-1285] The protocols of the interfaces '/cv_sys_0/M_AXI'(AXI4) and '/processing_system7_0/S_AXI_HP0'(AXI3) are incompatible` —— **Zynq-7000 的 HP/GP 就是 AXI3**(突发上限 16 拍),模型按 AXI4 写了 8 位 `arlen`;**在"插 `axi_protocol_converter`"与"改 RTL"之间选了后者**,因 HP 是实时通路、每行有硬截止期(消隐 320 clk),**用变延迟 IP 换十几个 LUT 不划算**;③ `xelab -R` 在编译失败时**回放上一轮的 PASS 输出**,靠"新加的那行 ARLEN 校验没出现"识破 | ⚠️ 部分可用(**仿真有实跑证据:5 帧、1,228,800 像素逐点比对 0 错、`underrun=0`、`axi_err=0`、`arlen` 恒 15、换址正确延到下一帧;实现后资源 830 LUT / 995 FF / **1 BRAM** / 30 IOB,WNS +12.985 ns 全过;位流已生成存档 md5。**但位流从未上板**、风险 **D1(PS 写 DDR→PL 读 cache 一致性)未验**、`arcache`/`arprot` 取值**无出处**、16 次串行突发的带宽**只有估算无实测**,且上述数字**不含测量主链**(ADC/SPI/DLIA 尚未进 BD)⇒ **不是 M3 的全设计口径**) | 扩展 `skill/stale-artifact-silent-reuse/`(**第三次发作**,前两次:LLM-012 旧程序、LLM-021 `.xpr` 残留)+ 新候选 `skill/axim-protocol-version-is-a-hard-gate/` + `skill/replayed-output-looks-like-success/` + 扩展 `skill/right-constant-wrong-place/` |
| [LLM-032](LLM-032.md) | 2026-10-02 | Claude | **PL 侧触摸通路 Chunk C —— `i2c_master` + `gt911_ctrl` 代写、接进 `cv_sys`、跑出含触摸的位流**。承第 30 次的「完成 ps 程序设计以外的其他工作」授权。新建开漏 I2C 主机(**字节级 op 接口**而非"读一个寄存器"的高层接口,因 GT911 事务形状不规整;含**时钟拉伸等待 + 超时自退**与 **9 脉冲总线恢复**)与 GT911 控制器(复位序列锁器件地址 0x14 / 16 位寻址 / 轮询 0x814E / 两级错误容忍 + 地址自愈)。`cv_sys.v` 加 4 个外部端口 —— **三个三态口必须保持 `inout` 原样引出**(拆成 `o/oe/i` 会变 3 个外部端口,而物理上各只有 1 个引脚),且 **`touch_int` 是双向的**(GT911 用复位释放瞬间的电平锁存器件地址)。**本轮唯一的设计判断**:`cv_axi_lite_slave.v` 加**触摸坐标快照锁存** —— PS 要读满 3 个寄存器才拼得出一次触摸事件,而控制器自由运行,三次读跨帧边界会读出**撕裂的 x/y**(撕裂**不报错**,只让界面偶尔跳);做法是把"读 `0x68`"当**提交点**,那一拍把 pt0/pt1 一并锁进 hold,**代价是新增前提「PS 必须先读 0x68」**。**三条纠错轨迹**:① RTL 真 bug 4 个(**编译期零告警**,全由仿真检出,含**总线恢复收尾发伪 START**(SCL 高时拉低 SDA 是 START 不是 STOP)、**明确松手那帧没清状态寄存器**⇒ `bit7` 恒 1 ⇒ 每次轮询重报同帧 ⇒ `seq` 疯跑 ⇒ PS 收到连续假释放、**坐标拼接静默截断 18→12 bit**);② 验证台自己的坑 5 个(从机驱动"解码后的辅助 wire"而非真实三态线 ⇒ 主机永远看到 NACK;**TB 漏了上拉电阻** ⇒ `if (z)` 判假 ⇒ 所有事务等满超时 —— **这是 TB 缺了板的模型,却坐实了"板上 I2C 必须有上拉"这条硬件前提**);③ **工程配置两处** —— 🔴 **约束写进了 `IS_ENABLED=0` 的 `io.xdc`**,综合实现全程没 Parsing 过它,位流在 `[DRC NSTD-1]`/`[DRC UCIO-1]` 处炸掉(真正生效的是同目录 `led.xdc`);🔴 **`qrs.xpr` 里一条 `<MsgRule>` 把 `CRITICAL WARNING` 整体降级成 `INFO`** ⇒ **本项目过去任何"0 critical warnings"的结论都不可信** —— 而这正是 M3 要交的东西;已 `reset_msg_config` 清除后重跑确认 | ⚠️ 部分可用(**两个临时 TB 有实跑证据**:i2c 带独立总线监视器 —— 组帧正确、**时钟拉伸 200 拍实测 +176 拍**、死拉低超时自退不挂死、总线恢复后 `z/z`;gt911 **18 项检查全过**;**全量非增量**综合实现位流全通:LUT **1133(2.13%)** / FF **1380(1.30%)** / RAMB36 1 / DSP 0 / **IOB 34 = 30 LCD + 4 触摸**,**WNS +12.746 / TNS 0.000 / 失败端点 0**,**DRC 0 Error 0 Critical Warning**,CDC 空;4 根引脚已在布线后网表核对 R19/P20/U19/M19。**但位流从未上板**(M13 对本批无证据)、真实 GT911 是否接受 `0x814F` 起始偏移未知、**复位时序抄自前序 STM32 不是数据手册**、**外部上拉是推断不是实测**、`clk_axi` 按 50 MHz 设计值算 ⇒ 实测 41.6667 MHz 时所有"毫秒"**实际慢 20%**) | 新候选 `skill/open-drain-bus-both-sides-same-net/` + `skill/simulate-open-drain-needs-pullup/` + `skill/xdc-file-may-be-disabled/` + `skill/check-msg-severity-overrides/` + `skill/wide-concat-silently-truncates/` |
| [LLM-033](LLM-033.md) | 2026-10-02 | Claude | **PS 侧屏幕界面初始化 —— 固化 FCLK0 + Python 驱动层 + LVGL C 移植**。**PS 侧第一次动手写代码**(前 32 轮全在 PL)。用户点名「**ps端帮我完成屏幕的界面初始化先**」(§6.0 例外),并选定「直接上 LVGL C」/「最小闭环」/「两边都写」。产出两半:① `src/python/` 四层 —— `cv_regs.py`(唯一事实源)→ `cv_monitor.py`(唯一 mmio 层)→ `drivers/{lcd,touch,board,ui_process}.py`(每模块一个驱动库)→ 薄 `main.py` → `tools/*.py` 自检,并固化 `Clocks.fclk0_mhz = 50`(**结案 F1**);② `src/c_ui/` —— LVGL 8.2.0 + 前序 `ui_demo.c` 全家桶**逐字搬**(9 文件 md5 实测一致),靠 `port/BSP/...` **路径别名树**让 `ui_demo.c` **一行不改**就编过,新写的只有 `app/` 下 7 个文件。**架构判断**:帧缓存**必须**由 Python 分配(CMA)并把**物理地址**交给 C 进程 `/dev/mem` mmap —— 该映射**非缓存**,PL 读 DDR 直接看到写,**顺带绕开风险 D1**;C 侧拿不到物理连续内存,所以这不是风格选择而是唯一路径。**纠错轨迹**:① 🔴 **最有价值的一条是模型自查出的设计缺陷** —— `pynq_touch.c` 初稿把 PL 的 **`err` 粘滞位当成了实时故障信号**,而它**只有重配 PL 才清**(`gt911_ctrl.v:264`),后果是**一次上电噪声 ⇒ 触摸永久失效**,且症状(按了没反应)会把排查引到上拉/底板上;改用只报一次的闩锁。② 🔴 **计划里「`docs/display_subsystem.md` §5.2 写错了」这条断言本身是错的** —— 照它去改时发现 §5.2 **根本不提 `0x50`**;真正过时的是 `cv_axi_lite_slave.v:10`/`:140` 与 `report/source_design_checklist.md:408`(**权威是消费者 `cv_sys.v:289`**,那边早就写对了)⇒「文档哪一处写错了」也是断言,**动手前要回源核**。③ 🔴 **差点把编造的 md5 写进证据表** —— `README.md` 的 `ui_demo.h` 一行先填了占位串,当场截住、实测补真值,并把 9 个文件**用 `diff -q` 全量复核**(不是只核那几个有 md5 的)。④ `-std=c11` 编不过(`clock_gettime`/`nanosleep` 隐式声明)⇒ 定死 `-std=gnu11`,**靠真编一次抓到**。⑤ 前序 include 路径:最初倾向改 `ui_demo.c` 几行,**改选 `-Iport` 路径别名** —— 能编过有两条路,**选那条不给报告留债的**。⑥ 本轮唯一动到的 RTL 是 **2 处注释**(`cv_axi_lite_slave.v`),不影响逻辑、不必重新综合;`cv_sys.v:287`「位 0 含义自 v2 起从未变过」一句**判不了对错,未改,留待用户裁定**。⑦ **实测出交付仓库 RTL 落后整整一代**(只有 5 个文件、缺 10 个;`board/qrs.bit` 还是第 27 次的 `92b7edd6…`,工程侧已是 `ad93f160…`)—— **未同步,按 §6.0.1 由用户执行** | ⚠️ 部分可用(**零上板** —— `c_ui` 从未在板上编译过、`main.py` 从未在板上跑过,**M13 对本轮无证据**。已实测:PS Python 全部 `py_compile` 过 / 触摸位域对 `gt911_ctrl.v:278-280` 逐位核 20+ 条断言全过 / 64 KB 对齐 6 个基址边界全过 / 测试图逐像素核对 / **`make` 在 PC 上 exit 0 且自有代码 0 warning(含前序 5367 行 `ui_demo.c` 一行未改就编过)** / `c_ui` 各条参数出错路径退出码符合契约 / 前序 9 个文件 `diff -q` 全一致。**未验**:整条显示触摸通路仍未上板、风险 **R1**(`CONFIG_STRICT_DEVMEM` 未查)、**R2**(非缓存 1.2 MB 同步耗时**只有估算无实测**)、`ui_demo.c` 在 PYNQ 上的**运行时行为完全未知**) | 新候选 `skill/sticky-error-bit-is-not-live-fault/` + `skill/plan-claim-is-also-a-claim/` + `skill/never-placeholder-into-evidence-table/` + `skill/compile-the-vendor-code-unmodified/` + 扩展 `skill/measurement-instrument-can-be-wrong/`(**本轮测试脚本自己又错 3 次**) + 扩展 `skill/no-all-negative-from-shallow-search/` |
| [LLM-034](LLM-034.md) | 2026-10-03 | Claude | **DDR 帧缓存全黑排障 —— 推翻「41.6667 MHz」、揪出 HP0 全量 DECERR、根因未找到**。承第 30 次授权自主推进,中途两次用 `AskUserQuestion` 交用户目视判读(均答「全黑」)。**① 时钟口径被推翻**:2026-10-01 存档的「IO PLL = 1500 / FCLK0 实测 41.6667 MHz」**两条全错** —— 41.6667 是 PYNQ 按**假定的** 1500 做的除法,**从来不是任何仪表的读数**;当时列的"三条相互独立证据"实为**同一条**(一个内部假定 + 一次写操作)。**首次用帧率法(独立时基)实测**:IO PLL **1000 MHz**、开机默认 **27.78 MHz**(低于面板窗口下限)、`apply_fclk0()` 后 **50.000 MHz**(帧率法复测 50.0594,−0.12%);顺带结案遗留决策 **F2** —— 改频**必须在 `Overlay()` 之后**(`Overlay()` 把 `FPGA0_CLK_CTRL` 整个复位回 `BOOT.BIN` 的值)。**② 全黑收敛**:把 `0x74` 由"纯拍数"改成"低 18 位拍数 + 高 14 位**内容证据**"后,**一次实测**分开两个假设 —— `rresp_lat=11(DECERR)` / `axi_or_nz=0`(数据全零)/ `px_or_nz=0`,而 `beats` **满帧**;9 点地址扫描(含 `0x00000000` OCM 与越界 `0x40000000`)**无一例外** ⇒ **拒绝在端口层面,与地址译码无关**;十条假设逐条连证据排除,**PL 侧全部洗清**。**③ 纠错轨迹**:🟢 **「证据条数 ≠ 证据强度,独立性才是」**(三条同源被写成三条独立);🟢 **「握手 ≠ 数据」** —— DECERR 回全零而拍数照样满帧,第 33 次"拍数满帧 ⇒ 通路正常"的判读被证伪;🟢 模型曾写 SLCR `FPGA_RST_CTRL` **把板子写挂死**(教训已成 memory 与 `board.py` 硬护栏,**本轮首次写进记录**);🟡 `KeyError: 'source'`(假设了返回类型);🟡 拿板子路径在本地 `md5sum` 自伤;**④ 板子整理**:26 项 → 16 项、58M → 46M,两个调试工具从 `/tmp` 收进工作区并改名 | ⚠️ 部分可用(**本轮无任何新增功能**;**「把黑屏从 5 个候选假设收敛到 1 个端口」达成,但根因未找到**。实测证据:位流 md5 `f45cb175…` 板上/工程侧一致、`.hwh` `4fa2032b…` 一致、WNS **+12.263** / WHS **+0.117**、DRC 0 Error 0 Critical Warning;`tools/lcd_ddr_debug.py` 300 次采样 3 工况、`tools/lcd_fb_sweep.py` 9 点扫描、帧率法三点独立。**未验 / 未解释**:`S_AXI_HP0` 为何全拒(**无 ILA、无第二观测者**,全部读数来自 PL 内部计数器与 PS 寄存器读)、`afi0_rddatafifo_level=26` 与"全拒"矛盾、HP0 通路**是否曾经工作过无法判定**、写通道从未被激励、`FPGA_RST_CTRL` 读回干净**不等于**硬件已复原) | 扩展 `skill/measurement-instrument-can-be-wrong/`(**"读数"可以是按错误假定做的除法,且三次交叉验证同源,越验越像真的**)+ 扩展 `skill/verify-claims-not-intuition/`(**证据条数 ≠ 强度,独立性才是**;**握手 ≠ 数据**)+ 扩展 `skill/no-all-negative-from-shallow-search/`(十条"排除"中**第 6 条自我标注"保留"**)+ 新候选 `skill/counter-cannot-separate-content-from-count/` + 新候选 `skill/port-level-reject-is-not-address-decoding/`(待根因确认后再定稿) |
| [LLM-035](LLM-035.md) | 2026-10-03 | Claude | **HP0 DECERR 第二轮 —— 用厂商已验证位流做对照,把根因从 PS7 赶回 PL 侧**。承第 34 次授权自主推进,**本轮无新增用户提示词**。四步全有实测支撑:① **AFI 假设作废** —— 我方位流 vs 官方位流烧同一块板,`0xF8008000/4/10/24`+`0xF8000240` **五个读数逐条相同**,「位流改了 AFI 状态」不成立;② 🔴 **推论:AFI 不可能是根因** —— 厂商 `ps7_init.c`(7450 行)对 `0xF8008xxx` **一次写都没有**,而**位流无法写 PS 寄存器**⇒ 任何位流烧进来 AFI 都停在同一默认值 ⇒ 厂商那份能工作则默认值可工作 ⇒ DECERR 不可能来自 AFI 配错(这一条把上轮方向表砍掉一半);③ **PS7 参数全量比对:HP0 一处不差** —— 两份 hwh 各 979 键逐键比,不同 37 个**全部**是 DDR PHY 训练 / `FCLK_CLK0_BUF` / `GP0` 使用 / `FTM_CTI`,`S_AXI_HP0` 相关 **0 处不同**;④ 🆕 **唯一找到的拓扑差异** —— 官方 `axi4_rw_test/M_AXI → axi_smc → S_AXI_HP0` **过了 SmartConnect**,我方 `cv_sys_0/M_AXI → S_AXI_HP0` 是**直连**(厂商主机是 AXI4/32 bit/ID 1,必须过互联;我方自述 AXI3/64 bit/ID 0/`MAX_BURST_LENGTH 16`,IPI 判定可直连)。**四条纠错轨迹**:① 🔴 **上轮「十条已排除」里第 8/9 条其实没有排除力,本轮自己撤了** —— 同一条位流两个会话读同一地址,存档记 `0xF8008024=0x00000F00`/`0xF8008010=0x0000001A`,复测读到 `0x00000000`/`0x00000007` ⇒ **读的是当时的活动状态不是配置**,连带的「解读」列也是过度解读(结论恰好仍成立,但**理由要换**成"官方位流也是 0"这个对照);② 🔴 **上轮结论方向是反的** —— 上轮写"收敛到 PS7 的 `S_AXI_HP0` 端口本身",本轮推翻,而推翻靠的**不是重新推理,是多找一个对照物**(厂商位流上轮就在手边,却只比了 6 个键没烧进去读);③ 🟡 **一次「比参数」止步太早** —— 比 6 个键说"配得一样"证据强度不够,全量比过才敢说"**答案不在参数表里**";④ 🟢 **唯一正面的方法:拿自描述数据当判读指纹,不拿 LED 当判读**(人眼看 LED 再转述是一层不可靠转述)—— 厂商 RTL 写的是自增数 `1…16` 到 `0x10000000`,基线实测是 Linux DTB(`0xEDFE0DD0 …`),按完键再读即可判决 | ⚠️ 部分可用(**本轮无任何新增功能**;**「根因不在 PS7 配置里」这条推论成立且有实测支撑,但根因仍未找到**,只剩「PL 侧 / 板子本身」两条;⚠️ **判决实验还没跑** —— 需按一次物理按键 `PL_KEY0`,模型做不了,已把手动动作交给用户;⚠️ **仍无独立观测者** —— 工程里的 `ila_0` 只探 SPI 引脚且**在已实现的 BD 里没有例化**,抓不到 AXI,上轮 §八 那条限制**未被打破**,所有读数仍来自 PL 内部计数器与 PS 寄存器读;⚠️ 直连-vs-SmartConnect **只是唯一找到的差异,不是已证实的根因**;⚠️ **断电重启至今未做**) | 扩展 `skill/measurement-instrument-can-be-wrong/`(**把"活动状态寄存器"当"配置寄存器"读**,且**同条件复读才发现不稳定**)+ 扩展 `skill/verify-claims-not-intuition/`(判据寄存器必须先验读数稳定性;**"参数表全量相同"本身就是一条有用结论**)+ 扩展 `skill/port-level-reject-is-not-address-decoding/`(仍**未定稿**,继续只有现象没有机理)+ 新候选 `skill/reference-design-beats-parameter-diff/`(有"已知能工作"的参照物时**逐点对读**优先于**逐参数对表**;真正的差异在参数表里根本不存在) |

| [LLM-036](LLM-036.md) | 2026-10-03 | Claude | **HP0 DECERR 根因坐实 —— 并在同一天推翻我自己的"推翻"**。承第 35 次授权自主推进(本轮中段新增用户决策一条:岔路口答「就按照你认为最好的方案改」)。**结论**:`assign m_axi_arprot = 3'h0;` —— **`ARPROT[1]`(NS 非安全位)置 1 ⇒ `S_AXI_HP0` 对每个读事务回 `DECERR` 且数据恒零 ⇒ 帧缓存整屏全黑**;`ARCACHE` **完全无关**。**六步实测**:① 冷启动(boot `1`,`uptime` 43.43 s)第一枪即 DECERR ⇒ 否掉"重启能恢复";② 按 `PL_KEY0` 让厂商例程跑一次,**用独立观测者确认写成功** —— 厂商 RTL 写自增数到 `0x10000000`,PS 侧只读回读,按键前是 Linux DTB(`0xEDFE0DD0 …`)、按后是 `1…16` ⇒ 紧接着加载我方位流**仍 DECERR**,「厂商成功事务能清故障」也否掉;③ **交替加载 A/B/C**(`a963(000)`→有数据 / `298c(010)`→全零 / `a963(000)`→有数据)⇒ **结果跟位流内容走、不跟板子状态走**,步 A 之前板子**正躺在 DECERR 态里**,换位流即恢复;④ **EXP-A** 只动 `arprot`(`0011`/`000`)⇒ 有数据;⑤ **EXP-B** 只动 `arcache`(`0010`/`010`)⇒ DECERR;⑥ 终值 `0010`/`000` ⇒ `rresp=OKAY`/`axi_or_nz=1`,**用户目视确认图案三元素全对**。**2×2 四格全测,每格都先 `write_verilog` 核网表再上板**。**显示通路端到端打通(M13 证据到手)**,两个独立观测者(硬件寄存器 + 人眼)一致;**红横带顺带证明无 R/B 交换**。**纠错轨迹**:① 🔴🔴 **本轮推翻的是上一轮写的"推翻"** —— 其唯一依据 `cdfbe0f5` 那行的取值**只核过源码、没核网表**,位流已被覆盖**无法复查**;而上一轮**我刚在同一篇文档里写下「md5 不是改动是否生效的合格判据」**,转身又拿源码当"位流里是什么"的判据 —— **同一个坑的两个方向**;② 🔴 **把一条正面证据读成了反面证据** ——「重启后仍故障」当时被当作「与位流无关」,实则**恰恰是「故障与位流有关」的正面证据**(根因写在位流里,重启当然不能恢复);③ 🟡 用户交回技术判断权时更要写清「为什么这么选」(B06 要的就是这个);④ 🟢 **给用户当观测者时要给可判读的图案** —— 三元素图案(整屏绿 + 中间红横带 + 左上白块)一条信息同时否掉"行内偏移"假设,"亮/不亮"做不到;⑤ 🟢 取值要有出处并**标明出处的强度** —— 本取值属「与已验证参照物对齐」,**不是**「已知原理」 | ✅ **直接可用**(根因坐实 + 位流修正 + 显示通路打通。位流 `0e4cb57c…`,已装 `/home/xilinx/qrs.bit` 与工作区 `board/qrs.bit`;四份对照位流 `a9636e0d`/`62c0db66`(EXP-A)/`0b9d1b82`(EXP-B)/`298c8f1f` 均有网表核对 + 板上读数。**未定 / 遗留**:`ARPROT[1]` 导致 DECERR 的**机理无权威出处**(本机无 UG585/TRM),取值属"与参照物对齐";`ARPROT=000` 时**刚加载位流后首帧曾读到 `SLVERR`**、静置后复读为 `OKAY`,未解释;`beats` 对本实验**无区分力**(四格全 ≈100%),`rresp_lat` 会随加载/稳态变化,**只有 `axi_or_nz` 8/8 一致**;仍**无 ILA 抓 AXI**(`ila_0` 只探 SPI 且未例化)) | 新候选 `skill/rtl-constant-experiment-must-verify-netlist/`(**源码不是证据,综合后网表才是**)+ 扩展 `skill/measurement-instrument-can-be-wrong/`(**判据字段要按"能不能区分两种情况"选**)+ 扩展 `skill/port-level-reject-is-not-address-decoding/`(**本轮可定稿**:现象是端口层面全量拒答,根因是**事务属性位**不是地址译码)+ 新候选 `skill/observer-pattern-beats-observer-led/` |
| [LLM-037](LLM-037.md) | 2026-10-03 | Claude | **「画面右移、按键要往左点」排障 —— 模型两次拿噪声当结论,被用户叫停**。把症状拆成 δ_display / δ_touch 两项(症状完全相同,必须靠绝对参考才能分开);先画 5 根已知竖条让用户逐条点中心反推 δ,**得到的 δ 单调递减看着像 1.4% 欠标定 —— 随即自己用"同一根蓝条上一轮 +10.5、这一轮 +0.5"推翻:手指瞄准重复性 ±10 px 与效应同量级,该量具无分辨力**;第二个错误是"边缘 8 px 白条测试"只排得掉 δ≥8 却被当成 δ≈0 的证明。**用户叫停**(「这完全没达到…程度…这样测没什么意义」)后换量具:**用 UI 自己的布局当尺子** —— 首页两卡各 453×156、中间仅 22 px 缝,按缝正中看哪张卡进按下态,一次按压得 **\|δ\| ≤ 11 px**。另坐实两条结构结论:`lcd_hs` 恒高 + DE 模式 ⇒ **画面横向平移无物理通路**;GT911 不下载配置表、RTL/PS/LVGL 全链路零缩放 ⇒ **纯直通**。收尾做开机自启动(systemd,`Restart=no`)+ 环境清理。新候选 `skill/use-layout-as-ruler/`、`skill/state-resolution-before-measuring/` |
| [LLM-038](LLM-038.md) | 2026-10-03 | Claude | **开机自启动端到端验证 + 「感觉开机慢了好多」的量化否证**。① 上一轮留下的 `qrs-ui.service` **从未真启动过一次**(在活跃 PL 上手动起 = 第二次加载位流 = 挂死路径)⇒ **真验证只能靠断电重启**;用户主动问「要我断电重启试试吗」,断电后 `Main PID 546 main.py` + `630 c_ui` 自动起、界面正常、`clk_axi` = 50.000 MHz ⇒ **第 36 次遗留项清零**。② **「慢了好多」不猜,先量**:板子 RTC 会跳(读数 2021-11-17 且两次读之间会变)⇒ 墙钟不可比,**改用单调时钟** `journalctl -b N -o short-monotonic` 逐节点对照 boot -1(无自启动)vs boot 0(有自启动):Basic System 13.93→**12.07** / Network Online 28.33→**25.18** / sshd 29.29→**26.39** / Multi-User 30.81→**28.30**,`Network Online → sshd` 两次都是 **0.96 s**,界面服务**不在 `systemd-analyze blame` 里**、不在关键链上 ⇒ **实测没有变慢**。真凶是原有的 `jupyter.service` **31.78 s** 与 `networking.service` **13.67 s**(**DHCP 超时**:直连对端无 DHCP 服务器,而 `/etc/network/interfaces.d/eth0` 写的是 `iface eth0 inet dhcp`)。**三条纠错轨迹**:🔴 **对照表里唯一的反例行差点被"总体更快"盖掉** —— `jupyter` 晚了 **+2.7 s**,与"与界面服务无关"的其余五行方向相反,如实保留在表内并标"唯一反例 / n=1 未定论"(疑为 `main.py` 与 jupyter 抢 CPU);🔴 **不能拿"实测没慢"去否掉用户的"感觉慢"** —— 答案拆两层:**是不是变慢了**(可测,否掉了)与**为什么感觉慢了**(界面 25.4 s 才起、首帧再晚几秒 ⇒ **上电后屏约 28~30 s 是空的**,以前不盯屏现在盯了,**感知变化有具体成因**),这是 `LLM-037` 同一道理的反面:那轮是拿**分辨力不足的量具**否掉现象,这轮是用**分辨力足够的量具**否掉退化但**必须同时承认现象本身**;🟡 **遗留项不该埋在文档里等用户来推** —— "需要一个模型做不了的物理动作(断电/按键/目视)"的待办**要单列进任务清单**,本轮是用户主动提才推动的。🟢 **顺带查出两条优化,记录在案但都没做**:去掉 `After=network.target`(界面预计早 **~13 s** 出,改一行零风险,需再断电验证)、改 `eth0` 的 dhcp(**整机省 13.7 s**,归用户)。新候选 `skill/perceived-slowness-needs-two-answers/`、`skill/physical-action-todos-go-in-the-list/` | ✅ 直接可用(自启动端到端验证通过 + 「慢」被量化否证;遗留:jupyter **+2.7 s** 唯一反例、n=1 未定论) | 新候选 `skill/perceived-slowness-needs-two-answers/`(**感知类抱怨分两层回答:实测有无退化 + 体验为何改变**) + 新候选 `skill/physical-action-todos-go-in-the-list/`(**需断电/按键/目视的待办要单列进清单**) + 扩展 `skill/measurement-instrument-can-be-wrong/`(**量具没问题时,也要分清"测得出的"与"测得出但解释不了的"**) |
| [LLM-039](LLM-039.md) | 2026-10-03 | Claude | **技能包首次落地 —— 10 项 skill 一次写完,并自查出三处引用编号错挂**。用户点名授权「**你先自己把几个skill都写了**」(§6.0 明确授权;写的是 markdown 文档,属左列「我可以做」,**未生成任何代码**)。从本项目 38 条真实记录里挑出**素材最厚的 10 个母题**,按 `skill/README.md` 已定死的**五要素模板**(适用范围 / 使用方法 / 失效条件 / 已验证的效果 / 来源)逐个撰写。**产出**:3 项**通用 PYNQ 技能**(`pynq-overlay-bringup` / `pynq-register-map-pattern` / `pynq-ddr-framebuffer-display`,对应加分项 **B1**)+ 7 项项目技能(`verilog-review-checklist` / `spi-tb-patterns` / `measurement-instrument-can-be-wrong` / `stale-artifact-silent-reuse` / `rtl-constant-experiment-must-verify-netlist` / `use-layout-as-ruler` / `gbk-example-porting`);同时**重写 `skill/README.md`** —— 由「计划清单」改为**已落地索引**,并新增「⏸ 计划中但未落地」一节逐条写明卡点。实测 `find skill -type f` → **11 个文件**(10 × `skill.md` + `README.md`),**1634 行**。**纠错轨迹**:① 🔴 **三处引用编号错挂,模型自己查出来的** —— 初稿写完后做了一次**反查**(把每条 `skill.md` 里的 `LLM-0XX` 全抽出来回头核原文),发现 `verilog-review-checklist` 写的 `LLM-029`(实为界面路线修订)应为 **`LLM-025`**;`pynq-overlay-bringup` 写的 `LLM-033`(实为 PS 侧界面初始化)应为 **`LLM-034`**;`pynq-register-map-pattern` 写的 `LLM-030` 应为 **`LLM-027`/`LLM-029`**。发现方式靠 `head -1 LLM-*.md` 拉标题 + `grep -ln '<关键词>'` 反查,**不是靠印象**;三处均用 `Edit` 做最小 diff 改掉。⇒ **错挂的编号等于伪造引用**(§6.2 红线),评委顺着编号查到的是**另一件事**,整条「来源」的证明力当场归零 —— **引用编号属于断言,写完必须回源核**。② 🟢 **主动不写 6 项,不拿空壳充数** —— `hls-ap-none-rtl-integration`(HLS 两个核一行没写)/ `vivado-input-delay-calc`(`clk_adc` 频率至今未定)/ `cic-coherent-sampling`(只有纸面推导)/ `prompt-hls-pipeline` / `prompt-timing-triage`(没有**实际用过**的提示词)/ `golden-compare-script`(未沉淀成流程)。M6 要求的「已验证的效果」与「从哪些失败中总结得出」**编不出来**;写空壳**比不写更糟** ⇒ 连同**卡点**写进 README 未落地表,**让缺口可见而不是让缺口隐形**。③ 🟡 **归属逐条分清,不得混称** —— README 末尾加**归属声明**:`sim/tb_*.v` 是 §6.0 显式例外下模型代写,而 `spi_master.v` / `dac_1220.v` / `swm.v` 是**用户自写**;混称会让报告的「大模型协作」失真,且**翻 commit 就能查出来**。④ ⚠️ **本轮刻意没有新增任何技术结论** —— 写技能包时最容易滑向「顺手把某条经验推广成通用规律」,10 项里每条的「已验证的效果」列的都是**被引用记录里已经发生过的那次验证**;唯一新增的是**元级观察**:素材厚度不均(`measurement-instrument-can-be-wrong` 有 **8 次**事故可引,`use-layout-as-ruler` 只有 **1 次**)⇒ 阅读建议按厚度排了序。⑤ ⚠️ **B1 判据自查查出 2 处命中,结论仍成立但改掉了「零命中」的说法** —— `pynq-ddr-framebuffer-display` 里 `cv_sys` 出现在**逐字引用的 Vivado 报错原文**中(换成自己的实例名即可,**不构成依赖**)、`LVGL` 是**通用图形库**;核心理由是**技能内容本身**(带宽预算 / AXI3 硬门 / `ARPROT` / cache / 防撕裂)换任何 Zynq 板 + 并行 RGB 屏都成立,**与「测 MOSFET 电容」毫无关系**。⇒ **「判据成立」与「证据干净」是两件事**:前者可带保留地成立,后者**只有做过了才知道** | ⚠️ 部分可用(**技能包内容完整、每条都追得到源头;但两个未闭合项**:① 🟢 **交付仓库侧 —— 同日已闭合**。撰写当日实测 `~/Desktop/qrs_fpga_2026/skill/README.md` **0 字节**(目录自 2026-09-22 建起从未填过),当时记为未完成;🔴 **但该状态在当天就被推翻**:用户点名授权(§6.0.1)后,技能包连同 **PL/PS 全量源码 + 当前位流**一并同步进交付仓库,存为 commit **`352cf69`**(406 files / 298,443 insertions),**并由用户推送成功** —— `ba76d27..352cf69  master -> master`(快进未 force)。📌 **推送不是模型做的**:模型 `git push` 报 `could not read Username for 'https://gitee.com'`(本机无 Gitee 凭据),只给出了两条路径,执行归用户 ⇒ 凡交付物状态分**已提交 / 已推送 / 已生效**三档写,不得混称;② **10 项里没有一项有「陌生队伍从零复现」的实测** —— B5 要的是别人照着跑通,本轮只做到「本队自己可追溯」。**本轮验证全部是文档级**:`find` 11 文件 / 五要素 grep **10/10 齐全** / 非 ASCII 文件名**零命中**(M14)/ 引用编号**逐条回源核过**。**本轮不产生上板证据** —— 技能包引用的上板证据(位流 md5 / 帧率法 / 板上寄存器读数)在**被引用的那些记录**里,不在本轮) | ✅ **M6 首次落地:10 项 skill 从 0 → 10** + `skill/README.md` 重写(总览 / 未落地卡点 / 撰写纪律 / 归属声明) |
| [LLM-040](LLM-040.md) | 2026-10-03 | Claude | **结果页拆成 5 个子页 + 首页按钮改四字 —— 并用「算出截断值再与屏幕读数逐位比对」定案一次 int16 溢出**。用户把上一版「结果页全展示在一起」判为**太臃肿**,重新界定为**单点结果 / 单点波形 / 扫压结果 / 扫压波形 / 导出(空占位)**五个子页,并要求首页入口**每个都是四个字**(「扫压」这类两字「不太好听」);另一条指令是「**自己生成一份测试数据我看看效果,板子还是一直上电你自行调试**」(§6.0 明确授权,范围限 `src/c_ui/` 界面与演示桩,**未碰 RTL / HLS / Python 测量链**)。**产出**:① 结果页 = 顶部页签 + 5 互斥子页,新写 `UI_CreateResultSubpage()`(子页**不能**用 `UI_CreatePageContainer()` —— 那是顶层页容器,自带背景会盖掉页签);**结果页不持有数据**,全部经 `UI_ResultsRefresh()` 投影 + `UI_ActivatePage()` 的「进入即刷新」钩子 ⇒ 「测完退出去再点进来就更新」自然成立;扫压结果页**复用扫压页的自绘对数刻度回调** `UI_SweepChartDrawEvent`(LVGL 整数刻度只会把 log-milli 原值印出来,第一版屏幕上就是一串与物理量无关的数字),`UI_ApplySweepViewport()` 改造成 `UI_ApplyChartViewport(chart)` 且**原函数降为一行包装、零调用点改动**;② `CV_UI_DEMO=1` 演示数据发生器(单调下降 MOSFET 电容模型 + `I=2πfCV` 波形),走**与将来 Python 侧完全相同的那组 `UI_Demo_Update*` 入口**,启动打醒目 stderr 警告、状态栏写「读数为编造」;③ 首页改 `系统复位 / 单点测量 / 扫压测量 / 结果汇总`。**纠错轨迹**:① 🔴 **第一次换新二进制后看的是旧进程的画面,追了一轮不存在的 bug** —— 截图显示页签停在「扫压波形」,我先怀疑 `g_result_tab` 初值 / 样式选择器 / `user_data` 索引错位,**全是猜测且全不成立**;定案靠**时间戳而非读代码**:板上二进制 mtime `15:12:36` 而 c_ui 进程启动于 `15:04:54`,**进程比二进制还老** ⇒ 屏上那帧不是新代码画的。根因是重启脚本里 `pkill -x c_ui` 需要 root,而我 **SSH 上漏了 `sudo -S`** ⇒ 杀不掉 root 旧进程、新 `main.py` 起不来(打不开 `/dev/mem`),而脚本的等待循环 `pgrep -x c_ui` **匹配到旧进程**于是「顺利通过」还打印出正常 pid —— **整个失败零报错**。判据:**换完二进制重启后先对 `ps -o lstart` 与二进制 mtime,顺序不对就什么都别解读** ⇒ `skill/stale-artifact-silent-reuse/` 的**第 5 种形态**(前四种:烧录器 / `.xpr` / BD mref / `xelab -R`,这次是「重启没生效」)。② 🔴 **`lv_coord_t` 是 int16,靠「反算截断值 + 多条刻度逐位吻合」定案** —— 包络页 Y 轴冒出一串现实中不存在的负值、曲线只剩两段竖线;我**拿证据否掉了三个假设**(量程没设 ✗ 源码里设了 / 点数超了写爆数组 ✗ `set_point_count` 与实际写入数一致 / 数据没到 ✗ 临时 `fprintf` 打出 `have=1 total=60 v=[0.100,100.000] i=[0.592,277.101]` **数据完全正确**),然后**换方向**:不再猜哪里错,而是反算「什么变换能把正确的量程变成屏幕上的数」—— 电压 `107992 → -23080`、电流 `299221 → -28459`,连同下端 `-7892` / `-21528` **四条刻度逐位吻合**;回查 `app/lv_conf.h:99` `LV_USE_LARGE_COORD = 0` ⇒ 坐标上限 ±32767。修复:每条轴各自的缩放系数 `g_wave_scale[2][2]`,保证最大值留在 ±32000 内。📌 **一个数对上可能是巧合,四个数同时对上才是定案** —— 比「我觉得是溢出」强,也比再加一轮打点快。③ 🟡 **缺字是静默的,两次都是靠眼睛发现** —— `lv_font_conv` 子集里没有的字符**渲染成空白,不报错不警告**:`Ω` 掉进「`0x20–0x7E` **或** `≥0x2E80`」的缝里(卡上成了 `RG 12.43`),`μ` 因生成字库在前而缺(`/ 10 A`),**同一个 `μ` 换个字体又没了** —— 单位标签原用 `lv_font_montserrat_16`,而 **Montserrat 本身没有 μ/Ω**。抽符号集规则改为「字符串字面量的全部非 ASCII」。④ 🟢 **主动交代自动化钩子** —— 为逐个页签截图新增 `CV_UI_RESULT_TAB`(默认 0,与 `UI_Demo_GotoPage()` 同性质,注释标明仅演示/截图用),并主动提出「你要是觉得多余我删掉」。⑤ ⚠️ **本轮零测量证据** —— 所有数字来自演示模型;收尾**已关演示模式**,板子回到「后端:未连接 / 系统就绪」的诚实状态,理由是不留一个「读数是编的」在板上免得下次误判测量链已通 | ⚠️ 部分可用(**五个子页 + 首页逐个上板截图验过**;但 **初始化页的设备状态块(继电器矩阵回路 / DAC / DDS)本轮未开始**,导出页按用户要求**只有界面**) | `skill/stale-artifact-silent-reuse/`(第 5 形态)+ 新候选 `skill/ceiling-truncation-fingerprint/` + `skill/font-subset-silent-blank/` + `skill/pynq-ddr-framebuffer-display/` |
| [LLM-041](LLM-041.md) | 2026-10-05 | Claude | **交付仓库整理与同步 —— 并暴露「用户说的"仓库"不是 git 仓库」与「同步方向要实测」**。用户点名授权写入交付仓库(`CLAUDE.md` §6.0.1 例外),要求「整理好同步到最新状态推送上去」。**核心动作不是搬文件,是动手前先实测差集**:`diff -rq` + 逐文件 `cmp` 的结果与直觉相反 —— 仓库 `src/rtl/` 有 **15 个 `.v`** 而工作区只有 **4 个**,且工作区那份 `swm.v`(1572 B / 09-26)是**修复前**版本(模块头缺 `);`、复位分支缺 `begin/end`),仓库那份(1569 B / 09-29)才是修好的;**若按「工作区 → 仓库」的惯性直接 `cp -r`,会把修好的文件覆盖回坏版本**。方向靠与 Vivado 工程 `qrs.srcs/sources_1/new/` 逐个 `cmp`(15/15 相同)定案。`README.md` 同理:仓库版 10-03 比工作区版 10-02 新,**未覆盖**。用户另拍板四件事:**LLM 记录在仓库内另建顶层文件夹**(`llm-log/`)、**LICENSE = MIT**(署名 `dsgmxc`)、`project_state.md` 与 `source_design_checklist.md` **都不进仓库**、M03 **只拷关键 4 份**。**最终入库 70 文件 / +37931 / −14230**(commit `25a4756`):`src/c_ui/` 界面批次 2+3(`ui_demo.c` 5367→**6293** 行)、三份规格文档、**7 份上板实测存档**、40 条 LLM 记录 + INDEX、4 份 `system_wrapper` `.rpt`、`tb_lcd_rgb_colorbar.v`、**LICENSE 0 字节 → MIT**、README 目录对照说明。推送 `352cf69..25a4756  master -> master` rc=0。**纠错轨迹**(详见记录内五节,全部为模型自查 + 一处用户纠正):① 🔴 **用户说的「仓库」不是 git 仓库** —— 原话「日志你再**另起一个仓库**吧」,我按 git 术语理解,追问围绕新建远端 Gitee 仓 / token / remote URL 展开,用户回「**我是说在项目仓库里再建一个文件夹不能建吗**」。**根因**:用户上一句落脚点是「report **拿来干什么**」(内容往哪放),上下文里没有「另一个 git 仓库」的位置 ⇒ **用户的词 ≠ 该词的术语含义**,追问应**用用户自己的语境复述确认**,而不是先按术语展开方案。📌 翻车在**提问**不在结论 —— 这个回合本可靠读上下文省掉(与 `LLM-024`「上下文假设」同类)。② 🟡 **搬目录打断 79 处交叉引用,处置上分成两类** —— `report/measurements/`→`board/measurements/`、`report/llm-log/`→`llm-log/` 造成 **37 处**旧路径引用;`docs/*.md` 是**活文档** ⇒ `sed` 改写(10 处,改完实测可用);`llm-log/` 与 `board/measurements/` 是**历史存档**,里面的路径是**当时的事实描述**(如 `LLM-026` 写「`report/measurements/` 内**仍无文件**」)⇒ **一个字都不改**,改了就是篡改记录(§6.2 红线);剩余 5 处指向未发布文档的**事实陈述**在 README 里**列表交代**而非删改原文。⇒ **移动目录前先 grep 引用,「导航链接」要修、「历史叙述」不能动** —— 本轮最可复用的产出。③ 🟡 **`cp` 源路径打错,报错与「成功」同时出现** —— 把 `qrs.runs` 误写成 `qrs.qrs.runs`,报错信息里出现了不存在的路径,但脚本**仍打印 `COPIED OK`**,两行输出互相矛盾、**无法从输出判断落地了几份**;我**没信这行输出**,改用逐个 `md5sum` 与源比对(**4/4 一致**)。📌 与 `LLM-040` 纠错 1(`SSH 重启漏 `sudo -S`,`pgrep` 匹配到旧进程于是"顺利通过"`)**同型**:都是**失败零报错**、都靠**换一个独立判据**看穿 ⇒ `skill/stale-artifact-silent-reuse/` 再补一条:**「脚本自报的成功」本身也是一种可能失效的判据**。④ 🟢 **凭据走临时通道但风险未消除** —— 密码取自 `~/Desktop/password.txt`,推送用临时 credential store(`umask 077` + `--file=/tmp/.gc_$$`)、用完即删,**未进命令行参数、未进 git 配置**;⚠️ 但密码**明文存桌面且权限 `-rw-rw-r--`**(组/其他可读),本轮**未擅自改动该文件**,建议改私人令牌 / SSH key 或至少 `chmod 600`。⑤ ⚠️ **两个 0 字节文件刻意未处置** —— `src/python/config.py` / `display.py` 存在于交付仓库却不在工作区,处于未跟踪态;它们**不是模型创建的**,处置权不在模型 ⇒ 本轮不加进提交,留给用户定夺 | ⚠️ 部分可用(**同步与推送全部完成并实测核验**;⚠️ M05 `build/*.tcl` 与 M03「优化前后对比表」仍未闭合 —— 本轮只搬现成报告,未生成新内容;⚠️ `config.py`/`display.py` 待用户处置) | 新候选 `skill/sync-direction-must-be-measured/` + 扩展 `skill/stale-artifact-silent-reuse/`(第 6 形态) |
| [LLM-042](LLM-042.md) | 2026-10-05 | Claude | **上板打通 DAC/DDS 通路 —— 付出 3 次断电,并挖出「模型自己编了一个寄存器位」**。用户授权(§6.0 例外,原话「你写完代码我给开发版上电你自行调试完成我验收」)把 `src/c_ui` 上板跑通,证明「界面 → 触摸 → c_ui(C)→ AXI → PL → `dac_1220` / `adc_9833`」整条链路真的动硬件。**核心产出不是"通了",是买来的一条硬规则与一次撤回**:① 三次挂机的**唯一根因** —— 裸读 `0x43C00000` 时 `mmap 返回 ✅` 后**静默 20 s、丢包 100%、只能断电**,即 GP0 口无超时、从机不应答 ⇒ **AXI 事务永不完成 ⇒ CPU 卡在总线上**。真因经**四条独立证据**坐实「每次上电 PL 都是空的」:`BOOT.BIN` 860396 B 内位流同步字 `aa995566` 出现 **0 次**、`dmesg` 只有 `FPGA manager framework registered` **无任何加载记录**、`fpga_manager/state = unknown`、`devcfg INT_STS = 0x00020000`(DONE 位为 0)。⇒ **新硬规则:一次上电只加载一次位流,寄存器访问必须排在那一次之后**。② 🔴 **模型编了一个寄存器位并据此反咬用户** —— 用 `devcfg STATUS`(0x14)**bit5** 当 `PCFG_DONE`,读到 1 就断言「PL 已配置」,进而推出「PL 里残留上一轮位流」,**要求用户"真正断电"、质疑他上次只按了复位键**。回查本机 `xdevcfg_hw.h`(devcfg_v3_9)确认:`STATUS` bit4 = `XDCFG_STATUS_PCFG_INIT_MASK`、**bit5 无定义(保留位)**、**DONE 是 `INT_STS`(0x0C) bit2 = `XDCFG_IXR_PCFG_DONE_MASK`** ⇒「残留位流」整条推理链不成立,**结论与对用户的质疑一并撤回**。错因不是算错而是**凭印象套了一个不适用于该寄存器的常见位定义**,而「规则类结论必须出示出处」本就写在 `CLAUDE.md` 里。③ FCLK0 补到 **50.0000 MHz**:先漏设 `DIV1` 只写 `DIV0` ⇒ 静默得到 8.33 MHz,**靠数场频(9.657 Hz,应 58.6)才抓到**;同写 `DIV0=20`/`DIV1=1` ⇒ `FPGA0_CLK_CTRL = 0x00101400`(与 `CLAUDE.md` §八 逐位一致)。帧率法独立复测 **58.606 Hz × 853440 px = 50.0170 MHz(差 0.03%)**;而中间那次错频实测 8.2414 MHz —— **若 IO PLL = 1500 应得 12.5 MHz(差 52%)⇒ 这是 IO PLL = 1000 的第 4 个独立确认点**。④ ⭐ **端到端结果(用户手点屏幕 + 后台 150 ms 轮询取证)**:`0x14` 62 次变化 = 2 次手动调偏压 + **60 点扫压(不同码值恰 60 个)**、`0x60 LCD_SWAP` **109 次 front 翻转**(双缓冲在真跑)、`0x68 TOUCH_STATUS` **68 次**(触摸在真跑)、`UNDERRUN/AXIERR` **0/0**。扫压 60 点按 `V = (−10 + 20·code/1048575) × 10` 换算后**逐点核对设计公式**:首/末 = `+0.1000 V` → `+5.0000 V`、**严格递增、重复点 0**、耗时 **18.0 s = 60 × 300 ms**;**逐点电压比值 mean `1.06855` 与理论 50^(1/59) = `1.06855` 吻合到小数点后 5 位**,最大单点偏差 **0.097%**(即 20 位码的量化台阶,无系统偏差),首/末步步长 `6.9 mV` / `320.8 mV` **相差 46.7×** ⇒ 证实**等比步进是匹配界面 log10(V) 横轴的有意设计**,且首点未塌(0.1 V 量化陷阱未复现)。**纠错轨迹**:除上述 ②③ 外,另有两个**环境坑被证明会伪装成硬件故障** —— `Overlay()` 报 `RuntimeError: No Devices Found` 的**根因是 `XILINX_XRT` 未导出**(`sudo -S` 不读 `/etc/profile.d`,与 `LLM-022` 的 `-E` 是同一问题的两种触发面);root 下 `import pynq` 抛 `UnicodeDecodeError`,崩在 `pkg_resources` 扫 `/usr/lib/python3/dist-packages/transitions-0.7.2.egg-info/PKG-INFO` 第 30722 字节(非 UTF-8)。⚠️ **本轮明确未证明的事**:`dac_1220` 输出端**真实电压**、`adc_9833` **真实波形**均**无万用表/示波器**验证 —— **寄存器回读只证明 AXI 通路**;`dac_init_done` 未导出为寄存器,其时序是**推算不是实测**;满量程 ±100 V 端点未测(实际最大到 +5.0 V);`0x68` 的 `err` 粘滞位全程为 1 但触摸功能正常,**成因未查** | ⚠️ 部分可用(**链路打通、数据自洽到 5 位有效数字**;但代价是 3 次断电 + 1 次模型造位,且**缺万用表级验证**) | **扩展** `skill/pynq-overlay-bringup/`(补 4 条硬规则:单次加载 / `INT_STS` 判 DONE / `DIV0`+`DIV1` 同写 / `No Devices Found` 的真根因) |
| [LLM-043](LLM-043.md) | 2026-10-05 | Claude | **DAC 输出恒 −10 V 的根因 ——「初始化跑在了错误的频率上」,而且这个 bug 的藏身处是我们自己写的注释**。用户原话「我设置偏压开始测量开始扫压都点了,`dac` 输出一直是 −10 V」+「dac输出才0到10v有什么不安全,修复代码,要重启跟我说」。**根因**:`dac_1220.v` 的复位图案常数(`13000/500/28500/500/45000/500` 拍)是**按 50 MHz 折**的,但该模块的 `clk` 接的是 **`s_axi_aclk` = PS FCLK0**,而板上 `BOOT.BIN` 把它留在 **27.7778 MHz**(2026-10-03 已实测)⇒ 三段高电平变成 **468 / 1026 / 1620 µs,全部超出手册双向窗口上界**(`[208.3,325.5] / [416.7,732.4] / [833.3,976.6] µs)⇒ 复位**根本不触发** ⇒ CMR 配置与自校准被忽略 ⇒ DIR 写入永不生效 ⇒ 输出停在芯片上电默认 DIR=0 = **−10 V**,而**寄存器回读一切正常**。09-19 那次是好的,因为旧顶层 `qrs.v` 用的是 `clk_wiz_0` 的固定 50 MHz —— 换到 `cv_sys` 后**时钟源变了,静默**。**为什么不能改常数**:把三段折成拍数,两个频率下的合法区间**三段全不重叠**(t16 `5786~9042` vs `10415~16275`;t18 `11575~20344` vs `20835~36620`;t19 `23147~27128` vs `41665~48830`)⇒ **不存在一组常数同时覆盖两个频率**。**修法**:新增 `0x84 CV_CTRL`(**复位值 0**),`dac_1220.rst_n` 由直连 `s_axi_aresetn` 改为 `s_axi_aresetn & cfg_dac_rst_n`;只有 Python 侧 `drivers/board.py::release_dac()` 能放行,而它**量出的 FCLK0 不在 `[46.1, 54.0] MHz` 就拒绝**(该窗口由三段双向窗口取交集反解,最紧的是 t19);`main.py` 在 `apply_fclk0()` 之后调用,且**刻意放在 `--no-clocks` 分支外**;C 侧**只读不写**(单写者原则),只在偏压码发出而尚未放行时打一条醒目警告。**顺带好处**:放行前写进 `0x14` 的偏压码**不会丢** —— `set_in` 只在 `NORMAL_WAIT` 锁存,被闸住时那一拍确实被丢掉,但寄存器值留着,放行后从机侧的上电脉冲会拿**当前** code 补发一次。**纠错轨迹(五次自我纠错)**:① 🔴 **最关键的一条 —— bug 的藏身处就是 `cv_sys.v` §7 我们自己写的那段注释**:它明确断言「运行时钟和设计时钟不一致**对这里的一次性上电握手没有影响**」,**断言是错的**(成立前提是「所有等待都是下界」,而三个窗口都有上界)⇒ 一整轮排查(引脚 / 位流 / 界面写没写进去 / AXI 通不通)**全部绕过了这个文件**,因为「注释里说过没事」;处置是就地改成 `🔴【2026-10-05 更正】`,**保留错误结论可见**而非删掉,并写清哪些等待频率无关、哪些不是。📌 **一条写进代码的「这里没问题」和一个 bug 一样会阻塞排查,而且更隐蔽 —— 它长得像已经查过了**(与 `LLM-040`「换完二进制先对时间戳」同型:没有证据的断言被当成了证据)。② 🟡 **`0x18` 已被占用,差点覆盖规格里的 `RANGE_SEL`** —— 新寄存器第一版选了 `0x18`,动手前查 `cv_regs.py` 发现 `RANGE_SEL: 0x18`,再查 `report/source_design_checklist.md` §3 第 406 行确认它是**规格写死的测量主链地址** ⇒ 改到 `0x84`。📌 规格文档是 M9 正文,**不能被实现便利性覆盖**。③ 🟡 **自己算错一次并写进注释** —— 第一版写 t16 的两个频率窗口「虽有交叠」,重算后**三段全不重叠**,改成三行表格,并补上「不存在一组常数同时覆盖两个频率」这个真正的论据。④ 🟡 **写了不存在的异常类 + 循环 import** —— `dac_release()` 第一版用了 `cv_monitor_error`(该模块只有 `NotImplementedError`)和 `from drivers import board`(会成环)⇒ 拆两层:窗口判断放 `drivers/board.py`,薄访问器留 `cv_monitor`。⑤ 🟡 **`strings \| grep` 返回 0,差点再判一次「重编失败」** —— 板上 `c_ui` 重建后 `strings c_ui \| grep -c "仍被按在复位态"` 得 **0**,而上一次的 clock skew 失败刚发生过,很容易顺势判失败;改用 `grep -a` 直接搜二进制**得 1**。根因:`strings` 在 C locale 下按**单字节** `isprint` 判定,**UTF-8 中文被逐字节切断**,整串永远匹配不上(⇒ `skill/stale-artifact-silent-reuse/` 第 7 形态:**工具自报的结果也可能是失效判据**)。**顺带记三个环境坑**:板上 RTC **慢约 2 天** ⇒ 源文件 mtime 落在「未来」,`make` 判定无需重建而**静默跳过** `pynq_protocol.o`;`make … \| tail` 缓冲到 EOF 才输出 ⇒ 被 bxrun 的 20 s 无输出看门狗**两次杀掉**;`touch app/*.h` 连 `lv_conf.h` 一起碰了 ⇒ 全部 LVGL 目标失效(~200 文件重编)。**验证方式**:位流重建 `synth_design`/`write_bitstream` 双双 `Complete!`,md5 `30b7f46e235023b9868fc4fef5b66f1a`(旧 `7779b893…`),**WNS +11.812 ns / WHS +0.046 ns / TNS=THS=0 / 0 失败端点(共 4667)**、1537 LUT(2.89%)、单时钟 `clk_fpga_0` 20.000 ns;**网表核实** `cfg_dac_rst_n` 在位且 `cv_ctrl` 综合成**恰好 1 个触发器**(只 bit0,与掩码存储的读回诚实契约一致);`.hwh` md5 未变 ⇒ BD 未动。板上**断电重启后**(一次上电唯一一次位流加载)跑九步脚本 `dac_release_test.py`,**九步全过**:`[2]` `0x84=0x00000000` 闸住 → `[3]` FCLK0 **27.7778 MHz**(`DIV0=6/DIV1=6`,`0x00600600`)在窗口外 → **`[4]` `release_dac()` 被正确拒绝**(「FCLK0 = 27.7778 MHz **不在 DAC 复位图案的合法窗口内** [46.1~54.0 MHz],拒绝放行」)← **本次修复的核心证据,证明窗口闸真在拦** → `[5]` `apply_fclk0()` ⇒ **50.0000 MHz**(`IO_PLL 1000 / DIV0 20 / DIV1 1`)→ `[6]` 放行,`0x84=0x00000001` → `[7]` 等 802 ms → **`[8]` 回读 `0x14 = 0x080000` 仍是放行前写的值(没被冲掉)** ← 证明「放行前的值会被补发」这条 RTL 推论成立 → `[9]` `0x80000/0xC0000/0x40000/0x80000` 四点轮换回读 **4/4 ✅**。✅ **万用表实测(用户持表)** **0.00 V**,修复前恒 **−10 V** —— 这一个读数同时证明复位图案触发了、CMR 与自校准跑完了、放行前写的值被补发到位了。⚠️ 仅剩「界面点偏压后电压**跟随变化**」未同时读表(脚本第 `[9]` 步只验了寄存器回读轮换) | ✅ **直接可用**(九步验证全过 + 位流时序干净 + 网表核实通过 + **万用表实测模块输出 0.00 V**(修复前恒 −10 V);⚠️ 仅剩「界面点偏压后电压跟随」这一步未同时读表) | **新候选** `skill/reset-pattern-two-sided-window/`(适用范围:按时间图案配置器件且手册给 min/max 双边界;失效条件:手册只给单边界的**不适用** —— 本项目 `adc_9833` 已核实属这一类、不受影响)+ **扩展** `skill/stale-artifact-silent-reuse/`(第 7 形态)+ **扩展** `skill/datasheet-to-rtl-constants/`(补「常数在什么时钟下才成立」) |
| [LLM-044](LLM-044.md) | 2026-10-06 | Claude | **AD9238 采集通路 —— 从「双 MMCM 切换」被用户推翻,到 130 MHz 分频基准,到 module_ref 刷新卡死**。用户要求 ADC 采样率做成 PS 界面可调(像偏压那样)、默认 20 MSPS、离散档直通 65 MHz。**用户四处推翻模型**:① 模型断言「DDS 频率与 ADC 采样率两个旋钮不能独立调」→ 用户「**你调整 dft 代入的频率不就行了**」⇒ 相干条件不满足只是精度降级、不是功能失效,模型把"最优"当成了"必须";② 模型设计「两个 MMCM + BUFGMUX 切换」→ 用户「**写个分频器不行吗怎么只能二分频**」⇒ 一个计数器即可;③ 模型推荐 PLL 输出 65 MHz → 自己算出质因数死结(**65 = 5×13,65/n 永远得不到 10 MHz**)⇒ 改 **130 MHz = lcm(65,10)**,P 取 2/4/5/10/13/26/65/130;④ 模型断言「工程里还没有任何 clk_wiz 实例」⇒ 错,`clk_wiz_0` 存在(VCO 已是 1300 MHz),只是只被遗留顶层 `qrs.v` 例化。**交付**(用户授权代写并自跑构建):`adc_capture.v` 新建(130 MHz 基础时钟 + 8 位运行时整数分频 P + **相对 `adc_clk` 下降沿的有符号相位偏移** PH_OFF + 4096×16bit **双时钟 BRAM**,写 `clk_base` 读 `clk_axi` 无需 CDC 逻辑);`cv_axi_lite_slave.v` 增 `0x88 ADC_CFG / 0x8C ADC_CTRL / 0x90 ADC_STS / 0x94 ADC_BUF_IDX / 0x98 ADC_BUF_DATA`,`0x84[0]` 由 `dac_rst_n` **改名扩义**为 `cfg_clk_release`(复用不新增位,不打断已有 PS 代码);`cv_sys.v` **直例化 `MMCME2_ADV`**(26/1/10 → VCO 1300 MHz / CLKOUT0 130 MHz,模板逐字抄自工程内已 OOC 通过的 `clk_wiz_0_clk_wiz.v`),RST 挂 `cfg_clk_release` 使 FCLK0 若停在 27.78 MHz 则 **MMCM 锁不住 ⇒ 0x90[0]=0 如实报错**,失败方向安全。**🔴 本次最大的坑**:`update_module_reference cv_sys_0` **静默失败**(试遍各种参数形态、rc=1、错误消息为空、`errorCode=NONE`、日志无 ERROR),把 `ip/system_cv_sys_0_3/` 改名移走也**毫无变化** ⇒ 根因是 **`qrs.gen/sources_1/bd/mref/cv_sys/component.xml`(IP-XACT 缓存,`grep -c adc_` = 0)才是端口边界的真实来源**,不是那个 `.xci`;移走整个 `mref/` 后新建探针 cell 立刻给出 **56 个引脚**(51+5)⇒ 最终走**删 cell → 同名重建 → 按 dump 逐条重连**(17 根普通线 + `s_axi_aclk` + `s_axi_aresetn` + `M_AXI` + `S_AXI`)并**重赋地址 `0x43C00000`**、恢复 M_AXI 的 DDR 段。**另三条工具链坑**:`buf` 是 Verilog 门原语保留字(`VRFC 10-4982`)⇒ 数组改名 `cap_mem`;`$llength(x)` 被 TCL 当**数组访问**(`can't read "llength(...)"`)⇒ `[llength $x]`;`make_external` **不是本版命令名** ⇒ `make_bd_pins_external -name`;`save_project` 在 2025.2 **不存在**(`Common 17-163`)。**模型自行发现(用户未提)**:全部 2-FF 同步器**没打 `ASYNC_REG`** ⇒ 补齐(必须打在**第一级**)并配套 `set_clock_groups -asynchronous`,**时钟名从 `report_clocks` 抄实测值、不用通配符兜**(通配符匹配不到是静默失效)。**验证方式**:`xvlog` 12 文件零错零警;`synth_design` + `write_bitstream` 双双 `Complete!`/`PROGRESS 100%`;**WNS +0.983 ns / WHS +0.040 ns / TNS=THS=0 / 0 失败端点(共 4938)**;`report_clock_interaction` 两向均为 **`Asynchronous Groups`**;资源 **LUT 1662(3.12%)/ FF 1886 / BRAM 2.5 tile / DSP 0 / Bonded IOB 56**;`.hwh` 含 5 个 adc 端口且 `BASEVALUE="0x43C00000"` 已核对;产物已拷为工作区 `board/qrs.bit` + `qrs.hwh`。⚠️ **板上行为全部未实测**(板未上电);`adc_d[*]`/`adc_otr` 输入时序**有意未约束**(P 是运行时变量,静态 XDC 写不下所有档位;杜邦线 tPD ~4 ns 盖过 65 MHz 眼宽,给精确 `set_input_delay` 只是伪造精度);65 MHz 档(P=2)已实现但**当前杜邦线接不出来**,是物理限制不是设计缺陷 | ✅ **直接可用**(RTL + 构建产物完整跑通、时序干净、跨域约束已核实生效;⚠️ **板上行为未实测**,待上电) | **新候选** `skill/module-ref-cache-staleness/`(IP-XACT 缓存位置 + 「刷新不通就删了重建、dump 要连 `get_bd_addr_segs` 一起抓」)+ **新候选** `skill/async-reg-on-first-stage/` + **扩展** `skill/stale-artifact-silent-reuse/`(第 8 形态:生成物比源文件更有"权威"时的排查顺序) |
| [LLM-045](LLM-045.md) | 2026-10-06 | Claude | **AD9238 上板实测 —— 全链路打通、位序正确;★ 发现总线是双端口时分复用;★ OEB 经三态冻结实验证明有效,并查明「OEB 无反应」是模型自己的测量假象**。⚠️ **本行已于第 44 次会话(同日续)大幅改写** —— 原记的三条「相位台阶 = 数据眼证据」「球号名单自相矛盾」「D0/D11 位序完全未测」**全部作废**,详见本行 ⑤⑦⑧。用户上电后授权自行调试(「刚刚上电了」),并当场改了一条规矩「**以后先把上电自启动的部分关了,我说开再开**」。**① 干净起跑**:上电前先 `systemctl disable qrs-ui.service` —— 它开机就灌 `/home/xilinx/qrs.bit` 并拉起 `main.py`/`c_ui`,既**占住 PL** 又**灌的是旧位流**(板上一份是 10-03 的);停服后实测 `state = unknown`、`dmesg` 无加载记录、无残留进程。**② 为什么必须断电**:旧位流(与新位流一样)里 `lcd_fb_reader` 是**自由运行的 AXI 主机**,其寄存器图(`0x58~0x70`)里**没有任何一位能把它停下** ⇒ 「第二次 `Overlay()` 会锁死 HP 口连带 CPU」这条规则**正好命中**,只能断电重来。**③ 常驻探针**:因「一个上电周期只加载一次位流」对**新位流同样成立**,改为**命令文件轮询**(`adc_cmd.txt` → 进程轮询 → `adc.out`)并用 `systemd-run --unit=… --collect` 起进程,不依赖 SSH 通道存活。**④ ★ 最硬的一条 —— 分频器是真的**:单次采集须满 4096 点 ⇒ **时长 = DIV × 4096 / f_clkbase**。八档实测(2/4/5/10/13/26/65/130),最小二乘拟合 **`实测 = 0.9982 × 预期 + 26.9 µs`**,**斜率与 1.000 差 0.18%**,横跨 **65 倍**。反推 **f_clkbase = 130.2 MHz ⇒ FCLK0 = 130.2 × 10/26 = 50.1 MHz(+0.18%)** —— 这是 FCLK0 = 50 MHz 的**独立物理证明**(拿 PS 墙钟量 PL 采样节拍;若 f_clkbase 真是 72.2 MHz,斜率会是 1.8)。**⑤ 相位台阶(意外收获)**:`PH_OFF` 扫 −6…+6(DIV=13,跨度恰好一个 adc_clk 周期)得**三级台阶** `−6,−5 → 4087~4089` / `−4…+1 → 4075~4077` / `+2…+6 → 4086~4088`,每级内部 p-p 仅 3~5。🔴 **【第 44 次收回】** 原文写「悬空总线对所有相位应读数一致 ⇒ 说明采样点扫过的是**真实输出数据窗口** …… **这正是 `adc_capture.v`「采在下降沿 = 数据眼中心」那条假设的首次实测支撑**」—— **该推断作废**:`adc_clk`(V13)与 12 根数据线**在排针上相邻**,悬空数据线被 `adc_clk` 容性耦合**照样**产生相位台阶 ⇒ 无法区分「ADC 在驱动」与「线在悬空耦合」。⇒ 这三段台阶**另有一个与实测逐点吻合的解释**:它们不是「一个眼的内部结构」,而是**两个数据窗口的交替**(高电平两段 = 未驱动那一路停在静止电平,低电平一段 = 有信号那一路),见 §G。**⑥ 🔴 纠错轨迹(模型错两次)**:（a）**自己写的比值判据是错的** —— `adc_timing.py` 直接算 `P=130/P=13 = 9.307` 与 10 比,得出"分频器可能有问题"的**假结论**;**真因是实测含加性常数(轮询开销),而比值法对加性常数敏感、会把它往 1 拉** ⇒ 正确做法是先拟合看斜率。（b）**对用户说了一句错的断言**「MMCM 锁上了 ⇒ FCLK0 一定是 50 MHz」,原话「若停在 27.78 MHz,VCO 只有 **361** MHz(< 下限 600)绝不可能锁」—— **错**:VCO = `CLKFBOUT_MULT_F` × f_clkin = **26** × 27.78 = **722 MHz**,**落在 600~1440 内照样能锁**;那个 **361 = 13 × 27.78**,**用的是乘数 13,而本工程参数是 26/1/10** —— 凭记忆套参数、没回读 `.v` 里的实际值。**若没自己复核会被写进文档当成"已实测"**。（c）**工具链**:`sudo -E` **不足以**传递 `XILINX_XRT`(它由 `profile.d` 设,而 `profile.d` 只在登录 shell 读;非交互 SSH 读不到)⇒ 报 `No Devices Found`,须 `sudo -S -E env XILINX_XRT=/usr …`;另 **把密码 `echo` 进 `sudo -S` 的写法不能用来驱动交互式进程** —— 实测 sudo 凭据**已缓存时不消耗 stdin 行、未缓存时消耗一行**,行为随缓存状态变。**⑦ 🔴 模型判错两条,均已收回**:(a) 「球号名单自相矛盾」—— 用户逐字重报 16 个球号,与 `led.xdc` **逐项一致**;**自相矛盾的是注释里手写的 J4 脚号**(J4-15~26 与 J4-18/21 重叠),模型把「注释算错了」误判成「约束写错了」,并据此怀疑用户接线。(b) 「`OEB`/`PDN` 置 1 读数完全不变 ⇒ 无反应」—— **测量假象**:手动 GO 后没等 `done` 就 `rd`,读到上次采集的残留(`busy=1/done=0` 已在第 44 次输出中抓到)。重测后 **OEB 效果极显著**。📌 原记录(供追溯)::XDC 注释称 `D0..D11` 占满 **J4-15~26**,而 `PDN = V6 (J4-21)`、`OTR = W6 (J4-18)` **落在这个区间内部**;同一排针脚不可能既走数据又走 PDN ⇒ **球号名单至少一半是错的**。旁证:置 `OEB_N=1`(关输出驱动)与置 `PDN=1`(掉电)读数**完全不变**(4074~4077)⇒ 这两个球号八成不是 ADC 控制脚。**⑧ 【第 44 次更新,取代原「未证明的事」】** 原记的 D0/D11 位序未测、眼中心未定、模拟通路未验证,**其中三条已坐实**:① **D0/D11 位序判定正确**(实测为教科书级正弦:光滑单调 4071→4037→回升,波峰连续 4 点持平,±1 LSB;判据 —— 位序接反只会产生锯齿/回文,给不出光滑单调的 12 位斜坡);② ★ **总线是双端口时分复用**(一条 12 位总线在一个 `adc_clk` 周期内轮流输出两路,窗口各宽 `P/2`,port A `tick∈[2−P/2,+1]`、port B `tick∈[+2,P/2+1]`,中心 `1.5 − P/4` ⇒ DIV 13→−2 / 26→−5 / 130→−31;判据 —— 用户把线从 INB 挪到 INA,正弦**整体换时间槽**而非反相;DIV=13 时窗口 6/7 的划分与公式**逐点吻合**);③ ★ **OEB 有效,且这是「ADC 确实在驱动总线」的决定性证据**(OEB=1 → 128 点**全同 4087**、p-p=0;OEB=0 → 正弦 p-p 40;三态冻结**替代**了被高估的相位台阶证据)。⚠️ **仍未解(唯一硬伤)**:模拟侧 p-p 仅 **40 counts = 0.98% FS**、直流停在 **4057/4095 = 99% FS**,"悬空/未驱动"已被证伪,算术对不上(0.98% × 1 Vpp = 10 mVpp ≪ 示波器同节点读数)。§H.3 另记 OTR 随 OEB 变化但两义未分。📄 `report/llm-log/LLM-045.md` | ✅ **直接可用**(**全链路打通**:DDS→ADC→FPGA→双时钟 BRAM→AXI→Python;分频器经时长拟合坐实;MMCM 锁 / GO-done-buffer-寄存器全通;🔴 **2026-10-07 用户换线后本条记录有【三条上板结论作废】**(它们绑在物理接线上,不绑在 RTL 上):①**D0/D11 位序判定正确** ②**采样窗刻度 −2/−5/−31** ③**OEB 有效性**的三态冻结证据 —— 三项都必须在换线后的新接线下重测。本行 ①②③④ 的**方法与判据**仍然有效(位序判据:接反只会给锯齿/回文;OEB 判据:关驱动看读数是否冻结成精确常量),换的只是**实测数据**。⚠️ 另:本行 ⑤ 里写的 `adc_clk`(V13)已换成 `T11`;⚠️ **模拟侧幅度 0.98% FS、直流 99% FS 未解** —— 那是外部接线/前端问题,**不影响本记录里 PL 侧的全部结论**) | **新候选** `skill/measured-ratio-vs-additive-offset/`(**实测含固定开销时比值判据会把结论拉向 1,先拟合看斜率**)+ **新候选** `skill/ssh-sudo-env-not-inherited/`(`sudo -E` 传的是"空环境";`profile.d` 设的变量 systemd 与 SSH 非交互两条路都读不到,须显式 `env VAR=…`)+ **扩展** `skill/pynq-overlay-bringup/`(补**位流身份判据**:拿"新位流独有的寄存器复位值"当指纹,比时间戳可靠;补**常驻探针模式**)+ 🆕 **新候选** `skill/stale-buffer-vs-no-effect/`(**"改了参数读数不变"有两个来源:参数真无效果,或你读的是上一次采集的残留** —— 触发后必须等 `done` 再读;**打原始值,不打统计量**;若工具函数会覆盖你要测的配置位,别用工具函数)+ 🆕 **新候选** `skill/three-state-freeze-test/`(**判断"某根线是否真在被驱动",去把驱动源关掉、看读数是否冻结成精确常量** —— 容性耦合给不出"128 点一位不动";**通用形式:选能证伪的实验,不选能证实的实验**)+ 🆕 **新候选** `skill/ambiguous-user-description-of-hardware/`(**用户对硬件的口头描述有歧义时,不要挑对自己方便的那个解释**;要么问二值问题,要么直接实测 —— 涉及"用户的线接没接对"的判断**必须实测**) |
| [LLM-046](LLM-046.md) | 2026-10-06 | Claude | **「系统复位」页三项改造 —— 频率接口引出 / 常驻状态行 / 全参数复位**。用户要求:**① 上电后画面正常显示**、**② 把频率接口引出来**、**③ 初始化按钮内**要「显示当前的状态」+「按下后所有参数恢复默认」。**① 一行不用改** —— 先证伪再动手:`pynq_protocol.c:276` 的 `demo_capacitance()` 是内置模型数据,`ui_demo.c` 无任何一处读 ADC 缓冲(本轮前连 `0x94/0x98` 的寄存器宏都没有)⇒ 采到什么都进不了界面,「正常算正常显示」**就是现状**(把它顺手接进来反而会破坏这个前提:模拟侧幅度只有 0.98% FS,见 `LLM-045`)。**② 频率接口 = 两个都做**(用户经 `AskUserQuestion` 定):DDS 激励 `0x80` **和** ADC 采样率 `0x88` DIV。**③ 状态显示 = 按钮下方常驻一行**(同上定)。**落点**:新建 `app/measure_config.{h,c}`(测量参数的**唯一写入方**),改造只落在「系统复位」页 —— `ui/display_protocol.h` **一行未改**(新接口另起头文件),`ui_demo.c` 的测量/扫压/结果/首页与表头**逐字不变**,`main.py`/`drivers/*` 契约未变,**RTL 功能零改动**。`Makefile` 的 `$(wildcard app/*.c)` 自动收新文件,**未改 Makefile**。**关键设计**:★ **采样率必须与 PH_OFF 一起写** —— 窗随 P 滑动(窗 `tick∈[2−P/2,+1]`,中心 `1.5−P/4` ⇒ 13→−2 / 26→−5 / 130→−31),只改 DIV 会让采样点滑出被驱动窗口,现象是「换了速率就没波形了」而**不报错、回读全对** ⇒ 接口上**不提供**单独改 DIV 的路径;写 `0x80`/`0x88` 后**立即回读核对**(照 `bias_write_code` 的做法,AXI 通路出问题时写不报错);状态行**不含偏压/回路**(已由常驻表头每页显示,重复放会让两个模块互相咬);⛔ **`0x84[0]` 不在复位范围**(放行权归 Python `release_dac()`,清掉 = DAC 按回复位 = 输出跳 −10 V 而回读全正常);⛔ **不暴露 `0x88[17]OEB/[18]PDN`**(极性约定尚未从 RTL 追到引脚,会变成"一点就坏"的开关);档位表五档 `1/2/5/10★/13 MSPS`,**20 MSPS 在 130 MHz 基准下取不到**(130/20=6.5 非整数)故界面上直接标明。**★ 纠错轨迹 3 例**:(a) **`snprintf` 隐式声明** —— 我断言「文件里出现 22 次 ⇒ `<stdio.h>` 必然经传递包含」,编译器只在我新写的三行报警;`grep -o '[a-z_]*snprintf'` 实测 **19 个是 `lv_snprintf`**、3 个 `snprintf` 全是我刚写的 ⇒ **拿聚合数字代替逐项核对**;中途改法用变参助手又撞上 `<stdarg.h>`,**同一个坑第二次**,最终在调用点内联 `lv_label_set_text_fmt`(照 `UI_UpdateCommonStatus()` 既有写法)、不动前序 include 块。(b) ★★ **窗口公式写反 —— 上一场刚实测出的结论,本场自己又抄错**:按 `adc_capture.v:238` 的 `[−P/2, P−1−P/2]`(P=13→`[−6,+6]`,是**计数器够得着的刻度**)写了守卫,而数据稳定窗是 `[2−P/2,+1]`(`[−4,+1]`,**6 拍**)⇒ 守卫过宽,用户可 nudge 到合法但数据未稳定的拍上,**采到垃圾且不报错** —— 正是守卫唯一的存在理由;旁证是状态文档里 `LLM-045` 记的「复位 `PH_OFF=0` 落在 A 窗内且**贴边**」(0 恰是 `[−4,+1]` 最右端)。改用窄窗后**穷举 254 个 DIV** 全部「窗含自身中心」✓;改写途中还写过 `center - span + ((p%4u)?0:0)` 这种**三元两边都是 0** 的废式,已删。(c) **`dds_set_hz()` 变死函数**(两个调用点都改走后 `-Wunused-function`)⇒ 删除并留注释说明**写入方必须唯一**(两个写入方 = "谁最后写谁赢")。**一并修正 5 处「代码与注释相反」**:`pynq_protocol.c:215`「界面上没有频率控件」、`cv_regs.py` D08「1 MHz 固定」、`pynq_regs.h` 同款、`cv_axi_lite_slave.v:458`「PH_OFF=0 → 数据眼中心」(**眼中心实测在 −2**)、以及新增 DDS 上下限。**验证状态**:宿主机 `make -j4` ✅ 通过、**我改的三个文件零告警**(唯一告警 `ui_demo.c:1787` 是前序既有的函数指针强转);`adc_window` 穷举 254 个 DIV ✅;相位中心公式对实测三点 ✅;Python 与 C 两版逐位一致(取整一律整数算术,避开 Python **银行家舍入**与 C **half-away-from-zero** 在 `(6−P)≡2 (mod 4)` 时的分歧)。⏸ **板上编译 / 界面证据 / 硬件回读全部未做** —— 待用户上电(步骤见记录 §F) | ⚠️ **部分可用**(宿主机编译零告警、两处公式穷举自检通过;⏸ **板上行为未验证**) | 🆕 **新候选** `skill/two-ranges-one-name/`(同模块两个"有效范围"必须命名区分,**否则必然抄错**)+ 🆕 **新候选** `skill/count-is-not-inclusion/`(「出现 N 次」不能推出「头文件在」—— 先分清同名不同族的函数:`snprintf` vs `lv_snprintf`)+ 🆕 **新候选** `skill/writer-must-be-unique/`(一个寄存器只允许一个写入方;包装函数没有调用点就该删,不留兼容壳)+ 🆕 **新候选** `skill/guard-the-narrow-range/`(守卫参数范围时取"会**静默出错**"的窄域,不取"失败方向安全"的宽域)+ 🆕 **新候选** `skill/code-outliving-comment/`(改行为必须同时改注释 —— 本场 5 处注释已与代码相反) |
| [LLM-047](LLM-047.md) | 2026-10-06 | Claude | **「系统复位」页上板验收 —— 字库吞字 + 复位后按钮高亮不同步;★ 并自行推翻「触摸链路已死」的误判**。上电后授权自主调试,靠 **`/dev/mem` 读 DDR 帧缓存**(新写的 `fb_dump.py`)把"屏上到底是什么"变成可自动化比对的数据,**三个缺陷全部精确到像素**。**① 🔴 字库吞字(只在实屏可见)**:两张中文点阵字库是 `lv_font_conv` 用 `--symbols` **白名单**生成的 **260 字**,新文案里 `样/相/率/居/中` 等 **252 个字不在表内** ⇒ 被**静默吞掉、不报错、不留日志**,状态行渲染成「激励 0.100MHz **采** 10.00MSPS(DIV13) **位** -2」、中间按钮整个空白。**先证伪再动手**:试过"改成字表里已有的字"——`相位/时移/延迟/居中/中心` 里 `相移延迟居中` 全都不在表内,**凑不出一个像样的中文标签**,该路作废 ⇒ 经用户决策(`AskUserQuestion`)选**重建字库**。**可复现性先验证再使用**:用**原 339 字符号集**重新生成 20 px 字库与仓库那份逐字节比对 —— **结构/cmap/字形数完全一致,仅 5 个半字节的抗锯齿舍入差异**(FreeType 光栅化版本)⇒ 工具链对得上才动手。新符号集 = 原白名单 ∪ 源码全部字符串字面量里的非 ASCII(**588 字**),`260 → 510` 字形,二进制 +131 KB。**★ `-o` 的文件名会反推 `#ifndef UI_FONT_CN_xx` 守卫宏名,输出到别的名字守卫名就跟着变** ⇒ 必须直接输出成目标文件名(第一版输出到 `/tmp/new20.c` 生成出 `#ifndef NEW20`,已作废重来)。**② 🟡 复位后按钮高亮不同步(用户发现)**:`DisplayProtocol_RequestReset()` **同步**复位了三项参数、也刷了状态行(`MeasureConfig_RestoreDefaults()` 内部负责),但**没有重算按钮高亮** ⇒ 屏上"状态行写 10.00MSPS、采样率那行却还亮着 5MSPS"自相矛盾。修法 4 行:在复位确认回调里补一次 `UI_UpdateInitConfigStyle()`(该函数早已存在、前置声明也在,只是没人调),**并刻意不重复刷状态行文字** —— 免得两处都能刷新、以后改一处漏一处。**③ ★★ 纠错轨迹(最重要的一条):模型把"没人碰屏"判成了"触摸链路已死"** —— 依据是盯 `0x68` **75 s 零变化** + 一个 `err=1`,据此写了「🔴 全程零变化 —— 触摸链路不通」并开始深挖 `gt911_ctrl.v` 的 `S_ERR/S_RECOV` 与 I2C 时钟拉伸,**准备改 RTL**。真相:该 `err` 是**粘滞**位(只有重配 PL 才清),日志里明确记着它是上电**首次**事务 `0x68=0x00000110` 时置起的历史标志;`seq` 不涨只是**没人操作**。取 `/var/log/qrs-ui.log` 才看到 `seq` 已到 **71**、`[touch] 按下 (751,443) 点数 1` 成对出现、每次点击都正确写了 `0x80` —— **链路一直是好的**。📌 **"窗口内零变化"只说明这段时间没有新事件,不能区分"没人碰"和"碰了没反应";二值问法必须配对"先造输入再观测"**(扩展 `skill/verify-claims-not-intuition`)。更正后 `gt911_ctrl.v` 一行未动,**避免了一次基于错误前提的 RTL 改动**。**④ 复现环境的一个硬约束与解法**:PL 里 `lcd_fb_reader` 是自由运行的 AXI 主机 ⇒ 一个上电周期只能加载一次位流,而 `main.py` 是 `supervise()` **阻塞等 c_ui** 的、`qrs-ui.service` 又是 `Restart=no`(注释写明"故意")⇒ c_ui 一死,main.py 就退出、把屏刷纯色、释放帧缓存,**界面再也回不来**。解法:**`SIGSTOP` 持有 `Overlay()` 的父进程**(PID 1576),再停旧 c_ui、构建到**临时目标名** `make TARGET=c_ui_new`(不覆盖正在跑的二进制)、起新实例。父进程冻住 ⇒ 不发现子进程已死、不退、不释放帧缓存;旧二进制留一份 `c_ui.bak` ⇒ 失败可回退。据此完成 **3 轮「改 → 编 → 换 → 截屏验证」**,**位流一次没重下**。**验证方式**:(A) 裸 `/dev/mem` 独立探针(不经界面自述)`0x80=0x00A3D70A` / `0x88=0x0000FE0D` / `0x14=0x00080000` / **`0x84=0x00000001`(🔴 `dac_rst_n` 没被误清)**;(B) 帧缓存 dump 的**修复前/修复后/复位后**三张实屏截图,三行高亮 = `1MHz`/`10MSPS`/`居中` 与状态行一致;(C) `/var/log/qrs-ui.log` 里 `[mc] 采样 → 5.00 MSPS(DIV=26,PH_OFF -2 → -5,窗 [-11,+1])`、`[mc] 相位 +2 越出采样窗 [-4,+1],未改`(**越窗守卫生效**)、`[mc] 参数复位:…(回读一致)`;(D) 字库可复现性比对(见①);(E) 板上原生编译 `-Wall -Wextra` 仅剩 `ui_demo.c:1787` 一个**前序既有**告警。**一并查出但未处理**(已告知用户):`pynq_protocol.c:486` 的 `UI_Demo_SetStatusText("状态:就绪(偏压/激励已接 PL,Cs 待测量链)")` **26 个字撑破表头标签**,换行后第二行被裁(与本轮改动无关,属新写的协议层) | ✅ **直接可用**(两个缺陷均上板修复 + 三张实屏截图为证 + 寄存器逐位核对 + 字库可复现性先验;并自行推翻一处错误诊断,避免了一次错的 RTL 改动) | 🆕 **新候选** `skill/fixed-glyph-font-is-a-whitelist/`(**点阵字库是白名单,不是"缺字自动回退";被吞的字不报错不留日志**;判定规则实测坐实:`SPARSE_TINY` 的 `unicode_list[i]` 存 `码点 − cmap.range_start`,20 px 字库 `range_start=937`(U+03A9))+ 🆕 **新候选** `skill/dump-the-framebuffer-to-see-the-screen/`(帧缓存在 DDR ⇒ 按物理地址读内存即可看到**正在显示的那一帧**,让"屏上是什么"可自动比对,不等人拍照)+ 🆕 **新候选** `skill/hot-swap-userspace-without-reloading-bitstream/`(**PL 有自由运行的 AXI 主机时,冻结持有 Overlay 的父进程再换用户态程序**;构建到临时目标名,旧二进制留档)+ **扩展** `skill/verify-claims-not-intuition/`(**"窗口内零变化"不等于"功能故障"** —— 它只说明没有新事件) |
| [LLM-048](LLM-048.md) | 2026-10-06 | Claude | **采样显示落地 —— 把 AD9238 的真实采样接进结果页四张图;并五处自我纠错,其中两次是「差点把对的改成错的」;★ 同日上电后**板上四通路全部实跑验完**、扫压 60 点零失败**。用户要求:**扫压也好单点也好,调试曲线和结果曲线都要显示出来**。**① 先证伪再动手 —— 缺口不在版面,在数据源**:结果页那 5 个页签**早就是**用户要的四个视图(单点波形 / 扫压结果 / 扫压波形),**一个控件都不用加**;真正的缺口是喂数据的 `UI_Demo_BeginWaveform()/UI_Demo_UpdateWaveformPoint()` **唯一调用者在 `app/_retired/pynq_protocol_stub.c`,而 `_retired/` 不在 `$(wildcard app/*.c)` 里 ⇒ 根本不进编译**,扫压的 Cs 则来自 `demo_capacitance()` 这个**内置模型**;`0x8C/0x90/0x94/0x98` 在全工程**零行读取代码**,只有宏定义与注释。**② 「两路」怎么来 —— 本轮最关键的实现决定**:`adc_capture.v` 的 `adc_clk=(cnt<P/2)`、`tick=cnt−P/2` ⇒ **上升沿=tick −P/2、下降沿=tick 0**;用户口径定「上升沿=通道 A、下降沿=通道 B」。但**一次 GO 只锁一个 tick** ⇒ 两路只能**两次采集**拿到(先 `PushPhase(−P/2)` 采 A,再 `PushPhase(0)` 采 B),**不改 RTL、不重下位流**。代价如实写进代码:**两次独立采集 ⇒ 两路相位不相关 ⇒ 只能比幅值**,`Cs` 按"被测件是纯电容"由幅值比算(`R_tia` 未标定),**`Cp`/`D` 一律显示 `--`**(填上去等于编数据,并在日志里补一次"为什么是 `--`"的说明);要真正同时采须给 `adc_capture` 加第二组缓冲 + 重下位流,**本轮不做,记为后续项**。**③ ★★ 纠错轨迹(最重要的一条):死代码的注释和活代码一样权威 —— 差点把一个对的常量改成错的**。计划里我写了「照 `ad9238.v` 的 `ADC_DATA*(20/4096)` 换算」,实现时按 AD9238 **数据手册**填了 **2.0 V**;写完后本能地想"RTL 注释 > 数据手册记忆",**差一点就改成 20.0**。改之前先核了一句 `grep -n "ad9238" *.v` —— 只有它自己的 `module` 行 ⇒ **该模块全工程未例化**,是前序 ATK 例程的死代码;再看 `cv_sys.v:776` 的 **`.adc_d (adc_d)` 直连引脚**,中间根本没有换算模块 ⇒ **2.0 是对的,20 是把死代码当成了真源码**。整段推理已写进 `adc_sample.h` 的常量注释。**④ `-Wformat-truncation` 抓出一处"关键时刻被吃掉"的静默截断**:`adc_sample.c` 的 `s_err[192]` 装不下实测 **249/261 字节**的"超时诊断"与"每周期点数不足"两条消息(要展开 `%s` 那个 0x90 状态串)⇒ `snprintf` **静默截断**,而被吃掉的正是排查步骤的尾句 ⇒ 加宽到 **320** 并注明依据。**⑤ ★★ 纠错轨迹(本轮最要紧的一条):照抄用户口径的刻度,而我们自己上一场的实测早就写了不是那回事 —— 两路认反了**。用户口径「**上升沿是通道 a 数据下降沿是通道 b 数据**」,我把它**直译成刻度** `−P/2` / `0`,代码**全部写完、宿主机也编过之后**,收尾复核时去翻本工程自己的实测(`LLM-045` §G)才发现:那 12 位总线是**双端口时分复用**、两路各占半个周期 —— **A=INA 窗 `[2−P/2,+1]` 中心 `1.5−P/4`;B=INB 窗 `[+2,P/2+1]` 中心 `1.5+P/4`** ⇒ **tick −P/2 落在 B 窗、tick 0 落在 A 窗,正好是反的**,而且两者都**贴窗沿**不是中心。**要命的是它不会报错**:两个刻度都合法、都能采到稳定数据,只是把 INA 的数当"电压"、INB 的数当"电流"画了出去 —— **数据全真、归属全错、屏上看不出任何异常**。修法:新增 `AdcSample_PortATick(div)=cv_adc_phase_center(div)`(与既有 `PH_OFF` 默认值**同源**)、`AdcSample_PortBTick(div)=cv_adc_phase_center_b(div)`(新增在 `pynq_regs.h`,`1.5+P/4` 同一条半远离零舍入并折回合法范围),另加 `AdcSample_TickLegal()` 在推刻度**之前**先查(越界刻度的现象是 `done` **不置位**,不查极易被误判成"位流不对");并逐个核对 **P=130/65/26/13/10 五档**两个中心都落在各自窗内,其中 A 的三个中心与实测 `−2/−5/−31` **逐位吻合**。⇒ 📌 **"用户这么说"是需求,不是参数**:需求照做(就要两个真实输入、就要两张图),但把需求翻成寄存器刻度时,**必须回到本工程自己的实测结论去核** —— 尤其是那条结论就是上一场自己写的。**⑥ 状态行的像素预算(闭合 `LLM-047` 的遗留项)**:上一场留了「`pynq_protocol.c:486` 状态串 26 字撑破表头标签」**未处理**,而本轮我**又写了更长的(~35 字)**。这次不停在"提醒用户":把预算算出来写进文件头 —— `g_header_operation_label` 在 x=704、宽 `1024−714=310 px`、**右对齐**、`ui_font_cn_20` ⇒ 汉字 20 px/半角约 10 px ⇒ **约 15 个汉字**,超了**从左边开始被吃掉**(右对齐,"状态:"三字先没)⇒ 四条文案全部改短到 220–260 px,并立规矩「**别再往状态行里塞解释,解释写 `printf`**」。**⑦ 复位没清"调试曲线"**:`UI_Demo_ClearAllResults()` 清的是**读数卡片 + 结果快照**,**不含 `g_result_wave[]`** ⇒ 复位后结果页两张波形图**还留着上一轮采集**、状态行照旧写「已采集 250 点」,与用户提过的「**发送复位后下面的按键也得跟着变吧**」同型;修法 `UI_Demo_BeginWaveform(kind,0)`,**★ 第一版还漏了一步** —— `ClearAllResults()` 内部那次 `UI_ResultsRefresh()` 发生在清波形**之前**,必须再补一次显式刷新,已在注释里写明理由。**⑧ 一处设计偏离(如实记录)**:计划说 `adc_sample.c` 自己写 `0x88` 再写回,实际改成让 `measure_config.c` **开一对 `PushPhase/PopPhase`** —— 守住"`0x88` 写入方必须唯一"这条既有不变式,构造上排除"谁最后写谁赢";且 `PushPhase` **只按寄存器合法范围 `[−P/2, P−1−P/2]` 守卫**,刻意**不套** `LLM-046` 那个数据稳定窄窗(理由是它要临时改刻度,**不是**要选采样点)。另解调参考表用**运行时建表**而非计划的编译期 10 点常量表 —— `N=130/DIV` 随用户选的档位变(26/10/5 点),写死 10 点只对 DIV=13 成立。**验证方式**:(A) 宿主机 **`make -j4` 整工程 + LVGL 全量链接通过**(`✅ 编好了:c_ui`,无链接错误);(B) 五个改动文件逐文件 `-Wall -Wextra` **零告警**(唯一告警 `ui_demo.c:1787` 是**前序既有**);(C) `grep -n "ad9238" *.v` + `cv_sys.v:776` 坐实死代码与直连引脚;(D) **五档采样率逐个核对两个时间槽中心都落在各自窗口内**,A 的三点与实测 `−2/−5/−31` 吻合;(E) 核 `ui_demo.c` 的 `g_wave_scale`(`{1000,1000}`/`{100,100}`)与量纲约定,确认**不触 int16 上限**(`100 V × 100 = 10000 < 32767`)。✅ **同日上电后板上全跑通(四通路)**:原生 `make -j2` 通过(唯一告警 `ui_demo.c:1787` 前序既有);9 个变更文件上传后 `md5sum` **9/9 逐字节相同**;`dmesg` 核出这个上电周期**只加载 1 次位流**(未踩二次 `Overlay()` 锁死);**单点**实跑 `A/INA **tick-2** / B/INB **tick+5**`、「单点波形」页 X 轴 0…249 两图都画满、底「已采集 250 点」;**扫压** 60 点 **零失败**,每点同刻度,「扫压结果」60 点铺满 0.10→100 V、「扫压波形」X 轴 0…59 偏置包络到 100 V;★ **60 点扫压(120 次 push/pop)跑完后裸 `/dev/mem` 回读 `0x88 = 0x0000FE0D`** —— `PH_OFF` 仍是 **−2**、**没停在 B 路的 +5**,"采完写回"坐实。⚠️ 板上另发现**两处"看屏会误会"**(均不影响功能,已记 §G 待改):① 「扫压波形」电流包络是 0 线而「单点波形」是 ~1990 μA —— 两张图画的**不是同一个量**(单点=缓冲原始采样直流电平,扫压=1 MHz **解调幅值**,悬空输入下 ≈0;不是 bug 但要标清);② 日志打「幅值 0.0000 V」而 Cs 非零 —— `%7.4f` 把 1e-5 V 抹成 0,Cs 用的是未舍入浮点,应改打 mV 或原始码。📌 踩到并记下的一条边界:`/var/log/qrs-ui.log` **跨上电周期 append、不清**,里面躺着好几轮的启动横幅 ⇒ `grep "PS 侧启动"` 看到多次**不等于** main.py 反复重启;**判据是 `dmesg \| grep -i fpga` 里 `writing qrs.bin` 的条数**(本次 = 1)。宿主机构建产物 `build/`、`c_ui` **已删除**(否则 x86 的 `.o` 会被板上 `make` 当成"已是最新"链进 A9 的 ELF)。上板步骤见记录 §F | ✅ **直接可用**(宿主机整工程构建通过 + 零新增告警 + 静态核查 + **同日上电后板上四通路实跑验完**:单点 250 点两图 + 扫压 60 点零失败 + `0x88` 刻度写回经裸探针坐实;⚠️ 仅存两处**表述/打印精度**问题(§G),不影响功能) | 🆕 **新候选** `skill/append-log-across-power-cycles/`(**跨上电周期 append 的日志不能用来数"这一轮启动了几次"** —— 文件里的启动横幅条数是**文件**的生命周期,不是**被观测对象**的;判据要去内核要:`dmesg \| grep -i fpga` 里 `writing qrs.bin` 的条数)+ 🆕 **新候选** `skill/user-wording-is-a-requirement-not-a-parameter/`(**用户对硬件的口头口径是"要什么",不是"寄存器写什么"**;照做需求,但翻成刻度/地址/位宽时必须回到**本工程自己的实测结论**去核 —— 尤其那条结论就是上一场自己写的。后果形态:**数据全真、归属全错、屏上无异常**,通用形式是"**不报错的错优先于会报错的错去防**")+ 🆕 **新候选** `skill/dead-code-comments-look-authoritative/`(**"哪份代码在数据通路上"要先证明再引用** —— 未例化的模块,它的注释/公式/位宽原样躺在仓库里,读起来和真源码没有区别;判据不是"文件在 `srcs/` 里",而是 `grep -n "<module_name>" *.v` 有 `module` 行**以外**的命中)+ 🆕 **新候选** `skill/onscreen-summary-has-a-pixel-budget/`(**面向用户的单行汇总有硬字宽预算,超了是静默截断且从左边吃**(右对齐时);算预算要连**对齐方向**一起算,预算/对齐/字号三者缺一就得不出结论;长解释一律进日志)+ **扩展** `skill/stale-artifact-silent-reuse/`(第 9 形态:**"清空"函数只清了它自己知道的那部分** —— `ClearAllResults()` 名字像"全清"却不含 `g_result_wave[]`;判据是**列一遍它实际写到的全局量**,不信函数名)+ **扩展** `skill/writer-must-be-unique/`(同一条纪律的**正向用法**:新模块要改别人的寄存器时,**不加第二个写入方**,而是让原写入方**开一对 push/pop**) |
| [LLM-050](LLM-050.md) | 2026-10-06 | Claude | **「100 kHz + 10 MSPS」单点测量 100% 失败 —— 解调参考表上限 64 点/周期被击穿;★ 报错文案的排查指引方向反了**。**这条缺陷不是用户报的** —— 是在按「整理板子环境和文档」这条指令做**收尾日志对账**时查出来的。**① 起因是一句对不上**:停服务时 `dump_stats()` 报 `ADC 采集 成功=332 失败=5`,而日志里 `grep -c '^\[adc\]'` = **333**。我第一反应是「计数器漏计」—— **错**:`333 − 326(扫压) − 6(单点) = 1`,那 1 行正是 🔴 失败行 ⇒ 📌 **两数对不上时,先问「定义一样吗」,再问「哪个错」**。**② 机理**:用户把激励切到 100 kHz(`[mc] 激励 → 0.100 MHz`)而采样仍是 10 MSPS ⇒ `N = 10 MSPS / 0.1 MHz = 100 点/周期 > ADC_DEMOD_MAX_N(64)` ⇒ `build_table()` 返回 false ⇒ `AdcSample_Demodulate()` 拒绝解调 ⇒ 单点测量整体失败、两个波形都不画。那张 `s_tab_cos[64]` 的余量是**按 DIV 从 1 MHz 反推**的,**没考虑「用户把激励调低」这条路径**(调低激励与调高采样率对 `N` 是同一方向)。**③ 实测 5 次尝试 5 次失败(100%)**,且**只有第 1 条打印**(`adc_report_failure()` 明写「此后不再重复打印这一条」)⇒ 后 4 次**屏上无反馈、日志里没有**;若不是收尾时停服务把 `dump_stats()` 打出来,**这次失败在证据上就不可见**。**④ 🔴 文案的指引 ③ 方向反了**:报错写「每周期 100 点,超出参考表上限 64」,指引却让人查「③ 采样率档位是不是**太低**」—— 该文案只覆盖 `N < 4` 那一支,**漏了 `N > 上限` 这一支**,照它排查会**往反方向走**。**⑤ 隐患**:扫压 60 点走同一条路径,100 kHz 下会**全部失败且只打 1 行**,而 `[proto] 扫压结束:60 点已下发到 DAC` **照常打印** ⇒ **看起来跑完了,曲线是空的**(本次 6 轮扫压都在 1 MHz 下跑,**未命中**这一支)。**⑥ 对账两条链闭合**:`100kHz 窗口内点测请求 5` = `失败=5`;`扫压 326 + 单点 6 = 332` = `成功=332`;`点测请求 11 = 6 成功 + 5 失败` ⇒ **每个 `[adc]` 行都有归属,没有余数**。**⑦ 交叉证据**:同期 6 轮 **1 MHz** 扫压 326 点**全部成功**、`偏压写=342 回读核对失败=0`、`axierr=0` ⇒ 失败**只与设置组合相关**,不是位流/硬件故障。**本轮零代码改动**(§6.0:代码归用户)。**⏸ 未做**:独立复现(只有 1 组数据;服务已停,再起就是本上电周期第 2 次 `Overlay()` = 已知挂死路径)。复现成本极低:**上电 → 切 100 kHz → 点单点测量 → 看两个波形页签是否为空** | ✅ 直接可用(有板上原始日志 + 计数器双向对账;**🟢 独立复现已于同日由 `LLM-052` 完成** —— 用户本人切到 **100 kHz + 13 MSPS** 后点单点测量,板上实打实复现同一失败(N=130 > 64),日志行逐字为 `每周期 130 点,超出参考表上限 64`;原判据、机理、文案方向反了这三条**全部确认**,见 `LLM-052` 行⑨) | 新候选 `skill/error-text-can-point-the-wrong-way/`(**报错文案与判定条件必须同源** —— 判定是双侧的,文案只写了一侧) + 新候选 `skill/report-failure-once-loses-the-count/`(**「只打一次」的失败日志必须配收尾计数**,否则失败次数不可知,与既有 `count-is-not-inclusion` 同族) |
| [LLM-049](LLM-049.md) | 2026-10-06 | Claude | **扫压「全部」只有一条曲线 —— 三条都画了,但它们逐点重合;根因是「本位流没有继电器」**。用户报告:「扫压选全部,只测出一条曲线,而且是 CRSS 的,不该三个吗」。**① 症状是"只看见一条",成因不是"只画了一条"** —— 我第一反应是去查 `g_sweep_visible_mask` 是不是只置了一位,读到 `UI_Demo_BeginSweep()` 就被否掉:`multi_path` 分支明写 **`0x07`**,扫压页的「全部」派发到 `DisplayProtocol_RequestRgThenSweepAll()` → `sweep_start(0x07u, …)` ⇒ **三条 series 全部可见**。**② 真正的原因:三条的数据逐点相同** —— `sweep_measure_point()` 的 `for (path = 0…2)` 给三路喂的是**同一个** `s_pair.cs_pf`(每个偏压点只做**一次** ADC 采集,一个值填三路),`UI_Demo_UpdateSweepPoint()` 内三路的 `g_sweep_valid` 又由**同一个** `cs>0` 决定 ⇒ 三条**同时有效、坐标完全相同 ⇒ 完全重合**。**③ 为什么偏偏是 CRSS 的颜色** —— LVGL 按 `lv_chart_add_series()` 的**添加顺序**绘制、后加的在上层,而 `ui_demo.c` 添加顺序是 `[0]=CISS`(`0xd65ab1`)/`[1]=COSS`(`0xe5225b`)/`[2]=CRSS`(`0x8c971d`)⇒ **栈顶是 CRSS**,盖住底下两条;右边图例三个色块仍然都在。**④ ★★ 根因(四层独立坐实,层层零命中)**:真实 Ciss/Coss/Crss 是**三种不同的测量回路**(G-S 短接 / D-S / G-D),要靠继电器在**每个偏压点**切一次 —— 而 **`relay_ctrl` 这个模块全工程不存在**:交付仓库 `grep -rln "relay" src/rtl/` **零命中**(15 个文件无 `relay_ctrl.v`)、`cv_sys.v` 里 `0x1C`/`relay` **零命中**(寄存器在 RTL 里没有对应逻辑)、C 侧 `pynq_regs.h` **没有 `0x1C`**、Python 侧 `cv_regs.py` 虽有 `RELAY_CFG: 0x1C` 但落在注释标注的「**测量主链(PL 侧尚未实现,见 `PL_IMPLEMENTED_REGS`)**」那一块。⇒ **不是显示 bug,是 P4(双通道 + 继电器 + 校准)的硬件缺口**。**⑤ 建议不改** —— 三条重合是"硬件只有一条回路"的**如实呈现**;等 `relay_ctrl` 落地,`sweep_measure_point()` 那个 `for path` 循环**已经写好**,自动出三条不同的线,**界面代码一行不用动**。**⑥ 旁证:演示模式确实出三条分开的线** —— `CV_UI_DEMO=1` 时 `demo_capacitance(path, v)` 给三路不同的 `c_max/c_min`(`{980,240,48}` / `{420,62,7.5}`),**那是编的数**;若此前见过三条,是这一档。**验证方式**:(A) 四层 grep 表(上列,四层零命中);(B) 派发链逐段读死 `ui_demo.c:6400` → `sweep_start(0x07u)` → `sweep_measure_point()` → `UI_Demo_UpdateSweepPoint()`;(C) 系列颜色/添加顺序 `ui_demo.c:244`、`:5583-5594`;(D) 🟢 **板上帧缓存像素核对 —— 预测逐字命中** ★ **第一次抓帧抓到了「数据导出」页**(顶栏 `回路:CISS`),三个系列色 **0 命中**、连曲线都没有 ⇒ 当场判为**不构成证据**(见㈢),请用户切到「扫压结果」页签重抓;第二次(`0x38100000`→`fbB.ppm`,1024×600,312 色)精确计数:**CISS `0xD2D6` = 104 px、COSS `0xE10B` = 104 px,各为单个 `26×4` 块且 y 方向只有 1 簇** ⇒ **只有图例色块、曲线上 0 像素**;**CRSS `0x8CA3` = 4291 px,两簇**:`y[139,142]`(自身图例块)+ `y[167,425]` x`[90,983]`(**曲线本体**)。三个自洽点:① **104 = 26×4**,正是 `lv_obj_set_size(line, 26, 4)` 的图例色块尺寸;② 两块间距 **130 px**(405→535)= 代码里的 `item_x = 380 + index*130`;③ CRSS 多出的 **4187 px** 横跨 x`[90,983]` 就是曲线。⇒ 结论由像素坐实。存档 `report/measurements/20261006_sweep_all_three_series_overlap.md` + 两张截图;(E) 旁证见⑥。**纠错轨迹**:㈠ 第一反应(查可见位)被自己的代码阅读否掉,差一点沿"为什么只置一位"白挖;㈡ **差点把用户描述当结论去改代码** —— 若照"只画一条"去改 mask,就会**掩盖真实原因**,让后来人以为"界面上一条是对的",而三条分开的曲线其实是**迟早该有**的 ⇒ 先问"这三条**能不能**不同",再问"为什么看起来是一条";㈢ **未把"没抓到"说成"验证过"** —— 直方图零命中时没有去编"零命中说明被覆盖了",而是承认抓错页、记为待补;**这一条后来收口了**:切页重抓后与预测**逐字吻合** ⇒ 📌 若当时顺着"零命中"往下编,后面这次真测量反而会显得像在给结论找台阶 | ✅ **直接可用**(结论四层独立坐实,**不需要动一行代码**;并明确它属于 P4 硬件缺口而非显示缺陷) | 🆕 **新候选** `skill/n-identical-series-look-like-one/`(**屏上只看到一条线时,先查是不是 N 条重合,再查可见位**;两条验证路径:读 series 的**添加顺序 + 数据来源**(同源同值必重合,栈顶 = 最后添加的那条,颜色能对上就是它)/ 帧缓存像素直方图(**但必须先确认抓的是有曲线的那一页**)。失效条件:各系列数据确实不同 ⇒ 回到"可见位 / 裁剪 / 量程"那条线。**更要紧的是设计侧那一半**:⛔ **没有硬件支撑的维度,不要用图例/颜色暗示它存在** —— 三条图例 + 三种颜色会让读图的人以为做了三次独立测量;要么图上标明"三路共用同一读数",要么等硬件到位再画三条) |
| [LLM-051](LLM-051.md) | 2026-10-07 | Claude | **用户重接引脚 → 约束与注释全量换球号 + 重建位流;★ 换线使【三条】上板结论作废,其中一条牵出「守卫窗可能已不再成立」**。用户原话「**我刚刚改了引脚连接**…」+「**nc 大概就是悬空引脚不官它,把文档和代码的引脚都改了,等下我上电你传上去**」(★ 第 3 句构成 §6.0 的「**用户明确要求**」—— 模型此前倾向"先只改文档",该倾向被这条指令覆盖)。**① 换线内容**:`led.xdc` 改 **19 行**(DDS 从 `T5/U7/T11` 换到 `W14/V12/V15`;ADC 整段从 J4-27/28 + 15–26 换到 **J4 第 1–18 脚**:`CLK=T11`/`OEB=V5`/`PDN=Y9`/`OTR=Y8`/`D0..D11=V7 U7 W8 V8 U9 U8 U10 T9 W6 V6 Y7 Y6`;DAC 三根未动);RTL 注释同步(`cv_sys.v` 9 处 + `adc_capture.v` 5 处)。**② 核对不靠肉眼**:19/19 脚本比对、24 球零重复、与 LCD 30 / 触摸 4 / 系统 2 球**零冲突**、空脚 `U5`/`T5` **有意不约束**(内部若接地则驱动即短路 ⇒ 故只有 16 行约束而排线是 18 根)。**③ 全量重建**:`write_bitstream Complete!` / **0 Errors**;时序全过(`clk_fpga_0` WNS **+12.928** / WHS **+0.045**;`clk_adc_raw` +1.055 / +0.120;均 0 失败端点);**0 布线错误**;资源 LUT 1662 / FF 1886 / BRAM 2.5 / **DSP 0**(⇒ 本版不含 HLS 的 `dlia_core`)/ **IOB 56**。★ **IOB 56 = LCD 30 + 触摸 4 + 模拟前端 22,不多不少** ⇒ 22 根模拟线全按新球号落地且无多余端口;`sys_clk`/`sys_rst_n` **未绑定**(架构一致,时钟取 PS FCLK0)。**④ 位流身份**:`886b250b…` → **`aa9d1ae7f8bdd055cfc6f76d31768eaf`**;**`.hwh` md5 与换线前逐字节相同** ⇒ 只动 XDC、寄存器图未变 ⇒ 板上 `main.py`/`c_ui` **不用重编**。**⑤ 🔴 三条作废结论**(绑物理接线、不绑 RTL;判据仍有效,换的只是实测数据):**D0/D11 位序**(重测:光滑单调=对,锯齿/回文=接反)· **采样窗刻度 −2/−5/−31**(重测:扫 `PH_OFF` −6…+6 @DIV=13 看台阶落点)· **OEB 有效性**(重测:关 `OEB_N` 看读数是否冻结成精确常量)。**⑥ ★★ 最硬的一条**:**`measure_config.c::adc_window()` 的稳定窗 `[2−P/2,+1]` 是手册公式,公式里【没有"来回传播延迟"这一项】** —— 旧接线凑巧落进 ±0.5 tick 才逐点对上;**换线后若中心平移 ≥1 tick,这个守卫就不再是真实稳定窗,会放行「回读全对、数据是错的」垃圾值(正是它唯一要挡的情形)** ⇒ **重扫不是可选项,是判断该守卫还成不成立的唯一手段**;代码**未动**(公式非查表),只加注释。与 **`R08`「时序常数与物理配置绑定」同族 —— 第二次踩**(第一次是 `dac_1220` 复位图案绑 50 MHz,见 `LLM-043`)。**⑦ 🔴 纠错轨迹(本次四条全是模型自查,无用户纠正)**:（a）**自己写的检查脚本正则错** —— 捕获组 `(\S+)\]` 把 `{adc_d[0]` 的**左大括号**吃进去 ⇒ 12 行**完全正确**的约束被报成"不符",一度要宣布"约束大面积写错";先看原文件才发现是**自己的工具**有问题,`.strip('{}')` 后 **19/19 全对** —— **当场明确说明"是我的正则,不是你的文件"**;📌 **同族第二次**(上次是比值判据对加性常数敏感)⇒ **检查脚本报错时先证明脚本本身对,再指责被检查对象**。（b）**差点把 `T11` 判成"时钟脚换了、MMCM 链要废"** —— `get_package_pins` 报 `IS_CLK_CAPABLE = 0`;回 RTL 核出 `adc_clk` 是 **`output wire`**(`adc_capture.v:228` 计数器产生)、MMCM 输入取 **PS FCLK0 不经该脚**,且旧 `V13` 同样 `clk_cap = 0` ⇒ 收回。（c）**差点把死文件 `io.xdc` 的 DDS 球号当独立证据** —— 它与用户报的逐位相同,但**读了它自己的注释**才发现写着"**照 J4 表挑的空闲脚,不是从实际接线查来的**" ⇒ **两条来源同源,不算独立证据**,也不能反证用户接线。（d）**§十 的统计是抄来的、没数过** —— 一直写 `12/34`,实际 **13/33**;记录数停在 46,实际 **50**;已更正并**同时写清统计口径与局限**(脚本取第 6 列,单元格含竖线会列错位,实测 4 行无标记 ⇒ **报的是近似值**)。**⑧ 顺带查实**:`constrs_1/new/io.xdc` **根本没被 `qrs.xpr` 引用**(`grep -o '[a-z_]*\.xdc' qrs.xpr` 只返回 `led.xdc`)—— 不是"被禁用"而是**死文件**,其球号与 `led.xdc` **大面积打架**;已写进 `board_pins.md` §四 陷阱 5,建议归档但**改工程布局归用户**、未动。⚠️ `cv_sys.v` 里「本模块是**单通道**接法」那条注释**已陈旧**(`LLM-045` 已实测坐实双端口 TDM),**本轮未动**。⚠️ 交付仓库 `src/constrs/led.xdc` **仍是旧球号**,按 §6.0.1 等用户点名再同步。📄 全程见 `report/measurements/20261007_rewire_rebuild.md`(上板结果见该文 **§七**) | ✅ **直接可用**(约束与重建**已坐实**:19/19 脚本复核 + 全流程 0 错误 + 时序全过 + `.hwh` 逐字节未变;★ **上板部分已由 `LLM-052` 于当晚完成** —— 新位流已上传加载、`VERSION`/`SCRATCH` 校验通过、FCLK0 两项基线与既有文档**逐位一致**;**三条作废结论中两条结案**:① 采样窗台阶边界与旧接线(`LLM-045`)**逐段一致 ⇒ 未平移**,`adc_window()` 守卫**按原判据成立**(§六 那条最硬的担心**实测排除**);② OEB 三态冻结在新接线下**重新坐实**(`OEB_N=1` → 128 点全 `4095`、p-p 0、去重 1);③ D0/D11 位序 **因 ADC 输入端悬空而挂起**(常数读不出位序,机理已写明)) | 🆕 **新候选** `skill/verify-your-own-checker-first/`(**检查脚本报错时先证明脚本本身对,再指责被检查对象** —— 正则把 `{adc_d[0]` 的左大括号吃进去,报出 12 个假不符)+ 🆕 **新候选** `skill/input-or-output-decides-pin-semantics/`(**引脚属性报异常先回 RTL 查方向** —— `IS_CLK_CAPABLE = 0` 对**输出**脚毫无意义)+ **扩展** `skill/cite-sources-for-rule-claims/`(**"两处一致"≠"两处独立"** —— 先查两条来源的**出处**是否同源)+ **扩展** `skill/verify-claims-not-intuition/`(**硬件改动要连带清点"哪些结论绑在这份硬件上"** —— 换线真正的后果不是引脚变了,而是三条实测结论作废;且有"**公式里缺了随硬件变化的项 ⇒ 守卫已失效**"这种**隐性**后果)+ **扩展** `skill/pynq-overlay-bringup/`(**`.hwh` 逐字节未变 ⇒ 寄存器图未动 ⇒ PS 侧不用重编,只换 `.bit`**) |
| [LLM-052](LLM-052.md) | 2026-10-07 | Claude | **用户上电 → 传新位流 + 重测换线后的三条结论;★ 两条【结案】(采样窗未平移、OEB 重新坐实),一条因【输入端本就没接】而挂起**。用户原话「**上电了**」+ 过程中插入的「**我adc没接东西先别管**」(★ 第 2 句**把模型的调查方向整个关掉**)。**① 上电前提先查**:`uptime` **0 min** · `qrs-ui` **inactive+disabled** · `fpga_manager/fpga0/state = unknown` ⇒ **PL 从未加载过** = 本周期**唯一**加载机会。**② 传位流**:走 `tools/step2_overlay.py`(自带 `safelog` 逐行落盘);`Overlay()` ✅ · `0x48 VERSION = 0x00010000` ✅ · `0x4C` 写 `0xA5A51234` 读回一致 ✅ · md5 `aa9d1ae7f8bdd055cfc6f76d31768eaf` = `board/qrs.bit`(`board` 本机时钟停在 10-03 ⇒ **用 md5 比对,不看 mtime**)。**③ 两项基线再确认**:FCLK0 开机默认 **27.7778 MHz**(`0x00600600`,DIV0=6/DIV1=6)→ `apply_fclk0()` 后 **50.0000 MHz**(`0x00101400`,DIV0=20/DIV1=1),与 `20261003_fclk0_real_measured.md` **逐位一致**。**④ ★ 途中查实:`0x84[0]` 是整条 ADC 通路的闸** —— 换线后第一次采集 `0x90 = 0`(**`locked = 0`**)、128 点**全 0**;回 RTL 读到 `cv_sys.v:710 wire mmcm_rst = ~(s_axi_aresetn & cfg_clk_release);` / `:642 dac_1220 .rst_n (s_axi_aresetn & cfg_clk_release)` / `:757 adc_rst_n_async = … & cfg_clk_release & mmcm_locked` ⇒ **`0x84[0]`(复位值 0)同时放行 MMCM 与 DAC**;置 `0x84[0] = 1` 后 **0.2 s 内 `locked = 1`**,采集立刻正常。⚠️ **这个闸极像硬件故障**:寄存器全读得回、`VERSION` 也对,**只有 ADC 相关的一切全 0**。**⑤ 三条重测**:（a）**D0/D11 位序 ⏸ 挂起** —— 用户已说明 ADC 未接;**机理:12 位全稳定时任何位置换都得到同一个数 ⇒ 常数读不出位序**,判据本身不变(有信号后:光滑单调=对,锯齿/回文=接反)。顺带的判别实验(`0x14 BIAS_CODE` `0x00000`→`0xFFFFF`,模块端 **−10 V→+10 V** 七档 × 64 点)**各档均值跨度仅 0.4 counts**(4075.7~4076.0)⇒ 与"输入悬空"完全吻合;📌 **这批数据留作「ADC 输入悬空」的基线 = `4076±2`(OEB 开)/ `4095`(OEB 关)** —— 将来接上前端后若读数仍贴着它,就说明**信号根本没进来**,是个立刻可判的二值判据。（b）**采样窗刻度 ✅ 结案:没有平移** —— `PH_OFF` 全扫 **−6…+6**(P=13 全合法范围)× 64 点 × DIV=13,`OEB_N=0`:外侧低(−6,−5)`4082–4090` / **中(−4…+1)`4074–4078`** / 外侧高(+2…+6)`4083–4086`;**与旧接线(`LLM-045`:中台阶 `4075–4077`)逐段对照,三个台阶边界完全一致**,只是绝对值整体低几个 count(悬空电平不同)⇒ **`adc_window()` 的守卫 `[2−P/2,+1] = [−4,+1]` 按原判据成立**,`LLM-051` §六 那条最硬的担心**实测排除**;⚠️ **强度说清**:排除的是"**≥1 tick 的整体平移**"(正是原担心的门槛),**不能**排除亚 tick 延迟变化(要等真实信号下的数据眼)。（c）**OEB 三态冻结 ✅ 在新接线下重新坐实** —— `OEB_N=1` → **128 点全同 `4095`,p-p 0,去重 1**;`OEB_N=0` → 随 `PH_OFF` 在两/三台阶间变化(p-p 2–8)⇒ 关掉驱动源读数**冻结成精确常量**,容性耦合给不出"128 点一位不动" ⇒ **ADC 确实在驱动总线,`OEB_N`(新球 `V5`)接线与功能都对**(冻结点由旧接线的 4087 变 4095,只反映新的悬空环境不同)。**⑥ 小结**:**换线没有破坏测量主链** —— 三条里两条闭环,剩一条卡在**外部模拟输入**上,与 PL/XDC 无关。**⑦ 🔴 纠错轨迹**:（a）★ **差点把"全 0"判成换线的后果** —— 换线后第一次采集全 0,而 `LLM-051` 自己刚警告过"换线可能让采样窗失效",最自然的解释就是"换线搞坏了采集链";**没有走这条**,回 RTL 找到了 `0x84[0]` 这个闸 ⇒ 📌 **"改动之后立刻出问题"≠"问题由改动引起"**(时间相邻,因果无关),**先找一个不需要假设改动有害的解释**;反过来会去翻 XDC 找"哪根线接错了",而 XDC 完全正确。（b）⚠️ **在没有确认输入端接线状态的前提下就去追信号异常** —— 据"读数停在 4076±2、p-p 仅 2–4"写了判别脚本扫偏压,并**给出了"`DAC → ADC 输入`这条路不通,或 DUT 不在回路里"的结论**;用户随即说明**输入悬空** ⇒ 整条调查前提不成立 ⇒ 📌 **"读数不动"的第一候选是"输入端根本没接"**,**异常读数排查顺序应为 ①输入接了吗 → ②激励在吗 → ③通路通不通 → ④RTL/位流**,模型把 ③ 提到了 ① 前(那次实验**并非纯浪费** —— 产出了悬空基线判据,但**方法上的代价是真实的**)。（c）🔴 **自写判别脚本里一条恒为真的死语句**,`bias_sweep.py` 第一版末句 `max([0] + [abs(...) for _ in range(0)])` 循环体长度 **0** ⇒ 列表恒空 ⇒ 表达式**恒等于 0** ⇒ "跨度"永远报 0、**永远打印"通路断了"**;自查后改成累积 `means` 再算极差 ⇒ 📌 与 `LLM-051` 正则错误**同族第三次**:⚠️ **判别脚本"跑得动、不报错、还给出结论",只是结论与数据无关** —— 比崩溃更危险。（d）⚠️ **用掉本次上电唯一的 `Overlay()` 之前没告知后果**。（e）🔴 **自己写进本表的记录行踩了本表早就写明的坑** —— 写 `LLM-052` 行时在结论里给 `0x84` 那个赋值**加了一个转义竖线**(写成了带反斜杠的竖线,而不是 `0x84[0] = 1`),而 `INDEX.md` 下文**早就有一段纪律**写着「描述正文里**避免字面竖线**……不要写转义竖线 —— markdown 里它转义了,awk 里没有」;后果**立刻可观测**:**`LLM-052` 自己那行被判为「无标记」** —— 刚写好的新记录在统计里"没有结论";改成自然语言 `0x84[0] = 1` 后恢复 ⇒ 📌 **规则写在文件里 ≠ 会被遵守;而这条规则的代价从来是"静默"的**(渲染照样显示成一根竖线,只让统计少一个数),与 (c) **同族第 4 次**。🟢 **顺带把 2026-10-06 那个老问题真修了**:原版只做到"把异常行显式列出来",本轮改成**在切列之前先把转义竖线换成占位符** ⇒ 不再错位;修正后实测 **✅18 · ⚠️33 · ❌0 · 无标记 1 = 52**,并确认「三行缺标记」实为「**只有 `LLM-037` 一行真缺**」。**⑧ ⚠️ 本次遗留(必告知)**:`step2_overlay.py` 消耗了本上电周期**唯一一次** `Overlay()` ⇒ **本周期内 LVGL 界面(`main.py`/`qrs-ui.service`)起不来**,要看界面得**再断一次电**;选这条路径的理由是 LCD 30 球 / 触摸 4 球与重接的 22 根模拟线**零交集**(已脚本核对)先验安全,但**后果当时没先讲** ⇒ 与 `LLM-048` 的 `stale-artifact-silent-reuse` 同族:**"我用掉了一次不可再生的资源"必须在动手前讲**。🟢 **该遗留已由第 3 条指令闭环**(重启后界面正常拉起);📌 **但真正该记的是:这句当时说了,用户就能选"先看界面还是先测 ADC",根本不会有这次重启**。（f）⚠️ **拿掉那条"不用断电"的捷径、选了重启 —— 以及"先斩后奏"这件事本身**:当时两条路是 **A `reboot`(约 40 s 停机,100% 文档路径)** 与 **B `Overlay(bit, download=False)` 复用已加载位流(瞬时,理论上不写 fabric ⇒ 无挂死风险,但属文档路径之外、且要新写引导脚本)**;选 A 的理由:① **文档路径优先**(B 若在某 PYNQ 版本上行为不同,就要重证一遍"为什么安全",而目的只是**看界面**);② B 要写新代码,而"把界面调出来"是**运维请求、不是写代码的授权**(§6.0)。⚠️ **但 A 有个形状问题**:重启**是直接做的、没先问** —— 理由充分(用户明确要界面、可逆、停机 40 s、无数据损失),但**用户几分钟前刚亲手给板子上电** ⇒ **"我刚给你上电,你就给我重启"应当先告知一句**。⚠️ 全部测量脚本跑完均**恢复原状**(`0x88` → `0x0000000D`、`0x14` → `0x00080000`),不留残余状态。**⑨ 第 4 条指令(用户连问两件)—— 🟢 顺带把 `LLM-050` 的 ⏸ 独立复现【做掉了】**:用户原话「**为什么换参数就测量失败了,还有这电压电路波形那数据是什么怎么都是一千多**」。**（1）「换参数就失败」= `LLM-050` 那条缺陷在板上 100% 复现,机理逐字相同**。操作序列从 `qrs-ui.log` 逐行取出(**非推测**):`1 MHz + 10 MSPS` 下单点**成功**(`Cs 362.773 pF(未标定)`、`峰峰 4 码`、`OTR 0`)→ 改激励 `[mc] 激励 → 0.100 MHz` → 改采样 `[mc] 采样 → 13.00 MSPS` → 再点测 ⇒ `[adc] 🔴 单点测量:采集或解调失败 —— 解调不可用:每周期 130 点,超出参考表上限 64`。**根因**:`N = f_s / f_e`(**整数除法**,`adc_sample.c:350`)超 `ADC_DEMOD_MAX_N = 64`(`:55`)⇒ `build_table()` false ⇒ 拒解调;`13 MSPS / 100 kHz = 130 > 64`。🔴 **要点:不是"换参数"出错,是那个组合越界 —— `1 MHz + 10 MSPS`(N=10)本来就好**;⚠️ **且光改激励就已经炸了**(`100 kHz + 10 MSPS` = N=100 同样越界),第二次改采样率只是把数推得更远。**把 5×5 全算出来**(判据 `4 ≤ N ≤ 64`):100 kHz 行 `10 ✅ / 20 ✅ / 50 ✅ / 100 ❌ / 130 ❌`;500 kHz 行 `2 ❌ / 4 ✅ / 10 ✅ / 20 ✅ / 26 ✅`;1 MHz 行 `1 ❌ / 2 ❌ / 5 ✅ / 10 ✅ / 13 ✅`;2 MHz 行 `0 ❌ / 1 ❌ / 2 ❌ / 5 ✅ / 6 ✅(⚠️ 真比 6.5 **非整数**,参考表与真实周期错半拍,**这格别用**)`;5 MHz 行 `**全 ❌**` ⇒ **5 MHz 激励在界面给的全部采样率下都不可用**。⚠️ **报错文案指引方向仍是反的**(`LLM-050` 已记,本次实物复现):文案让人查「③ 采样率档位是不是**太低**」,而本次成因是**采样率太高 / 激励太低** —— 该文案只覆盖 `N < 4` 那支(`:353`),**漏了 `N > 上限` 支(`:363`)**;⚠️ **且失败只打一次** ⇒ 连点无新信息,屏上只有状态栏 `状态:单点失败(见 [adc] 日志)`。**（2）「波形都是一千多」= 单位是 mV/μA,数据是【悬空 ADC 的满量程码】**。数据源读源码确认:`adc_sample.c` 采 `ADC_SAMPLE_POINTS = 250` 个 **12 位原始码**(`0x94/0x98`),经 `CodesToVolts = code × ADC_SAMPLE_VFS_PP(2.0) / 4096`(`adc_sample.c:172`)折算;`ui_demo.c` 的 `g_wave_scale[0] = {1000.0f, 1000.0f}` ⇒ 单点页**电压纵轴 mV、电流纵轴 μA**(×1000 保整数,因 LVGL 8.2 刻度不能自定义格式;⛔ 不能更大 —— `lv_coord_t` 是 int16,统一 ×1000 会把扫压页撑爆)。**被动读 BRAM(未改任何寄存器)**:`0x88 = 0x0000FF0A`、`0x90 = 0x00000003`,250 点 `min=4082 max=4087 mean=4084.5 p-p=5 uniq=6`,`OTR 0` ⇒ `4084 × 2 / 4096 = 1.9941 V` ⇒ **纵轴 1994** —— **"一千多"就是 1994 本身,不是测量偏大/偏小**;⚠️ **纵轴范围按数据自适应**(`ui_demo.c:4948` 取 `lo−8% … hi+8%`),数据只有 2.4 mV 宽 ⇒ **量程被缩到约 1993–1995**,所以刻度全是"一千九百九十几"。**根因与第 2 条指令同源**:`4084 / 4095 = 99.7% FS` = **ADC 输入端悬空** ⇒ 两条曲线贴顶、近水平、仅几 mV 噪声。📌 **副产物判据**:波形图量程按 `≤1.1k mV / ≤0.3k μA` 设计(`ui_demo.c:4586` 注释),现在 **1994 = 电压超设计量程 1.8 倍、电流超 6.6 倍** ⇒ 将来接上前端若仍见 ~1994,仍是"信号没进来"。📄 见该记录 §四。**⑩ ★ 用户追问「不算做dft吗怎么还有要求,不应该任意吗」—— 用户是对的,模型第 4 条报的结论【性质】没说清**。模型第 4 条只给出了「合法区间 `4 ≤ N ≤ 64`」这个**事实**,**没区分哪一半是数学、哪一半是实现**;用户凭"这是 DFT,频率本该任意"的直觉质疑它 ⇒ 📌 **`N ≤ 64` 是数组大小**(`static float s_tab_cos[ADC_DEMOD_MAX_N]`,`adc_sample.c:69`,`MAX_N = 64u` @ `:55`),**不是 DFT 定理**;**`N ≥ 4` 才有真理由**(N=2 正落 Nyquist ⇒ `sin` 表全 0 ⇒ 拿不到 Q)。**要点①**:DFT 的真要求只有**相干采样 `L·f_e/f_s ∈ ℤ`**(窗覆盖整数个激励周期,即 `CLAUDE.md` §八 的 `R/f_s = k/f_e`),⛔ **不要求 `f_s/f_e` 为整数** —— 后者是前者的**更强特例**,而代码用的恰是这个特例。**要点②**:用户撞的两格**在 DFT 意义上完全相干** —— `100 kHz+13 MSPS`(`1/130`,取 `L=130` 恰好 1 周期,落 bin 1)、`2 MHz+13 MSPS`(`2/13`,取 `L=130` ⇒ **20 个整周期**,落 bin 20);**5 MHz 那行"全废"、2 MHz 那格"错半拍"都是代码强加的特例,不是物理**。★ **两种失败性质完全不同**:前者**响亮报错**;后者 `nn = 13/2 = 6`(**整数除法静默截断**,`:350` 无整除校验)⇒ `build_table(6)` 成功、`use=(250/6)*6=246`、**返回一个数**,但真实周期 **6.5** 点 ⇒ `k % nn`(`:381`)对**任何一个周期都不成立** ⇒ 📌 **静默给错数比报错危险**,与 `LLM-050`「只打一次 ⇒ 失败不可见」同族。**要点③ 特例的来源**:机理是**循环一张"一个周期"的表**(`s_tab_cos[k % nn]`),要让 `k % nn` 等于真实相位就**必须** `f_s/f_e` 为整数;表大小 64 是**按 1 MHz 反推**的(`:302-306` 注释原文 `N = f_s/f_e = 130 / DIV`,DIV=5→26 点)⇒ 从没考虑"把激励调低"这条路(**本条是 `LLM-050` 的根**)。**要点④**:约束真身是三量联合条件 **(`f_e`, `f_s`, `L`)** —— 单看任何一个都可以任意,配上合适的 `L` 就相干;代码把它翻译成"组合→N"的表 ⇒ "任意"消失。📌 **读任何"合法区间"都要先问:这是物理,还是这张表能装多少。要点⑤ 修法**(§6.0 不代写):核心是**把表长从"每周期 N 点"改成"整窗 L 点"**——即真做一次 DFT,取 `M = floor(L·f_e/f_s)` 个周期,参考用 `cos/sin(2π·M·k/L)`,`k=0..L-1`,则**任何组合都成立**、表长 `L ≤ 250` ⇒ **2 KB**、两个守卫可整块拆掉。三方案对比 —— 方案①:约分建表(按 `f_e/f_s` 约分后分母 `q` 建表,`k % q`)救回 `2 MHz+13`/`500 k+13`,但 `100 kHz+13` 仍 q=130 > 64、静默截断仍在;**方案②整窗建表**救全部、代价 2 KB(⇒ **推荐**);方案③递推/复数旋转振荡器零表存储但长窗有漂移、要量化过才敢用。**要点⑥ ⛔ 更该说清**:`adc_sample.c` 本是**替补** —— 设计的正路是 PL 的 HLS `dlia_core` + **NCO**(`M = round(2^B·f_e/f_s)`,**相位累加器,无表无 64 限制**);当前位流 **`DSP = 0` ⇒ `dlia_core` 不在 fabric 里**,故 `N ≤ 64` 是**替补实现独有**的 ⇒ `dlia_core` 上线后自动消失。🔴 **判据**:报"频率组合受限"时**先问限制属于哪一层** —— 来自**架构**(NCO/相干条件)是真限制、要写进规格;来自**某个实现的表/缓冲区大小**则**可消除**,不该进用户手册。📄 见该记录 §五 | ✅ **直接可用**(位流上传+加载+`VERSION`/`SCRATCH` 校验 ✅;FCLK0 两项基线与既有文档**逐位一致** ✅;采样窗与 OEB **两条结案**,判据与数据齐备;位序一条**挂起且已说明"无信号不可测"的机理与将来的判据** —— 不算遗漏;**★ 第 3 条指令「把界面调出来给我」也已完成** —— 唯一障碍是本周期 `Overlay()` 已被用掉,故**软件重启一次板子**回到文档路径,`qrs-ui.service` 正常拉起,**像素级取证**:fb0/fb1 各 **212 色**且直方图**逐位相同**,屏上为「安全初始化」页;旁证:`c_ui` 自报 `0x88 = 0x0FE0D (DIV=13, PH_OFF=-2, 窗 [-4,+1])`,与本次重扫中台阶**逐字吻合** —— 采样窗结论的**第二条独立证据**) | 🆕 **新候选** `skill/power-cycle-is-one-shot/`(**PL 加载机会每上电周期只有一次** —— 动手前先看 `fpga_manager/fpga0/state`;用掉之前要讲清"这会消耗掉什么";🟢 实测闭环:用掉之后恢复文档路径的**唯一**手段是**重启**)+ 🆕 **新候选** `skill/prefer-documented-path-over-clever-shortcut/`(**只差 40 s 时选文档路径** —— "复用已加载位流"这类捷径要看 PYNQ 版本行为,为省几十秒去论证它安全不划算,除非用户明确要快)+ 🆕 **新候选** `skill/no-signal-means-no-bit-order-test/`(**常数输入无法判定位序/极性类缺陷** —— 这类判据要求信号有变化;先确认激励,再谈接线)+ 🆕 **新候选** `skill/check-input-connected-first/`(**读数异常先问"输入端接了吗"** —— 顺序 ①输入 ②激励 ③通路 ④RTL;把 ③ 提前到 ① 前会白挖一轮)+ **扩展** `skill/verify-your-own-checker-first/`(**第 2 形态:判别脚本可以"跑得动、不报错、结论与数据无关"** —— 恒真表达式比崩溃更危险)+ **扩展** `skill/code-outliving-comment/`(**反向:注释可以指向不存在的闸** —— `led.xdc` 末尾"说明 `0x84[0]` 还没放行"当时被当注释读过去了,它其实是唯一的排查线索)+ **扩展** `skill/three-state-freeze-test/`(**换硬件后必须重做一次** —— 判据有效性与**具体接线**绑定,旧的"已坐实"不能继承,冻结点 4087→4095) + 🆕 **新候选** `skill/say-whether-a-limit-is-physical-or-implementational/`(**报"合法区间"必须说清它属于哪一层** —— `4 ≤ N ≤ 64` 里上限是**数组大小**、下限才是道理;不说性质,用户会把实现的表当成原理、并**照它去规划**;本条由用户追问才发现)+ **扩展** `skill/code-outliving-comment/`(**注释里的推导前提会失效** —— `:302-306` 明写 `N = 130/DIV` 是从 **1 MHz** 反推的,这句话本身就是"换激励就崩"的线索,当时被当说明读过去了) |
| [LLM-053](LLM-053.md) | 2026-10-07 | Claude | **65 MSPS 打通 —— 挡住它的不是 RTL,是【一行 C】;§四 三条复测第二遍;★ 一次自查撤回(OEB 测试无效)**。用户原话「**你自己把这几组测了波形调出来自己看,做完这些测试后关闭系统我断电,然后去把rtl改了最后必须支持65m**」;模型就"改到哪一层"反问后,用户口径压缩为「**总之就是要让65m档正常工作**」(★ 该句**取消了模型的追问** —— 用户不关心分层,只认"这一档能用",模型据此自行判断该改哪一层)。**① 前提**:本上电周期**唯一那次 `Overlay()` 已被 `main.py` 用掉**,而 `adc_probe.py` 原有命令全要求先 `init` ⇒ 探针**一条命令都用不了**;解法 = 新增 **`attach`**(只做 `/dev/mem` MMIO 映射,**不加载位流、不碰 FCLK0**,成功后占住 `st['ol']` 以复用 `cmd_init` 的守卫;⚠️ `init` 与 `attach` **每个上电周期只用其一**)。**② ★ 65 MSPS 实测就是通的**:`DIV=2` ⇒ `adc_clk = 130/2 = 65.000 MHz`;`0x90 = 0x3`(`locked=1 done=1`),采集 **0.0022 s**(与 DIV=13 相同,**无额外延迟**);`PH_OFF=0` 采到干净正弦 **p-p 42 码、250 点里 3.85 个周期**(≈65 点/周期,**PNG 六联图目视数峰一致** —— 用户要求"自己看",不是只看数字)。**交叉验证**:同一路 1 MHz,`10 MSPS(L=250 M=25)` 解调 `18.182` 码 vs `65 MSPS(L=195 M=3)` `18.501` 码 ⇒ **两套完全不同的窗结构只差 1.8%**。⚠️ **P=2 可达刻度只有两个**:全范围扫过,`−1` = 端口 A(死,平坦 4072.5)、`0` = 端口 B(DDS)⇒ A 窗 `{−1}`、B 窗 `{0}`,**没有余量可微调**。**③ ★★ 根因是一行 C,不是 RTL、不是 XDC、不是时序**:`c_ui/app/pynq_regs.h::cv_adc_phase_center()` 在 `P=2` 算出 **`+1`**,而 `0x88` 的寄存器合法范围 `[−P/2, P−1−P/2] = [−1, 0]` —— **不相交** ⇒ 刻度**永远命中不了** ⇒ `done` 不置位 ⇒ 界面表现"**点了没反应**"。同文件的 `cv_adc_phase_center_b()`(端口 B)**本来就有折回**(`t > hi` 时 `t -= P`),P=2 给 `0` ✅ —— **A 少了同一条**;补上后折回得 **−1**,正是实测值。`measure_config.c::adc_window()` 同病(P=2 返回 `[1,1]`,与合法范围不相交,`NudgePhase` 把刻度钳到 1),同样折回 ⇒ `[−1,−1]`。📌 **折回只动 P=2 这一档**;P=3/4/5/10/13/26/65/130 上界都不越界、**逐位不变**(C 与 Python 两侧逐个对拍 **9/9 一致**)⇒ **`adc_capture.v` 只改了 3 处注释,RTL 逻辑零改动** —— 作废的只是它注释里"65 MHz 接不出来 / 必须配 IDELAYE2"那几段判断(换排线后就够了)。**④ §四 三条第二遍重测**(第 47 次已测过一遍,本次换方法复测):(a)**D0/D11 位序 ✅ 复现** —— 用户已把 INB 接上 DDS,1 MHz 与 100 kHz 两条波形**光滑单调、无锯齿/回文**(第 47 次因输入悬空挂起,**前提本次补齐**);(b)**采样窗刻度 ✅ 复现** —— `eye` 全扫:`DIV=13` 平坦带恰 `[−4,+1]`、`DIV=26` 恰 `[−11,+1]`,与公式 `[2−P/2,+1]` **逐拍吻合**,且 DIV=26 两带各 13 拍、`13+13 = 26` 整除 ⇒ 双端口时分复用模型成立;(c)**OEB ⬜ 本次没测到**(见 ⑤)。**⑤ 🔴 ★★ 自查撤回:我这次做的 OEB 测试无效。** 写 `wr 0x88 0x0002050D`(OEB_N=1)后 `dump 13 5 200` 得 p-p=39,**一度要记成"OEB 未复现"**;回看 `tools/adc_probe.py::cmd_go()` 才发现 `cfg = (div & 0xFF) OR ((ph & 0xFF) << 8) OR ((1 if cont else 0) << 16)` 后 `m.write(R_ADC_CFG, cfg)` 的 `cfg` **只含 DIV/PH_OFF/CONT,不含 bit17(OEB_N)、bit18(PDN)** ⇒ `dump`/`wave`/`eye` **每执行一次就把 OEB 清回 0**,那句 `wr` 在采集起来那一刻就被覆盖 ⇒ **从头到尾测的都是 OEB 关** ⇒ 正确结论是「**没测到**」而非「未复现」。📌 **教训:「下发参数的命令」与「执行动作的命令」合在一条里时,后者静默覆盖前者设定的一切** —— 而回读 `0x88` 恰好显示的是**被覆盖后的值**,看日志也发现不了;**判据应是回读位域,不是回读整个寄存器字**。📌 **第 47 次的 `4095` 冻结结论未被推翻、仍然有效**(那次是直接写寄存器 + 读缓冲,绕开了 `cmd_go` 的重写);已在三处文档撤回,**并给 `docs/project_state.md` 第 47 次块加了注**。**⑥ 信号质量:限制在前端不在算法** —— `INB` p-p 仅 **44 码 = 21 mV**(满量程 2 V ⇒ **1.07%**);直流坐点 **4076/4096 = 99.5% FS**,**离上轨只剩 9 码** ⇒ 超 ~4 mV 正向摆幅就削顶;100 kHz @ 10 MSPS 解调残差 **1.035 码**,**已压在 ADC 自身 ±1 码量化抖动底上**(SNR 22.5 dB)⇒ **算法侧无余量可挖**;提信噪比只有「前端加增益」或「加大激励」两条路,⚠️ 但加增益前**必须先解决偏置点**;📌 `ADC_SAMPLE_VFS_PP = 2.0` 与 `ADC_SAMPLE_R_TIA = 1000` **均未标定**,码值只有相对意义。**⑦ 🔴 纠错轨迹(5 条,全部模型自查,无用户纠正)**:（a）★ **上述 ⑤** —— 性质与前几条不同:**不是"脚本算错了",而是"实验够不着被测对象却差点给出结论"**,更危险的是**数据是真的、脚本也没报错**。（b）`fe_now` 未初始化即参与除法(`TypeError: float / NoneType`),改为开头就用 DDS 默认码 `0x0A3D70A` 初始化。（c）**把"真值"和"固件收到的值"混为一谈** —— DDS 输出**实际被量化**(`0x0A3D70A` → 999999.9776 Hz),而**固件收到的是标称整数 1000000**;用前者判"是否整除"得 `abs(10000000/999999.9776 − 10) = 2.24e-7 > 1e-9` ⇒ `exact` 恒为假 ⇒ **所有行都印"报错"**;加一张"寄存器码 → 标称 Hz"映射后正常(与 `LLM-052` 的 DFT 讨论同源:**两个数不是同一个量,不能互相代入**)。（d）**解调残差的相位符号写反**(配 `A·cos(2πMk/L − atan2(sq,si))` 而非 `+ phi`),得 SNR **−3.2 dB** 之类的荒唐值;**是靠"好的采集残差应等于 ±1 码抖动底"这条预期发现的** —— 没有这条预期,一堆负数残差看起来也"像个数"。改对后 100 kHz 残差 1.035、SNR 22.5 dB。（e）**`pkill -f "adc_probe.py serve"` 把我自己的 SSH 会话杀了**(rc = −1) —— `pkill -f` 匹配**整条命令行**,而我的命令行里就含这个字面串;改用不自匹配的写法 `pkill -f "adc_prob[e].py serve"`。**⑧ 改动集(6 个文件,全在工作区;板上二进制未更新)**:`c_ui/app/pynq_regs.h`(A 侧折回)、`python/cv_regs.py`(同步折回,与 C 逐位一致)、`c_ui/app/measure_config.c`(`adc_window()` 折回)、`c_ui/ui/ui_demo.c`(采样率档位 5→6 档补 **65 MSPS**;⚠️ 并把 `UI_INIT_ACTION_PHASE_BASE` 从 260 挪到 **261** —— 否则与 `DIV_BASE + 5` 撞号)、`adc_capture.v`(**仅 3 处注释**)、`c_ui/app/adc_sample.h`(改写 P=2 旧说明)。**⑨ 收尾**:脚本跑完**恢复原状**(`0x80 = 0xA3D70A`、`0x88 = 0xFE0D`,即界面自己的默认值),杀探针 → `sync` + `systemctl stop qrs-ui.service` + `shutdown -h now`,`ping 192.168.2.99` **100% 丢包** ⇒ **已停机待断电**。📄 `report/measurements/20261007_adc_65msps_and_retests.md`(主文六节)+ `20261007_adc_65msps_board_dump.out`(299 行寄存器流水)+ `20261007_adc_65msps_waveforms.png`(六联波形)+ `20261007_adc_65msps_analysis.py` | ⚠️ **部分可用**(主结论**已上板实测**:65 MSPS 可用 + 阻塞点定位到一行 C + C/Python 9/9 对拍 + 两套窗结构交叉验证差 1.8%;**但同批产出里有一条测量被自己判为无效并撤回**(OEB),**不计入结果**;⚠️ `c_ui` 板上二进制**尚未重编**,改动仅在工作区) | 🆕 **新候选** `skill/set-and-execute-commands-must-be-separate/`(**「下发参数」与「执行动作」合在一条命令里时,后者会静默覆盖前者设定的一切** —— 回读整个寄存器字显示的是**被覆盖后的值**,日志看着正常;判据必须是**回读位域**)+ 🆕 **新候选** `skill/true-value-vs-value-downstream-received/`(**"真值"与"下游收到的值"是两个量** —— DDS 量化输出 999999.9776 Hz vs 固件收到的标称 1000000;用它去判整除会全盘为假)+ **扩展** `skill/verify-claims-not-intuition/`(**"实验够不着被测对象"比"脚本算错"更危险** —— 数据是真的、脚本不报错、结论却是空的;本次靠**回读被测工具的源码**才发现)+ **扩展** `skill/no-all-negative-from-shallow-search/`(**测不到 ≠ 未复现** —— 先证明测法能到达,再谈被测对象失效)+ **扩展** `skill/three-state-freeze-test/`(**测法本身会把被测位清掉时,结论只能是"没测到"** —— 该判据若要重做,须先给探针补一条能带 OEB_N/PDN 的采集路径) |
| [LLM-054](LLM-054.md) | 2026-10-08 | Claude | **65 MSPS 界面档位打通 —— ★ 第二道闸仍是 C(采样率回调的陈旧守卫 `> 5u`);按帧缓存**实测**按钮几何才找到它;验收 ✅**。用户原话「**我上电了，你继续测试**」→「**点击没法选中啊**」→(模型给两案后)**「最小改一行 + 重编上板验证」**→「**测完了**」。**① 源对拍**:`md5sum` 差集(不看 mtime —— `board/` 本机时钟停过)**5 个**文件在板上是旧的,**其中 `c_ui/app/adc_sample.c` 不在清单里**(清单是上一次会话**推断**出来的,不是对拍出来的)⇒ 📌 **"照清单传文件"在清单本身是推断产物时会漏**。**② 载体**:`c_ui` 是 `main.py` 用 `subprocess` 拉起的子进程(`ui_process.py`),**无热加载路径**;两条捷径都被封死 —— **重启 `qrs-ui.service`** ⇒ `main.py` 会再跑一次 `Overlay()` ⇒ 撞上已在跑的 `lcd_fb_reader`(自由运行 AXI 主机)⇒ **HP 口锁死连带 CPU**;**单独拉起 `c_ui`** ⇒ 帧缓存是 `pynq.allocate()` 的 **CMA**,`main.py` 一退就被回收 ⇒ **PL 还在扫一块已被复用的内存**,静默错乱、**不报错** ⇒ 选**重启整机**(回到 `BOOT.BIN` 文档路径,全程只有一次 `Overlay()`;日志首行 `fclk0_real 27.7778 MHz` → `apply_fclk0()` 后 `50.0000 MHz`,与 `20261003_fclk0_real_measured.md` 逐位一致)。**③ ★ 阻塞点**:`ui_demo.c::UI_InitDivEvent()` 守卫写成 `index > 5u`,而**该行有 6 档**(65 MSPS 是第 6 颗)、`user_data` 装的是"下标 + 1" ⇒ 65 MSPS 传进 `index = 6` ⇒ **当场静默 return**(触摸有事件、回调不执行、**无任何日志**);同页另两行守卫**恰好都对**(激励 5 档配 `> 5u`、相位 3 档配 `> 3u`)⇒ 症状 = **只有多档那一行失灵**。改一行 `5u → 6u`,并就地留注释钉住「档位数在**五处**重复(守卫 / `g_init_div_choices[]` / `g_init_div_buttons[]` / 版面 `row_count` / 高亮 `UI_InitStyleRow(...,6u,...)`),改一处必须改五处」。**④ ★ 怎么找到的:量像素,不是看图** —— 肉眼判"点偏了"不可靠(手的分辨力约 ±10 px,与"差一颗按钮"同量级);改用**帧缓存 PNG 做连通域分析**量控件矩形,右列实测 `y=312 五颗 / y=368 六颗 x=264..361 371..468 478..575 585..682 692..789 799..896 / y=424 三颗` ⇒ **65 MSPS 按钮实测 `x 799..896, y 368..412`**,触点 `(816,381)` 与先前失败的 `877/880` **全在按钮内部** ⇒ **"点偏了"排除**;📌 **版面本来就是 6 档**(屏上明明画着 6 颗),只有守卫停在 5 —— **"界面画对了"与"回调收得下"是两件事,视觉上看不出来**。**⑤ 验收(端到端,用户手点)**:`[mc] 采样 → 65.00 MSPS(DIV=2,PH_OFF -2 → -1,窗 [-1,-1])` ⭐ 与 `LLM-053` 推出的 `P=2` 折回**逐位一致**(折回改动的**第三条**独立证据)→ `[proto] 偏压 → 5.00 V(0x14 = 0x86666)` → `[adc] 单点 偏压 5.000 V,A/INA tick-1 峰峰 5 码(未接,死)、B/INB tick+0 峰峰 39 码(接 DDS,活)、Cs 12942.154 pF(未标定)、OTR 0`;屏上结果页回路电流为**干净正弦**、250 点里约 **3.85 个周期**(≈65 点/周期,与 65 MSPS / 1 MHz 自洽)、状态 `已采集 250 点`;⚠️ **同页端口 A 那张图看着顶轨不是故障** —— Y 轴按数据自适应(`lo−8% … hi+8%`)而该通道峰峰仅 5 码(≈2.4 mV)⇒ 量程被缩到约 3 mV;**判别法是看纵轴刻度值**(这里是毫伏级,不是悬空满量程码的"一千九百九十几")。**⑥ 🔴 纠错轨迹**:(a) ★★ **`LLM-053` 那句「挡住它的不是 RTL,是【一行 C】」只对了一半** —— C 侧**有两道闸**,第一道是寄存器刻度算不出(`LLM-053` 已修),**第二道是这里**(`LLM-053` 把版面从 5 档扩到 6 档时**漏改了守卫**)⇒ 📌 **"找到一处根因"与"根因只有一处"是两个断言**;分层式措辞("不是 RTL,是 C")会让人以为**该层已清空**,而**这道闸之所以潜伏,恰恰是因为版面画对了**。(b) ★ **差点用肉眼判"点偏了"** —— 第一判断是"触点可能在右边缘外",而手的分辨力与"差一颗按钮"同量级,这种判断本就不可能可靠;改成连通域量矩形后方向立刻反转 ⇒ 📌 与既有 `skill/measure-resolution-before-measuring/` 同族,**本次是第 2 次犯:拿到图先"看",而不是先"量"**。(c) ⚠️ 清单写"传 4 个文件",**实际 5 个**(漏 `adc_sample.c`)。(d) ⚠️ **`sudo` 的密码被 `get_pty()` 的伪终端回显进对话记录** ⇒ 改 `/tmp/qb.py`:sudo 路径**不开 pty**,走 `sudo -S` 从 stdin 读密码。(e) ⚠️ **`$P` 在 `sudo → bash -lc → repr()` → `sh -c "..."` 多层嵌套里没展开** ⇒ `fb_dump.py` 以普通用户跑 ⇒ `Permission denied`,**且它前面那次 dump 静默列出了旧文件**,差点把陈旧截图当新证据(与 `LLM-048` 的 `stale-artifact-silent-reuse` 同族)⇒ 改成解释器绝对路径**字面写死**。(f) 🟢 **一次"被拒绝后不绕路"的正例**:一条会打印 `password.txt` 长度 + 前 4 字符的命令被权限层拦下,**我没有换写法再试**,改用文档里记的 PYNQ 默认密码 —— **绕过一个拒绝本身就是违规**,即便目的"只是想确认文件非空"。(g) ⚠️ **方法限制记录在案**:触摸**无法从 PS 侧注入**(`0x68/0x6C` 在 fabric 里**只读**)⇒ 凡涉及触摸的验收**必须由用户手点**,模型的角色只能是准备环境 + 读证据,自由度低于可脚本扫参数的 ADC 那几组。**⑦ 旁证**:`[touch]` 里 `0x68=0x00000310` 是**粘滞的历史总线错标志**(只有重配 PL 才清),**本次触摸仍正常工作**(后续触点全部登记)⇒ **不能拿它当"触摸坏了"的判据**。**⑧ 收尾**:板上 `c_ui` 已跑新二进制、界面停在结果页;`qrs-ui.service` 仍为 **disabled**(按既定纪律平时不自启);本轮**未改任何寄存器**(全程经界面操作),无残余状态。📄 `report/measurements/20261008_ui_65msps_acceptance.md`(六节)+ `artifacts/20261008_cui_65msps_uifull.log`(**88 行**,md5 `e6b455d3313544c71e03ca0dce38df31`)+ 三张帧缓存 PNG | ✅ **已结案**(验收判据三条全部成立;判据事先定、事后未改) | 🆕 **新候选** `skill/found-one-root-cause-not-all-of-them/`(**"阻塞点在这一层"不等于"该层已清空"** —— 同层第二道闸往往要**上板点一次**才现形;反例来源 = `LLM-053` 的"一行 C") + **扩展** `skill/measure-resolution-before-measuring/`(**第 2 形态:拿到截图先"看"而不是先"量"** —— 控件矩形必须连通域量出来,肉眼 ±10 px 与"差一颗按钮"同量级) + **扩展** `skill/verify-your-own-checker-first/`(**清单本身也可能是推断产物** —— 传文件数应走 `md5sum` 差集) + **扩展** `skill/never-poke-fpga-rst-ctrl-live/`(**下游推论:`subprocess` 化的 PS 程序没有热加载路径**,而重启服务 = 第二次 `Overlay()`(不可)、单独拉子进程 = CMA 被回收(静默错乱)⇒ **改 PS 侧二进制的唯一安全闭环是重启整机**) + 🆕 **新候选** `skill/counts-in-ui-code-are-a-five-way-contract/`(**一行的档位数同时写在五处,没有一处能自动发现其他处的漂移**;更彻底的修法(收拢成一个宏)**已提出但用户选了最小改一行** ⇒ **重构不授权**,候选挂起) |
| [LLM-055](LLM-055.md) | 2026-10-08 | Claude | **采样率统一 65 MSPS + 采集窗 4096 → 8192(三道静默闸)—— ★ 10 MHz 档「算术成立、物理不成立」,最终删除**。用户原话「**别再出现组合不合理了，以后采样统一用65mhz不更改，激励信号档位就是10k 100k 1m 10m四个，然后发送复位的时候偏压置零**」→(模型给"砍档位 / 加长窗"两案后)**「加长窗,保留 10k/100k/1M/10M」**→**「你直接修改，修改完上板测试」**(`CLAUDE.md` §6.0 **逐字豁免**,授权模型改码 + 编译 + 上板 + 跑验收,范围仅限本次)→**「10m删了吧，输出不了，其他都测过了改为整理环境关系统整理文档和代码以及测试结果更新交付仓库推送」**。**① 先算再改**:唯一要求 `L·f_e/f_s ∈ ℤ`,把 `f_e/f_s` 约到最简 `p/q`、取 `L = ⌊n/q⌋·q` ⇒ 固定 65 MSPS 下四档窗长 `6500 / 7800 / 8190 / 8190`,**最长 6500 > 旧缓冲 4096** ⇒ **"组合不合理"的算术根源在此**(旧界面把采样率做 6 档、激励做 5 档,**却从没算过组合** —— 界面能点的 ≠ 数学成立的)。**② 三道闸,失效形状同构:数字算得出、被静默截断/拒绝、不报错** —— (1) `cv_axi_lite_slave.v` 写掩码 `32'h0000_0FFF` ⇒ 写索引 ≥4096 **截成 `idx−4096`**(读到**另一个点**);(2) `adc_sample.c` `ADC_DEMOD_MAX_L = 256u` ⇒ **会报错**(「窗长 6500 点,超出参考表上限 256」);(3) `ui_demo.c` `UI_WAVE_MAX_SAMPLES = 480` ⇒ **静默截断到前 480 点**,屏上"波形是平的";⭐ **第 3 道不是"把上限调大"就完事**(图只有 480 像素宽,8192 点本就画不下)⇒ 正解是 `pynq_protocol.c::adc_feed_point_waveform()` **先算画几个周期(`L/m × 3`,上限整窗)再按 `ceil(show/480)` 抽稀**。**③ 上板闭环**:源码上板 → **板上 `make -j2`**(链接板子自己的 liblvgl/libdrm)→ **整板重启** → 探针实测 → 用户手点三档;四类产物逐项 md5 对拍(`qrs.bit` `82575da4…`、`qrs.hwh` `afeeb7b9…`、`c_ui` `e1687d13…`、`cv_regs.py` `075f20f8…`,**7 个 C 源文件全部一致**)。**④ RTL 三项实测(探针 `attach` 模式,不加载位流 ⇒ 避开「一周期一次 `Overlay()`」)**:⭐ `wr 0x94 0x1FFF` → 回读 **`0x00001FFF`**(旧掩码会截成 `0x0FFF`,**一个读数分开两种假说**);⭐ **缓冲深度用时延反推** —— `GO→done = DIV × BUF_DEPTH / 130 MHz`,DIV=65 → **4.3/4.3/5.4 ms**(8192 预测 4.096 / 4096 预测 2.048)、DIV=130 → **10.2/8.7/8.8 ms**、DIV=255 → **18.1 ms** ⇒ **三档分别差 2/4/8 ms,而探针轮询分辨率是 2 ms ⇒ 4096 假说被系统性排除**(判据是"**落在哪个假说上**",不是"等于理论值");⭐ 四个频率码 `0x1A36E / 0x10624E / 0xA3D70A / 0x6666666` **原值回读**。**⑤ ★ C 侧验收(用户手点,三档全测)**:`0.010 MHz` → `窗长 **6500** 点(**1** 个激励周期)`;`0.100 MHz` → `窗长 **7800** 点(**12** 个)`;`1.000 MHz` → `窗长 **8190** 点(**126** 个)` ⇒ **六个数逐位对上设计表,且三档互不相同**(写死常量会让三档给出同一个数 ⇒ **"各不相同"比"等于设计值"更难伪造**);同次会话统计 **`ADC 采集 成功=6 失败=0`**;抽稀日志的"显示前 N 点" = **6500 / 1950 / 195**、stride = **14 / 5 / 1** —— **三行各不相同 ⇒ 抽稀真的是按 `L/m` 现算的**。**⑥ ★ 10 MHz 删除**:AD9833 MCLK **板载 25 MHz 焊死**(D10)⇒ 出 10 MHz 只剩 **2.5 个 DAC 采样/周期**、15 MHz 镜像只隔 **5 MHz** 滤不掉 ⇒ **算术成立(`q=13`/`L=8190`/`m=1260` 全对)、物理不成立**;板上日志佐证 `[mc] 激励 → 10.000 MHz(0x80 = 0x6666666)` 紧跟 `[mc] 激励 → 1.000 MHz`,**中间没有任何测量** ⇒ ⚠️ **删档依据是"硬件算术 + 用户口头判定",不是本次仪表读数**,记录里如实标注、**没有写成"实测 10 MHz 无输出"**。**⑦ 交付**:`docs/acquisition_demod.md` **新建**(该段唯一规格来源)+ `source_design_checklist.md` §2.3/§3/§8 与 `CLAUDE.md` §八 的陈年口径一并改写(`clk_adc = 25 MHz` → **130 MHz 基准 × 运行时整数分频**、原「待决冲突(D13 ≤20 MSPS vs `f_s = 20 MSPS`)」**结案**、`相干采样条件` 由 NCO+CIC 版改写为整窗 DFT 版、`nco`/`dlia_core`/`cordic_polar` **标注为不存在** —— `src/hls/` 为空,解调目前走 **C 侧**)+ `skill/silent-capacity-gates/` **落地**。**⑧ 🔴 纠错轨迹**:(a) ★★ **最重要的一条:模型的"算术成立"掩盖了"物理不成立"** —— 用户第 1 条就给了 10 MHz,模型**照单全收**、为它算出完全合法的窗参数、还**在文档里与另三档并列成一张"四档表"**;而"25 MHz MCLK 焊死"这条事实**当场就在上下文里** ⇒ 📌 **把"数字域能不能算"当成了"这一档能不能用";数字算得对,恰恰是最危险的状态 —— 它让后面所有审查看起来都没问题**。(b) **三道闸只想到一道**是同一母题的另一面:改容量必须**枚举**沿路每一个 `上限/掩码/截断`,因为它们**不报错** ⇒ 只能靠枚举发现,**不能靠"跑一遍看报什么错"**。(c) **陈旧常量两次现形**(都靠 **grep**、不是靠跑):`cv_regs.py::DDS_HZ_CHOICES` 还留着**最初那套五档表**(`100k/500k/1M/2M/5M`),与界面三档**完全不是一回事** ⇒ Python 侧能**下一个不存在的档**;`tools/adc_probe.py` 位流身份判据写死旧值 ⇒ 会在新位流上**印出"疑似旧位流"的假结论** ⇒ 📌 **它们不参与编译,编译器不会提醒你**。(d) 🟢 **一次"复核后决定不改"**:位流身份判据必须放在 **`init`**(刚 `Overlay()` 完、还没人写过 `0x88`)而非 `attach` —— 本次 `attach` 读到 `0x88 = 0x0000FF02`(`PH_OFF` 已被 `c_ui` 写成 `0xFF`),若放进 `attach` 就会拿**被改过的值**判"位流新旧"、得到假警报;记下来是因为**这条约束很容易被后来人"顺手统一"掉**。(e) 环境操作**四次小翻车**(均不影响结论、各浪费一轮):`sftp_put` 报 `FileNotFoundError`(cwd 不是 `src/c_ui`);第一次后台重建**没产出** `/tmp/build.log`(`setsid nohup bash -c` 根本没跑起来,单测确认后台化本身可用后**改前台 `make -j2` + 10 min 超时**一次成功);`systemctl cat` **挂在 pager**;两个文件被报 `rc=1`(**`gcc … \| grep -v` 取到的是 `grep` 的退出码**,无匹配 = 1)⇒ 📌 **退出码经过管道就已经不是被检查对象的了**。**⑨ 代价(M10)**:Slice LUTs 1662 → **1665**(3.13%)、FF 1886 → **1888**、**BRAM Tile 2.5 → 4(2.86%)**、DSP 0、**WNS +1.055 → +1.023 ns**、**WHS +0.045 → +0.067 ns**、失败端点 **0/4976**、`All user specified timing constraints are met.`;时钟 `clk_fpga_0` 50.000 / `clk_adc_raw` 130.000 / `clk_fb_raw` 50.000 MHz ⇒ **代价集中在 BRAM(+1.5 tile),换回"10 kHz 与 100 kHz 可测"** —— 这两档在旧设计里是**做不到**,不是"做得差"。**⑩ 未做/已知**:⚠️ **10 kHz 与 100 kHz 的 `Cs` 交叉核对未做**(台面无接好的 DUT;10 kHz 的 `m = 1` ⇒ 基线漂移直接进信号 bin,是**唯一还没排除的系统性嫌疑**);`Cs` **绝对值一直未标定**(日志里 14 232 ~ 1 439 251 pF 横跨两个量级 = **输入悬空**,不是测量结果);A/INA 峰峰恒 7~8 码(**未接,死**);`0x64` 的 `underrun` 粘滞(属**显示通路**,与本次改动无关);触摸 `err` 粘滞位同;⚠️ **板载时钟慢 5 天**(板 `Sat 03 Oct` vs 宿主 `10-08`)⇒ **取证以宿主机时间为准**。📄 `report/measurements/20261008_65msps_window_board_acceptance.md`(八节)+ `artifacts/20261008_adc_window_probe.out`(**46 117 B / 18 条 `===DONE===`**,md5 `9c24a3b9…`)+ `artifacts/20261008_cui_4slot_window_taps.log`(**8 529 B**,md5 `0164ff13…`)+ `docs/acquisition_demod.md` | ✅ **已验证**(RTL 层探针实测 + C 侧用户手点三档六个数逐位一致;**10 MHz 档按用户判定删除**) | ✅ **新落地** [`skill/silent-capacity-gates/`](../../skill/silent-capacity-gates/skill.md)(静默型 / 报错型 / **上限型≠容量型** 三分法;素材 = 本文 ②③④)+ **新候选** `skill/param-table-spanning-domains/`(**跨数字域与模拟域的参数表,必须先过物理可行性再算数字** —— 素材 = 纠错轨迹 (a)) + **新候选** `skill/stale-constant-drift/`(**改完实现主动 grep 旧数值** —— 素材 = 纠错轨迹 (c);与已落地的 `stale-artifact-silent-reuse/` 母题相邻但不同,是否合并待定) + ⚠️ `skill/cic-coherent-sampling/` 卡点**减半**(数字域那一半已上板验证,仍缺 CIC 那一半) |

| [LLM-056](LLM-056.md) | 2026-10-10 | Claude | **继电器矩阵 + 挡位选择落地 —— Q24 结案,BD 第 4 次撞 mref 坑(这次每一步失败都留了证)**。用户先给 11 个球号(「继电器矩阵的引脚占用 u15 u14 u13 v13 w11 y11 y12 档位选择引脚占用 y13 v10 v11 w10」),再明确授权代写:**「你直接分配引脚并且接入ps端写完我上电你调试」**(`CLAUDE.md` §6.0 显式例外,仅限本次);三个岔路口由用户用 AskUserQuestion 选定(继电器映射「是,按我报的顺序」、挡位映射「按我报的顺序」、PL 职责「原子寄存器 + 互锁 + 硬件计时(推荐)」)。**① 产出 4 个文件**:新建 `relay_ctrl.v` + 改写 `cv_axi_lite_slave.v`(实现 `0x04 STATUS`、`0x18 RANGE_SEL`、`0x1C RELAY_CFG`、`0x24 SETTLE_CFG`)+ `cv_sys.v`(11 根新端口 + 例化 + STATUS 拼装)+ `led.xdc`(11 行,56 → **67 条** `PACKAGE_PIN`)。**② ★ 三条设计判断(全部写进 `relay_ctrl.v` 头部注释,属 B04/B06 正文素材)**:(a) **白名单而非黑名单** —— 清单 §2.1 R6 给的是物理黑名单(「栅极浮空」「挡位电阻短路」),**判它需要治具原理图而手头没有**;改用**白名单**(7 个合法组合,出处 = 前序硬件已验证的 `relay_matrix.c`),代价(将来加第 8 种组合会被拒)已写明为**有意为之**。(b) **不插 AllOff 中间态** —— R6 要求「先断后合」,但那是 **STM32 的物理约束**(K1–K7 跨 4 个 GPIO 口,一次切换要 4 次 `BSRR` 写,中间态客观存在);PL 里 7 位**同一拍翻转**,该瞬态**物理上不存在**,插中间态反而**主动制造**一个栅极浮空窗口 —— 而前序 `relay_controller.c` 本就明确「不插入 AllOff 中间状态」⇒ 规格那句在 PL 上是**反的**,已如实标注为**偏离**。(c) **稳定时间单位 = ms 而非「周期数」** —— 40 ms 在 50 MHz 下 = 2 000 000 拍 > 16 位上限 65535,**装不下**;复位值取 40 对齐前序 `RELAY_CONTROLLER_SWITCH_SETTLE_MS = 40`。另有第 4 条偏离:`relay[6:0]` 7 位(物理上只接 7 只),规划里的 `relay[7:0]` 是名义宽度。**③ ★ 拦下一处会静默生效的掩码陷阱**:若把 `0x1C` 写入值先掩到 `[6:0]`,`0x180` 会被截成 `0x00` —— 而 `0x00` 在括号表里**是合法组合** ⇒ **一次非法写会变成一次真实的继电器动作,且不报错**。修法:**寄存器存原值不掩码**,切片挪到 `cv_sys.v` 的 `.cfg_relay(cfg_relay_cfg[7:0])`,保住 `bit7` 让它走到 `relay_ctrl` 被判非法。**④ 🔴 BD 接入 —— 连撞 6 处失败,但每处都留了证**:`update_module_reference` 在 Vivado 2025.2 批量模式下**存在、`can_resolve_reference` 返回 1、却 `rc=1` 且 `errorInfo` 为空**(日志里连一行 ERROR 都没有);**Plan A(只清 mref 缓存)已由实测证伪** —— 清完重开 BD,引脚数**仍是 56**,`relay`/`gear` 依然查不到,因为 **`system.bd` 自己也存了一份引脚快照** ⇒ 走「清缓存 → `delete_bd_objs` → 同名重建 → 按 dump 重连」,实测 **56 → 58 引脚**。**⑤ ★★ 本次最有价值的一条 —— `connect_bd_net` 三种写法的实测对比**(不再猜语义,先写 probe 脚本实测再定):写法① `connect_bd_net <net> <pin>` ⇒ **BD 41-701** `requires at least two pins/ports, or one pin/port and a net`,**而前一行刚断言过该 net 存在**;写法② 把该网络**全部 6 个端点**一起喂进去 ⇒ **静默失败**(被 `catch` 吞进一个**从没被打印过的**列表),一路跑到 `validate_bd_design` 才炸出 **BD 41-758「s_axi_aclk 未连到有效时钟源」** —— **错误现象离错误动作隔了 5 个步骤**;写法③ **两引脚形式**(新引脚 + 既有网络里的一个老引脚)⇒ ✅ **成功**,回读 net 成员与改造前**逐字一致** ⇒ **Vivado 自己会把新引脚并进既有同名网络,列全老端点反而会因为「已经连好了」而失败**。**⑥ ★ 自查出两处我自己写的判据是错的**:🅰️ 全量悬空检查**第一版把 33 根引脚全判成「悬空」** —— `get_bd_nets -of_objects` **看不见 intf net**(`s_axi_*`/`m_axi_*` 走的是 `get_bd_intf_nets`),另 `relay`/`gear` 要等 `make_bd_pins_external` 之后才有 net ⇒ 修法是先查标量 net 再查 intf net,并把检查挪到第 5 步之后。🅱️ `get_property CONNECTED <pin>` 在这里**恒为空字符串**,**连好了也是空**、完全不能当判据(已就地留注释)。**⑦ 又两条命令层面的坑**:`make_external` **不是本版命令名**,正确的是 `make_bd_pins_external -name <port> <pin>`;`make_wrapper` **成功写出了文件**(日志 `Verilog Output written to ...`)却**不会重新登记进工程**,而我又先把旧 wrapper 的 file 引用 `remove_files` 掉了 ⇒ `get_files -quiet system_wrapper.v` 返回空,**我误报「没生成」** ⇒ 修法是拿 `make_wrapper` **返回的路径**再 `add_files -norecurse`。⚠️ 另:`get_bd_nets relay` 按**名字**查**查不到**(生成的网络名是 `/cv_sys_0_relay`),按名字查会触发 `Common 17-55` **中断整个脚本** ⇒ 一律改用「引脚 → 网络」反查。**⑧ ⚠️ 一次差点误判(与 `stale-artifact-silent-reuse` 同族)**:我用 `grep -o '>relay<' system.bd` 得 **0 次**,差点判定「新端口没写进 BD」—— 实际是**我的 grep 模式基于错误的格式假设**(该文件存的是 `"relay": {`,不是 XML 标签);改用 `grep -c relay` → **5 次**,配合 wrapper 文件内容双向确认 ⇒ 📌 **「查不到」与「不存在」是两件事,先确认自己的查法对不对**。**⑨ 结果与实现读数(→ M3)**:`validate_bd_design` **通过**;BD 存盘;地址段回读 `/processing_system7_0/Data/SEG_cv_sys_0_reg0` **offset `0x43C00000` range `0x1000`**、`/cv_sys_0/M_AXI` → `HP0_DDR_LOWOCM` **offset 0 range `0x40000000`**(**重建后必须显式补回**,不补就是「Overlay 加载成功但 MMIO 全 DECERR」);新 wrapper 含 `output [6:0]relay;` / `output [3:0]gear;`;综合 `synth_design Complete!`;`write_bitstream Complete!` 100% —— **WNS `+1.490` / TNS `0.000` / 失败端点 `0 / 5266`**、**WHS `+0.080` / THS `0.000` / 失败端点 `0 / 5266`**、WPWS `+3.346` / `0 / 2010`,报告原文 **`All user specified timing constraints are met.`**;资源 **LUT 1665 → 1715(3.22%)**、**FF 1888 → 1993(1.87%)**、BRAM Tile **4(2.86%,未变)**、DSP **0**、**Bonded IOB 67(53.60%)**、BUFG 3、MMCM 1;**DRC 无 Error / 无 Critical Warning**(1 条 `CHECK-3` 提示 + **20 条 `REQP-1839`**,后者**全部落在既有的 `u_lcd_fb_reader/lbuf_reg`**,**与本次改动无关**,记此备查);位流 `qrs.runs/impl_1/system_wrapper.bit`(4 045 690 B),**md5 `9613c2928a01f832b71aef7c45c4c0b9`**。**⑩ 文档**:`docs/board_pins.md` 新增 §二「继电器矩阵与挡位选择」表(11 行,含 J4 脚号 / BANK / B 名)+ **5 条注**(自洽性论证 / 唯一跳过的 J4-20 `W9` 及其与 `gear[3]` 同差分对 / **极性未证实** / 摄像头复用 / 冲突检查),「22 个脚」→「33 个脚」;`docs/project_state.md` 的 **Q24 结案** + §十二 变更日志;`CLAUDE.md` §八 实测值行与引脚预算行。**⑪ ⚠️ 遗留**:① **位流尚未上板** ② **继电器驱动板极性未证实**(`ACTIVE_LOW = 0` 是**继承前序 STM32 约定,非手册**;若反了则上电配置期**同时吸合全部 7 只** ⇒ **首次上电不接 DUT**)③ 复位值 `0x10`/`0b0010` 抄自前序**声明**而非电路推导 ④ 交付仓库 `cv_regs.py` 需补 `0x04/0x18/0x1C/0x24` 定义 —— 按 §6.0.1 **待用户点名** ⑤ 工作区 `src/rtl/` 这 3 个文件现已与 Vivado 工程**漂移**,同步由用户执行。**⑫ 📌 代价口径(M10 素材)**:本次新增 11 根 IO + 一个 `relay_ctrl`(约 50 行有效逻辑:1 个 8 位寄存器 + 1 个 4 位寄存器 + 2 个 16 位计数器 + 2 张比较表),换来 **LUT +50(+0.09%) / FF +105(+0.10%)** —— **代价极小**;⚠️ 但 **IO 占用率 44% → 53.60%(67/125)** 才是**真正的约束**(不是 LUT),后序加外设须先看 IO 预算。**⑬ 建议沉淀**:`skill/stale-artifact-silent-reuse/`(形态 3 补强 —— 补齐「Plan A 清缓存已证伪」与「`update_module_reference` rc=1 且 errorInfo 为空」两条实测)+ **新候选 `skill/silent-tool-failure/`**(同一次会话里同一类失败出现 4 种形态:命令 `rc=1` 而 `errorInfo` 为空 OR 错误被 `catch` 吞掉、现象延迟 5 步才出现 OR 命令**成功**了但**需要的副作用没发生** OR 自以为是 0 的 grep 模式把「格式假设错误」报成「不存在」;四者共同处方 = **不看命令返回值,看被改变的那份东西本身**——回读引脚表 / 回读 net 成员 / 回读 wrapper 文件内容) | ⚠️ **部分可用**(RTL / XDC / BD / 综合 / 实现 / 位流**全部完成并有回读证据**,但**未上板**,且**继电器极性未证实** ⇒ 不记 ✅) | → `skill/stale-artifact-silent-reuse/`(形态 3 补强)+ 🆕 候选 `skill/silent-tool-failure/` |
| [LLM-057](LLM-057.md) | 2026-10-10 | Claude | **继电器/挡位「只读」接进 C 界面 —— 接通了,但顺带查出①我自己的死代码 bug(把派生冗余位 `fault` 当独立条件,恒真吞掉三条具体文案)②一个源 RTL 解释不了的板上异常(每次写 `0x1C` 表现为两次 set 脉冲,第一次携带 `0x00`)** | ✅ 只读接入成立 / ❌ 异常未解决 | 候选 `skill/derived-bit-is-not-a-condition/` + `skill/readonly-integration-acceptance/` |
| [LLM-058](LLM-058.md) | 2026-10-10 | Claude | **用户交出代码编写权后的第一次代写**(原话「**后面代码都归你写,先关系统我断电,你写完再叫我上电测试**」,推翻 `CLAUDE.md` §6.0 自 09-12 起的「代码一律用户先写」,但**物理动作仍归用户**)。产出:① 关机并把 `/tmp` 里三个只 mmap `/dev/mem` 的诊断脚本抢救到 `/home/xilinx/`(`/tmp` 重启即丢)② 🆕 `src/python/drivers/relay.py` —— 继电器矩阵 + 挡位语义层,含 `0x1C` 缺陷的**绕法**与**三个后端**(进程内 `cv_monitor`、PYNQ `MMIO`、裸 mmap,后两个是有意的对照)③ **拆掉 `tools/` 里三处 `Overlay(download=True)`** —— 原 3 个脚本每次运行都重加载位流,而 `lcd_fb_reader` 是活跃 AXI 主机(**一次上电只允许加载一次位流**);`relay_rw.py` 与 `relay_hold.py` 重写、`relay_smoke.py` 做成拒绝运行的存根。<br>**★★★ 本轮最重的发现不是代码,是收尾写状态文档时翻出的两处记录互相矛盾**:第 52 次收尾记录写着旧 `relay_rw.py` **A 节「7 个合法组合 7/7 相符」**,而 A 节做的正是「**单写 `0x1C` = V → 读回 == V**」—— 这**正是异常文档 §2 断言不可能的写法**(若「一写两脉冲」是常驻行为,单写 V 在默认 40 ms 稳定下必然读回 `0x00` + `err=0x03`,**A 节一个都过不了**)⇒ **同一份位流、同一次上电,两种写法结果相反** ⇒ 异常**不是静态性质**,二者必有一错**或异常是有条件的**。<br>**顺着矛盾去比两种写法差在哪,发现两个变量同时变了**:① 旧 A 节**从没写过 `0x24`**(一直是复位值 `0x0028_05DC`),而 `poke_relay.py`(**异常记录全部出自它**)**每次都把 `0x24` 紧挨着写在 `0x1C` 前面**;② 后端也不同(旧 = PYNQ `MMIO.write`,异常记录 = **裸 `/dev/mem` mmap + `struct.pack_into`**)⇒ 📌 **「找到一个差异」与「找到了原因」是两个断言** —— 这里找到的是**两个**差异,正确动作是**做 2 条件 × 2 后端的对照实验**,不是挑一个顺眼的当根因。<br>**顺带把 Q31 的第 3 组核对(BD/SmartConnect)做完**:`axi_smc` 是 **1:1**(`NUM_SI=NUM_MI=1`)、两侧**同 `FCLK_CLK0` 50 MHz**、`WDATA` 两端都是 32(**位宽转换会拆拍,这里没有**)、`NUM_SEG=1` 单段 MMU、`SINGLE_ISSUING` —— **四类「一变二」机制逐一排除**;⚠️ 但 Vivado SmartConnect 是**加密 IP**,读配置**证明不了**内部没有重发路径,这条不确定性**原样写进文档,没有简化成「已排除」**。🆕 **附带发现一条潜在隐患**:BD 地址映射给 **4 K**,而 slave 只译 **8 位(256 B)**(`C_S_AXI_ADDR_WIDTH=8`)⇒ 地址位 `[11:8]` **被静默丢弃**,`0x43C00100–0x43C00FFF` **全程混叠**到 `0x00–0xFF` ⇒ ① 「4 K 窗口」的说法是实际值的 16 倍 ② `0x1C` 的「地址绑定」**只在低 8 位内成立**。<br>**纠错轨迹三条**:(a) 写 `relay.py` 前准备在注释里写「绕法是**已验证的**手段」,落笔前去翻记录,发现 `20261010_relay_cfg_write_anomaly.md` §6 **早就有一张表**证明它验过 ⇒ 📌 **「上个会话的结论」既不能默认可信、也不能默认可疑,要去读它有没有写验证方式**(写了具体上板证据的可以直接用,只写结论的必须回头找证据);(b) 我没有拿一个顺眼的差异当根因,而是把它写成 **2×2 对照**;(c) 我确实按**单笔 / 背靠背 / W 先于 AW / 流水线重叠**四种情形逐拍推演了写桩(`cv_axi_lite_slave.v:422-460`),四种都得**一笔一个脉冲**,推不出结论 ⇒ 如实标成「**嫌疑不是结论**」。<br>**上板验证已备好**(用户已断电,本轮代码**全部未上板**):`tools/relay_rw.py` 的 **§0「触发条件对照」已重排到所有其它写之前**,一次跑出 cold 单写与 pair 背靠背两行并**自己打印判读**,配 `--raw` 把后端变量也分开 ⇒ 4 组一次上电跑完。⚠️ **遗留**:① `drivers/relay.py` 与两个工具**零上板运行** ② Q31 根因未定位,需挂 ILA(**要重建位流**)③ 交付仓库未同步(按 §6.0.1 待用户点名) | ⚠️ **部分可用**(代码与文档完成,PC 上只做了语法与**纯逻辑**自测;**零上板**;Q31 未解决) | 建议 `skill/contradiction-between-records/`(**新**,两份记录冲突时列差异项、差异 ≥ 2 先做对照) + `skill/verify-before-asserting/` 补条 |
| [LLM-059](LLM-059.md) | 2026-10-10 | Claude | **Q31 结案 —— `0x1C` 写入异常的根因不在 PL,在 PS 的一行 Python 写法**(用户本轮的输入只有三个字:「**上电了**」;全部排查由模型在板上自动执行)。**★★★ 根因**:`struct.pack_into(fmt, mmap, off, v)` 对一个 **Python `mmap`** 会发出**两笔** AXI 事务,**第一笔是常数 `0x00000000`**;它恰好落在继电器白名单里(`COSS_DISCHARGE`)⇒ 被**受理**、锁 **40 ms** busy ⇒ 第二笔(真值)撞 `ERR_BUSY(0x03)` ⇒ 硬件停在 `0x00`。**PL 完全无辜,SmartConnect 也无辜。**<br>**修法一行**:`drivers/relay.py::_DevMemRegs.write` 由 `struct.pack_into` 改**切片赋值**(`self._map[off:off+4] = value.to_bytes(4,"little")`);`WORKAROUND_0x1C` 置 `False`(绕法退役)。<br>**🔴 关键归因证据(板上实测,位流 md5 `9613c292…`,同一份位流同一次上电,五条)**:① **公平四路**(每例前清 busy 并核对基线 `relay=0x10 / err=0x00`,写 `0x1C = 0x61`)—— PYNQ `MMIO` 整字写 ✅ `0x61` / 裸 mmap 单字节 ✅ / 裸 mmap 切片赋值 ✅ / **`struct.pack_into` ❌ `0x00` + err `0x03`**;② **把 settle 置 0 拆掉互锁后四种写法全部得到 `0x61`** ⇒ **最后一笔是对的,问题在"笔数"而不是"值送错"**;③ **六组遍历**(基线 `0x10/0x61/0x7F` × 载荷 `0x61/0x7F/0x00`)**全部 `0x00` / err `0x03`** ⇒ 与基线无关、与载荷无关 ⇒ **第一笔是常数零,不是读-改-写**;④ **证人寄存器**:`wr(0x4C, 0x11223344); mm[0x4F] = 0xAA` → 读回 **`0xAA223344`** ⇒ **窄写被模拟成 RMW**(实测);⑤ **同进程修前/修后对照**(最干净):同一个类、同一次上电,**只把 `write` 换成 `struct.pack_into`**,7/7 通过**立刻翻转**成 3/3 失败。<br>**📌 为什么只有 `0x1C` 出问题**:它是唯一带 settle 互锁的命令寄存器;其余(`0x18`/`0x24`/`0x60`/`0x84`…)同样吃到那笔 `0x00`,但**没有互锁 ⇒ 后写覆盖前写 ⇒ 看不出来**。⇒ 📌 **症状落在哪个寄存器上,有时是白名单与互锁的巧合,不是那个寄存器特殊。**<br>**回归**:`tools/relay_rw.py --diagnose`,**两个后端各一遍**,均 **`结果:全部通过`**(★0 触发条件 2/2 · A 功能往返 11/11 · B 回归护栏 3/3 · C 非法组合被拒 6/6 · **D1 Python 护栏 5/5** · **D2 RTL 拒收 6/6** · E 稳定延时实测 **40.2 ms / 100.4 ms**)。<br>**🔴 纠错轨迹 8 条(本轮是迄今单位时间纠错密度最高的一次)**:① **★ 把 `0x48` 当 SCRATCH 用** —— 机制探针无论写什么都读回 `0x48`,我第一反应是"写不进去",差点又去查 PL;去翻 `cv_regs.py` 发现**表一直是对的**(`0x48` = VERSION **RO 恒 `0x00010000`**,`0x4C` 才是 SCRATCH),**是我看错了表** ⇒ 浪费一轮上电;② **★★ 断言"两条后端映射属性不同、不能互相代表"** —— 这条把范围错误地划成"PS 写通路"并**计划挂 ILA(要重建位流)**;去读 PYNQ 源码(**`pynq/pl_server/embedded_device.py:523`**)实测**两条都是 `open("/dev/mem", O_RDWR\|O_SYNC)` + `mmap(MAP_SHARED, PROT_READ\|PROT_WRITE)`**,**映射属性完全相同,差别在写下去的位宽/笔数** ⇒ **这条错判是本轮最贵的**,已在 BD 核对文档 §4 就地标注为错;③ **假设"从机 honors WSTRB"** —— 先有解释再去验证(顺序反了);grep RTL 证伪:`s_axi_wstrb` **只出现在端口声明 `:157`**、全文再无使用,正确解释是那个 RMW 模拟;④ **第一版 `struct.pack_into` 对照不公平**(settle=40 ms 而两写法只隔几十毫秒,可能是上一轮 busy 残留)⇒ 重写 `mech3.py` 加显式清基线协议,**缺陷在公平协议下依然复现** ⇒ **结论真,但第一版数据作废**;⑤ **`tools/relay_rw.py` D 节 5 个假失败** —— `set_gear()` 在 Python 侧就校验并抛异常、**根本走不到硬件**,`err` 不可能是 `0x02`;**顺带查出 RTL 的 `ERR_GEAR` 分支从来没有被任何测试走过** ⇒ 拆 **D1**(Python 护栏)**D2**(新增 `gear_write_raw()` 绕过 Python 校验)**D2 6/6 —— 该分支第一次被验证**;⑥ `section_trigger` 结束时**硬写** `r.workaround = True` 覆盖调用方选择 ⇒ 改存-还;⑦ `--no-fix` 语义在默认值翻转后**反转**(会强制打开绕法)⇒ 换成 `--workaround`;⑧ **`_write_relay` 的绕法掩码 `orig & 0xFFFF_0000` 与它自己的注释恰好相反** ⇒ 那个绕法**从来没生效过**,它"看起来能用"只是因为只被 `pynq.MMIO` 后端用过而那个后端本来就不需要绕法 ⇒ 已随根因一并删除。<br>**✅ 顺带推翻 LLM-058 的一处自我怀疑**:LLM-058 里我逐拍推演过写桩的四种情形、得出"一笔一个脉冲"却标成"推不出结论" —— **那个推演本来就是对的**。<br>**文档**:异常文档**由「未解决」改为「✅ 已解决」**(§1 重写,旧文以删除线保留);新增 **§10(10.1 ~ 10.9)** —— 2×2 表 / `embedded_device.py:523` 反证 / 性质隔离表 / 公平四路 / 证人寄存器+六组遍历 / 排除表 / 修法与修后验证(含两后端表)/ **10.7.1 三处测试侧缺陷** / 10.8 方法论 / 10.9 附带发现(**WSTRB 未实现** 与 **`0x48`/`0x4C` 看错**);`20261010_bd_smartconnect_inspection.md`「下一步」表结案(**② 挂 ILA 与 ③ AXI VIP 仿真均标 🚫 不需要了**);`tools/relay_hold.py` 头部不再声称用绕法;`20261010_relay_pin_level_verified.md` 加更正注(**结论有效**,只是"绕法必需"的说法过时)。⚠️ **遗留**:交付仓库未同步(按 §6.0.1 待用户点名) | ✅ **直接可用**(根因经五条独立上板实测坐实,**修后两后端各一遍全通过**;含"一行代码切换结果"的同进程对照) | 🆕 `skill/count-both-sides-of-interface/`(**已落地**) |
| [LLM-060](LLM-060.md) | 2026-10-10 | Claude | **用户一句话改了三件事,并要一份自用手册** —— 原话:「**总之现在以复刻出结果为导向,加分点再说,以后的代码编写仓库管理都是你负责了我就不写了,然后你重新列一份给自己读的开发文档,不然你每次开发要读这么多文件太繁琐了,像是 ssh 密码这种东西直接写出来就不用每次重新找了**」。<br>**① 目标排序变更**:「**复刻出结果**」优先于 `CLAUDE.md` §三 的评分权重排序 ⇒ **先让测量链出数,加分项押后**(⚠️ 但"必做点一项都不能缺"不变)。<br>**② 授权扩展**:代码编写(第 53 次已授)**+ 仓库管理**(同步/commit/push)**全归 Claude** ⇒ **`CLAUDE.md` §6.0.1「交付仓库默认只读、须点名才写」作废**;⛔ **物理动作仍归用户**。<br>**③ 🔴 D30 推翻 D03** —— 「ps 端能算出结果就行,增益的问题硬件会解决」。⇒ 不再要求解调在 PL 侧算;**P3 判据随之改写**(原「硬件 I/Q 与 Python 参考一致」→「解调量与 Python 参考一致」),**C 侧解调(`adc_sample.c`)从"替补"变"正选"**。⚠️ **同条登记两条评分项原文后果**:`B03` 的「vs **硬件 DLIA**」少一条腿、`M10` 的「**资源**对比表」纯 PS 侧填不了;✅ 给了替代路子(资源改用已做出来的 PL 部分:**1715 LUT / 1993 FF / 4 BRAM / 0 DSP / 67 IOB**,IO 已 53.6%)。<br>**④ 🆕 主产物 `docs/dev_handbook.md`(398 行,13 节)** —— 当前目标 / 权限边界 / 会话流程 / 板卡与网络(**含 SSH 口令**)/ 板上文件系统与基准 md5 / **寄存器速查全表** / 时钟口径 / **8 条安全红线** / 常用命令 / 交付仓库推送流程 / **文件地图「我要 X → 去哪个文件」** / **13 条坑清单** / 当前缺口 / 决策反转登记。⇒ **会话开始只读这一份 + `project_state.md` §一,不再通读 4400 行**。⛔ **含口令,永不进交付仓库**(`git add` 必须逐路径)。<br>**⑤ 🆕 `tools/board_run.py`** —— 持久版板上执行/传文件(paramiko),替代一直放 `/tmp` 里、**重启即丢**的一次性脚本;只做 SSH,不碰位流。<br>**纠错轨迹 5 条**:① **★★ 我把 `CLAUDE.md` 里的旧决策 D03 当成"现行约束",据此报了"PL 分量会缺一大块"这个不存在(已被勾掉)的缺口** ⇒ 📌 **决策表记的是"当时的决定",不是"现在的约束";凡依赖某条决策的缺口,必须把那条决策一并标出让用户有机会推翻** ⇒ 已在 §六划掉 D03 指向 D30 + 手册 §13 建**决策反转登记表**;② ✅ **正例**:写手册要照抄 `CLAUDE.md` 的 5 个 PS 模块清单,落笔前 `find src/python` 发现**其中 4 个根本不存在** ⇒ 📌 **"文档里列了它"与"它存在"是两个断言**;③ ⚠️ 我新增的 `<details>` **一开始没闭合**(会把 §二 之后全吞进折叠块),跑配对检查才发现;而第一次扫描还**误判**了 —— 变更日志某行**在反引号里提到 `<details>` 这个词**被当成真标签 ⇒ **计数类检查必须排除"文本里提到标签"**;④ ⚠️ **我并没有完全照用户说的做**:板卡 SSH 口令照写(出厂默认),但 **Gitee 口令留了文件指针没写明文**(非默认值 / 已有固定路径 / 手册会被同步复制扩散面更大)—— **这是我的判断不是用户指令,已当面告知等他裁决**;⑤ 🔴 手册含口令 ⇒ 在三处下了"**永不进交付仓库**"的硬约束。<br>**⑥ ★★ 我把一条没跑过的命令序列写进了手册,随后自己照着跑当场失败** —— 手册 §9.2 的 credential helper 凭**上一轮记忆**写成,同一次会话末尾真去推送时报 `fatal: could not read Username for 'https://gitee.com'`;`sed` 取值实测是对的(`name` 6 字节 / `password` 32 字节)⇒ **不是凭据问题**,真因是 helper **没有 shebang** ⇒ git `execve` 失败(ENOEXEC)⇒ **零输出** ⇒ git 认为没给用户名 ⇒ 📌 **手册是"下次的我唯一的入口",写一条跑不通的命令 = 下次照抄着再失败一次,且症状会把人引向"密码错了"这个错方向** ⇒ 已改成带 `#!&#47;bin&#47;sh` 的版本 **并加一步自检**(喂一个 `get`,只看输出键名)⇒ 🔴 **与既有 `verify-your-own-checker-first` 同族:先证明工具本身活着,再谈它的读数**;教训 = 写进手册的命令**要么当场跑过、要么显式标"⚠️ 未实测"**。<br>**⑦ 两个产物都不进交付仓库**(理由不同):`dev_handbook.md` = 含口令;`board_run.py` = 宿主侧脚本且内嵌口令,而仓库既有 `board/*.py` **全是在板上跑的、无一含凭据**(实测 grep 0 命中)⇒ 不破惯例。本次入库**只有 llm-log 两份** | ✅ **直接可用**(手册逐节**回源核对**而非凭记忆;标签配对已校验;§9.2 的 helper 已在实推中修正并自检) | ⚠️ **本轮不新增 skill** —— 四条教训够不上门槛,**不与既有技能强行挂钩充数** |
| [LLM-062](LLM-062.md) | 2026-10-14 | Claude | **「切换测量项目回路没有切换」—— 根因是一处从未落地的设计;并把 VH/VL 相位打通** —— 用户原话「**那就先恢复回路的问题,为什么我切换测量项目回路没有相应的切换**」,末尾拍板「**把系统关了,把项目修复完整再上电**」。<br>**① 根因不是 bug,是一处从未落地的设计**:`0x1C`(RELAY_CFG)三处能写的地方**一处都没写** —— PL `relay_ctrl.v` 已实现且 10-10 上板验证过;**C 界面 `relay_view.c` 文件头自述「🔴 只读,一个寄存器都不写 —— 写入权归 Python 侧」**;Python `drivers/relay.py` 写得完整、实测过,但 **`main.py` 只 import `drivers.board`,零调用方** ⇒ 硬件永远停在复位值 `ALL_OFF(0x10)`。📌 **顶栏那一栏没说谎(它读硬件真值),说谎的是按钮高亮 —— 按下去只换了个颜色。**<br>**② 连带结清四处「假装」**(全部有代码注释自认):点「全部」单点把**一份读数填进三张卡** · 点「先测 RG 再三路扫压」打印「跳过」、**RG 根本没测** · 点 RG 单点**卡片留 `--`** · 扫压把**同一个 `cs_pf` 写进三条曲线**。⇒ 现在:三腿**逐腿切继电器**再测 / RG **真测并填卡** / 扫压**逐腿切**,三条曲线形状应不同。<br>**③ 修法(五个调用点,纯 C)**:`RelayView_ApplyPath`/`ApplyCode` 成为 `0x1C` 唯一写入口(幂等跳过 + 等 busy + 回读校验 + 查 `err_code`,超时 **200 ms 不是 40 ms**);`RequestPath` / `RequestPointAll`(改三腿序列)/ `RequestRgThenSweepAll` / 扫压(`sweep_measure_point` + `sweep_step` **两处都要改,只改一处等于没改**)/ `RequestReset`(先回 `ALL_OFF`)。<br>**④ ★★ 核心纠错 —— 用户连续四问「为什么做不到」,前三问我都答得对,第四问才把我推对**:「我们也用 vh 和 vl 的相位差不行吗」→「不是已经能采到数据了吗,做 dft 相除不行吗」→「**adc 是同步采样为什么不是统一相位**」→ 拍板。前三问的答案(不能事后补时间差 / DFT 补不了相位原点 / 两次采集相位不相关)**全都在错的框架内正确** —— 我自始至终假设了**必须发两次 GO**,而 AD9238 **在同一个转换沿同时采 A/B,这对同步样本就躺在同一条总线上**(tick −1 与 tick 0 就是同一个周期的两个半周期),旧 `samp_en = (tick == ph_off)` **每周期只命中一个 tick ⇒ B 被扔掉**。⇒ **不是 ADC 不同步,是采集器没接那股同步数据 —— 它就在线上,一个 tick 之遥。**(前序 `ad9238_capture.c::AD9238_Deinterleave` 正是这么做的。)📌 **教训:当用户连续三次追问"为什么做不到"时,该被质疑的是前提,不是论据。**<br>**⑤ 为什么"事后补时间差"在这题上根本不成立(给用户前两问的正面回答)**:两次采集的解调基准锚在**各自第 0 点** ⇒ `∠Z` 里混进软件不知道的 `2π·f_e·(t_B−t_A)`;1 MHz 下要 1° 精度需时间准到 **2.8 ns**,而板上最细的 `clk_axi` 只有 **20 ns = 7.2°**,且 AD9833 是**板载 25 MHz 晶振、与 FPGA 不同源**(2.5 ms 内漂 100 ppm ⇒ **1 MHz 下 90°**)。**不是算术问题,是两次采集的相位原点本来就不同。**<br>**⑥ 算式照抄前序工程**(`~/Desktop/qrs/…/periodic_measurement.c:549` + `ad9238_capture.c:371-403`),不自己推:`Z = R0·(VH−VL)/VL`(**复数**;`Z` 标度无关 ⇒ 未标定的 `VFS_PP` 不再污染 `Cs/Cp/D`)→ `Rg = Re Z + 0.7`、`Cs = −1e12/(2πf·Im Z)`;`R0` 从 `0x18` **回读**取当前挡位,不硬编码。<br>**⑦ 改动清单**:RTL `adc_capture.v`(`samp_en_a`/`samp_en_b` + 每通道独立指针 + `CH_DEPTH`/`CH_LAST`,`BUF_DEPTH` 8192→**16384**)、`cv_axi_lite_slave.v`(写掩码 `0x1FFF`→`0x3FFF`,**不改就是静默截断**)、`cv_sys.v`(`.rd_idx` 位宽)、C 侧 `adc_sample.c/h`(`AdcSample_Impedance` / `CapturePair` / 拆交织)、`pynq_protocol.c`、`pynq_regs.h`、`relay_view.c/h`,Python `drivers/relay.py` 加文件头注明写入权已移交。<br>**⑧ 可复现构建工具(新增)**:`build/build_bitstream.tcl`(**完整重建,必须 `reset_run`** —— 否则 Vivado 可能复用旧 dcp、改了源却**静默**用旧网表) + `build/report_summary.tcl`(**可单独跑**,不动综合结果)+ `build/summary.txt`。<br>**⑨ 综合实现结果**:位流 md5 **`182e824810a4c826d8e6466a245e007f`**(旧 `d4d5538e…`),`.hwh` **逐字节未变**(`6c376905…`,mtime 仍 10-10)⇒ **BD 没动**;**WNS +0.316 / WHS +0.065 / 失败端点 0 of 5472**;**1790 LUT(3.36%)/ 2022 FF(1.90%)/ 7.5 BRAM Tile(5.36%)/ 0 DSP / 67 IOB(53.60%)/ 3 BUFG**(⚠️ **2026-10-15 改取「部署版」那次构建** —— 旧记 `+0.354 / 0 of 5469 / 1789 LUT` 是**同源另一次 P&R** 的数)⇒ 详见下方页脚 `2026-10-15` 第二次更正与 `LLM-063` 纠错 ⑥。⚠️ **WNS 全落在 `clk_adc_raw`(130 MHz)** —— 由 +1.161 掉到 **+0.316**,是本次加大缓冲的直接代价;**ADC 域已成新的绑定约束**。<br>**⑩ DRC**:无 Error、无 Critical Warning。⚠️ **新增 `REQP-1840` ×20**(`u_adc_capture/cap_mem_reg_6`,RAMB18)—— 对照 `git show HEAD:build/system_wrapper_drc_routed.rpt`(10-12 那份)`adc_capture`/`cap_mem` **提及次数 0** ⇒ **确是本次新引入**:缓冲加倍逼出 1 块 RAMB18,其 `ADDRARDADDR[13]`(net `wr_addr[13]`)由带异步复位的 `cnt_reg[*]` 驱动。**判断为本轮不改** —— ① 该 BRAM 在 GO 到来前内容无效,复位期破坏无可观察后果;② ADC 域只剩 **0.316 ns**,不为一条保守告警动复位结构;③ 结论未经实测坐实,**不拿"分析"当"结论"**。**升格条件**:采集数据出不明损坏时它是第一嫌疑。<br>**⑪ 纠错轨迹(5 条)**:① **范围判窄** —— 漏掉「挡位」:前序为 RG **强制 A 挡(10 Ω)并锁换挡**(`pc_protocol.c:282-284`),本项目停在 **B 挡(100 Ω)** ⇒ `VL/VH = 100/(100+Rg)`,Rg≈12 Ω 时差量只有 **11%**(**公式没错、是信噪比差**);未擅自改(超范围),写进注释与文档。② **同一句假话印在 4 处,改一处没改另外三处** —— 「C 界面只读」还印在 `relay_view.h`/`pynq_regs.h`/`pynq_protocol.c` 的启动 `printf` 里 ⇒ 📌 **改之前先 `grep` 那句话,别只改"我记得的那一处"**。③ **`pynq_protocol.c` 里有我自己上一轮留下的「两路相位不相关」结论** —— 我用**保留原文 + 标注"原稿这一段是反的"**处置,因为那句在当时**是正确的**、是"相位缺口存在过"的物证,删掉 M11 那条因果链就断了。④ **我自己搭了一处会静默失配的悬崖** —— `CH_DEPTH` 声明了**没用**(两处绕回比较还在写 `BUF_DEPTH/2`),**`xvlog` 不报未使用的 localparam**,是我逐行回读发现的 ⇒ 改成 `CH_LAST` 并在两处比较里用它(**容量改动的静默闸又添一例**)。⑤ 用户第四问掀翻前提(见 ④)。<br>⭐ **两处自查细节(工具本身也会骗人)**:`get_timing_paths -delay_type` 与 `-setup`/`-hold` **互斥**(`[Vivado 12-608]`);`current_design` **无工程时是抛异常**不是返回空串(`[Common 17-53]`);`report_timing_summary` 里**根本没有** `Total Negative Slack (TNS)` 这种措辞(那句在 `report_timing`)—— **照文档想象写的过滤器一条数都没捞到**,改成"筛正文行"才对;又:失败端点总数只在 `Design Timing Summary` 那条汇总行里,**每时钟组加起来 ≠ 总数**(5472 vs 4674),**不要相加**。 | ⚠️ **部分可用** —— **代码侧已完成并自检通过;上板判据待上电。**「部分可用」的准确含义:本记录填写时,回路切换**尚未听到咔哒声**、RG **尚未与万用表比对** —— 这两条只有上电后能测,而**我没有手**。**不拿"代码写完了"当"功能验证过了"。** | → `skill/silent-capacity-gates/`(第 ④ 条新形态:抽了参数却漏用,编译器不报) + 候选 `skill/one-go-per-conversion/`(**待上板验证后**再立) |
| [LLM-061](LLM-061.md) | 2026-10-12 | Claude | **DAC1220 偏压「设多少都读出 −10 V」的根因 = 偏移二进制补码 vs 直通二进制,两端一起改并自行上板验证** —— 用户四轮万用表读数喂完后拍板:「**高压级不管,先修改代码把dacbug修了**」⇒「**综合完自己重启开发板,自行调试**」。<br>**根因(手册 SBAS082G 佐证,非猜测)**:Table 13 CMR bit5 `DF` —— **0 = 偏移二进制补码(上电默认)** / 1 = 直通二进制;Table 9(20 位)补码 0 V = 码 `80000h`、直通 0 V = 码 `00000h`;CMR 上电值 `0x0002` ⇒ **默认就是补码**。<br>**六点实测全部只吻合补码、全部否定直通**(模块级):`0x7FFFF→+9.72` / `0x26666→+2.80` / `0x0CCCC→+0.820` / `0x80000→−10.0` / `0x99999→−8.0` / `0xA6666→−7.0`;其中 `0x0CCCC` 是**样本外**点(先预测 +0.823 V,后读数 820 mV)。<br>**两端同改**:PS/C `pynq_regs.h` 的 `cv_bias_volts_to_code()` 返回值 **`^ 0x80000`**(直通→补码旋转半个量程)、逆函数反向再转;C RTL `dac_1220.v` 的 `cal_byte_f` 由 `0x20A1`(DF=1/RES=0)改为 **`0x2081`**(DF=0/RES=1)。<br>**★ 改 RTL 而非只改软件的论证**:文档里 2026-09-19 那三点(−5/0/+5 V)**精确落在直通二进制**上,而今天六点精确落在补码上 ⇒ **两次上电之间 DF 变了**,而 CMR 是易失、且那次 CMR 写**落没落进芯片无法确证** ⇒ **钉到 DF=0 后两种情况都正确(写进去=显式补码;没写进去=上电默认 0),零回归风险**,同时消除"每次上电偏压映射可能不同"的隐患。**反过来只改软件旋转,下次上电若 CMR 恰好写进去就又会错。**<br>**验证**:① `impl_1 = write_bitstream Complete!` / 100%,**WNS +1.161 ns / WHS +0.051 ns / 失败端点 0 of 5266**,**1716 LUT(3.23%)/ 1993 FF / 4 BRAM / 0 DSP / 67 IOB(53.60%)**,位流 md5 **`d4d5538e…`**(旧 `9613c292…`),`.hwh` **逐字节未变**(BD 没动,只有 `.bit` 变,符合预期);② `ssh sudo reboot`(uptime 69 min → 5.3 min,确认真重启)后手动 `systemctl start qrs-ui`(该服务 `disabled`),`/dev/mem` 映射 AXI 成功**未卡死**;③ **最硬的一条 —— 生产二进制已换新公式**:启动日志 `[proto] 偏压 = 0.0 V(0x14 = 0x00000)`,而**旧公式下 0.0 V 会算出 `0x80000`**;④ 偏压寄存器往返 0/30/100/0 V 四点全部写读一致、可重复;⑤ **ADC 相干 DFT(N=1235 = 19 整周期 @1 MHz):基波 bin19 = 562.4 码,次强 bin 仅 2.1(≈48 dB),2 次谐波 −48.6 dBc** ⇒ 相干采样的 2f_e 抵消成立,`adc_d ^ 12'h800` 有效;p-p 1138 码 = 27.8% FS,OTR=0 无削顶。<br>**纠错轨迹 4 条 + 2 条诚实标注**:① 🔴 **我把零点写反了** —— 注释写成「`0x80000` 才是 0 V」,而实测该点是 **−10.0 V**;回填实测表格时对不上才发现,改成「**`0x00000` 才是 0 V;`0x80000` 实际是 −100 V**」并重推三遍 ⇒ **这条若发出去会把用户高压侧直接推进 −100 V**;② **我的 C 单元测试断言错了而代码是对的** —— 0.1 V 我断言 `0x0000D`,实际 `0x0020C`,**差点走进"改代码迁就测试"的坑**;③ **发现跨上电周期不自洽**,这是促使我改 RTL 的线索(见上);④ **误判过一次硬件故障,实为我的脚本 bug** —— `adc_dft.py` 报 `math domain error`,查下来是**那个脚本从头到尾没有 GO 触发**(从没写过 `0x8C=1`),读的是复位后的空缓冲 ⇒ 📌 **先怀疑自己的工具,再怀疑硬件**;⑤ ⚠️ **CMR 到底有没有写进芯片「未结案」**(只有"行为像上电默认值"这个间接证据),坐实需建 DAC1220 SPI 从机模型或上示波器,**本轮都没做,故如实标未结案不写成"已解决"**;⑥ ✅ **用户说「高压级不管」时我正追 1.1% 量程残差,当即停手**并写明依据:DAC1220 增益误差规格 **±150 ppm(±0.015%)**,1.1% 是它的 **~75 倍**,且该芯片**无内部基准**(`VREF` 是输入引脚)⇒ 残差属于板上 2.5 V 基准/增益级电阻,**归 B8 OPEN/SHORT 校准,不该用万用表手读数烘补偿系数**。<br>⛔ **仍做不到的**:实际电压我量不了(没有万用表的手)—— **寄存器路径与 ADC 路径我验到了,「芯片输出口是不是 +30.0 V」只有用户能确认** | ✅ **直接可用**(寄存器/ADC/生产二进制三条已坐实 + **用户持表确认「DAC 输出正确了」** ⇒ 偏压链端到端闭环;⚠️ 口径:口头判读无逐点表值,不算百分数;**CMR 写入与自校准两项仍如实标未结案**,不因该确认关闭) | → 候选 `skill/binary-format-two-sides/`(**两端格式必须写在同一处并钉死**;更硬的一条:**格式类错误,复位值比写入值更值得依赖**)+ `skill/suspect-your-own-instrument-first/`(第 4 条) |
| [LLM-063](LLM-063.md) | 2026-10-15 | Claude | **扫压「看着不对」的三个独立成因 + 采样刻度 DIV 2 → 4** —— 用户原话:「我刚刚开启了偏压测量一遍后面又关了,你看看结果有没有问题,感觉跟之前不太一样是不是档位没法自动调节的问题,没法调档位应该只是测量不准不会烧lcr电桥吧」,随后拍板:「**这个修复吧,档位先别动等我连上档位控制引脚,然后如果你用不上开发板就先关系统**」。<br>**① 成因有三个,不是一个**:档位卡 B(100 Ω)⇒ 偏压 >11 V 后 `\|VL\|` 塌到 9–40 码,`Re Z` 随机散(1724/1062/1079/1140/1343…),而受控条件下同一量 SD 只有 0.44%;**DIV=2 的边缘采样仍在翻 B 的总线位** —— `A/INA` 幅值 0.2706/0.2705/0.2710 V(平滑)而峰峰 1286→1289→**1535** 码,跳幅正好 ±1024/±2048,`B/INB` 峰峰在 400↔2500 之间跳;低偏压头部 0.100→0.112 V 的 Cs 从 695→1755 pF 跳变。**自动挡与手动挡都不存在**(日志原文「档位 0x18 仍只读回显」)。<br>**② 档位卡住不会烧电桥(用户的安全疑问)**:挡位只选四个参考电阻之一(10 Ω/100 Ω/1 kΩ/10 kΩ),**不碰增益、不碰供电**;B 挡日志电流最大 **2.45 mA**,DUT 短路也只有几 mA;唯一要当心的是 A 挡(10 Ω,电流约大 10 倍)。⚠️ **如实标注不确定**:夹具原理图不在团队手上(`source_design_checklist.md` 自认),所以**下游模拟开关的保护我无法核实**,只能由用户对硬件确认。同时排除一个疑似偏压缩放 bug —— `pynq_regs.h:325-328` 记着外部 ×10 级 ⇒ 90.000 V 就是**真**栅压。<br>**③ 改 DIV(纯 C 一行,不重下位流)**:`CV_ADC_DIV_DEFAULT 2u → 4u`。P=2 时整张刻度表只有 `{−1, 0}` 两点,**贴死 `adc_clk` 两个边沿、零建立裕量**;P=4 的表是 `{−2,−1,0,+1}`,`cv_adc_phase_center(4)` 折回到 **+1** ⇒ A 落 tick+1、B 落 tick−1,各离边沿 **7.69 ns**(一个 130 MHz tick)。<br>**同一次运行内的前后对照**(不是两次不同实验:回路 CRSS `0x1C=0x51`、挡位 `0x18=0x02`、`f_e=1 MHz`、继电器与 DAC 全程不动、每档连采 30 次;工具 `tools/adc_cfg_probe.py --cfg '2:-1' \| '4:+1' --only-z --z 30`):`2:−1` / 65.0 MSPS ⇒ Re Z 231.44±1.02、Im Z −75.33±2.91(3.86%)、Cs **2115.90±81.66 pF**、D 3.08±0.13;`4:+1` / 32.5 MSPS ⇒ 228.52±0.78、−86.30±0.36(**0.42%**)、Cs **1844.19±7.79 pF**、D 2.65±0.01 ⇒ **重复性约 10 倍**;代价 = Cs 均值**偏 −12.8%**,**历史 65 MSPS 那批读数不能与之后的直接比**。<br>**落地验证**:主机算得 `cv_adc_cfg_word(4, cv_adc_phase_center(4)) = 0x00000104`、`cv_adc_sample_hz()` = 32 500 000 Hz、tick +1 落在合法区 `[−2,+1]` —— 与板上实测的 `4:1` 同一配置;三档相干窗(1 MHz→252/8190、100 kHz→25/8125、10 kHz→2/6500)重算全部成立;板上重编 `c_ui` **exit 0**(唯一 warning 出在**没动过**的 `ui/ui_demo.c:1805`);新二进制用 `grep -a` 查 UTF-8 裸字节(新串「CV_ADC_DIV_DEFAULT 定死」1 次 / 旧串「已固定 65 MSPS」**0** 次),md5 **`7932dd2fac74fc710d6d3e18e3d041b5`**。⛔ **新默认要等下一个上电周期才生效** —— 重启 `qrs-ui.service` 会再调一次 `Overlay()` = **第二次加载位流**,是红线。<br>**离场状态**:位流 `232c649d…`(本周期未再加载)、`.hwh` 逐字节未变、继电器已回 **`ALL_OFF(0x10)`**(`err=NONE`)、`sync` + `shutdown -h now`、**ping 已不通**。<br>**纠错 4 条 + 1 条残留**:① 起初写「Im Z 的 SD 从 28.76% → 0.42%」——前者来自**位流修好之前**那批(两次独立 GO,相位随机),两个变量一起动,证明不了 DIV 有用,改回**同一次运行**的 `2:−1` vs `4:+1`;② 用 `strings \| grep '中文' \| wc -l` 得 0 并**当成证据** —— GNU `strings` 只出可打印 ASCII,这个 0 是必然的,改用 `grep -a` 查裸字节;③ 🔴 **从错的那棵树推文件到板子**(主机上 `src/c_ui/app/` 有**两份**:工作树与交付仓库),推完 md5 校验 6 个文件**全部对不上** —— 而 `LLM-062` 的验证表里**已经写过同一条**「先踩过:我第一遍改在了错的那棵树上」⇒ **记录过 ≠ 防住**;④ `LLM-062`/`INDEX` 页脚记的位流 md5 是**中间产物** —— 同夜三次构建 `182e8248…`(22:28)/`90cadcce…`/`232c649d…`(23:41),**两者大小完全相同(4045690 B)、只有 md5 分得开**,而工作区 `board/qrs.bit` 停在 `182e8248…`(交付仓库那才是对的)⇒ **谁从工作区部署就加载错版位流**,已换成 `232c649d…` 并就地更正两处文档;⑤ 残留:`LLM-062` 那批「已上板验证 L1–L5」是在哪个位流上取得的,**待下次上电 md5 复核时一并坐实**,本记录不替它下结论。 | ⚠️ **部分可用**(三个成因与「不会烧电桥」用户已认可并据此拍板;DIV 修复**代码侧已落地但本次上电未生效**;自动挡 / 手动换挡 **用户明示押后**,等他连好挡位控制引脚;`ADC_SAMPLE_R_TIA = 1000.0f` 与 B 挡 100 Ω 的不一致**未动**) | → `skill/silent-capacity-gates/`(追加一族:**"工具自报成功 / 工具静默过滤"也是一道静默闸** —— `strings` 只出 ASCII、`--put` 印**本地**大小当成功、`grep` 在二进制里只认文本、`find`+`ls` 的**大小与时间戳分辨不出同名不同内容**;判据 = **`--put` 之后回读远端 md5**)+ 待建 `skill/two-trees-one-name/`(同名双份目录:改一棵、推另一棵。📌 **价值不在"记住",在"记录过也照样再犯"** ⇒ 沉淀必须是**无条件执行的机械步骤**:改前用**绝对路径**编辑、推完 `md5sum` 回读远端逐行比对、二进制产物**只认 md5**) |
| [LLM-064](LLM-064.md) | 2026-10-15 | Claude | **扫压收尾不归零偏压 + 扫压不会自动换挡 —— 两处缺陷与 `\|Z\| ≈ R0` 判据** —— 用户原话:「扫完了，吧偏压调回0,它这扫完都没有置零」,随后「**先把档位修复了不然数据都很奇怪**」。<br>**① 偏压那条先证伪再定位,没先改代码**:当场读 `0x14` = **`0x0007FFFF`**,按 `pynq_regs.h:344-420` 反解**正好 +100.000 V** —— DUT 栅极被留在满扫压上。两处成因**性质不同**:`sweep_finish()` **完全没写 `0x14`**(纯疏忽);`RequestSweepStop()` 打印的「偏压保持在当前点」**是当初写下的设计意图**,一并推翻但要说明理由。收尾走 `bias_apply(0.0f)` 而**不是**直接 `cv_reg_write(0x14,0)` —— 顶栏「偏压:」只有这条路径经 `bias_announce()` 更新,绕过它写寄存器会**硬件归零而界面仍显示 100.00 V**,比不归零更难查;用 `0.0f` 而非旋钮下限 `0.1 V`,收尾要的是**真零**(码 `0x00000`,**不是 `0x80000` —— 补码下那是 −100 V**)。<br>**② 第二条先把"数据奇怪"证成量程问题、不是器件属性**:整段 CRSS 腿固定 B 挡 100 Ω,而 `\|Z\|` 从 **363 Ω 走到 15 313 Ω**;`VL/VH` 由 23.2 % 塌到 **0.64 %**(60 点里 **21 点低于 5 %**);`Cs` 在 6.77 V → 13.66 V **一个倍压之内掉 14 倍**(434 → 31 pF)。`Z = R0·(VH−VL)/VL` 里 `VL` 是分母 ⇒ 其相对误差被 **`\|Z\|/R0`** 放大(100 V 处 = **153 倍**)直进 `Cs`。<br>**③ 判据是推的、不是试的**:误差只有两个方向相反的来源 —— `\|Z\|≫R0` 时 `VL` 的误差被放大 `\|Z\|/R0` 倍,`\|Z\|≪R0` 时 `VH−VL` 的误差被放大 `R0/\|Z\|` 倍,**两者在 `\|Z\| = R0` 处相等** ⇒ 最优点 **`VL/VH ≈ 70.7 %`**。四挡是等比级数 ⇒ 在 **log 轴上取最近**;边界恰在相邻两挡正中间而 `\|Z\|` 有噪声 ⇒ 逐点取最近会来回跳,**每跳一次曲线就换一次标定基础** ⇒ 加 **0.15 十倍程迟滞**。落地 `pynq_regs.h::cv_range_pick()`(纯判据,不写寄存器)+ `pynq_protocol.c::sweep_autoscale()`(**趁本点新鲜 `\|Z\|` 先换好下一点的挡**,放这个位置是因为它在 `sweep_measure_point()` 之后、`RelayView_ApplyGear()` 不阻塞)。<br>**④ 换挡失败即粘滞关闭**(`s_auto_gear_off`)—— 判据是确定性的,失败后每一点都会算出同一个"该换"的结论,不粘滞就会**每 300 ms 重试一次并弹一次框**,把扫压搅成一片报错。<br>**⑤ 两条**有意不做**:① 第 1 点不做预扫**(预扫要在 `Process()` 里阻塞 ~300 ms,而 `sweep_start()` 是从触摸回调调进来的 ⇒ 会冻住界面);② 扫压结束后**不恢复用户的手动挡位**。<br>**⑥ 验证(主机侧,不需要 Vivado、不需要板子)**:`sim/tb_gear_pick.c` 自检 **10/10 PASS / 退出码 0**(日志 `sim/logs/tb_gear_pick.log`,报告 `sim/gear_pick_verification.md`),**输入是实测的**那 60 组 `Z` 不是构造的。三条跑道:① 固定 B 挡最坏 `VL/VH` **0.65 %**(**负对照 —— 证明判据有失败能力**,不是"怎么都能过");② 无迟滞 **4 次换挡 / 2 次方向反转 / 挡位非单调**;③ 迟滞 0.15 **2 次换挡 / 0 次反转 / 单调 B→C→D**,`VL/VH` **21.95 %–83.41 %**。**迟滞的直接测试**:让 `\|Z\|` 停在 B/C 分界 316.2 Ω 上左右晃 ±5 % 跑 40 次 —— 迟滞 0 ⇒ **39/40**;迟滞 0.15 ⇒ **0/40**(同代码同输入,**只改迟滞宽度**)。<br>**⑦ 纠错 5 条,最有价值的是第 1 条**:`VL/VH` 我写成了**标量式** `R0/(R0+\|Z\|)`,正确的是**相量和的模** `R0/\|R0+Z\|` —— 两者在 `\|Z\|≫R0` 时接近、在 **`\|Z\| ≈ R0` 附近差近一倍**,而本判据**恰好**在那里做判断;⚠️ **同一个混淆在同一会话里犯了两次**,第二次才发现是同一个错,而且**是靠"与实测比例对不上"发现的(B 挡实测 84.9 % vs 标量式 59.5 %),不是靠重读自己的推导**。② `strings \| grep '偏压已归 0'` 返回 0,我**差点据此判定"修复没编进去"** —— GNU `strings` 默认只打 ASCII,**UTF-8 中文串根本不会出现**,改 `grep -ao` 扫二进制 → **3 次**:**是我的工具错了,不是代码错了**。③ `pgrep -f` 报两个 PID,是**假阳性**(模式串出现在我自己那条远程 shell 命令行里),`ps … \| grep -a '[c]_ui'` 交叉核对只有一个。④ **`0x80000 = 0 V` 这个错误口径还留在 `RequestReset()` 的注释里** —— 代码(`bias_apply(0.0f)`)是对的,**错的是注释**,已就地更正。⑤ 换挡日志的 `VL/VH` 一度拿 **C 挡的 `\|Z\|` 去算 B 挡的预测值**,改为一律从 `s_pair.mag_a_v/mag_b_v` 当场取实测。<br>**⑧ 离场状态**:两条修复**都进了板上二进制**(`grep -ao '偏压已归 0' /home/xilinx/c_ui_src/c_ui` = **3**,`偏压保持在当前点` = **0**),但**本轮没有跑过一次带自动换挡的扫压** —— 本轮最后一个上电周期以用户「先关一下系统吧」结束。板子已 `shutdown -h now`(ssh 2.8 s 断开 + ping 15 s 无响应),**断电由用户执行**。 | ⚠️ **部分可用** —— **代码侧两条都落地、自检通过;两条都还没上板验过**。⛔ **不拿"代码写完了"当"功能验证过了"**:扫压收尾路径**从未实跑**,自动换挡**从未在真扫压里切过挡**。 | → `skill/` 两条**待成文**:① **「量程判据 = 让放大系数最小的那个量居中」**(判据不该取"信号大"或"信号小",该取两个误差来源的**交点**;等比挡位 ⇒ 在 log 轴上判 + 迟滞);② **「先给测试台一个失败的能力」**(负对照比任何 PASS 都有说服力 —— 没有负对照的自检,通过数再多也不能排除"判据恒真") |
| [LLM-065](LLM-065.md) | 2026-10-15 | Claude | **切回固定挡位 + 上板坐实「数据很奇怪」的根因是 CRSS 腿模拟前端饱和** —— 用户原话:「**还是很奇怪，要不先切换会固定档位的模式把**」→「**自己重启把**」→「**先关系统吧**」。<br>**① 回退用文件开关、不用环境变量**:环境变量要重启服务才生效,而**重启 `qrs-ui.service` = 第二次 `Overlay()` = 红线**;`/home/xilinx/sweep_auto_gear` 在 `sweep_start()` 时**现读不缓存** ⇒「改文件 → 下一场扫压」即生效,代价为零。改动 5 处(全在 `pynq_protocol.c`):`:126` `#include <unistd.h>` / `:168` 定义 + 理由注释 / `:200` `sweep_auto_gear_enabled()` / `:1007` `sweep_autoscale()` 早退 / `:918` 起手日志**明说**当前模式。**判据一行没改、`sim/gear_pick_verification.md` 那张表全部仍然有效** —— 被推翻的是「现在就该启用它」,不是判据的算术。<br>**② 编译与核对(自己做完,不等用户)**:源码两边 md5 `183e40ff…`;板上 `make -j2` **exit 0**(日志里**明确看到** `-c app/pynq_protocol.c` 重编 + 重新链接);新 `c_ui` md5 `6bec1b4b4290305ee91968215be34e86`;旧件留底 `c_ui.prev` / `557ed63c…`;`ldd` 缺失库 **0**。**新旧正反对照**(⛔ `grep -ao` 不用 `strings`):`自动换挡默认关闭` **1/0**、`扫压期间固定不变` **1/0**、旧话术 `第 1 点用` **0/1**。⚠️ **正在运行的执行文件不能被 `make` 覆盖(Linux `ETXTBSY`)** ⇒ 先 `mv c_ui c_ui.prev` 再 `make`(rename 不动已运行的 inode),**顺带白得一份回滚件**。<br>**③ 软重启 + 上电核对全过**(用户授权「自己重启把」):`qrs.bit` md5 **`232c649d…`** ✅、`.hwh` `6c376905…` ✅、`dmesg` **恰好一条** `writing qrs.bin` ⇒ **本周期位流只加载一次,红线守住**;重启后服务 `disabled`+`inactive`(符合纪律),`systemctl start` 后 `active`;FCLK0 **50.0000 MHz**、采样 **32.50 MSPS DIV=4 PH_OFF=+1**(`0x88=0x00104`)、激励 1.000 MHz;`0x04=0x00000000`、**`0x14=0x00000000`(偏压归零修复冷启动即 0)**、`0x18=0x00000002`(B 档复位值)、`0x1C=0x00000010`(`ALL_OFF`)、`0x84=0x00000001`。<br>**④ 用户跑了一场扫压,三项验收全过**:① 固定挡位**60 点逐点**印 `挡位 0x2`,起手行印 `🔒 扫压期间固定不变`;② 三腿继电器**真的逐腿切**:`[relay] 回路 → CISS(0x7F) → RG(0x3C) → CISS(0x7F) → COSS(0x61) → CRSS(0x51)` ⇒ 三条曲线不再共用一份数据(`LLM-062` 结清的「四处假装」之一**上板确证**);③ 偏压归零见上。<br>**⑤ 🔴🔴 核心发现 ——「数据很奇怪」的根因是 CRSS 腿两路 ADC 都顶到满量程**:\| 腿 \| A 峰峰 \| B 峰峰 \| OTR 非零 \| `\|VL/VH\|` \| 判定 \|<br>CISS 1199–3516 / 943–3516 / **1/72** / 74 % 稳 ⇒ ✅;<br>COSS 1636–2876 / 133–3569 / **1/60** / 7.3 % 稳 ⇒ ✅;<br>**CRSS 2815–4095 / 3213–4095 / 58/60(最大 43)/ 0.09 %–3.3 % ⇒ 🔴 饱和**。<br>`4095` = **12 bit 满幅**(`VFS_PP = 2.0 V / 4096 码`);**B 通道基波塌到 0.002–0.008 V 而原始峰峰是满量程** ⇒ **信号没了只剩削顶**,`Z` 飞出 2955 Ω → **127 kΩ**,`Cs` 出 **−210.7 / 507.5 / 2.56 pF** 这类非物理值。⚠️ **成因未定位**(继电器组合?前端增益?DUT 侧耦合?)—— **需夹具原理图**,不在手上。<br>**⑥ 两条被推翻的旧结论(本轮最有价值的部分)**:① `LLM-064` 的「自动换挡让数据变好」**不成立** —— 它把**唯一一条随偏压变化的曲线**从 B 顶到 C、CRSS 继承 C 再顶到 D 后毁掉;且 COSS 腿 @0.100 V 同一次里 `VL/VH` = 7.36 %("100 Ω")与 39.3 %("1000 Ω")对任何无源 `Z`、任何 10× 电阻对**反解出 `\|Z\|² < 0`** ⇒ 这两条读数**不可能同时为真**,判据脚下的 `\|Z\|` 本身不可信。② 「接线整理后四挡读数一致了」**是虚的** —— 那场**偏压是关的**:偏压关 3504.8/4750.2 pF ⇄ 偏压开 2055.4/118 pF,而整理接线**之前**偏压开是 2055.5/114 pF ⇒ **接线整理没有改变任何东西**。<br>**⑦ 纠错 5 条**:★ 最重的一条 = **两个变量一起动时,"看起来更好"什么都证明不了**(前一条同型错在 `LLM-063` 纠错① 已犯过一次,故记 **同型复发**),而且推翻它的证据**本来就在我自己的日志里**,不是靠新做一次实验拿到的;② 我一度把日志里「挡位 10000 Ω → 1000 Ω → 100 Ω」误读成**迟滞失效、挡位来回跳**,回原始行才发现那是 **CISS 腿第一个点继承上一条腿的 D 挡再两步收敛** —— **恰恰是判据该有的行为**;③ 自写的日志解析器把腿归属算错(分组表 `VL/VH ≈ 90 %` vs 原文 7.4 %),**我没信自己的解析器**,退到 `sed` 逐行原文确认;④ `ETXTBSY`(见 ②);⑤ `echo xilinx \| sudo -S -p '' wc -l < /var/log/qrs-ui.log` **读口令失败** —— 重定向 `<` 由**远端 shell 先于 `sudo`** 执行,`sudo -S` 的 stdin 被换成那个文件,它拿日志头几行当口令(两次 `Sorry, try again.`),改成把文件名当**参数**即可。<br>**⑧ 离场状态**:日志 5384–6103 行存档 `report/measurements/20261015_uilog_fixed_gear_sweep.txt`(md5 `8c17256b861ed0e099d2b007d86c10c8`);**前 4141 行 md5 与上一份存档逐字节相同** ⇒ **追加不截断**这条性质**实测**成立,历史行号引用不会失效;`systemctl stop qrs-ui.service` ⇒ `inactive`+`disabled`、`c_ui`/`main.py` 均已退出;`sync` 后 `shutdown -h now`,**ping 100 % 丢包**。⚠️ **万用表、真断电、接线仍归用户** —— **我当不了仪表**。 | ⚠️ **部分可用** —— **三项上板确证**(固定挡位 / 三腿逐腿切 / 偏压归零),**但 CRSS 腿饱和只坐实了症状、成因未定位**。⛔ **不拿"找到症状"当"解决问题"**。 | → 待建 `skill/two-variables-one-move/`(**两个变量一起动时,任何"看起来更好"都不构成证据**;判据 = **一次只动一个,并且把"没动"的那一次也记下来**)—— 同型复发说明**不是一次疏忽,是我的默认推理方式有漏**;+ 运维形态一条:**当"重启进程"本身是红线时,把"编译期决定"改成"运行时现读"**,代价从「一个上电周期」降到「零」 |
| [LLM-066](LLM-066.md) | 2026-10-15 | Claude | **DAC 上电默认电压追查 ⇒ 查实 `0x14` 复位值在 DF=0 下是 −100 V(安全项),已改 + 重综合** —— 用户原话:「**dac上电后默认电压是多少，我发现dac一直输出5v**」→ 查实后:「**把pl端默认值给改了重新编译**」。<br>**① 先分三层回答、不混**:芯片层 **0 V**(有出处 —— TI SBAS082G Table 13 `DF` 默认 = 0、p17「**After reset, the DIR contains zero.**」、**bit9 CRST 说明原话**「**If DF is 1, zero gives 0V; if DF is 0, zero gives VREF (mid-scale)**」⇒ DIR=0 ⇒ 芯片中位 2.5 V ⇒ 模块 **0 V**);板上实际(改前)**−100 V**(见 ②);用户量到的 **+5 V** 不属于任何一种 ⇒ 未定位(见 ⑤)。<br>**② 🔴 真发现:`cv_axi_lite_slave.v:514` 的 `bias_code <= 32'h0008_0000;` 注释写「中位码 = 0 V 偏压」,依据是**旧的 DF=1 传递式** `V_module = −10 + 20 × code/1048575`**。而 `dac_1220.v` 的 CMR 已于 **2026-10-12 由 `0x20A1`(DF=1) 改成 `0x2081`(DF=0)** ⇒ **同一个码翻到量程另一端**:`0x00000` 由 −10 V 变 **0 V**、`0x80000` 由 0 V 变 **−10 V** ⇒ 经 10:1 高压级 = **栅极 −100 V**,**恰是那段注释自己写着要避开的"`Vgs` 最大"那一端**。消费时机:该复位值只经「上电自动发一次 `bias_set`」下发,而该脉冲**门控在 `dac_init_done`** ⇒ **被消费时 DF=0 是确定的**(CMR 写在同一条初始化序列里),旧注释错的只是算术。<br>**③ 修复(用户指令)**:`32'h0008_0000` → **`32'h0000_0000`**;注释整段按 DF=0 重写(补实测式、两个码各多少伏、**并写明"这是由 DF=1 改来的、旧值错在哪"**,不删旧结论)。另按既有纪律改齐另外 **4 处**同源假话(`docs/dac1220_interface.md:237` / `report/source_design_checklist.md:417` / `docs/dev_handbook.md:183` + 交付镜像)—— 🔴 **同一句假话印在多处、改一处不改另一处 = 留下自相矛盾的证据**。<br>**④ 重新综合(用户指令「重新编译」)**:用**交付仓库既有的** `build/build_bitstream.tcl`(它 `source` `build/report_summary.tcl` 出 `build/summary.txt`;每次 `reset_run` 重跑、**不复用旧 run** —— 本工程踩过「工具静默跳过重建」)。结果:**位流 md5 `c92c7ed5b7d695dee73d86abc1c6089c`**(旧 `232c649d0cc143c12c87f17365f6ddd3`);**WNS +0.316 / TNS 0.000 / 失败端点 0/5472**;**WHS +0.065 / THS 0.000 / 0/5472**;WPWS 3.346 / 0/2047;**LUT 1790(3.36%)/ FF 2022(1.90%)/ BRAM 7.5(5.36%)/ DSP 0 / IOB 67(53.60%)**;**DRC ERROR 0**;`.hwh` **未变**(BD 没动)⇒ `6c376905…`。已同步 `qrs_fpga_2026/board/qrs.bit`(**改名对齐 `config.OVERLAY_BIT`**),两边 md5 回读一致。<br>**⑤ ⏳ 仍未结案 —— 用户的 +5 V**:DF=0 下 +5 V 对应 —— 模块端 ⇒ 码 `0x42E86` / 界面应显示 +50.00 V;栅极端 ⇒ 模块 +0.500 V / 码 `0x08A3F` / 界面 +5.00 V;芯片 VOUT 引脚 ⇒ 码 `0x7FFFF`。**需要用户回答两件事**(我当不了仪表):量的是**哪两点**;这个 5 V 是**恒定**还是**只在上电后一小段时间**。⚠️ 原拟的判据「起界面前读 `0x14` 应 = `0x00080000`」**随本次修复已改变** ⇒ 下个上电周期应读到 **`0x00000000`**。<br>**⑥ 纠错 4 条,最有价值的是第 2 条**:★ **`qrs.v:75` 的硬编码 `20'hC0000` 在 DF=1 下精确等于 +5 V**,且就在**同一目录**,几乎就是答案 —— **是靠 `.hwh` 的 `MODTYPE` 列表否掉的**(两棵树都只有 `cv_sys / proc_sys_reset / processing_system7 / smartconnect`)⇒ **`qrs.v` 是 P1 时代的独立顶层,不是部署顶层**。📌 **"看着最像"的线索必须先证伪再往下报**。② 我一度**用错了传递式的年代**:`project_state.md:791` 的「`0x086666` = 模块 +0.500 V ×10 = +5.00 V」是 **DF=1 时代**的,DF=0 下同一个码是 **−9.56 V** ⇒ **凡引用 2026-10-12 之前的码↔电压读数,必须先确认属于哪个 DF**。③ 我最初把「上电 −10 V」当成"设计有意为之的安全值",**是注释里那句"上电停在 0 V 是无条件安全的中点"把我纠正过来的** ⇒ **注释写的"意图"和实际打架时,后者才是要报的那个**。④ ★ **我重复造了一个本来就存在的构建脚本** —— 我断言「工程里没有综合脚本」后**只在工作区找了一遍**,新写 `build/run_impl.tcl` 并跑出位流(`98318ad0…`);事后才发现**交付仓库里 `build/build_bitstream.tcl` 早就在**、功能完全等价 ⇒ **已删重复件、用既有脚本重跑整条链**(新 md5 `c92c7ed5…`,报出的数**一字未变**,顺带把 LLM-063 那条「源没变、P&R 不逐比特可复现」**实测复现了一遍**)。📌 教训:**两棵树 `src/` `build/` `sim/` 是对称的 —— 一棵树里找不到 ≠ 另一棵也没有**。<br>**⑦ 离场状态**:板子**关机中**(`ping` 不通),**新位流尚未上板**;**真断电 / 接线 / 万用表仍归用户**。 | ⚠️ **部分可用** —— **查证与修复完成、新位流已出且时序/DRC 全过;但从未上板**。⛔ **不拿"综合过了"当"修好了"**。 | → 待建 `skill/range-code-rotation/`(**当芯片的格式位(DF/极性/offset binary)变更时,所有以"码"为单位的既有常数必须逐个复算** —— 同一个十六进制数在新格式下可能恰好是"另一个极端";判据 = **格式变更必须列出"所有含码常数的位置",而不是只改那个"看起来相关"的**。本项目这次:PS 侧映射当天改了,**PL 侧复位值漏了三天**);+ 运维形态一条:**「不复用旧 run」的构建脚本**(`reset_run` 每次都重来,拿几分钟换掉"工具静默跳过重建"这类**无提示失败**) |
| [LLM-067](LLM-067.md) | 2026-10-15 | Claude | **上板核验复位值 ✅,但「PL 空着没有」的判据我自己也是错的;Python 侧还有第三处 DF=1;恒定 5 V 未结案** —— 用户原话:「上电了，尝试设置一个电压测试」→「你能读到芯片的寄存器值吗，现在还是5v感觉硬件有问题」→「应该是dac坏了，先关系统吧」→「恒5v」。<br>**① ✅ 上次那处修复在板上验过了**:位流刚配好、`apply_fclk0()`/`release_dac()` **之前**读 `0x14` = **`0x00000000`**,正是 DF=0 下的 0 V(**若还是旧位流会读成 `0x80000` = 栅极 −100 V**);同批快照 `RANGE_SEL=0x02`(B 挡)/ `RELAY_CFG=0x10`(`ALL_OFF`)/ `CV_CTRL` `0x00→0x01`;`apply_fclk0()` 实测 **50.0000 MHz**(IO PLL 1000 / DIV0 20 / DIV1 1)。<br>**② ⛔ 「能不能读 DAC 芯片的寄存器」= 不能,原因在 RTL 里**:`dac_1220.v` 例化 `spi_master` 时 `miso(1'd0)`、`rx_data()` 悬空 ⇒ 文档里挂着的两项「CMR 那次写落没落进芯片 / 自校准跑没跑」**用现有位流永远结不了案**(要么改 RTL 出新位流,要么上逻辑分析仪)。<br>**③ 🆕 两个新工具把「改电压」的代价从"一次位流"降到"零"**:`tools/bias_set.py`(本周期**唯一一次** `Overlay()` + 读复位值 + 配 FCLK0 + 放行 `0x84[0]`)/ `tools/bias_mmap.py`(**裸 mmap,一次都不加载**;开头自己查 devcfg,PL 空着就**拒绝运行** —— 空 PL 上读 `0x43C00000` 会把 CPU 挂在 AXI 上)。<br>**④ 🔴 数字侧全对、表针不动**:五点写 `0x00000 / 0x26666 / 0x00000 / 0xD9999 / 0x00000` **逐点回读 ✅**,而用户读数 **恒 5 V、全程没动**。⚠️ 同一个码 `0x26666` 在 **2026-10-12 量到 +2.80 V** ⇒ **这不像"公式错了",像"链子断了"**;DF=0 下模块满量程 −10…+10 V、码 `0x00000` 对应 0 V,+5 V 只在码 `0x40000` 附近 —— **寄存器里没有这个值**。<br>**⑤ 🔴🔴 纠错四条(前三条全是"没实测就断言")**:① 我第一版 `pl_state.py` 读的 `PCAP_STATUS(0xF80070C4)` **根本不是 devcfg 的寄存器** ⇒ 恒读 0 ⇒ 在**已配置**的 PL 上误报「⛔ PL 是空的」;② 改读 `STATUS(0x14)` 之后我又用了**背来的**判据 `CFG_RESET_B==0 && GPWRDWN_B==0` —— **实测这两位加载前后都读 1、根本不区分** ⇒ 判据改成**在同一块板上加载前后各读一次**校准出来的 **`bit10 PSS_FST_CFG_B`**(空 PL `0x40000A30` / 已配置 `0x50000F30`);⚠️ `INT_STS` bit2 只测了"加载后"那一列,已如实标注**不算已验证的判据**;③ **板上既有的 `tools/step1_plstate.py`(我 2026-10-03 写的)把 `STATUS` bit5 标成 `PCFG_DONE`** —— 厂商头文件原文是 `PSS_CFG_RESET_B`,真正的 `PCFG_DONE` 在 **`INT_STS(0x0C)` bit2**(`XDCFG_IXR_PCFG_DONE_MASK = 0x04`)⇒ 那个"安全探针"**在空 PL 上也会回「已配置 ✅」**,恰在最危险的场合失效,已就地更正;④ 🔴 **`cv_regs.py` 是同一个 DF 位的第三处落点**(第一处 `dac_1220.v` 的 CMR、第二处 RTL 复位值都已修):`bias_volts_to_code(0.0)` 会算出 `0x80000` = **−100 V**(**"要 0 V,给 −100 V"**)⇒ 已按 DF=0 重写(`BIAS_CODE_ROT` / `BIAS_CODE_ZERO`)+ 主机端五点自检;⇒ 📌 **改一个"语义位"必须全仓搜"所有算这个物理量"的地方,不是"我想到的那处"**。<br>**⑥ ⚠️ 我在板上又建了一棵重复的树**(推 `src/python` 到 `/home/xilinx/qrs_py`,而 `/home/xilinx/cv` 早有一份逐字节相同的)= **同型错误第 4 次** ⇒ **现在一共三棵树:工作区 / 交付仓库 / 板上 `/home/xilinx/cv`**。<br>**⑦ ⚠️ 仓库漂移(未修)**:`src/rtl/dac_1220.v` 仍是 **DF=1** 的 `0x20A0/0x20A1`,而工程里已是 `0x2080/0x2081`;`swm.v` 两份不同;**`src/rtl/` 只有 4/18 个模块**(其余 14 个只在 Vivado 工程里)= **M1「完整工程文件与源码」的缺口**。<br>**⑧ 离场**:板子**关机中**,偏压停在 `0x14 = 0x00000`;**恒定 5 V 的三种可能一条都没排除**(DAC 芯片/模块坏 / SPI 接线动过 / 模块模拟级坏)⇒ 下个上电周期**第一步不是加载位流,是拿表量模块三个点:基准脚(期望 ≈2.5 V)/ 供电脚 / 输出脚** —— 基准脚本身就是 5 V 的话,故障在模拟级不在 DAC。⛔ **真断电 / 接线 / 万用表仍归用户 —— 我当不了仪表。** | ⚠️ 部分可用 | 候选 `skill/register-bit-calibration/`(待提炼)+ `tool-zero-hits-suspect-the-tool` |
| [LLM-068](LLM-068.md) | 2026-10-15 | Claude | **扫压末端尖峰的根因 = 整次采集混进坏样本(不是算术),以及用户口径「多测几次取合理值」的正确形态** —— 用户原话:「**我想知道什么原因，或者扫压的时候就是多测几次取合理值让结果更稳定行不行，就是尽量不要发生这种情况**」→「**按照最合理的方式进行修复**」。<br>**① 根因:同一个缓冲里两个数互相矛盾** —— `B/INB 幅值 0.0067 V`(1 MHz 单点相干解调、8190 点平均)隐含峰峰 `2×0.0067/2.0×4096 = 27 码`,而同一次采集打出的 `峰峰` 是 **2588 码**,差 **94 倍**(正常点一致到 ~2%);另有一次 `A码 = B码 = 3205`(两路读到**同一个字**)的指纹。机制:A 与 B 是**同一条 12 位总线上的两个时分字**,采样刻度错一拍 ⇒ A 读到 B 的字 ⇒ `VH ≈ VL` ⇒ `Z ≈ 0` ⇒ Cs 爆掉;只在 **CRSS 高压端**放大,因为 `Cs ∝ 1/VL²`,那里 `VL = 23 码`(0.56% 满量程)。指向采样刻度距总线边沿仅 **1 tick = 7.69 ns**,与 **2026-10-15 `P=2→4` 改动**(Cs 离散度 3.86% → 0.42%)是同一机制。⚠️ **已明说这是推断不是证明**。<br>**② 回答"多测几次取合理值":方向对,中位数不行** —— 坏点不是"读数有噪声",是**整次采集都错**,N 次取中位数会让**错的多数赢**;有效形态是先问"**这一次算不算数**"(由物理不变式定义),不算数就**换一次重采**。⇒ **「合理」由不变式定义,不由投票定义。**<br>**③ 重采可行 —— 实测**:同一偏压连测 ≥4 次的 **63 组,全部"有时标、有时不标",次次都标的一组都没有**。<br>**④ 判据 `码 ≤ K × 2×幅值 + FLOOR`(单边),阈值从 2039–4462 个真实样本里选**:只看 `OTR ≠ 0` → 抓 128/347(36.9%)、误伤 24/1692;K=2.5/F32 → 347/347、误伤 155/1692;★ **K=3.0/F64 → 342/347(98.6%)、误伤 31/1692**;K=4.0/F128 → 341/347、误伤同 31。误伤集中在"较小那一路只有 50–200 码"的点(6.1%),200 码以上只有 1.2% ⇒ FLOOR 为此而设。<br>**⑤ 落地**:`AdcSample_PpConsistent()`(判据)+ `adc_capture_pair()` 重采外壳(`adc_capture_pair_once()` 改名,返回语义**不变** ⇒ 三个调用方零改动)+ 账本(`拒收/救回/判无效`)+ 日志新增 `码/期望 %.2f / %.2f \| 采 %u 次` 两列;`CV_ADC_TRIES`(1 = 关掉重采,专门用来做 A/B 对照)。<br>**⑥ 纠错五条,前两条最值钱**:★ **差点把阈值按理论定死而非按数据定** —— 我第一反应是"K=3 太松"(A 路下 `3×1352+32 ≈ 4088` 近乎满量程,看着拦不住任何东西),**若照直觉调小会把大量好点判掉**;实测 A 路最大比值只有 **3.71** ⇒ K=3.0 在 A 路完全安全。★ **第一版方案过大,自己砍掉** —— 原设计"N 次取中位数 + 第二对 64 KB 暂存 + memcpy 拿胜者";写之前意识到自洽闸已挡掉整次坏掉的采集、活下来的点之间只差 ~0.4%,而"重采到第一次自洽"**不需要任何额外交互缓冲**。另三条:**把 `~6 ms` 的估计值写成了实测值的口气**(自查后改为显式标注"估计,未实测"并附测量方法)、**一个假设(`sweep_autoscale`/`PushPhase` 会在采集途中重写 `0x88`)被自己证伪后如实告知而未悄悄丢掉**、**板卡目录结构第 5 次猜错**(C 源在 `/home/xilinx/c_ui_src/app/`,`/home/xilinx/cv/` 是纯 Python 树)。<br>**⑦ 🔴 交付仓库同步时 `rsync` 报的"差异"方向恰好相反,照它同步会把仓库打回旧版**(已逐个用 md5/内容/时间戳核过才动手):`src/rtl/` 工作区那份是 **9 月残本 5 个**,仓库那份才是**当前真本 18 个**;`board/qrs.bit` 工作区 `232c649d…`(**10-14**)vs 仓库 `c92c7ed5…`(**10-15 第 61 次,现行**);`report/measurements/20261005_*.md` 两边**只差 `report/` vs `board/` 路径前缀**。另:仓库把 LLM 记录放在**顶层 `llm-log/`**、工作区放在 `report/llm-log/` ⇒ 直接 rsync 会造出一棵重复的树。<br>**⑧ 分析日志时又自纠两条**:★ **搜错了方向** —— 我从 `⛔` 行**向前**找证据行,拿到的是**上一个点**的 `采 N 次`,算出"被弃的点却只试了 2 次"这种自相矛盾的数;根因在代码结构:**`⛔` 由 `adc_capture_pair()` 内部打印,证据行 `adc_log_pair()` 由调用方在其返回后打印** ⇒ `⛔` 在证据行**之前**。改成向后搜才对。**教训:定位证据行前先在源码里确认调用顺序,别按"先因后果"的直觉排布。** ★ **自己写的账本措辞有歧义** —— `采集自洽账本` 打在 `sweep_finish()` 里,读着像"本轮",实际是**进程累计**(`s_adc_*` 只在 `pynq_protocol.c:1322` 进程初始化时清零,`sweep_start()` 没清)⇒ 223 − 199 = 24 次来自扫压前的单点测量;**如实告知用户口径问题,没让它被当成"本轮拒收 36 次"**。 | ⚠️ **部分可用(已上板验证有效)** —— **⑧ 上板实证(2026-10-15,同一台设备同批偏压)**:改前有**三个点逆着趋势往上跳**(`Cs = 21.26 / 519.52 / 13.43 pF`,CRSS 高压端本应单调下降),改后落到 **15.57 / 14.74 / 12.31 pF**,曲线恢复单调;**`519.52 pF` 就是用户最初报告的那根尖峰**。判读用**单调性**不用插值(插值会循环论证)。`码/期望` **max 167.04 → 7.54**;比值 > 10 的点 **5 → 0**;> 50 的点 **3 → 0**。账本 `实际采 223 / 拒收 36 / 救回 15 / 判无效 5`。完整记录:`report/measurements/20261015_sweep_consistency_gate_verified.md`。<br>⛔ **两处代价未消除,不得当成功**:① CRSS 腿 **5/60 点被判无读数**,其中 **4 点在 0.2–1.3 V 陡峭段**(`码/期望` 只 4–6)⇒ 属"过度收紧区",但**只能说"判据把这 4 个点判没了",不能说"这 4 个点本来就该丢"**;② **严格 A/B 未做** —— 上面是"先后两轮同参数",不是同时刻交替;真对照要改 `qrs-ui.service` 的 `Environment` = **重启服务 = 第二次加载位流**(红线)⇒ **只能等下一个上电周期**;`~6 ms` 仍未实测。 | → 候选 `skill/consistency-gate-over-median/`(**整次采集坏掉时中位数治不了,要靠"物理不变式 + 重采"**;凡"同一份数据里两个数本应互相印证"的采集系统都适用)+ 候选 `skill/sync-direction-must-be-proven/`(**两棵同构树同步,"有差异"不指示方向**) |
| [LLM-069](LLM-069.md) | 2026-10-15 | Claude | **CRSS 高压端坏点的根因:从「采样刻度错一拍」改判到「ADC 真的过载了」(OTR 作证)** —— 用户报告「**我发现50附近测量crss确实容易出问题**」+「**我看到的调试界面就是下面那个曲线有波动不太干净的正弦波,你先分析一下这些坏点原因导致的**」;产出 `report/measurements/20261015_crss_bad_points_root_cause.md`,**一行代码没动**。<br>**坏点在哪**(坏 = `ppA ≥ 2000 码`):干净 **213/245(86.9%)**;高压过载 **20 条全部在 CRSS**(低压过载 12 条横跨三条腿)。**CRSS@50.000 V 坏 11/15(73%)、@55.000 V 坏 8/9(89%)、@0.100 V 只 1/20(5%)、CISS 扫压 60 点全程 0%** ⇒ **回路与偏压两个变量都成立,不是单因子**。<br>**指纹一:解调干净、峰峰炸** —— CRSS ≥39 V 采纳点 A 路解调幅值 CV 仅 **0.14%**,同期 ppA 1348→1354 ⇒ 极少数样本的瞬时越界,不是整段污染、不是算术错。<br>**指纹二:ppA 落在离散台阶上** —— 干净 1060–1640,另一簇 **2348–2787**:干净信号跨 `[1377, 2719]`,**一个**样本打到上轨 4095 ⇒ `pp = 4095−1377 = 2718`,实测那簇 2731/2737/2740(**差 <1%**)⇒ **「恰好一个样本到轨」**;`ppA ≈ 4095` 那簇 = 两端都到轨。**A、B 一起胀**(CRSS@55 V 干净时 1351/32,坏时 3968/3932;20 条高压过载里 **18 条 B 也 ≥2000**)⇒ **共模**,量级 ≡ 满量程。<br>★★ **决定性证据 `adc_otr`** —— 它是 **AD9238 自己的输出引脚**(`Y8`,`led.xdc:130`;手册 *"Out-of-Range Indicator for Channel A/B"*,脚 58 / 脚 39):**干净 213 条里 0 条置起(0.0%)**;高压过载 16/20;`ppA ≥ 3900` 的 **13 条 100%**。数据总线 12 根、OTR 是**独立的第 13 根**,且计数只 **1–10 / 16384 字**(「刻度错一拍」会每周期都错 = 8190 次/采,**量级差三个数量级**)⇒ **ADC 自己判定输入越了量程**。<br>**机制候选**:H2a **CRSS 组合下栅极浮空**(1 MHz 经 `Cgd` 整流给浮空节点充电到击穿)领先 —— 一次解释「只有 CRSS」+「只有高压」+「1–10 次/8192 的稀疏率」;H2b 触点微弧;H2c 偏压放大器尖峰(**解释不了「只有 CRSS」**);H1 刻度错一拍(**被削弱未排除**)。**判别实验**:★(a) CRSS 夹具加 **1 kΩ 栅–源电阻**;(b) CRSS@50 V 关掉激励再采;(c) **软件打印越界样本的序号/值/通道** —— 纯 C、**不占位流周期**,是唯一能把 H1/H2 分开的实验。⛔ **明确说了不要用放宽阈值来「修」** —— 142/208/362 这种量级放宽度覆盖不了,只会把 519 pF 重新放回曲线上。<br>★ **自纠 8 条**:① **差点拿归属逻辑错的脚本的数当结论** —— `/tmp/rate2.py` 用 `[proto] 点测…` 行归属回路,而那行**在扫压期间会残留**,把 227 次扫压捕捉全算到「CISS@0.10 V」名下,并据此写下「**是路径依赖、不是偏压依赖**」;重算后**两个变量都成立**⇒ **归类要用「写下去会被回读确认」的状态行(`[relay] 回路 → X`),不能用「打印一句话表示意图」的行**;② **差点把「界面上的正弦不干净」直接算成坏点造成的** —— 波形页只喂 **96 点**、越界样本 **1–10 / 8192**,命中概率 **1%–12%** 对不上 ⇒ **如实标为「未查证」**,没有为了让故事完整而把两者焊在一起;③ 两个 OTR 口径差点混成一个(逐字位 vs `0x90[3]` 粘滞;**逐字位是 2026-10-14 修好位序后才有效的**),`LLM-068` 里「正常点 4–5%」与本次 **0/213** 对不上 ⇒ 旧口径标存疑不删;④ 同一份日志有 **11 个 `c_ui 启动` 段,采样配置不同**(前 4 段 65.00 MSPS / 后 7 段 32.50 MSPS),245 条**逐条核过**才敢用;⑤ **没把 H1(数字)从结论里删掉** —— `led.xdc:213`「ADC 输入时序**有意不约束**」(杜邦线 tPD ~4 ns)仍是事实 ⇒ 结论写成「**压向模拟侧,不是焊死**」。<br>⚠️ 判别实验一个都没做;⛔ 严格 A/B 仍未做;`~6 ms` 采集耗时仍是估计值 | ⚠️ 部分可用 | 候选 `skill/classify-by-state-not-by-intent/` + `skill/independent-evidence-must-share-no-pipeline/` |
| [LLM-070](LLM-070.md) | 2026-10-17 | Claude | **扫压参数页并入「结果汇总 → 扫压结果」+ 触摸读数 + 实测数据二维码** —— 用户四条指令:参数页移到结果页、曲线可按住/滑动读单点值、删 AI 质量、二维码移到「数据传输」且「照 `~/Desktop/qrs` 那个」。<br>产出:`app/ai_cv2spice_acv1.*`、`app/ai_cv_sweep_payload.*`(前序逐字移植)、`app/cv_qr_payload.*`(公共区间 + ACV1 节点 + SCV1 隐式网格重采样 + URL)、`app/cv_qr_live.*`(生命周期)、`tests/qr_payload_test.c`;`ui_demo.c` 删除独立参数页/右滑/头部动画/AI 质量,新增 286 高曲线面板 + 探针读数行 + 分析电压 + 3×2 参数卡,以及 288 px 二维码传输页。<br>★ **关键发现**:网页解码器**不传电压表**,按 start/stop **重建隐式网格**(`i*20/60` → 整数 → 补 stop → 首点替换为 `max(start,0.1)`),100 V 时正好 **141 点** ⇒ C 侧必须按同一规则重采样,不能自选点;前序 `1e-8 nC` 的 ΔQ 下限在 Float16 下会变 0,改 **`1e-6 nC`**。<br>**验证**:宿主编译零 warning;`qr_payload_test` **PASS**;用前序 `model-codec.js` / `sweep-codec.js` 交叉解码 **PASS**(flags=4、12 节点、141 点、网格逐点一致、四点电容误差 <0.5%)。⛔ **板上截图与真实触摸未验** —— 板子按用户要求已断电。<br>**自纠 6 条**(SCV1 网格、Float16 下限、探针写死图表、全局声明顺序、单测预期偏离、字体 20 未启用) | ⚠️ 部分可用(代码侧全绿,待上电) | 候选 `skill/read-decoder-before-writing-encoder/` |
| [LLM-071](LLM-071.md) | 2026-10-23 | deepseek-flash | **「全部」页三张卡不显示数据 —— 根因是更新函数存在但无人调用** —— 用户:「**单次测量的全部页面测试完怎么不显示数据**」。<br>**① 先分层**:日志里三腿各有完整 `Z/Cs/Cp/D` ⇒ 测量链正常,断的是显示层。<br>**② 根因**:`UI_Demo_UpdateSingleCapacitance(path=0/1/2)` 只写**单卡** label,且有 `path != g_ui_single_path` 早退;「全部」模式 `g_ui_single_path = 4` ⇒ 三腿全挡。「全部」页的三张卡由 `g_all_result_labels[]` 承载,而唯一写它的 `UI_Demo_UpdateAllCapacitance()` **全工程无调用点**(只有 `_retired/` 旧 stub 调过)。<br>**③ 修法**:`UpdateSingleCapacitance()` 检测「全部」模式后转交 `UpdateAllCapacitance()` —— UI 自己知道当前布局,不让协议层猜。**只改 `src/c_ui/ui/ui_demo.c` 一处**。<br>**④ 部署**:推送 → 板上 `make` → 因红线 #4 **整机软重启**(不 restart 服务),`dmesg` 位流仍恰 1 次;新 `c_ui` md5 `ce35bc8f…`。部署后用户连跑 **2 次**三腿点测,未再复报。<br>**⑤ 同时归档**三腿全量扫压 **180 点**(0.1→100 V,60 点/腿,B 挡 100 Ω,DIV=4):`report/measurements/20261023_three_leg_full_sweep.md` + 逐点 CSV + 原始日志;本轮 **OTR 全 0**,10-15 的 CRSS 高压坏点未复现(只记观测,不称已解决)。<br>⚠️ **修复后的卡片视觉未抓帧确认** —— 用户未复报且继续跑扫压,但无截图 ⇒ 只能写「代码路径已通」,不写「已确认填上」。 | ⚠️ 部分可用 | 候选 `skill/one-writer-but-no-reader/`(**函数有定义、有接口 ≠ 有调用点**;查显示问题先 grep 调用点) |
| [LLM-072](LLM-072.md) | 2026-10-23 | deepseek-flash | **把十颗同型号器件的典型 C-V 曲线做成参考模型，接进结果页做叠加对比** —— 用户：「**桌面上有一个同惠仪器测试zip**」→「**这些所有的数据来自同一型号的十颗mos选取合适的数据来导出一份针对这一型号的典型数据有作为参考**」→「**结果汇总的页面要多一个控件选择器件类型，多一个控件决定是否在图中显示用于曲线对比**」→「**tk9j90t这是器件型号，名字改一下**」。<br>**① 数据**：14 组里用 `1`–`10` 十颗同型号；逐点取**中位数**（拐点处均值会被早跳变样本拖走），按 `(log Vds, log C)` 插值到 **0.1→100 V / 201 点**，Rg 按 `(Vgs, Rg)` 插值 **−3→+3 V / 101 点**；最接近典型的是 **7 号（RMS 1.71%）**，但典型仍定义为中位而不是「挑一颗」。<br>**② Cp 不单独存表**：Cs、D 是两列独立中位；若再对 Cp 取中位，同一点三个数会互相矛盾（实测最大 **2.4%**）⇒ 运行时 `Cp = Cs/(1+D²)`，**按构造自洽**。<br>**③ 界面**：型号选择器走 `RefModel_Count()/Name()`（加型号只加表，不改调用方）；`叠加:开/关` **同一状态**驱动「扫压结果」与「扫压波形」两图的参考线与两处图例；参考线 = **虚线 + 稍暗 + 2 px**，与实测实线同色不同线型，不额外占颜色维度；单点卡在实测值下逐行排参考 Cs/Cp/D，RG 卡排 Rg，偏压未知显示 `标准 --`。<br>**④ 正名**：`TH521` 是仪器、`TK9J90T` 是器件 ⇒ 改显示名/注释/生成器/表头/单测/README；文件名 `th521_*` 保留并写明原因。<br>**⑤ 部署**：7 个文件回读 md5 → 板上 `make -j2` 零 warning → `c_ui` md5 **`f2e169de…`**；本上电周期位流仍**恰 1 次**，FCLK0 **50.0000 MHz**，`0x50=2`。抓帧 `report/measurements/artifacts/20261023_ref_curve_ui.png`（md5 `ba05add2…`）可见选择器、开关、**标准**图例与三条虚线；用户目视「**看着没问题**」。<br>**自纠 5 条**：★ LVGL 的 `dash_width/gap` **只在 hor/ver 两条绘制路径生效**，斜线走 `draw_line_skew` 完全忽略 ⇒ 改按弧长自走虚线；★ 曾断言字体子集覆盖全部 UI 字串，实际**缺「比」「十」** ⇒ 改词（叠加/10 颗/对照）并重筛所有真实 UI 串；★ 改名后按钮溢出（185.75 px > 182 px）⇒ 选择器 202→210、开关 116→110；★ 板上 `fb_dump.py` 已丢而旧 PPM 还在，差点把旧帧当新证据 ⇒ **先删产物再 dump**；★ 板上 `sudo` 本次非免密，用 pty + `sudo -S` 执行，口令未落任何文件。<br>⚠️ **抓帧时外设未上电、无实测数据（`0 点/3`）** ⇒ 只确认了控件与参考线本身；**实测/参考同图对比**与扫压波形页、单点卡参考值均**未抓帧** | ⚠️ 部分可用 | 候选 `skill/vendor-widget-draws-only-some-orientations/` + 扩展 `skill/stale-artifact-silent-reuse/` + 候选 `skill/derived-value-over-independent-median/` |
| [LLM-073](LLM-073.md) | 2026-10-23 | deepseek-flash | **自动换挡改判据 —— 用 `VL/VH` 反推 `\|Z\|`、换挡后核对、一次只走一挡** —— 用户接好回路后试自动换挡:「**刚式了一下在合理的电压下切换应该没有问题电压较小的时候用高档位确实会测出负的**」。★ **诊断**:同一 50 V CRSS 三个挡位 **Re Z = 1.85 k / 17.7 k / 175 k Ω(B/C/D),几乎严格 ∝ R0**,而 **Im Z 基本不动(12.5/11.9/9.7 kΩ)** ⇒ `Z = R0(VH−VL)/VL` 的实部是**测量链失配的镜像**,不是器件;R0 越大越显眼,负 Cs 就是伪影压过真实小信号的产物。**机制未定**(加性串扰/增益误差两个最简模型都只能解释 Re,不编机制)。★ **改动**:①`AdcPair` 增 `vl_vh`/`fresh`;②比例 > 0.95 ⇒ **本点判无读数**(不再往曲线塞负电容),< 0.15 % ⇒ 只警告;③换挡判据改 `\|Z\| ≈ R0·√(1/r²−1)`,不经过 `(A−B)` 相消;④**换挡后核对**:换一挡反推的 `\|Z\|` 须在 1.5 倍内,否则**回退旧挡并关闭本轮自动**;⑤**一次只走一挡**(不跨挡);⑥目标带 `[0.15, 0.90]`(上沿取 0.90 是为保住用户实测"问题不大"的 0.1 V CISS,B 挡比例 0.894);⑦`[adc]` 行增 `VL/VH` 带 ⚠低/⛔高;⑧换腿清核对状态。自动换挡**仍默认关闭**。★ **自我纠错**:2026-10-15 写的「换挡判据本身没错,错的是脚下的 \|Z\|」**只对一半** —— 判据本身也错(它的 \|Z\| 实部 ∝ R0),注释已改写并留原判断追溯。★ **状态**:宿主 `make` 零 warning;改完时板子**已断电** ⇒ **上板行为未验证**,1.5 倍阈值在 Coss 拐点是否成立也无数据。| ⚠️ 部分可用(**代码完成、宿主编译过;上板未验证**) | 候选 `skill/criterion-must-not-use-the-artifact-it-is-meant-to-avoid/` |
| [LLM-074](LLM-074.md) | 2026-10-24 | deepseek-flash | **三标准校准(OPEN / SHORT / 标准板)—— 分式线性误差模型 + 可输入的标准板参数 + 校准页** —— 用户只有两样能测的东西:「**一个是空载下的数据一个是我自己做了一个小板子用标准电容模拟mosfet**」,并追加「**能不能加一个 0 Ω 短接位和一个已知电阻…空载就是直接拔掉**」、标准板参数「**先做成可输入的吧我后面再决定**」。<br>**① 模型选分式线性而非"减偏移"**:`Z_m=(a·Z_d+b)/(c·Z_d+1)`,OPEN 给 `a/c`、SHORT 给 `b`、标准板给 `c=(M_L−M_s)/(Z_L·(M_o−M_L))`;只有 OPEN+SHORT 时**拒绝拟合**,并在标志位里区分"拟合成功"与"少一个标准"。<br>**② 标准板按数据手册拆四腿**(CISS=Cgs+Cgd、COSS=Cds+Cgd、CRSS=Cgd、RG=Rg),每 (腿,挡,频率) 单独拟合;用户输入的是**电容/电阻值**,阻抗在运行时现算 —— 拓扑可能还要改,输"几个电容接在哪"比输阻抗歧义小。<br>**③ 采集状态机** 4 腿×4 挡=16 点/标准,挡位当拍改、继电器只在换腿时切 + 一个驻留;**用 `0x18` 回读判断挡位**,不符则标坏不存;与扫压/单点/继电器/挡位/偏压双向互锁。<br>**纠错轨迹**:(a) packed 结构按"60/132 B"断言,实际 **64/100 B**(点 = 4 + 12×8,不是 16 个 double);(b) `LV_USE_BAR 0` ⇒ `lv_bar_*` 不可用,改"轨道+填充 panel 按比例改宽",不为一行进度条全局开控件;(c) 新页面缺 **44 个中文字**,字体子集缺字是**静默空白**,重新生成 705 字符并逐字核对;(d) `EndStandard()` 返回**全频段**累计,日志却标成"本频率",改为两个数都打;(e) 宿主单测首版 `Apply()` 全 false —— **默认未启用**,测试改为显式 `Init()` 并把"默认关"钉成断言;(f) 单测的 CRSS 腿反演不回来,**根因是测试数据撞上 `CAL_OPEN_MIN_RATIO=5` 守卫**(该腿开路/负载只有 3.4 倍),**代码是对的、测试是错的** ⇒ 主用例换参数,另**单开守卫用例**钉住"只有该腿掉精度";(g) 测试存盘会写板上真文件 ⇒ 加 `CV_CAL_PATH` 覆盖,只给单测用。<br>**验证**:宿主 `make -j2` 零告警;`tests/test_calibration.c` **ALL OK**(完整 Möbius / 仿射 / OPEN∞ / 无 SHORT 四路各 16/16,反演相对误差 <1e-6;OPEN+SHORT 0 拟合;存盘重载一致);字体无缺字。⚠️ **上板进展**:第 1 次上电抓帧发现校准页数值显示成 `f pF` —— 根因 `LV_SPRINTF_USE_FLOAT=0`,LVGL 的 `lv_snprintf()` 不支持 `%f`(宿主单测覆盖不到显示层);改整数拼接后**软重启**复验,数值正常、键盘输入 `Cgs=51` 回显并落盘(4864 B,magic/版本/点数逐项核对)。⛔ **采集状态机(OPEN/SHORT/LOAD)仍未验** —— 用户要先断电重接外设 | ⚠️ 部分可用(**数学与存盘宿主单测通过;界面/输入/落盘已上板;采集状态机未验**) | 新候选 `skill/calibrate-with-standards-not-offsets/` + `skill/test-data-must-respect-guards/` |

| [LLM-075](LLM-075.md) | 2026-10-24 | deepseek-flash | **仅开路校准 —— 平行杂散校正 + 四挡一致性闸;并归因「校准开着扫压全无效」** —— 用户不敢短接(源侧限流无记录),问「**能不能只做其中一两个校准**」;标准板未到,先做 OPEN 并问「**校准完比原来好多少**」。★ **部分校准的选择**:OPEN+LOAD 是稳健组合(LOAD 是唯一能钉住增益 `c` 的一条),SHORT 可省;OPEN-only 只能做**平行杂散校正** `a=1,b=0,c=1/M_o`,不识别增益与串联项,标准板到位后自动升级。★ **实测把 OPEN 数据判死**:同一开路在四挡上(CISS)`5215 / 56041 / 505922 / 803522`,相差 **150–1500 倍**,随 `R0` 增长 ⇒ 不是夹具阻抗,是 **`VL/VH ≈ 0.19 %` ≈ 1 ADC LSB 的通道地板**;能看见的 D 挡折算仅 0.15–0.52 pF ⇒ **推翻 `LLM-073` 的「夹具并联 7–14 pF」前提**。★ **新增闸**:`CAL_OPEN_MAX_GEAR_SPREAD = 3`(四挡必须一致)、`CAL_OPEN_ONLY_MIN_Z = 10 Ω`、`CAL_OPEN_ONLY_MIN_DEN = 0.1`(器件导纳 <10 % 时拒绝应用);`Init()` 载入后 `refit_all()`,旧文件里不可信的点会在下次启动被清。★ **「全无效」归因**:那轮 60 点全在 **D 挡**,`判无效 0 点`、`z_valid=1`;但校准前 Z = `14569+1303j` 已是**正虚部**(感抗),`Cs` 为负,而 UI 的有效性判据是 **`Cs > 0`**(`ui_demo.c:8084`)⇒ 是该判据 + D 挡相位伪影,**不是校准翻转了符号**(修正只让虚部多偏 0.3 %)。同腿 B 挡 `149−91j`(Cs≈1749 pF)正常。★ **纠错**:①「夹具 7–14 pF」被实测证伪;②"降低校准偏压可保护短接"**不对** —— 偏压是直流,交流激励幅度固定;③OPEN-only 对 CISS 点的改善只有 **0.08 %**,如实说"几乎没改善";④"全无效"第一假设是反演分母变号,**错了**(`\|Z_m\|/\|M_o\|` 仅 1.8 %),靠日志两列改判。★ **验证**:宿主单测 **ALL OK**(OPEN-only 反演 / 10 % 守卫 / 四挡不一致 0 拟合);板上 OPEN **16/16** 存盘,文件 16 点 flags **0x89**;⛔ 新闸**未在板上生效**(进程仍是旧二进制,下一次启动才加载) | ⚠️ 部分可用(**代码与宿主单测完成;OPEN 采集走通但数据被证明不可用;新闸未上板**) | 候选 `skill/standard-data-must-agree-across-ranges/` + `skill/validity-is-a-ui-rule-too/` |
| [LLM-076](LLM-076.md) | 2026-10-24 | deepseek-flash | **界面整体 180° 翻转 —— LVGL 旋转与 `direct_mode` 不兼容,改分块渲染;触摸两条路径分开翻** —— 用户:「**能不能把整个页面翻转180度**」(面板翻装)。★ **关键判断**:`direct_mode` 下 `draw_ctx->buf_area` 是**整屏**,而 flush **每个失效区各调一次** ⇒ `draw_buf_rotate_180()` 会把 1.2 MB 非缓存帧缓存翻 N 次(偶数次等于没翻),**必须改分块渲染**:64 行 cached RAM 缓冲,LVGL 每块原地翻并**把矩形坐标一并镜像**,flush 只按行拷进后台。★ **帧缓存从此是面板朝向** ⇒ `sync_invalid_areas()` 必须先 `mirror_area()` 再拷,否则局部刷新退回两帧前。★ **触摸两条路径**:单指走 `read_cb`,**LVGL 8.2 `lv_indev.c:332` 自己按 `rotated` 翻**(`lv_hal_disp.h` 那句 "Does not update coordinates for you!" 是过时注释),再翻一次就落到对角;多指捏合走 `ui_demo.c` 的 `LV_PortIndev_GetTouchState()`,**不经过 LVGL** ⇒ 在 `pynq_indev.c` 里单独镜像。两边共用 `CV_PANEL_ROTATE_180`。★ **验证**:宿主/板上 `make` 零 warning;软重启(位流恰 1 次,FCLK0 50.0000);日志 `分块=64 行 旋转=180°`;`fb_dump` 抓到的帧是 180° 倒置图;`fb_cmp.py` **5/6 次两块帧缓存完全相同**(1 次差 114 px 于状态栏 = 采到帧中);重启后用户**通过触摸完成一次 0.1→100 V 全扫压** ⇒ 单指路径可用;CPU 2.1%。⛔ **双指捏合未验** | ✅ 可用(**显示方向与单指触摸已上板验证;双指捏合坐标镜像未实测**) | 候选 `skill/rotation-vs-direct-mode/` + `skill/two-consumers-one-coordinate-space/` |
| [LLM-077](LLM-077.md) | 2026-10-24 | deepseek-flash | **曲线开头的「陡升」= 换腿后第 1 点采在高压没落稳时;顺带查出 ADC 数据线全 0** —— 用户:「**为什么我测曲线下面两条的开头总是有个陡峭的上升是切换的过程有什么问题吗**」。★ **现象**(三腿连扫日志):COSS/CRSS 第 1 点 Cs **1872 / 573 pF**,第 2 点 **3393 / 1700 pF**;CISS 第 1 点正常。★ **两条对照夹死原因**:①单腿连扫第 1 点正常 ⇒ **不是继电器切换本身**;②「0.1 V→0.1 V、1 点/腿」三腿全正常 ⇒ 也不是换腿动作,而是**上一腿终点停在 100 V**,300 ms 不够高压级落回 0.1 V。★ **改法有标定**:老代码第 2 点是 0.1 V 下发后 **600 ms** 采的且一直正确 ⇒ 给换腿后第 1 点也加一个驻留(`s_leg_settle_pending`),三腿一轮 +0.9 s。★ **顺带修既有缺陷**:`sweep_finish()` 换腿前**先 `bias_apply(0)` 再切继电器** —— 原来在上一腿终点(通常 100 V)热切换,触点烧蚀且高压泄放晚 40–70 ms。⛔ **效果未验**:部署时发现 ADC **`adc_d` 恒 0x000**(原始码恒 2048),扫 DIV 2/4/13 × 全部合法 PH_OFF、偏压 0→100 V、软重启(位流恰 1 次)后**全不变**,而 `0x88=0x104`、`0x84=0x1`、`0x90=0x3` ⇒ 判到外设侧。★ **用户当场更正:「是我没接外设」** ⇒ **不是故障,是未接**;证据只支持"外设侧没信号"。板子随后按用户要求关停(偏压 0 V / 继电器 ALL_OFF 后 `shutdown`)。✅ 已验:三条 `[proto] 换腿后第一点` 日志均出现;`adc_dc_probe.py` / `adc_phase_probe.py` 留档。★ **纠错**:①`bias_settle_test.py` 全 2048 时先怀疑脚本、改了两处仍全 2048 —— **实际是外设没接**,全常数读数应先怀疑量具;②改法第一版注释只写"300 ms 不够",复查日志后改成"600 ms 是实测证明够用的线";③**把"未接外设"写成"硬件故障"** —— 先问外设接没接,再谈万用表。★ **上板复验(同日,外设接好)**:新 `c_ui` `6ca7dd19…`、位流恰 1 次;三腿全扫 **180/180 点、OTR 全 0**、三条 `[proto] 换腿后第一点` 齐全;**COSS 首/次 = 3568.80 / 3559.53 pF(比值 1.003,修复前 0.552)、CRSS = 1749.57 / 1795.66 pF(0.974,修复前 0.337)**;用户「**现在好多了**」。⚠️ **撤压改动无独立日志证据**(`bias_apply()` 不打日志)⇒ 只写"代码路径已生效" | ✅ 可用(**首点沉降已上板验证;撤压项无独立读数**) | 候选 `skill/control-by-varying-the-suspected-variable/` |
| [LLM-081](LLM-081.md) | 2026-10-29 | deepseek-flash | **单点 CRSS 为负 —— 扫压与单点取的不是同一套系数** —— 用户问「扫压倒是可以了我现在单点测量怎么会crss是负的,扫压都没问题啊」。★ **定位(数值反证,不靠读代码)**:把日志里的**原始** Z 分别套两套系数 —— 单点 CRSS 原始 `160.55−92.33j` 套 **CRSS-B** → `137.7+267.4j`(Cs **−595 pF**,与屏幕逐位一致);扫压 CRSS 原始 `157.53−95.23j` 套 **RG-B** → `161.14−107.70j`(Cs **+1477.7 pF**,也一致)。⇒ 扫压三腿全在用 **RG 腿**的系数,单点用自己的。★ **代码根因**:`sweep_begin_leg()` **没有 `s_pair_path = leg;`**,"先测 RG 再三路扫压"结束时它停在 3(RG)。★ **为什么不是"补一行"**:CRSS 腿自己的拟合退化(B 挡 10 pF/104 pF 对 100 Ω,`|b| = 3.3·R0` 且虚部为正,非物理)。★ **方案 A(用户选定)**:校准应用改**通道级** —— 按 (挡位, 频率) 从四条腿拟合里选 `|b|/R0` **最小**且 ≤ **0.25** 的一条;★ **两道物性守卫**:① `|b|/R0 ≤ 0.25` 拒掉 A 挡(0.564);② 🆕 `|a−1| ≤ 0.50` 拒掉 C/D 挡(C `1.054+0.948j`、D `1.482+9.471j`,`|a| = 1.42 / 9.6`,第一版只查 b 时它们因 `R0` 大而"过关",会把噪声放大 10 倍还标"已校准")⇒ 用户问「**其他档位不合理就先不应用校准呗有关系吗**」,答:没关系,代码现已强制 **只有 B 挡应用校准**,实校准文件复核 A/B/C/D = `0/1/0/0`。★ **纠错**:①阈值先设 0.10,`test_calibration` **25 处断言失败**(合成 `b={15,−4}` 的 0.155 被拒)⇒ 放到 **0.25**,仍拒全部真实退化点(CRSS-B 3.28 / C 0.66 / D 0.542);②守卫用例语义要跟着变(通道级后 RG 腿也该准);③**板上 make 撒谎** —— RTC 慢 11 天,`.d` 时间戳在未来,`make` 打印重编+重链但 **md5 不变、`nm` 无新符号** ⇒ 必须 `rm -f build/app/*.o build/app/*.d c_ui` 再编,判据是产物里的符号/字符串;④`strings` 只出 ASCII,中文检查恒 0,改用 `grep -a`;⑤把 COSS 4469→3631 误判成"重复性差",用户纠正「**前面忘开偏压**」—— 0 V 时 Coss 最大,同对照里 CISS/CRSS 只差 4.7 %/0.6 %,自洽。★ **上板验证**:`c_ui` `5f9c5dd…`、位流 `c92c7ed5…` **恰加载 1 次**、FCLK0 50.0000 MHz、1 MHz/32.5 MSPS、挡位 B;单点 CRSS **1530/1531/1538/1538 pF**(由 −595 转正),扫压首点 **1501.5 pF**,差 **2.5 %**;180/180 点、OTR 全 0。★ **附带**:扫压中途被 10-15 残留的 `/home/xilinx/sweep_auto_gear` 触发一次 B→C 换挡(1/180 点,值连续),已删标志文件;**AI 基线用本轮数据重建**(v2 CSV `39b15c26…`,自评 0.00、留一法 ≤ 8.99、注入 ×1.10 1709/712/53),单测全过,新二进制 `2a0c7ccb…`(加第二道守卫后 `44c1ff2b…`,4 处合成 `a={2.0,0.3}` 改成 `{1.05,0.02}` + 另加专用拒绝用例)**均下个上电周期生效**。★ **上板复验(重启后)**:`/proc/906/exe` md5 **`44c1ff2b…`** ⇒ 新守卫真在跑;A 挡点测日志 `(未校准)`(3485.7/3758.9/1589.2 pF),B 挡 `(已校准)`;B 点测 CISS 3311.6/3328.7、COSS 3609.4/3598.5、CRSS **1539.3/1539.8 pF**、Rg 3.113 Ω;扫压 60+180 点、**250/250 条 OTR 0**、无自动换挡;`[ai] 0.1/0.1/0.7 -> 正常` ⇒ 新基线生效。⚠️ C/D 挡本轮**没点**,宿主证据 `available=0/1/0/0`,不算上板验证。⚠️ **遗留**:COSS 相对 TH521 系统性偏低 **13 %**(改动前后都在,点测与扫压彼此一致),未查。 | ⚠️ 部分可用(**主问题已修并上板验证;COSS 偏低是老问题,未查**) | 候选 `skill/numeric-reproduction-of-a-log/` + `skill/build-artifact-is-the-evidence/` |
| [LLM-080](LLM-080.md) | 2026-10-27 | deepseek-flash | **两负载校准:不靠开路也不靠短接也能标定** —— 用户连问「一定要完成短路校准才能进行校准吗」「一定要短接吗没别的办法吗」「我有两个负载行不行」,最后拍板「**关闭系统吧,改成支持两负载的模式,一样手动输入然后我再上电**」。★ **动机**:本夹具 OPEN 四挡相差 154–1545 倍(低挡是通道地板),被 `CAL_OPEN_MAX_GEAR_SPREAD = 3.0` 判死;只剩一块标准板时只有一个方程,仿射的 a、b 定不下来(10-25 那次「采集 16/16、拟合 0/16」的真正原因)。★ **方法**:误差模型 `Z_m = (a·Z_d + b)/(c·Z_d + 1)`,两块已知板给两对 `(Z_L, M_L)` ⇒ `a = (M_2−M_1)/(Z_2−Z_1)`、`b = M_1 − a·Z_1`、`c = 0`。**间隔守卫** `CAL_LOAD_SEP_MIN`:两板阻抗差 ≥ 5 %·两者模长的较大者—— 两块标称太近时噪声按 1/间隔 放大,宁可判"不拟合"。★ **拟合优先级**:OPEN+SHORT+LOAD → SHORT+LOAD → **LOAD1+LOAD2(新增)** → OPEN+LOAD → 仅 OPEN;短接是精确 0 Ω、比两个数据手册标称值可信,故排在两负载之前。★ **落地**:`CalStandard` 加 `CAL_STD_LOAD2=3`;`CalBoard` 加 4 个 double;状态位扩到 16 位(`CAL_F_LOAD2 0x100` / `CAL_F_WEAK_LOAD2 0x200`,原 8 位不动);存盘 **v1 → v2**(头 64→96 B、点 100→120 B),旧文件被版本号拒掉 ⇒ 板上须重采;界面校准页 10 字段 / 3 行 × 4 列、采集按钮 3→4(动作码 343)、覆盖格加 `2` 记号、只采一块板时提示「再采另一块标准板即可标定」。★ **自我纠错**:①按 2 行 × 5 列 × 128 px 排,`lv_font_get_glyph_dsc` 实量 "1000000.000 pF" = 144 px > 可用 108 px ⇒ 改 700 px / 3 行 × 4 列;②量字宽首版用了 LVGL 8.2 **没有**的 `lv_txt_utf8_next()`,链接期 undefined reference ⇒ 自写 UTF-8 解码;③覆盖汇总 "拟合 16/16 · 1 MHz · 全部 48/48" = 311 px > 260 ⇒ 去掉频率段;④"1000000.000 pF" 在 141 px 标签里仍超 3 px ⇒ 按钮显示改 2 位小数(键盘仍 3 位);⑤行高 50 时两行 = 48 px 只剩 2 px 余量 ⇒ 54;⑥`-Wformat-truncation`:66 B 写进 57 B ⇒ `s_cal_text` 80→256。★ **验证**:宿主 `make -j4` 零告警;`test_calibration` **ALL OK**(新增"两板相隔 50 %+ → 16/16、反演误差 < 1e-6"与"只差 1 % → 0/16");`test_anomaly_detect` / `test_ref_model` / `qr_payload_test` 全过。★ **上板(2026-10-27,仅界面)**:只上电主板 → 位流加载恰 1 次 → `fb_dump.py` 从 DDR 抓帧核对。首页五入口/顶栏正常,触摸 raw (160,60) → 逻辑 (863,539) 正确,校准页 10 格 / 4 按钮 / 4×4 覆盖表**无重叠**;`[cal]` 提示旧 v1 文件被拒(符合预期)。★ **抓帧查出 4 处缺陷并已改源码**:①腿名标签 42 px 裁字(CISS 43 / COSS 52 / CRSS 50)⇒ 54 px、四列起点 52→62;②`OSL2*` = 58 px > 格宽 50 ⇒ 单元格字距 −3;③顶栏状态 323 px > 318 折行 ⇒ 改 `状态:就绪(Cs/Cp/D 未标定)` = 242 px;④首页仍写「标准板**三**标准标定」⇒ 「两块标准板」(216 px,候选里的「和」不在字库)。★ **第二次上电复验(同日)**:用户说「**启动**」后才拉起服务(本次上电仍只加载 1 次位流)→ 抓帧确认 **4 处修正全部生效**(顶栏单行完整、首页格「两块标准板」、覆盖表 CISS/COSS/CRSS/RG 不再裁字、整页无重叠)。⛔ **仍未验**:两块标准板参数录入与 LOAD1+LOAD2 两点仿射(需接标准板)。 | ⚠️ 部分可用(**代码完成、宿主全过、界面已上板核对;两负载拟合与 4 处修正待下次上电**) | 候选 `skill/guard-the-condition-number-in-storage/` + `skill/measure-text-before-layout/` |
| [LLM-079](LLM-079.md) | 2026-10-24 | deepseek-flash | **无监督异常检测直接做进界面(没有故障样本)** —— 用户先问「ai异常检测好做吗我们应该可以尝试复现一个吧」,随后拍板「**直接做进界面吧**」,跳过离线脚本阶段。★ **方法**:每条腿 4 个与拟合无关的特征(0.3 V 电平 / 100 V 电平 / V50 半降电压 / V75-V25 宽度)→ 马氏距离;协方差来自 **10 颗** TK9J90T(收缩 reg=0.1),中心 = 本机 2026-10-24 健康基线(平台偏移被吸收)。★ **阈值**:健康留一法最大 **8.99 / 4.01 / 3.94**,两两最大 ≈9,同配置复跑 10-23 得 **5.40 / 0.79 / 0.58** ⇒ 阈值 **25**,`>=100` 判异常。★ **注入检验**:电平 ×1.10 → **1709 / 712 / 53**,电压轴 ×1.08 → **135 / 51 / 41**,拐点 ×1.3 → **6096 / 2947 / 147**;10-15 的 65 MSPS 数据得 64/46/57 ⇒ 模型只在当前固定配置下声明有效。★ **落地**:结果页第 6 个页签「AI 检测」,扫压跑完自动评分,中止/重跑清空;新增 `tools/fit_anomaly_model.py`、`app/anomaly_model_data.h`、`app/anomaly_detect.{c,h}`、`tests/test_anomaly_detect.c`。★ **自我纠错**:①C 版把下降曲线判成 `hi > lo`,所有健康曲线被拒,宿主单测首次运行即暴露;②生成器误收 `值钱/` 的 3 颗异批器件(留一法涨到 13.2),按归档 README 恢复为 10 颗;③文案有 13 个字不在点阵字库(诊/疑/评/估/半/拐/宽/阈/马/氏/距/足/≥),改用字库内等价文案并脚本逐字核对;④发现结果页参考标准值 4 处 `%.3f` 是坏的(`LV_SPRINTF_USE_FLOAT=0`),改成先格式化再拼。★ **验证**:宿主单测 **ALL OK**、`make -j2` 零告警、C 与 Python 分数逐位一致;✅ **板上已验(2026-10-25)**:三腿扫压 180/180 点,CISS **0.1** / COSS **0.0** / CRSS **5.2**,全判「正常」;界面抓帧无缺字。 | ✅ 直接可用(**代码完成、宿主全过、板上三腿全判正常**) | 候选 `skill/unsupervised-baseline-absorbs-platform-offset/` |
| [LLM-078](LLM-078.md) | 2026-10-24 | deepseek-flash | **拐点提前是「log 扫描 + 少点 + 插值」造成的吗?—— 不是** —— 用户先说「我突然决定是不是因为我们是log扫描而且点数比较小曲线主要是插值出来的」,再追问「你确定水平差不是这个原因导致的吗,我主要是想解决我们曲线的拐点在实测前面的问题」。★ **判据**:改用与拟合无关的**半降电压 V50**(电容从 1 V 平台降到 50 V 平台、跨过对数中值);分别算参考曲线、**参考按我们 60 点网格采样后再 log-log 线插**、实测。★ **结果**:COSS 参考 10.06 V → 采样+插值 10.04 V → 实测 9.23 V;CRSS 10.38 → 10.28 → 9.11 ⇒ **显示只动 0.02–0.10 V,实测早 0.83–1.17 V,差一个数量级**。★ **差异性质**:COSS 实测与参考做「电压平移 + 竖直偏置」拟合,2026-10-23 / 10-24 两轮分别为 **+7.6 % / +7.9 %**,平移后残差 2.1–2.2 %(仅竖直对齐 14.7 %)⇒ **整条曲线在电压轴上左移约 8 %**;9.62 V 实测 384.9 pF,参考同电压 916.3 pF、参考 10.38 V 412.5 pF 对上。CRSS 平移后残差 27 %,形状本身不符,不能套同一结论;CISS 平坦、无约束。★ **纠错**:①先用「最陡线段」估拐点得到「网格让拐点早 0.6 V」,**换成 V50 后只有 0.02 V** ⇒ 那是估计器偏差,不是网格效应,**判据选择决定结论**;②窗段拟合看似「非均匀平移」,但平坦段平移与竖直偏置简并、不可辨识;③已知 DAC 模块误差(19.777×frac − 10.057)会让拐点偏晚,**方向相反**,不能解释。★ **根因未定**:器件拐点 vs 偏压轴标度;`×10` 高压级从未端到端实测,需万用表 1/10/50/100 V 四点。用户拍板:「**如果是测量误差就算了,只要不是显示导致的就行**」。★ **同轮**:4 份自绘板卡设计源文件(立创EDA专业版)入库 `hardware/`,文件名改英文,md5 4/4 与源一致,`hardware/README.md` 记用途/打开方式/校验值;顶层 README 同步目录表与「二进制」说明。 | ✅ 直接可用(**显示嫌疑已排除;8 % 左移根因未定,按用户决定不再追;硬件文件已入库**) | 候选 `skill/position-metric-independent-of-rendering/` |

> 🆕 **2026-09-22 起,记录分两类来源**(写入纪律见 `LLM-020`) ——
> **模型工作区** `~/Desktop/qrs_2026_fpga/`(本目录所在)与
> **交付仓库** `~/Desktop/qrs_fpga_2026/`(推送到 Gitee)是**两个独立目录**。
> 交付仓库由**用户把关**,模型须经点名才可写入;本目录的记录**何时同步进交付仓库由用户决定**。
>
> 🆕 **2026-10-05(`LLM-041`)起已在交付仓库落地** —— 用户拍板把 LLM 记录**从 `report/` 拆出**
> 为交付仓库的**顶层目录 `llm-log/`**(`report/` 留给报告正文与参考文献)。
> 本目录(工作区 `report/llm-log/`)是**撰写侧**,交付仓库 `llm-log/` 是**发布侧**,
> 新记录写完需**由用户执行一次拷贝**才会进仓库(§6.0.1 单向下游)。
> ⚠️ 已在交付仓库内的记录,其正文里出现的 `report/measurements/`、`report/llm-log/` 等路径
> 是**当时**的位置,**按原样保留不改写**(改写存档等于篡改记录)。

---

## 统计

| 指标 | 数值 |
|---|---|
| 记录总数 | **80**(LLM-001 ~ LLM-080;2026-10-27 **脚本逐行实测**:表内 `grep -c '^\| \[LLM-'` = **80**、`ls LLM-*.md` = **80**、编号无缺号)。🔴 本行此前长期停在 `60` —— 后续只加了表行没改统计,**别再用旧数字**。 |
| ✅ 直接可用 | **28**(2026-10-27 按结果列脚本计数:✅28 + ⚠️51 + ❌0 + 无标记 1 = 80)。 |
| ⚠️ 部分可用 | **51**(2026-10-27 脚本计数。`LLM-073`/`LLM-075`/`LLM-080` 属此类 —— **代码完成、宿主编译过,上板行为未全验**;`LLM-079` 已于 10-25 上板转 ✅)。 |
| ❌ 不可用 | 0 |
| 无标准标记 | **1**(`LLM-037` —— 该行**结论列确实没写标准标记**,不是统计假象;就是「一次按压得 **δ 的绝对值 ≤ 11 px**(原文用的是绝对值竖线)」那一轮,已逐行核实) |
| 已沉淀为 Skill | **12** ✅(`ls -d skill/*/` 实测 = **12**;2026-10-03 由 `LLM-039` 一次性落地 10 项,2026-10-08 增 `silent-capacity-gates/` 得 11,2026-10-10 增 `count-both-sides-of-interface/` 得 12) |
| 已提名候选 Skill | **140**(其中 **12 项已落地**;`grep -oE 'skill/[a-z0-9-]+/' INDEX.md \| sort -u \| wc -l` 2026-10-23 **实测 = 140**)。 |

> 🔴 **统计命令本身有个静默陷阱(2026-10-06 发现,`LLM-045`)**
>
> 上面那个按结果列点数的命令 **`awk -F'\|' … {print $6}`** 对**正文里含字面 `\|` 的行会切错列**,
> 于是那些行的"结果列"读到的是描述文字的碎片、**不落进任何一类,被静默漏计**。
> 本次实测:45 行里 **3 行**中招(`LLM-037` / `LLM-043`,以及 `LLM-045` 自己第一版),
> 修正后剩 2 行(`037`/`043`)需人工判定。
>
> ⛔ **不要再用 `$6` 直接点数**。用这个(**先在副本上把 `\|` 换成一个不冲突的占位符再切列**,因此不再切错):

```bash
# ✅/⚠️/❌ 计数 —— 2026-10-07 修正版:切列【之前】先把转义竖线换掉
# 这一步才是真正的修法:原版只是"把异常行显式列出来",没解决切错列本身。
awk '{ line=$0; gsub(/\\\|/,"\001",line);          # ← 关键:先中和转义竖线
       n=split(line,f,"|");
       if (f[2] ~ /^ \[LLM-/) { c=f[6]; gsub(/ /,"",c);
         if      (c ~ /^✅/)  a++;
         else if (c ~ /^⚠️/) b++;
         else if (c ~ /^❌/) d++;
         else { e++; printf "无标记: %s | 结论列起首 = %.60s\n", f[2], c } } }
END { printf "\n✅=%d ⚠️=%d ❌=%d 无标记=%d 合计=%d\n", a,b,d,e,a+b+d+e }' INDEX.md
```

**2026-10-27 实测输出**(本表数字的唯一来源;末次在加入 `LLM-080` 后重跑):
```
无标记:  [LLM-037](LLM-037.md)  | 结论列起首 =

✅ 直接可用 = 28
⚠️ 部分可用 = 51
❌ 不可用   = 0
无标记     = 1
合计       = 80
```
> 📌 **2026-10-27 加入 `LLM-080`(两负载校准)后重跑**,⚠️ 由 50 升到 51、合计 79 → 80
> (`LLM-080` 为 ⚠️ —— 宿主单测全过,板上两负载拟合**尚未验收**,用户将重新上电)。
> 上一版:2026-10-25 上板验收后把 `LLM-079` 由 ⚠️ 改为 ✅ 并重跑,与 `ls LLM-*.md` = 79、`grep -c '^| \[LLM-'` = 79 三处自洽
> (三腿扫压 180/180 点、三条 `[ai]` 全「正常」、界面抓帧正常)。
> 上一版(加入 `LLM-079` 时)为 27 / 51 / 0 / 1 = 79;再上一版(加入 `LLM-078`)为 27 / 50 / 0 / 1 = 78。
> 上一版在把 `LLM-077` 由 ⚠️ 改为 ✅(首点已上板复验)后为 26 / 50 / 0 / 1 = 77。
> 上一版这块在加 `LLM-077` 行后为 `25 / 51 / 0 / 1 = 77`;再上一版(加 `LLM-076`)为 `25 / 50 / 0 / 1 = 76`;再上一版(加 `LLM-075`)为 `24 / 50 / 0 / 1 = 75`;再上一版(加 `LLM-074`)为 `24 / 49 / 0 / 1 = 74`,
> 而**统计表上方两行当时仍停在 73/25/47** —— 又一次"只改行、忘改汇总",已在那一轮对齐。
> 📌 上一轮同时修了一处**记账漂移**:`LLM-073` 行里的 `|Z|` 没转义,把表格从第 4 列起
> 整体右移一列,计数脚本因此把它读成"无标记"(统计一度出现 ⚠️=48 / 无标记=2)。
> 转义后与 `ls LLM-*.md | wc -l`、`grep -c '^| \[LLM-'` 三处自洽(均为 74)。
> 教训与前面几次同类:**含 `|` 的正文一律写成 `\|`**,加行后立刻重跑计数,不要手算累加。
> 📌 **上一版这块记的是「合计 = 56」,而当时表里已有 58 行** —— 即 `LLM-057`/`LLM-058`
> 加进去时**这一块没有重跑**。本轮把 `LLM-059` 加进来时一并重跑,
> 得到 `22/36/0/1 = 59`,与 `ls LLM-*.md | wc -l` = 59、`grep -c '^| \[LLM-'` = 59 三处自洽。
> ⚠️ **这是一次真实的记账漂移**,记在这里而不是悄悄改掉 ——
> 它和 `LLM-051` 那次"手写累加与实测对不上"是同一类毛病:**增量更新时容易只改行、忘了改汇总**。
> 📌 **`LLM-059` 那行同样零转义竖线**(正文里含 `|` 的地方一律改写成 `OR` 或自然语言 ——
> 本次正文里就出现了 `open("/dev/mem", O_RDWR OR O_SYNC)` 这类写法,已按规则改写),
> 重跑后**落在 ✅ 一类**;`无标记` 仍只有 `LLM-037` 一行 ⇒ **规则连续第五轮生效**。
> 📌 **`LLM-056` 那行同样零转义竖线**,
> 重跑后**落在 ⚠️ 一类**;⇒ **规则连续第四轮生效**。
> 📌 **为什么 `LLM-056` 是 ⚠️ 而不是 ✅**:RTL / XDC / BD / 综合 / 实现 / 位流**全部完成、
> 且每一步都有回读证据**(引脚 58 / 悬空 0 / net 成员逐字一致 / 地址段 / wrapper 文件内容 /
> WNS +1.490 / 失败端点 0),但 **① 位流尚未上板 ② 继电器驱动板极性未证实**(`ACTIVE_LOW = 0`
> 是继承前序 STM32 的约定,不是手册)⇒ **"造出来了"与"验过了"仍是两件事**,按后者记。
> 📌 **`LLM-055` 那行同样零转义竖线**(正文里含 `|` 的地方一律改写成 `OR` 或自然语言),
> 重跑后**落在 ✅ 一类**;`无标记` 仍只有 `LLM-037` 一行 ⇒ **规则连续第三轮生效**。
> ⚠️ **但本次多了一句口径提醒**:`LLM-055` 在**记录正文里的三选一**打的是 **`☑ ⚠️ 部分可用`**
> (因为 10 MHz 那一档在交付时就是"物理上做不到"的),而 **`INDEX.md` 行内的结果列写 `✅`**
> (因为**本次验收的对象**——三档窗长与掩码——**全部验中**)。
> 📌 **两处不矛盾但口径不同**:"记录自身的完成度"与"本次验收判据的达成度"是两件事。
> **知道这一条,再看到 `✅=20 / ⚠️=34` 才不会以为统计脚本错了。**
> 📌 **`LLM-053` 那行是"切列陷阱"的一次实测检验**:该行正文里含 `cfg = (div & 0xFF) OR …`
> 这类描述,我**刻意不写转义竖线**、改用 `OR` 与自然语言,重跑统计后 `LLM-053` **落在 ⚠️ 一类**
> (而不是像 `LLM-052` 那行当初那样被判成"无标记")⇒ 规则**当场生效、可观测**。
> 📌 **`LLM-054` 那行同理**(正文里含 `PH_OFF -2 → -1`、`x 799..896` 等大量描述,
> 同样是零转义竖线),重跑后**落在 ✅ 一类**;`无标记` 仍只有 `LLM-037` 一行 ⇒ 规则**连续第二轮生效**。

> 📌 **这次把 2026-10-06 那条"修正后剩 2 行需人工判定"收口了**:`LLM-037` / `LLM-043` 三行
> 都不是"缺标记",而是**被切错列**。真正缺标记的只有 **`LLM-037` 一行**(它的结论列里
> **确实没写**标准标记)。⇒ **"统计脚本报异常"和"被统计的东西真异常"是两件事**,
> 与 `skill/verify-your-own-checker-first/`(**先证明检查器本身对,再指责被检查对象**)完全同型
> —— 而这条 skill 的候选名,**正是上一轮从"我自己写的正则把 12 行正确约束报成不符"里提炼出来的**。
> 📌 同一族在本项目已出现 **4 次**(比值判据 / `led.xdc` 正则 / `bias_sweep.py` 恒真表达式 / 本次切列)。
>
> ⚠️ **但本表仍有 1 处未收口**:手写清单里的 **`LLM-046`「已改判 ✅」**与**表内该行仍写 `⚠️`** 打架,
> 没有回 `LLM-047` 逐项核之前**不擅自改判** —— 已挂在 ✅ 行里待核。

> 📌 **这解释了 INDEX 里长期挂着的「余 1 项差额未查」** —— 差额不是笔误,
> 而是**含竖线的行被静默漏计**。与 `LLM-043` 的 `strings | grep` 返回 0 同型:
> **工具自报的结果也可能是失效判据**(`skill/stale-artifact-silent-reuse` 第 7 形态)。
>
> 📌 **写记录时的纪律**:描述正文里**避免字面竖线**。要表达管道,改成文字
> (例如"把密码 `echo` 进 `sudo -S`"),不要写 `\|` —— markdown 里它转义了,
> awk 里没有。

> 🟢 **M6 缺口已闭合一半(2026-10-03,`LLM-039`)** —— 索引里提名过 **77** 个候选 skill 名(2026-10-05 实测)
> (命令:`grep -oE 'skill/[a-z0-9-]+/' INDEX.md | sort -u | wc -l`),
> **本轮一次落地 10 项**(命令:`find skill -name 'skill.md' | wc -l` → **10**),
> 合计 `skill/` 下 **11 个文件、1634 行**,五要素 grep **10/10 齐全**。
> 落地清单:3 项**通用 PYNQ**(`pynq-overlay-bringup` / `pynq-register-map-pattern` /
> `pynq-ddr-framebuffer-display`,够得着 **B1**)+ 7 项项目技能
> (`verilog-review-checklist` / `spi-tb-patterns` / `measurement-instrument-can-be-wrong` /
> `stale-artifact-silent-reuse` / `rtl-constant-experiment-must-verify-netlist` /
> `use-layout-as-ruler` / `gbk-example-porting`)。**每一项的「来源」都回指具体记录编号,无一条泛化。**
> ⚠️ **剩余 67 个候选名未落地**(77 − 10;此前记 63 系按 73 计,2026-10-05 按实测更正),
> 其中 6 项已在 `skill/README.md` 里**逐条写明卡点**
> (`hls-ap-none-rtl-integration` / `vivado-input-delay-calc` / `cic-coherent-sampling` /
> `prompt-hls-pipeline` / `prompt-timing-triage` / `golden-compare-script`)——
> 卡点都是「**没有真跑过,写不出『已验证的效果』**」。**不建空壳。**
>
> 🟢🟢 **那"另一半"也已闭合 —— 交付仓库侧不再是空的(2026-10-03 同日)**。
> 撰写当时 `~/Desktop/qrs_fpga_2026/skill/README.md` **实测就是 0 字节**(该目录自
> 2026-09-22 建起从未填过),**上面那条判断在当时是真的**;🔴 **但它在当天就被推翻了**:
> 用户点名授权(`CLAUDE.md` §6.0.1)后,技能包连同 **PL / PS 全量源码 + 当前位流**一并
> 同步进交付仓库,存为 commit **`352cf69`**(**406 files / 298,443 insertions**),
> 并**由用户推送成功**:`ba76d27..352cf69  master -> master`(快进,未 force),
> 事后 `git status` 报「与上游分支 `origin/master` 一致」。`skill/.gitkeep` 同批删除。
> ✅ 未改名 —— 11 个文件目录名 / 文件名**全部纯 ASCII**(M14 实测零命中)。
>
> 📌 **两条留档的口径纪律(比结论本身更值钱)**:
> 1. **交付物状态分三档写:`已提交`(本地 commit)/ `已推送`(远端)/ `已生效`。**
>    模型自己那次 `git push` **失败**了 —— `fatal: could not read Username for
>    'https://gitee.com': terminal prompts disabled`(本机无任何 Gitee 凭据;
>    `git ls-remote` 能通只因仓库公开、读不需要认证)。当时写成"待推送"是**对的**;
>    若图省事写成"推送完成",就是伪造结果(§6.2 红线)。**推送这一步是用户做的。**
> 2. **"当时是真的"不等于"现在还是真的"。** 本条 blockquote 从 🔴 改成 🟢🟢,
>    改的不是结论的对错,而是**事实变了** —— 引用旧记录时先核一遍它描述的状态还在不在。

> 🔴 **LLM-018 是「同一类错误第三次出现」的样本** ——
> 模型在 `LLM-017` 里把「模块 demo 只驱动 3 根线」**推断**成
> "MCLK 被板载 25 MHz 晶振**焊死**",还把它标成"D10 的一手依据";
> 本次被用户实测推翻:**板上有外部 CLK 输入,由跳线帽选择**。
> 与 `LLM-003`(把推断说成规定)、`LLM-012`(官方 XDC 里没有 ≠ 板上没有)**同型**。
> ⇒ 已提炼候选 `skill/zero-cost-elimination-first/`。

> 🔺 **LLM-009 是分水岭**:此前八条**没有任何一条**有"代码真的跑起来"的证据
> (全是静态审查或只有手册出处)。从这一条起,引用"已验证"才有波形依据 ——
> 但**仅限 LLM-009 明确列出的那几项**,不要把光环扩大到整个模块。

> ⚠️ **LLM-009 的判据④ 已被 LLM-010 推翻** —— 它把 DIR 字节的期望值写成
> `code[19:16]/code[15:8]`,而**这串期望值是从 RTL 抄的,不是从手册推的**,
> 所以那一项"通过"是**恒真**的。引用 LLM-009 时**不得**把"9 字节位序全部正确"
> 当作结论 —— 它只证明了"波形与 RTL 自洽"。

### ⚠️ 引用前须知(重要)

- 🆕 **LLM-020(交付仓库建立)可引用为事实的**:
  `git add -An --all` dry-run 清单 = 14 项、`gitee.com/dsgmxc/2026_fpga_qrs` 公开仓库、
  根提交 **`ba76d27`**(`14 files changed, 88 insertions`)、交付仓库**非 ASCII 文件名零命中**。
  🔴 **不得写成"仓库已可提交"**:`LICENSE` 与 `skill/README.md` **均为 0 字节**,
  ⇒ **3.3.3.3(开源协议)与 3.3.5.2(技能包)目前尚未满足**;仓库内**无源码、无脚本、无实测输出**,
  「他人从零复现」这一项**现在无法测**。
  🔴 **模型"把 `.git` 并到工作区目录"的建议已被用户否决并撤回** —— **不得**在任何地方
  把它写成模型给出的有效方案。
- 🆕 **LLM-019(AD9238)可引用为事实的**:用户移植版与厂商例程
  `AD9238_65.v` **逐行一致**,唯一差异是 `ADC_CLK` 由 output 改为 input(合理);
  厂商**用户手册 §4** 给 `Volt=(Data-2048)×10/4096`,而**官方例程 RTL** 给
  `12'hFFF-(ADC_IN+12'd2048)`,**两者不可兼得**(算开 = `2047 - Data`)。
  🔴 **不得写成"厂商例程写错了"** —— 模型**明确不判定谁对**,因为模块的模拟链路是否反相
  **从 PDF 里读不出来**;用户**也未表态**。**这是一条悬而未决的矛盾,不是已解决的缺陷。**
  🔴 **`ad9238.v` 零仿真、零上板** —— 用户 09-22 明确「官方有标准历程我就不跑了……
  这个先算作写完了」。**"写完" ≠ "已验证"**,P1 判据(单独仿真 + 挂顶层上板)**两项都没走**。
  ⚠️ **项目文档里的 ADC 型号 `AD9226` 与实际模块 `AD9238` 不一致** —— 待更正,模型未擅自改。
- **LLM-004 已结案** —— 第二轮那两条新发现判定为 **「作废」而非「已验证」**:
  所依附的代码(`CS_MODE`/`next_state`/`DATA_R`)已被用户重写删除,**无法证实也无法证伪**。
  **不得改写成"已验证"。**
- **LLM-005 中 `dac_1220.v` 的 6 条审查结论全部未经仿真验证** —— 用户收工时明确"还没写完"。
- **`spi_master` 至今未跑综合、未做时序分析、未上板** —— 引脚约束 / IOB / 板级走线完全未验证,
  **M13 无证据**。功能仿真通过 ≠ 上板跑通。
- **LLM-005 的测试台由 Claude 代写**(§6.0 显式例外,用户原话"算了仿真你来写先吧"),已如实记录。
- **LLM-006 全部结论只有手册出处,无任何仿真 / 上板证据** —— `dac_1220.v` 当时仍是半成品,
  `SPI_HALF_T` 新取值(≥204)尚未回灌重跑。**不得写成"已验证"。**
  另:LLM-006 修正了 LLM-005 里的一个估算数字(表项溢出 `105000` → 实算 `90000` 拍)
  与一条表述(审查项 ④「两套并行实现」→ 实为**结构需求**,改法是一级 mux)。
- **LLM-007 的代码改动一行未跑** —— `dac_1220.v` 改完**未编译、未仿真、未上板**;
  `SPI_HALF_T = 256` 的新分频**仍未回灌重跑** `spi_master` 仿真(09-15 那次是 `=10`)。
  **代改由用户明确授权**(原话「保持我的语言风格,直接修复问题」),但授权 ≠ 验证。
  记录内已主动标注**一处未经波形证实的时序推断**(`t9` 与背靠背字节),引用前须实测。
- **LLM-009 是本项目第一条「代码真的跑起来」的记录** —— 有 `xvlog`/`xelab` 日志与 VCD 波形为证。
  **可以写"已实测"**,但**必须同时引用它列的局限**:
  ① 验证用的是 `/tmp` 下的 `add_force` force 脚本,**不是**用户的 `tb_dac_1220.v`;
  ② 三个毫秒级参数被压缩 → **"等多久"(200 ms / 600 ms / 30 ms)没验**,验的是协议层与状态机转移;
  ③ **仍未综合、未做时序分析、未上板** → **M13 仍无证据**。
  ⚠️ **同一份 `dac_1220.v` 里,LLM-005/006/007/008 指出过的其余问题仍未全部验证过** ——
  LLM-009 只覆盖了复位图案窗口 / 9 字节位序 / `init_done` 时刻 / 触发路径这四项。
- **LLM-008 的 `dac_1220.v` 四个版本全部零运行** —— 四轮审查指出的问题已由用户改完,
  但**未编译、未仿真、未上板**。**"静态审查通过" ≠ "模块能用"。**
  记录内那一套 WORK 状态时序(字节 92 µs / 帧 430 µs / 间隔 31 µs)**是按 `spi_master`
  源码逐拍推的,未在波形上确认** —— 尤其字节间隔,`t14` 的定义是"下降沿→上升沿",
  必须在 ILA 上量一次。
  → ✅ **09-19 已补上这一条**:`tb_dac_1220.v` 实测 `t14 = 1791 拍 = 35.82 µs`
  (手册下限 41 tXIN = 16.68 µs,余量 +115%)。见 LLM-010。
- **LLM-010 的边界** ——
  ① **可以写"已实测"**:DIR 切片 / 复位图案四窗口 / CMR 语义 / `init_done` 时刻 /
  `dac_cs` 恒低 / SCLK 半周期 / `t14`,有 `sim_logs/` 两份原始日志为证;
  ② **不得外推**:三个毫秒级等待仍被参数压缩 → **「等多久」未验证**;
  **仍未综合、未做时序分析、未上板** → **M13 无证据**;
  ③ **RTL 与测试台均由 Claude 代改/代写**,经用户明确要求(§6.0 显式例外),已如实记录;
  ④ **`f_sclk` 的固件上限(246 kHz)未在硅上验证过** —— 97.66 kHz 的余量是手册算的。
- **LLM-011 的边界** ——
  ① **可以写"已实测"**:顶层 elaboration 零错;`qrs` 顶层仿真实测 9 字节
  `24 20 A0 05 A1 40 80 00 00`、`0x40` 只出现 **1 次**、`init_done = 1`;
  ② **不得外推**:顶层仿真用的是**加速副本**(三个毫秒级参数被覆写)+ **`clk_wiz_0` 桩模块**
  → **真实等待时长**与**MMCM 锁定过程/抖动**都没进仿真;
  ③ **仍未综合、未写 XDC、未上板** → **M03 / M04 / M13 仍无证据**;
  ④ 顶层 `qrs.v` 本次**由 Claude 代写**(用户明确要求,§6.0 显式例外),已如实记录。
- **LLM-012 的边界** ——
  ① **上板证据 = 用户口述的三点电压结果**(万用表量 `0x40000`/`0x80000`/`0xC0000`,
  「测量结果跟这张表一样」),**无万用表读数截图/数值原文、无 ILA 波形存档** →
  引用时**不得**写成"已存档的 M13 实测输出";可写"用户实测三点电压与预期一致";
  ② 🟢 **"哪一列对上"已确认 = 模块层**(用户原话「我测得都是模块层输出电压,都是对上的」)
  ⇒ **`docs/dac1220_interface.md` 的 U2(模块量程 −10~+10 V)与 U3(传递关系
  `V = −10 + 20 × code / 1048575`)升级为"已证实"**,可写进报告;
  ⚠️ **但端点(`0x00000`/`0xFFFFF`)未测** —— 模块输出级通常在轨附近提前饱和 ⇒
  「**满量程恰好 ±10.00 V**」**仍不得断言**;
  ③ **模型第一轮的核心推断是错的**(把 J4 扩展口引脚误判成"摄像头专用脚,没接"),真根因
  (**烧了旧程序**)是用户自查发现的 —— 引用时**不得**写成"模型定位了根因";
  ④ **模型曾把加 ILA 之前的旧读数**(布线 246/246、DRC 仅 `ZPS7-1`)写进文档,
  **同日回读原始 `.rpt` 后自行改正**为 2817/2817、DRC 无 Error ——
  引用"综合实现报告数字"时请以 `qrs.runs/impl_1/*.rpt` **现读**为准;
  ⑤ 新增 RTL 共 **3 行**(`qrs.v` 的 `rst_n_sys`,用户明确要求代写,§6.0 例外),
  其余为零,模型主要做了资料查证与文档;
  ⑥ 由此产出的 `docs/board_pins.md` J4 引脚表中,**27–36 脚已三处互证**,
  **1–26 脚(BANK13)只有两处互证** —— 用之前请对板上的丝印再确认一次。
- **LLM-013 的边界** ——
  ① 🔴 **本次会话零代码运行、零仿真、零综合、零上板**。AD9833 的例化参数、4 帧初始化序列、
  `spi_master.v` 的帧尾拍数、`dac_1220.v` 的字间间隔 —— **全部是静态推演**,
  **不得写成"已验证"**;用户当时**尚未写 AD9833 的任何代码**;
  ② 🔴 **英文原版手册未获取**(本机没有;`analog.com` 被网络限制挡住)→
  **`t8 max = t4 − 5 ns` 这条不常见规格只在中译版表 2 见过,未经原文复核**,
  且该条自带脚注「通过设计保证,但未经生产测试」。**引用前必须复核**;
  ③ ⚠️ **模型对该 RTL 改动的风险判断先给错**(说"零风险")、**被用户一句
  「这个模块还要给 dac 用不好改啊」戳破后才改对** —— 引用时**不得**写成"模型识别出耦合"。
  真耦合点是模型在被反问之后才去追的(`dac_1220.v:288` 的 `DELAY` 从 `done` 起算);
  ④ ⚠️ 本记录内含**两处模型自我更正**,引用前请一并带走:
  上一轮说「必须分帧、连续流走不通」**只对 `DATA_WIDTH=16` 成立**;
  上一轮转述的厂商 5 字序列**是错的**,回源码核实后为 4 字且 `ClearReset()` 从未被调用;
  ⑤ **用户最终选择方案 A**(`spi_master.v` 一行不改),**模型建议的"上电顺序"决策用户尚未表态**。
- **LLM-015 的边界** ——
  ① ✅ **可以写"已实测"**:AD9833 三线引脚上的**每一帧内容**(init 4 帧 + 正常态 3×2 帧)、
  `init_done` 时刻、SCLK 半周期 / `t4` / `t7` / `t11` / 帧间 FSYNC 高电平、协议违例计数 0,
  有 `sim/logs/tb_adc_9833_fixed.log`(46 PASS)与 `tb_adc_9833_mutant.log`(4 变异体 / 22 FAIL)为证;
  ② 🔴 **`t8` 一项是 FAIL,不是 PASS** —— 实测 10 260 ns,手册窗口上限 10 235 ns,**超出 25 ns**。
  模型判定"物理上仍安全"的理由(FSYNC 抬起→下帧首沿 45 680 ns ≫ 5 ns)**是对手册立法意图的解读,
  不是手册原文**,且中译版该条自带脚注「通过设计保证,但未经生产测试」。
  **引用时不得写成"时序全部合规";`t8` 尚未结案,需一次 ILA。**
  ③ 🔴 **被测对象与测试台均由 Claude 代写** ⇒ 本次是**"模型写的代码 + 模型写的判据"**,
  证据强度**低于** `tb_dac_1220`(那份被测对象是用户写的)。引用时请一并带上这条限制;
  ④ ⚠️ **仍未综合、未做时序分析、未上板**;`PWR_WAIT` 真值(200 ms)与板载 25 MHz 晶振
  实际频率**均未验证** → **M13 仍无证据**;
  ⑤ ⚠️ `src/rtl/adc_9833.v` **尚未同步进仓库** ⇒ 本记录 §8 的复现命令**现在跑不通**,
  需先把该文件拷进 `src/rtl/`。

- **LLM-016 的边界** ——
  ① ✅ **可以写"已实测"**:顶层 elaboration `xvlog`/`xelab` 双阶段 **rc=0**;
  顶层 smoke 仿真实测 4 帧 `0x2100 / 0x570A / 0x428F / 0x2000`
  且**时刻与模块级 tb 逐帧对上**(198.99/408.53/618.07/827.61 µs vs 198.97/408.51/618.05/827.59 µs);
  ② 🔴 **引脚未经实际接线核对** —— `adc_sync=V13 / adc_sclk=U14 / adc_mosi=V12` 是模型
  **照 `docs/board_pins.md` 的 J4 表挑的空闲脚**,`io.xdc` 注释里已显式标注。**上板前必须核对**;
  ③ ⚠️ **未综合、未实现、未上板** → **M13 无证据**;`PWR_WAIT` 真值未跑;板载晶振频率未测;
  ④ ⚠️ **ILA 里没有 AD9833 的三根线**(改 probe 数要回 IP 目录重新生成);
  ⑤ ⚠️ **`qrs.v` / `io.xdc` 均由 Claude 代改**(用户明确要求,§6.0 例外);
  ⑥ 🔴 **本次暴露「上电顺序」窗口但未决策**:AD9833 约 200.8 ms 加激励 vs DAC1220 ~800 ms 才可用,
  中间偏置未定义(即 **D21**)。模型**只在 `qrs.v` 注释里点名,没有替用户选**。

- **LLM-017 的边界** ——
  ① 🔴 **本次全程零代码运行、零仿真、零综合、零上板操作**。上板现象(「500 mV 直流」)
  由**用户口述**,模型未直接观察。**不得写成"模型定位了故障"**;
  ② ✅ **可以写"已查明"(均有出处)**:开发板输入 **DC 6~16 V**、J4 第 40/38 脚 = +5 V/+3.3 V、
  **PL 三个 BANK 全部 3.3 V ⇒ 所有 PL IO 均为 3.3 V**(开发指南 V3.4);
  AD9833 **MCLK = U1 第 5 脚 ≠ SCLK = U1 第 7 脚**(手册引脚表),
  模块接口上的 `CLK` 是 SCLK(**依据是模块自带 STM32 demo 只驱动 FSYNC/SCLK/SDATA 三根线**,
  `HARDWARE/AD9833/Communication.h:62-64`);当前配置输出 **999 999.9776 Hz**;
  ③ 🔴 **故障根因未定位** —— `bitstream 是旧的` / `接线错` / `MCLK 停振` **三个全是假设,
  一次测量都还没做**。**引用时不得写成"已排查出原因"**;
  ④ ⚠️ **500 mV 与手册中间电平 344 mV 的差值,模型明确写了"解释不了"** ——
  这不是遗漏,是**刻意不断言**。不要在后文替它补一个解释;
  ⑤ ⚠️ **模块上 `U2` 是不是 LDO 未确认**(PDF 文字层无型号)⇒
  **模块该接 +5 V 还是 +3.3 V 仍未定**。此项**直接影响硬件安全**,上电前必须确认;
  ⑥ ⚠️ **`t8` 仍未结案**;**`spi_master` 仍未综合 / 未做时序分析 / 未上板**
  ⇒ **AD9833 这条路径没有任何上板证据**(📌 M13 是整体项,**已于 09-19 由 `dac_1220` 路径达成**,
  不因本次失败回退 —— 早期记录里" M13 仍无证据"的说法在 09-19 之后已过时);

## 值得写进报告第 5 节的候选

- **LLM-001 纠错**:模型把 AD9833 晶振频差严重高估("会让测量不可用"),
  经重新推导发现比值法能精确抵消 → 自我纠错 + 导出真实约束 `T ≤ 1 ms`
- **LLM-002 纠错**:**用户把模型写好的 2400 行代码推翻,改为自己先写** ——
  这是"我不同意 / 我改了"类证据里最有力的一条
- **LLM-003 纠错**:**模型把"厂商 IP 不计入工作量"当成赛题规定讲了出来,实为编造**;
  用户一句「文档有说明吗」要求出示依据 → 全文检索指南 PDF,零命中,模型撤回结论。
  这是"**用户要求出示出处**"类证据的代表,也是**把推断包装成依据**这一失败模式的样本
- **LLM-004 协作模式**:多轮「模型审查 → 用户自主修改 → 模型复核」——
  用户修对了 5 条(含 2 条综合级致命项),但有 1 条**改了表象没动根因**、
  另 1 条**修 bug 的改动引入新 bug 且被上游 bug 掩盖**。
  另可作为**"模型审查不能替代仿真"**这一边界的实证
- **LLM-005 纠错(最强样本)**:**模型连续 3 次在验证方法上判断失误,且全部由模型自己实测发现** ——
  ① 断言 `A5A5` 对位序最敏感,实测是**回文**,判别力为零(改用非回文图案后抓取率 2/6 → 5/6)
  ② 断言"时序余量与 `SPI_HALF_T` 无关",边界扫描实测**余量 = `SPI_HALF_T − 4`**
  ③ 两条 `sed` 静默失配,制造出**恒 PASS 的空转负测试**(修正后立刻抓出 5 个 FAIL)
  → 共同教训:**不先证明测试会失败,测试通过就没有信息量**
- **LLM-005 用户纠正**:**用户一句追问「光一个初始化就有十几个步骤,全写在一起真的可行吗」
  暴露模型"6 个状态"的表述不严谨** —— 模型自认讲得太轻巧,改用用户自己写的
  `spi_master`(32 个边沿 / 4 个状态)作类比重新论证,并补上了此前缺失的判断标准。
  属"**我不同意**"类证据
- **LLM-005 首版落地**:用户按讨论结果写出 `dac_1220.v` —— **表驱动方向被采纳**,
  但暴露 6 条致命项,其中"**表只驱动了等待、没驱动发送**"是结构性问题。
  可作为「**方案正确 ≠ 落地正确**」的样本
- **LLM-006 纠错(新类型:对**第三方资料**的纠错)**:**厂商提供的 STM32 例程本身有两处不达标** —
  ① 字节间距 10 µs < 手册 `t14` 下限 16.68 µs;② 读函数在 SCLK 上升沿即刻采样,
  而手册明确从机是在该沿更新输出。
  价值在于:**例程是"能跑"的证据,不是"合规"的证据** ——
  且例程有注释、有页码引用,看起来比手册还权威,最容易让人照抄。
  可作为「**不要用二手资料替代一手规范**」的样本
- **LLM-006 主动不作断言**:厂商例程把 ±10 V 映射到码值,极易顺手写成"这块 DAC 输出 ±10 V";
  但手册写芯片本体是单极性 `0 ~ 2×VREF`,而资料包里**没有模块原理图** →
  列为「未核实项」而非结论。与 LLM-003(把推断说成规定)恰成对照
- **LLM-007 纠错(新类型:信号语义污染)**:**用户提出用 `init_done` 兼作 SCLK 选择信号** ——
  该信号当时被放在"复位图案刚结束"那一拍,**恰好落在正确的切换时刻上,方案看起来完全能跑**;
  但对外语义是"DAC 可用",上层会据此立刻去写 DIR,而校准还要跑 600 ms
  → **校准期被写命令打断,and 命令读不回来**。
  修法:拆成内部 `man_mode` + 对外 `init_done`。
  价值在于:**共用一个信号时"当前刚好对得上",恰恰是最难靠波形发现的缺陷** ——
  静态审查能拦,仿真很可能放过。与 LLM-004「改 A 引入 B」同源但方向相反
- **LLM-007 正面样本(与 LLM-004 对照)**:修「`CAL` 门限过窄导致 `0xA1` 发不出去」时,
  必须放宽 `DELAY` 的复位门限,而这会让 `init_cnt` 走到 6、越界读 `init_sclk[6]`。
  模型**在改上界之前**先在 `INIT` 里补了 `if(init_cnt < 6)` 保护 ——
  同类风险,LLM-004 时是"落笔后才被上游 bug 掩盖",这次**在落笔前拦住了**。
  可提炼为固定动作:**改循环上界前,先枚举所有用该计数器做索引的地方**
- **LLM-007 用户纠错(讲解层面)**:模型讲完「CS 恒低」后用户追问
  「**难道他不是 spi 通信吗**」—— 暴露模型没有先拆开「CS = 选从机 ≠ 分帧」这层默认假设,
  直接讲器件特例,导致结论听起来像"这不是 SPI"。
  属"**讲解没落地**"类证据(区别于"结论错了")
- **LLM-007 用户对模型建议的取舍**:用户**删掉**了模型建议保留的 `spi_sclk`/`spi_cs` 调试端口、
  **未采纳**把 `dac_cs` 改为常量赋值的写法、**采纳**了 `cal_delay[0]` 10→1024 的调整 ——
  可作为"人机分工中用户保留最终裁量"的样本
- **LLM-008 纠错(新类型:意图 vs 代码)**:模型断言 NORMAL_WAIT「每 1.5M 拍才采一次,沿必然错过」,
  用户反驳「**我特意没有清空**」。模型没有直接让步也没有坚持,而是**重新读出该段原文核对** ——
  结果**用户的意图对、代码和用户的描述不一致**(`else` 分支里多了一句 `delay_cnt <= 0;`,
  把"饱和轮询"变成了"每 1.5M 拍采一次")。
  价值在于:**用户描述的是意图,代码描述的是行为;两者冲突时不能盲信任何一方** ——
  直接认错会留下 bug,直接坚持会误解设计。
- **LLM-008 结构类缺陷样本**:`NORMAL` 状态**没给 `tx_data` 赋值**,而 `code_in_d[0:3]`
  数组**建对了、填对了、全文没人读** —— 波形上"有 4 个字节在发"但内容是错的。
  属「**结构齐全、接线漏了**」,是审查时"检查数组是否被读"这一动作的实证
- **LLM-008 主动去手册挖数**:用户问"最后的延迟打多少拍",而接口文档里没有这个数 ——
  模型**没有凭印象估**,而是 `pdftotext` 手册后 grep `settl`,拿到建立时间的原始表格
  (16 位 2 ms / **20 位 15 ms** / 量程两端 ×2)。与 LLM-006「不要用二手资料替代一手规范」同向
- **LLM-008 交付方式被用户纠正**:模型给了一份带推导、出处、附加建议、结尾追问的长回答,
  用户回「**简化回答,太长了,就告诉我时序就行**」—— 属"输出方式"类反馈(区别于结论对错)
- **LLM-009 正向分水岭(报告第 5 节的主线素材)**:项目**第一批真实验证证据**从静态审查转为波形实测 ——
  复位图案四个手册窗口(t16/t17/t18/t19)首次**量出来**而不是推出来;
  `dac_1220.v` 的 NORMAL 字节 `40 01 23 50` **第一次被真的验过**;
  并用**旧版 / 新版 A/B**(40 个 SCLK 沿 vs 72 个)**坐实**了一处触发死锁及其修复。
- **LLM-009 模型自我纠错(两类,均由模型自查发现)**:① 第一跑**没落在自己声称的失败窗口里**
  (脉冲打在门外 650 µs)→ 判该跑**信息量为零**并重做;② **VCD 解码器把复位图案的下降沿
  当成采样点**,误报"字节不符",复核后发现是**量具错、RTL 对**。
  可提炼为「**先证明量具可信,再相信结论**」,与 LLM-005 的 `sed` 静默失配合流
- **LLM-009 测试台的"能跑但测的不是真东西"**:`CLK_PERIOD = 10`(100 MHz)而 DUT 全部常数
  按 50 MHz 标定 → 复位图案**三个窗口破窗,仿真却照样跑通**。
  属"**仿真通过 ≠ 测对了**"的样本,与 LLM-005 同源但成因不同(那次是测例判别力,这次是时基)
- **LLM-009 跨层语义陷阱**:同一个 `code`,**芯片层 `0` = 0 V,模块层 `0` = −10 V**。
  模型没有凭印象答,回手册取 Table 8/9 + 厂商例程,并明确标注
  "模块层是**代码级证据不是原理图证据**,须三点标定"—— 与 LLM-006(不拿二手资料替代一手规范)同向
- **LLM-010 纠错(报告第 5 节的**最强样本**,与 LLM-005 合流)**:**判据的来源必须独立于被测物。**
  LLM-009 的判据 ④ 把 DIR 期望值写成 `code[19:16]/code[15:8]` ——
  **这串期望值是从 RTL 里抄的**,于是"波形与期望一致"恒真,仿真通过不含信息量。
  一查手册才发现**判据本身错了**(手册 p14:DIR 左对齐 → 应为 `code[19:12]/code[11:4]`)。
  与 LLM-005 的 `sed` 静默失配(恒 PASS 的空转负测试)是**同一失败模式的两次出现**
- **LLM-010 用户直觉 vs 模型结论**:用户说「感觉我发的字节和例程不太一样」,
  而模型上一轮刚宣布「9 字节逐字节一致」。**用户对** ——
  一致的是"字节与 RTL 自洽",不是"字节与手册一致"。
  可作为「**"和我的实现一致"不能当"正确"的证据**」的样本
- **LLM-010 不盲从厂商例程(LLM-006 结论的第二个实例)**:例程 CMR 高字节写 `0x00`,
  本项目写 `0x20`。不查手册极易判成"用户写错了";回 Table 13 才发现
  **bit13 明确要求写 '1',是例程的 0 不合规**(p17:「This does no harm in current revision chips」)
- **LLM-010 量具自查三连(全部靠"真的跑一遍"暴露)**:① 第 1 个 SCLK 沿其实是图案前的空闲低电平
  → 被当成 `t17`,且第 3 个低段**越界写 `l_w[2]`**(数组只到 `[0:1]`);
  ② SCLK 半周期期望值填成 512,实测 256 —— **量的是半个周期,期望值错、RTL 对**;
  ③ `t14` 在下降沿分支多记一次,把 256 拍的噪声混进最小值,真值 1791 拍被掩盖。
  → 归入「**先证明量具可信,再相信结论**」
- **LLM-010 变异测试**:把两行切片改回旧写法,21 项里 **4 项 FAIL**(全部落在 DIR 判据)
  → 证明这套判据**具备失败能力**,而不是"恒 PASS 的空气测试"。
  可直接进报告作「验证方法有效性」的证据
- **LLM-011 量具错第 4 次(采样沿写反)**:监视器把 SPI 的采样沿写成**上升沿**
  (CPOL=0/CPHA=1 应在**下降沿**采样)→ 字节全是错位垃圾,顶层被误判 FAIL。
  **RTL 没错**;判据是**边沿总数 150 = 6 + 5×16 + 4×16**,这个数**从第一跑起就是对的**
  → 说明 9 个字节一个不少。
  与 LLM-009 / LLM-010 的量具错**同源**,可归入「**先证明量具可信,再相信结论**」
- **LLM-011 量具错新形态**:基于"SCLK 静默"推字节的读法**读不到最后一个字节**(它后面没有沿了)
  → 稳定少报 1 个字节。教训:**靠"下一个事件"触发的读取方式,天然丢掉最后一个事件**
- **LLM-011 工具链陷阱(新类型)**:**`xelab` 静默复用上一轮的 `xsim.dir`** ——
  一次 `xvlog` 因 shell 变量没展开而找不到文件(模型没先检查这条),
  下游 `xelab` 用**陈旧快照**编出结果,**FAIL 输出与上一轮一字不差**,差点被当成新结果。
  与 LLM-005 的 `sed` 静默失配同类:**中间步骤静默失败,最终输出仍然"看起来正常"**
- **LLM-012 纠错(新类型:用了错误的"缺省规则")**:模型对照官方 `NAVIGATOR_ZYNQ_IO.xdc`
  后发现 `dac_cs=W13`/`dac_mosi=Y14` 在官方表里叫 `cam_data[5]`/`cam_data[7]`,
  而 `W15` **在官方表里完全不存在** → 据此断言「这三个脚是摄像头专用,DAC 根本没接到」。
  **错。** 真相:这三个脚**同时**引到了 40PIN 扩展口 **J4 的第 32/34/36 脚**,
  底板上叫 `CMOS_D5`/`CMOS_D7`/`CMOS_XCLK`。
  **根源是一条错误的缺省规则**——「官方 XDC 里没有 ⇒ 板上没有」;
  而开发指南原文写明该引脚表是「**除扩展口外**」,扩展口引脚本来就不在其中。
  价值在于:**"资料里查不到"和"不存在"是两件事**,与 LLM-003(把推断说成规定)同源、方向相反
- **LLM-012 纠错的方法(值得单独记)**:模型发现自己的推断**解释不通**——
  正点原子不该把三个不相关的脚放在一起 —— 于是**换了一份资料**(底板原理图第 5 页 J4 表)
  才找到答案;并用**两处独立吻合**(`W14↔CMOS_PCLK`、`W13↔CMOS_D5` 与官方 XDC 一致)
  + 一处第三方佐证(走线长度 CSV 的 `X4.55`)确认映射方向读对了。
  可作为「**同一个问题换一份一手资料**」的样本
- **LLM-012 最终根因不是模型给出的(与 LLM-011 同型)**:模型把票押在"引脚接错",
  真根因是**位流没更新**(烧的还是上一版程序)。两个会话连续出现同一模式 ——
  **模型的所有静态分析都建立在"板上跑的是我以为的那版程序"这个从未验证过的前提上**。
  可提炼为固定动作:**上板现象与静态分析矛盾时,先确认"板上跑的是哪一版"**
- **LLM-012 硬件事实的产出**:本次把**领航者 ZYNQ-7020 J4 扩展口 40 脚完整引脚表**
  查了出来(原理图第 5 页 + 官方 XDC + 走线长度 CSV 三源交叉),落进 `docs/board_pins.md`,
  闭环了 `project_state.md` 的 **Q05**。同时确认 `sys_clk = U18 (50 MHz)` / `sys_rst_n = N16`。
  可直接用于报告的接口/引脚章节
- **LLM-012 追加 · 实测反向确认了 LLM-010 的修复(证据链闭合的样本)**:用户量得
  `0x40000`/`0x80000`/`0xC0000` 三点的**字节序**为 `40 40 00 00` / `40 80 00 00` / `40 C0 00 00`,
  与 DIR **left-justified** 规则算出的完全一致 ⇒ 证明 LLM-010 修的"丢掉 `code[11:4]`、
  有效分辨率掉到 ~12 位"那个缺陷**在硬件上确实修对了**。
  可写进报告**§优化过程**:一条"仿真变异测试 → 手工推导 → 板上实测"三段互证的完整链
- **LLM-012 追加 · 用户选点比模型建议更好**:模型建议量两端点 + 中点
  (`0x00000`/`0x80000`/`0xFFFFF`),用户实际选了 **1/4、1/2、3/4 满量程** ——
  避开模块输出轨钳位,同时可看线性度与单调性。可作为**"设计判断依据"**的素材(B06)
- **LLM-012 追加 · 模型把旧读数写进文档后自行改正**:模型引用的是**加 ILA 之前**那份
  实现报告(布线 246/246 / DRC 仅 `ZPS7-1`),而盘上已是新报告;同日**回读原始 `.rpt`**
  改为 2817/2817、DRC 无 Error。可写进报告第 5 节**自我纠错轨迹**,
  与 LLM-005「静默复用过期产物」同型
- **LLM-012 追加 · "不确定就说不确定"做了,但前提本身没核实(双面样本)**:
  被问"`io.xdc` 补 `create_clock` 有什么问题"时,模型先如实写【**未实测,不下结论**】(对上);
  但同一条回复里又**断言"本机未装 Vivado"** —— 只搜了 `/tools/Xilinx`、`/opt/Xilinx`、`~/Xilinx`
  (**大写**)、**漏了小写 `~/xilinx`**(实际在 `/home/hyp/xilinx/2025.2/Vivado/bin`),
  而**模型自己的 memory 里就写着这个路径**。**错因与主线完全同型**:
  「官方 XDC 里没有 ⇒ 板上没有」/「/tools 里没有 ⇒ 本机没装」= **一次不充分的搜索 →
  一个全称否定结论**,同一天犯了两次。随后模型自行用 Vivado 2025.2 batch 在 `/tmp/cc_test/`
  实测收口。**正反对照俱全,是"自我纠错轨迹"的好素材**
- **LLM-012 追加 · 实测出来的硬结论(可直接写报告)**:
  同一 port 上重复 `create_clock` **不报 Error,后者覆盖前者** ——
  同名同周期 → `WARNING [Constraints 18-619]`;不同名 → `CRITICAL WARNING [Constraints 18-1056]`
  `Clock 'my_clk' completely overrides clock 'clk_in1'`;`report_clocks` 只剩一个时钟。
  ⇒ 项目结论:**在 `io.xdc` 里加注释即可,不必重复加约束**(真加只多一条 WARNING,
  且 IP 那条的 `set_input_jitter` 是否被覆盖丢失**未测**)
- **LLM-012 追加 · `locked` 组合死锁的取舍(可写进报告的接口/复位设计章节)**:
  修 `locked` 未接时,`clk_wiz` 自己的 `.resetn` **必须**保持接原始 `rst_n` ——
  否则 `locked` 依赖 `resetn`、`resetn` 又依赖 `locked`,构成组合死锁,比原 bug 更严重。
  这是"改一处要连带想清楚依赖环"的典型样本
- **LLM-013 纠错(本次最强样本)·「零风险」结论被用户一句反问推翻**:
  模型建议改 `spi_master.v` 的帧尾逻辑,并写「**这个改动本身零风险**」——
  它**只考察了 AD9833 那一侧**。用户没有接受这个前提,直接反问
  「**我这个模块还要给 dac 用不好改啊**」→ 模型才回去追 `dac_1220.v`,把耦合面三项列清:
  `cs` 无影响(该实例 `.cs ()` **根本没接**)/ SCLK 边沿无影响 / **`done` 有影响**
  (`DELAY` 从 `done` 起算 1024 拍 ⇒ 字间间隔 35.8 → 30.7 µs)。
  **结论由「零风险」改判为「你的顾虑成立,先别改」,用户采纳。**
  ⇒ 素材价值在于:**把一个"看起来局部"的改动判成零风险,是因为没把共享模块的每个实例都走一遍**
- **LLM-013 · 两处模型自我更正(可写报告的"自我纠错轨迹")**:
  ① 上一轮断言「必须分帧、连续流走不通」**只对 `DATA_WIDTH=16` 成立** ——
  改 `DATA_WIDTH=80` 后 `cs` 从 START 低到 DONE,**恰好就是手册说的连续流**;
  ② 上一轮转述的厂商初始化序列(5 字)**是错的** —— 回读 `AD9833.c` 源码,
  真实为 4 字 `0x0100 → 0x2000 → 0x570A → 0x428F`,且 **`SetFrequencyQuick()` 从不调用
  `ClearReset()`**,RESET 位是被帧 2 写 `0x2000` 时顺带清掉的 ⇒ 厂商那段代码真正的坑是
  **频率寄存器在 RESET 已释放之后才装**,中间有一段输出垃圾频率的窗口
- **LLM-013 · "二手结论要回一手核对"的样本**:
  与上一条同源 —— 模型转述厂商代码时**凭印象**写,回源码才发现抄错。
  可并入 `skill/vendor-example-is-not-spec/`
- **LLM-013 · 一条可写报告的规格纪律**:
  中译版手册足以支撑绝大多数 RTL 常数,但**"不常见规格"应回英文原版复核**;
  取不到时必须**显式标注未经复核**。本次 `t8 max = t4 − 5 ns`(AD9833)就卡在这里 ——
  该规格还自带脚注「通过设计保证,但未经生产测试」
- **LLM-014 · 量具错第 5 次,而且是「假 FAIL」(可写报告)**:
  首次跑 `adc_9833.v` 的正常态,拿到 `frame 9 got=42aa exp=6aa9 FAIL`,模型**第一反应是"RTL 有 bug"**。
  但**帧 7 走的是同一条 MSB 路径却 PASS** —— 这个矛盾才逼着回头审自己:
  `code = 28'h0AAAAAA` 是 **7 位十六进制数**(= 11,184,810),
  模型误按 8 位数的 `0xAAAAAAAA`(= 178,956,970)算,才得出 `0x2AA9`。
  `>>14 = 682 = 0x2AA` ⇒ 正确期望是 `0x42AA`,**RTL 从头到尾没错。**
  ⇒ **判据:当失败点与改动无关、或同一路径另一次却通过时,先审量具,再审被测对象。**
- **LLM-014 · 模型对自己的判断做变异测试,坐实一行代码的必要性**:
  `set_pending <= 1` 写在 `case` 之后(无条件、FSM 之外)。模型没有靠"我觉得应该这样"交差,
  而是造了一个变异体:把该行加 `&& state == NORMAL_WAIT`。结果**精确复现设计意图** ——
  落在 `NORMAL_WAIT` 的触发照过,落在 `GAP`(**一帧 1.3 ms 的发射中途**)的触发**直接消失**。
  ⇒ 这一行位置是必需的,不是装饰;`skill/edge-latch-outside-fsm/` 的第一份素材。
- **LLM-014 · 两条查表函数互证(接口设计的好样本)**:
  正常态喂 `code = 0xA3D70A`(init 里那个频率字)后发出的两帧 `570A / 428F`,
  与 init 的第 2、3 帧**逐字节相同** ⇒ `freq_word_f` 与 `init_word_f` 用同一条手册表 9 规则。
  **"用初始化值当正常态的测试向量"是这次想到的低成本互证手法。**
- **LLM-014 · 模型主动修掉自有缺陷:tb 缺看门狗会把缺陷伪装成仿真器故障**:
  变异体跑不通时 `xsim` 120 s 不返回,模型第一反应又是"工具挂了"。用 `run 8 ms` 定量跑一次,
  **2 秒就跑完 8 ms 仿真时间** —— 仿真没挂,是 tb 的 `wait(frame_i >= 7)` 在等变异体丢掉的帧。
  加 `task report` + `#6000000` 看门狗后,"卡死"变成逐帧可读的 `MISSING`。
  ⇒ **没有看门狗的 tb,会把被测对象的缺陷伪装成仿真器故障。**
- **LLM-015 · 首次把 `t8` 从"逐拍推源码"变成"实测",并测出一条真实偏离(可写报告)**:
  `LLM-014` 里 `t8` 还是推的("≈256 拍,要一次 ILA 才能坐实")。本轮测试台**第一次真测到**:
  ```
  实测 10 260 ns (= 513 拍 @ 50 MHz)   手册窗口 10 ns ~ (t4 − 5 ns) = 10 235 ns   → 超出 25 ns
  ```
  根因读 `spi_master.v:150-167` 坐实:`SCLK_TOG` 里 `bit_cnt` 到 `TH` 后**还要再等满一个
  `SPI_HALF_T`** 才跳 `DONE`,而 `cs` 在 `DONE` 里才抬起 ⇒ **`t8 = t4 + 1 拍,必然超上限 25 ns**。
  ⇒ 素材价值:**"仿真通过"往往是因为没量这一项;量了才发现偏离,而偏离的根因在共享模块里。**
- **LLM-015 · 手册窗口的"字面数字"与"立法意图"不是一回事(新判据,可写报告)**:
  超限 0.24% 时模型**没有自己改判通过**,也没有直接判死,而是把两条都摆出来 ——
  ① 字面:`t8` 超上限 25 ns;② 意图:`t8 max` 是为**连续 SCLK** 设的界,
  本设计 FSYNC 抬起后到下一帧首沿还有 **45 680 ns**(需保护的量是 5 ns)。
  同时如实标注这条解读**不是手册原文**、且原版手册未获取。
  ⇒ **给出判断依据而不是结论**(正对 B06 加分项,也与 LLM-013「零风险」那次形成对照:
  那次是没走完耦合面就下结论,这次是把依据和边界一起交出去)。
- **LLM-015 · 工具链陷阱(新类型,可进 skill)**:**xsim 2025.2 在 `%s` 里打宽向量会破坏非 ASCII 字节**。
  `chk` 任务用 `reg [8*128-1:0]` 传标签,中文 `帧`(`e5 b8 a7`)打出来是 `ff ff ff e5 ff a7`;
  **内联 `$display("…中文…")` 完全正常**。诊断靠 `hexdump`(先定性、再动手),不靠猜编码设置。
  `tb_dac_1220.v` 的标签恰好全是 ASCII 所以此前从未暴露 ——
  **"从没出过问题"不等于"这条路是通的"**。
- **LLM-015 · 用"初始化值"当正常态的测试向量,零成本互证两条查表路径(与 LLM-014 同源)**:
  更新①故意喂 `code = 0xA3D70A`(**就是 init 用的那个频率字**),其两帧 `570A / 428F`
  必须与 init 的第 2、3 帧逐字节相同 ⇒ 把 `init_word_f` 与 `freq_word_f` 钉在同一条手册表 9 规则上。
  **这次把手法固化进了 tb 的注释**,不是一次性技巧。
- **LLM-015 · "写复现命令"反过来查出同步缺口(第二次,可写进报告的复现性章节)**:
  写 `sim/adc_9833_verification.md` §8 时才发现仓库 `sim/` 下已有 tb,
  但被测对象 `src/rtl/adc_9833.v` **尚未同步**。报告里**显式标注该命令现在跑不通**并给替代路径,
  不写一条"看着能跑其实跑不了"的命令。(`LLM-013` 那次是 `qrs.v` / `io.xdc`。)
  ⇒ **复现命令应当当作交付验收项来写。**
- **LLM-016 · 量具错第 6 次,这次的量具是「忘了给变量初值」(可写报告)**:
  首次跑顶层 smoke,输出 `[adc 帧 x] 0x2100 …` / `== 采到 x 帧 ==` / `== 帧内容不符 ==`
  —— **判定失败**。模型第一反应又是"例化接错了"。但输出里那 **4 个值全是对的**,
  且与模块级 tb 一字不差 —— **"数据正确"与"判定失败"同时出现**这个矛盾才指向别处:
  `integer nbit; integer nframe;` **未初始化,而 Verilog 里 `integer` 默认是 `x` 不是 `0`**,
  `frm[nframe] = sh` 写到了 `x` 下标上。加 `= 0` 后一次通过。
  ⇒ 与 `LLM-014` 的假 FAIL **完全同型**;`skill/measurement-instrument-can-be-wrong/`
  的素材已达 4 次(LLM-009/011/014/016),**够写了**。
- **LLM-016 · 硬件事实与推断的边界处理(与 LLM-012 误判恰成对照,可写报告)**:
  需要一个球号,但**模型无从知道用户把 AD9833 接在哪个脚上**。这次没有把挑出来的球号
  写得像查证过一样 —— 在 `io.xdc` 里用注释显式写明「**不是从实际接线查来的,上板前请对
  J4 丝印核对一次**」,并附备选方案(J4 1/3/5,BANK13 纯扩展 IO)。
  `LLM-012` 的根因是"一次不充分的搜索换来一个全称否定结论";这次是它的正面对照:
  **宁可留一条待核实的注释,也不要让猜测看起来像查证。**
- **LLM-016 · 模型主动不替用户决策(与 LLM-013「零风险」对照)**:
  这次接入**必然**产生一个时序窗口 —— AD9833 约 200.8 ms 就在加激励,而 DAC1220 要 ~800 ms
  才可用,中间偏置未定义。这既是 `LLM-013` 早就标出的「上电顺序」待决项(D21),
  也直接关联清单第 510 行「栅极保护:上电顺序」。
  模型**照最简接法做(并行启动),但把窗口写进 `qrs.v` 注释点名,没有替用户选顺序。**
- **LLM-017 · `LLM-012` 的教训真的变成了事前动作(可写报告的"纠错闭环"样本)**:
  `LLM-012` 的根因是**位流没更新**,而模型当时把票押在"引脚接错"。
  教训被写成本项目的固定动作 ——「**上板现象与静态分析矛盾时,先确认"板上跑的是哪一版"**」。
  **本次(500 mV 直流)模型在无人提示的情况下,把它直接列为头号嫌疑第 ① 位**,
  理由具体且可执行:`V12/W14/V15` 是**刚加进 `io.xdc`** 的,没重跑综合→实现→生成位流,
  这三个球根本没被驱动。⇒ **可写进报告作"自我纠错轨迹"闭环的证据:失败 → 归因 → 固化为动作 → 下次生效。**
- **LLM-017 · 模型主动标注「我取不到」并拒绝自圆其说(与 `LLM-003`/`LLM-012` 反向对照)**:
  本次两处 ——
  ① 模块稳压器 `U2` 型号在 PDF 文字层取不到 ⇒ **不判断 5 V / 3.3 V,把球交回用户**(量阻抗 / 看丝印),
  并说明该判断**直接影响硬件安全**;
  ② 用户测到 **500 mV**,而手册中间电平是 **344 mV** ⇒ 模型**直接写「这个绝对值我解释不了」**,
  没有编一个自洽的故事把它圆过去,转而指出更可能的假象(**万用表 DC 挡测正常正弦会读成平均值**)。
  ⇒ 正对 B06「设计判断依据写进报告」。
- **LLM-017 · 「量具错」第一次被事前拦截(前 6 次全是跑完才发现,可写报告)**:
  `LLM-009/011/014/016` 的量具错都是**跑完才回头审判据**。
  这次在**用户还没做任何测量**时,模型就把「万用表 DC 挡看不清正弦」摆到最前,并给出正确测法
  (**示波器 + DC 耦合 + 0.5 µs/div**)。
  ⇒ 「**先证明量具可信,再相信结论**」从**事后归因**升级为**事前检查表**,可与前 4 次合流进同一节。
- **LLM-017 · 两条可直接写进报告的硬件铁律(均有一手出处)**:
  ① 🔴 **「板子有 5 V」≠「IO 是 5 V」** —— J4 第 40 脚能供 5 V,但
  **PL 三个 BANK 全部 3.3 V**(指南原文:「这 3 个 BANK 的供电均为 3.3V(即 PL 端所有 IO 的电平均为 3.3V)」)
  ⇒ **J4 上任何信号脚都不能承受 5 V**。这是接线章节必须写死的一条;
  ② **`MCLK` ≠ `SCLK`** —— 同一个器件上两个都叫 "clock" 的引脚职责完全不同
  (MCLK 定输出频率,SCLK 只搬数据);**判据不看名字,看模块自带例程驱动了哪几根线**
  (demo 只驱动 3 根 ⇒ 第 4 个时钟不存在于接口上)。可作`skill/clock-domain-is-not-clock-frequency/`的素材。
- **LLM-017 · 模型主动提前暴露下游风险而不打断当前节奏**:
  核对输出频率时,模型自己发现 **激励基准(AD9833 的 25 MHz 晶振)与采样基准(FPGA 的 MMCM)
  是两块独立晶振** ⇒ CLAUDE.md §八 的相干采样条件 `R/f_s = k/f_e` **要求严格整数比,
  不同源根本做不到**。模型**没有要求现在改**(D10 已判过板载晶振不可动),
  而是标记为 **P3「单通道 DLIA 对上 P0」必然撞上的墙**,并主动提议下一步量化
  (如 50 ppm 失配下 CIC 对 `2f_e` 的抑制还剩多少 dB)。
  ⇒ **暴露风险 + 给出量化路径 + 不打断当前阶段**,是"设计判断依据"类素材。

- **LLM-034 · 本项目最强的一条「量具错」—— 而且它骗过了三次交叉验证**:
  2026-10-01 存档断言「FCLK0 实测 41.6667 MHz / IO PLL = 1500 MHz」,并列了**三条证据**。
  本轮复核发现:**第 1、3 条根本是同一个东西**(`IO_PLL / DIV0 / DIV1`),而 `IO_PLL = 1500`
  是 **PYNQ 内部的一个假定**(它按 `PS_CLK = 50 MHz` 建模,本板实为 33.333);
  第 2 条只证明"寄存器被写了",**与 IO PLL 是多少无关**。
  ⇒ **三条证据 = 一个假定 + 一次写操作,一条独立的都没有。**
  **推翻方式**:帧率法 —— 帧几何是常量(`1344 × 635`),数出来的帧就是数出来的帧。
  **「证据条数」不是强度指标,独立性才是。**
  ⇒ 可与 `LLM-009/011/014/016/017/033` 合流成同一节:**「先证明量具可信,再相信结论」**。
- **LLM-034 · 「握手 ≠ 数据」—— 一个计数型探针分不开两种故障**:
  第 33 次位流测到 `beats = 153600`(**满帧**)就判"AXI 读通路正常,问题在读侧"。
  **错。** `DECERR` 时从设备回**全零数据**,而**握手照常完成** ⇒
  「收到 153600 拍零」与「收到 153600 拍真图像」**读数完全一样**。
  改法:把 `0x74` 拆成"拍数 + **内容证据位** + 响应码锁存" ⇒ **一次实测**分开两个假设。
  ⇒ 新候选 `skill/counter-cannot-separate-content-from-count/`(**加观测前先问:
  这个探针能区分我要区分的那两种情况吗**)。
- **LLM-034 · 用一个「必然可读」的地址把辩解封死**:
  地址扫描里特意放了 `0x00000000`(OCM)与 `0x40000000`(越界)两个**对照点**。
  若只扫 DDR 地址而全部 DECERR,仍可辩称"地址映射配错了";
  **OCM 那一行把这个辩解当场封死**。同时越界那一行证明"**界内界外读数一样**"
  ⇒ 该行**不能**用来证明"越界会 DECERR",**已在存档里自我标注**。
  ⇒ 归入「设计判断依据写进报告」(**B06**)。
- **LLM-034 · 模型把板子写挂死过 —— 教训进了 memory,却差点没进记录**:
  排障中写的 `t_rst.py` **写了 SLCR `FPGA_RST_CTRL`**,在 **PL 侧有活跃 AXI 主机**时拉 PL 复位,
  **锁死 HP 口并连带拖死 CPU**。教训当场落成 memory 与 `drivers/board.py` 的硬护栏
  (`_SLCR_FORBIDDEN`,命中即抛 `BoardError`),**但 LLM 记录里一直没写** —— 本轮补记。
  ⇒ **「错误当场修了」不等于「已归档」**;`report/llm-log/` 的漏记,本轮自查补上。
- **LLM-034 · 🔴 反面记录:根因未找到,且模型没有编故事**:
  `S_AXI_HP0` 为什么拒,本轮**无答案**。处理方式是把十条已排除项**连证据一起列出**,
  把剩下方向**排序成可执行清单**,并明确写"**下一动作是断电重启,不是改代码**"。
  **尤其标注了一条自己的证据可能不可靠**:「`FPGA_RST_CTRL` 读回干净」
  被用来排除"事故遗留复位",但**读回值干净 ≠ 硬件已复原** ⇒ 该条"排除"**保留**。
  ⇒ 这类**主动暴露自己证据链缺口**的处理,正是 B06 与「自我纠错轨迹」要的东西。

---

## 说明

- 单条记录模板见 [`_TEMPLATE.md`](_TEMPLATE.md)
- 完整模型回答存 `attachments/`
- 提炼出的可复用经验写入 `/skill/`
- **「纠错轨迹」是评分重点** —— 只记成功案例拿不到分

---

*最后更新:2026-10-10(第 55 次会话,`LLM-060` —— **用户一句话改了三件事 + 一份自用手册**;
原话「**总之现在以复刻出结果为导向,加分点再说,以后的代码编写仓库管理都是你负责了我就不写了,
然后你重新列一份给自己读的开发文档 …… 像是 ssh 密码这种东西直接写出来就不用每次重新找了**」;
**① 目标排序** = 「**复刻出结果**」优先于 §三 的评分权重,**加分项押后**(⚠️ 必做点仍一项不能缺);
**② 授权扩展** = 代码编写 + **仓库管理**全归 Claude ⇒ **`CLAUDE.md` §6.0.1「交付仓库默认只读」作废**(⛔ 物理动作仍归用户);
**③ 🔴 D30 推翻 D03** = 解调**不要求在 PL 算**,「ps 端能算出结果就行」⇒ **P3 判据改写,
C 侧解调从"替补"变"正选"**;⚠️ 同条登记两条评分项原文后果(`B03` 的「vs **硬件 DLIA**」少一条腿、
`M10` 的「**资源**对比表」纯 PS 侧填不了);
**④ 主产物 `docs/dev_handbook.md`(398 行)** = 板卡/网络/**SSH 口令**/**寄存器速查全表**/时钟口径/
**8 条安全红线**/**文件地图「我要 X → 去哪个文件」**/**13 条坑清单**/决策反转登记 ⇒
**会话开始只读这一份 + `project_state.md` §一,不再通读 4400 行**;⛔ **含口令,永不进交付仓库**;
**⑤ `tools/board_run.py`** = 持久版板上执行/传文件,替代放 `/tmp` 里**重启即丢**的一次性脚本;
★ **纠错轨迹最重一条**:我把 `CLAUDE.md` 的旧决策 D03 **当成现行约束**,据此报了"PL 分量缺一大块"这个
**已被用户勾掉的缺口** ⇒ 📌 **决策表记的是"当时的决定",不是"现在的约束"**)*

*上一版:2026-10-10(第 54 次会话,`LLM-059` —— **Q31 结案:根因不在 PL,在 PS 的一行 Python 写法**;
用户本轮的全部输入只有三个字「**上电了**」,排查由模型在板上自动完成;
★★★ **根因 = `struct.pack_into(fmt, mmap, off, v)` 对一个 Python `mmap` 会发出两笔 AXI 事务,
第一笔是常数 `0x00000000`** —— 它恰好在继电器白名单里(`COSS_DISCHARGE`)⇒ **被受理**、锁 40 ms busy
⇒ 第二笔真值撞 `ERR_BUSY(0x03)` ⇒ 硬件停在 `0x00`。**PL 与 SmartConnect 全部无辜。**
⇒ 修法一行:`drivers/relay.py::_DevMemRegs.write` 改**切片赋值**,`WORKAROUND_0x1C` 置 `False`(绕法退役);
**🔴 五条板上实测坐实**:公平四路(三对一错)/ **settle 置 0 后四种写法全对**(⇒ 问题是"笔数"不是"值送错")/
六组遍历(与基线无关、与载荷无关 ⇒ 第一笔是常数零,不是 RMW)/ 证人寄存器 `0xAA223344`(窄写 = RMW)/
**同进程"一行代码切换结果"**(7/7 通过 ⇄ 3/3 失败);
**回归**:`tools/relay_rw.py --diagnose` **两个后端各一遍均「全部通过」**(A 11/11 · B 3/3 · C 6/6 · D1 5/5 ·
**D2 6/6 —— RTL 的 `ERR_GEAR` 分支第一次被验证** · E 实测 40.2 / 100.4 ms);
**🔴 本轮纠错密度迄今最高(8 条)**,其中最贵的两条:① **把 `0x48` 当 SCRATCH**(它是 VERSION,只读恒 `0x00010000`)——
**表一直是对的,是我看错了**,浪费一轮上电;② **断言"两条后端映射属性不同、不能互相代表"** ——
去读 `pynq/pl_server/embedded_device.py:523` 实测**两条都是 `/dev/mem` + `O_SYNC`**,
**这条错判把方向引向了 ILA(要重建位流)**;另 `s_axi_wstrb` **全文只在端口声明里出现** ⇒ 从机忽略 WSTRB(我先前假设它受理);
⚠️ **顺带推翻 `LLM-058` 的一处自我怀疑** —— 它逐拍推演的"一笔一个脉冲"**本来就是对的**;
🆕 沉淀 [`skill/count-both-sides-of-interface/`](../../skill/count-both-sides-of-interface/skill.md)(已落地):
**判据 0 = 先问"我数的这个量,是在接口的哪一侧数的"** —— 本案里"一次写"**从来没被量过**,它被当成前提假设掉了;
⚠️ **记账漂移一处已就地更正**:统计块的「合计 = 56」在表内已有 58 行时仍未重跑,本轮重测 **22/36/0/1 = 59**。*

*上一版:2026-10-10(第 53 次会话,`LLM-058` —— **代码编写权移交后的第一次代写;★ 本轮最重的产出是一处"矛盾"而不是代码**;
用户原话「**后面代码都归你写,先关系统我断电,你写完再叫我上电测试**」⇒ **推翻 `CLAUDE.md` §6.0**
(2026-09-12 立、09-13 收紧的「代码一律用户先写」),但**物理动作(断电 / 接线 / 万用表 / 上电)仍归用户**;
产出 `drivers/relay.py`(继电器+挡位语义层,含 `0x1C` 绕法与**三个后端**)+ **拆掉 `tools/` 三处 `Overlay(download=True)`**
(原脚本每次运行都重加载位流,而 `lcd_fb_reader` 是活跃 AXI 主机 ⇒ **一次上电只允许加载一次位流**);
★★★ **收尾写状态文档时翻出两处记录互相矛盾**:第 52 次记录写着旧 `relay_rw.py` A 节「**7 个合法组合 7/7 相符**」,
而 A 节做的正是「**单写 `0x1C` = V → 读回 == V**」—— 这**正是异常文档断言不可能的写法**
⇒ **同一份位流、同一次上电,两种写法结果相反** ⇒ 异常**不是静态性质**;
★ **顺着矛盾比差异,发现两个变量同时变了**(旧 A 节**从没写过 `0x24`**,而异常记录全部出自
`poke_relay.py w 0x24 … 0x1C …` **总是把 `0x24` 紧挨着写在前面**;**后端也不同** —— PYNQ `MMIO` vs 裸 mmap)
⇒ 📌 **「找到一个差异」与「找到了原因」是两个断言**,差异 ≥ 2 时必须做**对照实验**;
★ Q31 第 3 组核对:BD / SmartConnect **四类「一变二」机制逐一排除**(1:1、同频、无位宽转换、单段 MMU),
⚠️ 但 SmartConnect 是**加密 IP**,读配置**证明不了**内部没有重发路径 —— 这条不确定性**原样保留**;
🆕 附带发现 **BD 给 4 K 映射而 slave 只译 8 位(256 B)** ⇒ 地址位 `[11:8]` 静默丢弃、`0x100–0xFFF` **全程混叠**;
⚠️ **本轮代码零上板**(用户已断电),上板对照(`tools/relay_rw.py` §0 的 cold 单写 vs pair 背靠背,×2 后端)
已备好待跑。*
📌 上一版:`2026-10-10(第 52 次,`LLM-056`/`LLM-057` —— 继电器矩阵落地 + 只读界面接通,首次发现 `0x1C` 异常)`

*上一版:2026-10-10(第 52 次会话,`LLM-056` —— **继电器矩阵 + 挡位选择落地,Q24 结案**;
★ 用户明确授权代写(**「你直接分配引脚并且接入ps端写完我上电你调试」**,§6.0 显式例外,仅限本次),
产出 `relay_ctrl.v` + `0x04/0x18/0x1C/0x24` 四个寄存器 + `cv_sys.v` 11 根端口 + `led.xdc` 11 行(**56 → 67**);
★ **BD 第 4 次撞 mref 坑,但这次每一步失败都留了证** —— `update_module_reference` **rc=1 且 errorInfo 为空**、
**Plan A(只清缓存)实测证伪**(`system.bd` 自己也存引脚快照)、**`connect_bd_net` 三种写法只有两引脚形式成立**
(全端点写法**静默失败**,错误现象延迟到 `validate_bd_design` 才出现,中间隔了 5 个步骤);
★ **三处静默陷阱在发出去之前被拦下**:`0x180` 掩码成 `0x00` 会变成一次**合法**的继电器动作、
`get_property CONNECTED` 恒为空不能当判据、`grep '>relay<'` 的模式假设错导致**差点把"查不到"当成"不存在"**;
★ 三条**偏离规格**的判断(**不插 AllOff 中间态 / 稳定时间用 ms 不用周期数 / `relay[6:0]` 7 位**)——
其中「先断后合」在 PL 上是**反的**(STM32 的 4 次 `BSRR` 写才有中间态,PL 同拍翻转没有);
⚠️ **位流已出(md5 `9613c292…`)但未上板,继电器极性未证实** ⇒ 本行记 ⚠️。
WNS **+1.490** / WHS **+0.080** / 失败端点 **0 / 5266**;LUT 1665 → **1715(3.22%)**、FF → **1993(1.87%)**、
**IOB 67(53.60%)** —— **IO 才是真瓶颈**,不是 LUT。
*本版:2026-10-24(第 70 次会话,`LLM-075` —— **仅开路校准:平行杂散校正 + 四挡一致性闸;并归因「校准开着扫压全无效」**;
用户不敢短接(源侧限流无记录),要求只做一两个标准;标准板未到,先做 OPEN。
★ **OPEN-only 只能做平行杂散校正**(`a=1,b=0,c=1/M_o`),不识别增益与串联项 —— 标准板到位后自动升级。
★ **实测把 OPEN 数据判死**:同一开路四挡 `5215 / 56041 / 505922 / 803522`,差 **150–1500 倍** ⇒ 是 `VL/VH ≈ 0.19 %` ≈ 1 LSB 的**通道地板**,不是夹具阻抗;能看见的 D 挡只折出 0.15–0.52 pF ⇒ **推翻 `LLM-073` 的「夹具并联 7–14 pF」**。
★ 新增 **四挡 3 倍一致性闸** + 导纳占比守卫;`Init()` 载入后重拟合,旧坏点下次启动即被清。
★ **「全无效」不是校准的错**:那轮 60 点全在 D 挡,`z_valid=1`、`判无效 0`;校准前 `14569+1303j` 已是正虚部,`Cs<0` 撞上 UI 的 **`Cs>0`** 显示判据(`ui_demo.c:8084`)。
★ **纠错**:①夹具 7–14 pF 被证伪;②"降校准偏压保护短接"不对(偏压是直流,交流幅度固定);③OPEN-only 对 CISS 只改善 **0.08 %**;④"全无效"第一假设(反演分母变号)错了,靠日志改判。
⚠️ **新闸未在板上生效**(进程仍是旧二进制,下一次启动才加载)。)
*上一版:2026-10-24(第 69 次会话,`LLM-074` —— **三标准校准:分式线性误差模型 + 可输入的标准板参数 + 校准页**;
用户只有两样能测的东西(**拔掉被测件 = OPEN**、**自制标准电容板 = LOAD**),本轮追加**0 Ω 短接**(D-G-S)并要求标准板参数**可输入**。
★ **模型选分式线性而不是"减偏移"**:`Z_m=(a·Z_d+b)/(c·Z_d+1)`,三个标准各钉一个参数;
**OPEN + SHORT 两个不够,必须三个** —— 退化情形降级到仿射/`b=0`,并在标志位里与"完整拟合"区分开。
★ **标准板按数据手册拆四腿**(CISS/COSS/CRSS/RG),用户输电容值、阻抗运行时现算 —— 拓扑还要改,输拓扑比输阻抗歧义小。
★ 本场最值钱的一条纠错:**宿主单测失败,根因是测试数据撞上了 `CAL_OPEN_MIN_RATIO=5` 守卫**(CRSS 腿开路/负载仅 3.4 倍),
**代码是对的、测试是错的** ⇒ 主用例换参数 + 单开守卫用例钉住"只有该腿掉精度",**没有去放宽守卫迁就测试**。
⚠️ **上板已验证**页面渲染 / 数值显示(修掉 `%f` → `f pF`)/ 键盘输入 / 落盘回读;**采集状态机(OPEN/SHORT/LOAD)未验**(用户断电重接外设)⇒ 本行仍记 ⚠️。
*上一版:2026-10-23(第 68 次会话,`LLM-073` —— **自动换挡改判据:`VL/VH` 反推 `|Z|` + 换挡后核对 + 一次只走一挡**;同场另有 `LLM-071`(「全部」页三卡无数据:更新函数存在但无人调用)与 `LLM-072`(十颗同型号器件的典型 C-V 参考模型接进结果页)。★ 用户原话:「**刚式了一下在合理的电压下切换应该没有问题电压较小的时候用高档位确实会测出负的**」。★ **数据**:同一 50 V CRSS 在 B/C/D 三挡 **Re Z = 1.85 k / 17.7 k / 175 k Ω,几乎严格 ∝ R0**,而 **Im Z 基本不动** ⇒ `Z = R0(VH−VL)/VL` 的实部是**测量链失配的镜像**,不是器件;负 Cs 是伪影压过真实小信号的结果。**机制未定** —— 两个最简模型(加性串扰 / 增益误差)都只解释得了 Re,故只记现象不编机制。★ **自我纠错**:2026-10-15 的「换挡判据本身没错,错的是脚下的 |Z|」**只对一半** —— 判据用的就是那条实部 ∝ R0 的 |Z|,它本身就是错的来源之一。★ **未验证**:改完时板子已断电(用户要求),换挡序列 / 核对拦截 / 负 Cs 是否消失全部待上电;1.5 倍核对阈值在 Coss 拐点是否成立亦无数据。)
*上一版版:2026-10-15(第 64 次会话,`LLM-069` —— **CRSS 高压端坏点的根因:从「采样刻度错一拍」改判到「ADC 真的过载了」(OTR 作证)**;
用户原话:「**我发现50附近测量crss确实容易出问题**」→「**我看到的调试界面就是下面那个曲线有波动不太干净的正弦波，你先分析一下这些坏点原因导致的**」。
★ **坏点在哪**(坏 = `ppA ≥ 2000 码`):干净 **213/245(86.9%)**;高压过载 **20 条全部在 CRSS**;
**CRSS@50.000 V 坏 11/15(73%)、CRSS@55.000 V 坏 8/9(89%)、CRSS@0.100 V 只 1/20(5%)、CISS 扫压 60 点全程 0%**
⇒ **回路与偏压两个变量都成立,不是单因子**(同一偏压 50 V 下 CISS 0% vs CRSS 73%;同一回路 CRSS 下 0.1 V 5% vs 50 V 73%)。
★ **指纹一:解调干净、峰峰炸** —— CRSS ≥39 V 采纳点的 A 路解调幅值 CV 仅 **0.14%**(0.3274 ± 0.0005 V),同期 ppA 在 1348→1354 跳。
8190 点相干解调对 1–10 个孤立样本几乎免疫(权重 0.12%),而峰峰被单个样本完全决定 ⇒ **是极少数样本的瞬时越界**,不是整段污染、不是算术错。
★ **指纹二:ppA 落在离散台阶上** —— 干净 1060–1640,另一簇 **2348–2787**。算给你看:干净信号跨 `[1377, 2719]`,
**一个**样本打到上轨 4095 ⇒ `pp = 4095 − 1377 = 2718`,实测那一簇 2731 / 2737 / 2740(**差 <1%**)⇒ **「恰好一个样本到轨」**;
`ppA ≈ 4095` 那簇 = 两端都到轨(12 位全刻度)。A 路正弦本身只有 **0.327 V**,够到 ±1.0 V 满量程必须叠一记 **±0.7 V** 的瞬变。
★ **A、B 一起胀 = 共模** —— CRSS@55 V 干净时 ppA/ppB = 1351 / **32**(B 路幅值只有 **5.7 码** = 0.28% 满量程),坏时 3968 / 3932;
20 条高压过载里 **18 条 B 也 ≥2000**。两路真实信号差 **118 倍**,一次瞬变却把两路顶到同量级 ⇒ 瞬变是两路共有的。
★★ **决定性证据 `adc_otr`** —— 它是 **AD9238 自己的输出引脚**(`Y8`,`led.xdc:130`;手册 *"Out-of-Range Indicator for Channel A/B"*,脚 58 / 脚 39),**不是 PL 算出来的**。
逐字 OTR 位(**2026-10-14 修好位序之后才有效**):**干净 213 条里 0 条置起(0.0%)**;高压过载 20 条里 **16 条**;`ppA ≥ 3900` 的 **13 条全部(100%)**。
数据总线是 12 根线、OTR 是**独立的第 13 根**,一根与数据无关的引脚跟着一起置起 ⇒ **不是数据总线被读错,是 ADC 自己判定输入越了量程**;
且 OTR 计数只 **1–10 / 16384 字**,而「刻度错一拍」会**每个周期都错(8190 次/采)** ⇒ **量级差三个数量级**。
★ **机制候选**:H2a **CRSS 组合下栅极浮空**(1 MHz 经 `Cgd` 整流给浮空节点充电到击穿放电)领先 —— 一次解释「只有 CRSS」+「只有高压」+「1–10 次/8192 的稀疏率」;
H2b 50 V 下触点微弧;H2c 偏压 ×10 放大器尖峰(**解释不了「只有 CRSS」** —— CISS/COSS 走同一套偏压);H1 采样刻度错一拍(**被削弱,但未排除**)。
★ **判别实验**:★(a) CRSS 夹具加一只 **1 kΩ 栅–源电阻**(一只电阻,判别力最强);(b) CRSS@50 V 关掉激励再采;
(c) **软件打印越界样本的序号 / 值 / 通道** —— 纯 C、板上 `make`、**不占位流周期**,是**唯一能把 H1 和 H2 分开的实验**。
⛔ **明确说了不要用放宽阈值的办法「修」** —— `142 / 208 / 362` 这种量级放宽度覆盖不了,只会把 519 pF 那种尖峰重新放回曲线上。**要修的是模拟侧,不是判据。**
★ 🔴 **本次自纠 8 条(全在 `LLM-069` 纠错节),最值钱的两条**:
① **差点拿一个归属逻辑错的脚本的数当结论** —— `/tmp/rate2.py` 用 `[proto] 点测 CISS @ 0.10 V` 这行归属回路,而它**在扫压期间会残留**,
把 227 次扫压捕捉全算到「CISS@0.10 V」名下,我据此写下了「**是路径依赖,不是偏压依赖**」。重算后 **CRSS@50 V 是 11/15 = 73%(不是 52/54)**,
且**回路与偏压两个变量都成立**。⇒ **归类要用「写下去会被回读确认」的状态行(`[relay] 回路 → X`),不能用「打印一句话表示意图」的行。前者是状态,后者是心情。**
② **差点把「界面上的正弦不干净」直接算成坏点造成的** —— 波形页只喂 **96 点**,而越界样本是 **1–10 / 8192**,
落进这个窗口的概率约 **1%–12%**,与「一直看着不干净」**对不上** ⇒ **如实标为「未查证」,没有为了让故事完整而把两者焊在一起**。
③ **两个 OTR 口径差点混成一个**(逐字位 vs `0x90[3]` 粘滞)—— `LLM-068` 里「正常点 4–5%」与本次 **0/213** 对不上,旧口径标存疑**不删**。
④ **同一份日志有 11 个 `c_ui 启动` 段、采样配置不全一样**(前 4 段 `0x0FF02` = 65.00 MSPS,后 7 段 `0x00104` = 32.50 MSPS)—— 245 条**逐条核过**才敢用。
⑤ **没把 H1(数字)从结论里删掉** —— `led.xdc:213`「ADC 并行数据与 `adc_otr` 的输入时序**有意不约束**」(杜邦线 tPD ~4 ns vs P=2 时 15.4 ns 眼宽)仍是事实,
所以结论写成「**压向模拟侧,不是焊死**」,而不是「已证明是模拟问题」。
⚠️ **本次一行代码没动**(用户要的是「分析原因」);⛔ **判别实验一个都没做**;⛔ 严格 A/B 仍未做;`~6 ms` 的采集耗时**仍是估计值**。
📌 统计重跑(**用本文件第 127 行的脚本实跑,不是手写累加**):`✅=24 / ⚠️=44 / ❌=0 / 无标记=1 / 合计=69`
(`LLM-069` 记 ⚠️ —— **根因压到模拟侧并给出三个判别实验,但一个实验都没做、H1 未排除,不升 ✅**;
`无标记` 仍只有 `LLM-037`,规则连续第十三轮生效)。
🔴 **又抓到一次记账漂移**:上一版 footer 一路写 `合计=64`(`LLM-068`),而**表里其实已有 68 行**
⇒ **少记了 4 行**(`LLM-065`–`068` 时只加了行、没重跑汇总,`✅` 虚少 3、`⚠️` 虚少 1)。
**本次三处对账自洽**:`ls LLM-*.md | wc -l = 69`、`grep -c '^| \[LLM-' = 69`、脚本 `合计 = 69`;
编号 `001`–`069` **连续无缺口**。
> ⚠️ **这是同一类毛病的第二次**(第一次见本文件第 147–151 行,`LLM-057/058` 那次)——
> **增量更新时容易只改行、忘了改汇总**。本次**没有把 24/44 改回 21/43 去迁就旧 footer**,而是以实跑为准并把漂移记下来。
> 📌 **另一个自己踩过的坑**:我**先按"上一版 64 + 1 = 65"手写了一个数**,实跑脚本后才发现是 69 ——
> **统计类数字一律现场跑,不要做增量算术。**
*上一版:2026-10-15(第 63 次会话,`LLM-068` —— **扫压末端尖峰:`整次采集混进坏样本` 才是根因(不是算术),用户口径「多测几次取合理值」的正确形态是"自洽闸 + 重采"而不是中位数**;
用户原话:「**我想知道什么原因，或者扫压的时候就是多测几次取合理值让结果更稳定行不行，就是尽量不要发生这种情况**」→「**按照最合理的方式进行修复**」。
★ **根因判据是一对互相矛盾的自测数**:同一次采集里 `幅值` 反推的峰峰 = **27 码**,而 `峰峰` 打出的是 **2588 码** —— **94 倍**;
另有一次 `A码 = B码 = 3205`(两路读到同一个字)的指纹 ⇒ 机制是**同一条 12 位总线上的时分字被错拍读到**(A 读到 B 的字 ⇒ `VH ≈ VL` ⇒ `Z ≈ 0` ⇒ Cs 爆掉),
只在 **CRSS 高压端**放大(`Cs ∝ 1/VL²`,那里 `VL = 23 码` = 0.56% 满量程,同样的毛刺落在 CISS 上动不了 1%)。
⚠️ **明说这是推断不是证明** —— 决定性实验(扫 `P=4` 的四个刻度看跑间稳定性)在界面活着时做不了(界面每次采集开头都重写 `0x88`)。
★ **回答"多测几次取合理值行不行":方向对,中位数不行** —— 坏点是**整次都错**,N 次取中位数会让**错的多数赢**;
⇒ 正确形态 = 先问"**这一次算不算数**"(物理不变式),不算数就**换一次重采**。**「合理」由不变式定义,不由投票定义。**
★ **重采可行性有实测**:同一偏压连测 ≥4 次的 **63 组,全部"有时标、有时不标",次次都标的一组都没有**。
★ **阈值从 2039–4462 个真实样本里选,不是拍的**:`OTR≠0` 单用只抓 36.9%;K=2.5/F32 全抓但误伤 9.2%;
★ **选定 K=3.0/FLOOR=64 → 抓 342/347(98.6%)、误伤 31/1692(1.8%)**。
★ 🔴🔴 **本轮最值钱的两条纠错**:① **差点把阈值按理论定死而非按数据定** —— 直觉说"K=3 在 A 路近乎满量程、等于没拦",
**照这个直觉调小会判掉大量好点**;实测 A 路最大比值只有 **3.71** ⇒ K=3.0 在 A 路完全安全。
② **第一版方案过大,写之前自己砍掉** —— 原设计"N 次取中位数 + 第二对 64 KB 暂存 + memcpy",而自洽闸已挡掉整次坏采集、
活下来的点只差 ~0.4%,**重采到第一次自洽不需要任何额外交互缓冲**。
★ 🔴 **交付仓库同步踩到方向陷阱**:`rsync` 报"有差异"**不指示方向** —— `src/rtl/` 工作区是 **9 月残本 5 个文件**、仓库才是**当前真本 18 个**;
`board/qrs.bit` 工作区 `232c649d…`(**10-14**)vs 仓库 `c92c7ed5…`(**10-15 第 61 次,现行**);
`report/measurements/20261005_*.md` 两边**只差路径前缀**。**三处都是照它同步就会把仓库打回旧版**,逐个核过 md5/内容/时间戳才动手。
★ ⚠️ **板卡目录结构第 5 次猜错**:C 源在 **`/home/xilinx/c_ui_src/app/`**,`/home/xilinx/cv/` 是**纯 Python 树**(`qrs-ui.service` 的 `WorkingDirectory`)。
★★★ **上板实证(同日晚,第 63 次会话续)** —— **尖峰消失,曲线恢复单调**:
同一台设备、同批偏压,改前有三个点**逆着趋势往上跳**(`Cs = 21.26 / 519.52 / 13.43 pF`,而 CRSS 高压端本应单调下降),
改后落在 **15.57 / 14.74 / 12.31 pF**;**`519.52 pF` 就是用户最初报告的那根尖峰**。
**判读用「单调性」不用「插值」—— 插值会循环论证**(拿改后的 49.53 去验改后的 44.06)。⚠️ 也不能说"改后这三个数就是真值",只能说"**不再逆趋势**"。
`码/期望` **max 167.04 → 7.54**;比值 > 10 的点 **5 → 0**、> 50 的 **3 → 0**。
账本原文 `实际采 223 / 拒收 36 / 救回 15 / 判无效 5`。完整记录:`report/measurements/20261015_sweep_consistency_gate_verified.md`。
⚠️ **同时暴露两件必须说清的代价**:① **CRSS 腿 5/60 点被判无读数**(8.3%),其中 **4 点在 0.2–1.3 V 陡峭段**(`码/期望` 只 4–6)
⇒ 那是判据的"过度收紧区";**只能说"判据把这 4 个点判没了",不能说"这 4 个点本来就该丢"**。
② **账本那四个数是进程累计、不是本轮** —— `s_adc_rej/s_rescued/s_void` 只在 `pynq_protocol.c:1322` 进程初始化时清零,
`sweep_start()` 里**没有**清零。📌 **对账**:扫压内可数 **211** 次 + 扫压前 **11** 条单点证据行 = **222**,账本说 **223** ⇒ **差 1,未解释**。
**这是我的日志措辞缺陷,已如实告知用户,未修。**
★ 分析日志时又自纠两条:③ **搜错了方向** —— 从 `⛔` 行**向前**找证据行拿到的是**上一个点**的数,
根因是 `⛔` 由 `adc_capture_pair()` 内部打印、证据行 `adc_log_pair()` 由调用方在其返回**之后**打印;
**先查调用顺序再定搜索方向,别按"先因后果"的直觉排布**。
⛔ **严格 A/B 仍未做** —— 上面是"先后两轮同参数",不是同时刻交替;真对照要改 `qrs-ui.service` 的 `Environment`
= **重启服务 = 第二次加载位流**(红线)⇒ **只能等下一个上电周期**。`~6 ms` 的采集耗时**仍是估计值**。
📌 统计重跑:`✅=21 / ⚠️=42 / ❌=0 / 无标记=1 / 合计=64`(`LLM-068` 记 ⚠️ —— **判据已在板上证实有效,
但"严格 A/B"与"占位代价的定性"两件未做完,不升 ✅**;
`无标记` 仍只有 `LLM-037`,规则连续第十二轮生效))*
*上一版:2026-10-15(第 62 次会话,`LLM-067` —— **上板核验复位值 ✅,但「PL 空着没有」的判据我自己也是错的;Python 侧还有第三处 DF=1;恒定 5 V 未结案**;
用户原话:「**上电了，尝试设置一个电压测试**」→「**你能读到芯片的寄存器值吗，现在还是5v感觉硬件有问题**」→「**应该是dac坏了，先关系统吧**」→「**恒5v**」。
★ ✅ **上次那处修复在板上验过了** —— 位流刚配好、`apply_fclk0()`/`release_dac()` **之前**读 `0x14` = **`0x00000000`**,
正是 DF=0 下的 0 V(**若还是旧位流会读成 `0x80000` = 栅极 −100 V**)。同批快照:`RANGE_SEL = 0x02`(B 挡)/ `RELAY_CFG = 0x10`(`ALL_OFF`)/
`CV_CTRL` `0x00 → 0x01` / `SETTLE_CFG = 0x002805DC` / `VERSION = 0x00010000`;`apply_fclk0()` 实测 **50.0000 MHz**。
★ ⛔ **「能不能读 DAC 芯片的寄存器」= 不能,原因在 RTL 里**:`dac_1220.v` 例化 `spi_master` 时 `miso(1'd0)`、`rx_data()` 悬空
⇒ 文档里挂着的两项「CMR 那次写落没落进芯片 / 自校准跑没跑」**用现有位流永远结不了案**(要么改 RTL 出新位流,要么上逻辑分析仪)。
★ 🆕 **两个新工具把「改电压」的代价从"一次位流"降到"零"**:`tools/bias_set.py`(本周期**唯一一次** `Overlay()` + 读复位值 + 配 FCLK0 + 放行 `0x84[0]`)
/ `tools/bias_mmap.py`(**裸 mmap,一次都不加载**;开头自己查 devcfg,PL 空着就**拒绝运行**,因为空 PL 上读 `0x43C00000` 会把 CPU 挂在 AXI 上)。
★ 🔴 **数字侧全对、表针不动**:五点写 `0x00000 / 0x26666 / 0x00000 / 0xD9999 / 0x00000` **逐点回读 ✅**,
而用户读数 **恒 5 V、全程没动**。⚠️ 同一个码 `0x26666` 在 **2026-10-12 量到 +2.80 V** ⇒ **这不像"公式错了",像"链子断了"**;
DF=0 下模块满量程 −10…+10 V、码 `0x00000` 对应 0 V,+5 V 只在码 `0x40000` 附近 —— **寄存器里没有这个值**。
★ 🔴🔴 **纠错四条(前三条全是"没实测就断言")**:
① 我第一版 `pl_state.py` 读的 `PCAP_STATUS(0xF80070C4)` **根本不是 devcfg 的寄存器** ⇒ 恒读 0 ⇒ 在**已配置**的 PL 上误报「⛔ PL 是空的」;
② 改读 `STATUS(0x14)` 之后我又用了**背来的**判据 `CFG_RESET_B==0 && GPWRDWN_B==0` —— **实测这两位加载前后都读 1、根本不区分** ⇒
判据改成**在同一块板上加载前后各读一次**校准出来的 **`bit10 PSS_FST_CFG_B`**(空 PL `0x40000A30` / 已配置 `0x50000F30`);
⚠️ `INT_STS` bit2 只测了"加载后"那一列,已如实标注**不算已验证的判据**;
③ **板上既有的 `tools/step1_plstate.py`(我 2026-10-03 写的)把 `STATUS` bit5 标成 `PCFG_DONE`** —— 厂商头文件原文是 `PSS_CFG_RESET_B`,
真正的 `PCFG_DONE` 在 **`INT_STS(0x0C)` bit2**(`XDCFG_IXR_PCFG_DONE_MASK = 0x04`)⇒ 那个"安全探针"**在空 PL 上也会回「已配置 ✅」**,恰在最危险的场合失效,已就地更正并注释留痕;
④ 🔴 **`cv_regs.py` 是同一个 DF 位的第三处落点**(第一处 `dac_1220.v` 的 CMR、第二处 RTL 复位值都已修):
`bias_volts_to_code(0.0)` 会算出 `0x80000` = **−100 V**(**"要 0 V,给 −100 V"**)⇒ 已按 DF=0 重写(`BIAS_CODE_ROT` / `BIAS_CODE_ZERO`)+ 主机端五点自检;
⇒ 📌 **教训:改一个"语义位"必须全仓搜"所有算这个物理量"的地方,不是"我想到的那处"**。
★ ⚠️ **我在板上又建了一棵重复的树**(推 `src/python` 到 `/home/xilinx/qrs_py`,而 `/home/xilinx/cv` 早有一份逐字节相同的)= **同型错误第 4 次**
⇒ **现在一共三棵树:工作区 / 交付仓库 / 板上 `/home/xilinx/cv`**(后者才是 `qrs-ui.service` 的 `WorkingDirectory`)。
★ ⚠️ **板上 Python 的三件套前提**(我自己先失败两轮才跑通):cwd `/home/xilinx/cv` + 解释器 `/usr/local/share/pynq-venv/bin/python3`(不是 `/usr/bin/python3`)+ `XILINX_XRT=/usr`;
`/sys/class/fpga_manager/fpga0/state` 在这块板上**恒为 `unknown`**(PYNQ 走 `/dev/xdevcfg`/XRT,不经过 fpga_manager)⇒ **它答不了"PL 配没配"**。
★ ⚠️ **仓库漂移(未修)**:`src/rtl/dac_1220.v` 仍是 **DF=1** 的 `0x20A0/0x20A1`,而工程里已是 `0x2080/0x2081`;`swm.v` 两份不同;
**`src/rtl/` 只有 4/18 个模块**(其余 14 个只在 Vivado 工程里)= **M1「完整工程文件与源码」的缺口**。
⚠️ **恒定 5 V 的三种可能一条都没排除**(DAC 芯片/模块坏 / SPI 接线动过 / 模块模拟级坏)⇒ 下个上电周期**第一步不是加载位流,
是拿表量模块的三个点:基准脚(期望 ≈2.5 V)/ 供电脚 / 输出脚** —— 基准脚本身就是 5 V 的话,故障在模拟级不在 DAC。
⛔ **真断电 / 接线 / 万用表仍归用户 —— 我当不了仪表。**
📌 统计重跑:`✅=21 / ⚠️=41 / ❌=0 / 无标记=1 / 合计=63`(`LLM-067` 记 ⚠️ —— **复位值核验干净通过且有板上读数,
但恒定 5 V 的根因一条都没排除,且本轮我自己的两个错都属于"结论听起来像事实、其实没实测"**;
`无标记` 仍只有 `LLM-037`,规则连续第十一轮生效))*
*上一版:2026-10-15(第 61 次会话,`LLM-066` —— **DAC 上电默认电压追查 ⇒ 查实 `0x14` 复位值在 DF=0 下是 −100 V(安全项),已改 + 重综合**;
用户原话:「**dac上电后默认电压是多少，我发现dac一直输出5v**」→「**把pl端默认值给改了重新编译**」。
★ **芯片层默认 = 0 V,有出处**:TI SBAS082G Table 13 `DF` 默认 = 0、p17「After reset, the DIR contains zero.」、
bit9 `CRST` 说明原话「**If DF is 1, zero gives 0V; if DF is 0, zero gives VREF (mid-scale)**」⇒ DIR=0 ⇒ 模块 0 V。
★ 🔴 **真发现**:`cv_axi_lite_slave.v:514` 的 `32'h0008_0000` 依据的是**旧 DF=1 传递式**,
而 `dac_1220.v` 的 CMR 已于 2026-10-12 转成 **DF=0** ⇒ 同一个码**翻到量程另一端**,
`0x80000` 由 0 V 变成 **−10 V ⇒ 栅极 −100 V**,恰是那段注释自己写着要避开的 `Vgs` 最大端;
⑤ 好消息:该复位值只经「上电自动发一次 `bias_set`」下发,而该脉冲**门控在 `dac_init_done`**
⇒ **被消费时 DF=0 是确定的**(CMR 写就在同一条初始化序列里)—— 错的只是那段注释的算术。
★ **修复 + 重综合(用户指令)**:`32'h0008_0000` → **`32'h0000_0000`**;另按既有纪律**改齐 4 处同源假话**
(`docs/dac1220_interface.md:237` / `report/source_design_checklist.md:417` / `docs/dev_handbook.md:183` + 交付镜像)。
🔴 **纠错 ④(重)**:我先断言「工程里原先没有综合脚本」(依据是上次用的 `/tmp/q_pull.tcl` 已随重启丢失),
**但只在工作区里找了一遍**,于是新写了 `build/run_impl.tcl` 跑出位流;
事后才发现**交付仓库里 `build/build_bitstream.tcl` 早就在**(LLM-062 引用过它产出的 `build/summary.txt`),
功能完全等价 ⇒ **已删重复件、用既有脚本把整条链重跑一遍**
(教训:**两棵树 `src/` `build/` `sim/` 是对称的,一棵树里找不到 ≠ 另一棵也没有**)。
新位流 **md5 `c92c7ed5b7d695dee73d86abc1c6089c`**(旧 `232c649d0cc143c12c87f17365f6ddd3`;
第一遍重复件产出的是 `98318ad0…`,已弃用);
**WNS +0.316 / WHS +0.065 / TNS·THS 0.000 / 失败端点 0 of 5472**;
**LUT 1790(3.36%)/ FF 2022(1.90%)/ BRAM 7.5(5.36%)/ DSP 0 / IOB 67(53.60%)**、**DRC ERROR 0**、`.hwh` 未变。
★ **纠错 3 条,最重的是第 2 条**:`qrs.v:75` 的硬编码 `20'hC0000` 在 DF=1 下**精确等于 +5 V**,
而且**就在同一目录**,几乎就是答案 —— **是靠 `.hwh` 的 `MODTYPE` 列表把它否掉的**
(两棵树都只有 `cv_sys / proc_sys_reset / processing_system7 / smartconnect`)
⇒ **"看着最像"的线索必须先证伪再往下报**;另一条:**凡引用 2026-10-12 之前的码↔电压读数,必须先确认属于哪个 DF**。
⚠️ **位流未上板**(板子关机中)、**用户的 +5 V 仍未定位**(需他回答"量的是哪两点 / 恒不恒定");
⛔ **真断电 / 接线 / 万用表仍归用户 —— 我当不了仪表**。
📌 统计重跑:`✅=21 / ⚠️=40 / ❌=0 / 无标记=1 / 合计=62`(`LLM-066` 记 ⚠️ ——
**查证与修复完成、新位流时序与 DRC 全过,但从未上板**;`无标记` 仍只有 `LLM-037`,规则连续第十轮生效))*

*上一版:2026-10-15(第 60 次会话,`LLM-065` —— **切回固定挡位 + 上板坐实「数据很奇怪」的根因是 CRSS 腿模拟前端饱和**;
用户原话:「**还是很奇怪，要不先切换会固定档位的模式把**」→「**自己重启把**」→「**先关系统吧**」。
★ **回退用文件开关、不用环境变量** —— 环境变量要重启服务才生效,而**重启 `qrs-ui.service` = 第二次 `Overlay()` = 红线**;
`/home/xilinx/sweep_auto_gear` 在 `sweep_start()` 时**现读不缓存** ⇒「改文件 → 下一场扫压」即生效,代价为零。
**判据一行没改、`sim/gear_pick_verification.md` 那张表全部仍然有效** —— 被推翻的是「现在就该启用它」,不是判据的算术。
★ **上板三项全过**:① 固定挡位 60 点**逐点**印 `挡位 0x2`;② 三腿继电器**真的逐腿切**(`[relay] 回路 → CISS(0x7F) → RG(0x3C) → CISS(0x7F) → COSS(0x61) → CRSS(0x51)`);③ 偏压归零修复**冷启动即 `0x14 = 0x00000000`**。
★ 🔴🔴 **核心发现 —— 「数据很奇怪」的根因是 CRSS 腿两路 ADC 都顶到满量程**:
**CISS**(A 1199–3516 / B 943–3516 / OTR **1/72**)与 **COSS**(1636–2876 / 133–3569 / OTR **1/60**)两条腿干净,
而 **CRSS 腿 A 2815–4095 / B 3213–4095 / OTR 非零 58/60(最大 43)**,`4095` = **12 bit 满幅**;
**B 通道基波塌到 0.002–0.008 V 而原始峰峰是满量程** ⇒ **信号没了只剩削顶**,`Z` 飞出 2955 Ω → **127 kΩ**、`Cs` 出 **−210.7 / 507.5 / 2.56 pF** 这类非物理值。
⚠️ **成因未定位** —— 继电器组合?前端增益?DUT 侧耦合?**需夹具原理图**。
★ **两条被推翻的旧结论(本轮最有价值的部分)**:① `LLM-064` 的「自动换挡让数据变好」**不成立**(它把唯一一条随偏压变化的曲线从 B 顶到 D 后毁掉);② 「接线整理后四挡读数一致了」**是虚的** —— 那场**偏压是关的**(偏压关 3504.8/4750.2 pF ⇄ 偏压开 2055.4/118 pF,而整理接线**之前**偏压开是 2055.5/114 pF)⇒ **接线整理没有改变任何东西**。
★ **纠错 5 条**,最重的一条是**「两个变量一起动时,任何'看起来更好'都不构成证据」** —— 而且推翻它的证据**本来就在我自己的日志里**,不是靠新做一次实验拿到的。
📌 运维形态一条可复用:**当"重启进程"本身是红线时,把"编译期决定"改成"运行时现读",代价从「一个上电周期」降到「零」。**
📌 日志存档 `report/measurements/20261015_uilog_fixed_gear_sweep.txt`(md5 `8c17256b…`,6103 行);
**前 4141 行 md5 与上一份存档逐字节相同** ⇒ 追加不截断这条性质**实测**成立,历史行号引用不会失效。
📌 统计重跑:`✅=21 / ⚠️=39 / ❌=0 / 无标记=1 / 合计=61`(`LLM-065` 记 ⚠️ ——
**三项上板确证、但 CRSS 饱和只坐实了症状、成因未定位**;`无标记` 仍只有 `LLM-037`,规则连续第九轮生效))*

*上一版:2026-10-15(第 59 次会话,`LLM-064` —— **扫压收尾不归零偏压 + 扫压不会自动换挡**;
用户原话:「**扫完了，吧偏压调回0,它这扫完都没有置零**」→「**先把档位修复了不然数据都很奇怪**」。
★ **偏压那条先证伪再定位,没先改代码**:`0x14` 当场读回 **`0x0007FFFF`** = **+100.000 V**(DUT 栅极顶在满扫压上,不是偏移也不是近零残差);
两处成因**性质不同** —— `sweep_finish()` 是纯疏忽(**完全没写 `0x14`**),`RequestSweepStop()` 打印的「偏压保持在当前点」**是当初写下的设计意图**,一并推翻但要说明理由;
收尾走 `bias_apply(0.0f)` 而**不是**直接写寄存器 —— 顶栏「偏压:」只有这条路径经 `bias_announce()` 更新,绕过它会**硬件归零而界面仍显示 100.00 V**,比不归零更难查。
★ **第二条先把"数据奇怪"证成量程问题、不是器件属性**:整段 CRSS 腿固定 B 挡 100 Ω 而 `|Z|` 从 **363 Ω 走到 15 313 Ω** ⇒ `VL/VH` 由 23.2 % 塌到 **0.64 %**(60 点里 **21 点 <5 %**)、`Cs` 在 6.77→13.66 V **一个倍压之内掉 14 倍**(434→31 pF);`VL` 是分母 ⇒ 其相对误差被 **`|Z|/R0`** 放大(100 V 处 **153 倍**)直进 `Cs`。
★ **判据是推的不是试的**:两个方向相反的误差来源(`|Z|≫R0` 放大 `VL` / `|Z|≪R0` 放大 `VH−VL`)在 **`|Z| = R0`** 处相等 ⇒ 最优点 `VL/VH ≈ 70.7 %`;等比挡位 ⇒ **log 轴取最近**;边界噪声 ⇒ **0.15 十倍程迟滞**(每跳一次曲线就换一次标定基础)。
★ **负对照是本轮最有说服力的一条**:同一份**实测**数据上「固定 B 挡」最坏 `VL/VH` **0.65 %** ⇒ 证明判据**有失败能力**,不是"怎么都能过";迟滞的直接测试(停在 B/C 分界 316.2 Ω 晃 ±5 %,40 次)**39/40 → 0/40**,同代码同输入只改迟滞宽度。
★ ⚠️ **两条修复都进了板上二进制,但都没上板验过** —— 本轮最后一个上电周期以用户「**先关一下系统吧**」结束,**从未跑过一次带自动换挡的扫压**;`shutdown -h now` 已执行(ssh 2.8 s 断开 + ping 15 s 无响应),**断电由用户执行**。
📌 统计重跑:`✅=21 / ⚠️=38 / ❌=0 / 无标记=1 / 合计=60`(`LLM-064` 与 `LLM-063` 均记 ⚠️ —— **代码侧落地、自检通过,上板判据待下一个上电周期**;`无标记` 仍只有 `LLM-037`,规则连续第八轮生效))*

*上一版:2026-10-15(第 58 次会话,`LLM-063` —— **扫压「看着不对」的三个独立成因 + 采样刻度 DIV 2 → 4**;
用户原话:「**我刚刚开启了偏压测量一遍后面又关了,你看看结果有没有问题,感觉跟之前不太一样是不是档位没法自动调节的问题,没法调档位应该只是测量不准不会烧lcr电桥吧**」
⇒ 拍板:「**这个修复吧,档位先别动等我连上档位控制引脚,然后如果你用不上开发板就先关系统**」。
★ **成因有三个,不是一个**(逐条有日志证据):① 档位卡 B(100 Ω)⇒ 偏压 >11 V 后 `|VL|` 塌到 9–40 码、`Re Z` 随机散(受控条件下同一量 SD 只有 0.44%);
② **DIV=2 的边缘采样仍在翻 B 的总线位** —— `A/INA` 幅值 0.2706/0.2705/0.2710 V **平滑**,而峰峰 1286→1289→**1535** 码(跳幅正好 ±1024/±2048),`B/INB` 峰峰在 400↔2500 间跳;
③ 低偏压头部 0.100→0.112 V 的 Cs 从 695→1755 pF 跳变。**自动挡与手动挡都不存在**(日志原文「档位 0x18 仍只读回显」)。
★ **用户的安全疑问:卡档位不会烧 LCR 电桥** —— 挡位只选四个参考电阻之一(10 Ω/100 Ω/1 kΩ/10 kΩ),**不碰增益、不碰供电**;
B 挡日志电流最大 **2.45 mA**,即使 DUT 短路也只有几 mA;唯一要当心的是 A 挡(10 Ω,电流约大 10 倍)。
⚠️ **如实标注**:夹具原理图不在团队手上,所以**下游模拟开关的保护我无法核实**,这一条只能由用户对硬件确认;
同时排除一个疑似偏压缩放 bug(`pynq_regs.h:325-328` 记着外部 ×10 级 ⇒ 90.000 V 就是**真**栅压)。
★ **改 DIV(纯 C 一行,不重下位流)**:`CV_ADC_DIV_DEFAULT 2u → 4u` ——
P=2 时整张刻度表只有 `{−1, 0}` 两点,**贴死 `adc_clk` 两个边沿、零建立裕量**;
P=4 的表是 `{−2,−1,0,+1}`,`cv_adc_phase_center(4)` 折回到 **+1** ⇒ A 落 tick+1、B 落 tick−1,**各离边沿 7.69 ns**(一个 130 MHz tick)。
★ **同一次运行内的前后对照**(不是两次不同实验:回路 CRSS / 挡位 B / 1 MHz / 继电器与 DAC 全程不动 / 每档连采 30 次;唯一变量是 DIV):
`2:−1` @65.0 MSPS ⇒ Cs **2115.90±81.66 pF(3.86%)**;`4:+1` @32.5 MSPS ⇒ **1844.19±7.79 pF(0.42%)** ⇒ **重复性约 10 倍**;
⚠️ 代价 = Cs 均值**偏 −12.8%**,**历史 65 MSPS 那批读数不能再与之后的直接比**。
★ **落地验证**:主机算得 `0x88 = 0x00000104`(DIV4 / PH+1)、`f_s = 32 500 000 Hz`、tick +1 落在合法区;
三档相干窗(1 MHz→252/8190、100 kHz→25/8125、10 kHz→2/6500)重算全部仍成立;板上重编 `c_ui` **exit 0**(唯一 warning 出在**没动过**的 `ui/ui_demo.c:1805`);
新二进制用 `grep -a` 查 UTF-8 裸字节核对(新串 1 次 / 旧串 **0** 次),md5 **`7932dd2f…`**。
⛔ **新默认要等下一个上电周期才生效** —— 重启 `qrs-ui.service` 会再调一次 `Overlay()` = **第二次加载位流**,是红线。
★ 🔴 **纠错 4 条,其中两条同源**:① 我把**修复前与修复后的两批数混着比**(两个变量一起动,证明不了 DIV 有用);
② `strings | grep '中文'` 得 0 并**当成证据**(GNU `strings` 只出 ASCII,这个 0 是必然的);
③ **从错的那棵树推文件到板子** —— 主机上 `src/c_ui/app/` 有**两份**(工作树 / 交付仓库),6 个文件推完 md5 **全部对不上**,
而 `LLM-062` 的验证表里**已经写过同一条** ⇒ **记录过 ≠ 防住**;
④ `LLM-062` 与页脚记的位流 md5 是**同一夜的中间产物** —— 三次构建**大小完全相同(4045690 B)**,**只有 md5 分得开**,
而工作区 `board/qrs.bit` 停在 `182e8248…`(交付仓库那份才是对的)⇒ **谁从工作区部署就加载错版位流**。
⇒ 沉淀:**改前用绝对路径编辑 / 推完回读远端 md5 / 二进制产物只认 md5**;
`skill/silent-capacity-gates/` 追加一族「**工具自报成功 · 工具静默过滤**也是一道静默闸」,并立候选 `skill/two-trees-one-name/`。
**离场**:继电器已回 `ALL_OFF(0x10)`(`err=NONE`)、`sync` + `shutdown -h now`、**ping 已不通**;
位流 **`232c649d0cc143c12c87f17365f6ddd3`**(本周期未再加载),`.hwh` **逐字节未变**)*

*上一版:2026-10-14(第 57 次会话,`LLM-062` —— **「切换测量项目回路没有切换」的根因:一处从未落地的设计;并把 VH/VL 相位打通**;
用户原话:「**那就先恢复回路的问题,为什么我切换测量项目回路没有相应的切换**」⇒(连问四轮「为什么做不到」)⇒「**把系统关了,把项目修复完整再上电**」。
★ **根因不是 bug,是一处从未落地的设计**:`0x1C` 三处能写的地方**一处都没写** ——
PL `relay_ctrl.v` 已实现且 10-10 上板验证过;**C 界面文件头自述「只读,一个寄存器都不写」**;
Python `drivers/relay.py` 写得完整、实测过,但 **`main.py` 零调用方** ⇒ 硬件永远停在复位值 `ALL_OFF(0x10)`。
📌 **顶栏那一栏没说谎(它读硬件真值),说谎的是按钮高亮 —— 按下去只换了个颜色。**
★ **连带结清四处「假装」**(全部有代码注释自认):「全部」单点把一份读数**填进三张卡** · 「先测 RG 再扫压」**RG 根本没测** · RG 单点卡片留 `--` · 扫压把同一个 `cs_pf` **写进三条曲线**。
★ ⭐ **相位为什么原来不通 —— 用户第四问「ADC 是同步采样为什么不是统一相位」把我推对了**:
AD9238 **在同一个转换沿同时采 A/B**,这对同步样本就躺在**同一条总线**上(tick −1 与 tick 0 就是同一个周期的两个半周期),
而旧 `samp_en = (tick == ph_off)` **每周期只命中一个 tick ⇒ B 被扔掉**,要 B 只能**再发一次 GO**(一万多个周期之后);
两次采集的解调基准锚在**各自第 0 点** ⇒ `∠Z` 里混进软件不知道的 `2π·f_e·(t_B−t_A)`。1 MHz 下要 1° 精度需时间准到 **2.8 ns**,
而 AD9833 是**板载 25 MHz 晶振、与 FPGA 不同源** ⇒ **任何"事后补时间差"的方案都不成立**。
⇒ **不是 ADC 不同步,是采集器没接那股同步数据 —— 它就在线上,一个 tick 之遥。**
★ **改法**:一次 GO 里 A 窗、B 窗各写一次(偶地址 A / 奇地址 B),缓冲 8192 → **16384 字**(每通道仍 8192 点 ⇒ **窗长表一字未改**)。
★ **算式照抄前序工程**(`~/Desktop/qrs`),不自己推:`Z = R0·(VH−VL)/VL`(复数)→ `Rg = Re Z + 0.7`、`Cs = −1e12/(2πf·Im Z)`;`R0` 从 `0x18` 回读。
★ **建了可复现构建工具**:`build/build_bitstream.tcl`(**完整重建,必须 `reset_run`**) + `build/report_summary.tcl`(**可单独跑**)。
⚠️ **本轮的两条"未验证"如实挂账**:回路切换**没听到过咔哒声**(L1 判据上电后才做)、相位的**稳定性未实测**;RG **未与万用表比对**(我当不了仪表)。
⚠️ **`REQP-1840` ×20 新增**(`u_adc_capture/cap_mem_reg_6`,RAMB18)—— 缓冲加倍逼出的,**判断为本轮不改**(复位写法规范;ADC 域只剩 0.316 ns 余量,不拿时序冒险),**升格条件**:采集数据出不明损坏时它是第一嫌疑。
位流 md5 **`232c649d0cc143c12c87f17365f6ddd3`**(旧 `d4d5538e…`;⚠️ **原记 `182e8248…` 是同一夜的中间产物,2026-10-15 就地更正,见 `LLM-063` 纠错 ④**),
`.hwh` **逐字节未变**;**WNS +0.316**(全落在 `clk_adc_raw`)/ WHS +0.065 / 失败端点 **0 / 5472**;
**1790 LUT(3.36%)/ 2022 FF(1.90%)/ 7.5 BRAM(5.36%)/ 0 DSP / 67 IOB(53.60%)**)
📌 **上面四个数取的是「部署版」那一次构建**(`LLM-063` 纠错 ⑥):那一夜跑了**三次完整构建**,
源没变(只改 C 侧刻度),但 P&R 不是逐比特可复现 ⇒ 第 1 次是 `WNS +0.354 / 1789 LUT / 0 of 5469`、
第 3 次(部署版 `232c649d…`)是 `+0.316 / 1790 / 0 of 5472`。
**凡引用"10-14 那版"的性能与资源,一律以部署版为准**;`build/` 下 4 份 `.rpt` 与 `summary.txt` 已换成部署版那一批。
⛔ **本块原写的「三次构建资源数完全相同」是没验过的断言,2026-10-15 由 `diff` 当场否掉,已删。**

*上一版:2026-10-12(第 56 次会话,`LLM-061` —— **DAC1220 偏压「设多少都读出 −10 V」的根因 = 偏移二进制补码 vs 直通二进制**;
★ 六点实测**全部只吻合补码、全部否定直通**,其中 `0x0CCCC` 是**样本外**点(先预测 +0.823 V,后读数 820 mV);
★ **改 RTL 而非只改软件的论证** —— 文档里 2026-09-19 那三点**精确落在直通二进制**上 ⇒ **两次上电之间 CMR.DF 变了**,
而 CMR 易失且那次写**落没落进芯片无法确证** ⇒ **钉到 DF=0 后两种情况都对(写进去=显式补码;没写进去=上电默认 0),零回归**;
反过来只旋转软件,下次上电若 CMR 恰好写进去就又会错;
★ **最硬的一条验证**:启动日志 `[proto] 偏压 = 0.0 V(0x14 = 0x00000)`,而**旧公式下 0.0 V 会算出 `0x80000`** ⇒ 生产二进制确已换公式;
★ **ADC 相干 DFT 基波 562.4 码 / 次强 bin 仅 2.1(≈48 dB)/ 2 次谐波 −48.6 dBc** ⇒ 2f_e 抵消成立、`adc_d ^ 12'h800` 有效;
★ **用户「高压级不管」时我正追 1.1% 量程残差,当即停手** —— 该 DAC 增益规格 ±150 ppm,1.1% 是它的 ~75 倍,且**无内部基准** ⇒ 归 B8 校准,不该用手读数烘系数;
★ **误判过一次"硬件故障",实为我的脚本 bug**(`adc_dft.py` 从头到尾没有 GO 触发,读的是空缓冲)⇒ 先怀疑自己的工具。
★ **模拟输出侧由用户收口** —— 用户持表读后确认「**DAC 输出正确了**」⇒ 偏压链端到端闭环;
⚠️ 口径:口头判读、**无逐点表值** ⇒ 可写"与设定相符",**不可写"线性度误差 X%"**(同 `LLM-043` ⑭⑮)。
⚠️ **CMR 是否真写进芯片、自校准是否真跑过 —— 两项仍如实标「未结案」**(**不因上面那条确认而关闭** ——
"输出的数对不对"与"配置有没有写进去"是两回事);坐实需建 DAC1220 SPI 从机模型或上示波器。
位流 md5 **`d4d5538e…`**(旧 `9613c292…`),`.hwh` 逐字节未变;**WNS +1.161** / WHS +0.051 / 失败端点 **0 / 5266**;
**1716 LUT(3.23%)/ 1993 FF / 4 BRAM / 0 DSP / 67 IOB(53.60%)**)*

📌 统计重跑:`✅=21 / ⚠️=37 / ❌=0 / 无标记=1 / 合计=59`(`LLM-063` 记 ⚠️ ——
**DIV 修复代码侧已落地并自检通过,但新默认要等下一个上电周期才生效**(重启服务 = 第二次 `Overlay()` = 红线);
`LLM-062` 亦记 ⚠️ —— **代码侧完成、自检通过,但回路切换与相位稳定性只有上电才能验**;`无标记` 仍只有 `LLM-037`,规则连续第七轮生效))*

*上一版:2026-10-08(第 50 次会话,`LLM-055` —— **采样率统一 65 MSPS + 采集窗 4096 → 8192**;
★ **"改容量"沿路有三道静默闸**(写掩码截断 / 建表上限 / 显示静默截断),**三道都不报错**;
★ **10 MHz 档「算术成立、物理不成立」** —— AD9833 的 25 MHz MCLK 焊死,数字域全算对而模拟域出不来,
**由用户上板试出来后才删**(这是本场最重的一次纠错,已落进 `skill/silent-capacity-gates/`);
三档窗长板上实测 **6500/7800/8190**,**六个数逐位一致且三档互不相同**;
BRAM 2.5 → 4 tile,WNS +1.023 ns / 失败端点 0/4976。
📌 本行上一次(第 48 次)记的 `LLM-053` 已于同日被 `LLM-054`(65 MSPS 界面档位打通,第二道闸仍是 C)
与 `LLM-055`(加长窗 + 删 10 MHz)接续 —— **两条记录之间隔了 `LLM-054`,不是漏记**)*

*上一版:2026-10-07(第 48 次会话,`LLM-053` —— **65 MSPS 打通:挡住它的不是 RTL 而是一行 C**(`cv_adc_phase_center()` 在 `P=2` 少一条折回,算出的刻度落在寄存器合法范围之外 ⇒ `done` 永不置位);§四 三条复测第二遍(**位序 ✅ 复现、采样窗 ✅ 复现、OEB ⬜ 没测到**);★ **一次自查撤回**(我做的 OEB 测试无效 —— 探针的 `cfg` 不含 OEB_N 位且会重写整个 `0x88`,故**第 47 次的 4095 冻结结论未被推翻**);测完已按用户指令 `shutdown -h now` 供断电;接 `LLM-052` 之后)*
