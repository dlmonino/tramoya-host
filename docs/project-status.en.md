[Español](estado-proyecto.md) · **English** · [TRAMOYA Host](../README.en.md)

# TRAMOYA status and validation

## Current update — 14 October 2026

### Physically validated: TRAMOYA before the DNS correction

N16R8 build based on `7c197bc`, application **925,568 bytes**, SHA-256
`0a7e3144bece1178d6ae8f516d6f6734335fd114750c8e4004b4db7604d0d458`.
It was flashed with the inert diagnostic frontend; no homebrew payloads.
WPA2 and initial `tramoya_host` / `tramoya_host` settings are retained.

- WIFI.TXT: credential changes, absent file, invalid password and USB recovery
  passed. Initial settings were restored; the final ten HTTP/HTTPS requests passed.
- DHCP: 192.168.1.0/24, AP/DNS/router 192.168.1.1. Local A resolution and external
  name rejection passed; AAAA incorrectly returned A in that version.
- HTTP/HTTPS and the explicit local certificate passed short repeated checks.
  IPv4/IPv6 isolation passed for the interface and public destinations tested.
- HTTP curl 56 remains unexplained. Curl 63 was a test on the home Wi-Fi and
  is excluded from firmware failures. A passing batch does not close curl 56.

### Software candidate: DNS correction

[DNS contract and regression cases](dns.en.md): AAAA returns NOERROR/NODATA,
A/ANY remain local, unsupported classes are refused, RD is copied and RA is
clear. Malformed input and unsupported operations have explicit behavior.
New tests were demonstrated failing on the previous code, without removing
the existing coverage. Full test, ASan/UBSan, mandatory FAT and N16R8 build
results are recorded below. The final diff was independently reviewed.
**DNS hardware testing passed on 14 October, 20:38 (Europe/Madrid): 75 checks,
zero failures.** These comprise 69 DNS checks (empty AAAA, local A, external
rejection, types, classes, RD/RA and sizes), four HTTP/HTTPS integrity controls
and two IPv4/IPv6 isolation checks. The local record
`dns-fisico-20261014-203833-590203.json` retains packets and metrics. Identity
is checked using the certificate/content; the expected binary hash is a local
reference, not flash readback. This does not establish long-term stability or
resolve HTTP 56. Existing WIFI.TXT hardware results are preserved, not silently
relabelled as tests of the new binary.

Software verification completed with ESP-IDF 5.4.2: normal and ASan/UBSan
runs each passed **49 C tests, 9,203 assertions, five power tests and six FAT
checks**, with zero failures and no skips. The N16R8 build passed miniz linkage,
embedded certificate/key matching and partition checks. Application: **925,696
bytes**, SHA-256 `befcb546d27f031aef79d126af0b4a80db1e8c32f0239143775caa941fd68602`. Review found a UDP truncation issue: receiving 513 bytes now detects oversized
datagrams. A new transport simulation regression and both full suites passed
after the fix. The final review verdict is kept with the local evidence.

### HTTP 56 capture and USB frontend candidate

On the physically tested DNS candidate, 61 controlled requests (30 HTTP and
31 HTTPS) returned 200 with matching bodies. CDC captured Wi-Fi association
and HTTPS connections without observed errors during the run. **HTTP curl 56
was not reproduced; its cause remains open.** CDC started after boot and
cannot prove that no reset occurred. The local session archive holds the
traces, bodies, headers and console under
`outputs/http56-20261014/captura-20261014-205317-233225`.

The N16R8 candidate with a **hardware-tested USB update** copies the ZIP into
PSRAM before closing FAT and giving the medium to MSC. HTTP/HTTPS serve the
immutable copy. Its built application is **927,920 bytes**, SHA-256
`334f23534285f706fdf69e12b9210b5f0e6566770711f851d64cb887fac42e0a`.
With ESP-IDF 5.4.2, normal and ASan/UBSan recipes each passed 54 C tests,
9,252 assertions, five power checks and six FAT checks; the N16R8 build passed.
Eleven simulated USB update client tests and four flasher checks passed. An independent firmware/test
review found no confirmed blocking defect. The ZIP must fit the largest free
PSRAM block with a 512 KiB margin; large files and runs longer than 30 minutes still
need validation. A ZIP that cannot be indexed leaves the web frontend absent
and USB available for correction; CRC and DEFLATE are checked as entries
are served. DNS and web start only after MSC starts; `GET /__tramoya_storage`
returns a ZIP-independent witness which the update client requires before any
write or restore. The older DNS firmware has no witness and must not receive
USB writes.

David confirmed the USB test passed on 14 October: the 21:24 run backed up
WIFI.TXT/ZIP and checked that the old page remained served during the write.
The new page was observed afterward. At 21:31 the verifier confirmed HTTPS,
the new ZIP (1,140 bytes) and original WIFI.TXT (269 bytes), with matching
hashes and no further writes. Local record:
`outputs/frontend-20261014/verificacion-20261014-213134-152195/resultado.json`.
Repeating `write` at 21:25 stopped because the new page was already served;
this was a pre-write guard, not a firmware failure. The logs alone do not
identify the type of reset performed.

The same candidate **passed a physical 30-minute run** on 14 October:
`outputs/estabilidad-tramoya-20261014/ensayo-20261014-215646-779755/resumen.json`.
It completed 181 samples over 1,802.04 seconds, 561 web requests, 1,224 DNS
checks, 183 paired MSC reads and four isolation checks, all successful. Initial
and final regressions passed. CDC captured 25,087 bytes and 367 clocked lines
with no observed reset. The harness passed 20 simulated tests and independent
review. It does not read flash or free memory, rule out slower leaks, test a
large ZIP or every flash cell. The historical curl 56 remains unexplained.
David directly observed the ESP32 page in the User's Guide on his PS5 13.60
with the inert candidate. This is recorded as **physical evidence of page
access**. A connection-status photo shows IPv4 `192.168.1.3/24`, with
gateway and DNS `192.168.1.1`. David confirmed `tramoya_host`, Wi-Fi only,
and no LAN cable. The firmware runs as an access point without an uplink, and
prior Mac checks could not reach the Internet: the tested topology is isolated.
There was no direct PS5 Internet egress test, traffic trace, confirmation of
HTTP versus HTTPS, or evidence of certificate acceptance. MSC on the PS5 and
homebrew were not tested. The procedure and evidence are kept in
`outputs/ps5-inocua-20261014/` in the local archive.

The selected test board runs TRAMOYA. The other board preserves the independent
diagnostic. Frozen earlier images and a separate hello_world recovery kit
remain available. The following historical record describes only that diagnostic.

### Relapse 13.60: minimal hardware test passed

The [minimal integration](relapse-integration.en.md) pins the preserved Relapse
checkout at `d8e6896b5cb33b04f1e038a5d698d3d4aee5947c`, its kernel binary and
ELF loader. David's hardware test and photos IMG_4373/4374 confirm WebKit
readiness, kernel read/write, ready privileges, elfldr return `0x0`, and the
final TRAMOYA marker with optional payloads disabled on PS5 13.60. The minimal
chain and ELF loader startup are validated for this run. No additional ELF was
loaded in that minimal test; the later kstuff/ShadowMount+ profile has its result
below. Chain success rate remains untested. The ZIP has a manual entry point and
excludes the three optional
payloads whose exact versions have not been tied to verifiable releases.
The 14 October 23:43 USB record is VERIFIED: all 51 HTTPS resource hashes and
sizes match the ZIP, with HTTP200/curl0; the client checked ZIP and WIFI.TXT.
The N16R8 firmware is retained without reflashing, along with the inert ZIP and
WIFI.TXT backups. This chain does not need MSC on the PS5 or install
an autoloader shortcut. Evidence, tests and technical review are archived under
`outputs/integracion-relapse-20261014/` locally; the hardware result is in
`prueba-fisica-ps5/resultado.json`. Preparation manifests remain frozen. The
previous 30-minute inert-frontend run does not validate Relapse chain stability.

## Independent diagnostic record

Updated: **9 October 2026**, tests completed at 23:29 (Europe/Madrid).
This record summarizes results supplied by the project owner and reviewed
alongside the local test records.

## What has been validated

The separate `esp32s3-servidor-diagnostico` project was flashed and physically
tested on an ESP32-S3-WROOM-1 N16R8, chip revision v0.2. Its pages are benign
and contain no PS5 content. At that time the second board retained `hello_world`; see the 14 October
update for the current board roles.

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

## Current scope

David confirmed payload loading and observed ShadowMount+ operation on PS5 13.60.
On 15 October he chose not to run the proposed additional repeatability and
stability test at this stage. It is not scheduled as a requirement for further
work and is not recorded as passed.

Guide session restoration and console MSC remain untested. The inert page and
isolated Wi-Fi topology have been checked; there is still no direct Internet
egress test from the console.

The older diagnostic unmounts FAT and serves embedded pages. The new candidate
copies frontend.zip into PSRAM, closes FAT, then exposes MSC. HTTP does not read
the medium while the computer may write. The inert update and 30-minute
stability run passed; longer runs and pressure with large ZIPs remain pending.
The minimal chain reached elfldr; David then confirmed payload loading and
successful ShadowMount+ execution/operation with the later profile. Console MSC
remains untested; specific content operations were not described.
Explicit certificate acceptance on the Mac does not establish PS5 trust.

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


## Payload preparation — 15 October 2026

David selected standalone TRAMOYA without etaHEN. The [prepared profile](payloads.en.md)
pins kstuff Lite v1.11 and ShadowMount+ 1.7-beta5fix1 with separate manual sends
and explicit kstuff confirmation. Firmware is unchanged; rollback retains the
physically tested minimal ZIP. Normal/ASan/FAT, minimal regressions and N16R8
build pass. David subsequently confirmed the payloads loaded and ShadowMount+
ran and worked. This is recorded as a physical PASS for loading and observed
ShadowMount+ operation on PS5 13.60, without etaHEN. It does not identify a
specific mounting test or success rate. The local 00:15 USB trial remains ESCRITO:
three HTTPS responses and ZIP/WIFI backups checked, no 56-resource verification
output. Preparation manifests remain frozen; the later result is in
`outputs/payloads-20261015/prueba-fisica-ps5/resultado.json` locally. Repeatability,
stability of this combination, session restoration and specific content tests
remain pending. The earlier 30-minute run was inert; HTTP56 remains open.
Firmware and recovery sets are preserved.
