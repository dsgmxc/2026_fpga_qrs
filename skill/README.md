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

### 项目 Skill

> 面向本队题目,但同样按上述五节格式撰写;其中多数**方法本身是通用的**。

| Skill | 一句话说明 | 状态 |
|---|---|---|
| [`verilog-review-checklist/`](verilog-review-checklist/skill.md) | RTL 静态审查清单(按本赛道考察侧重排序),每条对应一次真实踩坑 | ✅ **已落地** |
| [`spi-tb-patterns/`](spi-tb-patterns/skill.md) | 自检测试台设计:图案判别力实测 + 变异测试 + 负测试自验证 + 看门狗 | ✅ **已落地** |
| [`measurement-instrument-can-be-wrong/`](measurement-instrument-can-be-wrong/skill.md) | **量具会错**:八次事故 + 四条通用判据(素材最厚的一项) | ✅ **已落地** |
| [`stale-artifact-silent-reuse/`](stale-artifact-silent-reuse/skill.md) | 工具缓存/产物比源文件活得久,且不报错 —— 四种形态 | ✅ **已落地** |
| [`rtl-constant-experiment-must-verify-netlist/`](rtl-constant-experiment-must-verify-netlist/skill.md) | 把 RTL 常量当实验自变量时:源码不是证据,综合后网表才是 | ✅ **已落地** |
| [`use-layout-as-ruler/`](use-layout-as-ruler/skill.md) | 用被测对象自己的几何当量具,而不是用人的手当量具 | ✅ **已落地** |
| [`gbk-example-porting/`](gbk-example-porting/skill.md) | 中文厂商 GBK 例程向 Linux 工具链移植:往返校验 + 正文 md5 | ✅ **已落地** |

### ⏸ 计划中但**未落地**(缺"已验证的效果"与"来源",不建空壳)

> M06 明确要求每项写明「**已验证的效果**」与「**从哪些失败中总结得出**」。
> 这两项**编造不出来** —— 所以下面这些**等阶段到了、真踩过坑再写**。

| Skill | 计划内容 | 卡在哪 |
|---|---|---|
| `hls-ap-none-rtl-integration/` | HLS 裸端口模块在 RTL 中例化的踩坑清单 | HLS 两个核(`dlia_core` / `cordic_polar`)**一行都还没写** |
| `vivado-input-delay-calc/` | 并行 ADC 的 `set_input_delay` 计算与约束模板 | 目前只有输入方向约束,`set_output_delay` 尚未写;`clk_adc` 频率未定 |
| `cic-coherent-sampling/` | 相干采样参数解算(M / R / f 的整数周期关系) | 只有**纸面推导**,尚无仿真或上板验证 |
| `prompt-hls-pipeline/` | 从算法描述生成 HLS 并引导流水线优化的提示词模板 | 同上 —— 没有实际用过的提示词 |
| `prompt-timing-triage/` | 把时序报告喂给 LLM 做瓶颈定位的提示词模板 | 同上 |
| `golden-compare-script/` | 黄金参考自动比对与误差报告生成 | 现在的手法是**离散的变异测试**,还没有沉淀出脚本化流程 |

---

## 📌 阅读建议(评委会关心的问题)

**「十项里哪几项最有实操价值?」**

1. **`measurement-instrument-can-be-wrong/`** —— 素材最厚(八次真实事故,跨 8 个不同工具)。
   它解决的是**上游问题**:测量本身是否可信。队伍拿到自己的板子上,第一天就会用到。
2. **`stale-artifact-silent-reuse/`** —— 四次发作分布在**四个不同工具**上
   (烧录器 / 工程文件 / BD 缓存 / 仿真器),说明是**系统性风险**而非某工具 bug。
3. **`pynq-ddr-framebuffer-display/`** —— 三个通用 PYNQ 技能里技术含量最高的一个:
   一次真实的"整屏全黑"根因定位,**判据选择、单变量分离、双观测者**三样都在里面。
4. **`use-layout-as-ruler/`** —— 最短,但**效率提升最明显**:一个悬了两轮的问题,
   换问法后**一次按压结案**。

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

**本目录当前状态(2026-10-03)**:

- 索引提名过 **73** 个候选 skill 名(`grep -oE 'skill/[a-z0-9-]+' report/llm-log/INDEX.md | sort -u | wc -l`),
  **已落地 10 项**。
- 落地的 10 项**全部**满足五要素;「已验证的效果」全部可追到
  `report/measurements/` 的存档或 `report/llm-log/` 的记录编号。
- **未落地的项写进上表并说明卡点**,不以空壳充数。

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
