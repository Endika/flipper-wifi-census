# WiFi Census

A Flipper Zero app that runs a **passive Wi-Fi device census** over an **ESP32 Marauder**
dev board on the GPIO header. It counts and types the devices around you, saves each scan to
a renamable file on the SD card, and lets you **compare captures from different locations**
to see how many devices overlap. Everything stays local on the Flipper's SD — nothing is
uploaded anywhere.

## What it can and cannot tell you (please read)

Modern phones deliberately randomize their MAC address to avoid being tracked, so an honest
tool has to be clear about its limits:

- **Reliable** unique detection and A↔B matching for devices with a **stable MAC** (many IoT
  gadgets, laptops, older kit, access points) and for devices that **probe for a specific
  network by name** (a directed probe request reveals a known SSID).
- Modern phones send randomized MACs and mostly wildcard probes, so they are **counted but
  over-estimated** and **cannot be reliably matched across locations**. To recognize a
  friend's device across places, add it to the **Known devices** list — either its stable
  MAC, or use the shared-SSID trick (have them join a network whose name you control once;
  phones keep a stable MAC *per network*, so it matches in both places).
- Capture is **passive only**: it reads the management frames devices broadcast in the
  clear. No deauth, no injection, no association, no decryption.
- Imported captures (see below) also carry an **IE fingerprint** and a **vendor hint from
  vendor-specific IEs** — which can name a maker (Apple / Qualcomm / Broadcom …) *even for a
  randomized MAC*. The fingerprint groups devices that look like the **same model**, not the
  same unit, so it is shown for insight but is **not** used to merge devices or cross-match
  captures (those stay on the reliable stable-MAC / probed-SSID signals) — that would
  under-count identical models or invent matches between strangers.

Use it on your own space and people who are fine with it. The probed network names it can
record are sensitive (they hint at where someone lives, works or travels), which is exactly
why nothing leaves the SD card.

## Hardware

- Flipper Zero + the **official ESP32-S2 Wi-Fi Dev Board** on the GPIO header (2.4 GHz only;
  no Bluetooth, no on-board SD — everything is stored on the Flipper's SD).
- The board must run **ESP32 Marauder v1.17.0**. This app drives Marauder's serial CLI
  (`sniffprobe` / `stopscan`) over the USART lines and parses its probe-sniff output.

UART wiring is the standard dev-board layout (Flipper pin 13 TX → ESP32 RX, pin 14 RX ←
ESP32 TX, GND/3V3 shared). Default baud is 115200 (switchable to 230400 in Settings).

### Flashing Marauder

Three ways, pick whichever is handy:

- **From the Flipper itself** — with the board on the GPIO header, use the **ESP Flasher**
  app to write Marauder to the board. No PC needed.
- **Web flasher** — Chrome/Edge WebSerial, board connected by USB, no local tooling:
  <https://flash.pingequa.com/devices/flipper-wifi-devboard-marauder>.
- **Command line** — from a machine with USB and esptool (here, the Pi).

Pin the version at **v1.17.0**; this app is written against that release's serial line
format. If you use a different build and the counts look wrong, the probe-line format is
isolated in `src/domain/wc_marauder_parse.c` and can be adjusted there.

## Using it

1. **Scan** — starts probe sniffing; the screen shows live counts (unique / stable /
   randomized / by type). Press **Back** to stop.
2. Name the capture and it is saved to the SD as `<name>.wcen` (plus a `<name>.csv` you can
   read on a PC).
3. Move to the other location and scan again.
4. **Compare** — pick two saved captures; you get how many devices each had, how many are
   common (high confidence: same stable MAC or same probed SSID), and how many randomized
   devices could not be crossed.
5. **Known devices** — from a capture's device list, mark a device with a label. Labeled
   devices are then highlighted in comparisons.

Files are stored under `/ext/apps_data/flipper_wifi_census/`.

## Import a pcap (no Flipper needed)

Any raw-802.11 probe-request capture (linktype 105 — what Marauder writes, also Kismet /
airodump) can be turned into a census, on the Flipper or on a laptop:

- **On a PC** — build the host tool and pipe a pcap through it:
  ```sh
  make tool
  ./wc_import capture.pcap > census.csv
  ```
  It prints the same CSV the app writes (mac, random, type, vendor, networks sought, …) and a
  one-line summary on stderr. Handy when you have the pcap but not the Flipper.
- **On the Flipper** — drop the `.pcap` into `/ext/apps_data/flipper_wifi_census/` and use
  **Import pcap** in the menu (on-device import reads files up to 40 KB; use the PC tool for
  larger ones).

## Serial debug

**Settings is for options; the menu's Serial debug** starts the board and shows the raw lines
Marauder sends over UART, unparsed. Use it to see the exact probe-sniff line format (handy for
tuning what the live scan can extract, e.g. whether the network name appears in the text).

## Auto-save (rotate) for big venues

Turn on **Settings → Auto-save (rotate)** and a scan that fills up saves the current file and
starts a fresh one automatically (`auto_YYYYMMDD_HHMM_1`, `_2`, …), instead of dropping
devices. Pressing Back saves the final chunk too. You then merge all the pieces on a PC (see
below) into one census of the whole venue. A brief moment is lost at each rotation while the
file is written.

## Combine many captures on a PC (beyond the device limit)

A single Flipper scan or merge is capped at 320 devices (its RAM). To census a huge venue,
capture it as several files and merge them all on a laptop, where there is no such cap:

```sh
make tool
./wc_merge part1.wcen part2.wcen scan.pcap ... > all.csv
```

`wc_merge` deduplicates across every file (a device seen in two of them is counted once) and
accepts `.wcen` captures and `.pcap` files mixed, producing one CSV of the whole venue —
thousands of devices if needed.

## Building

Host-side domain logic is plain C and unit-tested with gcc; `furi` is confined to the
adapters and UI. The `.fap` is built with `ufbt` against the **stable** firmware SDK.

```sh
make test          # host unit tests (domain + application, in-memory fakes)
make linter        # cppcheck
make format-check  # clang-format --dry-run --Werror over all sources
ufbt               # build the .fap (writes dist/flipper_wifi_census.fap)
ufbt launch        # build, install and run on a connected Flipper
```

## Architecture

Hexagonal. `domain/` is pure C (observation model, device signature, dedup/census, capture
codec + CSV, comparison, known-device rules, the Marauder line parser) and is the only part
under host test. `application/` orchestrates use cases through ports. `platform/` holds the
furi adapters (serial, storage, clock) behind those ports, and `app/` + `scenes/` are the
scene-manager UI. That single boundary is what keeps the logic testable on the host.

## License

GPL-3.0. See [LICENSE](LICENSE).
