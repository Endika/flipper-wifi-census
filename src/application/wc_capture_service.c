#include "include/application/wc_capture_service.h"

#include "include/application/wc_files.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool wc_capture_service_save(const WcStorePort *store, const char *basename,
                             const WcCaptureMeta *meta, const WcCensus *c) {
    if (basename == NULL || strlen(basename) == 0 || strlen(basename) > WC_BASENAME_MAX) {
        return false;
    }
    char name[WC_BASENAME_MAX + 8];

    // Binary capture, streamed record-by-record so a big census never needs a whole-file
    // buffer (which would double its RAM and OOM the FAP).
    snprintf(name, sizeof(name), "%s%s", basename, WC_CAP_EXT);
    WcFileWriter *w = store->open_write(store->self, name);
    if (!w) {
        return false;
    }
    uint8_t rec[256]; // >= header size and record size
    bool ok = true;
    wc_capture_put_header(rec, meta, c->count);
    ok = store->write(w, rec, wc_capture_header_size());
    for (uint16_t i = 0; ok && i < c->count; i++) {
        wc_capture_put_record(rec, &c->devices[i]);
        ok = store->write(w, rec, wc_capture_record_size());
    }
    ok = store->close(w) && ok;
    if (!ok) {
        return false;
    }

    // CSV sidecar, streamed row-by-row.
    snprintf(name, sizeof(name), "%s%s", basename, WC_CSV_EXT);
    w = store->open_write(store->self, name);
    if (!w) {
        return false;
    }
    char row[512];
    wc_capture_csv_header(row, sizeof(row));
    ok = store->write(w, (const uint8_t *)row, strlen(row));
    for (uint16_t i = 0; ok && i < c->count; i++) {
        wc_capture_csv_row(row, sizeof(row), &c->devices[i]);
        ok = store->write(w, (const uint8_t *)row, strlen(row));
    }
    ok = store->close(w) && ok;
    return ok;
}

uint16_t wc_capture_service_device_count(const WcStorePort *store, const char *filename) {
    uint8_t head[64]; // >= wc_capture_header_size()
    size_t n = wc_capture_header_size();
    if (n > sizeof(head)) {
        return 0;
    }
    // read_file refuses a buffer smaller than the file, so size the read to the whole file and
    // fall back to reading just what the header needs.
    size_t size = store->file_size(store->self, filename);
    if (size == 0 || size < n) {
        return 0;
    }
    uint8_t *buf = malloc(size);
    if (!buf) {
        return 0;
    }
    uint16_t count = 0;
    if (store->read_file(store->self, filename, buf, size) == size) {
        count = wc_capture_peek_count(buf, size);
    }
    free(buf);
    return count;
}

bool wc_capture_service_load(const WcStorePort *store, const char *filename, WcCaptureMeta *meta,
                             WcCensus *c) {
    size_t size = store->file_size(store->self, filename);
    if (size == 0) {
        return false;
    }
    uint8_t *buf = malloc(size);
    if (!buf) {
        return false;
    }
    size_t n = store->read_file(store->self, filename, buf, size);
    bool ok = (n > 0) && wc_capture_read(meta, c, buf, n);
    free(buf);
    return ok;
}
