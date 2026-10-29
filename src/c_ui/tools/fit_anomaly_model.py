#!/usr/bin/env python3
"""生成 C 侧异常诊断模型常数(anomaly_model_data.h)。

模型(每条腿独立,输入是 4 维特征):
    f0 = log10(Cs @ 0.3 V)          低压电平
    f1 = log10(Cs @ 100 V)          高压电平
    f2 = log10(V50)                 半降电压(电平 = sqrt(lo*hi))
    f3 = log10(V75 / V25)           拐点宽度(对数量程 25%~75% 的电压比)

健康总体 = 10 颗 TK9J90T 的中位曲线(TH521 原始数据);
平台基线 = 本机健康件一次完整扫压(默认 2026-10-29 已校准那次);
         ⚠️ 平台基线日期从 CSV 文件名前缀读,**不要写死** —— 校准开关一变,
            本机读数就会整体平移,中心必须跟着重采(2026-10-29 踩过);
重复性   = 同一块健康板的多次完整扫压(默认 2026-10-29 的 4 轮,当前通道级校准);
中心 = 重复扫压特征均值(没有 --repeat 时才退回单次平台基线);
协方差 = 10 颗器件的收缩协方差(reg = 0.1)
         + diag(平台重复性方差)  ← 这一项不能省
         器件协方差只描述"健康器件之间差多少",不包含**本机自己的重复性**。
         平台在两次扫压之间会因校准周期、继电器状态等整体平移 ~1 %,而 f3
         会把平移放大成 ~5 %;只用器件协方差时,健康件会被判成"异常"
         (2026-10-29 CISS 117 分)。重复性项就是给这种平台漂移留出的预算。

判据:马氏距离平方 score = d^T P d。
阈值 25 / 100 见下方常数;健康重复扫压的得分应远低于 25。
         score >= 100 判"异常"(对应注入 +10% 电平、8% 电压偏移、30% 拐点拓宽量级)。

用法:
    python3 src/c_ui/tools/fit_anomaly_model.py                    # 写 src/c_ui/app/anomaly_model_data.h
    python3 src/c_ui/tools/fit_anomaly_model.py --check            # 只打印健康/注入分数,不写文件
    python3 src/c_ui/tools/fit_anomaly_model.py --no-repeat        # 旧行为(只含器件协方差,不推荐)

    重复性 CSV 由 `extract_platform_repeats.py` 从板上日志导出。
"""
from __future__ import annotations

import argparse
import csv
import datetime
import math
import re
import sys
import zipfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]


def _data_dir() -> Path:
    """数据目录:工作区是 `report/measurements/artifacts/`,交付仓库是 `board/measurements/artifacts/`。

    两棵树的源码相同、数据位置不同;写死其中一个,另一棵树里默认参数会指到不存在的路径
    (而且报错发生在 `load_*` 里,看起来像"数据丢了")。按存在性选,两个都保留。
    """
    for rel in ("report/measurements/artifacts", "board/measurements/artifacts"):
        if (ROOT / rel).is_dir():
            return ROOT / rel
    return ROOT / "report/measurements/artifacts"


DATA = _data_dir()
DEFAULT_ZIP = DATA / "20261023_th521_reference/th521_source.zip"
DEFAULT_PLATFORM = DATA / "20261029_leg_settle_sweep_cal_v2.csv"
DEFAULT_REPEAT = DATA / "20261029_platform_repeat_cal_v2.csv"
DEFAULT_OUTPUT = ROOT / "src/c_ui/app/anomaly_model_data.h"

LEGS = ("Ciss", "Coss", "Crss")
CSV_LEGS = ("CISS", "COSS", "CRSS")
FEATURE_LOW_V = 0.3
FEATURE_HIGH_V = 100.0
SHRINKAGE = 0.1
THRESHOLD = 25.0
FAULT_THRESHOLD = 100.0
FEATURE_NAMES = ("low level", "high level", "V50", "knee width")


# ---------------------------------------------------------------------------
# 特征提取(必须与 src/c_ui/app/anomaly_detect.c 保持一致)
# ---------------------------------------------------------------------------
def interpolate_log(vs, cs, v):
    """在 (vs, cs) 上做 log-log 插值;点外按首/末两点外推。"""
    i = int(np.searchsorted(vs, v))
    i = max(1, min(i, len(vs) - 1))
    v0, v1 = vs[i - 1], vs[i]
    c0, c1 = cs[i - 1], cs[i]
    t = (math.log10(v) - math.log10(v0)) / (math.log10(v1) - math.log10(v0))
    return 10.0 ** (math.log10(c0) + t * (math.log10(c1) - math.log10(c0)))


def crossing(vs, cs, level):
    """曲线从 level 上方穿到下方的电压(log-log 线性插值);不穿过返回 nan。"""
    for i in range(len(vs) - 1):
        a, b = cs[i] - level, cs[i + 1] - level
        if a * b <= 0.0 and cs[i] != cs[i + 1] and cs[i] > 0.0 and cs[i + 1] > 0.0:
            t = (math.log10(level) - math.log10(cs[i])) / (
                math.log10(cs[i + 1]) - math.log10(cs[i])
            )
            return 10.0 ** (
                math.log10(vs[i]) + t * (math.log10(vs[i + 1]) - math.log10(vs[i]))
            )
    return float("nan")


def features(points):
    vs = np.array([p[0] for p in points], float)
    cs = np.array([p[1] for p in points], float)
    lo = interpolate_log(vs, cs, FEATURE_LOW_V)
    hi = interpolate_log(vs, cs, FEATURE_HIGH_V)
    ratio = hi / lo
    v50 = crossing(vs, cs, math.sqrt(lo * hi))
    v25 = crossing(vs, cs, lo * ratio ** 0.25)
    v75 = crossing(vs, cs, lo * ratio ** 0.75)
    return np.array(
        [math.log10(lo), math.log10(hi), math.log10(v50), math.log10(v75 / v25)]
    )


# ---------------------------------------------------------------------------
# 数据读入
# ---------------------------------------------------------------------------
def load_cohort(zip_path):
    z = zipfile.ZipFile(zip_path)
    # 🔴 只用「同惠仪器测试/1..10」这十颗同批器件。`值钱/` 下另外 3 颗是
    #    另一个批次(见 20261023_th521_reference/README.md),不混进健康总体。
    names = [
        n
        for n in z.namelist()
        if n.endswith("Ciss, Coss, Crss.csv")
        and n.count("/") == 2
        and n.split("/")[0] == "同惠仪器测试"
    ]
    if len(names) != 10:
        raise SystemExit(f"预期 10 颗同批器件,实际匹配到 {len(names)} 颗: {zip_path}")

    def parse(name):
        out, cur = {}, None
        for line in z.read(name).decode("utf-8-sig", "replace").splitlines():
            row = next(csv.reader([line.strip()]))
            if len(row) < 10:
                continue
            tag = row[0].strip()
            for leg in LEGS:
                if tag.startswith(leg):
                    cur = leg
                    break
            else:
                if not tag[:1].isdigit():
                    continue
            try:
                out.setdefault(cur, []).append(
                    (abs(float(row[1])), abs(float(row[9])) * 1e12)
                )
            except (TypeError, ValueError):
                pass
        return out

    devices = []
    for name in sorted(names):
        parsed = parse(name)
        if all(parsed.get(leg) for leg in LEGS):
            devices.append(parsed)
    if len(devices) < 3:
        raise SystemExit(f"可用器件数不足:{len(devices)}")
    return devices


def load_platform(path):
    out = {}
    with open(path, newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            bias = row.get("bias_v", row.get("bias_V"))
            cs = row.get("Cs_pF")
            if bias is None or cs is None:
                continue
            try:
                out.setdefault(row["leg"].upper(), []).append((float(bias), float(cs)))
            except (TypeError, ValueError):
                pass
    for leg in CSV_LEGS:
        if leg not in out:
            raise SystemExit(f"平台健康基线缺少 {leg}:{path}")
    return out


def load_repeats(path):
    """读多次完整扫压,返回 {run: {leg: [(v, cs), ...]}}。

    CSV 列 `run,leg,bias_v,Cs_pF`,由 `extract_platform_repeats.py` 从板上日志导出。
    一轮必须三条腿齐全、每腿至少 8 点、电压严格递增;否则整轮丢弃并提示 ——
    不拿半截曲线估计方差。
    """
    runs = {}
    with open(path, newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            leg = (row.get("leg") or "").strip().upper()
            run = (row.get("run") or "").strip()
            try:
                bias = float(row["bias_v"])
                cs = float(row["Cs_pF"])
            except (KeyError, TypeError, ValueError):
                continue
            if not run or leg not in CSV_LEGS or not (bias > 0.0) or not (cs > 0.0):
                continue
            runs.setdefault(run, {}).setdefault(leg, []).append((bias, cs))

    good = {}
    for run, legs in runs.items():
        if any(leg not in legs for leg in CSV_LEGS):
            print(f"跳过重复扫压 {run}:缺腿", file=sys.stderr)
            continue
        ok = True
        for leg in CSV_LEGS:
            pts = sorted(legs[leg])
            if len(pts) < 8 or any(
                pts[k][0] >= pts[k + 1][0] for k in range(len(pts) - 1)
            ):
                ok = False
                break
            legs[leg] = pts
        if not ok:
            print(f"跳过重复扫压 {run}:点数不足或电压不递增", file=sys.stderr)
            continue
        good[run] = legs

    if len(good) < 3:
        raise SystemExit(f"重复扫压不足 3 轮(实际 {len(good)}):{path}")
    return good


def shrink_cov(x, reg=SHRINKAGE):
    cov = np.cov(x.T, ddof=1)
    return (1.0 - reg) * cov + reg * np.eye(x.shape[1]) * np.trace(cov) / x.shape[1]


# ---------------------------------------------------------------------------
# 模型
# ---------------------------------------------------------------------------
def build(cohort_points, platform_points, repeat_points=None):
    cohort = np.array(
        [[features(dev[leg]) for leg in LEGS] for dev in cohort_points]
    )  # [device][leg][feature]
    platform = np.array(
        [features(platform_points[leg]) for leg in CSV_LEGS]
    )  # [leg][feature]

    run_ids = []
    if repeat_points:
        run_ids = list(repeat_points)
        repeat = np.array(
            [[features(repeat_points[run][leg]) for leg in CSV_LEGS]
             for run in run_ids]
        )  # [run][leg][feature]
        bad = [
            (run_ids[r], CSV_LEGS[leg])
            for r in range(repeat.shape[0])
            for leg in range(len(CSV_LEGS))
            if not np.isfinite(repeat[r, leg, :]).all()
        ]
        if bad:
            raise SystemExit(
                "重复扫压特征无效(曲线不单调/穿越点缺失): "
                + ", ".join(f"{run}/{leg}" for run, leg in bad)
            )
        center = repeat.mean(axis=0)  # [leg][feature] 健康重复扫压的特征均值
        repeat_var = np.var(repeat, axis=0, ddof=1)  # [leg][feature]
    else:
        repeat = None
        center = platform.copy()  # 旧行为:单次平台基线 = 模型中心
        repeat_var = np.zeros_like(platform)

    # 器件协方差描述"健康器件之间差多少";加上平台重复性,才描述"同一块健康板
    # 今天再扫一遍能差多少"。两者缺一,健康重复扫压就会撞上阈值。
    inv_cov = np.stack(
        [
            np.linalg.inv(shrink_cov(cohort[:, leg, :]) + np.diag(repeat_var[leg]))
            for leg in range(len(LEGS))
        ]
    )

    loo = []
    for leg in range(len(LEGS)):
        worst = 0.0
        for k in range(cohort.shape[0]):
            tr = np.delete(cohort[:, leg, :], k, axis=0)
            eff = shrink_cov(tr) + np.diag(repeat_var[leg])
            d = cohort[k, leg, :] - tr.mean(axis=0)
            worst = max(worst, float(d @ np.linalg.inv(eff) @ d))
        loo.append(worst)

    pairwise = []
    for leg in range(len(LEGS)):
        eff = shrink_cov(cohort[:, leg, :]) + np.diag(repeat_var[leg])
        p = np.linalg.inv(eff)
        worst = 0.0
        for a in range(cohort.shape[0]):
            for b in range(cohort.shape[0]):
                if a != b:
                    d = cohort[a, leg, :] - cohort[b, leg, :]
                    worst = max(worst, float(d @ p @ d))
        pairwise.append(worst)

    repeat_scores = []
    if repeat is not None:
        repeat_scores = [
            [float(d @ inv_cov[leg] @ d) for d in (repeat[:, leg, :] - center[leg])]
            for leg in range(len(LEGS))
        ]

    return {
        "cohort": cohort,
        "platform": platform,
        "repeat": repeat,
        "repeat_var": repeat_var,
        "repeat_scores": repeat_scores,
        "run_ids": run_ids,
        "mu": cohort.mean(axis=0),
        "center": center,
        "inv_cov": inv_cov,
        "offset": center - cohort.mean(axis=0),
        "loo": loo,
        "pairwise": pairwise,
    }


def score(x, model, leg):
    d = x - model["center"][leg]
    return float(d @ model["inv_cov"][leg] @ d)


def injected_scores(platform_points, model):
    """对平台健康基线做 3 类注入,返回 {类: [CISS,COSS,CRSS] 分数} 供报告引用。"""
    def inject(points, kind, mag):
        vs = np.array([p[0] for p in points], float)
        cs = np.array([p[1] for p in points], float)
        if kind == "level":
            cs2 = cs * mag
        elif kind == "volt":
            cs2 = np.array([interpolate_log(vs, cs, v * mag) for v in vs])
        else:  # knee
            lo = interpolate_log(vs, cs, FEATURE_LOW_V)
            hi = interpolate_log(vs, cs, FEATURE_HIGH_V)
            v50 = crossing(vs, cs, math.sqrt(lo * hi))
            cs2 = np.array(
                [interpolate_log(vs, cs, v50 * (v / v50) ** mag) for v in vs]
            )
        return list(zip(vs, cs2))

    out = {}
    for kind, mags in (("level", (1.05, 1.10)), ("volt", (1.05, 1.08)), ("knee", (1.2, 1.3))):
        for mag in mags:
            vals = []
            for leg, name in enumerate(CSV_LEGS):
                vals.append(
                    score(features(inject(platform_points[name], kind, mag)), model, leg)
                )
            out[(kind, mag)] = vals
    return out


# ---------------------------------------------------------------------------
# 输出
# ---------------------------------------------------------------------------
def baseline_date(platform_path: Path) -> str:
    """平台基线 CSV 的文件名前缀 YYYYMMDD → `YYYY-MM-DD`;没有前缀就用今天。"""
    m = re.match(r"(\d{4})(\d{2})(\d{2})", platform_path.name)
    if m is not None:
        return f"{m.group(1)}-{m.group(2)}-{m.group(3)}"
    return datetime.date.today().isoformat()


def write_header(path, model, zip_path, platform_path, repeat_path=None):
    center = ",\n".join(
        "    { " + ", ".join(f"{v:.8e}f" for v in model["center"][leg]) + " }"
        for leg in range(3)
    )
    inv_rows = []
    for leg in range(3):
        rows = []
        for row in model["inv_cov"][leg]:
            rows.append("        { " + ", ".join(f"{v:.8e}f" for v in row) + " }")
        inv_rows.append("    {\n" + ",\n".join(rows) + "\n    }")
    inv_cov = ",\n".join(inv_rows)

    center_comment = (
        "健康重复扫压的特征均值" if repeat_path is not None
        else "平台健康基线的 4 维特征"
    )
    if repeat_path is not None:
        center_desc = (
            f"{len(model['run_ids'])} 次本机健康扫压的特征均值"
            f"({repeat_path.name})"
        )
        cov_desc = (
            "10 颗器件收缩协方差(reg = 0.1) + diag(平台重复性方差)\n"
            " *           重复性 σ: " + " / ".join(
                f"{CSV_LEGS[leg]} ["
                + ", ".join(f"{v:.4f}" for v in np.sqrt(model['repeat_var'][leg]))
                + "]"
                for leg in range(3)
            )
        )
        repeat_line = (
            f" * 重复扫压: {repeat_path.name}("
            f"{len(model['run_ids'])} 轮同配置扫压,用于估计本机重复性)\n"
        )
        date_src = repeat_path
        healthy_max = max(
            (s for scores in model["repeat_scores"] for s in scores), default=0.0
        )
        healthy_line = (
            f"健康样本最大得分:留一法 {max(model['loo']):.2f} / "
            f"两两 {max(model['pairwise']):.2f} / "
            f"重复扫压 {healthy_max:.2f}"
        )
    else:
        center_desc = "平台健康基线的特征(= 总体均值 + 平台偏移,偏移已吸收)"
        cov_desc = "10 颗器件收缩协方差(reg = 0.1)的逆(未含平台重复性)"
        repeat_line = ""
        date_src = platform_path
        healthy_line = (
            f"健康样本最大得分:留一法 {max(model['loo']):.2f} / "
            f"两两 {max(model['pairwise']):.2f}"
        )

    text = f"""/* ⚠️ 自动生成,勿手改 —— src/c_ui/tools/fit_anomaly_model.py
 *
 * 生成日期: {datetime.date.today().isoformat()}
 * 健康总体: {zip_path.name} 中 10 颗 TK9J90T 的中位曲线(TH521 实测)
 * 平台基线: {platform_path.name}(本机健康件完整扫压,0.1–100 V,60 点/腿)
{repeat_line} * 特征:     [log10 Cs@0.3V, log10 Cs@100V, log10 V50, log10(V75/V25)]
 * 中心:     {center_desc}
 * 协方差:   {cov_desc}
 * 阈值:     score < {THRESHOLD:.0f} 正常;{THRESHOLD:.0f}–{FAULT_THRESHOLD:.0f} 可疑;≥ {FAULT_THRESHOLD:.0f} 异常
 *           {healthy_line}
 * ⚠️ 只对**当前固定配置**有效(1 MHz、32.5 MSPS、B 挡 100 Ω、通道级校准)。
 */
#ifndef ANOMALY_MODEL_DATA_H
#define ANOMALY_MODEL_DATA_H

#define ANOMALY_MODEL_FEATURE_COUNT   4u
#define ANOMALY_MODEL_THRESHOLD       {THRESHOLD:.1f}f
#define ANOMALY_MODEL_FAULT_THRESHOLD {FAULT_THRESHOLD:.1f}f
#define ANOMALY_MODEL_BASELINE_DATE   "{baseline_date(date_src)}"

/* 模型中心({center_comment}),行序 CISS / COSS / CRSS */
static const float g_anomaly_model_center[3][4] = {{
{center}
}};

/* 有效协方差的逆(器件协方差 + 平台重复性),行序同上 */
static const float g_anomaly_model_inv_cov[3][4][4] = {{
{inv_cov}
}};

#endif /* ANOMALY_MODEL_DATA_H */
"""
    path.write_text(text, encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--zip", type=Path, default=DEFAULT_ZIP)
    ap.add_argument("--platform", type=Path, default=DEFAULT_PLATFORM)
    ap.add_argument("--repeat", type=Path, default=DEFAULT_REPEAT,
                    help="多次健康扫压 CSV(extract_platform_repeats.py 导出)")
    ap.add_argument("--no-repeat", action="store_true",
                    help="不读重复性数据(旧行为;健康件会被平台漂移误报)")
    ap.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    ap.add_argument("--check", action="store_true", help="只打印分数,不写头文件")
    args = ap.parse_args()

    devices = load_cohort(args.zip)
    platform = load_platform(args.platform)
    repeats = None if args.no_repeat else load_repeats(args.repeat)
    model = build(devices, platform, repeats)

    source = "重复扫压均值" if repeats is not None else "单次平台基线"
    print(f"器件数 {len(devices)};中心 = {source}")
    print("留一法最大健康分 : " + " ".join(f"{v:6.2f}" for v in model["loo"]))
    print("两两最大健康分   : " + " ".join(f"{v:6.2f}" for v in model["pairwise"]))
    print("平台基线得分     : " + " ".join(
        f"{score(model['platform'][leg], model, leg):6.2f}" for leg in range(3)))
    print("平台偏移(log10)  : " + " ".join(
        f"{CSV_LEGS[leg]}[{', '.join(f'{v:+.3f}' for v in model['offset'][leg])}]"
        for leg in range(3)))
    if repeats is not None:
        for leg in range(3):
            print(f"重复性 σ {CSV_LEGS[leg]:4s} : " + " ".join(
                f"{FEATURE_NAMES[k]}={model['repeat_var'][leg][k] ** 0.5:.4f}"
                for k in range(4)))
        print("重复扫压得分(应 < 阈值):")
        for r, run in enumerate(model["run_ids"]):
            vals = [model["repeat_scores"][leg][r] for leg in range(3)]
            print(f"  run {run:>6s}  CISS {vals[0]:6.2f}  COSS {vals[1]:6.2f}  CRSS {vals[2]:6.2f}")
    print("注入异常得分:")
    for (kind, mag), vals in injected_scores(platform, model).items():
        print(f"  {kind:<5} x{mag:<4}  CISS {vals[0]:8.1f}  COSS {vals[1]:8.1f}  CRSS {vals[2]:8.1f}")

    if args.check:
        return
    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_header(args.output, model, args.zip, args.platform,
                 None if repeats is None else args.repeat)
    print(f"\n已写入 {args.output}")


if __name__ == "__main__":
    sys.exit(main())
