#!/usr/bin/env python3
"""Empaqueta la copia local fijada de Relapse para una prueba mínima en 13.60."""
import argparse
import hashlib
import json
from pathlib import Path
import zipfile
import zlib

TOOLS = Path(__file__).resolve().parent
LOCK = TOOLS / 'relapse-13.60.lock.json'
MAX_ZIP_BYTES = 1024 * 1024
OLD_HOOK = '''    watchR2(async () => {
      try {
        const { loadOptionalPayloads } = await import("./kexp.js");
        await loadOptionalPayloads(p, chain, (message) => log(message, "info"));
      } catch (error) {
        log(error instanceof Error ? error.message : String(error), "error");
      }
    });'''
NEW_HOOK = '''    // Esta entrega termina en elfldr; los payloads opcionales se validan aparte.
    log("TRAMOYA: prueba minima completa; payloads opcionales desactivados", "info");'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def source_bytes(source, lock):
    result = {}
    for name, expected in lock['files'].items():
        path = source / name
        if any(p.is_symlink() for p in (path, *path.parents)):
            raise ValueError('Enlace simbólico no permitido: ' + name)
        data = path.read_bytes()
        if sha(data) != expected:
            raise ValueError('Fuente distinta del commit fijado: ' + name)
        result[name] = data
    return result


def build(source, output):
    source, output = Path(source).absolute(), Path(output).absolute()
    if output.resolve().is_relative_to(source.resolve()):
        raise ValueError('El resultado debe quedar fuera de las fuentes conservadas.')
    provenance = Path(str(output) + '-source.json')
    if output.exists() or output.is_symlink() or provenance.exists() or provenance.is_symlink():
        raise ValueError('El destino ya existe; utiliza otro directorio para conservarlo.')
    lock = json.loads(LOCK.read_text())
    original = source_bytes(source, lock)
    # El índice del firmware usa minúsculas; conservar los bytes y normalizar nombres.
    files = {'relapse/' + name.lower(): data for name, data in original.items()
             if name not in lock['excluded']}
    if len(files) != len(original) - len(lock['excluded']):
        raise ValueError('Colisión de rutas al convertir a minúsculas.')
    main = original['src/main.js'].decode('utf-8')
    if main.count(OLD_HOOK) != 1:
        raise ValueError('El gancho opcional cambió; revisar la adaptación.')
    files['relapse/src/main.js'] = main.replace(OLD_HOOK, NEW_HOOK).encode('utf-8')
    for name in ('index.html', 'inicio.js'):
        files[name] = (TOOLS / 'relapse_entry' / name).read_bytes()
    # Fecha, orden y permisos fijos: el mismo entorno produce los mismos bytes.
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'x', compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9, allowZip64=False) as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.create_system = 3
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry, data, compresslevel=9)
    if output.stat().st_size > MAX_ZIP_BYTES:
        output.unlink()
        raise ValueError('ZIP mayor que el presupuesto conservador de 1 MiB.')
    record = {'integration': 'tramoya-relapse-13.60-r1',
              'target': 'esp32s3-n16r8', 'target_firmware': '13.60',
              'repository': lock['repository'], 'commit': lock['commit'],
              'source_lock_sha256': sha(LOCK.read_bytes()),
              'builder_sha256': sha(Path(__file__).read_bytes()),
              'sha256': sha(output.read_bytes()), 'zip_bytes': output.stat().st_size,
              'files': {n: sha(d) for n, d in sorted(files.items())},
              'excluded': lock['excluded'], 'zlib_runtime': zlib.ZLIB_RUNTIME_VERSION,
              'physical_validation': 'PENDIENTE',
              'patch': {'file': 'relapse/src/main.js',
                        'before': sha(original['src/main.js']),
                        'after': sha(files['relapse/src/main.js']),
                        'reason': 'Desactivar la carga conjunta de payloads no validados'}}
    # La creación exclusiva conserva también un sidecar aparecido durante la construcción.
    with provenance.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(record, ensure_ascii=False, indent=2) + '\n')
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--target', required=True, choices=['esp32s3-n16r8'])
    args = parser.parse_args()
    record = build(args.source, args.out)
    print(f"PASS: {len(record['files'])} archivos; {record['zip_bytes']} bytes")
    print('SHA256: ' + record['sha256'])
    print('Preparado en software; ejecución en PS5 pendiente.')


if __name__ == '__main__':
    main()
