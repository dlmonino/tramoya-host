#!/usr/bin/env python3
"""Generar la imagen FAT con nivelación de desgaste de la partición storage.

Contenido: frontend.zip, WIFI.TXT y el directorio opcional ps5_autoloader.
Se usa el generador FAT de ESP-IDF para montar la misma imagen en la
aplicación y exponerla por USB MSC.

Uso: storagegen.py <frontend.zip> [directorio_payloads] -o <salida.img>
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

# Tamaño tomado de la partición storage del perfil de placa seleccionado.
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
        shutil.copy(Path(__file__).resolve().parent / "usb_root" / "WIFI.TXT",
                    os.path.join(stage, "WIFI.TXT"))
        # Si se indica un directorio externo de payloads, sustituye la plantilla
        # incluida, que únicamente contiene instrucciones.
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
