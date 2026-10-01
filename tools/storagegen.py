#!/usr/bin/env python3
"""Build the wear-levelled FAT image for the `storage` partition.

Contents:
    frontend.zip          the WebKit Autoloader frontend (served by the AP)
    ps5_autoloader/       optional payloads the PS5 loads via unified-autoloader

The image is generated with IDF's own fatfsgen WL tooling so the firmware
can both mount it (VFS) and expose it over USB MSC. The same layout is what
a user sees when plugging the device into a PC.

Usage:
    storagegen.py <frontend.zip> [payload_dir] -o <out.img>
"""
import argparse
import os
import shutil
from pathlib import Path
import sys
import tempfile
from partition_layout import partition

IDF = os.environ.get("IDF_PATH")
if not IDF:
    print("IDF_PATH not set - run inside the ESP-IDF environment", file=sys.stderr)
    sys.exit(2)

FATFS = os.path.join(IDF, "components", "fatfs")
sys.path.insert(0, FATFS)
sys.path.insert(0, os.path.join(FATFS, "fatfs_utils"))

import wl_fatfsgen  # noqa: E402

# Keep in sync with partitions_4m.csv (storage partition size).
PART_SIZE = partition("storage")[1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("zip_path")
    ap.add_argument("payload_dir", nargs="?")
    ap.add_argument("-o", "--out", required=True)
    args = ap.parse_args()

    stage = tempfile.mkdtemp(prefix="wkal_storage_")
    try:
        shutil.copy(args.zip_path, os.path.join(stage, "frontend.zip"))
        # Payload drop-in dir for the PS5: the external payload dir replaces
        # the bundled template (readme only) when one is given.
        payloads = Path(args.payload_dir) if args.payload_dir else             Path(__file__).resolve().parent / "ps5_autoloader"
        if payloads.is_dir():
            os.makedirs(os.path.join(stage, "ps5_autoloader"), exist_ok=True)
            for name in sorted(os.listdir(payloads)):
                src = os.path.join(payloads, name)
                if os.path.isfile(src):
                    shutil.copy(src, os.path.join(stage, "ps5_autoloader", name))

        wl = wl_fatfsgen.WLFATFS(
            size=PART_SIZE, sector_size=512, fat_tables_cnt=2,
            sectors_per_cluster=1, explicit_fat_type=None,
            long_names_enabled=True, use_default_datetime=False,
            root_entry_count=512, wl_mode="perf")
        wl.plain_fatfs.generate(stage)
        wl.init_wl()
        wl.wl_write_filesystem(args.out)
    finally:
        shutil.rmtree(stage, ignore_errors=True)

    print(f"wrote {args.out}: {os.path.getsize(args.out)} bytes "
          f"({os.path.getsize(args.out)/1024:.0f} KiB)")


if __name__ == "__main__":
    main()
