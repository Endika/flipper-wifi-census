// Host tool: turn a raw-802.11 (linktype 105) pcap of probe requests into a WiFi Census,
// printed as CSV on stdout. Lets a laptop without a Flipper build a census from a capture
// taken by Marauder, Kismet, airodump, etc. Build: `make tool` (see Makefile).
//
//   wc_import capture.pcap > census.csv

#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"
#include "include/domain/wc_pcap_reader.h"
#include "include/domain/wc_probe_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void on_frame(void *ctx, const uint8_t *frame, size_t len) {
    WcObservation o;
    if (wc_parse_probe_frame(frame, len, &o)) {
        wc_census_observe(ctx, &o, 0);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s capture.pcap > census.csv\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0) {
        fclose(f);
        fprintf(stderr, "empty file\n");
        return 1;
    }
    uint8_t *buf = malloc((size_t)size);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        fprintf(stderr, "read error\n");
        return 1;
    }
    fclose(f);

    WcCensus *c = malloc(sizeof(WcCensus));
    if (!c) {
        free(buf);
        return 1;
    }
    wc_census_init(c);
    if (!wc_pcap_read(buf, (size_t)size, on_frame, c)) {
        fprintf(stderr, "not a supported pcap (need classic libpcap, linktype 105 IEEE802.11)\n");
        free(buf);
        free(c);
        return 1;
    }

    WcCensusStats s = wc_census_stats(c);
    fprintf(stderr, "devices: %u (stable %u, random %u), networks sought: %u\n", s.total,
            s.unique_stable, s.random_count, s.networks);

    size_t csv_len = wc_capture_to_csv(NULL, 0, &(WcCaptureMeta){0}, c);
    char *csv = malloc(csv_len + 1);
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "%s", argv[1]);
    wc_capture_to_csv(csv, csv_len + 1, &meta, c);
    fputs(csv, stdout);

    free(csv);
    free(buf);
    free(c);
    return 0;
}
