# pynq-register-map-pattern

> **通用 PYNQ 技能**(不与本队题目绑定)—— 用自写 RTL 实现 AXI4-Lite 从机寄存器文件,
> 让 PYNQ 侧能 `read` / `write`。含**设计范式 + 上板故障定位**两半。

## 适用范围

| 项 | 值 |
|---|---|
| 目标 | 不用厂商 IP(`axi_gpio` 之类),用**自写 RTL** 给 PS 提供寄存器入口 |
| 平台 | Zynq-7000 + Vivado BD + PYNQ 2.7(BD 里以 `module_ref` 形式挂载) |
| 已验证 | `C_S_AXI_ADDR_WIDTH = 7`(32 个 32-bit 槽、窗口 4 KB)、32 位数据、单时钟域 |
| 未验证 | `WSTRB` 部分写、突发、多 outstanding、MMIO 之外的 AXI 特性 |

**不适用**:需要 DMA 的高吞吐场景(那该用 `axi_dma` + AXI-Stream,不是寄存器文件)。

---

## 使用方法

### 一、设计范式(每一条都对应一个真实踩过的坑)

#### 1. 地址与数据**必须在握手当拍锁存**

```verilog
// ✅ 正确
always @(posedge s_axi_aclk) begin
    if (s_axi_awvalid && s_axi_awready) awaddr_r <= s_axi_awaddr;
    if (s_axi_wvalid  && s_axi_wready ) wdata_r  <= s_axi_wdata;
end
// ❌ 错误:译码直接读输入引脚 s_axi_awaddr
```

**为什么**:AXI 协议只要求 `AWADDR` 在 `AWVALID` 为高期间保持稳定;`AWREADY` 拉高之后
主设备**可以立刻改 `AWADDR` 去发下一笔**。若 W 通道晚于 AW 到达,写入那一拍的
`s_axi_awaddr` 可能已经是**下一笔的地址** ⇒ 译到别的寄存器去。

⚠️ **这类 bug 在单笔访问下不会暴露**,只有 AW/W 时序错开才现形 ——
所以它**必须靠仿真覆盖**(故意让 W 迟到),上板测不出来。

#### 2. `BVALID` 要等 **AW 与 W 都到齐**

两个通道各自握手成功 ≠ 可以回响应。缺一条就回 `BVALID`,会在主设备侧造成
"写完成但数据没写进去"。

#### 3. 🔴 `C_S_AXI_ADDR_WIDTH ≥ 7`

- `ADDR_WIDTH = 5` → 只有 8 个槽(`a[4:2]`)⇒ `0x4C` 会**混叠**到 `0x0C` 之类的低地址。
- `ADDR_WIDTH = 7` → `a[6:2]` → **32 个槽**,窗口 4 KB(`0x000`–`0x7C`)。
- ⚠️ **升位不是免费动作**:改 `.v` 之外还要改 BD 里 `module_ref` 的边界快照(见 §4)。

#### 4. `module_ref` 的端口边界**会快照进 `.xci`**

以 `module_ref` 形式挂自写 RTL 时,Vivado 会把**当时的端口列表**快照进
`<proj>.gen/sources_1/bd/<bd>/ip/<cell>/*.xci` 的 `boundary` 节。

⇒ **改了 RTL 端口,BD 里不会自动出现新引脚,而且不报错。**
必须 `Refresh Module Reference`(或清缓存,见 `skill/stale-artifact-silent-reuse/`)。

#### 5. `X_INTERFACE_PARAMETER` 与常规 `s_axi_*` 命名**可以互相替代**

两种写法 PYNQ 都能正确解析出 `MEMRANGE` / `ADDR_WIDTH`:

```verilog
(* X_INTERFACE_PARAMETER = "XIL_INTERFACENAME S_AXI, PROTOCOL AXI4LITE, ADDR_WIDTH 7" *)
// 或者靠 s_axi_* 的标准命名让 Vivado 自动识别
```

### 二、BD 侧接线(最小可跑)

```
processing_system7_0/M_AXI_GP0 → axi_smc → <your_ip>/S_AXI
s_axi_aclk    ← FCLK_CLK0
s_axi_aresetn ← peripheral_aresetn
地址:分配一个 4 KB 窗口,例如 0x43C0_0000
```

### 三、上板自检(PYNQ 侧)

```python
ol = Overlay('qrs.bit')
d = ol.ip_dict['cv_axi_lite_slave_0']
print(d['fullpath'], hex(d['phys_addr']), d['addr_range'], d['parameters']['C_S_AXI_ADDR_WIDTH'])
# 期望:裸实例名 / 0x43c00000 / 4096 / '7'

mmio = ol.cv_axi_lite_slave_0.mmio
print(hex(mmio.read(0x48)))                 # VERSION 常量
mmio.write(0x4C, 0xDEADBEEF); print(hex(mmio.read(0x4C)))
mmio.write(0x4C, 0xA5A55A5A); print(hex(mmio.read(0x4C)))   # 🔑 换值第二笔
```

🔑 **为什么必须写两笔**:只写一笔测不出「读通路恒返回常数 / 压根没连」的**假通过**。
两笔不同的值要求寄存器真的**存住了最后一笔写**。

### 四、故障定位速查

| 现象 | 先查什么 |
|---|---|
| `ol.<name>` 取不到 | hwh 里有没有这个 MODULE?它**有没有 AXI 从机接口**?(裸 RTL 模块没接口 ⇒ 没地址 ⇒ 不进 `ip_dict`,**这是正确行为**) |
| `ip_dict` 里地址不对 | BD 的 Address Editor 分配了没?`MEMRANGE` 对不对? |
| 读回值恒为常数 | 换一个值再写一笔;再不行读一个**未实现地址**看是不是恒 0 |
| 写入不生效 | AW/W 是否真的都握手了?`BVALID` 是否提前? |
| 端口改了但 BD 没有 | `.xci` 边界快照过期 ⇒ Refresh Module Reference |
| `registers: {}` 是空的 | **对裸 `MMIO.read/write` 无影响**(地址走 `MEMRANGE`)。只有依赖 PYNQ 自动生成寄存器驱动时才有差别 |

---

## 失效条件

- ⛔ **`ADDR_WIDTH ≤ 5`** 且寄存器地址跨过了 8 个槽 —— 静默混叠,不报错。
- ⛔ **译码读输入引脚而非锁存值** —— AW/W 错开时写错寄存器。
- ⛔ **改了 RTL 端口却直接重建 BD cell** —— 会拿到**过期端口表**,且不报错。
- ⛔ **单笔写就宣布"回环通过"** —— 有假通过的可能。
- ⚠️ **上板跑通 ≠ AXI 协议合规**。它只证明**被测到的那几条通路**能工作;
  未实现地址的读、`WSTRB`、复位行为、背靠背访问都要另外测。

## 已验证的效果

| 项 | 结果 | 证据 |
|---|---|---|
| 自写 AXI4-Lite 从机首次上板 | ✅ **P2 判据达成**:`VER 0x10000` / `S0 0x0` / `W1 0xdeadbeef` / **`W2 0xa5a55a5a`** | `report/measurements/20260929_scratch_loopback.md` |
| 实现质量 | WNS **+13.943 ns** / TNS 0.000 / 失败端点 0 / DRC 0 error / LUT **684**(1.29%) / FF **878**(0.83%) | `impl_1/system_wrapper_*.rpt` |
| PYNQ 真解析器(2.7)读自建 BD | ✅ 名字 = **裸实例名**(无 `/S_AXI` 后缀)、`phys_addr = 0x43C00000`、`C_S_AXI_ADDR_WIDTH = '7'` —— **三项事前预测、事后确认** | 同上 §五 |
| 一次自查出的真 AXI 协议 bug | ✅ 第一稿译码直接读 `s_axi_awaddr`,自查时改为握手当拍锁存 | `cv_axi_lite_slave.v:111-116` 注释 + `LLM-027` |
| 从零重来一遍的路径 | ✅ `.bit`+`.hwh` 同名 → `sudo -E` → 三条自检,见 `skill/pynq-overlay-bringup/` | `LLM-022/027` |

⚠️ **未验证**:`cv_axi_lite_slave.v` **没有功能仿真** —— 只过了 `xvlog` / `xelab` 的
语法检查(rc=0),**零激励、零波形**。上板跑的只有 VERSION 读 + SCRATCH 读写两条通路。
「AW/W 乱序」那条修复**至今没有被任何测试覆盖过** —— 这是本技能包已知的最大缺口。

## 来源

1. **`LLM-027`(2026-09-29)**,两条:
   - 🔴 **把下游阶段的依赖当成当前阶段的阻塞**。用户问"PS 怎么控制继电器",模型答
     "继电器引脚一根没分配 ⇒ 现在推不动" —— 引脚确实没分配(事实无误),
     错在**用它去否决一件与之无关的事**:`SCRATCH` 回环**需要零根引脚**。
     用户一句「`cv_axi_lite_slave.v` 为什么不现在开始写这个」把问题问回来。
     > 判据:**看它的交付物依赖什么,不看它最终服务于谁。**
   - 🔴 **AWADDR 未锁存的真 bug**(上一节 §1),由写码时自查发现,非事后暴露。
2. **`20260929_scratch_loopback.md` §五** —— `ip_dict` 三条事前预测事后确认,
   证明「`.hwh` 里 `BDTYPE` 只属于嵌套 BD ⇒ PYNQ 里的名字是裸实例名」这条推断成立。
3. **`LLM-027` / `LLM-029`** —— 寄存器地址空间规划:`ADDR_WIDTH = 7` 的取舍与占用核对
   (`0x00`–`0x7C` 共 32 槽,项目已用 29 个)。

> ⚠️ **引用纪律**:`axi_gpio` 回环(厂商 IP)与 `SCRATCH` 回环(自写 RTL)**不是同一件事**,
> 报告里不得混称。
