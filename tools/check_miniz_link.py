import os
"""Fail builds that resolve the vendored decompressor to the incompatible ROM ABI."""
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[1]
build = Path(os.environ.get("WKAL_BUILD_DIR", root / "build/esp32s3-n4r2"))
commands = json.loads((build / "compile_commands.json").read_text())
row = next(row for row in commands if row["file"].endswith("/main/wk_http.c"))
command = shlex.split(row["command"])
nm = str(Path(command[0]).with_name("xtensa-esp32s3-elf-nm"))
symbols = subprocess.check_output([nm, "--defined-only", str(build / "ps5_wkal_host.elf")], text=True)
implementation = next(line.split() for line in symbols.splitlines()
                      if line.split()[-1] == "wkal_tinfl_decompress")
assert implementation[1] == "T" and int(implementation[0], 16) >= 0x42000000, implementation
obj = Path(row["directory"]) / command[command.index("-o") + 1]
references = subprocess.check_output([nm, "-u", str(obj)], text=True)
assert " U wkal_tinfl_decompress" in references
assert " U tinfl_decompress\n" not in references
print(f"PASS: HTTP uses vendored miniz at 0x{implementation[0]}, not the ROM ABI")
