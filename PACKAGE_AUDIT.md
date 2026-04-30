# Package Audit

## Included Files and Groups

- Top-level docs and metadata:
  - `README.md`
  - `LICENSE`
  - `CITATION.cff`
  - `requirements.txt`
  - `environment.yml`
  - `REPRODUCE.md`
  - `DATASET_SOURCE.md`
- Results tables (`results/tables/`):
  - `t5_safety_dashboard.csv`
  - `t5_main_results.csv`
  - `t5_lossless_baselines.csv`
  - `t5_method_ranking.csv`
  - `t5_embedded_profile.csv`
  - `t5_safety_thresholds.csv`
  - `birds_eps_dataset_summary.csv`
  - `birds_eps_channel_list.csv`
  - `t5_stress_test_results.csv` (present and included)
- Results plots (`results/plots/`):
  - `fig_safety_outcomes.pdf`
  - `fig_cr_latency_safety.pdf`
  - `fig_embedded_latency.pdf`
  - `fig_deployment_tiers.pdf`
- Repro metadata (`results/repro/`):
  - `t5_repro_manifest.json`
  - `t5_checksums.txt`
  - `t5_environment.txt`
- Scripts (`scripts/`):
  - `build_t5_paper_figures.py`
  - `verify_results.py`
  - `reproduce_tables.py`
- Firmware (`firmware/`):
  - `esp32_classical_profile/`
  - `esp32_tflm_encoder_profile/`
  - `stm32f429zi_profile/`
  - `stm32f413zh_profile/`
  - `README.md`
- Hardware logs (`hardware_logs/`):
  - `t5_pi4_profile.csv`
  - `esp32_profile_final.csv`
  - `esp32_tflm_profile_final.csv`
  - `stm32f429zi_profile_final.csv`
  - `stm32f413zh_profile_final.csv`

## Excluded Files and Categories

Excluded by policy and copy rules:
- virtual environments (`.venv`)
- Python cache (`__pycache__`)
- MCU debug/build directories (`Debug/`, `build/`, `.pio/`)
- binary artifacts not needed for paper reproducibility (`*.bin`, `*.elf`, `*.map`)
- Cursor/editor cache artifacts
- smoke-test and non-final logs
- private notes and temporary checkpoints not required by final paper
- absolute host path junk and sensitive files
- nested vendored git metadata (for example `lib/TensorFlowLite_ESP32/.git`)

## Plot Filename Mapping Note

The exact required plot filenames were not present in source folders. They were generated from final tables using `scripts/build_t5_paper_figures.py` and saved under required publication names in `results/plots/`.

## Verification Result

Command run:

```bash
python scripts/verify_results.py
```

Result: PASS
- All required CSV files exist
- Safety dashboard counts match (32, 149, 143)
- Embedded profile includes Pi4, ESP32, ESP32 TFLM, STM32F429ZI, STM32F413ZH evidence
- No rows claim measured energy

## Tree Command Status

Requested command `tree -L 3` could not be executed because `tree` is not installed in this environment.
