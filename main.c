#include <furi.h>

// Minimal entry point for the scaffold. The full app (scene manager, scan/compare/known
// screens) is wired in via wc_app_run() when the UI task lands; until then this builds as a
// valid FAP so every task keeps the .fap green.
int32_t wc_app(void *p) {
    UNUSED(p);
    return 0;
}
