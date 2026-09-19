# WiFi Census

Passive Wi-Fi device census for the Flipper Zero, driving an **ESP32 Marauder** dev board
over UART. It counts and types the devices around you, saves each scan to a renamable file
on the SD card, and lets you **compare captures from different locations** to see how many
devices overlap. Everything stays local on the Flipper's SD — nothing is uploaded.

> Status: work in progress. Assembly, flashing and usage docs land with the final task.

## What it can and cannot tell you (honest limits)

- **Reliable** unique detection and A/B matching for devices with a **stable MAC** (many IoT
  gadgets, laptops, older kit, access points) and for devices that **probe for a specific
  network name** (a directed probe request reveals a known SSID).
- Modern phones randomize their MAC and mostly send wildcard probes, so they are **counted
  but over-estimated**, and **cannot be reliably matched across locations**. To recognize a
  friend's phone across places, add it to the **known-devices** list (stable MAC, or the
  shared-SSID consent trick).
- Capture is **passive only**: it reads management frames devices broadcast in the clear. No
  deauth, no injection, no association, no decryption.

## License

GPL-3.0. See [LICENSE](LICENSE).
