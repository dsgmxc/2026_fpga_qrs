# 20260925 — Vivado 2025.2 导出的 `.hwh` 与 PYNQ 2.7 的兼容性验证

> **性质**:⚠️ **主机侧(host-side)验证,不是上板测量。**
> 本目录其余文件是板上实测证据;本条是**工具链兼容性**验证,
> 被测对象是「Vivado 2025.2 产出的 hwh 文件」与「PYNQ 2.7 的 hwh_parser.py」,
> 二者都在开发机 / SD 卡镜像里,**没有经过板子**。
> 按本目录 §「每条证据必须同时记录」的 6 项,不适用的项已改写为对应物。

| 项 | 本次记录 |
|---|---|
| **被测对象 / 测点** | `hwh_parser.py` 对 `.hwh` 的解析结果(`ip_dict`),非物理测点 |
| **所烧位流版本** | **不适用**(本次未上板、未烧任何 bitstream) |
| **烧录时间** | **不适用** |
| **板型 / 屏型号** | 目标板 xc7z020clg400-2;未使用板子。**工具版本才是本次的环境**:Vivado **2025.2** / PYNQ **2.7.0**(镜像 `ZYNQ-7020-2.7.0.img`,Release 2025_08_27,md5 **`0c410e8eb3f39848a39c2d9961c7006e`**) |
| **预期值 vs 实测值** | 预期:2025.2 的 hwh 能解析出与 2020.2 同构的 `ip_dict`。<br>**实测:符合**(见下表) |
| **仪器 / 量程** | Vivado 2025.2 batch 模式;解析器为**真件**,md5 `841ea7686cc3d1fee8c61281db466f5c` |

---

## 1. 为什么做这个验证

`CLAUDE.md` §二 规定本项目用 **Vivado 2025.2**;而 PYNQ 2.7.0 镜像打包于 2025-08-27,
其自带的示例 hwh 全部是 **Vivado 2020.2** 产物。
`hwh_parser.py` 里**没有任何版本闸门**(不比对 `VIVADOVERSION`),所以
「2025.2 导出的 hwh 能否被解析」此前是 **P2 的头号未验风险**。

## 2. 方法

**不问「版本号一不一样」,直接拿真解析器跑真文件。**

三步:
1. 用本机 Vivado 2025.2 在 `/tmp` 建**一次性探针工程**(不触碰用户的 `qrs` 工程),
   搭出与目标 overlay 同形的 BD,`generate_target hw_handoff` 导出 hwh。
2. 从 `ZYNQ-7020-2.7.0.img` 的 ext4 分区里用 `debugfs` 取出**板上真正在跑的**
   `hwh_parser.py`(免 root、免挂载)。
3. 补一个 4 行的 `pynq.ps` 桩(只提供 `CPU_ARCH` 等四个常量),
   在 Python 里把真解析器跑到真 hwh 上,对比 `ip_dict`。

**对照组**:厂商 `ZYNQ_7020_PYNQ.zip` 里的 `2_axi_gpio` 示例,其 BD 内容与探针 1
几乎一致(PS7 + AXI GPIO),但由 **2020.2** 生成 —— 同内容、异版本,是最干净的 A/B。

## 3. 结果

| 用例 | Vivado | hwh md5 | 解析结果 |
|---|---|---|---|
| 厂商 `axi_gpio`(对照) | 2020.2 | `8d223915…` | ✅ 4 个 IP,地址正确 |
| **探针 1:PS7 + axi_gpio** | **2025.2** | `fa5d36e9…` | ✅ **2 个 IP,地址正确**(`axi_gpio_0` → `0x41200000`) |
| 探针 2:纯 module_ref(`spi_master`) | 2025.2 | — | ✅ 解析不报错;`ip_dict` 空(原因见 §4) |
| 探针 3:PS7 + 互联 + 无 AXI 的 module_ref | 2025.2 | `17c243f7…` | ✅ 解析不报错;module_ref **不进** `ip_dict`(负对照) |
| 厂商 `resizer` / PYNQ-Z2 `base` | 2020.2 | — | ✅ 正常 |

**逐字段对照(2020.2 vs 2025.2),完全一致:**

| 检查项 | 2020.2 | 2025.2 |
|---|---|---|
| 根节点 | `EDKSYSTEM` | `EDKSYSTEM` |
| `EDWVERSION` | `1.2` | **`1.2`** |
| 根子节点 | SYSTEMINFO / EXTERNALPORTS / EXTERNALINTERFACES / MODULES | **同** |
| `SYSTEMINFO` 属性 | ARCH / DEVICE / NAME / PACKAGE / SPEEDGRADE | **同** |
| `MODULE` 属性集 | INSTANCE / FULLNAME / VLNV / MODTYPE / IPTYPE / MODCLASS / HWVERSION / COREREVISION / IS_ENABLE | **同** |
| `PARAMETERS` / `REGISTERS` / `ADDRESSBLOCKS` / `MEMRANGE` 层级 | — | **同** |

## 4. 顺带查实的一件事(与 P2 设计直接相关)

**没有 AXI 从机接口的 RTL 模块引用,在 PYNQ 里没有对象可访问。**

探针 2/3 里,`spi_master` 被当作 module_ref 挂进 BD 后:
- ✅ 它**确实出现在** hwh 的 `MODULE` 列表里(2025.2 正常导出,`MODTYPE='spi_master'`,
  `VLNV='xilinx.com:module_ref:spi_master:1.0'`);
- ❌ 但它**不进入** `ip_dict` —— 因为它的 `ADDRESSBLOCK` / `MEMRANGE` / `BUSINTERFACES` 计数均为 **0**,
  没有地址就没有 MMIO 入口。

⇒ **推论(直接影响 `cv_axi_lite_slave.v` 的设计)**:寄存器文件必须包在
**AXI4-Lite 从机**里,PS 侧 `ol.<name>` 才成立。
这与 `report/source_design_checklist.md` §3 的寄存器映射表方向一致,此处是**实测确认**。

## 5. 本次**没有**验到的(不要当成已验)

1. 🔴 **module_ref 带 AXI 接口**的情况没跑到 —— 手上没有带 AXI 从机接口的 RTL 模块可挂。
   解析器里**有专门分支**处理它(`hwh_parser.py:282`,
   `vlnv.split(':')[:2] == ['xilinx.com','module_ref']`),说明是 PYNQ 官方支持的路径,
   但**未经我实测**。⇒ 等 `cv_axi_lite_slave.v` 写出来即可闭环,正好是 P2 第一步。
2. ⚠️ 解析器是在**本机 Python 3.10.12** 上跑的;板上是 **Python 3.8.2**。
   用到的 ElementTree API 两边一致,风险低,但**板上跑一遍才算数**。
3. ⚠️ 全程**未上板**,`.bit` 未烧,`fpga_manager` 未介入。
4. ⚠️ 厂商示例中**没有任何一个用了 module_ref**,所以
   「2020.2 对 module_ref 写不写 `BDTYPE`」**无法对照** —— 本次 2025.2 的实测是
   `BDTYPE` **缺失**,而解析器对它是 `.get()` 取值、随后解包丢弃(`:258` 的 `_, _`),
   **缺失无害**。

## 6. 复现方式

```bash
# 1) 探针工程(脚本在 /tmp 下,一次性,不进仓库)
source /home/hyp/xilinx/2025.2/Vivado/settings64.sh
vivado -mode batch -nojournal -notrace -source /tmp/hwhprobe/probe.tcl

# 2) 从镜像取板上真解析器(免 root)
dd if=<IMG>/ZYNQ-7020-2.7.0.img of=/tmp/pynq_rootfs.ext4 bs=512 skip=268288 count=15533648
debugfs -R "cat /home/xilinx/pynq/pl_server/hwh_parser.py" /tmp/pynq_rootfs.ext4 > /tmp/hwh_parser.py

# 3) 跑解析器(需自建 pynq.ps 桩,见 /tmp/hwhtest)
```

---

*记录时间:2026-09-25 · 由 Claude 执行,Vivado 由 Claude 调用,未上板*
