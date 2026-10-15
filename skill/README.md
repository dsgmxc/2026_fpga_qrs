# 技能包 / Skill Pack

> 本目录是 2026 竞赛「大模型协作与技能包」评分项的提交物,占 15%。
> 评价标准:**一支陌生队伍拿到本目录,能否直接用在它自己的题目上。**

## 目录结构

```
skill/
├── README.md                          # 本文件:技能总览
├── <skill-name>/
│   ├── skill.md                       # 必需:适用范围/使用方法/失效条件/已验证效果/来源
│   ├── assets/                        # 可选:模板、脚本、示例
│   └── ...
└── ...
```

## 技能总览

每一项必须包含以下五节,**缺一不可**:

| 字段 | 说明 |
|---|---|
| **适用范围** | 什么场景下能用,对什么样的项目/板卡/工具版本有效 |
| **使用方法** | 具体怎么用,给操作步骤或命令 |
| **失效条件** | 什么情况下**不能**用 / 会失效 |
| **已验证的效果** | 在本队项目中实际验证的结果(带上板 / 仿真 / 报告证据) |
| **来源** | 从哪些失败或观察中总结得出 |

---

### 通用 PYNQ Skill(单独加分)

> 加分判定标准:**面向 PYNQ 框架,且不与本队题目绑定** —— 换题目、换板卡仍然可用。

| Skill | 一句话说明 | 状态 |
|---|---|---|
| [`pynq-overlay-bringup/`](pynq-overlay-bringup/skill.md) | Overlay 加载与自检清单:`sudo -E`、`.bit`/`.hwh` 同名、FCLK 必须在 `Overlay()` 之后、hwh 版本兼容验证法 | ✅ **已落地** |
| [`pynq-register-map-pattern/`](pynq-register-map-pattern/skill.md) | 自写 AXI4-Lite 从机寄存器文件:设计范式 + 上板故障定位 | ✅ **已落地** |
| [`pynq-ddr-framebuffer-display/`](pynq-ddr-framebuffer-display/skill.md) | DDR 帧缓存 + PL 实时取行驱动并行 RGB 屏:带宽预算、AXI3 硬门、`ARPROT` 全量 DECERR、cache 一致性、防撕裂 | ✅ **已落地** |
| [`count-both-sides-of-interface/`](count-both-sides-of-interface/skill.md) | **接口两侧都要数**:「一次调用 = 一笔事务」是假设不是事实;附 `struct.pack_into` 在设备 mmap 上**先写一个 0** 的实测 | ✅ **已落地**(2026-10-10) |

### 项目 Skill

> 面向本队题目,但同样按上述五节格式撰写;其中多数**方法本身是通用的**。

| Skill | 一句话说明 | 状态 |
|---|---|---|
| [`verilog-review-checklist/`](verilog-review-checklist/skill.md) | RTL 静态审查清单(按本赛道考察侧重排序),每条对应一次真实踩坑 | ✅ **已落地** |
| [`spi-tb-patterns/`](spi-tb-patterns/skill.md) | 自检测试台设计:图案判别力实测 + 变异测试 + 负测试自验证 + 看门狗 | ✅ **已落地** |
| [`measurement-instrument-can-be-wrong/`](measurement-instrument-can-be-wrong/skill.md) | **量具会错**:八次事故 + 四条通用判据(素材最厚的一项) | ✅ **已落地** |
| [`stale-artifact-silent-reuse/`](stale-artifact-silent-reuse/skill.md) | 工具缓存/产物比源文件活得久,且不报错 —— **五种形态**(第五种:**交付源码比它自己发布的位流旧**) | ✅ **已落地**(2026-10-15 补形态 5) |
| [`rtl-constant-experiment-must-verify-netlist/`](rtl-constant-experiment-must-verify-netlist/skill.md) | 把 RTL 常量当实验自变量时:源码不是证据,综合后网表才是 | ✅ **已落地** |
| [`use-layout-as-ruler/`](use-layout-as-ruler/skill.md) | 用被测对象自己的几何当量具,而不是用人的手当量具 | ✅ **已落地** |
| [`gbk-example-porting/`](gbk-example-porting/skill.md) | 中文厂商 GBK 例程向 Linux 工具链移植:往返校验 + 正文 md5 | ✅ **已落地** |
| [`silent-capacity-gates/`](silent-capacity-gates/skill.md) | 改一个容量时沿路的 `掩码/上限/截断`:静默型 / 报错型 / **上限型≠容量型** 三分法 | ✅ **已落地**(2026-10-08) |
| [`classify-by-state-not-by-intent/`](classify-by-state-not-by-intent/skill.md) | 日志分组统计:归类要挑「写下去会被回读确认」的**状态行**,不能挑会残留的**意图行** | ✅ **已落地**(2026-10-15) |
| [`recount-dont-increment/`](recount-dont-increment/skill.md) | **汇总数字一律现场重跑**;增量算术会让你同时继承错误基线、又叠加自己的增量 | ✅ **已落地**(2026-10-15) |
| [`independent-evidence-must-share-no-pipeline/`](independent-evidence-must-share-no-pipeline/skill.md) | 判"独立证据"要分两层问:另一根线?**同一采样刻度?** 同拍即同一条腿 | ✅ **已落地**(2026-10-15) |
| [`consistency-gate-over-median/`](consistency-gate-over-median/skill.md) | 整次采集坏掉时**中位数治不了**:物理不变式 + 重采。「合理」由不变式定义,不由投票定义 | ✅ **已落地**(2026-10-15) |

### ⏸ 计划中但**未落地**(缺"已验证的效果"与"来源",不建空壳)

> M06 明确要求每项写明「**已验证的效果**」与「**从哪些失败中总结得出**」。
> 这两项**编造不出来** —— 所以下面这些**等阶段到了、真踩过坑再写**。

| Skill | 计划内容 | 卡在哪 |
|---|---|---|
| `hls-ap-none-rtl-integration/` | HLS 裸端口模块在 RTL 中例化的踩坑清单 | HLS 两个核(`dlia_core` / `cordic_polar`)**一行都还没写** |
| `vivado-input-delay-calc/` | 并行 ADC 的 `set_input_delay` 计算与约束模板 | 目前只有输入方向约束,`set_output_delay` 尚未写;`clk_adc` 频率未定 |
| `cic-coherent-sampling/` | 相干采样参数解算(M / R / f 的整数周期关系) | ⚠️ **卡点已减半**:数字域那一半(整窗 DFT 的窗长解算)2026-10-08 **已上板验证**(三档窗长 `6500/7800/8190`,见 `report/measurements/20261008_65msps_window_board_acceptance.md`);**仍缺 CIC 那一半** —— `dlia_core` / `cordic_polar` 一行还没写 |
| `prompt-hls-pipeline/` | 从算法描述生成 HLS 并引导流水线优化的提示词模板 | 同上 —— 没有实际用过的提示词 |
| `prompt-timing-triage/` | 把时序报告喂给 LLM 做瓶颈定位的提示词模板 | 同上 |
| `golden-compare-script/` | 黄金参考自动比对与误差报告生成 | 现在的手法是**离散的变异测试**,还没有沉淀出脚本化流程 |

---

## 📌 阅读建议(评委会关心的问题)

**「这么多项里哪几项最有实操价值?」**

1. **`count-both-sides-of-interface/`** —— **代价最高的一次误判**(4 天),但结论最便宜:
   一行 Python 改法。真正值钱的是**判据 0** —— 先问"我数的这个量是在接口哪一侧数的"。
   任何写 AXI 寄存器的 PYNQ 队伍,第一天就会踩到 `struct.pack_into` 那一支。
2. **`measurement-instrument-can-be-wrong/`** —— 素材最厚(八次真实事故,跨 8 个不同工具)。
   它解决的是**上游问题**:测量本身是否可信。
3. **`stale-artifact-silent-reuse/`** —— 四次发作分布在**四个不同工具**上
   (烧录器 / 工程文件 / BD 缓存 / 仿真器),说明是**系统性风险**而非某工具 bug。
4. **`pynq-ddr-framebuffer-display/`** —— 通用 PYNQ 技能里技术含量最高的一个:
   一次真实的"整屏全黑"根因定位,**判据选择、单变量分离、双观测者**三样都在里面。
5. **`use-layout-as-ruler/`** —— 最短,但**效率提升最明显**:一个悬了两轮的问题,
   换问法后**一次按压结案**。
6. **`classify-by-state-not-by-intent/`** —— 最"不起眼"但**后果最严重**的一个:
   一个归属逻辑错的统计脚本,长出了一整条**已经写进草稿的错结论**。
   判据只有三行,但适用于**任何把日志当数据源的分组统计**。
7. **`consistency-gate-over-median/`** —— 直接回答用户那句朴素提问
   「**多测几次取合理值行不行**」:**方向对,中位数不行**。
   里面有一整套"先证明重采有用、再采纳"的实证流程,
   以及"判据的阈值要从分布里选、不是从理论里拍"的完整四行对照表。

**「这些是成功了才写的吗?」**

**不是。** 逐项检查:「来源」一节引用的全部是 `report/llm-log/` 里的**失败与纠错轨迹**,
包括模型自身的错误(排序错误、判据选错、把噪声当结论、把正面证据读成反面证据)。
每项都能追到一次真实的交互记录编号。**本目录不含任何未标注来源的结论。**

---

## ⚠️ 撰写纪律

**技能包不能编。** 评分要求写明每一项须包含"**已验证的效果**"和"**从哪些失败或观察中总结得出**" ——
这两项**编造的写不出来**,评委一追问就会露馅。

所以正确做法是:**从项目第一天起,每踩一个坑就回来补一条**。
`report/llm-log/` 里的纠错轨迹,就是本目录的原料。

**本目录当前状态(2026-10-15)**:

> 📌 **本节数字全部现场重跑**(`skill/recount-dont-increment/` 的自家纪律;
> 上一版这几行是手写的,已核对——本次为**改成实跑值**,不是在上版基础上递增)。

| 项 | 值 | 怎么来的 |
|---|---|---|
| 已落地 skill | **16 项** | `find skill -name skill.md \| wc -l` |
| README 表内链接 | **16 条** | `grep -oE '\]\([a-z0-9-]+/skill\.md\)' skill/README.md \| sort -u \| wc -l` |
| 索引提名过的候选名 | **135 个** | `grep -oE 'skill/[a-z0-9-]+' llm-log/INDEX.md \| sort -u \| wc -l`(2026-10-10 时为 73) |
| 交付仓已入库的 LLM 记录 | **69 篇** | `ls -1 llm-log/LLM-*.md \| wc -l` 与 `grep -c '^\| \[LLM-' llm-log/INDEX.md` **两条独立路径一致** |

- 落地的 16 项**全部**满足五要素;「已验证的效果」全部可追到
  `board/measurements/` 的存档或 `llm-log/` 的记录编号。
- **未落地的项写进上表并说明卡点**,不以空壳充数。
- 📌 新增项的来源固定是**同一周内真实发生的事**:
  - `silent-capacity-gates/` ← `LLM-055` 的三道静默关卡 + 一次上板验收
    (`20261008_65msps_window_board_acceptance.md`);
  - `count-both-sides-of-interface/` ← `LLM-059` 的四天误判 +
    `20261010_relay_cfg_write_anomaly.md` §9(记录互相矛盾)→ §10(结案);
  - 2026-10-15 新增 **4 项** ← `LLM-068` + `LLM-069` 两份记录的纠错轨迹
    (见各 skill 的「来源」节,逐条给出编号);
    **`stale-artifact-silent-reuse/` 同日补了第五种形态**(该形态来自交付仓现状核对,
    不是 LLM 会话,故在 skill 内**如实标注**"无 llm-log 记录")。
  **没有为了凑数而写。**

**归属声明**:本目录下的 `skill.md` 均为 **Claude 撰写的 markdown 文档**
(`CLAUDE.md` §6.0:非代码文档属模型职责)。
其中引用的 RTL / 测试台 / 脚本,**归属逐条在正文中标注** ——
例如 `sim/tb_*.v` 是 Claude 代写(§6.0 显式例外),
`spi_master.v` / `dac_1220.v` / `swm.v` 是用户自写。**不得混称。**

---

## 单项模板

```markdown
# <skill-name>

## 适用范围

<什么项目、什么工具版本、什么条件下适用>

## 使用方法

<步骤 / 命令 / 代码片段>

## 失效条件

<什么时候不能用,为什么>

## 已验证的效果

<在本队项目中的实际验证结果,附证据出处(仿真脚本 / 报告 / 上板记录)>

## 来源

<从哪次失败或观察中总结得出>
```
