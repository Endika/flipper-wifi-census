#include "include/domain/wc_census.h"
#include "include/domain/wc_observation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static WcObservation obs_of(const uint8_t mac[6], int8_t rssi, const char *ssid, bool beacon) {
    WcObservation o;
    memset(&o, 0, sizeof(o));
    memcpy(o.mac, mac, 6);
    o.mac_random = wc_mac_is_random(mac);
    o.rssi = rssi;
    o.channel = 6;
    o.is_beacon = beacon;
    if (ssid) {
        strncpy(o.probed_ssid, ssid, WC_SSID_MAX_LEN);
    }
    return o;
}

static void test_same_mac_is_one_device(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t mac[6] = {0xB8, 0x27, 0xEB, 1, 2, 3};
    WcObservation a = obs_of(mac, -60, NULL, false);
    WcObservation b = obs_of(mac, -50, NULL, false);
    wc_census_observe(&c, &a, 1);
    wc_census_observe(&c, &b, 2);
    assert(c.count == 1);
    assert(c.devices[0].obs_count == 2);
    assert(c.devices[0].rssi_max == -50);
}

static void test_ssid_links_rotating_random_macs(void) {
    WcCensus c;
    wc_census_init(&c);
    // Same phone, two different randomized MACs, both probing the same directed SSID.
    uint8_t mac1[6] = {0xDA, 0, 0, 0, 0, 1};
    uint8_t mac2[6] = {0xDE, 0, 0, 0, 0, 2};
    WcObservation o1 = obs_of(mac1, -55, "MyHomeNet_5G", false);
    WcObservation o2 = obs_of(mac2, -57, "MyHomeNet_5G", false);
    wc_census_observe(&c, &o1, 1);
    wc_census_observe(&c, &o2, 2);
    assert(c.count == 1); // linked by shared directed SSID
}

static void test_random_without_ssid_never_merges(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t mac1[6] = {0xDA, 0, 0, 0, 0, 1};
    uint8_t mac2[6] = {0xDE, 0, 0, 0, 0, 2};
    WcObservation o1 = obs_of(mac1, -55, NULL, false); // wildcard probe
    WcObservation o2 = obs_of(mac2, -57, NULL, false);
    wc_census_observe(&c, &o1, 1);
    wc_census_observe(&c, &o2, 2);
    assert(c.count == 2); // two distinct, not-linkable devices
}

static void test_ap_not_linked_to_client_by_ssid(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t ap[6] = {0x00, 0x1A, 0x11, 1, 1, 1};
    uint8_t client[6] = {0xDA, 0, 0, 0, 0, 9};
    // A beacon carries no probed SSID in this model; a client probes a directed SSID.
    WcObservation beacon = obs_of(ap, -40, NULL, true);
    WcObservation probe = obs_of(client, -60, "CorpWiFi", false);
    wc_census_observe(&c, &beacon, 1);
    wc_census_observe(&c, &probe, 2);
    assert(c.count == 2);
    assert(c.devices[0].type == WcDeviceAp);
}

static void test_stats(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t ap[6] = {0x00, 0x1A, 0x11, 1, 1, 1};     // Apple OUI -> but beacon -> AP
    uint8_t laptop[6] = {0x00, 0x1B, 0x21, 2, 2, 2}; // Intel -> laptop
    uint8_t phone[6] = {0xDA, 0, 0, 0, 0, 1};        // random -> phone
    WcObservation ob_ap = obs_of(ap, -40, NULL, true);
    WcObservation ob_lap = obs_of(laptop, -55, NULL, false);
    WcObservation ob_ph = obs_of(phone, -60, NULL, false);
    wc_census_observe(&c, &ob_ap, 1);
    wc_census_observe(&c, &ob_lap, 1);
    wc_census_observe(&c, &ob_ph, 1);

    WcCensusStats s = wc_census_stats(&c);
    assert(s.total == 3);
    assert(s.unique_stable == 2);
    assert(s.random_count == 1);
    assert(s.pct_random == 33);
    assert(s.by_type[WcDeviceAp] == 1);
    assert(s.by_type[WcDeviceLaptop] == 1);
    assert(s.by_type[WcDevicePhone] == 1);
}

static void test_table_full_drops(void) {
    WcCensus c;
    wc_census_init(&c);
    for (int i = 0; i < WC_CENSUS_MAX_DEVICES + 5; i++) {
        uint8_t mac[6] = {0x02, 0, 0, (uint8_t)(i >> 8), (uint8_t)i, 0};
        // 0x02 sets LAA; make each unique and wildcard so none merge.
        WcObservation o = obs_of(mac, -70, NULL, false);
        wc_census_observe(&c, &o, 1);
    }
    assert(c.count == WC_CENSUS_MAX_DEVICES);
    assert(c.dropped == 5);
}

static void test_ssid_tally(void) {
    WcCensus c;
    wc_census_init(&c);
    uint8_t m1[6] = {0x00, 0x1B, 0x21, 0, 0, 1};
    uint8_t m2[6] = {0x00, 0x1B, 0x21, 0, 0, 2};
    uint8_t m3[6] = {0x00, 0x1B, 0x21, 0, 0, 3};
    WcObservation a = obs_of(m1, -50, "HomeA", false);
    WcObservation b = obs_of(m2, -50, "HomeA", false); // same net as a
    WcObservation d = obs_of(m3, -50, "HomeB", false);
    wc_census_observe(&c, &a, 1);
    wc_census_observe(&c, &b, 1);
    wc_census_observe(&c, &d, 1);

    WcCensusStats s = wc_census_stats(&c);
    assert(s.networks == 2); // HomeA, HomeB

    WcSsidTally t[8];
    uint16_t n = wc_census_ssid_tally(&c, t, 8);
    assert(n == 2);
    // HomeA sought by 2 devices, HomeB by 1 (order: first appearance)
    assert(strcmp(t[0].ssid, "HomeA") == 0 && t[0].devices == 2);
    assert(strcmp(t[1].ssid, "HomeB") == 0 && t[1].devices == 1);

    // count-only mode
    assert(wc_census_ssid_tally(&c, NULL, 0) == 2);
}

int main(void) {
    test_same_mac_is_one_device();
    test_ssid_tally();
    test_ssid_links_rotating_random_macs();
    test_random_without_ssid_never_merges();
    test_ap_not_linked_to_client_by_ssid();
    test_stats();
    test_table_full_drops();
    printf("test_census: OK\n");
    return 0;
}
