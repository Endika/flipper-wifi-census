#include "include/domain/wc_marauder_parse.h"
#include "include/domain/wc_observation.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bool parse(const char *line, WcObservation *o) {
    return wc_parse_summary_line(line, strlen(line), o);
}

static void test_full_labeled_line(void) {
    WcObservation o;
    assert(parse("RSSI: -45 CH: 6 MAC: AC:DE:48:00:11:22 SSID: HomeNet", &o));
    const uint8_t expect[6] = {0xAC, 0xDE, 0x48, 0x00, 0x11, 0x22};
    assert(memcmp(o.mac, expect, 6) == 0);
    assert(!o.mac_random);
    assert(o.rssi == -45);
    assert(o.channel == 6);
    assert(strcmp(o.probed_ssid, "HomeNet") == 0);
    assert(!o.is_beacon);
}

static void test_wildcard_probe_random_mac(void) {
    WcObservation o;
    assert(parse("RSSI: -70 CH: 11 MAC: DA:11:22:33:44:55 SSID: ", &o));
    assert(o.mac_random);
    assert(o.rssi == -70);
    assert(o.channel == 11);
    assert(o.probed_ssid[0] == '\0'); // wildcard: no directed SSID
}

static void test_quoted_ssid_with_space(void) {
    WcObservation o;
    assert(parse("RSSI:-33 CH:1 MAC:00:1B:21:AA:BB:CC SSID:\"Cafe WiFi\"", &o));
    assert(o.rssi == -33);
    assert(o.channel == 1);
    assert(strcmp(o.probed_ssid, "Cafe WiFi") == 0);
}

static void test_lowercase_mac_no_labels(void) {
    WcObservation o;
    assert(parse("probe from b8:27:eb:aa:bb:cc", &o));
    const uint8_t expect[6] = {0xB8, 0x27, 0xEB, 0xAA, 0xBB, 0xCC};
    assert(memcmp(o.mac, expect, 6) == 0);
    assert(o.rssi == 0 && o.channel == 0);
    assert(o.probed_ssid[0] == '\0');
}

static void test_garbage_rejected(void) {
    WcObservation o;
    assert(!parse("no mac here at all", &o));
    assert(!parse("", &o));
    assert(!parse("aa:bb:cc", &o)); // partial MAC
}

static void test_ssid_over_32_truncated(void) {
    WcObservation o;
    char line[128];
    snprintf(line, sizeof(line), "MAC: 00:1B:21:00:00:01 SSID: %s",
             "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"); // 40 A's
    assert(parse(line, &o));
    assert(strlen(o.probed_ssid) == WC_SSID_MAX_LEN); // clamped to 32
}

static void test_long_line_is_safe(void) {
    WcObservation o;
    // MAC near the front of a very long line: parses fine, no overflow.
    char big[600];
    memset(big, 'X', sizeof(big));
    memcpy(big, "MAC: 00:1B:21:00:00:02 ", 23);
    big[sizeof(big) - 1] = '\0';
    assert(wc_parse_summary_line(big, strlen(big), &o));
    const uint8_t expect[6] = {0x00, 0x1B, 0x21, 0x00, 0x00, 0x02};
    assert(memcmp(o.mac, expect, 6) == 0);

    // MAC only beyond the parse window: safely truncated away -> not found, no crash.
    char big2[600];
    memset(big2, 'y', sizeof(big2));
    memcpy(big2 + 300, "00:1B:21:00:00:03", 17);
    assert(!wc_parse_summary_line(big2, sizeof(big2), &o));
}

int main(void) {
    test_full_labeled_line();
    test_wildcard_probe_random_mac();
    test_quoted_ssid_with_space();
    test_lowercase_mac_no_labels();
    test_garbage_rejected();
    test_ssid_over_32_truncated();
    test_long_line_is_safe();
    printf("test_marauder_parse: OK\n");
    return 0;
}
