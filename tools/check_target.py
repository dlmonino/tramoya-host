"""Verify the selected partition geometry, sdkconfig and application budget."""
import json
import os
from pathlib import Path
from partition_layout import BOARD, PROFILE, partition, validate

ROOT = Path(__file__).resolve().parents[1]
BUILD = Path(os.environ["WKAL_BUILD_DIR"])
validate()
config = json.loads((BUILD / "config/sdkconfig.json").read_text())
assert config["ESPTOOLPY_FLASHSIZE"] == f'{PROFILE["flash_mb"]}MB'
assert config["PARTITION_TABLE_CUSTOM_FILENAME"] == PROFILE["partition_table"]
assert config["SPIRAM_MODE_" + ("QUAD" if PROFILE["psram_mode"] == "quad" else "OCT")]
size = (BUILD / "ps5_wkal_host.bin").stat().st_size
limit = partition("factory")[1]
assert size <= limit, f"App {size} exceeds factory budget {limit}"
print(f"PASS: {BOARD}, app {size}/{limit} bytes, storage {partition('storage')[1]} bytes")
