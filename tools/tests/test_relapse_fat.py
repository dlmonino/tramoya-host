#!/usr/bin/env python3
"""Comprueba el ZIP candidato tras generar y extraer FAT real con ESP-IDF."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--zip', type=Path, required=True)
    parser.add_argument('--helper', type=Path, action='append', required=True)
    parser.add_argument('--target', choices=['esp32s3-n16r8'], required=True)
    args = parser.parse_args()
    if not os.environ.get('IDF_PATH'):
        parser.error('ESP-IDF debe estar activado; FAT no se omite.')
    os.environ['WKAL_TEST_ZIP'] = str(args.zip.resolve())
    os.environ['WKAL_BOARD'] = args.target
    # Se reutiliza el extractor ya probado; su main no se ejecuta al importar.
    import test_fat_image as fat
    root = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix='tramoya_relapse_fat_', dir='/private/tmp') as temp:
        folder = Path(temp)
        image = folder / 'storage.img'
        subprocess.run([sys.executable, str(root / 'tools/storagegen.py'),
                        str(args.zip.resolve()), '-o', str(image)], check=True)
        assert image.stat().st_size == 14483456, 'Tamaño de storage incorrecto'
        boot, table, data = fat.read_wl_image(str(image))
        extracted = folder / 'extraido'
        fat.extract_all(boot, table, data, str(extracted))
        files = {p.name.lower(): p for p in extracted.rglob('*') if p.is_file()}
        recovered = files['frontend.zip']
        assert recovered.read_bytes() == args.zip.read_bytes(), 'ZIP alterado por FAT'
        assert files['wifi.txt'].read_bytes() == (root / 'tools/usb_root/WIFI.TXT').read_bytes(), 'WIFI.TXT alterado'
        with zipfile.ZipFile(recovered) as archive:
            assert archive.testzip() is None
            count = 0
            for helper in args.helper:
                for name in archive.namelist():
                    result = subprocess.run([str(helper.resolve()), str(recovered), '/' + name],
                                            capture_output=True, check=True)
                    assert result.stdout == archive.read(name), name
                    count += 1
        print(f'PASS: FAT N16R8, ZIP y WIFI.TXT íntegros; {count} lecturas C tras extracción')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
