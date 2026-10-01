import os
"""Check the actual linked PEM buffers, including the mbedTLS NUL requirement."""
from pathlib import Path
import subprocess
import tempfile

from elftools.elf.elffile import ELFFile


root = Path(__file__).resolve().parents[1]
with (Path(os.environ.get("WKAL_BUILD_DIR", root / "build/esp32s3-n4r2")) / "ps5_wkal_host.elf").open("rb") as stream:
    elf = ELFFile(stream)
    symbols = elf.get_section_by_name(".symtab")
    buffers = {}
    for filename in ("servercert.pem", "prvtkey.pem"):
        prefix = "_binary_" + filename.replace(".", "_")
        start = symbols.get_symbol_by_name(prefix + "_start")[0]
        end = symbols.get_symbol_by_name(prefix + "_end")[0]
        section = elf.get_section(start["st_shndx"])
        offset = start["st_value"] - section["sh_addr"]
        length = end["st_value"] - start["st_value"]
        data = section.data()[offset:offset + length]
        source = (root / "main/certs" / filename).read_bytes()
        assert data == source + b"\0", f"{filename}: linked PEM must include exactly one trailing NUL"
        buffers[filename] = data[:-1]
        print(f"PASS: linked {filename}, {length} bytes including PEM terminating NUL")

with tempfile.TemporaryDirectory(prefix="wkal_tls_") as temp:
    paths = {}
    for filename, data in buffers.items():
        paths[filename] = Path(temp) / filename
        paths[filename].write_bytes(data)
    cert_public = subprocess.check_output([
        "openssl", "x509", "-in", str(paths["servercert.pem"]), "-pubkey", "-noout"])
    key_public = subprocess.check_output([
        "openssl", "pkey", "-in", str(paths["prvtkey.pem"]), "-pubout"])
    assert cert_public == key_public, "Linked certificate and private key do not match"
    subprocess.run(["openssl", "x509", "-in", str(paths["servercert.pem"]),
                    "-noout", "-subject", "-dates"], check=True)
    print("PASS: linked certificate parses and matches linked private key")
