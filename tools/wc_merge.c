// Host tool: merge many capture files into one census, printed as CSV on stdout. The Flipper
// caps a single scan/merge at WC_CENSUS_MAX_DEVICES for RAM reasons; on a PC there is no such
// limit, so you can capture a big venue as several 100-device files and combine them all here.
// Accepts .wcen captures and raw-802.11 .pcap files, mixed. Build: `make tool`.
//
//   wc_merge part1.wcen part2.wcen scan.pcap ... > all.csv

#include "tools/wc_load.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s file1.wcen [file2.wcen|scan.pcap ...] > all.csv\n", argv[0]);
        return 2;
    }
    WcCensus *acc = malloc(sizeof(WcCensus));
    if (!acc) {
        return 1;
    }
    wc_census_init(acc);
    wc_census_set_max(acc, WC_TOOL_MAX_DEVICES);

    for (int i = 1; i < argc; i++) {
        WcCensus part;
        wc_census_init(&part);
        wc_census_set_max(&part, WC_TOOL_MAX_DEVICES);
        if (wc_tool_load_file(argv[i], &part)) {
            wc_census_merge(acc, &part);
            fprintf(stderr, "+ %s (%u devices)\n", argv[i], part.count);
        } else {
            fprintf(stderr, "! skipped %s (not a .wcen or supported .pcap)\n", argv[i]);
        }
        wc_census_free(&part);
    }

    WcCensusStats s = wc_census_stats(acc);
    if (acc->dropped > 0) {
        fprintf(stderr,
                "! %u devices were DROPPED at the ceiling - this should never happen"
                " off-device; report it\n",
                acc->dropped);
    }
    fprintf(stderr, "= merged: %u devices (stable %u, random %u), networks sought: %u\n", s.total,
            s.unique_stable, s.random_count, s.networks);

    size_t csv_len = wc_capture_to_csv(NULL, 0, &(WcCaptureMeta){0}, acc);
    char *csv = malloc(csv_len + 1);
    if (csv) {
        wc_capture_to_csv(csv, csv_len + 1, &(WcCaptureMeta){0}, acc);
        fputs(csv, stdout);
        free(csv);
    }
    wc_census_free(acc);
    free(acc);
    return 0;
}
