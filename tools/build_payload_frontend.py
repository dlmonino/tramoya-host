#!/usr/bin/env python3
"""Prepara kstuff y ShadowMount fijados sobre la integración mínima conservada."""
import argparse
import json
from pathlib import Path
import struct
import tempfile
import zipfile

import build_relapse_frontend as minimo

TOOLS = Path(__file__).resolve().parent
LOCK = TOOLS / 'payloads-13.60.lock.json'
HOOK = '''    const { instalarControles } = await import("./controles.js");
    const { cargarPayloadTramoya } = await import("./kexp.js");
    instalarControles(document, nombre => cargarPayloadTramoya(nombre, p, chain),
      (mensaje, tipo) => log(mensaje, tipo || "info"));'''


def build(source, payload_dir, output):
    source, payload_dir, output = map(lambda p: Path(p).absolute(), (source, payload_dir, output))
    provenance = Path(str(output) + '-source.json')
    for folder in (source, payload_dir):
        if output.resolve().is_relative_to(folder.resolve()):
            raise ValueError('El resultado debe quedar fuera de las fuentes y binarios.')
    if any(p.exists() or p.is_symlink() for p in (output, provenance)):
        raise ValueError('El destino ya existe; no sobrescribir.')
    lock = json.loads(LOCK.read_text())
    payloads = {}
    for name, record in lock['payloads'].items():
        p = payload_dir / name
        if any(part.is_symlink() for part in (p, *p.parents)):
            raise ValueError('Enlace simbólico en payload: ' + name)
        data = p.read_bytes()
        if len(data) != record['bytes'] or minimo.sha(data) != record['sha256']:
            raise ValueError('Payload distinto de la release fijada: ' + name)
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', data, 18)[0] != 62:
            raise ValueError('Se requiere ELF64 little-endian x86-64: ' + name)
        payloads['relapse/payloads/' + name] = data
    with tempfile.TemporaryDirectory(prefix='tramoya_payloads_') as temporary:
        base = Path(temporary) / 'minimo.zip'
        base_record = minimo.build(source, base)
        with zipfile.ZipFile(base) as archive:
            files = {n: archive.read(n) for n in archive.namelist()}
    main = files['relapse/src/main.js'].decode()
    if main.count(minimo.NEW_HOOK) != 1:
        raise ValueError('Adaptación mínima inesperada.')
    files['relapse/src/main.js'] = main.replace(minimo.NEW_HOOK, HOOK).encode()
    kexp = files['relapse/src/kexp.js'].decode()
    start = kexp.index('export async function loadOptionalPayloads(')
    end = kexp.index('\nfunction patchShellcode(', start)
    # Sustituir solo la carga conjunta; conservar las funciones del kernel y transporte.
    kexp = kexp[:start] + (TOOLS / 'payload_entry/cargar.js').read_text() + '\n' + kexp[end:]
    kexp = kexp.replace('// AF_INET, port 9021', '// AF_INET, puerto 9021')
    files['relapse/src/kexp.js'] = kexp.encode()
    files['relapse/src/controles.js'] = (TOOLS / 'payload_entry/controles.js').read_bytes()
    html = files['index.html'].decode().replace('TRAMOYA · Relapse 13.60', 'TRAMOYA · Payloads 13.60')
    html = html.replace('Los payloads opcionales quedan fuera de esta prueba.',
                        'Después podrás enviar kstuff y ShadowMount+ por separado. Requiere un arranque limpio; no cargar etaHEN.')
    files['index.html'] = html.encode()
    files.update(payloads)
    # Los avisos originales de Relapse permanecen; añadir procedencia de los dos ELF.
    files['payloads-procedencia.json'] = (json.dumps(lock, indent=2) + '\n').encode()
    files['shadowmount-license.txt'] = (TOOLS / 'payload_entry/shadowmount-license.txt').read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9, allowZip64=False) as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry, data, compresslevel=9)
    if output.stat().st_size > lock['max_zip_bytes']:
        output.unlink()
        raise ValueError('El ZIP supera el presupuesto del perfil N16R8.')
    result = {'integration': 'tramoya-payloads-13.60-r1', 'target': 'esp32s3-n16r8',
              'target_firmware': '13.60', 'physical_validation': 'PENDIENTE',
              'minimal_zip_sha256': base_record['sha256'],
              'builder_sha256': minimo.sha(Path(__file__).read_bytes()),
              'base_builder_sha256': minimo.sha(Path(minimo.__file__).read_bytes()),
              'payload_lock_sha256': minimo.sha(LOCK.read_bytes()),
              'sha256': minimo.sha(output.read_bytes()), 'zip_bytes': output.stat().st_size,
              'files': {n: minimo.sha(d) for n, d in sorted(files.items())},
              'payloads': lock['payloads'],
              'adaptations': ['main: controles manuales tras elfldr',
                              'kexp: sustituir carga conjunta por envío individual permitido',
                              'entrada: identificar perfil; controles: confirmar kstuff antes de ShadowMount'],
              'excluded': 'etaHEN: alternativa incompatible con ShadowMount actual y mayor que PSRAM'}
    with provenance.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--payload-dir', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--target', choices=['esp32s3-n16r8'], required=True)
    args = parser.parse_args()
    result = build(args.source, args.payload_dir, args.out)
    print(f"PASS: {len(result['files'])} archivos; {result['zip_bytes']} bytes; SHA256 {result['sha256']}")
    print('Preparado en software; carga y funcionamiento en PS5 pendientes.')
