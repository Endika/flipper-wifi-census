// Host tool: merge many capture files into one census, printed as CSV on stdout. The Flipper
// caps a single scan/merge at WC_CENSUS_MAX_DEVICES for RAM reasons; on a PC there is no such
// limit, so you can capture a big venue as several 320-device files and combine them all here.
// Accepts .wcen captures and raw-802.11 .pcap files, mixed. Build: `make tool`.
//
//   wc_merge part1.wcen part2.wcen scan.pcap ... > all.csv

#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"
#include "include/domain/wc_pcap_reader.h"
#include "include/domain/wc_probe_frame.h"

#include <stdio.h>
#include <stdlib.h>

#define WC_MERGE_MAX_DEVICES 60000 // off-device ceiling: effectively "all of them"

static void on_frame(void *ctx, const uint8_t *frame, size_t len) {
    WcObservation o;
    if (wc_parse_probe_frame(frame, len, &o)) {
        wc_census_observe(ctx, &o, 0);
    }
}

// Load one file (either a .wcen capture or a .pcap) into `into`. Returns true if recognised.
static bool load_file(const char *path, WcCensus *into) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    bool ok = false;
    if (size > 0) {
        uint8_t *buf = malloc((size_t)size);
        if (buf && fread(buf, 1, (size_t)size, f) == (size_t)size) {
            WcCaptureMeta meta;
            if (wc_capture_read(&meta, into, buf, (size_t)size)) {
                ok = true; // a .wcen capture
            } else {
                wc_census_init(into);
                wc_census_set_max(into, WC_MERGE_MAX_DEVICES);
                ok = wc_pcap_read(buf, (size_t)size, on_frame, into); // a raw pcap
            }
        }
        free(buf);
    }
    fclose(f);
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s file1.wcen [file2.wcen|scan.pcap ...] > all.csv\n", argv[0]);
        return 2;
    }
    WcCensus *acc = malloc(sizeof(WcCensus));
    wc_census_init(acc);
    wc_census_set_max(acc, WC_MERGE_MAX_DEVICES);

    for (int i = 1; i < argc; i++) {
        WcCensus part;
        wc_census_init(&part);
        wc_census_set_max(&part, WC_MERGE_MAX_DEVICES);
        if (load_file(argv[i], &part)) {
            wc_census_merge(acc, &part);
            fprintf(stderr, "+ %s (%u devices)\n", argv[i], part.count);
        } else {
            fprintf(stderr, "! skipped %s (not a .wcen or supported .pcap)\n", argv[i]);
        }
        wc_census_free(&part);
    }

    WcCensusStats s = wc_census_stats(acc);
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
