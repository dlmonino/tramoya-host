#!/usr/bin/env python3
"""Portable target-aware wrapper. Run from an activated ESP-IDF v5.4.2 shell."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile

from diagnostic_frontend import generate
from fetch_dependencies import fetch
from power_config import prepare_power_config, validate_power_config

ROOT = Path(__file__).resolve().parents[1]
BOARDS = json.loads((ROOT / "targets/boards.json").read_text())


def run(args, env):
    subprocess.run([str(a) for a in args], cwd=ROOT, env=env, check=True)


def resolve_frontend(args, build, env):
    """frontend.zip for the storage image: an explicit ZIP wins; otherwise
    build it from an explicitly pinned upstream tag or generate
    the explicitly selected offline diagnostic page. Runs as a subprocess so partition_layout
    sees this target's WKAL_BOARD. Always leaves a frontend.zip-source.json
    provenance record next to the packaged ZIP."""
    provenance = build / "frontend.zip-source.json"
    if args.frontend:
        frontend = args.frontend.resolve()
        source = Path(str(frontend) + "-source.json")
        if source.is_file():
            provenance.write_bytes(source.read_bytes())
        else:
            provenance.write_text(json.dumps(
                {"output": str(frontend), "mode": "external-file"}) + "\n")
        return frontend
    out = build / "frontend.zip"
    if args.frontend_source == "release":
        command = [sys.executable, "tools/build_frontend_zip.py", "--out", out,
                   "--target", args.target]
        if args.upstream_tag:
            command += ["--tag", args.upstream_tag]
        subprocess.run(command, cwd=ROOT, env=env, check=True)
        return out
    generate(out)
    provenance.write_text(json.dumps({"output": str(out), "mode": "diagnostic"}) + "\n")
    return out


def validate_config(build, board, profile):
    validate_power_config(build / "sdkconfig")
    config = (build / "sdkconfig").read_text()
    required = [f'CONFIG_ESPTOOLPY_FLASHSIZE="{profile["flash_mb"]}MB"',
                f'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="{profile["partition_table"]}"',
                'CONFIG_SPIRAM_MODE_' + ("QUAD" if profile["psram_mode"] == "quad" else "OCT") + '=y']
    if not all(line in config.splitlines() for line in required):
        raise ValueError("sdkconfig does not match " + board + "; use a fresh build directory")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["targets", "deps", "build", "storage", "package", "test", "test-asan", "menuconfig", "monitor", "flash"])
    parser.add_argument("--target", choices=sorted(BOARDS), help="Required board profile except for targets/deps")
    parser.add_argument("--frontend", type=Path, help="External ZIP; overrides --frontend-source")
    parser.add_argument("--frontend-source", choices=["release", "diagnostic"], default="release",
                        help="Packaged firmware serves the frontend built from the "
                             "pinned upstream release tag (network); 'diagnostic' "
                             "uses our offline placeholder page")
    parser.add_argument("--upstream-tag", default=None,
                        help="Required upstream tag for release storage/package without --frontend")
    parser.add_argument("--payload-dir", type=Path, help="Optional external USB payload directory")
    parser.add_argument("--port", help="Explicit serial port; never auto-select a connected board")
    parser.add_argument("--erase-storage", action="store_true", help="With flash, write storage too (erases USB files)")
    parser.add_argument("--require-fat", action="store_true",
                        help="Fail before tests if ESP-IDF is not activated; never silently skip FAT")
    args = parser.parse_args()
    if args.command not in ("targets", "deps") and not args.target:
        parser.error("Select the board explicitly with --target (David's validated board: esp32s3-n16r8)")
    if args.require_fat and (args.command not in ("test", "test-asan") or not os.environ.get("IDF_PATH")):
        parser.error("--require-fat requires test/test-asan and an activated ESP-IDF shell")
    if args.command in ("storage", "package") and not args.frontend and args.frontend_source == "release" and not args.upstream_tag:
        parser.error("Reproducible packaging requires --frontend or --upstream-tag; use --frontend-source diagnostic for offline checks")
    if args.command == "targets":
        print(json.dumps(BOARDS, indent=2)); return
    fetch()
    if args.command == "deps":
        return
    env = dict(os.environ, WKAL_BOARD=args.target)
    # espressif/idf container images export IDF_TARGET=esp32; idf.py rejects
    # a differing command-line target, so pin the environment too.
    env["IDF_TARGET"] = "esp32s3"
    build = ROOT / "build" / args.target
    build.mkdir(parents=True, exist_ok=True)
    env["WKAL_BUILD_DIR"] = str(build)
    if args.command in ("test", "test-asan"):
        run([sys.executable, "tools/tests/test_power_config.py"], env)
        path = args.frontend.resolve() if args.frontend else generate(ROOT / "build/test/frontend.zip", test=True)
        env["WKAL_TEST_ZIP"] = str(path)
        run(["make", "-C", "tools/tests", "clean"], env)
        run(["make", "-C", "tools/tests", "asan" if args.command == "test-asan" else "all"], env)
        return
    if not env.get("IDF_PATH"):
        parser.error("Activate ESP-IDF v5.4.2 first (IDF_PATH is unset)")
    idf = [sys.executable, str(Path(env["IDF_PATH"]) / "tools/idf.py"),
           "-B", str(build), "-D", "WKAL_BOARD=" + args.target,
           # Pin the target: the espressif/idf container exports
           # IDF_TARGET=esp32, which would override sdkconfig.defaults.
           "-D", "IDF_TARGET=esp32s3"]
    profile = BOARDS[args.target]
    if args.command == "monitor":
        if not args.port: parser.error("monitor requires --port")
        run(idf + ["-p", args.port, "monitor"], env); return
    certs = ROOT / "main/certs"
    if not all((certs / name).is_file() for name in ("servercert.pem", "prvtkey.pem")):
        run([sys.executable, "tools/gen_ssl_cert.py"], env)
    if args.command == "menuconfig":
        run(idf + ["menuconfig"], env); return
    if args.command != "storage":
        prepare_power_config(build / "sdkconfig")
        run(idf + ["build"], env)
        validate_config(build, args.target, profile)
        run([sys.executable, "tools/check_miniz_link.py"], env)
        run([sys.executable, "tools/check_tls_embed.py"], env)
        run([sys.executable, "tools/check_target.py"], env)
    if args.command in ("storage", "package") or (args.command == "flash" and args.erase_storage):
        frontend = resolve_frontend(args, build, env)
        with zipfile.ZipFile(frontend) as archive:
            if "index.html" not in archive.namelist() or archive.testzip() is not None:
                raise ValueError("Frontend must have index.html at ZIP root and valid CRCs")
        print("Frontend SHA256: " + hashlib.sha256(frontend.read_bytes()).hexdigest())
        command = [sys.executable, "tools/storagegen.py", frontend]
        if args.payload_dir: command.append(args.payload_dir.resolve())
        run(command + ["-o", build / "storage.img"], env)
    if args.command == "package":
        run([sys.executable, "tools/package_firmware.py"], env)
    if args.command == "flash":
        if not args.port: parser.error("flash requires --port; no automatic device selection")
        # esptool refuses images whose header Flash size exceeds the physical chip.
        # Explicit port is mandatory. Storage is untouched unless explicitly selected.
        run(idf + ["-p", args.port, "flash"], env)
        if args.erase_storage:
            import csv
            table = ROOT / profile["partition_table"]
            offset = next(row[3] for row in csv.reader(table.read_text().splitlines()) if row and row[0] == "storage")
            run([sys.executable, "-m", "esptool", "--chip", "esp32s3", "--port", args.port,
                 "write_flash", offset, build / "storage.img"], env)


if __name__ == "__main__":
    main()
