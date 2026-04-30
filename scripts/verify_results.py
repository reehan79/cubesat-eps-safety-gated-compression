#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

import pandas as pd

ROOT = Path(__file__).resolve().parents[1]
TABLES = ROOT / "results" / "tables"

REQUIRED_CSVS = [
    "t5_safety_dashboard.csv",
    "t5_main_results.csv",
    "t5_lossless_baselines.csv",
    "t5_method_ranking.csv",
    "t5_embedded_profile.csv",
    "t5_safety_thresholds.csv",
    "birds_eps_dataset_summary.csv",
    "birds_eps_channel_list.csv",
]


def check_required_files(errors: list[str]) -> None:
    for name in REQUIRED_CSVS:
        p = TABLES / name
        if not p.is_file():
            errors.append(f"Missing required file: {p}")


def check_safety_dashboard(errors: list[str]) -> None:
    p = TABLES / "t5_safety_dashboard.csv"
    if not p.is_file():
        return
    df = pd.read_csv(p)
    needed = {
        ("lossless_baseline", "True"): 32,
        ("learned_compression", "False"): 149,
        ("learned_compression_all_numeric_stress_test", "False"): 143,
    }
    for (group_value, safe_pass), expected in needed.items():
        mask = (
            (df["group_dim"].astype(str) == "result_type")
            & (df["group_value"].astype(str) == group_value)
            & (df["safe_pass"].astype(str) == safe_pass)
        )
        found = int(df.loc[mask, "count"].sum())
        if found != expected:
            errors.append(
                f"Safety dashboard mismatch for {group_value}/{safe_pass}: "
                f"expected {expected}, found {found}"
            )


def check_embedded_profile(errors: list[str]) -> None:
    p = TABLES / "t5_embedded_profile.csv"
    if not p.is_file():
        return
    df = pd.read_csv(p)
    if "board" not in df.columns:
        errors.append("t5_embedded_profile.csv has no 'board' column")
        return

    boards = set(df["board"].dropna().astype(str).tolist())
    required_boards = {"raspberry_pi_4", "esp32", "stm32f429zi", "stm32f413zh"}
    missing = sorted(required_boards - boards)
    if missing:
        errors.append(f"Embedded profile missing required board rows: {missing}")

    if "method" not in df.columns:
        errors.append("t5_embedded_profile.csv has no 'method' column")
    else:
        tflm_mask = (df["board"].astype(str) == "esp32") & (
            df["method"].astype(str) == "tflite_micro_encoder_int8"
        )
        if not bool(tflm_mask.any()):
            errors.append("Missing ESP32 TFLM row (method=tflite_micro_encoder_int8)")

    if "energy_status" in df.columns:
        energy = df["energy_status"].fillna("").astype(str).str.lower()
        measured_mask = energy.str.contains("measured") & ~energy.str.contains("not_measured")
        if bool(measured_mask.any()):
            errors.append("Found row(s) claiming measured energy, which is not allowed")


def main() -> int:
    errors: list[str] = []

    check_required_files(errors)
    check_safety_dashboard(errors)
    check_embedded_profile(errors)

    optional_stress = TABLES / "t5_stress_test_results.csv"
    stress_note = "present" if optional_stress.is_file() else "not present"

    print("=== T5 Repro Verification ===")
    print(f"Tables directory: {TABLES}")
    print(f"Optional t5_stress_test_results.csv: {stress_note}")
    if errors:
        print("\nFAIL")
        for err in errors:
            print(f"- {err}")
        return 1

    print("\nPASS")
    print("- All required CSV files exist")
    print("- Safety dashboard counts match expected values")
    print("- Embedded profile has required board rows including ESP32 TFLM")
    print("- No rows claim measured energy")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
