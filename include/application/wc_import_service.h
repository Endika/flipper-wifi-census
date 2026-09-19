#pragma once

#include "include/ports/wc_clock_port.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

#define WC_IMPORT_MAX_BYTES (40 * 1024) // on-device import cap; larger pcaps -> use the PC tool

// Read a raw-802.11 (linktype 105) pcap file `pcap_name` from storage, build a census from
// its probe requests, and save it as "<out_basename>.wcen" (+ .csv). Returns false if the
// file is missing, too big (> WC_IMPORT_MAX_BYTES — use the PC tool for larger), not a
// supported pcap, or the save fails.
bool wc_import_service_run(const WcStorePort *store, WcClockPort clock, const char *pcap_name,
                           const char *out_basename);
