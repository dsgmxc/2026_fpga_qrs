"""
drivers/lcd.py —— 屏幕显示驱动(PS 侧语义层)

════════════════════════════════════════════════════════════════════════════
本模块管的事:**帧缓存怎么分配、什么时候换址、源怎么选**
本模块【不】管的事:往帧缓存里画什么(LVGL C 侧 / tools/display_probe.py)
════════════════════════════════════════════════════════════════════════════

数据通路(决策 D24):
    PS 渲染 → DDR 双帧缓存 → PL 的 lcd_fb_reader 按行读出 → lcd_timing 扫描输出

三条硬约束(写错任一条都会**静默**出问题):
  1. 🔴 **基址必须是物理地址且 64 KB 对齐** —— 传虚拟地址 = 花屏,不报错
  2. 🔴 **换址只在 VBlank 生效** —— 由 `lcd_vblank_ctrl` 硬件保证,PS 无法绕过;
     所以 `swap()` 必须**等 pending 落下去**再认为换成功(否则下一帧画到已经被扫描的缓冲)
  3. 🔴 **`LCD_CTRL` 选帧缓存的值是 2** —— 写 1 会得到纯色填充(见 cv_regs.py 文件头"错 1")

⚠️ 本模块**从未上板**。所有数值来自 RTL 与文档,实测结论待回填。
"""

import time

import cv_regs


class LcdError(RuntimeError):
    """显示通路不可用(帧缓存分配失败 / 寄存器未实现 / 换址超时)。"""


# 换址超时。一帧 17.069 ms(见 docs/display_subsystem.md §4.1),留 3 帧余量。
SWAP_TIMEOUT_MS = 60.0
SWAP_POLL_MS    = 0.2


class LcdPanel:
    """屏幕驱动。

    典型用法::

        lcd = LcdPanel(mon)
        lcd.allocate()                 # 分配双帧缓存并写进 PL
        lcd.select(cv_regs.SRC_DDR)    # 切到帧缓存
        lcd.buffers[0][:, :] = 0xF800  # 用 numpy 直接画
        lcd.refresh(0)                 # 提交 + 等生效
    """

    def __init__(self, monitor):
        self._mon = monitor
        self._owner = None          # pynq.allocate 的整块内存(保持引用,别被 GC)
        self._phys = [None, None]
        self._views = [None, None]
        self._front = None          # 当前生效的前台(由 PL 读回,不是本地猜的)

    # -----------------------------------------------------------------------
    # 分配
    # -----------------------------------------------------------------------
    def allocate(self):
        """分配两块 64 KB 对齐的帧缓存,并把物理基址写进 PL。

        ⚠️ 必须用 `pynq.allocate`(走 CMA)—— 它的缓存维护由 PYNQ 负责。
           用普通 numpy 数组:① 物理不连续,PL 按行取数会错位;② 没有物理地址可写。
        """
        try:
            from pynq import allocate
        except ImportError as e:            # 不在板上跑(比如 PC 上做语法检查)
            raise LcdError(f"需要板上 PYNQ 环境才能分配帧缓存:{e}") from e

        import numpy as np

        # 一次分配、切两块,而不是分两次 —— 少一次 CMA 申请,也少一次 flush。
        #   FB_SLOT_SIZE 本身是 64 KB 的整数倍 ⇒ 只要**第一块**对齐,第二块自动对齐。
        total = 2 * cv_regs.FB_SLOT_SIZE + cv_regs.FB_ALIGNMENT
        try:
            self._owner = allocate(shape=(total,), dtype=np.uint8)
        except Exception as e:
            raise LcdError(
                f"帧缓存分配失败({total} B ≈ {total / 1048576:.2f} MiB)。"
                f"常见原因:CMA 池太小 —— 查 `cat /proc/meminfo | grep Cma`。"
                f"原始错误:{e}"
            ) from e

        base_phys = int(self._owner.device_address)
        # 向上取到 64 KB 边界;`(-base) % A` 比 `A - base % A` 少一个"整除时加一整块"的坑
        offset = (-base_phys) % cv_regs.FB_ALIGNMENT
        aligned_phys = base_phys + offset

        for i in (0, 1):
            p = aligned_phys + i * cv_regs.FB_SLOT_SIZE
            view = self._owner[offset + i * cv_regs.FB_SLOT_SIZE:
                               offset + i * cv_regs.FB_SLOT_SIZE + cv_regs.FB_FRAME_SIZE]
            # RGB565:一个像素 2 字节,按 uint16 看才是 1024 列
            px = view.view(np.uint16).reshape(cv_regs.PANEL_HEIGHT, cv_regs.PANEL_WIDTH)
            self._phys[i] = p
            self._views[i] = px
            self._mon.lcd_set_fb_base(i, p)

        return self

    # -----------------------------------------------------------------------
    # 访问器
    # -----------------------------------------------------------------------
    @property
    def buffers(self):
        """两块帧缓存的 numpy 视图(uint16, shape = (600, 1024))。"""
        if self._views[0] is None:
            raise LcdError("还没 allocate()")
        return self._views

    @property
    def phys_addrs(self):
        return tuple(self._phys)

    def flush(self):
        """把 CPU 缓存里的像素推回 DDR。

        🔴 **PS 写完帧缓存必须 flush** —— PL 是在读**物理内存**,看不到 ARM 的
           cache。漏掉它的表现是"屏上有一部分是旧的",而且**不报错**:
           写得少的时候 cache 行不脏、刚好没暴露,改个尺寸就露馅。
           (`pynq.allocate` 的内存是 cacheable 的,由 PYNQ 负责维护。)
        """
        if self._owner is None:
            raise LcdError("还没 allocate()")
        self._owner.flush()

    # -----------------------------------------------------------------------
    # 源选择
    # -----------------------------------------------------------------------
    def select(self, src):
        """切显示源。src 用 `cv_regs.SRC_BARS` / `SRC_FILL` / `SRC_DDR`。

        ⚠️ 从小到大:0 彩条 → 1 填充 → 2 帧缓存。**2 才是帧缓存。**
        """
        self._mon.lcd_set_source(src)
        got = self._mon.lcd_get_source()
        if got != src:
            raise LcdError(f"LCD_CTRL 写 {src} 后读回 {got} —— 位流可能与代码不匹配")
        return src

    def show_bars(self):
        """回到厂商彩条。这是**不依赖 DDR 与 PS** 的回归路径,排障时先回这里。"""
        return self.select(cv_regs.SRC_BARS)

    def show_fill(self, rgb888):
        """整屏纯色(RGB888,24 位)。用来验证"PL 收到 PS 的写"这条链。"""
        self._mon.lcd_set_fill_rgb(rgb888)
        return self.select(cv_regs.SRC_FILL)

    def show_framebuffer(self):
        """切到 DDR 帧缓存。**必须在 allocate() 之后。**"""
        if self._phys[0] is None:
            raise LcdError("show_framebuffer 前必须先 allocate() —— 帧缓存基址还是 0")
        return self.select(cv_regs.SRC_DDR)

    # -----------------------------------------------------------------------
    # 刷新(提交后台缓冲)
    # -----------------------------------------------------------------------
    def refresh(self, index, timeout_ms=SWAP_TIMEOUT_MS):
        """把第 `index` 块推成前台,并**等它真的生效**。

        返回值:换址实际耗时的毫秒数(超时则抛 LcdError)。

        ⚠️ 为什么一定要等:换址由 `lcd_vblank_ctrl` 在**下一个 VBlank** 执行。
           请求写下去后如果立刻返回,调用方会以为"画完了",而 PS 下一步往往
           又去写另一块缓冲 —— 而那一块此刻**还在被扫描**。等 pending 落下,
           就等价于"扫描已经翻到新前台了"。
        """
        # ★ 先 flush 再换址:否则换址之后 PL 读到的可能还是 cache 里的旧像素。
        #   顺序反了不会报错,只会"偶尔看到上一版画面"。
        self.flush()

        front, pending = self._mon.lcd_swap_state()

        # ⚠️ 2026-10-03 更正:这里原注释写"写 0x60 会清掉粘滞标志"—— **是错的**。
        #    0x60(LCD_SWAP)从来只做换址;清除地址是 **0x64**(见 `clear_sticky()`)。
        #    仍然先存一份快照 —— 换址超时时要把它打进错误信息里(见下面的 except)。
        before = self._mon.lcd_status()

        if front == index and pending == 0:
            self._front = front
            return 0.0                      # 已经是前台,不必再请求

        self._mon.lcd_request_swap(index)

        t0 = time.monotonic()
        deadline = t0 + timeout_ms / 1000.0
        while time.monotonic() < deadline:
            front, pending = self._mon.lcd_swap_state()
            if pending == 0 and front == index:
                self._front = front
                return (time.monotonic() - t0) * 1000.0
            time.sleep(SWAP_POLL_MS / 1000.0)

        raise LcdError(
            f"换址超时({timeout_ms} ms):请求切到 {index},读回 front={front} "
            f"pending={pending}。\n"
            f"  可能原因:① `LcdPanel.refresh` 与 `lcd_vblank_ctrl` 的握手没走通;"
            f"② PL 侧帧缓存通路不在跑(检查 LCD_CTRL 是否 = SRC_DDR);"
            f"③ 位流不是含显示通路的那一版。\n"
            f"  换址前的状态:underrun={before['underrun']} axi_err={before['axi_err']}"
        )

    def flip(self, timeout_ms=SWAP_TIMEOUT_MS):
        """切到**另一块**并等生效。返回换到的那块索引。"""
        if self._front is None:
            self._front = self._mon.lcd_swap_state()[0]
        target = 1 - self._front
        self.refresh(target, timeout_ms=timeout_ms)
        return target

    # -----------------------------------------------------------------------
    def status(self):
        """诊断快照。上板排障时先打这个。"""
        s = self._mon.lcd_status()
        front, pending = self._mon.lcd_swap_state()
        return {
            "source":     self._mon.lcd_get_source(),
            "front":      front,
            "pending":    pending,
            "fb_base":    self._phys,
            "vblank_cnt": s["vblank_cnt"],
            "swap_cnt":   s["swap_cnt"],
            "underrun":   s["underrun"],      # 行缓冲欠载,必须保持 0
            "axi_err":    s["axi_err"],
        }

    def clear_sticky(self):
        """清 `underrun` / `axi_err`(写 0x64)。

        ⚠️ 先切回非 DDR 源再清(`show_bars()`),否则 PL 正在取数,下一个坏响应
           会立刻把 `axi_err` 置回 1 —— 看起来像"清不掉"。
        """
        self._mon.lcd_clear_sticky()
        return self._mon.lcd_status()

    def debug(self):
        """调试观测快照(0x74/0x78/0x7C)—— 排查"src=2 黑屏"时先打这个。

        `verdict` 直接给结论,判决表见 `cv_regs.DBG2_BIT`。
        """
        d = self._mon.lcd_dbg()
        s = self.status()
        d.update({k: s[k] for k in ("source", "front", "pending", "fb_base")})
        return d

    def assert_healthy(self):
        """欠载/总线错误任一置起就抛 —— 给自检脚本用。"""
        s = self.status()
        bad = []
        if s["underrun"]:
            bad.append("underrun(行缓冲欠载)")
        if s["axi_err"]:
            bad.append("axi_err(AXI 读响应非 OKAY)")
        if bad:
            raise LcdError("显示通路不健康:" + "、".join(bad) + f"\n  完整状态:{s}")
        return s
