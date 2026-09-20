#pragma once

#include "include/ports/wc_clock_port.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// On-device import cap. The reader allocates the file's actual size (not this whole amount), so
// small pcaps stay cheap; a file larger than this is rejected (the Flipper can't hold it) — use
// the PC tool for those. Kept conservative because a failed malloc aborts the app on the Flipper.
#define WC_IMPORT_MAX_BYTES (64 * 1024)

// Read a raw-802.11 (linktype 105) pcap file at absolute path `pcap_path` (as picked in the file
// browser), build a census from its probe requests, and save it as "<out_basename>.wcen" (+ .csv).
// Returns false if the file is missing, too big (> WC_IMPORT_MAX_BYTES — use the PC tool for
// larger), not a supported pcap, or the save fails.
bool wc_import_service_run(const WcStorePort *store, WcClockPort clock, const char *pcap_path,
                           const char *out_basename);
