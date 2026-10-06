"""
cv_regs.py —— PS 侧寄存器契约(单一事实源)

权威出处:
  · 寄存器地址 / 位域 —— `report/source_design_checklist.md` §3
  · 显示子系统    —— `docs/display_subsystem.md` §5
  · **实际实现**    —— RTL 才是最终权威。本文件与 RTL 冲突时,以 RTL 为准并改本文件。

⚠️ 本文件是【唯一】定义寄存器地址与位域的地方。
   `cv_monitor.py` / `drivers/*.py` / `tools/*.py` 一律从这里取,不得写魔法数。

────────────────────────────────────────────────────────────────────────────
🔴 两处曾经写错的语义(照抄文档会静默出错,已按 RTL 更正)
────────────────────────────────────────────────────────────────────────────

【错 1】`LCD_CTRL` 选帧缓存的值是 **2**,不是 1
    权威:`cv_sys.v:289` `wire [1:0] lcd_src_sel = cfg_lcd_ctrl[1:0];`(它是消费者)
      · 0 = 厂商彩条(复位默认)
      · 1 = 整屏填充 LCD_FILL[23:0]
      · 2 = **DDR 帧缓存**
      · 其他 → 落到彩条(失败安全)
    旧文档写 `[0]src_sel` 且说 "1 = 帧缓存" —— 写 1 会得到**纯色填充**,
    屏上不报错,只是画不出画面。

【错 2】`FCLK0` 就是像素时钟
    `cv_sys.v` `assign lcd_clk = s_axi_aclk;`
    `cv_sys.v` `lcd_timing.clk_lcd = s_axi_aclk`
    ⇒ **改 FCLK0 等于改 PCLK**。屏允许窗口 **40.8~67.2 MHz**,掉出窗口屏会无图或有异常边。

【错 3】🔴 **"实测 41.6667 MHz" 从来不是实测值**(2026-10-03 更正)
    它 = `Clocks.fclk0_mhz` 读回值,而 PYNQ 内部假定 **IO PLL = 1500 MHz**
    (`1500 / DIV0 6 / DIV1 6 = 41.667`)—— 那是**算出来的**,不是量出来的。
    本板 IO PLL 实为 **1000 MHz**,于是同一个寄存器配置的真值是 **27.78 MHz**。
    2026-10-03 用帧率反推三点验证(见 `FCLK0_*` 常量),三点与 1000 MHz 模型全部吻合。

    ⚠️ **连带后果**:`Clocks.fclk0_mhz = 50` 写 DIV0=30,真值只有 **33.3 MHz** ——
       比开机默认的 27.8 高,但**仍低于 40.8 的窗口下限**。
       ⇒ 要真 50 MHz,必须**额外把 DIV0 改成 20**。`drivers/board.apply_fclk0()` 已实现。

────────────────────────────────────────────────────────────────────────────
📌 口径纪律(CLAUDE.md §八,2026-10-03 修订)
    `clk_axi` = **设计值 50 MHz / 真值(按 IO PLL 反算)以 `drivers/board.fclk0_real_mhz()` 为准**。
    ⛔ 不得引用 41.6667 这个数 —— 它是 PYNQ 的**计算值**,不是实测值。
    ⛔ 不得直接引用 `Clocks.fclk0_mhz` 当作真值(内部假定 IO PLL = 1500,本板是 1000)。
"""

# ---------------------------------------------------------------------------
# 基址
# ---------------------------------------------------------------------------
BASE_ADDR = 0x43C00000          # cv_axi_lite_slave(4 KB 窗口)
GPIO_BASE_ADDR = 0x41200000     # axi_gpio_0,channel1 → 板上 H15 LED(09-29 实测点亮)

# ---------------------------------------------------------------------------
# BD 顶层实例名 —— 唯一定义处
# ---------------------------------------------------------------------------
# 🔴 `BASE_ADDR` 属于哪个实例,由 BD 决定,不由 RTL 决定。
#    自 2026-10-01 起 `cv_axi_lite_slave` 被包进 `cv_sys`(见 `cv_sys.v` 头注释:
#    「BD 里只挂 cv_sys 这一个 module_ref」)⇒ 顶层实例名是 `cv_sys_0`。
#    · `drivers/board.py` 用它取 overlay 属性;
#    · `cv_monitor.CVMonitor.bindto` 由它拼出 VLNV。
#    ⇒ **BD 里换顶层模块名时,只改下面这两行。**
#
# ⚠️ 这两个字符串**不一样**,不能互相推导:
#      · 实例名(Vivado 自动加 `_0`)→ 用作 PYNQ 的 overlay 属性:`overlay.cv_sys_0`
#      · 模块名(不带 `_0`)        → 用作 `.hwh` 里的 VLNV:`…:module_ref:cv_sys:1.0`
#    2026-10-02 实测:按实例名拼 VLNV 拼成 `cv_sys_0`,**对不上**(已用 hwh grep 核实)。
TOP_MODULE      = "cv_sys_0"                              # overlay 属性名
TOP_MODULE_VLNV = "xilinx.com:module_ref:cv_sys:1.0"      # .hwh 里的 VLNV

# ---------------------------------------------------------------------------
# 寄存器(字节偏移)
# ---------------------------------------------------------------------------
# ⚠️ 地址窗口 = 0x00 ~ 0xFC(6 位字索引)。
#    2026-10-03 起 7 位/32 个编号**已用满**(连 DBG0/1/2 吃掉三个预留都不够),
#    2026-10-05 为加 `DDS_FREQ(0x80)` 把 `C_S_AXI_ADDR_WIDTH` 由 7 升到 8 ⇒ **64 个**。
#    📌 升位宽不是改个 parameter 就完事:BD 里的 `cv_sys_0` 是 module_ref,
#       会缓存旧 ADDR_WIDTH 与旧端口清单,必须刷新 module reference
#       (`build/bd_add_dac_dds.tcl`)。实测:不刷新时 `system.bd` 里一直写着 `"7"`。
REG = {
    # ---- 测量主链(PL 侧尚未实现,见 PL_IMPLEMENTED_REGS)-------------------
    "CTRL":       0x00,   # RW  [0]start [1]soft_rst [2]ac_en [3]relay_en [4]cal_mode
    "STATUS":     0x04,   # RO  [0]busy [1]done [2]fault [15:8]err_code [31:16]meas_cnt
    "NCO_FREQ":   0x08,   # RW
    "NCO_PHASE":  0x0C,   # RW
    "EXC_AMP":    0x10,   # RW
    "BIAS_CODE":  0x14,   # RW  [19:0] → dac_1220.code ★ 2026-10-05 已实现,见 BIAS_* 常量
                          #     ⚠️ **20 位不是 16 位**。写 = 立即发 1 拍 set_in 脉冲
                          #        (dac_1220 沿触发 ⇒ 连写同一个值不会丢)
    "RANGE_SEL":  0x18,   # RW
    "RELAY_CFG":  0x1C,   # RW
    "CIC_DECIM":  0x20,   # RW
    "SETTLE_CFG": 0x24,   # RW
    "I_A":        0x28,   # RO  有符号
    "Q_A":        0x2C,   # RO  有符号
    "I_B":        0x30,   # RO  有符号
    "Q_B":        0x34,   # RO  有符号
    "MAG_A":      0x38,   # RO
    "PHASE_A":    0x3C,   # RO
    "MAG_B":      0x40,   # RO
    "PHASE_B":    0x44,   # RO

    # ---- 存活寄存器(已实测)----------------------------------------------
    "VERSION":    0x48,   # RO  位流应返回 0x00010000
    "SCRATCH":    0x4C,   # RW  链路自检回环

    # ---- 显示通路(2026-10-02 落地)---------------------------------------
    "LCD_CTRL":   0x50,   # RW  [1:0]src_sel ★ 见 SRC_* 常量
    "LCD_FILL":   0x54,   # RW  [23:0]整屏填充色 RGB888(src_sel==1 时生效)
    "FB_BASE0":   0x58,   # RW  帧缓存 0 **物理**基址(PS 分配后写入)
    "FB_BASE1":   0x5C,   # RW  帧缓存 1 物理基址
    "LCD_SWAP":   0x60,   # RW  写 [0]=目标索引(1 拍脉冲);读 [0]当前前台 [1]pending
    "FB_STATUS":  0x64,   # RW  ★ 读 = 状态;写(任意值)= **清粘滞位**(1 拍脉冲)
                          #     [0]front [1]swap_pending [10:2]vblank_cnt [18:11]swap_cnt
                          #     [19]underrun(粘滞) [20]axi_err(粘滞)  ← 两者都由**写 0x64** 清
                          # ⚠️ 文档曾写"写 0x60 清",RTL 此前也未实现 ⇒ axi_err 恒为 1。
                          #    2026-10-03 起:清除地址 **0x64**,RTL 已实现。

    # ---- 触摸通路(2026-10-02 落地)---------------------------------------
    "TOUCH_STATUS": 0x68,  # RO  ★ 一次触摸事件的**提交点**,必须最先读
    "TOUCH_PT0":    0x6C,  # RO  读的是快照(非直连上报字)
    "TOUCH_PT1":    0x70,  # RO  同上

    # ---- 调试观测(2026-10-03 落地,只为排查 src=2 黑屏)-------------------
    # 上板故障:src=2(DDR 帧缓存)全黑,而 src=0/1 正常。PS 侧能读到的 FB_STATUS
    # 对两种**完全不同**的故障给出**相同读数**(黑屏 + underrun=0):
    #     假设 A:AXI 读事务没拿到数据(HP 口 / SmartConnect 层面)
    #     假设 B:数据拿到了,但乒乓选错缓冲,显示侧读到没写过的全零 BRAM
    # ⇒ 加这三个寄存器把二者分开。判决表见 DBG2_BIT 的注释。
    # ★ 2026-10-03 二次修订 0x74:拍数证明是满帧(153600)屏却仍全黑 ⇒
    #   改为「低 18 位拍数 + 高 14 位内容证据」,见 DBG1_BIT。
    "DBG_BEATS":  0x74,   # RO  拍数 + **数据内容证据**,见 DBG1_BIT
    "DBG_ARADDR": 0x78,   # RO  最近一次 AR 握手成功时的地址
    "DBG_STATE":  0x7C,   # RO  状态快照,见 DBG2_BIT

    # ---- 模拟前端(2026-10-05 落地)--------------------------------------
    "DDS_FREQ":   0x80,   # RW  [27:0] → adc_9833.code ★ 见 DDS_* 常量
                          #     ⚠️ 是 0x80,靠 8→**8 位地址宽度**才够得着
                          #     复位值 = 1.0000 MHz,与 adc_9833 的初始化序列**逐位相同**

    "CV_CTRL":    0x84,   # RW  [0] dac_rst_n ★ 2026-10-05
                          #     🔴 **复位值 = 0 = dac_1220 被按在复位态,不跑初始化。**
                          #     软件必须在 apply_fclk0() 把 FCLK0 补到 50 MHz **之后**
                          #     写 1 放行 —— 否则三段复位图案会超出手册双向窗口上界,
                          #     复位不触发、DIR 写入无效、输出停在 −10 V。见 DAC_RST_*。
                          #     ⚠️ 不是 0x18:0x18 是 RANGE_SEL 的规格位置(测量主链)。

    # ---- AD9238 采集通路(2026-10-06 落地)------------------------------
    # 📌 与 C 侧的镜像:`src/c_ui/app/pynq_regs.h` 的 CV_REG_ADC_*。
    #    **改这里要同时改那里**,两处不一致的后果是"写进去了、没反应、不报错"。
    "ADC_CFG":      0x88, # RW  [7:0]DIV [15:8]PH_OFF(有符号) [16]CONT [17]OEB_N [18]PDN
                          #     复位值 = 0x0000_000D ⇒ DIV=13 → 130/13 = **10 MSPS**,
                          #     PH_OFF=0,单次模式,输出使能,不掉电。
                          #     ⇒ 采样率 = 130 MHz / DIV,档位见 ADC_DIV_*。
    "ADC_CTRL":     0x8C, # W   [0]GO —— **命令**(1 拍脉冲),读回恒 0。
                          #     ⚠️ 与其它寄存器"整字存"的语义不同,写 1 触发一次采集。
    "ADC_STS":      0x90, # RO  [0]mmcm_locked [1]done [2]busy [3]otr_sticky
                          #     busy=1 期间读 ADC_BUF_DATA 拿到的是**上一次**的残留
                          #     —— 必须等 done 才读(2026-10-06 踩过这个坑)。
    "ADC_BUF_IDX":  0x94, # RW  [11:0] 采样缓冲读指针(4096 深)
    "ADC_BUF_DATA": 0x98, # RO  [11:0]sample [12]OTR
}

# 0x74 的位布局(2026-10-03 二次修订)。出处:RTL `lcd_fb_reader.v` §6。
DBG1_BIT = {
    "beats_cnt":  (17, 0),    # 本帧 m_axi_rvalid 拍数(满帧 153600)
    "rresp_lat":  (19, 18),   # 本帧最后一次非 OKAY 响应:00 无错 / 10 SLVERR / 11 DECERR
    "axi_or_nz":  20,         # 本帧任一拍 rdata 有任一比特为 1
    "px_or_nz":   21,         # 本帧 px_data 出现过非零(行缓冲确实读出了东西)
}
DBG1_BEATS_MASK = 0x3FFFF

# DECERR/SLVERR 的判决:rresp_lat 与 axi_or_nz 一眼分开两种"拍数满帧却全黑"
RRESP_NAME = {0b00: "OKAY", 0b01: "EXOKAY", 0b10: "SLVERR", 0b11: "DECERR"}

# ---------------------------------------------------------------------------
# 显示源选择 —— ★ 值 2 才是帧缓存(见文件头"错 1")
# ---------------------------------------------------------------------------
SRC_BARS = 0    # 厂商彩条(复位默认,不依赖 DDR/PS 的回归路径)
SRC_FILL = 1    # 整屏填充 LCD_FILL
SRC_DDR  = 2    # **DDR 帧缓存**(lcd_fb_reader 取行)—— 本工程主路径

# ---------------------------------------------------------------------------
# 位域
# ---------------------------------------------------------------------------
CTRL_BIT = {
    "start":     0,
    "soft_rst":  1,
    "ac_en":     2,
    "relay_en":  3,
    "cal_mode":  4,
}

STATUS_BIT = {
    "busy":      0,
    "done":      1,
    "fault":     2,
    "err_code":  (8, 15),    # (msb, lsb)
    "meas_cnt":  (16, 31),
}

# ⚠️ 位定义出自 RTL:`cv_axi_lite_slave.v` 的 sts_lcd_status 注释块 + `lcd_vblank_ctrl.v`
FB_STATUS_BIT = {
    "front":       0,
    "vblank_cnt":  (10, 2),
    "swap_cnt":    (18, 11),
    "underrun":    19,        # 行缓冲欠载(粘滞,PS **写 0x64** 清)
    "axi_err":     20,        # AXI 读响应非 OKAY(粘滞,同样写 0x64 清)
}

# ⚠️ 位定义出自 RTL:`lcd_fb_reader.v` §6 的 dbg_state 拼接(2026-10-03)
#    pulse 类位由 RTL 在 **frame_start 清零** ⇒ 读到的永远是"本帧内是否发生过",
#    PS 不必抢拍,随便什么时候读都行。
DBG2_BIT = {
    "state":           (1, 0),    # 0=IDLE 1=AR 2=DATA
    "rd_sel":          2,         # ★ 假设 B 会看到它恒为 0
    "wr_sel":          3,
    "buf_ready":       (5, 4),
    "fetch_pend":      6,
    "fetch_en":        7,         # = (src_sel == 2)
    "rresp_bad_frm":   8,         # 本帧出现过 rresp != OKAY
    "rvalid_frm":      9,         # 本帧收到过读数据
    "arready_frm":     10,        # 本帧 AR 握手成功过
    "arvalid_frm":     11,        # 本帧发起过 AR
    "line_start_frm":  12,        # 本帧 line_start 出现过 ← 若为 0,问题在 lcd_timing
    "de_req_frm":      13,
    "wdt_expired_frm": 14,
    "both_ready_frm":  15,        # 本帧两块行缓冲都装满过
    "wdt_expire_cnt":  (23, 16),  # 本帧看门狗超时次数(饱和 255)
    "burst_idx":       (27, 24),
    "beat_cnt":        (31, 28),
}
# 📋 判决表(src=2 全黑时,先读 DBG_BEATS 再读 DBG_STATE)
#     beats_cnt == 0   且 DBG_STATE["arvalid_frm"] == 1
#         ⇒ AR 发出去了、一拍数据都没回来 ⇒ **假设 A**(HP / SmartConnect 层面)
#     beats_cnt ≈ 153600 且 axi_or_nz == 1 且 px_or_nz == 1
#         ⇒ 数据进了 PL、也读出来了 ⇒ 问题在**下游**(de_act 选通 / 像素 mux)
#     beats_cnt ≈ 153600 且 axi_or_nz == 1 且 px_or_nz == 0
#         ⇒ 数据是好的但行缓冲没取出来 ⇒ 问题在本模块 **§4/§5**(读侧相位 / 乒乓)
#     beats_cnt ≈ 153600 且 axi_or_nz == 0
#         ⇒ **数据本身就是全零** ⇒ 从设备拒了地址(看 rresp_lat 是不是 11/DECERR);
#           握手照常完成,所以拍数看不出问题 —— 这是第 34 次位流才分出来的情况
#     DBG_STATE["line_start_frm"] == 0
#         ⇒ line_start 根本没动,问题在 lcd_timing,与 fb_reader 无关
FULL_FRAME_BEATS = 256 * 600    # 153600,一行 256 拍 × 600 行

# ⚠️ 位定义出自 RTL:`gt911_ctrl.v:278-280`
#     sts_touch_status <= {16'b0, seq, 3'b0, err_sticky, pen_down, cur_count};
#     sts_touch_pt0    <= {4'b0, pt0_y, 4'b0, pt0_x};
TOUCH_STATUS_BIT = {
    "count":     (2, 0),      # 触点数
    "pen_down":  3,
    "err":       4,           # 粘滞错误标志
    "seq":       (15, 8),     # 每产生一个**新坐标帧**递增(不是"读一次递增")
}
TOUCH_PT_BIT = {
    "x": (11, 0),
    "y": (27, 16),
}

# 有符号寄存器:裸 mmio 读回来是无符号,必须做二补数转换
SIGNED_REGS = ("I_A", "Q_A", "I_B", "Q_B")

# ---------------------------------------------------------------------------
# 模拟前端换算(2026-10-05)
# ---------------------------------------------------------------------------
# 🔴 DAC1220 → 偏压:板上**有一级外部高压放大,比例 10:1**。
#    即 DAC 模块出 10 V,被测器件上就是 100 V。**这一级不在寄存器里** ——
#    寄存器只管 20 位码值,倍数完全由硬件接线决定,软件改不了。
#
# 模块级传递函数(板上三点实测过 −5 / 0 / +5 V,见 docs/dac1220_interface.md):
#     V_module = -10 + 20 * code / 1048575
#
# 三个端点(右边都乘了 10):
#     code = 0        → V_module = -10.0 V → 偏压 **-100.0 V**  ← ⚠️ 危险端,不是 0 V!
#     code = 0x80000  → V_module =   0.0 V → 偏压    0.0 V      ← ★ 复位值取这里
#     code = 0xFFFFF  → V_module = +10.0 V → 偏压 +100.0 V
#
# ⚠️ 复位值之所以取**中位**而不是 0:码值 0 会让上电偏压停在 **-100 V**,
#    而"0 码"极易被误读成"0 V"。中位是唯一不依赖高压级极性的安全点。
#    RTL 侧的落地见 cv_axi_lite_slave.v 的 bias_code 复位值 + 上电 set 脉冲。
BIAS_CODE_MAX    = 0xFFFFF       # 20 位满量程(1048575)
BIAS_CODE_MID    = 0x80000       # 复位值 = 0 V 偏压
BIAS_MODULE_VMIN = -10.0         # 模块级端点(V)
BIAS_MODULE_VMAX = +10.0
BIAS_HV_RATIO    = 10.0          # ★ 外部高压级倍数(硬件接线,非寄存器)
BIAS_VOLT_MIN    = BIAS_MODULE_VMIN * BIAS_HV_RATIO    # -100.0 V
BIAS_VOLT_MAX    = BIAS_MODULE_VMAX * BIAS_HV_RATIO    # +100.0 V


def bias_volts_to_code(volts):
    """偏压(V,**含** 10:1 高压级)→ 20 位 DAC 码。超出 ±100 V 夹到端点。"""
    module_v = volts / BIAS_HV_RATIO
    raw = (module_v - BIAS_MODULE_VMIN) * BIAS_CODE_MAX / \
          (BIAS_MODULE_VMAX - BIAS_MODULE_VMIN)
    return max(0, min(BIAS_CODE_MAX, int(round(raw))))


def bias_code_to_volts(code):
    """20 位 DAC 码 → 偏压(V,**含** 10:1 高压级)。把读回值翻成人看的数。"""
    module_v = BIAS_MODULE_VMIN + \
        (BIAS_MODULE_VMAX - BIAS_MODULE_VMIN) * (int(code) & BIAS_CODE_MAX) / BIAS_CODE_MAX
    return module_v * BIAS_HV_RATIO


# 激励频率:AD9833 板载晶振 **25 MHz,焊死,不可由 FPGA 供 MCLK**(决策 D10)
#     f_out = code * 25 MHz / 2^28
# ⚠️ 决策 D08 定激励**默认 1 MHz**。
#    🔄 2026-10-06 修订:原先写"固定 1 MHz、界面没有频率控件"。用户明确要求把频率
#       接口引出来,现在 **c_ui 的「系统复位」页有五档按钮**(100k/500k/1M/2M/5M)。
#       C 侧实现在 `src/c_ui/app/measure_config.c`,换算常量在 `pynq_regs.h`。
#       **1 MHz 仍是默认值与复位值**,D08 的"默认"含义不变,只是不再"固定"。
DDS_MCLK_HZ    = 25_000_000
DDS_PHASE_BITS = 28
DDS_FREQ_1MHZ  = 0x0A3D70A       # 10737418 ← round(1e6 * 2^28 / 25e6)
DDS_HZ_CHOICES = (100_000, 500_000, 1_000_000, 2_000_000, 5_000_000)


def dds_hz_to_code(hz):
    """激励频率(Hz)→ 28 位 AD9833 频率码。夹到 [0, MCLK/2]。"""
    raw = int(round(float(hz) * (1 << DDS_PHASE_BITS) / DDS_MCLK_HZ))
    return max(0, min((1 << DDS_PHASE_BITS) - 1, raw))


# ---------------------------------------------------------------------------
# AD9238 采样率与相位(0x88)—— 2026-10-06
# ---------------------------------------------------------------------------
ADC_CLK_BASE_HZ = 130_000_000     # MMCM:VCO = 50×26/1 = 1300 MHz,CLKOUT0 = 130 MHz
ADC_DIV_DEFAULT = 13              # → 10.0 MSPS,与 RTL 0x88 复位值一致
ADC_BUF_N       = 4096            # 单次采集点数(采集时长 = DIV × 4096 / 130 MHz)

# 界面/脚本可用档位。⛔ 不给 DIV=5(26 MSPS)及以上 —— 决策 D13 定 f_s ≤ 20 MSPS。
# ⚠️ **20 MSPS 取不到**:130/20 = 6.5,不是整数。所以这套硬件最高只能到 13 MSPS。
ADC_DIV_CHOICES = (
    (130, 1.0), (65, 2.0), (26, 5.0), (13, 10.0), (10, 13.0),
)   # (DIV, MSPS)


def adc_sample_hz(div):
    """分频比 → 采样率(Hz)。`P = max(div, 2)` 与 adc_capture.v 一致。"""
    p = max(int(div), 2)
    return ADC_CLK_BASE_HZ / p


def adc_phase_center(div):
    """分频比 → 端口 A 采样窗中心(PH_OFF,tick)。

    = 1.5 − P/4,四舍五入到整数 tick。**三点实测吻合**(2026-10-06):
        DIV=13 → −2      DIV=26 → −5      DIV=130 → −31

    🔴 **换采样率必须同时换 PH_OFF。** 窗口位置随 P 走,只改 DIV 会让采样点
       滑出被驱动的窗口 —— 现象是"换了速率就没波形",而不是报错。
       窗 = tick ∈ [2 − P/2, +1],宽 P/2(P=13 时就是 −4…+1 这 6 拍)。
    """
    p = max(int(div), 2)
    # ⚠️ 用整数运算,**与 pynq_regs.h 的 cv_adc_phase_center() 逐位等价**。
    #    别图省事写成 int(round((6-p)/4)):Python 的 round() 是"四舍六入五取偶",
    #    而 C 侧是"五away from zero",在 (6-P) ≡ 2 (mod 4)(即 P=4, 8, 12 …)上
    #    两边会差 1 —— 这种分歧在"查了半天波形对不上"的时候极难定位。
    n = 6 - p
    return (n + 2) // 4 if n >= 0 else -((-n + 2) // 4)

def dds_code_to_hz(code):
    """28 位频率码 → 激励频率(Hz)。"""
    return (int(code) & ((1 << DDS_PHASE_BITS) - 1)) * DDS_MCLK_HZ / \
           float(1 << DDS_PHASE_BITS)


# ---------------------------------------------------------------------------
# CV_CTRL(0x84)—— dac_1220 初始化的软件放行位
# ---------------------------------------------------------------------------
CV_CTRL_DAC_RST_N = 0x1          # [0] 1 = 放行(开始初始化)/ 0 = 按在复位

# 🔴 **FCLK0 能落在哪个区间,这个 DAC 才初始化得起来** —— 由 dac_1220.v 的
#    复位图案常数与手册的双向窗口反解得到,三个约束取交集:
#
#      t16 = 13000 拍 / f ∈ [208.3, 325.5] µs  ⇒  f ∈ [39.9, 62.4] MHz
#      t18 = 28500 拍 / f ∈ [416.7, 732.4] µs  ⇒  f ∈ [38.9, 68.4] MHz
#      t19 = 45000 拍 / f ∈ [833.3, 976.6] µs  ⇒  f ∈ [46.1, 54.0] MHz
#      ─────────────────────────────────────────────────────────────
#      **交集 f ∈ [46.1, 54.0] MHz**  ← 最紧的是 t19
#
#    ⇒ 50.000 MHz(设计值)在窗口正中,余量 ±7.9%。
#    ⚠️ 板上 BOOT.BIN 的默认 **27.78 MHz 远在窗口外** —— 这正是
#       「偏压一直是 −10 V」的真因:复位根本没触发。
#    ⚠️ **这个窗口不能靠改常数拓宽**:把窗口折成【拍数】,两个频率下的
#       合法区间【三段全都不重叠】——
#                   @27.78 MHz      @50 MHz
#         t16        5786~9042      10415~16275
#         t18       11575~20344     20835~36620
#         t19       23147~27128     41665~48830
#       ⇒ 没有一组常数能同时覆盖两个频率。**只能保证初始化在设计频率下跑。**
FCLK0_DAC_RST_WINDOW_MHZ = (46.1, 54.0)

# 放行之后,DAC 多久才真的能接受偏压码(单位 ms,@50 MHz)。
#   由 dac_1220.v 的三个参数反推,全部是**下限型**(慢一点无害):
#     PWR_WAIT    = 10 000 000 拍 = 200.0 ms  (手册要求晶振起振 ≥25 ms)
#     init_delay  =     87 500 拍 =   1.75 ms (六段复位图案)
#     CAL_WAIT_T  = 30 000 000 拍 = 600.0 ms  (手册自校准 300~500 ms)
#   ⇒ 合计 ≈ 801.8 ms。这一段里到来的 set_in 会被 dac_1220 的 set_pending
#     锁住,等它就绪再消费 —— **不会丢**,所以"先写 0x14 再放行"是安全的。
DAC_INIT_MS = 801.8

# ---------------------------------------------------------------------------
# 面板与帧缓存布局(与 `docs/display_subsystem.md` §3/§4 一致)
# ---------------------------------------------------------------------------
PANEL_WIDTH   = 1024
PANEL_HEIGHT  = 600
BYTES_PER_PX  = 2                       # RGB565
FB_FRAME_SIZE = PANEL_WIDTH * PANEL_HEIGHT * BYTES_PER_PX   # 1 228 800 B
FB_ALIGNMENT  = 0x10000                 # 64 KB
# 单槽(向上对齐到 64 KB):两块帧缓存之间必须有这个间隔,否则第二块会压到第一块尾部
FB_SLOT_SIZE  = (FB_FRAME_SIZE + FB_ALIGNMENT - 1) & ~(FB_ALIGNMENT - 1)   # 1 245 184 B

# ---------------------------------------------------------------------------
# 时钟
# ---------------------------------------------------------------------------
IO_PLL_MHZ         = 1000.0      # ★ 实测。2026-10-03 用三点帧率反推一致(±0.2%):
                                 #   DIV0=6/DIV1=6 → 27.78 | DIV0=30 → 33.33 | DIV0=20 → 50.0
FCLK0_DESIGN_MHZ   = 50.0        # 本工程固化值(见 main.py → drivers/board.apply_fclk0)
FCLK0_BOOT_MHZ     = 27.78       # 板上 BOOT.BIN 留下的默认值(寄存器 0x00600600:DIV0=6/DIV1=6)
                                 #   ⚠️ 这**低于**屏窗口下限 40.8 ⇒ 开机默认画面不受支持
FCLK0_PYNQ_50_MHZ  = 33.33       # `Clocks.fclk0_mhz = 50` 的**真值**(PYNQ 按 IO PLL 1500 算 ⇒ 差 1.5×)
FCLK0_DIV0_FOR_50  = 20          # = round(IO_PLL_MHZ / 50) —— 真 50 MHz 必须写这个 DIV0
PCLK_WINDOW_MHZ    = (40.8, 67.2)  # 屏手册允许窗口 —— FCLK0 落在窗口外屏会无图

# ---------------------------------------------------------------------------
# PL 侧实现状态 —— 写代码时用来决定"这个功能能不能真调"
# ---------------------------------------------------------------------------
# 🆕 2026-10-02 第 31 次位流(含显示 + 触摸)后更新。
#    以 RTL 为准:cv_axi_lite_slave.v 里已落到寄存器逻辑的有 VERSION / SCRATCH /
#    LCD_CTRL / LCD_FILL / FB_BASE0 / FB_BASE1 / LCD_SWAP / FB_STATUS /
#    TOUCH_STATUS / TOUCH_PT0 / TOUCH_PT1。
#    测量主链(CTRL…PHASE_B)对应的 RTL 模块尚未进 BD。
#    🆕 2026-10-03 第 33 次位流追加 DBG_BEATS / DBG_ARADDR / DBG_STATE(0x74/0x78/0x7C),
#       并把 FB_STATUS(0x64)的**写**变成"清粘滞"命令。
#    🆕 2026-10-05 位流追加 **BIAS_CODE(0x14)** 与 **DDS_FREQ(0x80)**
#       (DAC1220 偏压 + AD9833 激励),同时把地址宽度 7→8。
#    🆕 2026-10-05 追加 **CV_CTRL(0x84)** —— dac_1220 初始化的软件放行位。
#       原因:FCLK0 开机默认 27.78 MHz,三段复位图案全部超出手册双向窗口上界
#       ⇒ 复位不触发 ⇒ 偏压恒 −10 V。故把 DAC 的 rst_n 交给软件在改频之后放行。
#    🆕 2026-10-06 追加 **ADC_CFG(0x88)/ADC_CTRL(0x8C)/ADC_STS(0x90)/
#       ADC_BUF_IDX(0x94)/ADC_BUF_DATA(0x98)** —— AD9238 采集通路。
#       该版已实测:采出周期正好 10 点的 1 MHz 正弦(10 MSPS),D0/D11 位序正确。
PL_IMPLEMENTED_REGS = (
    "VERSION", "SCRATCH",
    "BIAS_CODE", "DDS_FREQ", "CV_CTRL",
    "ADC_CFG", "ADC_CTRL", "ADC_STS", "ADC_BUF_IDX", "ADC_BUF_DATA",
    "LCD_CTRL", "LCD_FILL", "FB_BASE0", "FB_BASE1", "LCD_SWAP", "FB_STATUS",
    "TOUCH_STATUS", "TOUCH_PT0", "TOUCH_PT1",
    "DBG_BEATS", "DBG_ARADDR", "DBG_STATE",
)
PL_HAS_LCD   = True      # lcd_timing / lcd_fb_reader / lcd_vblank_ctrl 已进位流
PL_HAS_TOUCH = True      # i2c_master / gt911_ctrl 已进位流
PL_HAS_DAC   = True      # dac_1220(偏压)已进位流 —— 2026-10-05
PL_HAS_DDS   = True      # adc_9833(激励)已进位流 —— 2026-10-05
PL_HAS_ADC   = True      # AD9238 采集(adc_capture)已进位流 —— 2026-10-06
# ⚠️ 但**测量主链仍不在**:DLIA / CORDIC / 继电器 / 扫压编排都还没有,
#    所以"设置偏压"能改硬件输出,"开始扫压"目前只能改变偏压(测不到 C 值)。

# ---------------------------------------------------------------------------
# 板上路径与运行约束
# ---------------------------------------------------------------------------
# ⚠️ 这些是【板上】的路径,不是开发 PC 上的。
# PYNQ 要求 .bit 与 .hwh 主干同名且放在同一目录,Overlay() 才能解析出 ip_dict。
BOARD_HOME   = "/home/xilinx"
OVERLAY_BIT  = "/home/xilinx/qrs.bit"
C_UI_BINARY  = "/home/xilinx/c_ui_src/c_ui"
#   ⚠️ 2026-10-03 板上实测修正:原写 `/home/xilinx/c_ui`,**该路径在板上不存在**。
#      `src/c_ui/Makefile` **没有 install 目标**,二进制就在构建目录原地产出,
#      部署后落在 `/home/xilinx/c_ui_src/c_ui`(实测 `ls -l` 2516768 B)。
#      原值会让 `main.py` 不带 `--ui-bin` 时直接找不到界面。
#      改常量而非加 install 目标:部署路径已是既成事实,加目标只为迁就一个错值。

# 已知的板级/工具链约束(踩过的坑,写在这里免得忘):
#   · 上板必须 `sudo -E python3` —— bare sudo 会清掉环境变量,报 No Devices Found
#   · 屏幕上电默认走厂商彩条(src_sel 复位 = 0);要出画面必须显式写 SRC_DDR
