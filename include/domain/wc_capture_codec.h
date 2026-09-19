#pragma once

#include "include/domain/wc_census.h"

#include <stddef.h>

#define WC_LABEL_MAX 32
#define WC_CAP_VERSION 1

// Capture metadata written alongside the device records. `channels_mask` has bit (ch-1) set
// for each 2.4 GHz channel scanned (1..14); `mode` is reserved (0 = census).
typedef struct {
    char label[WC_LABEL_MAX + 1];
    uint32_t epoch;
    uint32_t duration_s;
    uint16_t channels_mask;
    uint8_t mode;
} WcCaptureMeta;

// Exact byte size a binary capture of `c` will occupy.
size_t wc_capture_size(const WcCensus *c);

// Serialize meta + census into `buf` (little-endian, endian-independent). Returns bytes
// written, or 0 if `cap` is too small.
size_t wc_capture_write(uint8_t *buf, size_t cap, const WcCaptureMeta *meta, const WcCensus *c);

// Parse a binary capture. Validates magic, version, and that the declared record count fits
// the buffer exactly; rejects truncated or malformed input without reading out of bounds.
// Returns true on success.
bool wc_capture_read(WcCaptureMeta *meta, WcCensus *c, const uint8_t *buf, size_t len);

// Render a human-readable CSV of the capture into `out` (NUL-terminated). Returns the length
// that a full render needs (excluding the NUL); if it is >= `cap` the output was truncated.
size_t wc_capture_to_csv(char *out, size_t cap, const WcCaptureMeta *meta, const WcCensus *c);
