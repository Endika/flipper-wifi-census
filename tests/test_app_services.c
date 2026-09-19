#include "include/application/wc_capture_service.h"
#include "include/application/wc_compare_service.h"
#include "include/application/wc_files.h"
#include "include/application/wc_import_service.h"
#include "include/application/wc_known_service.h"
#include "include/application/wc_merge_service.h"
#include "include/application/wc_scan_service.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- in-memory fake store (no mocks, a real working fake) ---

#define FAKE_MAX_FILES 12
#define FAKE_MAX_BYTES 32768
typedef struct {
    char name[64];
    uint8_t data[FAKE_MAX_BYTES];
    size_t len;
    bool used;
} FakeFile;
typedef struct {
    FakeFile files[FAKE_MAX_FILES];
} FakeStore;

static FakeFile *fake_find(FakeStore *s, const char *name) {
    for (int i = 0; i < FAKE_MAX_FILES; i++) {
        if (s->files[i].used && strcmp(s->files[i].name, name) == 0) {
            return &s->files[i];
        }
    }
    return NULL;
}

static bool fake_write(void *self, const char *name, const uint8_t *data, size_t len) {
    FakeStore *s = self;
    if (len > FAKE_MAX_BYTES) {
        return false;
    }
    FakeFile *f = fake_find(s, name);
    if (!f) {
        for (int i = 0; i < FAKE_MAX_FILES; i++) {
            if (!s->files[i].used) {
                f = &s->files[i];
                f->used = true;
                snprintf(f->name, sizeof(f->name), "%s", name);
                break;
            }
        }
    }
    if (!f) {
        return false;
    }
    memcpy(f->data, data, len);
    f->len = len;
    return true;
}

static size_t fake_read(void *self, const char *name, uint8_t *buf, size_t cap) {
    const FakeFile *f = fake_find(self, name);
    if (!f || f->len > cap) {
        return 0;
    }
    memcpy(buf, f->data, f->len);
    return f->len;
}

static size_t fake_file_size(void *self, const char *name) {
    const FakeFile *f = fake_find(self, name);
    return f ? f->len : 0;
}

static bool fake_rename(void *self, const char *from, const char *to) {
    FakeFile *f = fake_find(self, from);
    if (!f) {
        return false;
    }
    snprintf(f->name, sizeof(f->name), "%s", to);
    return true;
}

static bool fake_delete(void *self, const char *name) {
    FakeFile *f = fake_find(self, name);
    if (!f) {
        return false;
    }
    f->used = false;
    return true;
}

static uint16_t fake_list(void *self, WcStoreNameFn cb, void *ctx) {
    FakeStore *s = self;
    uint16_t n = 0;
    for (int i = 0; i < FAKE_MAX_FILES; i++) {
        if (s->files[i].used) {
            cb(ctx, s->files[i].name);
            n++;
        }
    }
    return n;
}

static WcStorePort fake_store_port(FakeStore *s) {
    WcStorePort p = {.self = s,
                     .write_file = fake_write,
                     .read_file = fake_read,
                     .file_size = fake_file_size,
                     .rename_file = fake_rename,
                     .delete_file = fake_delete,
                     .list = fake_list};
    return p;
}

static uint32_t g_now = 1000;
static uint32_t fake_now(void *self) {
    (void)self;
    return g_now;
}
static WcClockPort fake_clock(void) {
    WcClockPort c = {.self = NULL, .now = fake_now};
    return c;
}

// --- tests ---

static void feed(WcScanService *svc, const char *line) {
    wc_scan_on_line(svc, line, strlen(line));
}

static void test_scan_service_builds_census(void) {
    WcScanService svc;
    wc_scan_init(&svc, fake_clock());
    feed(&svc, "RSSI: -40 CH: 6 MAC: 00:1B:21:00:00:01 SSID: Net"); // stable laptop
    feed(&svc, "RSSI: -42 CH: 6 MAC: 00:1B:21:00:00:01 SSID: Net"); // same -> dedup
    feed(&svc, "RSSI: -70 CH: 1 MAC: DA:00:00:00:00:09 SSID: ");    // random phone
    feed(&svc, "garbage line no mac");                              // ignored
    WcCensusStats s = wc_scan_stats(&svc);
    assert(s.total == 2);
    assert(s.unique_stable == 1);
    assert(s.random_count == 1);
}

static void test_capture_save_load_and_csv(void) {
    FakeStore *store_data = calloc(1, sizeof(FakeStore));
    assert(store_data);
    WcStorePort store = fake_store_port(store_data);

    WcScanService svc;
    wc_scan_init(&svc, fake_clock());
    feed(&svc, "MAC: 00:1B:21:00:00:01 SSID: Net");
    feed(&svc, "MAC: DA:00:00:00:00:09 SSID: ");

    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "casa");
    meta.epoch = 1758200000u;
    assert(wc_capture_service_save(&store, "casa", &meta, &svc.census));

    // both the binary and the CSV sidecar exist
    assert(fake_find(store_data, "casa" WC_CAP_EXT));
    assert(fake_find(store_data, "casa" WC_CSV_EXT));

    WcCaptureMeta m2;
    WcCensus *c2 = calloc(1, sizeof(WcCensus));
    assert(c2);
    assert(wc_capture_service_load(&store, "casa" WC_CAP_EXT, &m2, c2));
    assert(c2->count == 2);
    assert(strcmp(m2.label, "casa") == 0);
    free(c2);
    free(store_data);
}

static void test_compare_service_end_to_end(void) {
    FakeStore *store_data = calloc(1, sizeof(FakeStore));
    assert(store_data);
    WcStorePort store = fake_store_port(store_data);

    // Location A: a shared laptop + a local-only device.
    WcScanService a;
    wc_scan_init(&a, fake_clock());
    feed(&a, "MAC: 00:1B:21:00:00:01 SSID: Net"); // shared laptop
    feed(&a, "MAC: B8:27:EB:00:00:AA SSID: ");    // only in A
    // Location B: the same laptop.
    WcScanService b;
    wc_scan_init(&b, fake_clock());
    feed(&b, "MAC: 00:1B:21:00:00:01 SSID: Net");

    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "A");
    assert(wc_capture_service_save(&store, "A", &meta, &a.census));
    snprintf(meta.label, sizeof(meta.label), "B");
    assert(wc_capture_service_save(&store, "B", &meta, &b.census));

    WcCompareResult *r = calloc(1, sizeof(WcCompareResult));
    assert(r);
    assert(wc_compare_service_run(&store, "A" WC_CAP_EXT, "B" WC_CAP_EXT, r));
    assert(r->na == 2 && r->nb == 1);
    assert(r->intersection == 1);
    assert(r->matches[0].reason == WcMatchByMac);
    free(r);
    free(store_data);
}

static void test_known_service_mark_and_match(void) {
    FakeStore *store_data = calloc(1, sizeof(FakeStore));
    assert(store_data);
    WcStorePort store = fake_store_port(store_data);

    // Empty registry loads clean.
    WcKnownDb db;
    assert(wc_known_service_load(&store, &db));
    assert(db.count == 0);

    WcSignature sig;
    memset(&sig, 0, sizeof(sig));
    const uint8_t mac[6] = {0xB8, 0x27, 0xEB, 1, 2, 3};
    memcpy(sig.mac, mac, 6);
    sig.mac_random = false;
    assert(wc_known_service_mark(&store, &sig, "Pi lab"));

    WcKnownDb db2;
    assert(wc_known_service_load(&store, &db2));
    assert(db2.count == 1);
    const WcKnown *m = wc_known_match(&db2, &sig);
    assert(m && strcmp(m->label, "Pi lab") == 0);

    // A randomized MAC with no SSID cannot be marked.
    WcSignature bad;
    memset(&bad, 0, sizeof(bad));
    bad.mac_random = true;
    assert(!wc_known_service_mark(&store, &bad, "nope"));
    free(store_data);
}

static void test_known_service_save_load_direct(void) {
    FakeStore *store_data = calloc(1, sizeof(FakeStore));
    assert(store_data);
    WcStorePort store = fake_store_port(store_data);

    WcKnownDb db;
    wc_known_init(&db);
    WcKnown k;
    memset(&k, 0, sizeof(k));
    k.type = WcRuleBySsid;
    snprintf(k.label, sizeof(k.label), "Ekin");
    snprintf(k.ssid, sizeof(k.ssid), "Ekin_Casa");
    assert(wc_known_add(&db, &k));
    assert(wc_known_service_save(&store, &db));

    WcKnownDb db2;
    assert(wc_known_service_load(&store, &db2));
    assert(db2.count == 1);
    assert(strcmp(db2.items[0].ssid, "Ekin_Casa") == 0);
    free(store_data);
}

static void test_merge_service_end_to_end(void) {
    FakeStore *store_data = calloc(1, sizeof(FakeStore));
    assert(store_data);
    WcStorePort store = fake_store_port(store_data);

    // Day 1: a laptop + a local device.
    WcScanService d1;
    wc_scan_init(&d1, fake_clock());
    feed(&d1, "MAC: 00:1B:21:00:00:01 SSID: Net"); // laptop
    feed(&d1, "MAC: B8:27:EB:00:00:AA SSID: ");    // pi, only day 1
    // Day 2: same laptop + a new device.
    WcScanService d2;
    wc_scan_init(&d2, fake_clock());
    feed(&d2, "MAC: 00:1B:21:00:00:01 SSID: Net"); // same laptop
    feed(&d2, "MAC: 3C:A9:F4:00:00:BB SSID: ");    // new, only day 2

    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    snprintf(meta.label, sizeof(meta.label), "d1");
    assert(wc_capture_service_save(&store, "d1", &meta, &d1.census));
    snprintf(meta.label, sizeof(meta.label), "d2");
    assert(wc_capture_service_save(&store, "d2", &meta, &d2.census));

    assert(wc_merge_service_run(&store, "d1" WC_CAP_EXT, "d2" WC_CAP_EXT, "acc", NULL));

    WcCaptureMeta mm;
    WcCensus *merged = calloc(1, sizeof(WcCensus));
    assert(merged);
    assert(wc_capture_service_load(&store, "acc" WC_CAP_EXT, &mm, merged));
    // laptop deduped across days, pi + new device distinct -> 3.
    assert(merged->count == 3);
    free(merged);
    free(store_data);
}

static void test_import_service_from_pcap(void) {
    FakeStore *store_data = calloc(1, sizeof(FakeStore));
    assert(store_data);
    WcStorePort store = fake_store_port(store_data);

    // Build a minimal classic pcap (LE, linktype 105) with two probe requests.
    uint8_t pcap[256];
    size_t o = 0;
    const uint8_t gh[24] = {0xD4, 0xC3, 0xB2, 0xA1, 0x02, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 105,  0x00, 0x00, 0x00};
    memcpy(pcap, gh, 24);
    o = 24;
    // A probe request: FC=0x40, dur, A1 bcast, A2 (stable mac), A3 bcast, seqctrl, SSID IE.
    const uint8_t mac[6] = {0x00, 0x1B, 0x21, 0x01, 0x02, 0x03};
    for (int r = 0; r < 2; r++) {
        uint8_t frame[64];
        size_t fl = 0;
        frame[fl++] = 0x40;
        frame[fl++] = 0x00;
        frame[fl++] = 0x00;
        frame[fl++] = 0x00;
        memset(frame + fl, 0xFF, 6);
        fl += 6; // A1
        memcpy(frame + fl, mac, 6);
        fl += 6; // A2
        memset(frame + fl, 0xFF, 6);
        fl += 6; // A3
        frame[fl++] = (uint8_t)(r << 4);
        frame[fl++] = 0; // seqctrl
        frame[fl++] = 0;
        frame[fl++] = 3;
        frame[fl++] = 'N';
        frame[fl++] = 'e';
        frame[fl++] = 't'; // SSID IE "Net"
        // pcap record header
        memset(pcap + o, 0, 8);
        pcap[o + 8] = (uint8_t)fl;
        pcap[o + 9] = 0;
        pcap[o + 10] = 0;
        pcap[o + 11] = 0;
        pcap[o + 12] = (uint8_t)fl;
        pcap[o + 13] = 0;
        pcap[o + 14] = 0;
        pcap[o + 15] = 0;
        memcpy(pcap + o + 16, frame, fl);
        o += 16 + fl;
    }
    assert(store.write_file(store.self, "cap.pcap", pcap, o));

    assert(wc_import_service_run(&store, fake_clock(), "cap.pcap", "imp"));
    WcCaptureMeta m;
    WcCensus *c = calloc(1, sizeof(WcCensus));
    assert(c);
    assert(wc_capture_service_load(&store, "imp" WC_CAP_EXT, &m, c));
    // both records are the same device -> deduped to 1, probing "Net".
    assert(c->count == 1);
    assert(strcmp(c->devices[0].ssids[0], "Net") == 0);
    free(c);
    free(store_data);
}

int main(void) {
    test_scan_service_builds_census();
    test_import_service_from_pcap();
    test_merge_service_end_to_end();
    test_capture_save_load_and_csv();
    test_compare_service_end_to_end();
    test_known_service_mark_and_match();
    test_known_service_save_load_direct();
    printf("test_app_services: OK\n");
    return 0;
}
