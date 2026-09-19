#include "include/domain/wc_pcap_reader.h"

#define WC_PCAP_GLOBAL_HDR 24
#define WC_PCAP_REC_HDR 16
#define WC_LINKTYPE_IEEE802_11 105
#define WC_PCAP_MAX_FRAME 4096 // sane upper bound (typical snaplen) to reject garbage lengths

static uint32_t rd_u32(const uint8_t *p, bool le) {
    if (le) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
               ((uint32_t)p[3] << 24);
    }
    return (uint32_t)p[3] | ((uint32_t)p[2] << 8) | ((uint32_t)p[1] << 16) | ((uint32_t)p[0] << 24);
}

bool wc_pcap_read(const uint8_t *data, size_t len, WcPcapFrameFn cb, void *ctx) {
    if (len < WC_PCAP_GLOBAL_HDR) {
        return false;
    }
    bool le;
    if (data[0] == 0xD4 && data[1] == 0xC3 && data[2] == 0xB2 && data[3] == 0xA1) {
        le = true; // file written little-endian
    } else if (data[0] == 0xA1 && data[1] == 0xB2 && data[2] == 0xC3 && data[3] == 0xD4) {
        le = false; // big-endian
    } else {
        return false; // not a classic pcap (nanosecond/pcapng not supported here)
    }
    if (rd_u32(data + 20, le) != WC_LINKTYPE_IEEE802_11) {
        return false;
    }

    size_t off = WC_PCAP_GLOBAL_HDR;
    while (off + WC_PCAP_REC_HDR <= len) {
        uint32_t incl = rd_u32(data + off + 8, le);
        off += WC_PCAP_REC_HDR;
        if (incl > WC_PCAP_MAX_FRAME || off + incl > len) {
            break; // truncated or implausible record — stop cleanly
        }
        cb(ctx, data + off, incl);
        off += incl;
    }
    return true;
}
