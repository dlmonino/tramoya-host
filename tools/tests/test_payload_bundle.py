#!/usr/bin/env python3
"""Regresiones del perfil autónomo con binarios fijados y núcleo HTTP C real."""
import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import build_payload_frontend as builder
SOURCE = PAYLOADS = HELPER = None


class PayloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('node') or not HELPER.is_file():
            raise RuntimeError('Node y núcleo HTTP C son obligatorios; no omitir pruebas.')
        cls.tmp = tempfile.TemporaryDirectory(prefix='tramoya_payload_test_', dir='/private/tmp')
        cls.root = Path(cls.tmp.name)
        cls.zip = cls.root / 'frontend.zip'
        cls.record = builder.build(SOURCE, PAYLOADS, cls.zip)
        cls.lock = json.loads(builder.LOCK.read_text())

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_reproducible_inventory_and_hashes(self):
        other = self.root / 'repetido.zip'
        builder.build(SOURCE, PAYLOADS, other)
        self.assertEqual(other.read_bytes(), self.zip.read_bytes())
        with zipfile.ZipFile(self.zip) as z:
            self.assertIsNone(z.testzip())
            self.assertEqual(len(z.namelist()), 56)
            self.assertEqual(set(z.namelist()), set(self.record['files']))
            self.assertEqual(len(z.namelist()), len(set(z.namelist())))
            self.assertTrue(all(n == n.lower() for n in z.namelist()))
            for n in z.namelist():
                self.assertEqual(builder.minimo.sha(z.read(n)), self.record['files'][n])
            for n, record in self.lock['payloads'].items():
                self.assertEqual(builder.minimo.sha(z.read('relapse/payloads/' + n)), record['sha256'])
                self.assertEqual(z.read('relapse/payloads/' + n), (PAYLOADS / n).read_bytes())
            self.assertFalse(any('etahen' in n.lower() for n in z.namelist()))
        self.assertLess(self.zip.stat().st_size, self.lock['max_zip_bytes'])
        self.assertEqual(self.record['physical_validation'], 'PENDIENTE')

    def test_only_documented_upstream_changes(self):
        lock = json.loads(builder.minimo.LOCK.read_text())
        with zipfile.ZipFile(self.zip) as z:
            for n in lock['files']:
                if n in lock['excluded'] or n in ('src/main.js', 'src/kexp.js'):
                    continue
                self.assertEqual(z.read('relapse/' + n.lower()), (SOURCE / n).read_bytes(), n)
            kexp = z.read('relapse/src/kexp.js').decode()
            self.assertNotIn('export async function loadOptionalPayloads(', kexp)
            start = kexp.index('export async function cargarPayloadTramoya(')
            end = kexp.index('\nfunction patchShellcode(', start)
            original = (SOURCE / 'src/kexp.js').read_text()
            oldstart = original.index('export async function loadOptionalPayloads(')
            oldend = original.index('\nfunction patchShellcode(', oldstart)
            self.assertEqual(kexp[:start].replace('// AF_INET, puerto 9021', '// AF_INET, port 9021'), original[:oldstart])
            self.assertEqual(kexp[end:], original[oldend:])
            main = z.read('relapse/src/main.js').decode()
            self.assertEqual(main, (SOURCE / 'src/main.js').read_text().replace(builder.minimo.OLD_HOOK, builder.HOOK))

    def test_tampered_payload_and_symlink_rejected(self):
        copied = self.root / 'binarios'
        copied.mkdir()
        for n in self.lock['payloads']:
            shutil.copyfile(PAYLOADS / n, copied / n)
        p = copied / 'kstuff.elf'
        data = p.read_bytes(); p.write_bytes(data[:-1] + bytes([data[-1] ^ 1]))
        with self.assertRaisesRegex(ValueError, 'release fijada'):
            builder.build(SOURCE, copied, self.root / 'alterado.zip')
        self.assertFalse((self.root / 'alterado.zip').exists())
        p.unlink(); p.symlink_to(PAYLOADS / 'kstuff.elf')
        with self.assertRaisesRegex(ValueError, 'simbólico'):
            builder.build(SOURCE, copied, self.root / 'enlace.zip')

    def test_no_overwrite_and_size_budget(self):
        with self.assertRaisesRegex(ValueError, 'sobrescribir'):
            builder.build(SOURCE, PAYLOADS, self.zip)
        output = self.root / 'sidecar.zip'
        Path(str(output) + '-source.json').symlink_to(self.root / 'ausente')
        with self.assertRaisesRegex(ValueError, 'sobrescribir'):
            builder.build(SOURCE, PAYLOADS, output)
        lock = dict(self.lock, max_zip_bytes=100)
        small = self.root / 'limite.json'; small.write_text(json.dumps(lock))
        with patch.object(builder, 'LOCK', small):
            with self.assertRaisesRegex(ValueError, 'presupuesto'):
                builder.build(SOURCE, PAYLOADS, self.root / 'exceso.zip')
        self.assertFalse((self.root / 'exceso.zip').exists())

    def test_manual_controls_and_js_syntax(self):
        subprocess.run(['node', str(ROOT / 'tools/tests/test_payload_controls.js')], check=True)
        with zipfile.ZipFile(self.zip) as z:
            for n in z.namelist():
                if n.endswith('.js'):
                    subprocess.run(['node', '--input-type=module', '--check'], input=z.read(n), capture_output=True, check=True)

    def test_every_resource_real_http_and_guide_routes(self):
        with zipfile.ZipFile(self.zip) as z:
            for n in z.namelist():
                for prefix in ('/', '/app/', '/document/es/ps5/', '/document/en/ps5/'):
                    r = subprocess.run([str(HELPER), str(self.zip), prefix + n + '?v=1'], capture_output=True, check=True)
                    self.assertEqual(r.stdout, z.read(n), prefix + n)
            r = subprocess.run([str(HELPER), str(self.zip), '/relapse/payloads/etahen.elf'], capture_output=True)
            self.assertEqual(r.returncode, 1)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--payload-dir', type=Path, required=True)
    p.add_argument('--helper', type=Path, required=True)
    a, rest = p.parse_known_args()
    SOURCE, PAYLOADS, HELPER = a.source.resolve(), a.payload_dir.resolve(), a.helper.resolve()
    unittest.main(argv=[__file__, *rest])
