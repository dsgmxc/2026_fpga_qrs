#!/usr/bin/env python3
"""Build the typical C-V / Rg reference table for the MOSFET batch measured with a
TongHui TH521 (2026-07-16).

Source: TH521 export zip, 13 device folders.  Device folders 1..10 are taken as the
ten samples of the same model — **TK9J90T** (per user, 2026-10-23; TH521 is the
instrument, not the part).  The three folders under '值钱/' (11, 13, 14) are kept out
of the typical table but reported for transparency.

Method
    * Each device's C-V sweep is measured on a slightly different Vds grid (the meter
      records the actual bias), so every curve is first linearly interpolated in
      (log Vds, log C) onto the nominal 201-point grid 0.1 -> 100 V.
    * Rg is interpolated in (Vgs, R) onto the nominal grid -3 -> +3 V, step 0.06 V.
    * 'Typical' = per-point MEDIAN over the ten devices.  Median is used instead of the
      mean because the Coss/Crss knee region has a heavy right tail (a few devices
      transition earlier); the median stays on the majority behaviour.
    * The SD / min / max columns are the device-to-device spread, not measurement noise.

Usage
    python3 tools/make_th521_reference.py [--zip PATH] [--out DIR]
"""

import argparse
import csv
import hashlib
import io
import math
import os
import statistics
import zipfile

SECTIONS = ("Ciss", "Coss", "Crss")
TYPICAL_DEVS = [str(i) for i in range(1, 11)]        # the ten same-model samples
EXTRA_DEVS = [("值钱/11", "11"), ("值钱/3", "13"), ("值钱/14", "14")]
N_CV = 201
N_RG = 101


def nominal_cv_grid():
    return [10.0 ** (-1.0 + 3.0 * i / (N_CV - 1)) for i in range(N_CV)]


def nominal_rg_grid():
    return [-3.0 + 6.0 * i / (N_RG - 1) for i in range(N_RG)]


def parse_sections(text, cv=False):
    """Return {section: [(x, value), ...]} including the label row (first point).

    cv=True keeps (VDS, R, X, C) for the C-V sections so Cp / D can be derived
    from the meter's own R and X at every bias; otherwise (X, R) for the Rg sweep.
    """
    secs = {}
    cur = None
    for row in csv.reader(io.StringIO(text)):
        row = [c.strip() for c in row if c.strip() != ""]
        if not row:
            continue
        head = row[0]
        is_label = head.startswith(("Ciss", "Coss", "Crss", "Rg")) and not head[1:2].isdigit()
        if is_label:
            cur = head.rstrip("0123456789")
            secs.setdefault(cur, [])
        if cur is None or len(row) < 10:
            continue
        try:
            if cur == "Rg":
                secs[cur].append((float(row[3]), float(row[9])))   # VGS, R
            elif cv:
                secs[cur].append((float(row[1]), float(row[6]),
                                  float(row[7]), float(row[9])))   # VDS, R, X, C
            else:
                secs[cur].append((float(row[1]), float(row[9])))   # VDS, C
        except ValueError:
            continue
    return secs


def loglog_interp(points, grid):
    pts = sorted(points)
    xs = [math.log(p[0]) for p in pts]
    ys = [math.log(p[1]) for p in pts]
    out = []
    j = 0
    for g in grid:
        x = math.log(g)
        while j < len(xs) - 2 and xs[j + 1] < x:
            j += 1
        if x <= xs[0]:
            out.append(math.exp(ys[0]))
        elif x >= xs[-1]:
            out.append(math.exp(ys[-1]))
        else:
            t = (x - xs[j]) / (xs[j + 1] - xs[j])
            out.append(math.exp(ys[j] + t * (ys[j + 1] - ys[j])))
    return out


def lin_interp(points, grid):
    pts = sorted(points)
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    out = []
    j = 0
    for g in grid:
        while j < len(xs) - 2 and xs[j + 1] < g:
            j += 1
        if g <= xs[0]:
            out.append(ys[0])
        elif g >= xs[-1]:
            out.append(ys[-1])
        else:
            t = (g - xs[j]) / (xs[j + 1] - xs[j])
            out.append(ys[j] + t * (ys[j + 1] - ys[j]))
    return out


def logx_interp(points, grid):
    """Linear in (log x, y) — used for D, which can approach zero."""
    pts = sorted(points)
    xs = [math.log(p[0]) for p in pts]
    ys = [p[1] for p in pts]
    out = []
    j = 0
    for g in grid:
        x = math.log(g)
        while j < len(xs) - 2 and xs[j + 1] < x:
            j += 1
        if x <= xs[0]:
            out.append(ys[0])
        elif x >= xs[-1]:
            out.append(ys[-1])
        else:
            t = (x - xs[j]) / (xs[j + 1] - xs[j])
            out.append(ys[j] + t * (ys[j + 1] - ys[j]))
    return out


def load_device(zf, prefix):
    """prefix: path inside the zip, e.g. '同惠仪器测试/1'."""
    def read(name):
        return zf.read(f"{prefix}/{name}").decode("utf-8", errors="replace")

    cv = parse_sections(read("Ciss, Coss, Crss.csv"), cv=True)
    rg = parse_sections(read("Rg.csv"))
    return cv, rg


def stats_row(vals):
    m = statistics.median(vals)
    return m, statistics.stdev(vals), min(vals), max(vals)


def emit_c(path, grid, med_cs, med_d, rg_ohm, zip_md5):
    """Write the compact C table consumed by src/c_ui/app/ref_model.c."""
    def cfmt(v):
        s = f"{v:.6g}"
        if ("." not in s) and ("e" not in s) and ("E" not in s):
            s += ".0"          # "100f" 是非法整数后缀,必须写成 "100.0f"
        return s + "f"

    def block(values, indent="    "):
        out = []
        for i in range(0, len(values), 8):
            out.append(indent + ", ".join(cfmt(v) for v in values[i:i + 8]) + ",")
        return "\n".join(out)

    def matrix(name, rows):
        body = ",\n".join("  {   /* %s */\n%s\n  }" % (sec, block(rows[sec]))
                          for sec in SECTIONS)
        return f"const float {name}[REF_TH521_LEGS][REF_TH521_CV_POINTS] = {{\n{body}\n}};\n"

    with open(path, "w", encoding="utf-8") as f:
        f.write("/* AUTO-GENERATED by tools/make_th521_reference.py -- do not edit.\n"
                " *\n"
                " * Source: TongHui TH521 export, ten TK9J90T devices, 2026-07-16.\n"
                " * (TH521 = the C-V analyzer; TK9J90T = the part under test.)\n"
                f" * Zip md5: {zip_md5}\n"
                " * Typical = per-point median over devices 1..10 on the nominal grid\n"
                " * Vds = 0.100 .. 100 V, 201 log-spaced points.\n"
                " * Cs = series-equivalent capacitance from X (same definition as the\n"
                " * project's Cs).  D = |R / X|.  Cp is NOT stored: the UI derives it\n"
                " * as Cs / (1 + D^2), so the three displayed values stay consistent\n"
                " * even though Cs and D are independent per-point medians.\n"
                " * Rg: median of the 101-point median Rg curve, Vgs -3 .. +3 V.\n"
                " */\n\n"
                '#include "ref_th521_data.h"\n\n')

        f.write(f"const float g_ref_th521_vds[REF_TH521_CV_POINTS] = {{\n"
                f"{block(grid)}\n}};\n\n")
        f.write(matrix("g_ref_th521_cs_pf", med_cs))
        f.write("\n")
        f.write(matrix("g_ref_th521_d", med_d))
        f.write("\n")
        f.write(f"const float g_ref_th521_rg_ohm = {cfmt(rg_ohm)};\n")


def main():
    ap = argparse.ArgumentParser()
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    default_zip = os.path.join(
        here, "report/measurements/artifacts/20261023_th521_reference/th521_source.zip")
    ap.add_argument("--zip", default=default_zip)
    ap.add_argument("--out", default=os.path.join(
        here, "report/measurements/artifacts/20261023_th521_reference"))
    ap.add_argument("--emit-c", default=os.path.join(
        here, "src/c_ui/app/ref_th521_data.c"),
        help="also write the C table used by the on-board UI ('' to skip)")
    args = ap.parse_args()

    cv_grid = nominal_cv_grid()
    rg_grid = nominal_rg_grid()

    with zipfile.ZipFile(args.zip) as zf:
        cv, rg = {}, {}
        for d in TYPICAL_DEVS:
            cv[d], rg[d] = load_device(zf, f"同惠仪器测试/{d}")
        extra = {}
        for folder, label in EXTRA_DEVS:
            extra[label] = load_device(zf, f"同惠仪器测试/{folder}")

    # interpolate every typical device onto the nominal grid.
    # Cs: log-log (the C-V law).  D = |R/X|: linear in (log Vds, D).
    # Cp is not tabulated -- it follows from Cs and D by definition.
    def cs_points(pts):
        return [(v, c) for (v, _r, _x, c) in pts if v > 0 and c > 0]

    def d_points(pts):
        out = []
        for (v, r, x, _c) in pts:
            if (v <= 0) or (x == 0) or not math.isfinite(r):
                continue
            out.append((v, abs(r / x)))
        return out

    curves = {s: {d: loglog_interp(cs_points(cv[d][s]), cv_grid)
                  for d in TYPICAL_DEVS} for s in SECTIONS}
    curves_d = {s: {d: logx_interp(d_points(cv[d][s]), cv_grid)
                    for d in TYPICAL_DEVS} for s in SECTIONS}
    rg_curves = {d: lin_interp(rg[d]["Rg"], rg_grid) for d in TYPICAL_DEVS}

    os.makedirs(args.out, exist_ok=True)

    # ---- typical C-V table ----
    cv_path = os.path.join(args.out, "th521_typ_cv.csv")
    with open(cv_path, "w", newline="") as f:
        w = csv.writer(f)
        head = ["Vds_V"]
        for s in SECTIONS:
            head += [f"{s}_typ_pF", f"{s}_sd_pF", f"{s}_min_pF", f"{s}_max_pF"]
        w.writerow(head)
        for i, vds in enumerate(cv_grid):
            row = [f"{vds:.6g}"]
            for s in SECTIONS:
                vals = [curves[s][d][i] * 1e12 for d in TYPICAL_DEVS]
                row += [f"{x:.4f}" for x in stats_row(vals)]
            w.writerow(row)

    # ---- typical Rg table ----
    rg_path = os.path.join(args.out, "th521_typ_rg.csv")
    with open(rg_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Vgs_V", "Rg_typ_ohm", "Rg_sd_ohm", "Rg_min_ohm", "Rg_max_ohm"])
        for i, vgs in enumerate(rg_grid):
            vals = [rg_curves[d][i] for d in TYPICAL_DEVS]
            w.writerow([f"{vgs:.4g}"] + [f"{x:.4f}" for x in stats_row(vals)])

    # ---- summary ----
    def med_at(vals_by_dev, grid, target):
        i = min(range(len(grid)), key=lambda k: abs(math.log(grid[k]) - math.log(target)))
        return i

    print("typical C-V (median of 10 devices), pF")
    print(f"{'Vds':>8} | {'Ciss':>9} {'±SD':>7} | {'Coss':>9} {'±SD':>7} | {'Crss':>9} {'±SD':>7}")
    for target in (0.1, 1, 10, 50, 100):
        i = med_at(cv_grid, cv_grid, target)
        out = []
        for s in SECTIONS:
            vals = [curves[s][d][i] * 1e12 for d in TYPICAL_DEVS]
            out += [statistics.median(vals), statistics.stdev(vals)]
        print(f"{cv_grid[i]:8.3f} | {out[0]:9.1f} {out[1]:7.1f} | {out[2]:9.1f} {out[3]:7.1f} | {out[4]:9.2f} {out[5]:7.2f}")

    rg_med = [statistics.median([rg_curves[d][i] for d in TYPICAL_DEVS]) for i in range(N_RG)]
    rg_typ = statistics.median(rg_med)
    print(f"\nRg: median over Vgs = {rg_typ:.3f} ohm "
          f"(curve range {min(rg_med):.3f}..{max(rg_med):.3f})")

    if args.emit_c:
        med_cs = {s: [statistics.median([curves[s][d][i] * 1e12 for d in TYPICAL_DEVS])
                      for i in range(N_CV)] for s in SECTIONS}
        med_d = {s: [statistics.median([curves_d[s][d][i] for d in TYPICAL_DEVS])
                     for i in range(N_CV)] for s in SECTIONS}
        with open(args.zip, "rb") as zf:
            zip_md5 = hashlib.md5(zf.read()).hexdigest()
        os.makedirs(os.path.dirname(args.emit_c), exist_ok=True)
        emit_c(args.emit_c, cv_grid, med_cs, med_d, rg_typ, zip_md5)
        print(f"wrote {args.emit_c}")

    # most typical single device: smallest RMS log deviation from the median curve, 1..100 V
    lo = cv_grid.index(next(v for v in cv_grid if v >= 1.0))
    hi = len(cv_grid) - 1
    best = []
    for d in TYPICAL_DEVS:
        e = []
        for s in SECTIONS:
            for i in range(lo, hi + 1):
                med = statistics.median([curves[s][x][i] for x in TYPICAL_DEVS])
                e.append(math.log(curves[s][d][i] / med))
        best.append((math.sqrt(statistics.mean([x * x for x in e])), d))
    best.sort()
    print("most typical single device (RMS log dev vs median, 1..100 V):")
    for rms, d in best[:3]:
        print(f"  dev {d}: {rms * 100:.2f}%")

    # excluded devices: deviation from the typical curve
    print("\nexcluded (值钱) devices, median |dev| vs typical over 1..100 V:")
    for label, (ecv, _) in extra.items():
        devs = []
        for s in SECTIONS:
            arr = loglog_interp(cs_points(ecv[s]), cv_grid)
            for i in range(lo, hi + 1):
                med = statistics.median([curves[s][x][i] for x in TYPICAL_DEVS])
                devs.append(abs(arr[i] / med - 1))
        print(f"  dev {label}: median {statistics.median(devs) * 100:.2f}%  max {max(devs) * 100:.1f}%")

    print(f"\nwrote {cv_path}\nwrote {rg_path}")


if __name__ == "__main__":
    main()
