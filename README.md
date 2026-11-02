# QRS 2026 — MOSFET 寄生电容 C-V 在线监测平台

全国大学生嵌入式芯片与系统设计竞赛 2026 · FPGA 创新设计赛道 · AMD 自主选题赛道(初级组)

以正点原子领航者 ZYNQ-7020(XC7Z020)为核心:PL 侧完成 ADC 采集、正交解调、继电器与 DDS 控制、LCD 时序与触摸 IIC;PS 侧完成扫描编排、阻抗反演、通道级校准、AI 异常检测与 LVGL 界面。

| 项 | 配置 |
|---|---|
| 激励 | AD9833 DDS,1.000 MHz(另支持 10 kHz / 100 kHz) |
| 采集 | AD9238-65,固定 **32.50 MSPS**(130 MHz ÷ 4) |
| 量程 | A 10 Ω / B 100 Ω / C 1 kΩ / D 10 kΩ,继电器切换 |
| 校准 | OPEN / SHORT / LOAD 通道级复增益;仅 B 挡通过幅值守卫 |
| 界面 | 1024×600 RGB888 并行屏 + GT911 电容触摸 |
| 工具 | Vivado / Vitis 2025.2(兼容 2026.1) |

## 目录对照说明

本仓库采用指南 3.3.5.4 的推荐目录结构,对应关系如下:

| 目录 / 文件 | 内容 |
|---|---|
| `README.md` | 项目简介与复现入口 |
| `src/rtl/` | 手写 RTL(13 个模块:采集、解调、SPI、DDS、继电器、LCD、IIC;已删除未例化的旧顶层与遗留模块) |
| `src/constrs/` | 引脚与时序约束(`led.xdc`) |
| `src/hls/` | HLS 预留目录(当前为空) |
| `src/python/` | PS 侧 PYNQ 软件(`main.py`、寄存器驱动、校准与可视化) |
| `src/c_ui/` | PS 侧 C/LVGL 界面进程(含 LVGL 8.2.0 第三方源码) |
| `sim/` | 测试台与验证报告 |
| `build/` | 构建脚本、综合与实现报告、构建汇总 |
| `board/` | 位流、`.hwh`、systemd 服务、板上运行脚本、逐次实测存档 |
| `data/` | 测试数据与参考结果 |
| `hardware/` | 自绘板卡设计源文件(立创EDA 专业版,4 块板) |
| `skill/` | 技能包(入口 `skill/README.md`) |
| `report/` | 设计报告正文与参考文献 |
| `llm-log/` | 大模型协作逐条记录与索引 |
| `docs/` | 接口规格、引脚表、显示/触摸子系统、采样解调规格、开发流程手册 |
| `LICENSE` | MIT |

与推荐结构相比新增 `docs/`、`hardware/`、`llm-log/`、`src/c_ui/`,并把上板实测存档放在 `board/measurements/`。拆分理由:这些内容的更新频率、体量与读者均不同于报告正文;指南允许在 README 中给出对照说明。

## 提交物索引

| 必做点 | 位置 |
|---|---|
| M1 工程文件与源码 | `src/`、`hardware/` |
| M2 仿真与验证 | `sim/`(5 个测试台 + 4 份验证报告) |
| M3 综合与实现报告 | `build/`(4 份 `.rpt` + `summary.txt`) |
| M4 上板与实测输出 | `board/`、`board/measurements/` |
| M5 可复现构建 | `build/build_bitstream.tcl`、`docs/dev_workflow.md` |
| M6 技能包 | `skill/` |
| M7–M12 设计报告 | `report/design_report.md`、`docs/`、`llm-log/` |
| M13 板上跑通 | `board/measurements/` 逐次存档(含原始日志与截图) |
| M15 开源协议 | `LICENSE`(MIT) |

## 当前版本

| 项 | 值 |
|---|---|
| 位流 | `board/qrs.bit`,md5 `c92c7ed5b7d695dee73d86abc1c6089c` |
| 硬件描述 | `board/qrs.hwh`,md5 `6c376905b48aa8d79778f0d4b21f0bbc` |
| 资源 | LUT 1790(3.36%) / FF 2022(1.90%) / BRAM 7.5(5.36%) / DSP 0 |
| 时序 | WNS **+0.316 ns**,WHS **+0.065 ns**,失败端点 **0 / 5472**,约束全部满足 |
| 时钟 | `clk_axi` 50.000 MHz · `clk_adc` 130 MHz(分出 32.50 MSPS)· PCLK 50 MHz |

`board/qrs.bit` 与 `board/qrs.hwh` 必须成对使用;`build/` 下 4 份报告与当前位流同批生成。

## 复现步骤

1. **环境**:Vivado / Vitis 2025.2(或 2026.1)、PYNQ 2.7.0 镜像、正点原子领航者 ZYNQ-7020 板卡。
2. **位流**:准备 Vivado 工程 `qrs.xpr`,然后执行
   ```bash
   export PATH=$HOME/xilinx/2025.2/Vivado/bin:$PATH
   QRS_PROJ_DIR=/path/to/qrs \
     vivado -mode batch -notrace -source build/build_bitstream.tcl \
     -log build/vivado_build.log -journal build/vivado_build.jou
   ```
   脚本执行完整重建并输出 `build/summary.txt`。BD 侧改动另见 `build/bd_add_*.tcl`。
3. **上板**:接线、上电、网络与串口步骤见 `docs/dev_workflow.md`;位流加载用 `board/qrs-ui.service`。
4. **界面**:将 `src/c_ui/` 拷到板上,在板端 `make -j2`,生成 `c_ui`。
5. **运行**:`python3 src/python/main.py` 或启用 `qrs-ui.service`。

## 二进制文件

- `board/qrs.bit` / `board/qrs.hwh`:位流与硬件描述,必须同基名、同版本。
- `board/measurements/artifacts/*.bit`:对照实验位流原件(报告中引用的证据)。
- `hardware/*.epro2`:立创EDA 专业版工程归档,打开方式见 `hardware/README.md`。

## 许可

MIT License,见 `LICENSE`。
