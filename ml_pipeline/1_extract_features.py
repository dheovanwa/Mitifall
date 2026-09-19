import os
import pandas as pd
import numpy as np

def compute_features(df_window):
    """Computes basic statistical features for a given window."""
    features = {}
    for axis in ['X-Axis', 'Y-Axis', 'Z-Axis']:
        data = df_window[axis].values
        if len(data) == 0:
            features[f"{axis}_mean"] = 0
            features[f"{axis}_std"] = 0
            features[f"{axis}_max"] = 0
            features[f"{axis}_min"] = 0
        else:
            features[f"{axis}_mean"] = np.mean(data)
            features[f"{axis}_std"] = np.std(data)
            features[f"{axis}_max"] = np.max(data)
            features[f"{axis}_min"] = np.min(data)
    return features

def extract_from_file(filepath, label):
    df = pd.read_csv(filepath)
    if df.empty:
        return None
    
    # Separate sensors
    df_acc = df[df['SensorType'] == 0].reset_index(drop=True)
    df_gyro = df[df['SensorType'] == 1].reset_index(drop=True)
    
    if df_acc.empty or df_gyro.empty:
        return None
        
    # Find the peak acceleration to center our window
    # Compute magnitude
    magnitudes = np.sqrt(df_acc['X-Axis']**2 + df_acc['Y-Axis']**2 + df_acc['Z-Axis']**2)
    peak_idx = magnitudes.idxmax()
    
    # Take 50 samples before and 50 after the peak (approx 2 seconds total)
    start_idx = max(0, peak_idx - 50)
    end_idx = min(len(df_acc), peak_idx + 50)
    
    acc_window = df_acc.iloc[start_idx:end_idx]
    
    # For gyro, try to find the matching timestamp window, or just take same proportional window
    # We will match by TimeStamp if possible
    start_time = acc_window['TimeStamp'].min()
    end_time = acc_window['TimeStamp'].max()
    
    gyro_window = df_gyro[(df_gyro['TimeStamp'] >= start_time) & (df_gyro['TimeStamp'] <= end_time)]
    
    acc_feats = compute_features(acc_window)
    gyro_feats = compute_features(gyro_window)
    
    # Combine features
    combined = {}
    for k, v in acc_feats.items():
        combined[f"Acc_{k}"] = v
    for k, v in gyro_feats.items():
        combined[f"Gyro_{k}"] = v
        
    combined['Label'] = label
    return combined

def main():
    processed_dir = "../dataset/processed"
    fall_dir = os.path.join(processed_dir, "fall")
    normal_dir = os.path.join(processed_dir, "normal")
    
    all_features = []
    
    print("Extracting features from FALL data...")
    if os.path.exists(fall_dir):
        for f in os.listdir(fall_dir):
            if f.endswith(".csv"):
                feats = extract_from_file(os.path.join(fall_dir, f), 1)
                if feats:
                    all_features.append(feats)
                    
    print("Extracting features from NORMAL (ADL) data...")
    if os.path.exists(normal_dir):
        for f in os.listdir(normal_dir):
            if f.endswith(".csv"):
                feats = extract_from_file(os.path.join(normal_dir, f), 0)
                if feats:
                    all_features.append(feats)
                    
    features_df = pd.DataFrame(all_features)
    out_path = "features.csv"
    features_df.to_csv(out_path, index=False)
    print(f"Extracted {len(features_df)} feature vectors. Saved to {out_path}.")

if __name__ == "__main__":
    main()
