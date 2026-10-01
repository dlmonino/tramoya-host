"""Select memory geometry from the shared target manifest."""
import csv
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOARDS = json.loads((ROOT / "targets/boards.json").read_text())
BOARD = os.environ.get("WKAL_BOARD", "esp32s3-n4r2")
if BOARD not in BOARDS:
    raise ValueError("Unknown WKAL_BOARD: " + BOARD)
PROFILE = BOARDS[BOARD]
TABLE = ROOT / PROFILE["partition_table"]
FLASH_SIZE = PROFILE["flash_mb"] * 1024 * 1024


def number(value):
    value = value.strip()
    if value[-1:].upper() in ("K", "M"):
        return int(value[:-1], 0) * {"K": 1024, "M": 1024 * 1024}[value[-1].upper()]
    return int(value, 0)


def entries():
    offset = 0
    for row in csv.reader(TABLE.read_text().splitlines()):
        if not row or row[0].lstrip().startswith("#"):
            continue
        current = number(row[3]) if row[3].strip() else offset
        size = number(row[4])
        yield row[0].strip(), current, size
        offset = current + size


def partition(name):
    for label, offset, size in entries():
        if label == name:
            return offset, size
    raise ValueError(f"Partition {name!r} not found in {TABLE}")


def validate():
    previous = 0x9000
    names = set()
    for name, offset, size in entries():
        if name in names or size <= 0 or offset < previous or offset + size > FLASH_SIZE:
            raise ValueError("Invalid partition geometry: " + name)
        if offset % 4096 or size % 4096:
            raise ValueError("Partition must be 4 KiB aligned: " + name)
        names.add(name)
        previous = offset + size
    if previous != FLASH_SIZE or not {"factory", "storage", "coredump"} <= names:
        raise ValueError("Missing partition or incomplete layout")
