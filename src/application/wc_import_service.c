#include "include/application/wc_import_service.h"

#include "include/application/wc_capture_service.h"
#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"
#include "include/domain/wc_pcap_reader.h"
#include "include/domain/wc_probe_frame.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    WcCensus *census;
    uint32_t now;
} ImportCtx;

static void on_frame(void *ctx, const uint8_t *frame, size_t len) {
    WcObservation o;
    if (wc_parse_probe_frame(frame, len, &o)) {
        ImportCtx *ic = ctx;
        wc_census_observe(ic->census, &o, ic->now);
    }
}

bool wc_import_service_run(const WcStorePort *store, WcClockPort clock, const char *pcap_name,
                           const char *out_basename) {
    uint8_t *buf = malloc(WC_IMPORT_MAX_BYTES);
    WcCensus *census = malloc(sizeof(WcCensus));
    bool ok = false;
    if (buf && census) {
        size_t n = store->read_file(store->self, pcap_name, buf, WC_IMPORT_MAX_BYTES);
        if (n > 0) {
            wc_census_init(census);
            ImportCtx ic = {.census = census, .now = wc_clock_now(&clock)};
            if (wc_pcap_read(buf, n, on_frame, &ic)) {
                WcCaptureMeta meta;
                memset(&meta, 0, sizeof(meta));
                strncpy(meta.label, out_basename, WC_LABEL_MAX);
                meta.epoch = ic.now;
                ok = wc_capture_service_save(store, out_basename, &meta, census);
            }
            wc_census_free(census);
        }
    }
    free(buf);
    free(census);
    return ok;
}
