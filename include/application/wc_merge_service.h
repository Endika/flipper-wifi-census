#pragma once

#include "include/ports/wc_store_port.h"

#include <stdbool.h>

// Load two capture files (names including .wcen), merge the second into the first, and save
// the result as a new capture "<out_basename>.wcen" (+ .csv). Metadata is combined: epoch =
// earliest, duration = wall-clock span covered, channels = union. Censuses are heap-held.
// Returns false if either load fails, the name is bad, or the save fails.
bool wc_merge_service_run(const WcStorePort *store, const char *name_a, const char *name_b,
                          const char *out_basename);
