#pragma once

#include <stddef.h>
#include <stdint.h>

// Called once per complete line received from the ESP32 (no trailing newline). `line` is
// only valid for the duration of the call.
typedef void (*WcSerialLineFn)(void *ctx, const char *line, size_t len);

// Port: the serial link to the ESP32 Marauder board. `start` begins probe sniffing and
// delivers each received line to `on_line`; `stop` ends it. Adapter (platform/) wraps
// furi_hal_serial and reassembles lines; tests inject canned lines through a fake.
typedef struct {
    void *self;
    void (*start)(void *self, WcSerialLineFn on_line, void *ctx);
    void (*stop)(void *self);
} WcSerialPort;
