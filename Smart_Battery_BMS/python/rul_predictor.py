#!/usr/bin/env python3
"""
Battery BMS analytics and RUL estimator.

Accepts either:
1. A CSV exported from the ESP32 serial DATA line after converting it to columns.
2. A simple CSV containing voltage,current,power,temperature,soc,soh,cycles,capacity,rul.

Usage:
    python python/rul_predictor.py data/sample_battery_data.csv
"""

from __future__ import annotations
import sys
from pathlib import Path

import numpy as np
import pandas as pd


NOMINAL_LIFE_CYCLES = 400.0
EOL_SOH = 80.0


def load_data(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    df.columns = [c.strip().lower() for c in df.columns]

    aliases = {
        "Voltage": "voltage",
        "Current": "current",
        "Power": "power",
        "Temperature": "temperature",
        "SoC": "soc",
        "SoH": "soh",
        "Cycles": "cycles",
        "Capacity": "capacity",
        "RUL": "rul",
    }

    for src, dst in aliases.items():
        if src.lower() in df.columns and dst not in df.columns:
            df[dst] = df[src.lower()]

    required = ["cycles", "soh", "capacity"]
    missing = [c for c in required if c not in df.columns]
    if missing:
        raise ValueError(f"Missing columns: {missing}")

    return df


def estimate_rul(df: pd.DataFrame) -> pd.Series:
    # Linear interpolation/extrapolation of cycle life from the observed
    # SoH degradation. EOL is defined as 80% SoH.
    x = pd.to_numeric(df["cycles"], errors="coerce")
    y = pd.to_numeric(df["soh"], errors="coerce")

    valid = np.isfinite(x) & np.isfinite(y)
    if valid.sum() < 2:
        return pd.Series(np.full(len(df), NOMINAL_LIFE_CYCLES), index=df.index)

    xv = x[valid].to_numpy()
    yv = y[valid].to_numpy()

    slope, intercept = np.polyfit(xv, yv, 1)

    if slope >= 0:
        remaining = NOMINAL_LIFE_CYCLES - x
    else:
        eol_cycle = (EOL_SOH - intercept) / slope
        remaining = eol_cycle - x

    return remaining.clip(lower=0)


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python python/rul_predictor.py <csv>")
        return 1

    path = Path(sys.argv[1])
    df = load_data(str(path))

    df["predicted_rul_cycles"] = estimate_rul(df)

    latest = df.iloc[-1]
    print("\n=== SMART BATTERY BMS ANALYTICS ===")
    print(f"Samples              : {len(df)}")
    print(f"Latest SoC           : {latest.get('soc', float('nan')):.2f} %")
    print(f"Latest SoH           : {latest['soh']:.2f} %")
    print(f"Latest cycle count   : {latest['cycles']:.5f}")
    print(f"Latest capacity      : {latest['capacity']:.2f} mAh")
    print(f"Predicted RUL        : {df['predicted_rul_cycles'].iloc[-1]:.2f} cycles")

    output = path.with_name(path.stem + "_with_rul.csv")
    df.to_csv(output, index=False)
    print(f"\nSaved: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
