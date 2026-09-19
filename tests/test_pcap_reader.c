#include "include/domain/wc_pcap_reader.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int count;
    size_t last_len;
    uint8_t first_byte[8];
} Collector;

static void collect(void *ctx, const uint8_t *frame, size_t len) {
    Collector *c = ctx;
    if (c->count < 8) {
        c->first_byte[c->count] = frame[0];
    }
    c->last_len = len;
    c->count++;
}

// Little-endian pcap global header, linktype 105.
static const uint8_t k_gh[24] = {0xD4, 0xC3, 0xB2, 0xA1, 0x02, 0x00, 0x04, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x10, 0x00, 0x00, 105,  0x00, 0x00, 0x00};

static void put_rec(uint8_t *buf, size_t *o, const uint8_t *frame, uint32_t len) {
    memset(buf + *o, 0, 8); // timestamp
    buf[*o + 8] = (uint8_t)len;
    buf[*o + 9] = (uint8_t)(len >> 8);
    buf[*o + 10] = 0;
    buf[*o + 11] = 0;
    buf[*o + 12] = (uint8_t)len; // orig_len
    buf[*o + 13] = (uint8_t)(len >> 8);
    buf[*o + 14] = 0;
    buf[*o + 15] = 0;
    memcpy(buf + *o + 16, frame, len);
    *o += 16 + len;
}

static void test_reads_records(void) {
    uint8_t f1[24];
    uint8_t f2[30];
    memset(f1, 0x11, sizeof(f1));
    f1[0] = 0x40;
    memset(f2, 0x22, sizeof(f2));
    f2[0] = 0x40;

    uint8_t buf[256];
    size_t o = 0;
    memcpy(buf, k_gh, 24);
    o = 24;
    put_rec(buf, &o, f1, sizeof(f1));
    put_rec(buf, &o, f2, sizeof(f2));

    Collector c = {0};
    assert(wc_pcap_read(buf, o, collect, &c));
    assert(c.count == 2);
    assert(c.last_len == sizeof(f2));
    assert(c.first_byte[0] == 0x40 && c.first_byte[1] == 0x40);
}

static void test_rejects_bad(void) {
    Collector c = {0};
    uint8_t bad[24];
    memcpy(bad, k_gh, 24);
    bad[0] = 0x00; // bad magic
    assert(!wc_pcap_read(bad, sizeof(bad), collect, &c));
    assert(c.count == 0);

    uint8_t wrong_lt[24];
    memcpy(wrong_lt, k_gh, 24);
    wrong_lt[20] = 127; // radiotap, unsupported
    assert(!wc_pcap_read(wrong_lt, sizeof(wrong_lt), collect, &c));

    // Truncated record length must not over-read.
    uint8_t trunc[24 + 16 + 4];
    memcpy(trunc, k_gh, 24);
    size_t o = 24;
    uint8_t f[100];
    memset(f, 0, sizeof(f));
    // claim incl_len=100 but only provide 4 bytes
    trunc[o + 8] = 100;
    trunc[o + 9] = 0;
    trunc[o + 10] = 0;
    trunc[o + 11] = 0;
    Collector c2 = {0};
    assert(wc_pcap_read(trunc, sizeof(trunc), collect, &c2)); // header ok
    assert(c2.count == 0);                                    // record dropped, no over-read
}

int main(void) {
    test_reads_records();
    test_rejects_bad();
    printf("test_pcap_reader: OK\n");
    return 0;
}
