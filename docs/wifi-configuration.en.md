[Español](configuracion-wifi.md) · **English** · [TRAMOYA Host](../README.en.md)

# Configure Wi-Fi over USB

The firmware reads **`WIFI.TXT` at the root of the USB volume** once at startup.
The initial contents are:

```ini
ssid=tramoya_host
password=tramoya_host
```

The access point uses **WPA2-PSK with AES/CCMP**. Edit both values without
rebuilding or reflashing firmware. These credentials configure TRAMOYA's own
access point, not an upstream router connection.

## Change the name or password

1. Connect the device to a computer and open `WIFI.TXT` on its USB volume.
2. Edit `ssid=` and `password=` in a plain-text editor and save as UTF-8.
   In TextEdit, use plain text; do not save as RTF or `WIFI.TXT.txt`.
3. Safely eject the volume. Resolve any ejection error before disconnecting.
4. Disconnect and reconnect USB power. The next boot applies the file.
   Reconnect clients if the network credentials changed.

New storage images include [tools/usb_root/WIFI.TXT](../tools/usb_root/WIFI.TXT).
Create the file manually on an existing volume if it is absent. Firmware does
not create or overwrite it at startup. Regenerating and flashing storage replaces
its files; editing the settings does not require that operation.

## Accepted format

| Field or rule | Requirement |
|---|---|
| Filename | `WIFI.TXT` at the volume root |
| `ssid` | 1–32 UTF-8 bytes, without control characters |
| `password` | 8–63 printable ASCII characters, including spaces |
| Authentication | WPA2-PSK; no open network or 64-character hexadecimal key |
| File size | At most 1024 bytes, including comments and BOM |
| Lines | LF or CRLF; optional initial UTF-8 BOM and final line without a newline |
| Comments | Lines beginning with `#`; blank lines are accepted |
| Keys | Exactly `ssid` and `password`, once each, in either order |

Use lowercase keys with no spaces before `=`. Spaces, `#`, `=` and quotes
inside values are literal: do not quote values or add trailing comments to
assignments. SSID length is measured in bytes; accented letters may use more
than one byte.

The password is plain text on USB and visible to anyone with volume access.
The application does not print it in logs or persist it as Wi-Fi settings in NVS.

## Missing or invalid configuration

**Wi-Fi stays off.** No open network, previous credentials or automatic factory
fallback is used. USB remains available to create or repair `WIFI.TXT`; eject
and restart afterward. DHCP, DNS and HTTP/HTTPS servers do not start.
Logs identify the error without showing the password.

`tramoya_host` / `tramoya_host` are storage-template defaults, not a recovery
network for an invalid file. Failure to mount the storage itself is a separate
failure; the firmware does not automatically format the volume.

## Startup order and scope

1. Mount FAT locally without enumerating USB to the computer.
2. Read, close and validate `WIFI.TXT`.
3. If valid, copy the complete ZIP into PSRAM and start WPA2; close the
   application FAT mount and expose MSC with USB as its exclusive owner.
   Only after MSC starts successfully, start DNS and HTTP/HTTPS, serving
   the PSRAM copy and MSC witness.
4. If invalid, close the application FAT mount and enable USB-only recovery.

Credential reads finish before the computer can write. Settings are not reloaded
while running. The new candidate serves the PSRAM snapshot; eject and restart
to use an edited ZIP. If it cannot fit in PSRAM or cannot be indexed, USB remains
available for correction.

Physical validation on 14 October 2026 (build based on `7c197bc`): credential
changes, missing-file behavior, invalid-password rejection and USB recovery
passed. Original settings were restored and HTTP/HTTPS passed afterward.
The new DNS candidate subsequently passed its DNS hardware test and
HTTP/HTTPS/isolation controls; see [Status](project-status.en.md). The exclusive
FAT ownership candidate passed its inert ZIP hardware update with WIFI.TXT
integrity on 14 October. The same candidate passed a physical 30-minute run
of DNS, HTTP/HTTPS, MSC reads and isolation. Longer runs and the complete PS5
network/USB flow remain untested. David directly observed the ESP32 page in
the PS5 User's Guide and its Wi-Fi connection to tramoya_host without LAN. The
photo shows IPv4 192.168.1.3, gateway and DNS 192.168.1.1. The Guide's
transport was not recorded.
