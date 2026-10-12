#!/usr/bin/env python3
import os
import json
import hashlib

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
MODELS_DIR = os.path.join(BASE_DIR, "models")
MANIFEST_PATH = os.path.join(BASE_DIR, "models_manifest.json")
RAW_BASE_URL = "https://raw.githubusercontent.com/rezzylrn/samp-mobile-optimized/main/server-side/models"

def compute_sha256(filepath):
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def main():
    if not os.path.exists(MODELS_DIR):
        os.makedirs(MODELS_DIR)

    models_data = []

    # Format sample 0.3.DL art entry
    # AddSimpleModel(-1, 19379, -2000, "sample_cube.dff", "sample_cube.txd");
    files = sorted(os.listdir(MODELS_DIR))
    dff_files = [f for f in files if f.lower().endswith(".dff")]

    base_new_id = -2000
    for idx, dff in enumerate(dff_files):
        prefix = os.path.splitext(dff)[0]
        txd = f"{prefix}.txd"
        txd_path = os.path.join(MODELS_DIR, txd)
        dff_path = os.path.join(MODELS_DIR, dff)

        if not os.path.exists(txd_path):
            continue

        model_entry = {
            "id": base_new_id - idx,
            "base_id": 19379,
            "type": "simple",
            "virtual_world": -1,
            "dff": dff,
            "txd": txd,
            "dff_url": f"{RAW_BASE_URL}/{dff}",
            "txd_url": f"{RAW_BASE_URL}/{txd}",
            "dff_sha256": compute_sha256(dff_path),
            "txd_sha256": compute_sha256(txd_path),
            "dff_size": os.path.getsize(dff_path),
            "txd_size": os.path.getsize(txd_path)
        }
        models_data.append(model_entry)

    manifest = {
        "server_name": "Nusantara Roleplay (open.mp)",
        "endpoint": "127.0.0.1:7777",
        "sync_version": "1.0",
        "total_custom_models": len(models_data),
        "models": models_data
    }

    with open(MANIFEST_PATH, "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"Manifest written to {MANIFEST_PATH} with {len(models_data)} models.")

if __name__ == "__main__":
    main()
