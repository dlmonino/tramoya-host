"""Generate our own diagnostic frontend and separate synthetic test data."""
import argparse
import hashlib
from pathlib import Path
import zipfile

INDEX = """<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-S3 offline host</title><link rel="icon" href="/favicon.svg">
<link rel="stylesheet" href="/style.css"></head><body><main>
<img src="/logo.svg" width="64" height="64" alt="">
<h1>ESP32-S3 host is ready</h1>
<p>This diagnostic page is included with the firmware. Import your own frontend.zip
to use a different frontend.</p><p>HTTP, HTTPS, DNS and storage are available.</p>
</main></body></html>"""
SVG = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="12" fill="#334155"/><path d="M16 24h32v16H16z" fill="#38bdf8"/></svg>'
CSS = "body{background:#0f172a;color:#e2e8f0;font:18px sans-serif;margin:0;padding:24px}main{max-width:680px;margin:10vh auto}p{line-height:1.6}img{display:block}"


def generate(path, test=False):
    files = {"index.html": INDEX.encode(), "style.css": CSS.encode(),
             "logo.svg": SVG.encode(), "favicon.svg": SVG.encode()}
    if test:
        # An inert byte fixture: ELF magic exercises MIME/path handling only.
        # It is neither an executable nor any upstream exploit/payload.
        files["payloads/payload.elf"] = b"\\x7fELF" + b"".join(
            hashlib.sha256(str(i).encode()).digest() for i in range(4096))
        files["nested/stream.bin"] = bytes(range(256)) * 300
        # Realistic size so entry-sampling tests have a usable offset range.
        files["app.js"] = b"/* synthetic fixture */" + b" " * 8192
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=9, allowZip64=False) as archive:
        for name, data in sorted(files.items()):
            info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, data)
    return path


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--test", action="store_true")
    args = ap.parse_args()
    generate(args.out, args.test)
