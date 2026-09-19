# Changelog

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
