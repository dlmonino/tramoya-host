[Español](estado-proyecto.md) · **English** · [TRAMOYA Host](../README.en.md)

# TRAMOYA status and validation

Updated: **9 October 2026**, tests completed at 23:29 (Europe/Madrid).
This record summarizes results supplied by the project owner and reviewed
alongside the local test records.

## What has been validated

The separate `esp32s3-servidor-diagnostico` project was flashed and physically
tested on an ESP32-S3-WROOM-1 N16R8, chip revision v0.2. Its pages are benign
and contain no PS5 content. The second board remains a reference running
the previously tested `hello_world` firmware.

| Test | Observed result |
|---|---|
| Flashing | Bootloader, partition table and application verified by esptool; file hashes checked before flashing; no full-chip erase. |
| Startup | Diagnostic firmware booted after pressing RST with BOOT released. No reflash was needed. |
| PSRAM | 8,388,608 bytes detected; internal probe passed, with the SDK memory test enabled. This is not an exhaustive test of every memory cell. |
| Wi-Fi | WPA2 AP `ESP32S3-Diagnostico` reachable from the Mac. |
| DHCP | Client `192.168.4.2/24`, server and DNS `192.168.4.1`; no router option. |
| DNS | `diagnostico.test A`: NOERROR and `192.168.4.1`; AAAA: NOERROR with no addresses; `example.com A`: NXDOMAIN. |
| HTTP | `/status.json` returned the firmware identity, IDF version and successful internal probes. |
| HTTPS | TLS 1.2 and HTTP 200; certificate and `diagnostico.test` hostname verified using explicit trust within the test process; `otro.test` rejected. |
| Isolation | No public IPv4/IPv6 routes through the tested Wi-Fi interface; Mac IP forwarding disabled; external connections failed while local services remained reachable. |
| USB MSC | `ESP32-S3 Diagnostico`, VID:PID `303A:4002`; approximately 13.5 MB medium and 13.2 MB FAT12 partition. |
| MSC files | `DIAG.TXT` read; 65,536 bytes written, synced and read; volume ejected and the same SHA-256 subsequently verified. |
| Stability | Ten minutes of HTTP, HTTPS and MSC reads: zero errors and no detected reboot. |

Persistence was checked after the instructed disconnect/reconnect step.
The hash proves content integrity; the physical power cycle is taken from
the context of the user's procedure, without independent measurement.

### Stability run

- Window: 23:19:00–23:29:01; recorded duration: 601.06 seconds.
- 61 samples, 122 HTTP/HTTPS requests and 61 MSC reads of 65,536 bytes.
- HTTP followed by concurrent HTTPS and MSC reads every ten seconds;
  no MSC writes during this run.
- MSC reads requested `F_NOCACHE`, which macOS accepted.
- Uptime increased from 234,057 to 835,065 ms, with no detected reboot.
- Internal memory reported by HTTP after a two-minute warm-up:
  minimum, maximum and medians of 227,659 bytes; median change: 0.

This result applies to that duration and load. It does not establish absence
of slow leaks, long-term reliability, large-file behavior or PS5 compatibility.
No reduction in power consumption or temperature has been measured.

### Diagnostic firmware identity

| Item | Value |
|---|---|
| SDK | ESP-IDF 5.4.2 |
| Memory and CPU | 16 MB flash; octal PSRAM at 80 MHz; fixed 160 MHz CPU |
| Application | 963,312 bytes |
| Bootloader / partition table / application | `0x0` / `0x8000` / `0x10000` |
| Application partition | 3 MiB |
| Storage | `0x310000`–`0x1000000`, 13,565,952 bytes |
| Configured logs | UART0, 115200 baud, TX GPIO43 and RX GPIO44 |

SHA-256 of the flashed application:

```text
e47eb8c2f56d2f828f11683e7a20eacb7768c3a7e2e690e6406d2716196a8e8d
```

SHA-256 of the MSC test file `TRMYTEST.BIN` (65,536 bytes):

```text
f9e123dd7c4b963886afa28e2153689634f7c31d6547c7bedb41068f2ee6ca07
```

These hashes identify the session artifacts, not a release of this repository's
firmware. The diagnostic project, images and test clients are kept separately
and are not added by this documentation update.

## What remains in this repository

The base revision is `70aed11faa1a657cd25cc5a52781c2b3764398f5`. Preparation
of the full firmware, called stage A in the work records, is incomplete.
USB components, miniz, frontend sources, Relapse and Payload Manager sources
were acquired and verified. Dependencies and preparation remain unresolved,
including Payload Manager translations and other sources and binaries required
by the selected flow. That complete flow has no validated build in this session.

Resuming work does not require downloading already acquired sources again.
The next work must fix the scope, resolve the required dependencies and record
revisions, hashes and licenses. Finding firmware 13.60 data in a source tree
does not prove functionality on the console.

The diagnostic application unmounts FAT and hands the volume to the computer;
HTTP serves embedded content. The base serves `frontend.zip` from FAT using
a different mounting model. **The validated MSC write does not yet demonstrate
served frontend updates or coordinated storage access in the full firmware.**

## Before testing on PS5

1. Complete full-firmware preparation and select content and versions applicable
   to PS5 13.60, recording provenance and licenses.
2. Integrate and build for N16R8 using a coherent image set and partition table.
   Review the AP, WPA2, DHCP, DNS and absence of Internet forwarding; the base
   still uses an open AP and does not inherit the diagnostic WPA2 configuration.
3. Validate actual `frontend.zip` updates from the computer: exclusive volume
   ownership during writes, ejection, reboot, integrity and newly served content.
   Use files of representative size.
4. Repeat startup, PSRAM, network, HTTP/HTTPS, isolation, MSC and stability checks
   against that final build, extending the run beyond ten minutes.
5. Prepare browser entry, DNS and HTTPS handling on the PS5. Explicit certificate
   trust in the Mac test client does not imply console acceptance. Also check
   USB power and the recovery procedure.

The first console test should check a benign page and isolated local connectivity
before moving to PS5 content. No console, payload or homebrew chain has been
tested with TRAMOYA. The reference board and separate `hello_world` recovery
image set are preserved.

## Evidence and continuity

Original records remain in the project's local work archive:

- `registro-pruebas-tramoya.txt`: chronology and reviewed outputs.
- `verificacion-artefactos.json`: pre-flash verification; its physical-tests-pending
  status describes that earlier point in time.
- `estabilidad-20261009-231900-672050.jsonl` and
  `estabilidad-20261009-231900-672050-resumen.json`: samples and stability summary.
- `tramoya-etapa-A-informe.txt`: full-firmware preparation status.
- The Drive logbook, updated with diagnostic completion and remaining work.

This document summarizes those records. The local files are not included or
linked as though they were accessible from this public repository.
