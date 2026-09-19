import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import accuracy_score, classification_report, confusion_matrix
from micromlgen import port
import os

def main():
    print("Loading extracted features...")
    df = pd.read_csv("features.csv")
    
    if df.empty:
        print("Error: features.csv is empty.")
        return
        
    print(f"Dataset shape: {df.shape}")
    
    # Split into X (features) and y (labels)
    X = df.drop('Label', axis=1)
    y = df['Label']
    
    print(f"Features used: {list(X.columns)}")
    
    # Train-test split (80% train, 20% test)
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    
    # Initialize Random Forest Classifier
    # We keep the trees small (max_depth=5, n_estimators=20) to ensure the exported C++ file is very small 
    # and fits easily in the ESP32 RAM/Flash.
    clf = RandomForestClassifier(n_estimators=20, max_depth=5, random_state=42)
    
    print("Training Random Forest Classifier...")
    clf.fit(X_train, y_train)
    
    # Evaluate
    y_pred = clf.predict(X_test)
    acc = accuracy_score(y_test, y_pred)
    
    print("\n--- Evaluation Results ---")
    print(f"Accuracy on Test Set: {acc * 100:.2f}%")
    print("\nConfusion Matrix:")
    print(confusion_matrix(y_test, y_pred))
    print("\nClassification Report:")
    print(classification_report(y_test, y_pred, target_names=["Normal (ADL)", "Fall"]))
    
    # Export to C++ using micromlgen
    print("\nExporting model to C++ header file (RandomForest.h)...")
    c_code = port(clf)
    
    # Save the C++ code to a file
    out_file = "../src/RandomForest.h"
    with open(out_file, "w") as f:
        f.write(c_code)
        
    print(f"Export successful! C++ header saved to: {out_file}")
    print("You can now `#include \"RandomForest.h\"` in your main.cpp to run inference on the ESP32.")

if __name__ == "__main__":
    main()
