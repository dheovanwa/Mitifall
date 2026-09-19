import zipfile
import io
import pandas as pd
import os

zip_path = "/home/kevin/Downloads/4214283.zip"
out_fall_dir = "/home/kevin/Documents/PlatformIO/Projects/Mitifall/dataset/processed/fall"
out_normal_dir = "/home/kevin/Documents/PlatformIO/Projects/Mitifall/dataset/processed/normal"

os.makedirs(out_fall_dir, exist_ok=True)
os.makedirs(out_normal_dir, exist_ok=True)

processed_fall = 0
processed_normal = 0
total_rows = 0

print("Starting processing...")

with zipfile.ZipFile(zip_path, "r") as z_outer:
    # Open the inner zip file
    with z_outer.open("UMAFall_Dataset_corrected_version.zip") as f_inner:
        inner_zip_bytes = io.BytesIO(f_inner.read())
        with zipfile.ZipFile(inner_zip_bytes, "r") as z_inner:
            file_list = z_inner.namelist()
            csv_files = [f for f in file_list if f.endswith(".csv")]
            
            for f_name in csv_files:
                # Classify based on filename
                # Files with 'ADL' are Activities of Daily Living (Normal)
                # Files with 'Fall' are Falls
                if "_ADL_" in f_name:
                    is_fall = False
                    out_dir = out_normal_dir
                elif "_Fall_" in f_name:
                    is_fall = True
                    out_dir = out_fall_dir
                else:
                    # Skip if it doesn't match expected naming
                    continue
                
                with z_inner.open(f_name) as f:
                    # Read the CSV. The delimiter is ';'
                    # We skip rows starting with '%' which are comments/metadata
                    # We will define the column names manually since the header is commented out
                    col_names = ["TimeStamp", "SampleNo", "X-Axis", "Y-Axis", "Z-Axis", "SensorType", "SensorID"]
                    
                    try:
                        df = pd.read_csv(f, sep=";", comment="%", header=None, names=col_names, engine="python")
                    except Exception as e:
                        print(f"Failed to read {f_name}: {e}")
                        continue
                    
                    if df.empty:
                        continue
                        
                    # Clean up: sometimes there's a trailing empty column due to a trailing semicolon
                    # The names array has 7 elements, if there's an 8th it gets dropped or causes issues.
                    # pandas handles this by filling NaN. Let's just keep the 7 columns we defined.
                    df = df[col_names]
                    
                    # 1. Filter for Right Wrist (SensorID == 3)
                    df = df[df["SensorID"] == 3]
                    
                    # 2. Drop Magnetometer (SensorType == 2)
                    df = df[df["SensorType"] != 2]
                    
                    if not df.empty:
                        # Save the cleaned data
                        out_filename = os.path.basename(f_name)
                        out_filepath = os.path.join(out_dir, out_filename)
                        
                        df.to_csv(out_filepath, index=False)
                        
                        if is_fall:
                            processed_fall += 1
                        else:
                            processed_normal += 1
                        total_rows += len(df)

print(f"\nProcessing Complete!")
print(f"Total Fall files processed: {processed_fall}")
print(f"Total Normal files processed: {processed_normal}")
print(f"Total data rows across all files: {total_rows}")
