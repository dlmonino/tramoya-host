#!/usr/bin/env python3
"""Build frontend.zip for FAT storage from the staged frontend directory.

The zip uses raw-DEFLATE entries at max compression, mirroring the reference
host's embedding (tools/build_host.py) but WITHOUT base64: the firmware reads
the archive through the FAT filesystem and streams the DEFLATE entries.

Path filtering mirrors build_host.py's include_in_zip():
  - never embed .git, *.sha256 sidecars, slopkit/readme.png,
  - slopkit/payloads/ keeps only kexp*.bin.
"""
import argparse
from datetime import datetime, timezone
import importlib.util
import os
from pathlib import Path
import re
import zipfile
from partition_layout import partition


def include_in_zip(rel):
    if "/.git/" in rel or rel.endswith("/.git"):
        return False
    if rel.startswith("slopkit/payloads/"):
        name = os.path.basename(rel)
        return name.startswith("kexp") and name.endswith(".bin")
    if rel == "slopkit/readme.png":
        return False
    if rel.endswith(".sha256"):
        return False
    return True


def build(src, overrides, out_path, payload_path=None, repo=None):
    file_map = {}
    for base in (src, overrides):
        if not base or not os.path.isdir(base):
            continue
        for root, dirs, names in os.walk(base):
            dirs.sort()
            for name in sorted(names):
                if name == ".DS_Store":
                    continue
                full = os.path.join(root, name)
                rel = os.path.relpath(full, base).replace(os.sep, "/")
                if include_in_zip(rel):
                    file_map[rel] = full
    if payload_path:
        file_map["payloads/payload.elf"] = payload_path

    repo = Path(repo) if repo else Path(src).resolve().parents[1]
    spec = importlib.util.spec_from_file_location("wkal_icons", repo / "tools/gen_icons.py")
    icons = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(icons)
    master = (repo / "assets/icon.svg").read_text(encoding="utf-8")
    generated = {"logo.svg": master.encode(),
                 "favicon.svg": icons.build_wrapper_svg(master).encode()}
    base_version = re.search(r'#define\s+WKAL_VERSION\s+"([^"]+)"',
                             (repo / "include/wkali.h").read_text()).group(1)
    version = f"{base_version}-esp32s3"
    build_time = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    index = Path(file_map["index.html"]).read_text(encoding="utf-8")
    generated["index.html"] = index.replace("[[VERSION_PLACEHOLDER]]", version).replace(
        "[[BUILD_TIME_PLACEHOLDER]]", build_time).encode()
    app = Path(file_map["app.js"]).read_text(encoding="utf-8").replace("[[EXPLOIT_MODE]]", "auto")
    # The upstream startup leaves the splash covering all diagnostics when
    # a non-PS5 browser has no eligible exploit. Reveal the existing error UI.
    old = "if (!picked) {\n      updateProgress(0, 'Unsupported firmware.');\n      return;\n    }"
    new = "if (!picked) {\n      updateProgress(0, 'Unsupported firmware.');\n      revealExploit();\n      return;\n    }"
    if app.count(old) != 1:
        raise ValueError("Upstream startup changed; review the unsupported-browser splash patch")
    generated["app.js"] = app.replace(old, new).encode()
    css = Path(file_map["style.css"]).read_bytes()
    generated["style.css"] = css + b"\n" + Path(__file__).with_name("frontend_mobile.css").read_bytes()

    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED,
                         compresslevel=9, allowZip64=False) as zf:
        for rel in sorted(file_map.keys() | generated.keys()):
            if rel in generated:
                zf.writestr(rel, generated[rel])
            else:
                zf.write(file_map[rel], arcname=rel)
        raw = sum(entry.file_size for entry in zf.infolist())
        count = len(zf.infolist())

    return count, raw, os.path.getsize(out_path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="staged frontend dir (autoloader)")
    ap.add_argument("--overrides", default=None, help="PC-host overrides dir")
    ap.add_argument("--payload", default=None,
                    help="ELF to serve as payloads/payload.elf (installer flow)")
    ap.add_argument("--repo", default=None, help="Reference repo containing icon source and version")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    n, raw, zsize = build(args.src, args.overrides, args.out, args.payload, args.repo)
    print(f"frontend.zip: {n} files, {raw/1024:.0f} KiB raw -> {zsize/1024:.1f} KiB")
    # storagegen enforces the exact FAT/WL overhead and capacity.
    if zsize > partition("storage")[1]:
        print("ERROR: zip exceeds the storage partition")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
