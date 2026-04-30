# Reproduce and Verify

## 1) Verify Final Result Tables

Run:

```bash
python scripts/verify_results.py
```

This checks:
- required tables exist
- safety dashboard counts match expected final values
- embedded profile has required target rows
- no row claims measured energy

## 2) Regenerate Required Figures

Run:

```bash
python scripts/build_t5_paper_figures.py
```

Generated files in `results/plots/`:
- `fig_safety_outcomes.pdf`
- `fig_cr_latency_safety.pdf`
- `fig_embedded_latency.pdf`
- `fig_deployment_tiers.pdf`

## 3) Inspect Hardware Logs

Logs are in `hardware_logs/` and include final records for Pi4, ESP32, ESP32 TFLM encoder, STM32F429ZI, and STM32F413ZH.

## 4) What Cannot Be Fully Reproduced Without Hardware

- On-device timing behavior for each target board under identical bench conditions
- MCU-specific runtime conditions outside these provided final logs
- Any deployment conclusion requiring additional power instrumentation

## 5) Interpreting the Safety Dashboard

`results/tables/t5_safety_dashboard.csv` summarizes counts by `result_type` and `safe_pass`.
Final expected totals verified by script:
- `lossless_baseline` + `safe_pass=True` -> 32
- `learned_compression` + `safe_pass=False` -> 149
- `learned_compression_all_numeric_stress_test` + `safe_pass=False` -> 143
