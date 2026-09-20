// Host tool: cross two or more captures and print who is in which, past the device ceiling the
// Flipper has to keep. Accepts .wcen captures and raw-802.11 .pcap files. Build: `make tool`.
//
//   wc_compare monday.wcen friday.wcen > common.csv
//   wc_compare mon.pcap tue.pcap wed.pcap > seen.csv
//
// Matches are high confidence by construction: an identical stable MAC, or a directed SSID
// shared between non-AP devices with at least one randomized side.

#include "include/domain/wc_compare.h"
#include "tools/wc_load.h"

#define WC_COMPARE_MAX_FILES 12

int main(int argc, char **argv) {
    const int n = argc - 1;
    if (n < 2 || n > WC_COMPARE_MAX_FILES) {
        fprintf(stderr, "usage: %s a b [c ...] > seen.csv   (2 to %d captures)\n", argv[0],
                WC_COMPARE_MAX_FILES);
        return 2;
    }

    WcCensus *c = calloc((size_t)n, sizeof(WcCensus));
    if (!c) {
        return 1;
    }
    for (int i = 0; i < n; i++) {
        wc_census_init(&c[i]);
        wc_census_set_max(&c[i], WC_TOOL_MAX_DEVICES);
        if (!wc_tool_load_file(argv[i + 1], &c[i])) {
            fprintf(stderr, "! %s is not a .wcen or a supported .pcap\n", argv[i + 1]);
            return 1;
        }
        wc_tool_report_dropped(argv[i + 1], &c[i]);
        WcCensusStats s = wc_census_stats(&c[i]);
        fprintf(stderr, "%d: %-28s %5u devices (%u randomized, %u crossable)\n", i, argv[i + 1],
                s.total, s.random_count, s.unique_stable);
    }

    fputs("\n   in both", stderr);
    for (int b = 1; b < n; b++) {
        fprintf(stderr, "%6d", b);
    }
    fputc('\n', stderr);
    for (int a = 0; a < n - 1; a++) {
        fprintf(stderr, "%10d", a);
        for (int b = 1; b < n; b++) {
            if (b <= a) {
                fputs("      ", stderr);
                continue;
            }
            uint16_t both = 0;
            for (uint16_t i = 0; i < c[a].count; i++) {
                if (wc_compare_find(&c[b], &c[a].devices[i], NULL, NULL)) {
                    both++;
                }
            }
            fprintf(stderr, "%6u", both);
        }
        fputc('\n', stderr);
    }

    // One row per device of the first capture that turns up in at least one other: the regulars
    // of a place, which is the question several captures are taken to answer.
    printf("mac,random,seen_in,captures\n");
    uint32_t regulars = 0, everywhere = 0;
    for (uint16_t i = 0; i < c[0].count; i++) {
        const WcSignature *d = &c[0].devices[i];
        char where[WC_COMPARE_MAX_FILES * 3 + 1];
        size_t w = 0;
        uint16_t seen = 1;
        w += (size_t)snprintf(where + w, sizeof(where) - w, "0");
        for (int b = 1; b < n; b++) {
            if (wc_compare_find(&c[b], d, NULL, NULL)) {
                seen++;
                w += (size_t)snprintf(where + w, sizeof(where) - w, " %d", b);
            }
        }
        if (seen < 2) {
            continue;
        }
        regulars++;
        if (seen == n) {
            everywhere++;
        }
        printf("%02X:%02X:%02X:%02X:%02X:%02X,%d,%u,%s\n", d->mac[0], d->mac[1], d->mac[2],
               d->mac[3], d->mac[4], d->mac[5], d->mac_random ? 1 : 0, seen, where);
    }
    fprintf(stderr, "\n%u devices of capture 0 turn up again; %u are in all %d\n", regulars,
            everywhere, n);

    for (int i = 0; i < n; i++) {
        wc_census_free(&c[i]);
    }
    free(c);
    return 0;
}
