#pragma once

#include "include/domain/wc_capture_codec.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// Persist a census as "<basename>.wcen" (binary) plus a "<basename>.csv" sidecar for the PC.
// Returns false if the basename is too long or either write fails.
bool wc_capture_service_save(const WcStorePort *store, const char *basename,
                             const WcCaptureMeta *meta, const WcCensus *c);

// Load a binary capture file (name including the .wcen extension) into meta + census.
bool wc_capture_service_load(const WcStorePort *store, const char *filename, WcCaptureMeta *meta,
                             WcCensus *c);
