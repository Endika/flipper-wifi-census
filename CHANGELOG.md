# Changelog

## [0.1.16](https://github.com/Endika/flipper-wifi-census/compare/v0.1.15...v0.1.16) (2026-09-19)


### Bug Fixes

* parse Marauder v1.17 probe lines (bare RSSI, Ch, Client, Requesting SSID) ([776dc4a](https://github.com/Endika/flipper-wifi-census/commit/776dc4a16eeb2cd7fbc1a25d170176c655c96909))

## [0.1.15](https://github.com/Endika/flipper-wifi-census/compare/v0.1.14...v0.1.15) (2026-09-19)


### Features

* add a Serial debug view showing raw Marauder lines ([54cf475](https://github.com/Endika/flipper-wifi-census/commit/54cf47560dce5aaab886fc1be80114ae89ed3924))

## [0.1.14](https://github.com/Endika/flipper-wifi-census/compare/v0.1.13...v0.1.14) (2026-09-19)


### Bug Fixes

* also read a quoted SSID from Marauder probe lines lacking an SSID label ([40b674e](https://github.com/Endika/flipper-wifi-census/commit/40b674e1547f1fa1e2209f5fca46e13b2a5ef511))

## [0.1.13](https://github.com/Endika/flipper-wifi-census/compare/v0.1.12...v0.1.13) (2026-09-19)


### Features

* auto-save rotation option to capture huge venues across files ([f9c482e](https://github.com/Endika/flipper-wifi-census/commit/f9c482e11202a6cbab7df0e3942ac544489698b7))

## [0.1.12](https://github.com/Endika/flipper-wifi-census/compare/v0.1.11...v0.1.12) (2026-09-19)


### Features

* merge many captures off-device with the wc_merge host tool (no device cap) ([2f200ce](https://github.com/Endika/flipper-wifi-census/commit/2f200ce66162688a95bee4c76861defb98cc69f1))

## [0.1.11](https://github.com/Endika/flipper-wifi-census/compare/v0.1.10...v0.1.11) (2026-09-19)


### Features

* raise census ceiling to 320 devices for denser venues ([b0a8f67](https://github.com/Endika/flipper-wifi-census/commit/b0a8f671ede5c3ebec670b839c1545b8346610f3))

## [0.1.10](https://github.com/Endika/flipper-wifi-census/compare/v0.1.9...v0.1.10) (2026-09-19)


### Bug Fixes

* reserve census up front and shrink per-device size to stop mid-scan OOM reboots ([1ab1c42](https://github.com/Endika/flipper-wifi-census/commit/1ab1c42ad609c79b8978472d11cee15642005894))

## [0.1.9](https://github.com/Endika/flipper-wifi-census/compare/v0.1.8...v0.1.9) (2026-09-19)


### Features

* default merges to merge_* and show dropped count when the census caps ([4df77a3](https://github.com/Endika/flipper-wifi-census/commit/4df77a30123e178ddfdb2082771cd8fca90054e5))

## [0.1.8](https://github.com/Endika/flipper-wifi-census/compare/v0.1.7...v0.1.8) (2026-09-19)


### Bug Fixes

* stream capture save to avoid the double-buffer OOM (merge/scan reboot) ([b634ff4](https://github.com/Endika/flipper-wifi-census/commit/b634ff48e5178ba8693438c9f2e1c8fbb6ccb6ef))

## [0.1.7](https://github.com/Endika/flipper-wifi-census/compare/v0.1.6...v0.1.7) (2026-09-19)


### Bug Fixes

* census grows on demand (fits big scans/merges) and merge shows a result summary ([a2eb175](https://github.com/Endika/flipper-wifi-census/commit/a2eb175d408d6eed650043860d69ba460bfe836e))

## [0.1.6](https://github.com/Endika/flipper-wifi-census/compare/v0.1.5...v0.1.6) (2026-09-19)


### Bug Fixes

* merge dedups by exact MAC first, matching live-session behavior ([5acde18](https://github.com/Endika/flipper-wifi-census/commit/5acde18e3df0985a0df492712f0b3289706bac6b))
* shrink census memory so merge/compare/import fit the FAP heap ([a1d897b](https://github.com/Endika/flipper-wifi-census/commit/a1d897bb90b138f582e53aea1adf57980eb412b0))

## [0.1.5](https://github.com/Endika/flipper-wifi-census/compare/v0.1.4...v0.1.5) (2026-09-19)


### Features

* store IE fingerprint and detect vendor from probe vendor IEs ([ca0b9e8](https://github.com/Endika/flipper-wifi-census/commit/ca0b9e84c386dcd111c6fce9803812a1acf5e3d8))

## [0.1.4](https://github.com/Endika/flipper-wifi-census/compare/v0.1.3...v0.1.4) (2026-09-19)


### Features

* import a pcap into a census on-device and via a host tool ([4213ec1](https://github.com/Endika/flipper-wifi-census/commit/4213ec16caee6aff6ab897210153fbaec14c448c))
* parse raw 802.11 probe frames with an IE fingerprint ([e3e87d6](https://github.com/Endika/flipper-wifi-census/commit/e3e87d6f8cb63a50e51dad31152d3e838b4d75fe))

## [0.1.3](https://github.com/Endika/flipper-wifi-census/compare/v0.1.2...v0.1.3) (2026-09-19)


### Features

* label stable-MAC devices by vendor via a verified OUI table ([df34605](https://github.com/Endika/flipper-wifi-census/commit/df3460577050d46df6fd6b446f894122e13d252b))
* merge two captures into an accumulated census ([3ff7ac1](https://github.com/Endika/flipper-wifi-census/commit/3ff7ac1357c47c3dff8edecc4cdf08a4d9807c0c))
* prefill a timestamped default name when saving a capture ([2e5803d](https://github.com/Endika/flipper-wifi-census/commit/2e5803d7659e7e02abcf8c720591fb44cf41c47b))
* surface networks devices probe for and drop the AP counter ([9768291](https://github.com/Endika/flipper-wifi-census/commit/9768291741b47447c954079407a9059f66d64fa1))

## [0.1.2](https://github.com/Endika/flipper-wifi-census/compare/v0.1.1...v0.1.2) (2026-09-19)


### Features

* show version, author and GitHub link in About ([914cc30](https://github.com/Endika/flipper-wifi-census/commit/914cc303531ff91dc99748765637fd7275ef3af7))

## [0.1.1](https://github.com/Endika/flipper-wifi-census/compare/v0.1.0...v0.1.1) (2026-09-19)


### Features

* add binary capture codec and CSV export ([170186a](https://github.com/Endika/flipper-wifi-census/commit/170186aafcb743745210fd39c066f9e3befe0128))
* add capture comparison and known-device registry ([4ee4c15](https://github.com/Endika/flipper-wifi-census/commit/4ee4c1533d82759368b0b412095666eb8cfd5cae))
* add device observation and signature domain model ([16dabcd](https://github.com/Endika/flipper-wifi-census/commit/16dabcd00d966d0d8a218398e156e1b969c2754e))
* add furi serial, storage and clock adapters ([d3f1509](https://github.com/Endika/flipper-wifi-census/commit/d3f150900d5b52ec0b475a195dfdf83fac1315fd))
* add scan, capture, compare and known application services ([09b147c](https://github.com/Endika/flipper-wifi-census/commit/09b147ccc4f3c1ea8cedb2017674069c4d2b03fb))
* add scene-manager UI with scan, files, compare, known and settings ([6ae4bc1](https://github.com/Endika/flipper-wifi-census/commit/6ae4bc1f5891528e0180f4072bb437b0ca7bdd95))
* deduplicate observations into a unique-device census ([69531bc](https://github.com/Endika/flipper-wifi-census/commit/69531bc3cdda277a8e2a7b5574bd4ef85dd9d677))
* parse Marauder probe-sniff serial lines into observations ([643b56f](https://github.com/Endika/flipper-wifi-census/commit/643b56f23106cf238483e3dca3b66a0e512d2087))


### Bug Fixes

* bound Marauder RSSI/channel digits to avoid integer overflow ([c1924c4](https://github.com/Endika/flipper-wifi-census/commit/c1924c4163515ce65e5d733faf599db9e29756bd))
* heap-allocate known-db buffer to avoid FAP stack overflow ([928ec01](https://github.com/Endika/flipper-wifi-census/commit/928ec013d3e8ba2d6381f12c52d6a3329bd18cb9))
