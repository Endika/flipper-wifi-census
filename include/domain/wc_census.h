#pragma once

#include "include/domain/wc_observation.h"
#include "include/domain/wc_signature.h"

#define WC_CENSUS_MAX_DEVICES 128 // heap-held table; bounds memory in a busy environment

// The set of unique devices built up over one scan session.
typedef struct {
    WcSignature devices[WC_CENSUS_MAX_DEVICES];
    uint16_t count;
    uint16_t dropped; // observations discarded because the table was full
} WcCensus;

typedef struct {
    uint16_t total;         // == count
    uint16_t unique_stable; // devices with a stable (non-random) MAC
    uint16_t random_count;  // devices with a randomized MAC (not linkable across sessions)
    uint8_t pct_random;     // random_count as a percentage of total
    uint16_t by_type[5];    // indexed by WcDeviceType
    uint16_t networks;      // distinct directed SSIDs devices are probing for
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
