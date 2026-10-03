"""
drivers/touch.py —— GT911 触摸驱动(PS 侧语义层)

════════════════════════════════════════════════════════════════════════════
本模块管的事:**怎么读一次事件、怎么判新旧、怎么判按下/抬起**
本模块【不】管的事:I2C 时序、GT911 寄存器、复位序列 —— 全在 PL 的
`gt911_ctrl` 里跑,PS 只看三个寄存器(`0x68` / `0x6C` / `0x70`)。
════════════════════════════════════════════════════════════════════════════

PL 已经把三件难事做掉了,PS 侧【不要】重复实现:

  ① **`bit7 == 0` 不是松手** —— GT911 在"手指按着不动"时就不产生新帧,
     `gt911_ctrl` 内部遇到 bit7=0 会**保持**上次的 `count` / `pen_down`。
     ⇒ PS 这边读到的 `pen_down` 已经是一个**保持态**,不需要自己算保持窗。
     (前序 STM32 侧那段 `LCD_TOUCH_NO_UPDATE` 处理,在 PL 里已经消化掉了。)

  ② **明确释放只在"收到新帧且点数 = 0"** 时才产生。所以 `pen_down` 由 1 → 0
     是有意义的边沿,PS 直接用它。

  ③ **`seq` 每产生一个新坐标帧 +1**,不是"读一次 +1"。
     ⇒ `seq` 不变 = 这帧已经报过了,PS 应当原样沿用上次结论,不必重算。

────────────────────────────────────────────────────────────────────────────
🔴 一条硬约束:**读序必须是 0x68 → 0x6C → 0x70,不能调换**

    `gt911_ctrl` 自由运行,来一帧就刷新上报字。而 PS 要读满 3 个寄存器才拼得出
    一次触摸事件 —— 三次读之间若跨过帧边界,就会读出"状态说 1 个点、坐标却是
    下一帧"的**撕裂**。撕裂**不报错**,只让界面偶尔跳一下。

    从机把"读 `0x68`"那一拍当**提交点**,同时把 pt0/pt1 锁进 hold
    (`cv_axi_lite_slave.v:347-349` 的 `tch_pt0_hold` / `tch_pt1_hold`)。
    ⇒ 先读坐标再读状态,拿到的是上一拍快照,与 `seq` 对不上。

    本模块只经 `monitor.touch_read_event()` 读,那个方法内部就是固定读序。

⚠️ 本模块**从未上板**。所有语义来自 RTL 与文档,实测结论待回填。
"""

import time

import cv_regs


class TouchError(RuntimeError):
    """触摸通路不可用。"""


# PL 侧的快轮询周期是 10 ms(`gt911_ctrl.v:117` INTV_FAST)。
# PS 跟着 10 ms 走最省事:再快只是把同一帧重复读一遍(seq 不变)。
POLL_MS_DEFAULT = 10.0


class TouchEvent:
    """一次触摸事件(已按 `seq` 判过新旧)。

    字段:
        seq       产生此帧时 `gt911_ctrl` 的序号(**8 位,会在 0~255 之间回绕**)
        count     触点数(`[2:0]`,0 表示这一帧是明确释放)
        pen_down  是否有手指按着(**已是 PL 侧的保持态**,见文件头 ①)
        err       PL 侧粘滞错误标志 —— **置起后只有重新配置 PL 才清**
                  (见 `gt911_ctrl.v:264` 是唯一的清零点)⇒ 它表示
                  "**开机以来曾经出过错**",不是"现在正在出错"。
        x, y      触点 0 坐标(12 位,见下"坐标范围")
        x1, y1    触点 1 坐标(count < 2 时是**上一次双指的残留值**,不是 0)
        fresh     相对上一次 `poll()`,这是不是**新的一帧**
        raw_status 裸 `0x68` 值,排障用

    ⚠️ **坐标范围**:GT911 按它**配置表里写的分辨率**上报。前序对该 1024×600
       面板没有做坐标变换(三个变换宏全 0,x/y 直通),本模块照搬。
       若实测 x/y 超出 `0 ~ 1023 / 0 ~ 599`,问题在 GT911 配置表的分辨率字段,
       **不要在 PS 侧用缩放糊过去** —— 那会把"配置错"变成"永远查不出"。
    """

    __slots__ = ("seq", "count", "pen_down", "err", "x", "y", "x1", "y1",
                 "fresh", "raw_status")

    def __init__(self, seq, count, pen_down, err, x, y, x1, y1, fresh, raw_status):
        self.seq = seq
        self.count = count
        self.pen_down = pen_down
        self.err = err
        self.x = x
        self.y = y
        self.x1 = x1
        self.y1 = y1
        self.fresh = fresh
        self.raw_status = raw_status

    # ---- 给调用方的判定便捷属性 -------------------------------------------
    @property
    def pressed(self):
        """新的一帧,且按着 —— LVGL 的按下边沿。"""
        return self.fresh and self.pen_down

    @property
    def released(self):
        """新的一帧,且没按 —— LVGL 的抬起边沿。"""
        return self.fresh and not self.pen_down

    @property
    def held(self):
        """按着,但这一帧已经报过了(`seq` 没变)。"""
        return (not self.fresh) and self.pen_down

    @property
    def points(self):
        """有效触点列表 —— `count` 是几就给几个。"""
        pts = [(self.x, self.y)]
        if self.count >= 2:
            pts.append((self.x1, self.y1))
        return pts[:self.count] if self.count else []

    def __repr__(self):
        tag = "NEW " if self.fresh else "held"
        return (f"<TouchEvent {tag} seq={self.seq:3d} count={self.count} "
                f"pen={int(self.pen_down)} err={int(self.err)} "
                f"({self.x:4d},{self.y:4d})>")


class TouchPanel:
    """GT911 触摸驱动。

    典型用法::

        tp = TouchPanel(mon)
        ev = tp.poll()                 # 读一次
        if ev.pressed:  ...
        ev = tp.wait_press(timeout_ms=5000)   # 阻塞等手指按下
    """

    def __init__(self, monitor, panel_size=(cv_regs.PANEL_WIDTH, cv_regs.PANEL_HEIGHT)):
        self._mon = monitor
        self._w, self._h = panel_size
        self._last_seq = None
        self._last_xy = (0, 0)         # 最近一次**按着**时的坐标(供 read_state 用)
        self._out_of_range = 0         # 坐标越界计数(配置表分辨率不对的判据)

    # -----------------------------------------------------------------------
    # 读
    # -----------------------------------------------------------------------
    def poll(self):
        """读一次触摸事件。**不阻塞。**

        ⚠️ 同一次调用内部走了 `touch_read_event()`(固定 0x68→0x6C→0x70)。
           不要把这里的三个寄存器拆到别处去读 —— 拆开就破坏了快照原子性。
        """
        status_raw, pt0_raw, pt1_raw = self._mon.touch_read_event()

        st = self._mon.parse_touch_status(status_raw)
        p0 = self._mon.parse_touch_point(pt0_raw)
        p1 = self._mon.parse_touch_point(pt1_raw)

        seq = st["seq"]
        # 首次读一律算"新" —— 否则上电时手指已经按在屏上的话,
        # 因为 seq 在"按着不动"期间不变,界面永远收不到那次按下。
        fresh = (self._last_seq is None) or (seq != self._last_seq)
        self._last_seq = seq

        if p0["x"] >= self._w or p0["y"] >= self._h:
            if st["count"]:
                self._out_of_range += 1

        ev = TouchEvent(
            seq=seq, count=st["count"], pen_down=bool(st["pen_down"]),
            err=bool(st["err"]), x=p0["x"], y=p0["y"], x1=p1["x"], y1=p1["y"],
            fresh=fresh, raw_status=status_raw,
        )
        # 只在**按着**时记坐标。松手那一帧的坐标是残留值(PL 不更新它),
        # 拿它当"最后按点"会让 LVGL 的抬起事件落到屏上另一个位置。
        if ev.pen_down:
            self._last_xy = (ev.x, ev.y)
        return ev

    def wait(self, timeout_ms=POLL_MS_DEFAULT * 10, poll_ms=POLL_MS_DEFAULT):
        """等**新的一帧**(`seq` 变化)。返回 `TouchEvent`,超时返回 `None`。"""
        deadline = time.monotonic() + timeout_ms / 1000.0
        while time.monotonic() < deadline:
            ev = self.poll()
            if ev.fresh:
                return ev
            time.sleep(poll_ms / 1000.0)
        return None

    def wait_press(self, timeout_ms=5000, poll_ms=POLL_MS_DEFAULT):
        """等按下边沿(从"没按"到"按着")。返回 `TouchEvent` 或 `None`。

        ⚠️ 判据是 `ev.pressed`,而它要求 `fresh` —— 所以本方法必须持续
           `poll()`,不能看一次就睡。PL 在有人按时转 10 ms 快轮询,摸得到。
        """
        deadline = time.monotonic() + timeout_ms / 1000.0
        while time.monotonic() < deadline:
            ev = self.poll()
            if ev.pressed:
                return ev
            time.sleep(poll_ms / 1000.0)
        return None

    def wait_release(self, timeout_ms=5000, poll_ms=POLL_MS_DEFAULT):
        """等抬起边沿。返回 `TouchEvent` 或 `None`。"""
        deadline = time.monotonic() + timeout_ms / 1000.0
        while time.monotonic() < deadline:
            ev = self.poll()
            if ev.released:
                return ev
            time.sleep(poll_ms / 1000.0)
        return None

    def wait_press_release(self, timeout_ms=5000, poll_ms=POLL_MS_DEFAULT):
        """按下 → 抬起 一次完整点击。返回 `(press_ev, release_ev)`,任一超时则 `None`。

        触屏验收就用这个:能拿到一对事件,就说明"有帧 / 有坐标 / 有释放"三件事都通。
        """
        t0 = time.monotonic()
        press = self.wait_press(timeout_ms=timeout_ms, poll_ms=poll_ms)
        if press is None:
            return None
        left = timeout_ms - int((time.monotonic() - t0) * 1000)
        rel = self.wait_release(timeout_ms=max(left, 1000), poll_ms=poll_ms)
        if rel is None:
            return None
        return press, rel

    # -----------------------------------------------------------------------
    # 给 C 侧 LVGL indev 用的状态读法
    # -----------------------------------------------------------------------
    def read_state(self, poll_ms=POLL_MS_DEFAULT):
        """返回 `(pressed, x, y)` —— **不判新旧**,只反映当前状态。

        语义上等价于 PL 的保持态:`pen_down` 在没新帧时不会自己变 0,
        所以轮询频率只要不比 PL 的 10 ms 慢太多,LVGL 就不会看到假抬起。

        📌 C 侧的 `pynq_touch.c` 直接读寄存器,不走这里 —— 本方法是给
           Python 侧(自检脚本 / 将来的 Python 降级路线)用的同语义入口。
        """
        ev = self.poll()
        # 松手后仍返回**最后按着时**的坐标:LVGL 的抬起事件要带原坐标,
        # 否则点击判定会落到 (0,0) 或松手帧的残留值上。
        return ev.pen_down, self._last_xy[0], self._last_xy[1]

    # -----------------------------------------------------------------------
    # 诊断
    # -----------------------------------------------------------------------
    def status(self):
        """诊断快照。上板排障先打这个。"""
        ev = self.poll()
        return {
            "seq":          ev.seq,
            "count":        ev.count,
            "pen_down":     ev.pen_down,
            "err_sticky":   ev.err,        # 粘滞:开机以来曾出错
            "x":            ev.x,
            "y":            ev.y,
            "out_of_range": self._out_of_range,
            "raw_status":   f"0x{ev.raw_status:08X}",
        }

    def assert_healthy(self):
        """只拦"坐标越界"这一条 —— 它是**配置表分辨率写错**的判据。

        ⚠️ `err_sticky` **不在这里判失败**:它是开机以来的累计标志,一次上电时序
           抖动就会置起,而之后再没错的话功能完全正常。把它当致命错误会把
           偶发噪声变成"每次都失败"。要看它请直接读 `status()["err_sticky"]`。

        ⚠️ 坐标越界只在 `count > 0` 时计数(见 `poll()`),所以"没碰屏"不会误报。
        """
        s = self.status()
        if s["out_of_range"]:
            raise TouchError(
                f"读到 {s['out_of_range']} 次越界坐标(面板 {self._w}×{self._h})。"
                f"当前 ({s['x']}, {s['y']})。\n"
                f"  最可能的原因:GT911 **配置表里的分辨率字段**与面板不符 —— "
                f"这不是 PS 侧能修的,要去查 GT911 配置(前序是烧进配置表的)。\n"
                f"  不要在 PS 侧加缩放 —— 那只会让『配置错』变成『永远查不出』。"
            )
        return s
