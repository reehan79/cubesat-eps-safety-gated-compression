# Reproducibility Artifacts for Safety-Gated Edge-AI Compression for Bandwidth-Limited CubeSat EPS Telemetry

**Authors:** Rehan Mahmood, Maheen Zulfiqar, Muhammad Nofal Bin Akhlaq

This repository provides the public reproducibility package for the IEEE Access paper on safety-gated telemetry compression for CubeSat EPS channels. It includes final result tables, generated paper figures, verification scripts, hardware profiling logs, and public-safe firmware profiling code.

## Repository Structure

- `results/tables/` - final CSV tables used in paper reporting
- `results/plots/` - paper-ready plots generated from final tables
- `results/repro/` - reproducibility manifest, checksums, and environment snapshot
- `scripts/` - verification and figure-generation scripts
- `firmware/` - profiling firmware for ESP32 and STM32 targets
- `hardware_logs/` - final profiling logs for Pi4/ESP32/STM32 targets
- `paper/` - paper-facing artifacts and helper material

## Quick Verification

```bash
pip install -r requirements.txt
python scripts/verify_results.py
python scripts/build_t5_paper_figures.py
```

## Hardware Artifacts Included

- Raspberry Pi 4 profile log
- ESP32 classical profile log
- ESP32 TFLite Micro encoder profile log
- STM32F429ZI profile log
- STM32F413ZH profile log

## Limitations

- No CCSDS compliance claim is made in this artifact package.
- No flight readiness claim is made; firmware is for profiling/reproducibility only.
- No measured energy-per-bit claim is made (energy is marked as not measured).
- ESP32 TFLite Micro results are encoder-only and do not claim full decoder deployment.

## Citation

See `CITATION.cff` for citation metadata.
