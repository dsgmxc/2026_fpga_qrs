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

>
> 🆕 **2026-09-22 起,记录分两类来源**(写入纪律见 `LLM-020`) ——
> **模型工作区** `~/Desktop/qrs_2026_fpga/`(本目录所在)与
> **交付仓库** `~/Desktop/qrs_fpga_2026/`(推送到 Gitee)是**两个独立目录**。
> 交付仓库由**用户把关**,模型须经点名才可写入;本目录的记录**何时同步进交付仓库由用户决定**。

---

## 统计

| 指标 | 数值 |
|---|---|
| 记录总数 | **40**(LLM-001 ~ LLM-040) |
| ✅ 直接可用 | **9**(LLM-004 结案 + LLM-009 + LLM-010 + LLM-011 + LLM-014 + LLM-023 + LLM-027 + LLM-036 + LLM-038) |
| ⚠️ 部分可用 | **31**(表内逐行点数为 31;此前记 26 系计数笔误,2026-10-03 按实测更正,并在 LLM-037 / LLM-039 / LLM-040 后各 +1) |
| ❌ 不可用 | 0 |
| 已沉淀为 Skill | **10** ✅(2026-10-03 由 `LLM-039` 一次性落地;此前长期为 0) |
| 已提名候选 Skill | **76**(其中 **10 项已落地**,66 项仍未落地;命令见下)。⚠️ **2026-10-04 实测为 76** —— 本轮新增 2 项(`ceiling-truncation-fingerprint` / `font-subset-silent-blank`),而上轮记载为 73,**余 1 项差额未查、不编原因** |

> 🟢 **M6 缺口已闭合一半(2026-10-03,`LLM-039`)** —— 索引里提名过 **76** 个候选 skill 名(2026-10-04 实测)
> (命令:`grep -oE 'skill/[a-z0-9-]+/' INDEX.md | sort -u | wc -l`),
> **本轮一次落地 10 项**(命令:`find skill -name 'skill.md' | wc -l` → **10**),
> 合计 `skill/` 下 **11 个文件、1634 行**,五要素 grep **10/10 齐全**。
> 落地清单:3 项**通用 PYNQ**(`pynq-overlay-bringup` / `pynq-register-map-pattern` /
> `pynq-ddr-framebuffer-display`,够得着 **B1**)+ 7 项项目技能
> (`verilog-review-checklist` / `spi-tb-patterns` / `measurement-instrument-can-be-wrong` /
> `stale-artifact-silent-reuse` / `rtl-constant-experiment-must-verify-netlist` /
> `use-layout-as-ruler` / `gbk-example-porting`)。**每一项的「来源」都回指具体记录编号,无一条泛化。**
> ⚠️ **剩余 63 个候选名未落地**,其中 6 项已在 `skill/README.md` 里**逐条写明卡点**
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

*最后更新:2026-10-04*
