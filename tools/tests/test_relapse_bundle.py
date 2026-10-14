#!/usr/bin/env python3
"""Verificación local del empaquetado; requiere las fuentes fijadas y Node."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('build_relapse', ROOT / 'tools/build_relapse_frontend.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
SOURCE = None
HELPER = None


class BundleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not SOURCE or not SOURCE.is_dir() or not shutil.which('node'):
            raise RuntimeError('Son obligatorios --source y Node; no se omiten pruebas.')
        cls.temp = tempfile.TemporaryDirectory(prefix='tramoya_relapse_', dir='/private/tmp')
        cls.root = Path(cls.temp.name)
        cls.zip = cls.root / 'frontend.zip'
        cls.record = builder.build(SOURCE, cls.zip)
        cls.lock = json.loads(builder.LOCK.read_text())

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_reproducible_and_crc(self):
        other = self.root / 'repetido.zip'
        builder.build(SOURCE, other)
        self.assertEqual(self.zip.read_bytes(), other.read_bytes())
        with zipfile.ZipFile(self.zip) as z:
            self.assertIsNone(z.testzip())
            self.assertEqual(len(z.namelist()), len(set(z.namelist())))
            self.assertTrue(all(i.compress_type == zipfile.ZIP_DEFLATED for i in z.infolist()))

    def test_exact_inventory_and_provenance(self):
        expected = {'relapse/' + n.lower() for n in self.lock['files'] if n not in self.lock['excluded']}
        expected |= {'index.html', 'inicio.js'}
        with zipfile.ZipFile(self.zip) as z:
            self.assertEqual(set(z.namelist()), expected)
            self.assertTrue(all(n == n.lower() for n in z.namelist()))
            for name in expected:
                self.assertEqual(hashlib.sha256(z.read(name)).hexdigest(), self.record['files'][name])
        self.assertEqual(self.record['sha256'], builder.sha(self.zip.read_bytes()))
        self.assertLess(self.zip.stat().st_size, builder.MAX_ZIP_BYTES)
        self.assertEqual(self.record['physical_validation'], 'PENDIENTE')

    def test_upstream_unchanged_except_documented_patch(self):
        with zipfile.ZipFile(self.zip) as z:
            for name in self.lock['files']:
                if name in self.lock['excluded']:
                    continue
                original = (SOURCE / name).read_bytes()
                if name == 'src/main.js':
                    original = original.decode().replace(builder.OLD_HOOK, builder.NEW_HOOK).encode()
                self.assertEqual(z.read('relapse/' + name.lower()), original, name)

    def test_optional_payloads_disabled(self):
        with zipfile.ZipFile(self.zip) as z:
            main = z.read('relapse/src/main.js').decode()
            self.assertEqual(main.count('watchR2('), 1)  # Solo queda la definición sin llamadas.
            self.assertNotIn('await loadOptionalPayloads', main)
            payloads = {n for n in z.namelist() if '/payloads/' in n}
            self.assertEqual(payloads, {'relapse/payloads/elfldr-ps5-1360.elf',
                                        'relapse/payloads/kexp_2026_05_25.bin'})
            self.assertIn('relapse/license', z.namelist())

    def test_source_corruption_rejected_before_output(self):
        copied = self.root / 'alterado'
        copied.mkdir()
        for name in self.lock['files']:
            target = copied / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(SOURCE / name, target)
        (copied / 'offsets/13.60.js').write_text('corrupto')
        output = self.root / 'rechazado.zip'
        with self.assertRaisesRegex(ValueError, 'Fuente distinta'):
            builder.build(copied, output)
        self.assertFalse(output.exists())

    def test_symlink_rejected(self):
        copied = self.root / 'enlace'
        copied.mkdir()
        name = next(iter(self.lock['files']))
        target = copied / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.symlink_to(SOURCE / name)
        with self.assertRaisesRegex(ValueError, 'simbólico'):
            builder.build(copied, self.root / 'enlace.zip')

    def test_no_overwrite_or_source_output(self):
        with self.assertRaisesRegex(ValueError, 'ya existe'):
            builder.build(SOURCE, self.zip)
        with self.assertRaisesRegex(ValueError, 'fuera de las fuentes'):
            builder.build(SOURCE, SOURCE / 'prohibido.zip')
        output = self.root / 'sidecar-enlace.zip'
        destination = self.root / 'no-crear.json'
        Path(str(output) + '-source.json').symlink_to(destination)
        with self.assertRaisesRegex(ValueError, 'ya existe'):
            builder.build(SOURCE, output)
        self.assertFalse(output.exists())
        self.assertFalse(destination.exists())
        output = self.root / 'sidecar-concurrente.zip'
        provenance = Path(str(output) + '-source.json')
        original_zip = zipfile.ZipFile
        def concurrent(*args, **kwargs):
            provenance.write_bytes(b'registro previo')
            return original_zip(*args, **kwargs)
        with patch.object(builder.zipfile, 'ZipFile', side_effect=concurrent):
            with self.assertRaises(FileExistsError):
                builder.build(SOURCE, output)
        self.assertEqual(provenance.read_bytes(), b'registro previo')

    def test_manual_gate(self):
        html = (ROOT / 'tools/relapse_entry/index.html').read_text()
        self.assertIn('<iframe id="relapse" src="about:blank"', html)
        self.assertLess(html.index("document.getElementById('relapse').src = 'about:blank'"),
                        html.index('<script src="/inicio.js"'))
        subprocess.run(['node', str(ROOT / 'tools/tests/test_relapse_gate.js'),
                        str(ROOT / 'tools/relapse_entry/inicio.js')], check=True)

    def test_javascript_syntax_without_execution(self):
        with zipfile.ZipFile(self.zip) as z:
            names = [n for n in z.namelist() if n.endswith('.js')]
            for name in names:
                subprocess.run(['node', '--input-type=module', '--check'],
                               input=z.read(name), capture_output=True, check=True)
        self.assertGreater(len(names), 30)

    def test_real_http_core_every_file_and_guide_routes(self):
        if not HELPER or not HELPER.is_file():
            self.fail('--helper es obligatorio: no se omite el núcleo C real.')
        with zipfile.ZipFile(self.zip) as z:
            for name in z.namelist():
                for prefix in ('/', '/app/', '/document/en/ps5/', '/document/es/ps5/'):
                    r = subprocess.run([str(HELPER), str(self.zip), prefix + name + '?v=123'],
                                       capture_output=True, check=True)
                    self.assertEqual(r.stdout, z.read(name), prefix + name)
                    if name.endswith('.js'):
                        self.assertIn(b'javascript', r.stderr)
            self.assertEqual(subprocess.run([str(HELPER), str(self.zip), '/'],
                             capture_output=True, check=True).stdout, z.read('index.html'))
            r = subprocess.run([str(HELPER), str(self.zip), '/relapse/payloads/kstuff.elf'],
                               capture_output=True)
            self.assertEqual(r.returncode, 1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--helper', type=Path, required=True)
    args, rest = parser.parse_known_args()
    SOURCE = args.source.resolve()
    HELPER = args.helper.resolve()
    unittest.main(argv=[__file__, *rest])
