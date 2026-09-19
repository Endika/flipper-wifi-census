#pragma once

#include <stddef.h>
#include <stdint.h>

// Build a default, filesystem-safe capture name from a Unix epoch (UTC):
// "cap_YYYYMMDD_HHMM" (no separators that a FAT filename dislikes). Writes into `out`
// (needs >= 20 bytes). Pure and host-testable.
void wc_default_capture_name(uint32_t epoch, char *out, size_t cap);
