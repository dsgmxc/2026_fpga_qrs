#!/usr/bin/env python3
"""继电器 / 挡位 —— 功能测试 + `0x1C` 缺陷复现。

🔴 **本脚本不加载位流。** 理由(`lcd_fb_reader` 是活跃 AXI 主机):
   `Overlay(download=True)` 会复位 fabric,打断未完成的 HP0 读事务,
   锁死 HP 口连带 CPU,**只能断电**。一次上电只允许加载一次位流
   (2026-10-03 踩过,见 `tools/adc_probe.py:11` 与 `qrs-ui.service` 文件头 ①)。
   ⇒ 本脚本假设 `main.py` **已经**把位流放下去了,自己只读写 `/dev/mem`。

用法(在板上,必须 `sudo -E` —— bare `sudo` 会清掉环境变量)::

    sudo -E python3 relay_rw.py                  # 功能测试(A + C + D),默认走 PYNQ MMIO
    sudo -E python3 relay_rw.py --raw            # 换裸 mmap 后端 —— 见下
    sudo -E python3 relay_rw.py --diagnose        # 追加:B 缺陷复现 + E 稳定计时

⚠️ 会真的吸合/释放继电器。结束时写回 `ALL_OFF(0x10)` + B 挡。

────────────────────────────────────────────────────────────────────────────
✅ `0x1C` 缺陷**已结案**(2026-10-10 当晚)—— 两个后端现在都正常

结论(完整证据 `report/measurements/20261010_relay_cfg_write_anomaly.md` §10):

    PS 侧用 **`struct.pack_into`** 写寄存器时会发出 **两笔** AXI 事务
    (先写一个 `0x00000000`,再写载荷);而 `pynq.MMIO`(numpy 整字存)
    与 `mm[off:off+4] = ...`(切片赋值)**都只发一笔**。

    ⇒ `relay_ctrl` 一笔事务一拍脉冲 ⇒ 第一笔 `0x00`(白名单里的
      `COSS_DISCHARGE`)被受理并锁 40 ms busy,第二笔(真值)撞 `ERR_BUSY`。
      **PL 无缺陷**,病根在调用方式。

修法已落在 `drivers/relay.py::_DevMemRegs.write`(改成切片赋值)。
⇒ `--raw` 与默认后端现在**结果应当一致**;若又不一致,那是新问题,回去看 §10。

⚠️ 历史:这个脚本早先的两个后端被当作"有意的对照"。那次对照确实有用 ——
   它把结论从"PL 多发脉冲"推到了"PS 多写一次",**方向是对的**;
   只是当时以为是**映射开法**不同,实际是**写下去的调用方式**不同
   (实测两者都是 `/dev/mem` + `O_SYNC`,见 §10.2)。
"""

import argparse
import os
import sys
import time

sys.path.insert(0, "/home/xilinx/cv")
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "python"))

import cv_regs as R                                     # noqa: E402
from drivers.relay import RelayError, RelayMatrix       # noqa: E402

FAILS = []
DIAGNOSE = False
FCLK0_REG = 0xF8000170          # SLCR FPGA0_CLK_CTRL


# ---------------------------------------------------------------------------
def hdr(t):
    print("\n" + "=" * 72)
    print(t)
    print("=" * 72)


def check(label, got, want, soft=False):
    ok = got == want
    tag = "OK " if ok else ("~~~" if soft else "✗  ")
    print(f"  {tag} {label:46s} got={got!r} want={want!r}")
    if not ok and not soft:
        FAILS.append(label)
    return ok


def fclk0_real_mhz():
    """直读 `0xF8000170` 反算真值 —— 不依赖 PYNQ 的 `Clocks`(它假定 IO PLL=1500)。"""
    import mmap
    import struct

    with open("/dev/mem", "rb") as f:
        m = mmap.mmap(f.fileno(), 0x1000, mmap.MAP_SHARED, mmap.PROT_READ,
                      offset=0xF8000000)
        v = struct.unpack("<I", m[0x170:0x174])[0]
        m.close()
    div0 = (v >> 8) & 0x3F
    div1 = (v >> 20) & 0x3F
    mhz = R.IO_PLL_MHZ / div0 / div1 if div0 and div1 else 0.0
    return v, div0, div1, mhz


# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# 🔴 第 0 节 —— 必须**第一个**跑,在任何其它写之前
# ---------------------------------------------------------------------------
def section_trigger(r):
    """★ 唯一可能定位根因的一节:**把"单写"与"背靠背两写"分开**。

    线索来自两处的**互相矛盾**(2026-10-10 晚发现):

    | 谁 | `0x1C` 之前有没有写过 `0x24`? | 单写 `0x1C` 的结果 |
    |---|---|---|
    | 旧 `tools/relay_rw.py` A 节(日志记「7/7 相符」) | **没有** —— `0x24` 一直是复位值 | ✓ 读回 = 写入值 |
    | `poke_relay.py w 0x24 <S> 0x1C <V>`(异常记录全部出自它) | **有,而且是紧挨着** | ✗ 读回 = 0x00,err=0x03 |

    ⇒ **同一个位流,同一次上电,两种写法结果相反** ⇒ 异常**不是**位流+源的静态性质。
    ⇒ 第一嫌疑:**紧挨着的两次 AXI 写**。

    ⚠️ 所以本节**不能**先做 SCRATCH 回环 —— 那本身就是三次写,会把状态搞乱。
       本节必须在**任何**其它写之前跑,`--cold` 与 `--pair` 两次之间还要有足够间隔。
    """
    hdr("★ 0. 触发条件对照 —— 单写 vs 背靠背两写(**在所有其它写之前**)")

    settle0 = r.settle_get()
    print(f"  `0x24` 上电/当前值 = 0x{settle0:08X}"
          f"  (继电器 {settle0 >> 16} ms / 偏压 {settle0 & 0xFFFF} ms)")
    if settle0 >> 16 != R.RELAY_SETTLE_MS:
        print(f"    ⚠️ 不是复位值 {R.RELAY_SETTLE_MS} ms —— 说明本次上电已经有人写过 `0x24`,"
              f"下面的 cold 单写就不再是干净的「冷」条件。")

    # ---- ① 单写:不动 0x24,直接写 0x1C ------------------------------------
    v = 0x7F
    print(f"\n  ① --cold:不碰 `0x24`,直接写 `0x1C` = 0x{v:02X}")
    orig_wa = r.workaround                    # ★ 存起来,本节结束时**原样还回去**
    r.workaround = False                      # 原生路径,不用绕法
    r.relay_write_raw(v)
    time.sleep(0.150)
    rb = r.relay_read()
    st = r.read_status()
    cold_ok = (rb == v)
    print(f"     读回 0x{rb:02X} ({r.name(rb)})  err=0x{st['err']:02X}({st['err_name']})"
          f" busy={st['busy']}  ⇒ {'✓ 正常' if cold_ok else '✗ 异常(读回 != 写入)'}")

    # 让它彻底静下来,再测第二次
    time.sleep(0.200)

    # ---- ② 背靠背两写:先 0x24 再 0x1C -------------------------------------
    v2 = 0x61
    print(f"\n  ② --pair:紧挨着先写 `0x24`(settle=40 ms)再写 `0x1C` = 0x{v2:02X}")
    r.settle_set_raw((R.RELAY_SETTLE_MS << 16) | (settle0 & 0xFFFF))
    r.relay_write_raw(v2)                     # ← 中间不做任何事,模拟 poke_relay.py
    time.sleep(0.150)
    rb2 = r.relay_read()
    st2 = r.read_status()
    pair_ok = (rb2 == v2)
    print(f"     读回 0x{rb2:02X} ({r.name(rb2)})  err=0x{st2['err']:02X}({st2['err_name']})"
          f" busy={st2['busy']}  ⇒ {'✓ 正常' if pair_ok else '✗ 异常(读回 != 写入)'}")

    print("\n  ── 判读 ──")
    print("  ℹ️ 本节的期望在 2026-10-10 结案后**反转过一次**:")
    print("     结案前:选哪个后端决定结果(`struct.pack_into` 会发两笔事务)。")
    print("     结案后:`_DevMemRegs` 改成一笔写 ⇒ **两个后端、两种写法都应正常**。")
    if cold_ok and pair_ok:
        print("  ⇒ ✓ **两种写法都正常** —— 与结案后的期望一致。")
        print("     若 `--raw` 也这样,说明修法覆盖了两条后端;若哪个后端异常,")
        print("     先看 `drivers/relay.py::_DevMemRegs.write` 是不是又被改回 "
              "`struct.pack_into`。")
    elif not cold_ok and not pair_ok:
        print("  ⇒ ✗ **两种写法都异常** ⇒ 与写法无关。先查后端写下去的**笔数**")
        print("     (anomaly 文档 §10.4 那张表),再谈挂 ILA。")
    elif cold_ok and not pair_ok:
        print("  ⇒ ⚠️ 冷单写正常、背靠背两写异常 ⇒ 与结案结论**不符**,")
        print("     回去重跑 anomaly 文档 §10.4 的四路对照。")
    else:
        print("  ⇒ ⚠️ 冷单写异常、背靠背反而正常?**先怀疑测量本身**,别急着下结论。")
    print("  ⚠️ 本节只测了两次,样本极小。要么判读,要么把两次都重复 3 遍再看。")
    print("  📄 完整证据与机制:`report/measurements/20261010_relay_cfg_write_anomaly.md` §10。")

    r.restore_settle()
    r.workaround = orig_wa                    # ★ 还回调用时的值(别硬写 True)
    return cold_ok, pair_ok


def section0(r):
    """只读前言 —— 确认"我在跟谁说话"。**跑在 `section_trigger` 之后。**"""
    hdr("0. 只读前言")

    ver = r._regs.read("VERSION")
    print(f"  VERSION(0x48) = 0x{ver:08X}   (位流期望 0x00010000)")
    check("VERSION", ver, 0x00010000, soft=True)

    ok = True
    for pat in (0xDEADBEEF, 0x00000000, 0x5A5A1234):
        r._regs.write("SCRATCH", pat)
        rb = r._regs.read("SCRATCH") & 0xFFFFFFFF
        ok &= (rb == pat)
    check("SCRATCH 回环(3 个图案)", ok, True)
    print("  ⚠️ 本节做了 3 次写。**如果 C 节(非法值)结果怪,先看这里有没有扰动** ——")
    print("     旧版脚本 A 节跑在 SCRATCH 回环**之后**,它的「7/7 相符」是在有写历史的情况下拿到的。")

    snap = r.snapshot()
    print(f"  后端      : {snap['source']}")
    print(f"  RELAY_CFG : 0x{snap['relay']:02X}  {snap['relay_name']}")
    print(f"  RANGE_SEL : 0x{snap['gear']:X}   {snap['gear_name']}")
    print(f"  SETTLE_CFG: 0x{r.settle_get():08X}"
          f"   (继电器 {r.settle_get() >> 16} ms / 偏压 {r.settle_get() & 0xFFFF} ms)")
    print(f"  STATUS    : err={snap['err_name']} busy={snap['busy']} done={snap['done']}")

    reg, d0, d1, mhz = fclk0_real_mhz()
    print(f"  FCLK0     : 0x{FCLK0_REG:08X} = 0x{reg:08X}  DIV0={d0} DIV1={d1}"
          f"  → {mhz:.3f} MHz")
    if abs(mhz - R.FCLK0_DESIGN_MHZ) > 1.0:
        print(f"    ⚠️ 不是设计的 {R.FCLK0_DESIGN_MHZ} MHz。")
        print(f"       ⇒ 硬件的 {R.RELAY_SETTLE_MS} ms 稳定期实际是"
              f" {R.RELAY_SETTLE_MS * R.FCLK0_DESIGN_MHZ / mhz:.1f} ms(方向安全)。")
        print(f"       ⇒ 本脚本的绕法走 **PS 侧 sleep**,不受影响。")
    return mhz


def section_a(r):
    """A. 7 个合法组合 + 4 个挡位 的写→读回。**P4 的闸。**"""
    hdr("A. 功能往返 —— 这是 P4 的闸")
    print(f"  绕法 = {r.workaround};等待 = "
          f"{'PS 侧 sleep(绕法下 busy 不置起)' if r.workaround else '硬件 busy 落下(wait_idle)'}")
    print(f"  {'写入':>6}  {'名称':16s} {'回读':>6}  err")
    for v in R.RELAY_LEGAL:
        try:
            got = r.apply(v)
            st = r.read_status()
            print(f"  0x{v:02X}  {r.name(v):16s} 0x{got:02X}  err={st['err_name']}")
            check(f"A 0x{v:02X} {r.name(v)} 回读", got, v)
        except RelayError as e:
            print(f"  0x{v:02X}  {r.name(v):16s} **异常**: {str(e).splitlines()[0]}")
            FAILS.append(f"A 0x{v:02X} {r.name(v)}")

    print()
    for g in ("A", "B", "C", "D"):
        try:
            got = r.set_gear(g)
            st = r.read_status()
            print(f"  挡位 {g} → 0x{got:X} ({r.gear_name(got)})  err={st['err_name']}")
            check(f"A 挡位 {g}", got, r._gear_code(g))
        except RelayError as e:
            print(f"  挡位 {g} **异常**: {str(e).splitlines()[0]}")
            FAILS.append(f"A 挡位 {g}")


def section_b(r):
    """B. **默认稳定时间下的回归护栏** —— 写什么就该读回什么。

    🔴 2026-10-10 修:本节原来是"缺陷复现",**期望就是失败**(读回 0x00 / err=0x03)。
       缺陷当晚结案后(根因 = PS 侧 `struct.pack_into` 发两笔事务),期望**反转**:
       现在它必须**通过**。留着不复现,是为了挡住"哪天又被人写回去"。

    ⚠️ 若本节变红 ⇒ 先看 `drivers/relay.py::_DevMemRegs.write` 是不是又被改回
       `struct.pack_into` 了(那是本缺陷唯一的已知成因),再谈别的。
    """
    hdr("B. 默认稳定时间下的写→读回(回归护栏,期望全通过)")
    saved = r.workaround
    r.workaround = False                     # ← 走 PL 的原生路径(硬件互锁生效)
    try:
        r.settle_set_raw((R.RELAY_SETTLE_MS << 16) | (r.settle_get() & 0xFFFF))
        for v in (0x3C, 0x61, 0x51):
            r.relay_write_raw(v)
            time.sleep(0.100)                # 40 ms 稳定期 + 余量
            rb = r.relay_read()
            st = r.read_status()
            ok = (rb == v) and (st["err"] == 0x00)
            print(f"  写 0x{v:02X} → 读 0x{rb:02X}  err={st['err_name']}"
                  f"(0x{st['err']:02X})  {'✓ 写什么读回什么' if ok else '✗ 缺陷回来了'}")
            if not ok:
                FAILS.append(f"B 0x{v:02X} 写→读回不符(读到 0x{rb:02X} err=0x{st['err']:02X})")
    finally:
        r.workaround = saved
        r.restore_settle()
    print("  ⇒ 本节原为「缺陷复现」,2026-10-10 结案后期望反转 —— 见文件头。")


def section_c(r):
    """C. 非法组合必须被拒 + **状态不变**。

    ⚠️ 本节**必须**在绕法关闭时跑:绕法下第一次脉冲总携带 0x00 并被受理,
       非法值那一次只是把 err 置起来 —— 状态**会**动(变成 0x00)。
       见 anomaly 文档 §7.2 的 ③。
    """
    hdr("C. 非法组合 —— 期望被拒且状态不变(绕法关闭,走原生路径)")
    saved = r.workaround
    r.workaround = False
    try:
        r.settle_set_raw((R.RELAY_SETTLE_MS << 16) | (r.settle_get() & 0xFFFF))
        r.relay_write_raw(0x7F)
        time.sleep(0.100)
        before = r.relay_read()
        print(f"  前置状态 = 0x{before:02X} ({r.name(before)})")
        check("C 前置状态 = 0x7F", before, 0x7F)

        for bad in (0x01, 0x02, 0x80, 0xFF, 0x11, 0x20):
            r.relay_write_raw(bad)
            time.sleep(0.100)
            rb = r.relay_read()
            err = r.read_status()["err"]
            ok = (rb == before) and (err == 0x01)
            print(f"  写 0x{bad:02X} → 读 0x{rb:02X} err=0x{err:02X}"
                  f"  {'✓ 被拒且未变' if ok else '✗ 期望 0x'+before.__format__('02X')+'/err=0x01'}")
            if not ok:
                FAILS.append(f"C 非法 0x{bad:02X} 未被正确拒绝")
    finally:
        r.workaround = saved
        r.restore_settle()


def section_d(r):
    """D. 挡位非法值 —— 分**两层**测,别混。

    🔴 2026-10-10 修:原来的 D 节**测错了层**。
       `drivers/relay.py::set_gear()` 在 **Python 侧**就把非独热值挡掉了 ——
       它抛 `RelayError`,**根本不碰硬件**。所以那条路上 `err` 只可能是 0x00,
       而 D 节却在期望 0x02 ⇒ 5 条假失败。
       真要去验 `relay_ctrl.v` 块④ 的 `ERR_GEAR` 分支,必须用 `gear_write_raw()`
       把非法值**真写下去**。分开测之后,两层各自有明确期望值。
    """
    hdr("D. 挡位非法值 —— D1 测 Python 护栏 / D2 测 RTL 拒收")
    r.set_gear("B")
    before = r.gear_read()
    check("D 前置挡位 = B(0x2)", before, R.GEAR_RESET)

    print("  -- D1:set_gear() 的 Python 护栏(应抛异常,**且不碰硬件**)--")
    for bad in (0x0, 0x3, 0x5, 0xF, 0xC):
        try:
            r.set_gear(bad)
            print(f"    set_gear(0x{bad:X}) → **竟然没被拦** ✗")
            FAILS.append(f"D1 set_gear(0x{bad:X}) 未被拦")
        except RelayError:
            rb, err = r.gear_read(), r.read_status()["err"]
            # 护栏在写之前就抛 ⇒ 硬件侧必须毫无动静:挡位不变,而且**不该有 err**
            ok = (rb == before) and (err == 0x00)
            print(f"    set_gear(0x{bad:X}) 被拦;读 0x{rb:X} err=0x{err:02X}"
                  f"  {'✓ 硬件未被触碰' if ok else '✗ 硬件被碰了'}")
            if not ok:
                FAILS.append(f"D1 set_gear(0x{bad:X}) 却动了硬件")

    print("  -- D2:gear_write_raw() 把非法值真写下去(RTL 必须拒收)--")
    for bad in (0x0, 0x3, 0x5, 0xF, 0xC, 0x10):
        r.gear_write_raw(bad)
        time.sleep(0.05)
        rb, err = r.gear_read(), r.read_status()["err"]
        ok = (rb == before) and (err == 0x02)
        print(f"    写 0x{bad:X} → 读 0x{rb:X} err=0x{err:02X}"
              f"  {'✓ RTL 拒收且未变' if ok else '✗'}")
        if not ok:
            FAILS.append(f"D2 非法挡位 0x{bad:X} err 期望 0x02,读到 0x{err:02X}")
        # 下一轮之前把 err 清掉,免得把上一轮的残留当成本轮结果
        r.set_gear("B")


def section_e(r, fclk):
    """E. 稳定延时是否真的按 ms 计(需要原生路径)。"""
    hdr("E. 稳定延时实测")
    saved = r.workaround
    r.workaround = False
    try:
        for ms in (0, 40, 100):
            r.settle_set_raw((ms << 16) | (r.settle_get() & 0xFFFF))
            r.relay_write_raw(R.RELAY_RESET)
            time.sleep(0.300)
            t0 = time.time()
            r.relay_write_raw(0x00)
            while (r.read_status()["busy"]) and time.time() - t0 < 1.0:
                time.sleep(0.0002)
            dt = (time.time() - t0) * 1000.0
            exp = ms * R.FCLK0_DESIGN_MHZ / fclk if fclk else ms
            print(f"  SETTLE={ms:3d} ms → 实测 busy {dt:7.2f} ms"
                  f"  (按 FCLK0={fclk:.2f} MHz 折合期望 {exp:6.2f} ms)")
            if ms > 0 and not (exp * 0.6 < dt < exp * 1.4):
                print("    ⚠️ 落期望区间外")
                FAILS.append(f"E 稳定延时 {ms} ms")
    finally:
        r.workaround = saved
        r.restore_settle()


def closeout(r):
    hdr("收尾")
    try:
        r.apply(R.RELAY_RESET)
        r.set_gear(R.GEAR_RESET)
    except RelayError as e:
        print(f"  ✗ 收尾失败: {str(e).splitlines()[0]}")
        FAILS.append("收尾")
    snap = r.snapshot()
    print(f"  RELAY_CFG = 0x{snap['relay']:02X} ({snap['relay_name']})")
    print(f"  RANGE_SEL = 0x{snap['gear']:X} ({snap['gear_name']})")
    print(f"  err = {snap['err_name']}")
    check("收尾 = ALL_OFF", snap["relay"], R.RELAY_RESET)
    check("收尾 = B 挡", snap["gear"], R.GEAR_RESET)


# ---------------------------------------------------------------------------
def main():
    global DIAGNOSE
    p = argparse.ArgumentParser(description="继电器/挡位功能测试(不加载位流)")
    p.add_argument("--raw", action="store_true",
                   help="用裸 mmap 后端(与 PYNQ MMIO 对照;修后两者结果应一致)")
    p.add_argument("--diagnose", action="store_true",
                   help="追加 B(写→读回回归护栏)与 E(稳定计时实测)")
    p.add_argument("--workaround", action="store_true",
                   help="显式打开 0x1C 的旧绕法(**默认关**;缺陷已修,只为复现历史)")
    args = p.parse_args()
    DIAGNOSE = args.diagnose

    if os.geteuid() != 0:
        print("✗ 需要 root(要 map /dev/mem)。板上用:`sudo -E python3 relay_rw.py`")
        return 2

    backend = "devmem" if args.raw else "mmio"
    # ⚠️ mon=None ⇒ 自己开映射。**不会**加载位流。
    #    backend 就是本脚本的对照变量,两条路分别落在 _MMIORegs / _DevMemRegs。
    r = RelayMatrix(mon=None, workaround=args.workaround, backend=backend)
    print(f"后端 = {r._regs.source}   绕法 = {r.workaround}")
    try:
        # ★ 顺序有意:触发条件对照必须在**任何其它写之前** —— 见 section_trigger 文档串。
        section_trigger(r)
        fclk = section0(r)
        section_a(r)
        section_b(r)              # 缺陷必须一直可见,不做成可选
        if DIAGNOSE:
            section_e(r, fclk)
        section_c(r)
        section_d(r)
        closeout(r)
    except RelayError as e:
        print(f"\n✗ 致命: {e}")
        return 2
    finally:
        r.close()

    hdr(f"结果:{'全部通过' if not FAILS else str(len(FAILS)) + ' 项不符'}")
    for f in FAILS:
        print("  ✗", f)
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
