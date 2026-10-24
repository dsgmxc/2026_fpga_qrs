# `hardware/` —— 自绘硬件板卡设计源文件

本目录收录本项目**自行设计**的四块板卡,均为 **立创EDA专业版(EasyEDA Pro)工程归档**,
内含完整原理图与 PCB 设计,可直接编辑。

## 一、文件清单

| 文件 | 板卡 | 在本项目中的作用 |
|---|---|---|
| `cv_control_expansion_board.epro` | CV 测试主控拓展板 | 主控与各功能板之间的接口/转接 |
| `lcr_acquisition_board_v3.epro2` | LCR 采集板(第三版) | 模拟前端与采集通路 |
| `relay_matrix.epro2` | 继电器矩阵板 | CISS / COSS / CRSS 回路与量程切换 |
| `power_board_110v_48v_in_110v_12v_5v_out.epro2` | 电源板 | 110 V / 48 V 输入 → 110 V / 12 V / 5 V 输出 |

> 作用一栏按**设计用途**填写;具体连接关系、器件选型与参数**以工程内原理图为准**。
> 其中 `.epro` 与 `.epro2` 是立创EDA专业版的两种工程归档格式,不是两个软件。

## 二、打开方式

1. 安装 **立创EDA专业版**(工程记录 `editorVersion = 4.1.78`);
2. 「文件 → 打开 → 工程」,选择对应的 `.epro` / `.epro2` 文件;
3. 打开后可查看原理图、PCB,并自行导出 Gerber / BOM / 坐标文件。

## 三、收录范围

| 收录 | 未收录 |
|---|---|
| 四块板卡的**设计源文件**(原理图 + PCB) | Gerber、BOM、钢网/坐标文件(可由工程导出) |
| — | 板卡单独的测试记录(本项目实测记录见仓库 `board/measurements/`) |

## 四、文件校验

| 文件 | 大小 | MD5 |
|---|---|---|
| `cv_control_expansion_board.epro` | 102,176 B | `20b46ffc43762b43ce00dd8e060905d4` |
| `lcr_acquisition_board_v3.epro2` | 270,729 B | `cc6461a522a059058a04da3cb8621f7f` |
| `power_board_110v_48v_in_110v_12v_5v_out.epro2` | 419,302 B | `8f7e76eff68ce8401845ef9033ac1afd` |
| `relay_matrix.epro2` | 491,762 B | `fe2c9b657c6767c85c345cbe542ffffe` |
