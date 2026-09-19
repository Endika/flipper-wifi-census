#include "include/domain/wc_census.h"

#include <string.h>

void wc_census_init(WcCensus *c) {
    memset(c, 0, sizeof(*c));
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
    if (!existing && !obs->is_beacon) {
        existing = find_client_by_ssid(c, obs->probed_ssid);
    }
    if (existing) {
        wc_signature_merge(existing, obs, now);
        return existing;
    }
    if (c->count >= WC_CENSUS_MAX_DEVICES) {
        c->dropped++;
        return NULL;
    }
    WcSignature *slot = &c->devices[c->count++];
    wc_signature_from_obs(slot, obs, now);
    return slot;
}

WcCensusStats wc_census_stats(const WcCensus *c) {
    WcCensusStats s;
    memset(&s, 0, sizeof(s));
    s.total = c->count;
    for (uint16_t i = 0; i < c->count; i++) {
        const WcSignature *d = &c->devices[i];
        if (d->mac_random) {
            s.random_count++;
        } else {
            s.unique_stable++;
        }
        if ((unsigned)d->type < 5) {
            s.by_type[d->type]++;
        }
    }
    if (s.total > 0) {
        s.pct_random = (uint8_t)((s.random_count * 100u) / s.total);
    }
    return s;
}
