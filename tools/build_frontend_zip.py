#!/usr/bin/env python3
"""Build frontend.zip from the upstream ps5-webkit-autoloader release.

Standalone helper (no ESP-IDF needed): it stages the upstream source tree at
its latest GitHub release tag via tools/upstream.py, applies the same
filters/patches as the firmware packaging, and writes a frontend.zip that can
be flashed with the firmware OR copied straight onto the device's USB drive
(plug the ESP32-S3 into a PC, replace frontend.zip on the drive, unplug).

The served ZIP is never committed to this repository; every build records
where it came from in a <out>-source.json provenance file.

Usage:
    python3 tools/build_frontend_zip.py                    # latest release
    python3 tools/build_frontend_zip.py --tag v0.4.0       # pinned release
    python3 tools/build_frontend_zip.py --upstream ../ps5-webkit-autoloader
"""
import argparse
import hashlib
import json
import os
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import upstream  # noqa: E402

BOARDS = json.loads((TOOLS.parent / "targets/boards.json").read_text())


def build(out, spec=None, target=None, payload=None):
    # partition_layout pins the board at import time from the environment.
    target = target or os.environ.get("WKAL_BOARD", "esp32s3-n4r2")
    os.environ["WKAL_BOARD"] = target
    import make_frontend_zip
    from partition_layout import BOARDS, partition

    if target not in BOARDS:
        sys.exit(f"error: unknown target {target}; known: {', '.join(sorted(BOARDS))}")
    try:
        staged, provenance = upstream.stage(spec)
    except upstream.UpstreamError as error:
        sys.exit(f"error: {error}")
    print(f"upstream: {provenance.get('tag') or 'local tree'} "
          f"({(provenance.get('commit') or '?')[:12]})")

    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    count, raw, zsize = make_frontend_zip.build(
        src=staged / "frontend/autoloader",
        overrides=staged / "pc-host/overrides",
        out_path=out,
        payload_path=payload,
        repo=staged)
    data = Path(out).read_bytes()
    budget = partition("storage")[1]
    if zsize > budget:
        sys.exit(f"error: frontend.zip is {zsize} bytes but the {target} "
                 f"storage partition only holds {budget}; try a larger board "
                 "(--target) or a smaller payload")
    provenance.update({
        "output": str(out),
        "sha256": hashlib.sha256(data).hexdigest(),
        "entries": count,
        "raw_bytes": raw,
        "zip_bytes": zsize,
        "target": target,
        "storage_budget_bytes": budget,
        "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    })
    Path(str(out) + "-source.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print(f"frontend.zip: {count} files, {raw / 1024:.0f} KiB raw -> "
          f"{zsize / 1024:.1f} KiB ({zsize}/{budget} of the {target} storage partition)")
    print("SHA256: " + provenance["sha256"])
    return provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default="frontend.zip", help="output ZIP path (default: frontend.zip)")
    parser.add_argument("--upstream", default=None,
                        help="'release' (default), an upstream tag like v0.4.0, "
                             "or a path to an already-cloned upstream tree")
    parser.add_argument("--tag", default=None, help="pin the upstream release tag, e.g. v0.4.0")
    parser.add_argument("--target", default=None, choices=sorted(BOARDS),
                        help="board whose storage budget the ZIP is checked "
                             "against (default: esp32s3-n4r2)")
    parser.add_argument("--payload", default=None,
                        help="ELF to serve as payloads/payload.elf instead of the "
                             "upstream unified-autoloader payload (installer flow)")
    args = parser.parse_args()
    spec = args.tag or args.upstream or os.environ.get("WKAL_UPSTREAM")
    build(Path(args.out), spec, args.target, args.payload)


if __name__ == "__main__":
    main()
