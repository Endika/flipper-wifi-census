#pragma once

#include "include/domain/wc_known.h"
#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// Load the known-devices registry. A missing file is not an error: `db` comes back empty.
// Returns false only if a file exists but is corrupt.
bool wc_known_service_load(const WcStorePort *store, WcKnownDb *db);

bool wc_known_service_save(const WcStorePort *store, const WcKnownDb *db);

// Mark a device (from a capture) as known: load, derive a rule, append, save. Returns false
// if the device yields no durable rule (randomized MAC with no directed SSID) or the db is
// full or a write fails.
bool wc_known_service_mark(const WcStorePort *store, const WcSignature *sig, const char *label);
