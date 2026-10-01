#!/usr/bin/env python3
"""End-to-end FAT storage image test.

Builds a wear-levelled FAT image exactly like the firmware's 'fat' partition
(IDF's own wl_fatfsgen.py), then reads it back with IDF's fatfsparse
internals and verifies:
  1. frontend.zip and ps5_autoloader/* survive the WL round-trip byte-exact,
  2. the firmware's zip index + inflate path (wk_zip/wk_http, compiled into
     a helper) serves payload.elf from the round-tripped archive with a
     matching CRC.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

IDF = os.environ.get("IDF_PATH")
if not IDF:
    print("IDF_PATH not set - run inside the ESP-IDF environment", file=sys.stderr)
    sys.exit(2)

FATFS = os.path.join(IDF, "components", "fatfs")
sys.path.insert(0, FATFS)
sys.path.insert(0, os.path.join(FATFS, "fatfs_utils"))

import wl_fatfsgen  # noqa: E402
import fatfsparse  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from storagegen import PART_SIZE
ZIP = os.environ["WKAL_TEST_ZIP"]

failures = 0


def check(name, cond, detail=""):
    global failures
    if cond:
        print(f"ok   - {name}")
    else:
        print(f"FAIL - {name} {detail}")
        failures += 1


def build_wl_image(stage_dir, out_img):
    import argparse
    args = argparse.Namespace()
    args.input_directory = stage_dir
    args.output_file = out_img
    args.partition_size = PART_SIZE
    args.sector_size = 512
    args.sectors_per_cluster = 1
    args.root_entry_count = 512
    args.long_name_support = True
    args.use_default_datetime = True
    args.fat_type = None
    args.fat_count = 2
    args.wl_mode = "perf"
    wl = wl_fatfsgen.WLFATFS(size=args.partition_size,
                             sector_size=args.sector_size,
                             fat_tables_cnt=args.fat_count,
                             sectors_per_cluster=args.sectors_per_cluster,
                             explicit_fat_type=args.fat_type,
                             long_names_enabled=args.long_name_support,
                             use_default_datetime=args.use_default_datetime,
                             root_entry_count=args.root_entry_count,
                             wl_mode=args.wl_mode)
    wl.plain_fatfs.generate(args.input_directory)
    wl.init_wl()
    wl.wl_write_filesystem(args.output_file)


def read_wl_image(img_path):
    fs = fatfsparse.read_filesystem(img_path)
    fs = fatfsparse.remove_wear_levelling_if_exists(fs)
    boot = fatfsparse.BootSector()
    boot.parse_boot_sector(fs)
    fat = fatfsparse.FAT(boot.boot_sector_state, init_=False)
    return boot, fat, fs


def extract_all(boot, fat, fs, out_dir):
    """Use fatfsparse's own tree walker to dump the image to out_dir."""
    fatfsparse.args = type("A", (), {"long_name_support": True})()
    start = boot.boot_sector_state.root_directory_start
    sectors = boot.boot_sector_state.root_dir_sectors_cnt
    sector_size = boot.boot_sector_state.sector_size
    root = fs[start:start + sectors * sector_size]
    fatfsparse.traverse_folder_tree(
        root, out_dir, boot.boot_sector_state, fat, fs)
    return out_dir

def main():
    stage = tempfile.mkdtemp(prefix="wkal_fat_stage_")
    out_img = os.path.join(tempfile.gettempdir(), "wkal_test_fat.img")

    # Stage: frontend.zip + a ps5_autoloader payload dir.
    shutil.copy(ZIP, os.path.join(stage, "frontend.zip"))
    os.makedirs(os.path.join(stage, "ps5_autoloader"))
    with open(os.path.join(stage, "ps5_autoloader", "etaHEN.elf"), "wb") as f:
        f.write(b"\x7fELF" + os.urandom(4096))
    payload_src = open(os.path.join(stage, "ps5_autoloader", "etaHEN.elf"), "rb").read()

    build_wl_image(stage, out_img)
    img_size = os.path.getsize(out_img)
    check(f"WL image size == partition size ({PART_SIZE})",
          img_size == PART_SIZE, f"(got {img_size})")

    boot, fat, fs = read_wl_image(out_img)
    # traverse_folder_tree CREATES the volume-label dir itself, so the path
    # we hand it must not exist yet.
    extracted = os.path.join(tempfile.gettempdir(),
                             "wkal_fat_extract_%d" % os.getpid())
    extract_all(boot, fat, fs, extracted)
    def find_case_insensitive(base, rel):
        cur = base
        for part in rel.split("/"):
            match = None
            for entry in os.listdir(cur):
                if entry.upper() == part.upper():
                    match = entry
                    break
            if match is None:
                return None
            cur = os.path.join(cur, match)
        return cur

    zip_rt_path = find_case_insensitive(extracted, "frontend.zip")
    elf_rt_path = find_case_insensitive(extracted, "ps5_autoloader/etaHEN.elf")
    # Debug listing when missing
    if zip_rt_path is None or not os.path.exists(zip_rt_path):
        for root, dirs, files in os.walk(extracted):
            for f in files:
                print(f"  extracted: {os.path.join(root, f)}")
    zip_rt = open(zip_rt_path, "rb").read() if zip_rt_path is not None and os.path.exists(zip_rt_path) else None
    elf_rt = open(elf_rt_path, "rb").read() if elf_rt_path is not None and os.path.exists(elf_rt_path) else None

    zip_src = open(ZIP, "rb").read()
    check("frontend.zip round-trips byte-exact",
          zip_rt is not None and zip_rt == zip_src,
          f"(rt={len(zip_rt) if zip_rt else None} src={len(zip_src)})")
    check("payload ELF round-trips byte-exact",
          elf_rt is not None and elf_rt == payload_src)
    # Feed the round-tripped archive through the firmware's zip index + inflate.
    if zip_rt is not None:
        with tempfile.NamedTemporaryFile(suffix=".zip", delete=False) as tf:
            tf.write(zip_rt)
            rt_zip_path = tf.name
        helper = os.path.join(ROOT, "tools", "tests", "zip_verify_helper")
        rc = subprocess.run([helper, rt_zip_path], capture_output=True, text=True)
        check("wk_zip index + payload.elf inflate + CRC on round-tripped archive",
              rc.returncode == 0, f"\n{rc.stdout}\n{rc.stderr}")
        os.unlink(rt_zip_path)

    shutil.rmtree(stage, ignore_errors=True)
    shutil.rmtree(extracted, ignore_errors=True)
    os.unlink(out_img)
    print(f"\n{'ALL FAT IMAGE CHECKS PASSED' if failures == 0 else f'{failures} FAILURES'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
