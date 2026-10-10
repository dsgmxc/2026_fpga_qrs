"""
cv_monitor.py —— S1 通用 Driver(PS 侧寄存器访问层)

职责边界:
  · 本文件是 **唯一** 直接碰 mmio / 寄存器偏移 / 位域的地方
  · 不做任何业务判断(扫描、校准、反演、页面逻辑都不在这里)
  · 显示 / 触摸的**语义**(换址时序、bit7 语义)在 `drivers/lcd.py` / `drivers/touch.py`

三个必须遵守的约定:
  1. 用了 bindto 的类必须在 Overlay() 之前 import,否则绑定不生效
  2. 寄存器偏移只在 `cv_regs.REG` 里定义一次,这里不写魔法数
  3. 回调给上层的原始值保持"裸"形态;位域解析集中在本文件的 `*_bits()` 里

⚠️ 关于 register_map:
   实测 `module_ref` 型 IP 的 `registers` 是**空字典**(RTL 模块引用没有 AXI 寄存器标注),
   ⇒ `self.register_map.XXX` 会 AttributeError,**只能走裸 mmio**。
   见 `report/measurements/20260929_scratch_loopback.md`(那次测的是旧顶层实例名)。
"""

from pynq import DefaultIP

import cv_regs


def to_signed32(value):
    """裸 mmio 读回来是 0 ~ 2^32-1,有符号寄存器必须转二补数,否则负值读成 40 亿。"""
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def extract(raw, field):
    """按 `(msb, lsb)` 或 `bit` 取出位域。两种写法都支持,免得调用方记两套。"""
    if isinstance(field, tuple):
        hi, lo = field
        return (raw >> lo) & ((1 << (hi - lo + 1)) - 1)
    return (raw >> field) & 1


class CVMonitor(DefaultIP):
    """cv_axi_lite_slave 的 PS 侧驱动(挂在 BD 顶层 module_ref 上)。

    DefaultIP 已经提供了 self.mmio(base_addr, length)。
    """

    # 🔴 必须与 BD 里**顶层** module_ref 的 VLNV 逐字一致。
    #    对不上时 PYNQ **静默**退化成 DefaultIP —— 不报错,也没有 link_check()。
    #    顶层名只在 `cv_regs` 定义一次(`TOP_MODULE` / `TOP_MODULE_VLNV` 两个)。
    #    地址仍是 0x43C00000 / 4 KB(`cv_sys` 对 S_AXI 是直通、无偏移)⇒ 寄存器偏移未变。
    bindto = [cv_regs.TOP_MODULE_VLNV]

    # -----------------------------------------------------------------------
    # 低层:唯一碰 mmio 的地方
    # -----------------------------------------------------------------------
    def _read(self, name):
        """按寄存器名读。未知名字直接 KeyError —— 不要静默返回 0。"""
        return self.mmio.read(cv_regs.REG[name])

    def _write(self, name, value):
        self.mmio.write(cv_regs.REG[name], value & 0xFFFFFFFF)

    # -----------------------------------------------------------------------
    # 自检 —— 这两个寄存器是活的,可以上板验证链路
    # -----------------------------------------------------------------------
    def version(self):
        """读版本号。当前位流应返回 0x00010000。"""
        return self._read("VERSION")

    def loopback(self, value):
        """写 SCRATCH 再读回。value 要取非常数,否则排除不了"恒返回常数"的假通过。"""
        self._write("SCRATCH", value)
        return self._read("SCRATCH")

    def link_check(self, verbose=True):
        """链路自检:版本 + 换值回环。返回 True 表示 PS→PL→PS 通路是通的。"""
        ver = self.version()
        w1, w2 = 0xDEADBEEF, 0xA5A55A5A
        r1 = self.loopback(w1)
        r2 = self.loopback(w2)
        ok = (ver == 0x00010000) and (r1 == w1) and (r2 == w2)
        if verbose:
            print(f"[cv_monitor] VERSION  = 0x{ver:08X}  (expect 0x00010000)")
            print(f"[cv_monitor] SCRATCH  = 0x{r1:08X} / 0x{r2:08X}  "
                  f"(expect 0x{w1:08X} / 0x{w2:08X})")
            print(f"[cv_monitor] link_check: {'PASS' if ok else 'FAIL'}")
        return ok

    # -----------------------------------------------------------------------
    # 未实现寄存器的护栏
    # -----------------------------------------------------------------------
    def _require(self, name):
        if name not in cv_regs.PL_IMPLEMENTED_REGS:
            raise NotImplementedError(
                f"寄存器 {name} 在 PL 侧尚未实现。裸 mmio 会写进「黑洞」:"
                f"读回 0、写被忽略,不会报错 —— 所以这里主动拦住。"
                f"当前已实现:{cv_regs.PL_IMPLEMENTED_REGS}"
            )

    # -----------------------------------------------------------------------
    # 显示通路 —— 原始访问。换址时序等语义在 drivers/lcd.py
    # -----------------------------------------------------------------------
    def lcd_set_source(self, src):
        """写 LCD_CTRL[1:0]。取值用 cv_regs.SRC_BARS / SRC_FILL / SRC_DDR。"""
        self._require("LCD_CTRL")
        self._write("LCD_CTRL", src & 0x3)

    def lcd_get_source(self):
        return self._read("LCD_CTRL") & 0x3

    def lcd_set_fill_rgb(self, rgb888):
        """设整屏填充色(src_sel == SRC_FILL 时才看得见)。"""
        self._require("LCD_FILL")
        self._write("LCD_FILL", rgb888 & 0xFFFFFF)

    def lcd_set_fb_base(self, index, phys_addr):
        """写帧缓存物理基址。index 取 0 或 1。

        ⚠️ 地址必须** 64 KB 对齐**且是**物理地址** —— 传虚拟地址会读到一片无关内存,
           屏上表现为花屏,不会报错。
        """
        name = "FB_BASE0" if index == 0 else "FB_BASE1"
        self._require(name)
        self._write(name, phys_addr)

    def lcd_get_fb_base(self, index):
        return self._read("FB_BASE0" if index == 0 else "FB_BASE1")

    def lcd_request_swap(self, index):
        """请求在**下一个 VBlank** 把前台切到 index。

        ⚠️ 这是「受理」不是「生效」—— 生效要看 `lcd_swap_state()` 的 back 字段。
        """
        self._require("LCD_SWAP")
        self._write("LCD_SWAP", index & 0x1)

    def lcd_swap_state(self):
        """读回 LCD_SWAP。返回 (front, pending)。

        front   = 已生效的前台索引
        pending = 已受理但还没到 VBlank 的请求(1 表示"请求在路上")

        ⚠️ 读的是 `sts_lcd_status[1:0]` 而非从机本地锁存 —— pending 的真值持有者是
           `lcd_vblank_ctrl`(决策 5.3 第 1 条:读回必须是「已生效」值)。
        """
        raw = self._read("LCD_SWAP")
        return raw & 0x1, (raw >> 1) & 0x1

    def lcd_status_raw(self):
        return self._read("FB_STATUS")

    def lcd_status(self):
        """解析 FB_STATUS。返回 dict —— 字段名与 `cv_regs.FB_STATUS_BIT` 一致。"""
        raw = self.lcd_status_raw()
        out = {k: extract(raw, f) for k, f in cv_regs.FB_STATUS_BIT.items()}
        out["raw"] = raw
        return out

    def lcd_clear_sticky(self):
        """清 `underrun` / `axi_err` 两个粘滞位 —— **写 0x64**(写什么都行)。

        ⚠️ 清除地址是 **0x64(FB_STATUS)**,与读同址。文档曾写"写 0x60 清",
           而 RTL 在 2026-10-03 之前**根本没实现**清除路径 ⇒ `axi_err` 上电即为 1
           且永远清不掉。第 33 次位流起落地在 0x64。

        ⚠️ 必须在 `LCD_CTRL != SRC_DDR` 时清才有意义 —— 否则 PL 正在取数,
           下一个坏响应会立刻把它置回 1。
        """
        self._write("FB_STATUS", 1)

    def lcd_dbg_raw(self):
        """读三个调试观测寄存器。返回 `(beats, araddr, state)` 原始值。"""
        return (self._read("DBG_BEATS"),
                self._read("DBG_ARADDR"),
                self._read("DBG_STATE"))

    def lcd_dbg(self):
        """解析调试观测。返回 dict —— 判决表见 `cv_regs.DBG2_BIT` 下方注释。

        `verdict` 字段直接给结论,省得每次上板重推一遍:
            "A_axi_no_data"   —— AR 发出去了但一拍数据都没回来(HP / SmartConnect)
            "C_ddr_zero"      —— 拍数满帧但 **rdata 全零** ⇒ 从设备拒了地址(DECERR)
            "D_readside"      —— 数据是好的,但行缓冲没取出来(读侧相位 / 乒乓)
            "E_downstream"    —— 数据进了 PL 也读出来了 ⇒ 问题在 de_act 选通 / 像素 mux
            "no_line_start"   —— line_start 没动,问题在 lcd_timing
            "idle_ok"         —— 数据在流,没看出异常
        """
        beats, araddr, state = self.lcd_dbg_raw()
        out = {k: extract(state, f) for k, f in cv_regs.DBG2_BIT.items()}
        out.update({k: extract(beats, f) for k, f in cv_regs.DBG1_BIT.items()})
        out["axi_or_nz"] = bool(out["axi_or_nz"])
        out["px_or_nz"] = bool(out["px_or_nz"])
        out["rresp_name"] = cv_regs.RRESP_NAME.get(out["rresp_lat"], "?")
        out["beats"] = beats                      # 原始 32 位
        out["beats_n"] = out["beats_cnt"]         # 低 18 位 = 真实拍数
        out["araddr"] = araddr
        out["full_frame"] = cv_regs.FULL_FRAME_BEATS
        n = out["beats_n"]

        if not out["line_start_frm"]:
            out["verdict"] = "no_line_start"
        elif out["arvalid_frm"] and n == 0:
            out["verdict"] = "A_axi_no_data"
        elif n >= cv_regs.FULL_FRAME_BEATS * 0.9:
            if not out["axi_or_nz"]:
                out["verdict"] = "C_ddr_zero"
            elif not out["px_or_nz"]:
                out["verdict"] = "D_readside"
            else:
                out["verdict"] = "E_downstream"
        else:
            out["verdict"] = "idle_ok"
        return out

    # -----------------------------------------------------------------------
    # 触摸通路 —— 原始访问。bit7 语义 / 保持窗在 drivers/touch.py
    # -----------------------------------------------------------------------
    def touch_read_event(self):
        """★ 一次触摸事件的**原子读**。返回 (status_raw, pt0_raw, pt1_raw)。

        🔴 **读序固定 0x68 → 0x6C → 0x70,不可调换**(2026-10-02 第 31 次定)。

            理由:PS 要读满 3 个寄存器才拼得出一次触摸事件,而 `gt911_ctrl` 自由运行、
            来一帧就刷新上报字。三次读之间若跨过帧边界,会读出
            "**状态说 1 个点、坐标却是下一帧**"的**撕裂**。
            撕裂**不报错**,只让界面偶尔跳一下 —— 最难归因的那类 bug。

            从机把"读 0x68"当**提交点**,那一拍把 pt0/pt1 一并锁进 hold。
            ⇒ 先读坐标再读状态,拿到的是上一拍快照,与 `seq` 对不上。

        ⚠️ 本方法只保证"这三个数来自同一次提交",**不**保证它们是新的。
           判新旧请看 `seq`(由 `drivers/touch.py` 封装)。
        """
        status = self._read("TOUCH_STATUS")     # ★ 必须在最前
        pt0 = self._read("TOUCH_PT0")
        pt1 = self._read("TOUCH_PT1")
        return status, pt0, pt1

    @staticmethod
    def parse_touch_status(raw):
        return {k: extract(raw, f) for k, f in cv_regs.TOUCH_STATUS_BIT.items()}

    @staticmethod
    def parse_touch_point(raw):
        return {k: extract(raw, f) for k, f in cv_regs.TOUCH_PT_BIT.items()}

    # -----------------------------------------------------------------------
    # 测量主链(PL 侧尚未实现,见 cv_regs.PL_IMPLEMENTED_REGS)
    # -----------------------------------------------------------------------
    def set_bias(self, code):
        """写 BIAS_CODE(DAC1220 偏置码,[15:0])。"""
        self._require("BIAS_CODE")
        self._write("BIAS_CODE", code)

    # -----------------------------------------------------------------------
    # 继电器矩阵 / 量程挡位(2026-10-10 落地)
    #   ⚠️ 本层只做【裸寄存器访问】。组合语义、`0x1C` 缺陷的绕法、挡位的
    #      PS 侧稳定延时,全部在 `drivers/relay.py` —— 别在这里加。
    # -----------------------------------------------------------------------
    def relay_read(self):
        """读 0x1C,返回**已生效**的继电器组合位图([6:0] = K1..K7)。

        🔴 是「已生效」不是「刚请求」—— 真值由 `relay_ctrl.v` 的 `relay_reg` 持有,
           本层锁存的请求值不参与读出(见 `cv_axi_lite_slave.v` IDX_RELAY_CFG 读分支)。
        """
        self._require("RELAY_CFG")
        return self._read("RELAY_CFG") & 0xFF

    def relay_write_raw(self, value):
        """裸写 0x1C。

        ⚠️ **正常业务代码请用 `drivers/relay.py`** —— 它有组合白名单校验、挡位
           处理与读回确认,本层只有裸寄存器。

        ✅ **本层这条路是安全的。** 2026-10-10 曾观察到"一次写 0x1C 会让继电器停在
           0x00",当晚已定位:**根因是 PS 侧写通路用 `struct.pack_into` 时会发两笔
           AXI 事务**(先写一个 `0x00000000`,再写载荷),PL 侧完全正常。
           本层走 `pynq.MMIO`(numpy `uint32` 整字存)= **一笔**,不受影响。
           证据与修法:`report/measurements/20261010_relay_cfg_write_anomaly.md` §10。
        """
        self._require("RELAY_CFG")
        self._write("RELAY_CFG", value & 0xFF)

    def gear_read(self):
        """读 0x18,返回**已生效**的挡位独热码。"""
        self._require("RANGE_SEL")
        return self._read("RANGE_SEL") & 0xF

    def gear_write_raw(self, value):
        """裸写 0x18。⚠️ 语义同 `relay_write_raw`:挡位也没有稳定延时,
        PS 侧要自己等(见 `drivers/relay.py`)。"""
        self._require("RANGE_SEL")
        self._write("RANGE_SEL", value & 0xF)

    def settle_get(self):
        """读 0x24 整字。高 16 位是继电器稳定 ms,低 16 位是偏压稳定 ms(未消费)。"""
        self._require("SETTLE_CFG")
        return self._read("SETTLE_CFG")

    def settle_set(self, relay_ms=None, bias_ms=None, raw=None):
        """写 0x24。**读-改-写**:只动你指定的那一半,另一半原样保留。

        ⚠️ 必须读-改-写,不能整字写 —— 偏压那半([15:0],复位 0x05DC = 1500)
           虽然在 PL 侧还没人消费,但整字覆盖会把它抹成 0,等 `meas_fsm` 落地时
           会变成"为什么偏压稳定时间突然是 0"这种查不出源头的坏账。
        """
        self._require("SETTLE_CFG")
        if raw is not None:
            self._write("SETTLE_CFG", raw)
            return raw & 0xFFFFFFFF
        v = self._read("SETTLE_CFG")
        if relay_ms is not None:
            v = (v & 0x0000FFFF) | ((int(relay_ms) & 0xFFFF) << 16)
        if bias_ms is not None:
            v = (v & 0xFFFF0000) | (int(bias_ms) & 0xFFFF)
        self._write("SETTLE_CFG", v)
        return v

    # -----------------------------------------------------------------------
    # DAC1220 初始化放行(0x84 CV_CTRL)—— 2026-10-05
    #   ⚠️ 本层只做【裸寄存器访问 + 读回校验】。
    #      「FCLK0 在不在窗口内」那一层判断在 `drivers/board.py::release_dac()`
    #      —— 因为这里反向 import drivers 会成环,而那里本来就有 BoardError。
    # -----------------------------------------------------------------------
    def dac_rst_held(self):
        """读 CV_CTRL,返回 True 表示 DAC **仍被按在复位态**(0x84[0] == 0)。"""
        self._require("CV_CTRL")
        return (self._read("CV_CTRL") & cv_regs.CV_CTRL_DAC_RST_N) == 0

    def dac_release(self):
        """放行 dac_1220 的初始化(写 0x84[0] = 1)。幂等。

        返回 True = 这一次真的把 DAC 从复位态放了出来;
        返回 False = 调用前就已经是放行态(重复调用无害)。

        🔴 调用前请先确保 FCLK0 已是设计值 —— 用
           `drivers.board.release_dac(mon)`,它替你查这一条。
        """
        was_held = self.dac_rst_held()
        self._write("CV_CTRL", cv_regs.CV_CTRL_DAC_RST_N)
        # 读回校验:与 lcd.select / set_bias 同一个契约 —— 读回必须是【已生效】值
        if self.dac_rst_held():
            raise RuntimeError(
                "写 0x84 = 1 后读回仍是被按在复位态 —— 位流与代码不匹配。\n"
                "  该位是 2026-10-05 才加的;旧位流里 0x84 会被静默忽略(读回 0)。"
            )
        return was_held

    def set_decim(self, R):
        """写 CIC_DECIM(抽取率 R)。

        ⚠️ 硬性约束(清单 §3):测量中写入会破坏相干性,整次积分作废。
           硬件侧应"挂起到下一次 SET_BIAS 才生效",PS 侧不要在中途改写。
        """
        self._require("CIC_DECIM")
        self._write("CIC_DECIM", R)

    def status(self):
        """读 STATUS。返回 (raw, busy, done, fault, err_code, meas_cnt)。"""
        self._require("STATUS")
        raw = self._read("STATUS")
        return (
            raw,
            extract(raw, cv_regs.STATUS_BIT["busy"]),
            extract(raw, cv_regs.STATUS_BIT["done"]),
            extract(raw, cv_regs.STATUS_BIT["fault"]),
            extract(raw, cv_regs.STATUS_BIT["err_code"]),
            extract(raw, cv_regs.STATUS_BIT["meas_cnt"]),
        )

    def read_iq(self):
        """读四路解调结果,自动做有符号转换。"""
        for n in cv_regs.SIGNED_REGS:
            self._require(n)
        return {n: to_signed32(self._read(n)) for n in cv_regs.SIGNED_REGS}

    def read_mag_phase(self):
        """读 CORDIC 输出的模值与相位(四路)。"""
        names = ("MAG_A", "PHASE_A", "MAG_B", "PHASE_B")
        for n in names:
            self._require(n)
        return {n: self._read(n) for n in names}
