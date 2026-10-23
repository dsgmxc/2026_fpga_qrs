# TK9J90T typical C-V / Rg reference — ten-device batch (2026-07-16)

Typical curves for the MOSFET used on the QRS fixture — **TK9J90T** — derived from
the TongHui TH521 export provided by the user (`同惠仪器测试.zip`, archived here as
`th521_source.zip`, md5 `666f047494e4dbc7001d26a390c7e479`).

> **TH521 is the C-V analyzer, not the part.** The generator/file names keep the
> instrument prefix (`th521_*`); the device model shown in the UI is TK9J90T
> (corrected by the user 2026-10-23).

## Source and selection

| Item | Value |
|---|---|
| Instrument | TongHui **TH521** C-V analyzer |
| Measurement date | 2026-07-16 |
| Device under test | **TK9J90T** (per user 2026-10-23) |
| Devices used | **ten**, folders `1`–`10` (same model) |
| Excluded | `值钱/11`, `值钱/13` (folder `值钱/3`), `值钱/14` — see note below |
| Excitation | **1 MHz**, **AC 0.5 V**, NPLC 1 |
| Bias sweep | Vds **0.1 → 100 V**, log, **201 steps**, single sweep |
| Meter config | `PhaseCompensation: Auto`, `CrssPlus: OFF` |
| Rg sweep | Vgs **−3 → +3 V**, 101 steps, Vds ≈ 0 |

The three excluded "值钱" devices sit only **1.4–2.5 %** (median) from the typical
curve over 1–100 V, i.e. they are probably the same model from another batch. They
were left out because the user specified ten same-model samples; re-including them
would change the typical values by less than the device spread except near the
Coss/Crss knee.

## Method

1. Each device records the **actual** Vds/Vgs (not the commanded value), so the
   curves do not share a grid. Every curve is therefore interpolated onto the
   nominal grid — linear in `(log Vds, log C)`; linear in `(Vgs, Rg)`.
2. **Typical = per-point median over the ten devices.** Median, not mean: the
   Coss/Crss knee has a few early-transition outliers that would drag the mean.
3. SD / min / max columns are **device-to-device spread**, not repeatability.
4. Most typical single device (smallest RMS log deviation from the median,
   1–100 V): **device 7 (1.71 %)**, then 9 (1.97 %), 8 (3.43 %).

Generator: `tools/make_th521_reference.py` (re-runs from the archived zip).

## Files

| File | Content |
|---|---|
| `th521_typ_cv.csv` | 201 rows: `Vds_V`, then `typ/sd/min/max` in pF for Ciss, Coss, Crss |
| `th521_typ_rg.csv` | 101 rows: `Vgs_V`, `Rg` typ/sd/min/max in Ω |
| `th521_source.zip` | original instrument export, untouched |

The same generator emits the firmware table used by the on-screen UI
(`--emit-c`, default `src/c_ui/app/ref_th521_data.c`):

| Symbol | Content |
|---|---|
| `g_ref_th521_vds[201]` | Vds grid, 0.1 → 100 V, log |
| `g_ref_th521_cs_pf[3][201]` | median Cs per leg (Ciss, Coss, Crss) |
| `g_ref_th521_d[3][201]` | median D per leg |
| `g_ref_th521_rg_ohm` | 3.06448 Ω |

**No Cp table is stored.** Cs and D are the two independent medians; Cp is
derived at lookup time as `Cp = Cs / (1 + D²)` so that the three numbers shown
for one bias point are always mutually consistent by construction. (Taking a
per-point median of Cp independently would break that identity by up to 2.4 % —
the three columns of a single meter reading are exactly consistent, but the
*medians* over ten devices of three correlated quantities are not the median
device's numbers.)

## Typical values

| Vds | Ciss pF | Coss pF | Crss pF |
|---:|---:|---:|---:|
| 0.1 V | 3212 ± 29 | 4163 ± 21 | 1504 ± 31 |
| 1 V | 2935 ± 25 | 3009 ± 20 | 1153 ± 24 |
| 10 V | 2136 ± 27 | 761 ± 182 | 230 ± 58 |
| 50 V | 2019 ± 4 | 123.1 ± 1.0 | 5.27 ± 0.36 |
| 100 V | 2005 ± 4 | 87.1 ± 0.6 | 3.72 ± 0.21 |

**Rg ≈ 3.06 Ω** (median of the 101-point median curve; device-to-device SD 0.03 Ω,
curve range 2.95–3.75 Ω from Vgs −3 V to +3 V).

## Caveats

- This is a **model-level reference**, not the response of one specific part.
  Comparing a single fixture reading against it is valid to roughly the device
  spread quoted above (±1 % for Ciss/Coss at high bias, up to ±30 % in the Coss knee).
- The Coss/Crss **knee location varies strongly between devices** (Coss @10 V spans
  396–977 pF). A mismatch there is not by itself evidence of a measurement fault.
- At 100 V, Crss is ~4 pF while the meter's own R reading changes sign — treat the
  last decade of Crss as at the instrument's resolution limit.
- The meter's capacitance column is the **series-equivalent Cs from X** (verified:
  `C = 1/(2πf|X|)` reproduces it numerically), i.e. the same definition as the
  project's `Cs = −1e12/(2πf·Im Z)`. `Cp` and `D` from our board are **not**
  comparable without removing the ~7–10 Ω of series resistance our fixture adds.
