#pragma once

// Shared by the host tools: load a capture file, accepting either a .wcen census or a raw
// 802.11 (linktype 105) .pcap, so every tool takes the same arguments. Off-device there is no
// RAM ceiling, so the census is opened wide.

#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"
#include "include/domain/wc_pcap_reader.h"
#include "include/domain/wc_probe_frame.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WC_TOOL_MAX_DEVICES 60000 // off-device ceiling: effectively "all of them"

static inline void wc_tool_on_frame(void *ctx, const uint8_t *frame, size_t len) {
    WcObservation o;
    if (wc_parse_probe_frame(frame, len, &o)) {
        wc_census_observe(ctx, &o, 0);
    }
}

// Say so if the ceiling ever refused a device. Off-device it must not happen; when it does the
// count is short, and silence is the worst way to learn that.
static inline void wc_tool_report_dropped(const char *what, const WcCensus *c) {
    if (c->dropped > 0) {
        fprintf(stderr, "! %s: %u devices DROPPED at the ceiling - the count is short\n", what,
                c->dropped);
    }
}

// Write a census as a .wcen, streamed record by record so a venue-sized one needs no buffer of
// its own size. Without this the host tools could only emit CSV, which nothing reads back: no
// chained merges, and no way to bring a PC-built census to the Flipper.
static inline bool wc_tool_write_wcen(const char *path, const WcCaptureMeta *meta,
                                      const WcCensus *c) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return false;
    }
    uint8_t rec[256];
    wc_capture_put_header(rec, meta, c->count);
    bool ok = fwrite(rec, 1, wc_capture_header_size(), f) == wc_capture_header_size();
    for (uint16_t i = 0; ok && i < c->count; i++) {
        wc_capture_put_record(rec, &c->devices[i]);
        ok = fwrite(rec, 1, wc_capture_record_size(), f) == wc_capture_record_size();
    }
    return (fclose(f) == 0) && ok;
}

// Loads `path` into `into` (already init'd). Returns false if the file is neither format.
// `inline` so a tool that only needs one of these two helpers still compiles clean.
static inline bool wc_tool_load_file(const char *path, WcCensus *into) {
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
                wc_census_set_max(into, WC_TOOL_MAX_DEVICES);
                ok = wc_pcap_read(buf, (size_t)size, wc_tool_on_frame, into); // a raw pcap
            }
        }
        free(buf);
    }
    fclose(f);
    return ok;
}
