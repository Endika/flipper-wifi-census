#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Called once per stored file name during a listing. `name` is valid only for the call.
typedef void (*WcStoreNameFn)(void *ctx, const char *name);

// Port: persistent storage for capture files and the known-devices db. Names are bare file
// names within the app's data directory (no paths); the adapter maps them under
// APP_DATA_PATH and enforces that. Adapter (platform/) wraps furi Storage; tests use an
// in-memory fake.
typedef struct {
    void *self;
    bool (*write_file)(void *self, const char *name, const uint8_t *data, size_t len);
    // Reads up to `cap` bytes; returns bytes read, or 0 on any failure (missing/too big).
    size_t (*read_file)(void *self, const char *name, uint8_t *buf, size_t cap);
    // Size of a stored file in bytes, or 0 if missing (lets a caller size a read buffer).
    size_t (*file_size)(void *self, const char *name);
    bool (*rename_file)(void *self, const char *from, const char *to);
    bool (*delete_file)(void *self, const char *name);
    uint16_t (*list)(void *self, WcStoreNameFn cb, void *ctx);
} WcStorePort;
