# ML Pipeline Review — Known Issues & TODO

Review of the fall-detection ML pipeline (UMAFall dataset → Random Forest → ESP32/C export).
Findings are ordered from most to least important. This is a working reference, not a spec —
update it as issues get fixed.

Reviewed files:
- `dataset/process_dataset.py`
- `ml_pipeline/1_extract_features.py`
- `ml_pipeline/2_train_and_export.py`
- `src/main.cpp` (firmware)

> Note: the review was done by reading the code and inspecting the processed data. The pipeline
> was not run end-to-end, so accuracy-related claims are inferred from the code, not measured.

---

## What is currently correct

- `process_dataset.py` filters `SensorID == 3` (wrist unit) and drops `SensorType == 2`
  (magnetometer). Verified: processed files contain only `SensorID == 3` and `SensorType` in {0, 1}.
- The acc/gyro split in `1_extract_features.py` (`SensorType == 0` = accelerometer,
  `SensorType == 1` = gyroscope) is consistent with what was kept.
- ADL vs Fall labeling by filename (`_ADL_` / `_Fall_`) matches the UMAFall naming convention.
- `2_train_and_export.py` does print a confusion matrix and classification report (good), and
  keeps the RF small (`n_estimators=20`, `max_depth=5`) so it fits on the ESP32.

---

## Critical issues (train/serve skew — model will not behave on-device as in testing)

### 1. Sampling-rate mismatch between dataset and device
- UMAFall wrist ("SELF") accelerometer samples are ~20–50 ms apart (nominally ~20 Hz, irregular).
  Measured timestamp deltas in a wrist file ranged roughly 16–76 ms.
- Firmware (`main.cpp`) samples at a fixed **50 Hz (20 ms)**.
- `1_extract_features.py` windows by **sample count** ("50 samples before and 50 after the peak"),
  not by time. So 100 samples ≈ 2–5 s on the dataset but exactly 2 s on the device.
- Statistical features (std, min, max) depend on window duration → the model trains on one physical
  window length and infers on another.
- **Fix:** define the window in TIME, and resample both dataset and device stream to a common rate
  (e.g. 50 Hz) before feature extraction.

### 2. Accelerometer / gyroscope unit mismatch
- UMAFall wrist acceleration is in **g** (rest values ≈ 1.0, e.g. 1.039).
- Adafruit MPU6050 `a.acceleration.*` is in **m/s²** (rest ≈ 9.81) → ~9.8× scale difference on every
  accelerometer feature.
- Same class of problem for gyro: dataset units vs Adafruit's rad/s.
- **Fix:** pick one unit system and convert consistently (e.g. divide device acc by 9.80665 to get g;
  reconcile gyro deg/s vs rad/s).

### 3. Firmware never computes features or runs inference
- `main.cpp` only prints CSV over serial. No windowing, no magnitude/peak detection, no feature
  computation, no call into `RandomForest.h`.
- The trained model is not wired up on-device yet.
- **Fix:** implement windowing + feature computation + inference in `main.cpp`, matching
  `1_extract_features.py` EXACTLY (feature names, order, acc vs gyro, units, std definition).

---

## Methodology issues

### 4. One window per file
- `extract_from_file` produces a single peak-centered window per recording (~746 samples total:
  208 fall + 538 ADL).
- Peak-centering is questionable for ADL files (walking/clapping have no impact peak); collapsing each
  ADL recording to its single highest-acceleration moment biases ADL windows to look fall-like.
- **Fix:** use sliding windows across each recording to use the data better and represent ADL honestly.

### 5. Subject leakage in the train/test split
- `train_test_split(..., stratify=y)` splits randomly across windows; windows from the same subject can
  appear in both train and test.
- Standard practice for fall detection is subject-wise / leave-one-subject-out evaluation, else accuracy
  is optimistic. Filenames contain `Subject_XX`.
- **Fix:** group by subject with `GroupShuffleSplit` / `GroupKFold` (or LOSO CV).

### 6. Accuracy is the headline metric on imbalanced data
- ~72% of samples are ADL, so accuracy is misleading.
- For fall detection, Fall-class **recall** (don't miss falls) and false-alarm rate matter most.
- **Fix:** report precision/recall/F1 for the Fall class explicitly; treat recall as primary.

### 7. Gyro window matching can silently degrade
- Gyro window is selected by timestamp range from the acc window; `compute_features` writes zeros for
  empty arrays, so a mismatch quietly injects all-zero gyro features instead of failing.
- **Fix:** log/warn when the gyro window is empty or much smaller than the acc window.

---

## Minor

- `np.std` uses population std (ddof=0) — make sure the device uses the same definition.
- No ordered feature-column list is emitted for the firmware to match against — consider dumping it.
- `dataset/process_dataset.py` has hardcoded absolute paths (`/home/kevin/...`) for
  `zip_path`, `out_fall_dir`, `out_normal_dir` — make them relative to the project.

---

## Suggested priority order

1. Define the window by TIME; resample dataset + device to a common rate (fixes #1).
2. Reconcile units between UMAFall and the MPU6050 (fixes #2).
3. Implement feature computation + inference in `main.cpp`, matching the Python exactly (fixes #3).
4. Switch to subject-wise evaluation; report Fall-class recall/precision (fixes #5, #6).
5. Use sliding windows instead of one window per file (fixes #4).
