"""Verify the selected partition geometry, sdkconfig and application budget."""
import json
import os
from pathlib import Path
from partition_layout import BOARD, PROFILE, partition, validate

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ["WKAL_BUILD_DIR"])
validate()
config = json.loads((BUILD / "config/sdkconfig.json").read_text())
if config["ESPTOOLPY_FLASHSIZE"] != f'{PROFILE["flash_mb"]}MB':
    raise ValueError("Flash size does not match selected target")
if config["PARTITION_TABLE_CUSTOM_FILENAME"] != PROFILE["partition_table"]:
    raise ValueError("Partition table does not match selected target")
if not config["SPIRAM_MODE_" + ("QUAD" if PROFILE["psram_mode"] == "quad" else "OCT")]:
    raise ValueError("PSRAM mode does not match selected target")
size = (BUILD / "ps5_wkal_host.bin").stat().st_size
limit = partition("factory")[1]
if size > limit:
    raise ValueError(f"App {size} exceeds factory budget {limit}")
print(f"PASS: {BOARD}, app {size}/{limit} bytes, storage {partition('storage')[1]} bytes")
