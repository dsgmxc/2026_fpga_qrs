"""
drivers/ui_process.py —— 拉起 / 收掉 C 侧 LVGL 界面进程

**为什么是独立进程,而不是在 Python 里画**:
  ① 用户的要求是"每个模块有自己的基础驱动库,别什么都堆到 main 里,别混在一起"
     —— 界面(C/LVGL)与编排(Python)分成两个进程,边界最硬;
  ② 前序 STM32 那套 LVGL 界面(`ui_demo.c` + 中文字库)是 C 代码,**原样搬过来**
     比用 Python 重画一遍省得多,也保得住"与前序逐字对照"的可辩护性;
  ③ 以后要接测量数据,把前序的 UART7 协议换成 pipe/socket 即可,**不污染 UI 代码**。

────────────────────────────────────────────────────────────────────────────
📌 **C 侧命令行契约**(`src/c_ui/main.c` 必须与此一致,改一处要改两处)

    c_ui --axi-base <hex> --fb0 <hex> --fb1 <hex> [--touch] [--poll-ms <n>]

    --axi-base   AXI 从机基址(默认 0x43C00000)。C 侧 mmap 这一段来读写寄存器。
    --fb0/--fb1  两块帧缓存的**物理**基址 —— 由 Python 用 `pynq.allocate` 拿到后传进来。
                 ⚠️ C 侧拿不到物理连续内存,这是**唯一**可靠的来源。
    退出码       0 = 正常退出;非 0 = 初始化失败(见 stderr)

⚠️ C 侧用 `/dev/mem` mmap(非缓存)⇒ PL 读 DDR 直接看到它的写,**绕开了风险 D1**
   (PS cache 与 PL 读到的不一致)。前提是内核没开 `CONFIG_STRICT_DEVMEM`。
"""

import os
import subprocess
import time

import cv_regs


class UiProcessError(RuntimeError):
    """C 侧界面进程起不来 / 中途退了。"""


class UiProcess:
    """C 侧 LVGL 界面进程的句柄。

    用法::

        ui = UiProcess(binary, mon, lcd)
        ui.start()          # 起了就不管,它自己跑
        ...
        ui.stop()
    """

    def __init__(self, binary=cv_regs.C_UI_BINARY, monitor=None, panel=None,
                 poll_ms=10, extra_args=()):
        self.binary = binary
        self._mon = monitor
        self._panel = panel
        self.poll_ms = poll_ms
        self.extra_args = list(extra_args)
        self.proc = None

    # -----------------------------------------------------------------------
    def build_argv(self):
        """拼命令行。**这就是上面那份契约的唯一实现。**"""
        argv = [
            self.binary,
            "--axi-base", f"0x{cv_regs.BASE_ADDR:08X}",
            "--poll-ms", str(int(self.poll_ms)),
        ]
        if self._panel is not None:
            fb0, fb1 = self._panel.phys_addrs
            if fb0 is None or fb1 is None:
                raise UiProcessError(
                    "帧缓存还没分配 —— 必须在 LcdPanel.allocate() 之后才能拉起界面"
                )
            argv += ["--fb0", f"0x{fb0:08X}", "--fb1", f"0x{fb1:08X}"]
        argv += self.extra_args
        return argv

    # -----------------------------------------------------------------------
    def check_available(self):
        """在拉起之前先判:文件在不在、能不能执行。返回 (ok, 原因)。"""
        if not os.path.isfile(self.binary):
            return False, (f"找不到可执行文件 {self.binary}\n"
                           f"    ⇒ 去 `src/c_ui/` 里 `make`(板上原生编译),"
                           f"再把产物拷到 {os.path.dirname(self.binary)}/")
        if not os.access(self.binary, os.X_OK):
            return False, f"{self.binary} 没有可执行权限(试 `chmod +x`)"
        return True, None

    # -----------------------------------------------------------------------
    def start(self):
        """拉起进程。**stdout/stderr 直接继承** —— C 侧的日志要能立刻看到。"""
        ok, why = self.check_available()
        if not ok:
            raise UiProcessError(why)

        argv = self.build_argv()
        print(f"[ui] 启动:{' '.join(argv)}")
        print(f"[ui] ⚠️ 需要读 /dev/mem —— 本进程必须是 root(main.py 已经是)")
        try:
            self.proc = subprocess.Popen(argv)
        except OSError as e:
            raise UiProcessError(f"exec 失败:{e}") from e
        return self

    # -----------------------------------------------------------------------
    @property
    def running(self):
        return self.proc is not None and self.proc.poll() is None

    def poll(self):
        """非阻塞看一眼。返回退出码;还在跑返回 `None`。"""
        return None if self.proc is None else self.proc.poll()

    def wait(self, timeout=None):
        """等它退出,返回退出码。`timeout` 到了还没退则返回 `None`(进程还在)。"""
        if self.proc is None:
            return None
        try:
            return self.proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            return None

    def stop(self, grace_s=2.0):
        """先 SIGTERM,给 `grace_s` 让它自己收尾;不退再 SIGKILL。

        ⚠️ 必须**先 TERM** —— LVGL 退出时要跑 `lv_deinit`,顺便把屏恢复成
           一个明确的状态(而不是留着半张残图)。直接 KILL 会跳过这些。
        """
        if self.proc is None or self.proc.poll() is not None:
            return self.proc.poll() if self.proc else None

        self.proc.terminate()
        t0 = time.monotonic()
        while time.monotonic() - t0 < grace_s:
            if self.proc.poll() is not None:
                return self.proc.returncode
            time.sleep(0.05)
        print(f"[ui] {grace_s}s 内没退出,发 SIGKILL")
        self.proc.kill()
        self.proc.wait()
        return self.proc.returncode

    # -----------------------------------------------------------------------
    def supervise(self, on_exit=None):
        """前台守着,直到进程退出或收到 Ctrl-C。返回退出码。

        `on_exit(code)` 在退出后调用一次(用来把屏切回彩条之类)。
        """
        code = None
        try:
            while True:
                code = self.poll()
                if code is not None:
                    print(f"[ui] 进程退出,退出码 {code}")
                    break
                time.sleep(0.2)
        except KeyboardInterrupt:
            print("\n[ui] 收到 Ctrl-C,停界面 …")
            code = self.stop()
        finally:
            if on_exit is not None:
                # 收尾动作失败不能盖掉真正的退出码 —— 但也不能一声不吭
                try:
                    on_exit(code)
                except Exception as e:              # noqa: BLE001
                    print(f"[ui] on_exit 收尾失败(不影响退出码 {code}):{e}")
        return code
