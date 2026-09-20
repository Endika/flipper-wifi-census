#include "include/application/wc_merge_service.h"

#include "include/application/wc_capture_service.h"
#include "include/domain/wc_capture_codec.h"
#include "include/domain/wc_census.h"

#include <stdlib.h>
#include <string.h>

static uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}
static uint32_t max_u32(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

// Fold `name` into `into`, a record at a time straight off the SD. Loading it into a census of
// its own first held that file and its devices in RAM alongside the first census - the peak
// that pins the device ceiling.
static bool merge_from_file(const WcStorePort *store, const char *name, WcCensus *into,
                            WcCaptureMeta *meta) {
    uint8_t head[64];
    const size_t hdr = wc_capture_header_size();
    if (hdr > sizeof(head) || store->read_range(store->self, name, 0, head, hdr) != hdr) {
        return false;
    }
    uint16_t count = 0, version = 0;
    if (!wc_capture_get_header(head, hdr, meta, &count, &version)) {
        return false;
    }
    uint8_t buf[256];
    const size_t rec = wc_capture_record_size();
    if (rec > sizeof(buf)) {
        return false;
    }
    for (uint16_t i = 0; i < count; i++) {
        WcSignature d;
        if (store->read_range(store->self, name, hdr + (size_t)i * rec, buf, rec) != rec ||
            !wc_capture_get_record(buf, rec, version, &d)) {
            return false;
        }
        wc_census_merge_one(into, &d);
    }
    return true;
}

bool wc_merge_service_run(const WcStorePort *store, const char *name_a, const char *name_b,
                          const char *out_basename, uint16_t *out_devices, uint16_t *out_dropped) {
    WcCensus *a = malloc(sizeof(WcCensus));
    bool ok = false;
    if (a) {
        wc_census_init(a);
        WcCaptureMeta ma, mb;
        if (wc_capture_service_load(store, name_a, &ma, a) &&
            merge_from_file(store, name_b, a, &mb)) {

            WcCaptureMeta out;
            memset(&out, 0, sizeof(out));
            strncpy(out.label, out_basename, WC_LABEL_MAX);
            out.epoch = min_u32(ma.epoch, mb.epoch);
            uint32_t end = max_u32(ma.epoch + ma.duration_s, mb.epoch + mb.duration_s);
            out.duration_s = (end > out.epoch) ? (end - out.epoch) : 0;
            out.channels_mask = ma.channels_mask | mb.channels_mask;
            out.mode = 0;
            ok = wc_capture_service_save(store, out_basename, &out, a);
            if (ok && out_devices) {
                *out_devices = a->count;
            }
            if (ok && out_dropped) {
                *out_dropped = a->dropped;
            }
        }
    }
    if (a) {
        wc_census_free(a);
    }
    free(a);
    return ok;
}
