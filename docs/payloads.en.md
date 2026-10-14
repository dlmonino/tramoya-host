**English** · [Español](payloads.md) · [Minimal Relapse](relapse-integration.en.md)

# Standalone payload profile for PS5 13.60

Updated 15 October 2026. David selected a standalone ESP32:
**Relapse → elfldr → kstuff Lite → ShadowMount+**, without etaHEN. **Hardware
test passed on his PS5 13.60:** he confirms the payloads loaded and ShadowMount+
ran and worked. His direct observation is accepted as the result for this run;
repeatability and stability still require testing.

## Pinned selection

| Payload | Release | ELF SHA-256 |
|---|---|---|
| kstuff Lite | v1.11; execution patches used by ShadowMount; release declares 1.00–13.60 support | `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` |
| ShadowMount+ | 1.7-beta5fix1; storage content mounting; requires kstuff and this release explicitly excludes etaHEN | `e85fd63b705498488f39c09c9aea6f9fa22e0df2e932865de49a8cedf25172d5` |

Sources: [kstuff](https://github.com/EchoStretch/kstuff-lite/releases/tag/v1.11),
[ShadowMount](https://github.com/drakmor/ShadowMountPlus/releases/tag/1.7-beta5fix1).
`tools/payloads-13.60.lock.json` pins commits, assets, sizes and hashes. Builder
inputs match GitHub release digests. No latest lookup, download, binary rebuild,
console execution or publication occurs in the builder. Relapse MIT notices and
ShadowMount GPL license are preserved. Binaries remain outside Git.

## etaHEN remains an alternative

The unofficial [etaHEN Unified r1 port](https://github.com/GronedWaffel/etahen-11.00-13.60/releases/tag/v2.5B-unified.1)
already includes kstuff v1.11. Never load a second kstuff. It provides Toolbox,
plugins, FPS display and additional tools absent from this profile. Its ELF is
36,746,616 bytes, SHA-256 `46c161473b27d12f1fa5e2d1d10466ce587db841ed9e8d71a219424d8fcf637f`.
A single-entry DEFLATE9 ZIP measured 12,297,655 bytes, beyond 8 MiB physical PSRAM;
the current snapshot firmware cannot hold it. It is preserved outside this ZIP.
This incompatibility statement applies to ShadowMount 1.7-beta5fix1, not every
ShadowMount version. The port also publishes a matched pair in
[v2.5B-13.60-r3](https://github.com/GronedWaffel/etahen-11.00-13.60/releases/tag/v2.5B-13.60-r3),
which is not selected or tested here. Its compressed size has not been measured.

## Packaging and behavior

```sh
python3 tools/build_payload_frontend.py --target esp32s3-n16r8 \
  --source "$TRAMOYA_RELAPSE_SOURCE" \
  --payload-dir "$TRAMOYA_PAYLOAD_DIR" --out build/payloads/frontend.zip
```

Both input directories must contain the exact locked sources/official ELF files.
Altered files, symlinks, overwrites and output inside source directories fail.
The minimal builder, upstream sources and firmware remain unchanged.
The new ZIP has **56 files, 2,297,244 bytes**, SHA-256
`5cb0b53e9aa285f4882bb5019b8b6cc5f23a84fb70732e8b73de0054becb1c90`.
Its packaging budget is 3 MiB; the minimal profile keeps its existing 1 MiB
budget. Neither budget measures available PSRAM. Firmware still requires a
contiguous snapshot allocation plus 512 KiB headroom. David confirmed physical use of this larger-ZIP profile; free PSRAM headroom
has not been measured. N16R8 firmware remains 927,920 bytes, SHA-256
`334f23534285f706fdf69e12b9210b5f0e6566770711f851d64cb887fac42e0a`; no flash is needed.

Manual controls replace the joint R2 hook after elfldr. Send kstuff, observe its
startup on the PS5, explicitly confirm, then send ShadowMount. There is no
blind timed startup. WebKit/kernel code, core binaries and original transport
remain intact; the adapter only accepts the two pinned names and releases its
send buffer. A completed TCP send is not proof of startup or operation. An
error locks further sends. Page state prevents repeated clicks but cannot detect
payloads loaded elsewhere or before reload. Always start from a fresh boot;
never reload or repeat the chain in the same console session.

## Evidence and recovery

Normal and ASan/UBSan mandatory-FAT recipes each pass 54 C tests/9,252 assertions,
five power checks and six FAT checks. N16R8 ESP-IDF 5.4.2 build passes with the
same firmware hash. All ten minimal-profile tests pass in both modes. Six new
profile tests pass per mode: inventory/hashes/reproducibility, preserved sources,
input tampering/symlinks, overwrite/budget guards, mocked manual controls and
all resources through the real C HTTP core under four route prefixes. Controls
cover ordering, confirmation, double clicks, errors and send-buffer release.
Candidate FAT round-trip preserves ZIP/WIFI and passes 112 C resource reads.
These checks do not execute the PS5 payloads.

The separate USB kit backs up ZIP/WIFI and restores the physically tested minimal
ZIP `8335f216cc9d590130fe567e5862de092a53e59b4f6e540e3deafb97f0934529`.
It requires local en0, certificate, MSC handoff witness and exact old content
before writing. HTTP serves the old PSRAM snapshot while the Mac owns FAT.
WIFI.TXT is never rewritten. Eject and restart, then verify all 56 HTTPS
resources without execution; large resources allow 90 seconds per request.
The reference board, old inert/hello_world recovery sets, WPA2 and initial
tramoya_host/tramoya_host credentials are retained. Kit procedure and evidence
are stored locally under `outputs/payloads-20261015/`; SHA256SUMS identifies the
kit files. Restoring a ZIP does not undo PS5 payload side effects.

**Hardware result — 15 October:** David confirms the payloads loaded and
ShadowMount+ ran and worked. Payload loading and observed ShadowMount+ operation
are recorded as PASS. The local USB trial remains ESCRITO (written), with valid
handoff witness, old HTTPS content before/during the write and intact ZIP/WIFI
backups. It contains no subsequent `verificar` execution, so a separate 56-resource
verification is not claimed. This does not invalidate David's direct functional
result. Frozen preparation manifests are supplemented by
`outputs/payloads-20261015/prueba-fisica-ps5/resultado.json` in the local archive.

**Pending:** specific content/format operations not described in this confirmation,
repeatability, session restoration and stability of this combination. The previous
30-minute test covered the inert frontend. HTTP56 remains unexplained. The minimal
ZIP rollback is retained.

On 15 October David chose not to run the proposed additional repeatability and
stability test at this stage. It is not scheduled as a requirement for further
work; the scope of the existing hardware result is retained.
