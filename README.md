# Reproducibility Artifacts for Safety-Gated Edge-AI Compression for Bandwidth-Limited CubeSat EPS Telemetry

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.19929577.svg)](https://doi.org/10.5281/zenodo.19929577)

**Manuscript title:** Safety-Gated Edge-AI Compression for Bandwidth-Limited CubeSat EPS Telemetry

**Authors:** Rehan Mahmood, Maheen Zulfiqar, Muhammad Nofal Bin Akhlaq

This repository provides the public reproducibility package for a safety-gated Edge-AI compression benchmark for CubeSat Electrical Power System (EPS) telemetry using BIRDS on-orbit data. Instead of ranking methods only by compression ratio, each lossy model is screened using worst-case per-channel reconstruction-error gates derived from training data. The package includes final result tables, generated paper figures, verification scripts, hardware profiling logs, and public-safe firmware profiling code for Raspberry Pi 4, ESP32, and STM32 targets.

This repository supports the manuscript submitted to IEEE Transactions on Aerospace and Electronic Systems.

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

If using this repository, please cite the archived Zenodo release:

Mahmood, R., Zulfiqar, M., & Bin Akhlaq, M. N. (2026). Reproducibility Artifacts for Safety-Gated Edge-AI Compression for Bandwidth-Limited CubeSat EPS Telemetry (v1.0.0). Zenodo. https://doi.org/10.5281/zenodo.19929577

```bibtex
@software{mahmood2026cubesat_eps_safety_gated_compression_artifacts,
  author  = {Mahmood, Rehan and Zulfiqar, Maheen and Bin Akhlaq, Muhammad Nofal},
  title   = {Reproducibility Artifacts for Safety-Gated Edge-AI Compression for Bandwidth-Limited CubeSat EPS Telemetry},
  year    = {2026},
  version = {v1.0.0},
  doi     = {10.5281/zenodo.19929577},
  url     = {https://github.com/reehan79/cubesat-eps-safety-gated-compression}
}
```
