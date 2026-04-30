#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

import matplotlib

matplotlib.use("pdf")
import matplotlib.pyplot as plt
import pandas as pd

ROOT = Path(__file__).resolve().parents[1]
TABLES = ROOT / "results" / "tables"
PLOTS = ROOT / "results" / "plots"


def fig_safety_outcomes() -> None:
    dash = pd.read_csv(TABLES / "t5_safety_dashboard.csv")
    sub = dash[dash["group_dim"].astype(str) == "result_type"].copy()
    pivot = sub.pivot_table(
        index="group_value",
        columns="safe_pass",
        values="count",
        aggfunc="sum",
        fill_value=0,
    )
    ax = pivot.plot(kind="bar", figsize=(7, 4.5))
    ax.set_xlabel("Result Type")
    ax.set_ylabel("Count")
    ax.set_title("Safety Outcomes by Result Type")
    ax.legend(title="safe_pass")
    plt.tight_layout()
    plt.savefig(PLOTS / "fig_safety_outcomes.pdf")
    plt.close()


def fig_cr_latency_safety() -> None:
    main = pd.read_csv(TABLES / "t5_main_results.csv")
    main = main.copy()
    main["safe_pass_str"] = main["safe_pass"].astype(str)
    colors = main["safe_pass_str"].map({"True": "tab:green", "False": "tab:red"}).fillna("tab:gray")

    plt.figure(figsize=(7, 5))
    plt.scatter(
        pd.to_numeric(main["compression_ratio_mean"], errors="coerce"),
        pd.to_numeric(main["encode_ms_mean"], errors="coerce"),
        c=colors,
        alpha=0.8,
        s=30,
    )
    plt.xlabel("Compression Ratio (mean)")
    plt.ylabel("Encode Latency ms (mean)")
    plt.title("Compression vs Latency with Safety Labels")
    plt.tight_layout()
    plt.savefig(PLOTS / "fig_cr_latency_safety.pdf")
    plt.close()


def fig_embedded_latency() -> None:
    emb = pd.read_csv(TABLES / "t5_embedded_profile.csv")
    use = emb.dropna(subset=["board", "encode_ms_mean"]).copy()
    grouped = (
        use.groupby("board", as_index=False)["encode_ms_mean"].min().sort_values("encode_ms_mean", ascending=True)
    )

    plt.figure(figsize=(7, 4.5))
    plt.barh(grouped["board"].astype(str), grouped["encode_ms_mean"])
    plt.xlabel("Best Encode Latency ms (lower is better)")
    plt.title("Embedded Latency by Board (min encode mean)")
    plt.tight_layout()
    plt.savefig(PLOTS / "fig_embedded_latency.pdf")
    plt.close()


def fig_deployment_tiers() -> None:
    emb = pd.read_csv(TABLES / "t5_embedded_profile.csv")
    board_counts = emb.groupby("board").size().sort_values(ascending=False)

    plt.figure(figsize=(7, 4.5))
    plt.bar(board_counts.index.astype(str), board_counts.values)
    plt.xlabel("Deployment Target")
    plt.ylabel("Profiled Rows")
    plt.title("Deployment Tiers Coverage")
    plt.xticks(rotation=20, ha="right")
    plt.tight_layout()
    plt.savefig(PLOTS / "fig_deployment_tiers.pdf")
    plt.close()


def main() -> int:
    PLOTS.mkdir(parents=True, exist_ok=True)
    fig_safety_outcomes()
    fig_cr_latency_safety()
    fig_embedded_latency()
    fig_deployment_tiers()
    print("Wrote required plots to results/plots/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
