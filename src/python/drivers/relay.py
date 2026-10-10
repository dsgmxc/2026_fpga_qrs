"""
drivers/relay.py —— 继电器矩阵 + 量程挡位(S2 语义层)

职责边界:
  · 裸寄存器访问 —— 进程内用 `cv_monitor.CVMonitor`,独立工具用本文件的 `_DevMem`
  · **本文件负责**:① 组合 / 挡位的语义与合法性 ② 写后读回的时序
                   ③ 🔴 **写通路不许用 `struct.pack_into`**(见下节 §0 —— 这是全项目最容易
                      踩、后果又最重的一处:会让继电器停在"全断"而不是你要的那一路)
  · **不做**测量编排(扫偏压、校准、反演都是 S3+ 的事)

板上用法::

    from drivers.relay import RelayMatrix
    r = RelayMatrix(mon)          # 进程内(main.py 那条链)
    r.apply("CISS")               # 拨到 CISS 回路
    r.set_gear("B")               # 切到 B 挡 100 Ω
    print(r.read_status())        # 硬件状态快照

    r = RelayMatrix()             # 独立工具:自己 mmap /dev/mem,**不加载位流**

────────────────────────────────────────────────────────────────────────────
🔴 §0 `0x1C` 那个「写一次切不动回路」的坑 —— 根因在 **PS 侧**,不在 PL

**结论(2026-10-10 板上闭环实测,证据 `report/measurements/20261010_relay_cfg_write_anomaly.md`)**

    `struct.pack_into(fmt, mmap, off, v)` 对 Python `mmap` 会发出 **两笔**
    AXI 事务:**先写一个 `0x00000000` 整字,再写载荷**。
    而 `mm[off:off+4] = ...`、`pynq.MMIO`(numpy `uint32` 整字存)、
    以及**单个字节存**都只发 **一笔**。

对 `cv_axi_lite_slave`,`一笔 AXI 写 = 一拍 cfg_relay_set 脉冲`
(`relay_set_r` 在 `do_write` 那拍置 1,`:715`)。于是:

    笔①`0x00000000`  ⇒ 在白名单里(COSS_DISCHARGE = 全断)⇒ **被受理**,
                        `relay_reg ← 0x00` 并 arm busy 40 ms
    笔②真值          ⇒ 撞 `busy` ⇒ 拒收,`err ← 0x03`(ERR_BUSY)
    ⇒ **无论写什么合法值,继电器最后都停在 `0x00`。**

`settle = 0` 时没有互锁 ⇒ 后写覆盖前写 ⇒ 一切正常;
**这就是为什么这个坑只在 `0x1C` 上显形** —— 它是唯一带"稳定时间互锁"的命令寄存器,
其它寄存器(0x18/0x24/0x60/0x84…)虽然**同样**被多写一笔零,但后写胜出,看不出来。

**PL 侧无缺陷。** 早先"一次写 → 两拍脉冲是 PL 的 bug"的推断**已被推翻**:
两拍脉冲来自 PS 发出两笔事务,`relay_set_r` 忠实地一笔一拍。
⇒ 修法就是**改这一处调用方式**(`_DevMemRegs.write` 用切片赋值),不是改 RTL、也不用挂 ILA。

**已用实测排除的替代解释**(都试过,都不是):
  · `O_SYNC` —— 有无都一样;
  · 裸 mmap vs PYNQ MMIO 的**映射开法** —— 实测两者都是 `/dev/mem` + `O_SYNC`
    (`pynq/pl_server/embedded_device.py:523`),没有区别;
  · 窄写被模拟成"读-改-写" —— 确实存在(`mm[0x4F]=0xAA` 后 PL 锁到 `0xAA223344`),
    但它**保留**其它字节,所以**不解释** `0x1C` 归零;pack_into 是另加了那笔零。

────────────────────────────────────────────────────────────────────────────
🔴 §1 绕法(`WORKAROUND_0X1C`)—— **默认已关**,仅留作历史复现

原做法:把「等继电器稳定」从硬件搬到 PS 侧自己等 ——

    ① 写 0x24 让继电器稳定时间 = 0   # busy 永不置起 ⇒ 两笔**都被受理**,后一笔(真值)胜出
    ② 写 0x1C = 目标组合
    ③ 恢复 0x24 原值
    ④ PS 自己 time.sleep

**它现在不需要了**,因为 §0 的根因已经修在 `_DevMemRegs.write` 里。
留着开关只为复现历史数据(例如重跑旧对照)。

⚠️ 采纳绕法必须同时接受:
  1. **`relay_ctrl` 三道保护全部失效** —— 不置 `busy`/`done`,忙时写入也不再回 `err=0x03`。
  2. **`done` 永久停在 0**(settle=0 分支没有把它置回 1 的路径)⇒ `apply()` 只能 PS 侧 sleep。
  3. ② 那拍会有一次 `0x00`(全断)瞬时请求;机械继电器 ms 级吸合,不足以动,
     但那是**靠继电器时间常数兜着**。
  4. 进程若崩在 ①②之间,`0x24` 会停在 0 ⇒ `apply()` 用 try/finally,`restore_settle()` 兜底。

🐛 **顺带修掉的历史 bug**:这一节原来的第 ① 步写的是 `orig & 0xFFFF_0000`,
   **与它自己的注释相反** —— 那个式子清的是**偏压**低半字,保的才是继电器稳定时间。
   也就是说:**即便绕法开着,它也从来没有真正把 settle 清 0 过**。
   它当时之所以"看起来好使",是因为它一直配 `pynq.MMIO` 后端跑 ——
   而那条路本来就是一写一笔、根本不需要绕。已改为 `& 0x0000_FFFF`。

────────────────────────────────────────────────────────────────────────────
📌 §2 挡位为什么**不**需要绕法

`relay_ctrl.v` 块④ 的挡位通路**刻意不进 busy**(理由写在那个文件里:挡位是模拟开关,
ns 级建立,用 40 ms 那把尺子量它是量错尺度)。⇒ 就算它也被写两次:

    脉冲①`0x00`  ⇒ 非独热 ⇒ 拒收,err=0x02,gear_reg **不动**
    脉冲②真值    ⇒ 独热 ⇒ 受理,gear_reg ← 真值,err 清 0

**净结果自洽** —— 挡位没有"被合法覆盖成别的值"的路径。所以本模块对 `0x1C` 绕、
对 `0x18` 不绕,只做读回校验。

⚠️ 挡位**没有稳定延时**,PS 必须自己等 `cv_regs.GEAR_SETTLE_MS`(150 ms,
出处 = 前序交接文档 §3.9)。这是本模块 `set_gear()` 的职责。
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import cv_regs

#: 位流的 AXI 基址。`cv_sys` 对 S_AXI 是直通,所以寄存器偏移就是 `cv_regs.REG` 的值。
AXI_BASE = 0x43C00000
AXI_SPAN = 0x1000              # BD 给 cv_sys_0 分的是 4 KB


class RelayError(RuntimeError):
    """继电器/挡位操作失败(非法组合、读回不符、独立路径拿不到寄存器)。"""


# ---------------------------------------------------------------------------
# 寄存器访问:两条路,同一个 read/write 接口
# ---------------------------------------------------------------------------
class _MonitorRegs:
    """进程内路径 —— 复用已经建好的 `CVMonitor`。

    走 `mon.mmio`(DefaultIP 的公开属性),不碰 `CVMonitor._read/_write` 私有方法。
    """

    def __init__(self, mon):
        self._mon = mon
        self.source = "CVMonitor"

    def read(self, name):
        return self._mon.mmio.read(cv_regs.REG[name])

    def write(self, name, value):
        self._mon.mmio.write(cv_regs.REG[name], value & 0xFFFFFFFF)


class _MMIORegs:
    """PYNQ 官方路径 —— `pynq.MMIO`。

    ⚠️ **它和 `_DevMemRegs` 是两条不同的路**,尽管最终都落到 `/dev/mem` 的 mmap 上:
       映射属性由各自的开法决定。2026-10-10 排查 `0x1C` 缺陷时,历史实测**全部**
       来自临时脚本的裸 mmap,**从没走过这条路** ⇒ 这个类存在的意义就是当对照。
       (见 `tools/relay_rw.py` 文件头。)

    `from pynq import MMIO` **不会**加载位流 —— 它只是个 mmap 包装,可以放心 import。
    """

    def __init__(self, base=AXI_BASE, span=AXI_SPAN):
        from pynq import MMIO

        self.source = f"pynq.MMIO @0x{base:08X}"
        self._m = MMIO(base, span)

    def read(self, name):
        return self._m.read(cv_regs.REG[name])

    def write(self, name, value):
        self._m.write(cv_regs.REG[name], value & 0xFFFFFFFF)

    def close(self):
        # ⚠️ `pynq.MMIO`(2.7)**没有** `close()` —— 板上实测 `AttributeError`。
        #    它把映射交给 `self.device` / `self.array` 持有,进程退出时自然收掉。
        #    这里只做「有就关」,不让 `close()` 自己变成新的崩溃点
        #    (它本来是为异常路径兜底的,兜底代码崩掉最讽刺)。
        close = getattr(self._m, "close", None)
        if callable(close):
            close()


class _DevMemRegs:
    """裸 mmap 路径 —— 自己开 `/dev/mem`,**不碰位流**。

    🔴 为什么需要它:
      `lcd_fb_reader` 是活跃的 AXI 主机,一直在 HP 口上读 DDR 帧缓存。
      **一次上电只允许加载一次位流** —— 在它跑着的时候复位 fabric,会打断未完成的
      HP0 读事务,锁死 HP 口连带 CPU,只能断电(2026-10-03 踩过,见
      `tools/adc_probe.py:11` 与 `qrs-ui.service` 文件头 ①)。
      而 `board.open_monitor()` 会 `Overlay(download=True)` ⇒ **独立工具不能再调它**。

    ⚠️ 本类与 `drivers/board.py::_slcr` 是**同一套办法的两处实现**(那处只为一个
       SLCR 寄存器,按次开关映射)。若将来第三个模块也要 /dev/mem,就该把它提出来
       做成 `drivers/devmem.py`,而不是抄第三遍。
    """

    def __init__(self, base=AXI_BASE, span=AXI_SPAN):
        import mmap

        self.source = f"/dev/mem @0x{base:08X}"
        try:
            self._fd = os.open("/dev/mem", os.O_RDWR | os.O_SYNC)
        except PermissionError as e:
            raise RelayError(
                "打不开 /dev/mem —— 需要 root。\n"
                "  板上用 `sudo -E python3 ...`(bare `sudo` 会清掉环境变量)。"
            ) from e
        try:
            self._map = mmap.mmap(self._fd, span, flags=mmap.MAP_SHARED,
                                  prot=mmap.PROT_READ | mmap.PROT_WRITE,
                                  offset=base)
        except Exception as e:
            os.close(self._fd)
            raise RelayError(f"mmap 0x{base:08X} 失败:{e}") from e

    def read(self, name):
        import struct
        off = cv_regs.REG[name]
        # 读用 `unpack_from` 没问题 —— 它不写。**只有写才有下面那个坑。**
        return struct.unpack_from("<I", self._map, off)[0]

    def write(self, name, value):
        """🔴 **必须用切片赋值,不许用 `struct.pack_into`。**

        2026-10-10 板上实测(证据 `report/measurements/20261010_relay_cfg_write_anomaly.md` §10):

            `struct.pack_into("<I", mm, off, v)` 对 Python `mmap` 会发出
            **两笔** AXI 事务 —— **先写一个 `0x00000000` 整字,再写载荷**;
            而 `mm[off:off+4] = ...`(以及 numpy 的 `uint32` 整字存)只发 **一笔**。

        对 `cv_axi_lite_slave` 而言,`一笔 AXI 写 = 一拍 cfg_*_set 脉冲`。于是
        `0x1C` 上那两笔分别被 `relay_ctrl` 当成两个请求:
        第一笔 `0x00`(= 白名单里的 `COSS_DISCHARGE`)被受理并锁 40 ms `busy`,
        第二笔(你真正想要的值)撞 `ERR_BUSY` 被拒 —— **硬件最终停在 `0x00`**。

        ⇒ 病根在**这一行**(写通路的调用方式),不在 PL。切片赋值是实测过的一笔写法。
        """
        off = cv_regs.REG[name]
        self._map[off:off + 4] = (value & 0xFFFFFFFF).to_bytes(4, "little")

    def close(self):
        try:
            self._map.close()
        finally:
            os.close(self._fd)


# ---------------------------------------------------------------------------
# 主类
# ---------------------------------------------------------------------------
class RelayMatrix:
    """继电器矩阵(K1–K7)+ 量程挡位(A–D)的语义层。

    :param mon: `cv_monitor.CVMonitor` —— 进程内路径(`main.py` 那条链)。
    :param workaround: 是否启用 §1 的 `0x1C` 绕法。**默认关**(见文件头 §0:它补偿的
                       那个缺陷已被证明在 PS 侧,不在 PL;`_DevMemRegs.write` 修好后
                       就不再需要它)。留着开关只为复现历史。
    :param backend: 只在 `mon` 为 `None` 时有意义:`"mmio"` = PYNQ `MMIO`(默认),
                    `"devmem"` = 裸 mmap,`None` = 先试 mmio 再退 devmem。
                    ⚠️ 两者的差别**不在映射开法**(实测都是 `/dev/mem` + `O_SYNC`),
                    而在**写下去的位宽/调用方式** —— 见文件头 §0。
    :param regs: 直接塞一个后端对象(带 `read`/`write`/`close`/`source`)。优先级最高。

    ⚠️ **构造它不会加载位流。** 独立工具用完记得 `close()`(或用 `with`)。
    """

    #: `0x1C` 绕法总开关。见文件头 §0/§1。
    #: 🔴 2026-10-10 由 `True` 改 `False`:根因查明后绕法**不再需要**,
    #:    而它的代价是真的(关掉 `relay_ctrl` 三道保护、`done` 永久停在 0)。
    WORKAROUND_0X1C = False

    #: 名字 → 位图。**从 `cv_regs.RELAY_STATE` 推导**,只有「一个位图多个名字」那种
    #: 情况才在这里额外补 —— 不把白名单手抄第二遍(抄两遍就一定会漂移)。
    #: `0x10` 挂两个名字:前序文档 §3.8 里 `ALL_OFF` 与 `CRSS_DISCHARGE` 位图相同,
    #: **不是笔误** —— 位图唯一、语义有二。`name()` 反查时取 `RELAY_STATE` 里的那个。
    ALIASES = {
        **{name: val for val, name in cv_regs.RELAY_STATE.items()},
        "CRSS_DISCHARGE": 0x10,
    }

    def __init__(self, mon=None, workaround=None, backend=None, regs=None):
        if regs is not None:
            self._regs = regs
        elif mon is not None:
            self._regs = _MonitorRegs(mon)
        elif backend == "devmem":
            self._regs = _DevMemRegs()
        elif backend == "mmio":
            self._regs = _MMIORegs()
        else:
            # auto —— 独立工具用:优先 PYNQ 的 MMIO,没有 pynq 才退到裸 mmap。
            # ⚠️ 这里是**宽 except**:开发 PC 上没有 pynq,板上有。
            #    真要区分后端就别用 auto,显式传 backend=。
            try:
                self._regs = _MMIORegs()
            except Exception:
                self._regs = _DevMemRegs()
        self._owns_regs = (mon is None and regs is None)
        self.workaround = (self.WORKAROUND_0X1C if workaround is None
                           else bool(workaround))
        self._check_implemented()

    # -- 生命周期 -----------------------------------------------------------
    def close(self):
        """释放自己开的映射(用 `mon` / `regs` 建的实例无需调用)。"""
        if self._owns_regs:
            self._regs.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _check_implemented(self):
        need = ("STATUS", "RANGE_SEL", "RELAY_CFG", "SETTLE_CFG")
        missing = [n for n in need if n not in cv_regs.PL_IMPLEMENTED_REGS]
        if missing:
            raise RelayError(
                f"该位流没有实现 {missing} —— 继电器/挡位进不了位流。\n"
                f"  当前已实现:{sorted(cv_regs.PL_IMPLEMENTED_REGS)}\n"
                f"  裸 mmio 写没实现的寄存器会落进「黑洞」:读回 0、写被忽略、不报错。"
            )

    # -- 值 ↔ 名字 ----------------------------------------------------------
    @staticmethod
    def resolve(state):
        """把 `"CISS"` / `0x7F` 统一成一个位图整数。非法值抛 `RelayError`。"""
        if isinstance(state, str):
            key = state.strip().upper()
            if key not in RelayMatrix.ALIASES:
                raise RelayError(
                    f"未知组合名 {state!r}。可用:"
                    f"{sorted(RelayMatrix.ALIASES)}"
                )
            state = RelayMatrix.ALIASES[key]
        if state not in cv_regs.RELAY_LEGAL:
            raise RelayError(
                f"0x1C = 0x{state:02X} 不在白名单里,硬件会拒收(并置 err_code=0x01)。\n"
                f"  合法组合:{[f'0x{v:02X} {cv_regs.RELAY_STATE[v]}' for v in cv_regs.RELAY_LEGAL]}\n"
                f"  ⚠️ 白名单不是黑名单:bit7 恒非法(物理上只有 7 只继电器),"
                f"不在表内的组合一律拒收。"
            )
        return state & 0xFF

    @staticmethod
    def name(state):
        return cv_regs.RELAY_STATE.get(state & 0xFF, f"0x{state & 0xFF:02X}")

    # -- 裸访问(带护栏)----------------------------------------------------
    def relay_read(self):
        """读 0x1C 的**已生效**组合位图。"""
        return self._regs.read("RELAY_CFG") & 0xFF

    def relay_write_raw(self, state):
        """**原生路径**:把 `0x1C` 直接写下去,**不走绕法、不做任何等待或校验**。

        ⚠️ 这是给**缺陷复现工具**留的窄口(`tools/relay_rw.py` 的 §0 / C / E 节)——
           它需要在「`0x24` 保持原值」的条件下观察一次**裸写**的行为,
           也就是异常文档 §9.4 里 `--cold` 那一支。
           **正常业务代码请用 `apply()`** —— 只有它带读回校验与绕法。

        ⚠️ 值**不截到 8 位** —— 与 RTL 一致(`cv_axi_lite_slave.v` 有意整字存,
           好让 `0x180` 这类值走到 `relay_ctrl` 被判非法;见该文件的长注释)。
        """
        self._regs.write("RELAY_CFG", state)

    def gear_write_raw(self, code):
        """**原生路径**:把 `0x18` 直接写下去,**不做独热预校验、不等、不读回**。

        为什么需要它:`set_gear()` 在 **Python 侧**就把非独热值挡掉了(抛 `RelayError`,
        **根本不碰硬件**)⇒ 光靠 `set_gear()` 永远测不到 `relay_ctrl.v` 块④ 里
        那条 `ERR_GEAR` 分支。要验硬件那一条,必须走这个口。

        ⚠️ 安全:非独热/全 0 都会被 PL **拒收**且 `gear_reg` 一位不动
           (`relay_ctrl.v:327-333`),所以引脚不会动。**但这条保证依赖 RTL**,
           别把它当成通用许可 —— 正常业务代码请用 `set_gear()`。

        ⚠️ 值**不截到 4 位** —— 与 RTL 一致(整字存,好让越界值走到 `legal_gear` 被判非法)。
        """
        self._regs.write("RANGE_SEL", code)

    def gear_read(self):
        """读 0x18 的**已生效**挡位独热码。"""
        return self._regs.read("RANGE_SEL") & 0xF

    def settle_get(self):
        """读 0x24 整字。"""
        return self._regs.read("SETTLE_CFG")

    def settle_set_raw(self, value):
        """整字写 0x24。"""
        self._regs.write("SETTLE_CFG", value)

    def restore_settle(self):
        """把 `0x24` 恢复成复位值 `40 ms / 1500 ms`(异常路径兜底用)。"""
        self.settle_set_raw(cv_regs.SETTLE_RESET)
        return cv_regs.SETTLE_RESET

    def read_status(self):
        """读 0x04 STATUS,返回 dict。

        ⚠️ `fault` **不是**独立故障 —— `cv_sys.v` 里
           `sts_status = {..., (io_err_code != 0), io_done, io_busy}`,
           它是 `err_code != 0` 的**派生冗余位**。判错只用 `err`。
        """
        raw = self._regs.read("STATUS")
        err = (raw >> 8) & 0xFF
        return {
            "raw": raw,
            "busy": raw & 0x1,
            "done": (raw >> 1) & 0x1,
            "fault": (raw >> 2) & 0x1,          # = (err != 0),见 docstring
            "err": err,
            "err_name": cv_regs.RELAY_ERR.get(err, f"UNKNOWN(0x{err:02X})"),
            "meas_cnt": (raw >> 16) & 0xFFFF,   # ⚠️ 未实现(meas_fsm 不存在),恒 0
        }

    def wait_idle(self, timeout_ms=200):
        """轮询 `STATUS.busy` 落下 —— 等**硬件**把这一轮稳定时间走完。

        ⚠️ 判据用 `busy` **不是** `done`:`relay_ctrl.v` 块③ 在 `settle == 0`
           的分支里 `done_r <= 1'b0` 之后,没有任何路径把它置回 1
           ⇒ settle=0 时 `done` 永久停在 0,**等它会等到超时**。
           (`busy` 则在 settle=0 时压根不置起,轮询会立刻返回 —— 这才是对的。)

        ⚠️ 绕法开着时这个函数没意义:绕法把 settle 清 0,`busy` 永不置起。

        :return: 最后读到的 `read_status()` 字典(超时也返回,由调用方判 `busy`/`err`)
        """
        deadline = time.time() + timeout_ms / 1000.0
        while True:
            st = self.read_status()
            if not st["busy"]:
                return st
            if time.time() >= deadline:
                return st
            time.sleep(0.001)

    # -- 写:绕法 -----------------------------------------------------------
    def _write_relay(self, state):
        """按 `self.workaround` 选路径写 0x1C。"""
        if not self.workaround:
            self._regs.write("RELAY_CFG", state)
            return

        # ★ 绕法:见文件头 §1。**默认已关**(WORKAROUND_0X1C = False)。
        #   try/finally 保证「崩在中间也不会把 settle 留在 0」——
        #   否则后续的写全部落在无保护状态,而且**看不出来**。
        #
        # ⚠️ 2026-10-10:这里原来写 `orig & 0xFFFF_0000`,**与注释说的相反** ——
        #    那个式子保住的是**继电器**稳定时间高半字、清掉的是偏压低半字,
        #    而绕法要的恰恰是**把继电器稳定时间清 0**(好让 busy 永不置起)。
        #    也就是说:这行代码**从来没有真正生效过**。它之所以「看起来好使」,
        #    是因为当时它是配 `pynq.MMIO` 后端跑的 —— 而那条路本来就是一写一笔、
        #    根本不需要绕。现已改正为 `& 0x0000_FFFF`。
        orig = self.settle_get()
        try:
            self.settle_set_raw(orig & 0x0000_FFFF)   # ① 继电器稳定时间 ← 0
            self._regs.write("RELAY_CFG", state)      # ② 两拍都被受理,真值胜出
        finally:
            self.settle_set_raw(orig)                 # ③ 还回去

    def apply(self, state, wait=True, settle_ms=None):
        """把继电器拨到 `state`,读回确认。返回读回的位图。

        :param state: `"CISS"` 这样的名字,或 `0x7F` 这样的位图(必须在白名单里)
        :param wait: `True` 则等继电器稳定再返回
        :param settle_ms: 覆盖等待时长(默认 `cv_regs.RELAY_SETTLE_MS` = 40 ms)

        ⚠️ 等待方式**取决于绕法开关**:
             · 绕法**关**(默认)⇒ 等**硬件**的 `busy` 落下(`wait_idle()`),
               超时上限 = 稳定时间 ×4。这才是 PL 里那套 busy/done 的本意。
             · 绕法**开** ⇒ `busy` 永不置起,只能 PS 侧 `time.sleep`(见 §1 代价 1/2)。
               那种模式下**不要**在别处等 `STATUS[1] done`,它永远不置起。
        """
        code = self.resolve(state)
        self._write_relay(code)

        if wait:
            if self.workaround:
                ms = cv_regs.RELAY_SETTLE_MS if settle_ms is None else int(settle_ms)
                if ms > 0:
                    time.sleep(ms / 1000.0)
            else:
                ms = cv_regs.RELAY_SETTLE_MS if settle_ms is None else int(settle_ms)
                self.wait_idle(max(int(ms) * 4, 50))

        got = self.relay_read()
        if got != code:
            st = self.read_status()
            raise RelayError(
                f"0x1C 写 0x{code:02X}({self.name(code)}) 后读回 0x{got:02X}"
                f"({self.name(got)})—— 不一致。\n"
                f"  STATUS = 0x{st['raw']:08X} err={st['err_name']} "
                f"busy={st['busy']} done={st['done']}\n"
                f"  ⇒ 若 err=RELAY,是组合被拒(白名单外);\n"
                f"     若绕法开着还这样,说明 `0x1C` 的行为又变了 —— 回去看 "
                f"report/measurements/20261010_relay_cfg_write_anomaly.md。"
            )

        st = self.read_status()
        if st["err"] != 0:
            # 读回对了但 err 非 0:绕法下不该出现(两个脉冲都该被受理并清 err)。
            raise RelayError(
                f"0x1C 写 0x{code:02X} 读回正确,但 err_code=0x{st['err']:02X}"
                f"({st['err_name']})非零。\n"
                f"  ⇒ 粘滞错误位本该被这一次受理的写清掉。硬件行为与预期不符。"
            )
        return got

    def reset(self, **kw):
        """回到复位组合 `ALL_OFF`(前序称「安全状态」)。"""
        return self.apply(cv_regs.RELAY_RESET, **kw)

    def all_disconnect(self, **kw):
        """拨到 `COSS_DISCHARGE` = **全断**。

        ⚠️ 与 `reset()` 的 `ALL_OFF` **不是同一个物理组合**:`ALL_OFF` = K5 闭合,
        全断 = 七只全低。前序文档 §3.8 专门警告过这个命名陷阱。
        """
        return self.apply(0x00, **kw)

    # -- 写:挡位 -----------------------------------------------------------
    @staticmethod
    def _gear_code(gear):
        if isinstance(gear, str):
            key = gear.strip().upper()
            if key not in cv_regs.GEAR_BIT:
                raise RelayError(f"未知挡位 {gear!r};可用 {sorted(cv_regs.GEAR_BIT)}")
            return 1 << cv_regs.GEAR_BIT[key]
        return int(gear) & 0xF

    def set_gear(self, gear, wait=True):
        """切挡位。`gear` 取 `"A"/"B"/"C"/"D"` 或独热码 `1/2/4/8`。

        ⚠️ **稳定延时在 PS 侧**,PL 不提供 —— 见文件头 §2。默认等
           `cv_regs.GEAR_SETTLE_MS`(150 ms,前序交接文档 §3.9)。
        """
        code = self._gear_code(gear)
        if code not in cv_regs.GEAR_LEGAL:
            raise RelayError(
                f"0x18 = 0x{code:X} 不是独热码,硬件会拒收(置 err_code=0x02)。\n"
                f"  合法:{[f'0b{c:04b}' for c in cv_regs.GEAR_LEGAL]}"
                f"(A/B/C/D = {cv_regs.GEAR_OHM})\n"
                f"  ⚠️ 两位置 1 ⇒ 两个 R0 并联 ⇒ 阻值偏小,而 PS 仍按标称值反演\n"
                f"     ⇒ 所有电容系统性偏大,**不报错**。所以必须恰好一位。"
            )
        self._regs.write("RANGE_SEL", code)

        if wait:
            time.sleep(cv_regs.GEAR_SETTLE_MS / 1000.0)

        got = self.gear_read()
        if got != code:
            st = self.read_status()
            raise RelayError(
                f"0x18 写 0x{code:X} 后读回 0x{got:X} —— 不一致"
                f"(STATUS err={st['err_name']})。"
            )
        return got

    @staticmethod
    def gear_name(code):
        for k, b in cv_regs.GEAR_BIT.items():
            if (1 << b) == (code & 0xF):
                return f"{k}({cv_regs.GEAR_OHM[k]} Ω)"
        return f"0x{code & 0xF:X}"

    # -- 汇总 ---------------------------------------------------------------
    def snapshot(self):
        """一次读全:给日志 / 界面用。**只读,无副作用。**"""
        st = self.read_status()
        relay = self.relay_read()
        gear = self.gear_read()
        return {
            "relay": relay,
            "relay_name": self.name(relay),
            "gear": gear,
            "gear_name": self.gear_name(gear),
            "err": st["err"],
            "err_name": st["err_name"],
            "busy": st["busy"],
            "done": st["done"],
            "source": self._regs.source,
            "workaround": self.workaround,
        }
