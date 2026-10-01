"""Fetch only the pinned compression library; never fetch exploits implicitly."""
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def fetch():
    dep = json.loads((ROOT / "tools/dependencies.json").read_text())["miniz"]
    out = ROOT / ".deps/miniz"
    if all((out / name).is_file() and
           hashlib.sha256((out / name).read_bytes()).hexdigest() == digest
           for name, digest in dep["files"].items()):
        return out
    with urllib.request.urlopen(dep["url"], timeout=60) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != dep["sha256"]:
        raise ValueError("miniz archive SHA256 mismatch")
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        files = {name: archive.read(name) for name in (*dep["files"], "LICENSE")}
    for name, digest in dep["files"].items():
        if hashlib.sha256(files[name]).hexdigest() != digest:
            raise ValueError("miniz file SHA256 mismatch: " + name)
    out.mkdir(parents=True, exist_ok=True)
    for name, content in files.items():
        (out / name).write_bytes(content)
    print("Verified external miniz " + dep["version"])
    return out


if __name__ == "__main__":
    fetch()
