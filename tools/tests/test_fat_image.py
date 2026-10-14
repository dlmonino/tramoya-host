#!/usr/bin/env python3
"""Prueba completa de la imagen FAT con nivelación de desgaste.

Genera la imagen mediante storagegen.py y la extrae con las herramientas IDF.
Comprueba frontend.zip, WIFI.TXT y los payloads byte a byte; después sirve el
ZIP extraído mediante el código C del firmware y verifica el CRC del payload.
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


def read_wl_image(img_path):
    fs = fatfsparse.read_filesystem(img_path)
    fs = fatfsparse.remove_wear_levelling_if_exists(fs)
    boot = fatfsparse.BootSector()
    boot.parse_boot_sector(fs)
    fat = fatfsparse.FAT(boot.boot_sector_state, init_=False)
    return boot, fat, fs


def extract_all(boot, fat, fs, out_dir):
    """Extraer la imagen en out_dir con el recorrido de directorios de fatfsparse."""
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

    # Preparar frontend.zip y el directorio ps5_autoloader de prueba.
    shutil.copy(ZIP, os.path.join(stage, "frontend.zip"))
    os.makedirs(os.path.join(stage, "ps5_autoloader"))
    with open(os.path.join(stage, "ps5_autoloader", "etaHEN.elf"), "wb") as f:
        f.write(b"\x7fELF" + os.urandom(4096))
    payload_src = open(os.path.join(stage, "ps5_autoloader", "etaHEN.elf"), "rb").read()

    # Usar el generador real de imágenes, incluida su plantilla WIFI.TXT.
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "storagegen.py"),
                    ZIP, os.path.join(stage, "ps5_autoloader"), "-o", out_img], check=True)
    img_size = os.path.getsize(out_img)
    check(f"WL image size == partition size ({PART_SIZE})",
          img_size == PART_SIZE, f"(got {img_size})")

    boot, fat, fs = read_wl_image(out_img)
    # traverse_folder_tree crea el directorio de la etiqueta del volumen;
    # la ruta de destino no debe existir todavía.
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
    wifi_rt_path = find_case_insensitive(extracted, "WIFI.TXT")
    wifi_template = os.path.join(ROOT, "tools", "usb_root", "WIFI.TXT")
    with open(wifi_template, "rb") as f:
        wifi_src = f.read()
    wifi_rt = None
    if wifi_rt_path is not None:
        with open(wifi_rt_path, "rb") as f:
            wifi_rt = f.read()
    check("WIFI.TXT template round-trips byte-exact", wifi_rt == wifi_src)
    check("default template contains the requested network credentials",
          b"\nssid=tramoya_host\npassword=tramoya_host\n" in wifi_src)
    # Mostrar los archivos extraídos si falta el ZIP.
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
    # Pasar el ZIP recuperado por el índice y la descompresión del firmware.
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
