"""Package the first-flash image with all four required binary regions."""
import os
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from partition_layout import partition, FLASH_SIZE, BOARD, PROFILE, validate

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ.get("WKAL_BUILD_DIR", ROOT / ("build/" + BOARD)))


def main():
    validate()
    config = json.loads((BUILD / "flasher_args.json").read_text())
    regions = {int(offset, 0): BUILD / name for offset, name in config["flash_files"].items()}
    storage_offset, storage_size = partition("storage")
    regions[storage_offset] = BUILD / "storage.img"
    if regions[storage_offset].stat().st_size != storage_size:
        raise ValueError("Storage image size does not match partition")
    app_offset, app_size = partition("factory")
    if regions[app_offset].stat().st_size > app_size:
        raise ValueError("App exceeds factory partition")
    previous_end = 0
    for offset, path in sorted(regions.items()):
        end = offset + path.stat().st_size
        if offset < previous_end or end > FLASH_SIZE:
            raise ValueError(f"Overlapping or oversized flash region: {path}")
        previous_end = end
    output = BUILD / "ps5_wkal_host-full.bin"
    args = [sys.executable, "-m", "esptool", "--chip", "esp32s3", "merge_bin",
            "-o", str(output), "--flash_mode", config["flash_settings"]["flash_mode"],
            "--flash_freq", config["flash_settings"]["flash_freq"], "--flash_size", f"{PROFILE['flash_mb']}MB",
            "--fill-flash-size", f"{PROFILE['flash_mb']}MB"]
    for offset, path in sorted(regions.items()):
        args.extend([hex(offset), str(path)])
    subprocess.run(args, check=True)
    data = output.read_bytes()
    if len(data) != FLASH_SIZE:
        raise ValueError("Merged image does not match the selected Flash size")
    for offset, path in regions.items():
        original = path.read_bytes()
        if data[offset:offset + len(original)] != original:
            raise ValueError(f"Merged bytes mismatch: {path}")
    manifest = {"target": "esp32s3", "board": BOARD, "flash_size": FLASH_SIZE,
                "psram_mb": PROFILE["psram_mb"], "psram_mode": PROFILE["psram_mode"], "flash_offset": "0x0",
                "image": output.name, "sha256": hashlib.sha256(data).hexdigest(),
                "regions": [{"offset": hex(offset), "file": path.relative_to(BUILD).as_posix(),
                             "size": path.stat().st_size,
                             "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                            for offset, path in sorted(regions.items())]}
    frontend_source = BUILD / "frontend.zip-source.json"
    if frontend_source.is_file():
        manifest["frontend"] = json.loads(frontend_source.read_text())
    (BUILD / "firmware-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Verified {output.name}: {len(data)} bytes, SHA256 {manifest['sha256']}")


if __name__ == "__main__":
    main()
