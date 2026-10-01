#!/usr/bin/env python3
"""Generate a self-signed certificate for manuals.playstation.net.

Produces certs/servercert.pem + certs/prvtkey.pem, which the build embeds
as NUL-terminated text (EMBED_TXTFILES in main/CMakeLists.txt). This is a BUILD-time step and
uses the host's openssl; the firmware itself parses the PEMs with mbedtls.
"""
import argparse
import os
import subprocess

CN = "manuals.playstation.net"
VALID_DAYS = 3650


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                 "..", "main", "certs"))
    args = ap.parse_args()
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    cert_path = os.path.join(out, "servercert.pem")
    key_path = os.path.join(out, "prvtkey.pem")

    subprocess.run(
        ["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
         "-days", str(VALID_DAYS), "-sha256",
         "-keyout", key_path, "-out", cert_path,
         "-subj", "/CN=" + CN],
        check=True, capture_output=True)
    print(f"wrote {cert_path} ({os.path.getsize(cert_path)} bytes)")
    print(f"wrote {key_path} ({os.path.getsize(key_path)} bytes)")


if __name__ == "__main__":
    main()
