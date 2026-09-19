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

bool wc_merge_service_run(const WcStorePort *store, const char *name_a, const char *name_b,
                          const char *out_basename) {
    WcCensus *a = malloc(sizeof(WcCensus));
    WcCensus *b = malloc(sizeof(WcCensus));
    bool ok = false;
    if (a && b) {
        WcCaptureMeta ma, mb;
        if (wc_capture_service_load(store, name_a, &ma, a) &&
            wc_capture_service_load(store, name_b, &mb, b)) {
            wc_census_merge(a, b);
            free(b); // b is folded into a; free it before the (allocating) save to cut peak RAM
            b = NULL;

            WcCaptureMeta out;
            memset(&out, 0, sizeof(out));
            strncpy(out.label, out_basename, WC_LABEL_MAX);
            out.epoch = min_u32(ma.epoch, mb.epoch);
            uint32_t end = max_u32(ma.epoch + ma.duration_s, mb.epoch + mb.duration_s);
            out.duration_s = (end > out.epoch) ? (end - out.epoch) : 0;
            out.channels_mask = ma.channels_mask | mb.channels_mask;
            out.mode = 0;
            ok = wc_capture_service_save(store, out_basename, &out, a);
        }
    }
    free(a);
    free(b);
    return ok;
}
