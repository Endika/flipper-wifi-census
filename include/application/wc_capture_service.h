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

// Devices a stored capture declares, read from its header alone (0 if unreadable). A file with
// more than this build's ceiling is refused by the loader, and without this the refusal is
// indistinguishable from a corrupt file.
uint16_t wc_capture_service_device_count(const WcStorePort *store, const char *filename);
