#include "include/application/wc_known_service.h"

#include "include/application/wc_files.h"

#include <stdlib.h>

// Max on-disk size of the registry: header + WC_KNOWN_MAX items. Bounds the read buffer.
#define WC_KNOWN_MAX_BYTES (8 + (size_t)WC_KNOWN_MAX * 65)

bool wc_known_service_load(const WcStorePort *store, WcKnownDb *db) {
    wc_known_init(db);
    // Heap, not stack: WC_KNOWN_MAX_BYTES (~4 KB) would blow the FAP's 4 KB stack.
    uint8_t *buf = malloc(WC_KNOWN_MAX_BYTES);
    if (!buf) {
        return false;
    }
    size_t n = store->read_file(store->self, WC_KNOWN_FILE, buf, WC_KNOWN_MAX_BYTES);
    bool ok = (n == 0) ? true : wc_known_read(db, buf, n);
    free(buf);
    return ok;
}

bool wc_known_service_save(const WcStorePort *store, const WcKnownDb *db) {
    uint8_t *buf = malloc(WC_KNOWN_MAX_BYTES);
    if (!buf) {
        return false;
    }
    size_t n = wc_known_write(buf, WC_KNOWN_MAX_BYTES, db);
    bool ok = (n > 0) && store->write_file(store->self, WC_KNOWN_FILE, buf, n);
    free(buf);
    return ok;
}

bool wc_known_service_mark(const WcStorePort *store, const WcSignature *sig, const char *label) {
    WcKnownDb *db = malloc(sizeof(WcKnownDb));
    if (!db) {
        return false;
    }
    bool ok = false;
    if (wc_known_service_load(store, db)) {
        WcKnown rule;
        if (wc_known_rule_from_signature(&rule, sig, label) && wc_known_add(db, &rule)) {
            ok = wc_known_service_save(store, db);
        }
    }
    free(db);
    return ok;
}
