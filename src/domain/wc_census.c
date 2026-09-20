#include "include/domain/wc_census.h"

#include <stdlib.h>
#include <string.h>

void wc_census_init(WcCensus *c) {
    c->devices = NULL;
    c->count = 0;
    c->capacity = 0;
    c->dropped = 0;
    c->max = WC_CENSUS_MAX_DEVICES;
}

// cppcheck-suppress unusedFunction // used by the off-device tools (not in the FAP link set)
void wc_census_set_max(WcCensus *c, uint16_t max) {
    c->max = max;
}

void wc_census_free(WcCensus *c) {
    free(c->devices);
    wc_census_init(c);
}

// Ensure room for one more device, growing the array in chunks up to the ceiling.
static bool ensure_one(WcCensus *c) {
    if (c->count < c->capacity) {
        return true;
    }
    if (c->capacity >= c->max) {
        return false;
    }
    uint16_t newcap = c->capacity + WC_CENSUS_GROW;
    if (newcap > c->max) {
        newcap = c->max;
    }
    WcSignature *nd = realloc(c->devices, (size_t)newcap * sizeof(WcSignature));
    if (!nd) {
        return false;
    }
    c->devices = nd;
    c->capacity = newcap;
    return true;
}

WcSignature *wc_census_add(WcCensus *c, const WcSignature *sig) {
    if (!ensure_one(c)) {
        c->dropped++;
        return NULL;
    }
    c->devices[c->count] = *sig;
    return &c->devices[c->count++];
}

bool wc_census_reserve(WcCensus *c, uint16_t n) {
    if (n > c->max) {
        n = c->max;
    }
    if (n <= c->capacity) {
        return true;
    }
    WcSignature *nd = realloc(c->devices, (size_t)n * sizeof(WcSignature));
    if (!nd) {
        return false;
    }
    c->devices = nd;
    c->capacity = n;
    return true;
}

static WcSignature *find_by_mac(WcCensus *c, const uint8_t mac[6]) {
    for (uint16_t i = 0; i < c->count; i++) {
        if (memcmp(c->devices[i].mac, mac, 6) == 0) {
            return &c->devices[i];
        }
    }
    return NULL;
}

// A client device (not an AP) that already knows this directed SSID.
static WcSignature *find_client_by_ssid(WcCensus *c, const char *ssid) {
    if (ssid[0] == '\0') {
        return NULL;
    }
    for (uint16_t i = 0; i < c->count; i++) {
        if (c->devices[i].type == WcDeviceAp) {
            continue;
        }
        if (wc_signature_has_ssid(&c->devices[i], ssid)) {
            return &c->devices[i];
        }
    }
    return NULL;
}

WcSignature *wc_census_observe(WcCensus *c, const WcObservation *obs, uint32_t now) {
    WcSignature *existing = find_by_mac(c, obs->mac);
    // SSID linking only re-attaches a ROTATING (randomized) MAC to a device already seen; a
    // stable MAC is its own identity and must never be merged with another by a shared SSID.
    if (!existing && obs->mac_random && !obs->is_beacon) {
        existing = find_client_by_ssid(c, obs->probed_ssid);
    }
    if (existing) {
        wc_signature_merge(existing, obs, now);
        return existing;
    }
    if (!ensure_one(c)) {
        c->dropped++;
        return NULL;
    }
    WcSignature *slot = &c->devices[c->count++];
    wc_signature_from_obs(slot, obs, now);
    return slot;
}

bool wc_census_phone_bound(const WcCensus *c, uint16_t *min_phones, uint16_t *max_phones) {
    uint16_t randoms = 0, distinct = 0;
    bool any_fingerprint = false;
    for (uint16_t i = 0; i < c->count; i++) {
        const WcSignature *d = &c->devices[i];
        if (!d->mac_random) {
            continue;
        }
        randoms++;
        if (d->ie_hash == 0) {
            continue; // no frame-level data for this one: it can only count as its own phone
        }
        any_fingerprint = true;
        bool seen = false;
        for (uint16_t j = 0; j < i; j++) {
            if (c->devices[j].mac_random && c->devices[j].ie_hash == d->ie_hash) {
                seen = true;
                break;
            }
        }
        if (!seen) {
            distinct++;
        }
    }
    if (!any_fingerprint) {
        return false;
    }
    // A randomized device with no fingerprint cannot be folded into any group, so it raises
    // the floor by one on its own.
    for (uint16_t i = 0; i < c->count; i++) {
        if (c->devices[i].mac_random && c->devices[i].ie_hash == 0) {
            distinct++;
        }
    }
    *min_phones = distinct;
    *max_phones = randoms;
    return true;
}

WcCensusStats wc_census_stats(const WcCensus *c) {
    WcCensusStats s;
    memset(&s, 0, sizeof(s));
    s.total = c->count;
    s.dropped = c->dropped;
    for (uint16_t i = 0; i < c->count; i++) {
        const WcSignature *d = &c->devices[i];
        if (d->mac_random) {
            s.random_count++;
        } else {
            s.unique_stable++;
        }
        if ((unsigned)d->type < WcDeviceTypeCount) {
            s.by_type[d->type]++;
        }
    }
    if (s.total > 0) {
        s.pct_random = (uint8_t)((s.random_count * 100u) / s.total);
    }
    s.networks = wc_census_ssid_tally(c, NULL, 0);
    return s;
}

uint16_t wc_census_ssid_tally(const WcCensus *c, WcSsidTally *out, uint16_t cap) {
    uint16_t distinct = 0;
    for (uint16_t i = 0; i < c->count; i++) {
        const WcSignature *d = &c->devices[i];
        for (uint8_t j = 0; j < d->ssid_count; j++) {
            const char *ssid = d->ssids[j];
            // Already tallied this SSID (from an earlier device or slot)?
            bool seen = false;
            for (uint16_t pi = 0; pi < i && !seen; pi++) {
                if (wc_signature_has_ssid(&c->devices[pi], ssid)) {
                    seen = true;
                }
            }
            for (uint8_t pj = 0; pj < j && !seen; pj++) {
                if (strncmp(d->ssids[pj], ssid, WC_SSID_MAX_LEN) == 0) {
                    seen = true;
                }
            }
            if (seen) {
                continue;
            }
            uint16_t devices = 0;
            for (uint16_t k = 0; k < c->count; k++) {
                if (wc_signature_has_ssid(&c->devices[k], ssid)) {
                    devices++;
                }
            }
            if (out != NULL && distinct < cap) {
                strncpy(out[distinct].ssid, ssid, WC_SSID_MAX_LEN);
                out[distinct].ssid[WC_SSID_MAX_LEN] = '\0';
                out[distinct].devices = devices;
            }
            distinct++;
        }
    }
    return distinct;
}

// Find a dst device that a src device should merge into: a stable MAC matches by MAC; a
// randomized src device matches by any shared directed SSID with a non-AP dst device.
static WcSignature *find_merge_target(WcCensus *dst, const WcSignature *sd) {
    // Exact MAC equality is the same device regardless of the random bit — try it first, as a
    // live session's dedup does, so merging same-day captures does not double-count a phone
    // that kept one randomized MAC. Only then fall back to shared-SSID linking (randoms).
    WcSignature *m = find_by_mac(dst, sd->mac);
    if (m) {
        return m;
    }
    if (sd->mac_random) {
        for (uint8_t j = 0; j < sd->ssid_count; j++) {
            m = find_client_by_ssid(dst, sd->ssids[j]);
            if (m) {
                return m;
            }
        }
    }
    return NULL;
}

void wc_census_merge(WcCensus *dst, const WcCensus *src) {
    // Reserve once for the worst case so appends below never trigger realloc growth spikes.
    wc_census_reserve(dst, (uint16_t)(dst->count + src->count));
    for (uint16_t i = 0; i < src->count; i++) {
        const WcSignature *sd = &src->devices[i];
        WcSignature *m = find_merge_target(dst, sd);
        if (m) {
            wc_signature_absorb(m, sd);
        } else {
            wc_census_add(dst, sd); // grows; bumps dropped at the ceiling
        }
    }
}
