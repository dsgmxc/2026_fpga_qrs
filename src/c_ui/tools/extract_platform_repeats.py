#!/usr/bin/env python3
"""从 `qrs-ui` 日志里抽出多次**完整三路扫压**的 Cs 曲线。

为什么需要它:
    `fit_anomaly_model.py` 的协方差原来只来自 TH521 的 10 颗器件
    (器件之间的差异),没有包含**本机平台自己的重复性**。平台在两次扫压之间
    可能因为校准周期、继电器状态等整体平移 ~1 %,而 `V75/V25` 特征会把这种
    平移放大成 ~5 % ⇒ 健康件被 AI 判成"异常"(2026-10-29 的 CISS 117 分)。

    这个脚本把同一块健康板的多次健康扫压导出成 CSV,交给
    `fit_anomaly_model.py --repeat`,让模型把平台的重复性算进协方差。

只收什么样的扫压:
    - 一次扫完 CISS/COSS/CRSS 三条腿;
    - 每条腿点数与开始行的 `N 点/腿` 一致(默认 60);
    - 所有读数的括号里都是 `(已校准)`;
    - Cs > 0。
    未校准、单腿、点数不齐、含坏点的扫压整轮丢弃 —— 不拿半截数据去估方差。

用法(数据在交付仓库时把 `report/` 换成 `board/`):
    python3 src/c_ui/tools/extract_platform_repeats.py \
        --log report/measurements/artifacts/20261029_ai_abnormal_run.log \
        --min-line 8170 \
        --output report/measurements/artifacts/20261029_platform_repeat_cal_v2.csv

    `--min-line` / `--max-line` 按日志行号(1 起)筛选。日志里那 7 轮已校准
    扫压中,8170 起才是当前的**通道级校准**;5700–7300 是旧的按腿校准,
    不要混进同一个重复性总体。

输出列:
    run,leg,bias_v,Cs_pF
    `run` 是扫压开始行在源日志里的行号(同一份日志内唯一,便于回溯)。
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import re
import sys
from pathlib import Path

LEG_ORDER = ("CISS", "COSS", "CRSS")

SWEEP_START_RE = re.compile(
    r"\[proto\] 扫压开始:(?P<legs>[A-Z+]+),.*?(?P<points>\d+) 点/腿"
)
LEG_RE = re.compile(r"\[proto\] 扫压第 \d+ 腿:(?P<leg>[A-Z]+)")
BIAS_RE = re.compile(r"\[adc\] 扫压\s+偏压\s+(?P<v>-?[\d.]+) V")
CS_RE = re.compile(
    r"\[adc\] 扫压\s+Z =.*?\| Cs\s+(?P<cs>-?[\d.]+) pF.*?\((?P<state>已校准|未校准)\)"
)
END_RE = re.compile(r"\[proto\] 扫压结束")


def parse_sweeps(lines):
    """返回 [{run, legs:{leg:[(v,cs)]}, row_no, points}] 仅含完整已校准扫压。"""
    starts = [i for i, line in enumerate(lines) if SWEEP_START_RE.search(line)]
    sweeps = []

    for pos, start in enumerate(starts):
        m = SWEEP_START_RE.search(lines[start])
        if m is None:
            continue
        header_legs = m.group("legs").split("+")
        points = int(m.group("points"))
        if tuple(header_legs) != LEG_ORDER:
            continue

        stop = starts[pos + 1] if pos + 1 < len(starts) else len(lines)
        leg = header_legs[0]  # CISS 的腿标记打在"扫压开始"之前,先按表头初始化
        curves = {}
        bias = None
        ok = True

        for i in range(start, stop):
            line = lines[i]
            if END_RE.search(line):
                break
            m = LEG_RE.search(line)
            if m is not None:
                leg = m.group("leg")
                curves.setdefault(leg, [])
                bias = None
                continue
            m = BIAS_RE.search(line)
            if m is not None:
                bias = float(m.group("v"))
                continue
            m = CS_RE.search(line)
            if m is not None and bias is not None:
                if m.group("state") != "已校准":
                    ok = False
                    break
                cs = float(m.group("cs"))
                if not (cs > 0.0):
                    ok = False
                    break
                curves.setdefault(leg, []).append((bias, cs))
                bias = None

        if not ok:
            continue
        if any(len(curves.get(leg, [])) != points for leg in LEG_ORDER):
            continue
        # 偏压必须严格递增,否则曲线不可用(中途换挡/重采会打乱顺序)。
        if any(
            any(curves[leg][k][0] >= curves[leg][k + 1][0] for k in range(points - 1))
            for leg in LEG_ORDER
        ):
            continue
        sweeps.append(
            {"run": start + 1, "legs": {leg: curves[leg] for leg in LEG_ORDER}}
        )

    return sweeps


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--min-line", type=int, default=0,
                    help="只收开始行号 >= 该值的扫压(1 起)")
    ap.add_argument("--max-line", type=int, default=0,
                    help="只收开始行号 <= 该值的扫压(1 起;0 = 不限)")
    args = ap.parse_args()

    raw = args.log.read_bytes()
    lines = raw.decode("utf-8", "replace").splitlines()
    sweeps = [
        s for s in parse_sweeps(lines)
        if s["run"] >= args.min_line and (args.max_line == 0 or s["run"] <= args.max_line)
    ]
    if not sweeps:
        raise SystemExit("没有找到符合条件的完整已校准三路扫压")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    rows = 0
    with open(args.output, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(("run", "leg", "bias_v", "Cs_pF"))
        for s in sweeps:
            for leg in LEG_ORDER:
                for bias, cs in s["legs"][leg]:
                    # str() 保留日志里的原值(最短往返表示),不用 :g 截有效数字。
                    w.writerow((s["run"], leg, str(bias), str(cs)))
                    rows += 1

    print(f"源日志: {args.log}")
    print(f"源 md5: {hashlib.md5(raw).hexdigest()}")
    print(f"扫压: {len(sweeps)} 轮 @ 行 " +
          ", ".join(str(s["run"]) for s in sweeps))
    print(f"每轮: {len(LEG_ORDER)} 腿 × {len(sweeps[0]['legs'][LEG_ORDER[0]])} 点")
    print(f"已写入 {args.output}({rows} 行)")


if __name__ == "__main__":
    sys.exit(main())
