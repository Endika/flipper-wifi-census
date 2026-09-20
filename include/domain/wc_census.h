#pragma once

#include "include/domain/wc_observation.h"
#include "include/domain/wc_signature.h"

#define WC_CENSUS_MAX_DEVICES 100 // safety ceiling (bounds FAP heap; scan/merge hold this much)
#define WC_CENSUS_GROW 16         // device-array growth chunk

// The set of unique devices seen over one scan session. The device array grows on demand
// (realloc), so RAM tracks the real device count — a quiet room costs a few hundred bytes, a
// packed venue grows toward the ceiling. Always pair wc_census_init with wc_census_free.
typedef struct {
    WcSignature *devices;
    uint16_t count;
    uint16_t capacity;
    uint16_t dropped; // observations discarded at the ceiling / on allocation failure
    uint16_t max; // per-census ceiling (WC_CENSUS_MAX_DEVICES on the Flipper; higher off-device)
} WcCensus;

typedef struct {
    uint16_t total;         // == count
    uint16_t unique_stable; // devices with a stable (non-random) MAC
    uint16_t random_count;  // devices with a randomized MAC (not linkable across sessions)
    uint8_t pct_random;     // random_count as a percentage of total
    uint16_t by_type[WcDeviceTypeCount]; // indexed by WcDeviceType
    uint16_t networks;                   // distinct directed SSIDs devices are probing for
} WcCensusStats;

// One "network a device is looking for": a directed probe SSID and how many devices sought it.
typedef struct {
    char ssid[WC_SSID_MAX_LEN + 1];
    uint16_t devices;
} WcSsidTally;

// Tally the distinct directed SSIDs across the census. With out != NULL, fills up to `cap`
// entries (SSID + device count). Returns the number of distinct SSIDs (may exceed `cap`;
// pass out=NULL, cap=0 to only count).
uint16_t wc_census_ssid_tally(const WcCensus *c, WcSsidTally *out, uint16_t cap);

void wc_census_init(WcCensus *c);

// Raise (or lower) this census's device ceiling. Default after init is WC_CENSUS_MAX_DEVICES,
// which suits the Flipper's RAM; an off-device tool (e.g. merging many captures on a PC) can
// set it much higher. Call right after wc_census_init, before adding devices.
void wc_census_set_max(WcCensus *c, uint16_t max);

// Release the device array. Safe to call on a zeroed/empty census; leaves it re-init'd.
void wc_census_free(WcCensus *c);

// Append a copy of `sig` (growing the array). Returns the stored signature, or NULL at the
// ceiling / on allocation failure (bumps dropped). Used by the codec and tests.
WcSignature *wc_census_add(WcCensus *c, const WcSignature *sig);

// Pre-allocate room for `n` devices (capped at the ceiling) in one shot. Doing this before a
// scan or a load avoids repeated realloc growth — which transiently needs ~2x the array and
// can exhaust the FAP heap on a busy scan. Returns false on allocation failure.
bool wc_census_reserve(WcCensus *c, uint16_t n);

// Fold one observation into the census, deduping into an existing device or adding a new
// one. Dedup rules, in order:
//   1. exact MAC match -> same device (repeated frames from the current MAC);
//   2. else, a client probe carrying a directed SSID already recorded on an existing
//      client device -> linked to it (collapses MAC rotation and shared-SSID clients);
//   3. else -> a new device.
// Rule 2 never links into an access point, and randomized MACs with no directed SSID are
// never merged across MACs (counted separately, reported as not-linkable).
// Returns the new/updated signature, or NULL if the table was full.
WcSignature *wc_census_observe(WcCensus *c, const WcObservation *obs, uint32_t now);

WcCensusStats wc_census_stats(const WcCensus *c);

// Merge every device of `src` into `dst` using the same dedup rules as a live session (same
// stable MAC, or a shared directed SSID for a randomized device), accumulating stats. Used
// to combine captures of one place taken on different days. Overflow bumps dst->dropped.
void wc_census_merge(WcCensus *dst, const WcCensus *src);
