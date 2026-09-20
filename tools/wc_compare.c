// Host tool: cross two captures and print what they have in common, past the device ceiling the
// Flipper has to keep. Accepts .wcen captures and raw-802.11 .pcap files. Build: `make tool`.
//
//   wc_compare monday.wcen friday.wcen > common.csv

#include "include/domain/wc_compare.h"
#include "tools/wc_load.h"

#include <string.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s a.wcen b.wcen > common.csv\n", argv[0]);
        return 2;
    }

    WcCensus *a = malloc(sizeof(WcCensus));
    WcCensus *b = malloc(sizeof(WcCensus));
    WcCompareResult *r = malloc(sizeof(WcCompareResult));
    if (!a || !b || !r) {
        return 1;
    }
    wc_census_init(a);
    wc_census_set_max(a, WC_TOOL_MAX_DEVICES);
    wc_census_init(b);
    wc_census_set_max(b, WC_TOOL_MAX_DEVICES);

    for (int i = 1; i <= 2; i++) {
        WcCensus *into = (i == 1) ? a : b;
        if (!wc_tool_load_file(argv[i], into)) {
            fprintf(stderr, "! %s is not a .wcen or a supported .pcap\n", argv[i]);
            return 1;
        }
    }

    wc_compare(r, a, b);

    uint16_t by_mac = 0, by_ssid = 0;
    for (uint16_t i = 0; i < r->match_count; i++) {
        if (r->matches[i].reason == WcMatchBySsid) {
            by_ssid++;
        } else {
            by_mac++;
        }
    }

    fprintf(stderr, "A %s: %u devices (%u randomized)\n", argv[1], r->na, r->random_a);
    fprintf(stderr, "B %s: %u devices (%u randomized)\n", argv[2], r->nb, r->random_b);
    fprintf(stderr, "in both: %u (by stable MAC %u, by shared network %u)\n", r->intersection,
            by_mac, by_ssid);
    if (r->match_count < r->intersection) {
        fprintf(stderr,
                "! only %u of the %u matches are listed: rebuild with a larger"
                " WC_COMPARE_MAX_MATCHES\n",
                r->match_count, r->intersection);
    }
    // The honest denominator: a randomized device with no directed SSID cannot be crossed at
    // all, so the intersection is a floor over the crossable part, not over everyone.
    uint16_t crossable_a = (uint16_t)(r->na - r->random_a);
    fprintf(stderr,
            "crossable in A: %u stable-MAC devices (+ randomized ones that probe a"
            " named network)\n",
            crossable_a);

    printf("mac,reason,detail\n");
    for (uint16_t i = 0; i < r->match_count; i++) {
        const WcMatch *m = &r->matches[i];
        printf("%02X:%02X:%02X:%02X:%02X:%02X,%s,%s\n", m->mac[0], m->mac[1], m->mac[2], m->mac[3],
               m->mac[4], m->mac[5], m->reason == WcMatchBySsid ? "ssid" : "mac", m->detail);
    }

    wc_census_free(a);
    wc_census_free(b);
    free(a);
    free(b);
    free(r);
    return 0;
}
