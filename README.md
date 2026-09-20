# Mitifall

A fall-detection IoT project. It has two parts:

- **Firmware** — an Arduino/ESP32 app (Seeed XIAO ESP32-C3 + MPU6050) built with [PlatformIO](https://platformio.org/).
- **ML pipeline** — Python scripts that process the UMAFall dataset, extract features, train a Random Forest classifier, and export it to C for the microcontroller.

## Prerequisites

- Python 3.11 (recommended for compatibility with the ML dependencies)
- [PlatformIO](https://platformio.org/install) (only needed for the firmware)

## Python setup

The Python dependencies live in `ml_pipeline/requirements.txt`. After cloning the repo, create a virtual environment and install them:

```bash
# From the repo root
python3.11 -m venv .venv

# Activate the environment
source .venv/bin/activate.fish   # fish
# source .venv/bin/activate      # bash / zsh
# .venv\Scripts\activate         # Windows PowerShell

# Install dependencies
pip install -r ml_pipeline/requirements.txt
```

The `.venv/` directory is gitignored, so each contributor creates their own locally.

### Running the ML pipeline

With the environment activated:

```bash
# 1. Process the raw UMAFall dataset (adjust the paths in the script first)
python dataset/process_dataset.py

# 2. Extract features from the processed CSVs
python ml_pipeline/1_extract_features.py

# 3. Train the model and export it to C
python ml_pipeline/2_train_and_export.py
```

> Note: `dataset/process_dataset.py` currently has hardcoded absolute paths. Update `zip_path`, `out_fall_dir`, and `out_normal_dir` to match your machine before running it.

## Firmware setup

The firmware is a PlatformIO project targeting the Seeed XIAO ESP32-C3. Library dependencies are declared in `platformio.ini` and installed automatically by PlatformIO on the first build:

```bash
# Build
pio run

# Upload to the board
pio run --target upload

# Serial monitor
pio device monitor
```
