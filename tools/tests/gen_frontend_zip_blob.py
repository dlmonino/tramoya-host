#!/usr/bin/env python3
"""Generate frontend_zip_blob.c: the embedded archive for host tests.

Converts the archive named by WKAL_TEST_ZIP (the synthetic fixture that
build.py assembles, or any release-built frontend.zip) into a C array so
the HTTP/storage tests run against the exact bytes being served.
"""
import os
import sys
import zipfile

SRC = os.environ["WKAL_TEST_ZIP"]
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   "frontend_zip_blob.c")


def main():
    if not os.path.exists(SRC):
        print(f"{SRC} missing - run 'make zip' first", file=sys.stderr)
        sys.exit(1)
    data = open(SRC, "rb").read()
    with zipfile.ZipFile(SRC) as archive:
        count = len(archive.infolist())
        payload = archive.getinfo("payloads/payload.elf")
        payload_size = payload.file_size if payload else 0
    with open(OUT, "w") as f:
        f.write("/* frontend_zip_blob.c - GENERATED from WKAL_TEST_ZIP. */\n")
        f.write("#include <stdint.h>\n#include <stddef.h>\n\n")
        f.write("const uint8_t g_frontend_zip[] = {\n")
        for i in range(0, len(data), 16):
            f.write("    " + ",".join("0x%02x" % b for b in data[i:i+16]) + ",\n")
        f.write("};\n")
        f.write("const size_t g_frontend_zip_len = %d;\n" % len(data))
        f.write("const size_t g_frontend_zip_file_count = %d;\n" % count)
        f.write("const size_t g_frontend_zip_payload_size = %d;\n" % payload_size)
    print(f"wrote {OUT}: {len(data)/1024:.1f} KiB, payload {payload_size} B")


if __name__ == "__main__":
    main()
