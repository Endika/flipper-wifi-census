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

    // Binary capture.
    size_t bin_len = wc_capture_size(c);
    uint8_t *bin = malloc(bin_len);
    if (!bin) {
        return false;
    }
    bool ok = (wc_capture_write(bin, bin_len, meta, c) == bin_len);
    if (ok) {
        char name[WC_BASENAME_MAX + 8];
        snprintf(name, sizeof(name), "%s%s", basename, WC_CAP_EXT);
        ok = store->write_file(store->self, name, bin, bin_len);
    }
    free(bin);
    if (!ok) {
        return false;
    }

    // CSV sidecar: size it, render it, write it.
    size_t csv_len = wc_capture_to_csv(NULL, 0, meta, c);
    char *csv = malloc(csv_len + 1);
    if (!csv) {
        return false;
    }
    wc_capture_to_csv(csv, csv_len + 1, meta, c);
    char name[WC_BASENAME_MAX + 8];
    snprintf(name, sizeof(name), "%s%s", basename, WC_CSV_EXT);
    ok = store->write_file(store->self, name, (const uint8_t *)csv, csv_len);
    free(csv);
    return ok;
}

bool wc_capture_service_load(const WcStorePort *store, const char *filename, WcCaptureMeta *meta,
                             WcCensus *c) {
    size_t cap = wc_capture_max_size();
    uint8_t *buf = malloc(cap);
    if (!buf) {
        return false;
    }
    size_t n = store->read_file(store->self, filename, buf, cap);
    bool ok = (n > 0) && wc_capture_read(meta, c, buf, n);
    free(buf);
    return ok;
}
