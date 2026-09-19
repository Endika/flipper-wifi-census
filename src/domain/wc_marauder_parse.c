#include "include/domain/wc_marauder_parse.h"

#include <string.h>

static int hex_val(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

// Parse exactly "hh:hh:hh:hh:hh:hh" at the start of s (which has at least 17 readable bytes).
static bool parse_mac_at(const char *s, uint8_t mac[6]) {
    for (int i = 0; i < 6; i++) {
        int hi = hex_val(s[i * 3]);
        int lo = hex_val(s[i * 3 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        if (i < 5 && s[i * 3 + 2] != ':')
            return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Find the first MAC anywhere in the NUL-terminated buffer. Returns true and fills mac.
static bool find_mac(const char *buf, size_t buflen, uint8_t mac[6]) {
    if (buflen < 17)
        return false;
    for (size_t i = 0; i + 17 <= buflen; i++) {
        if (parse_mac_at(buf + i, mac))
            return true;
    }
    return false;
}

// After the first occurrence of `key`, skip separators (: = space) and read an optional
// sign and decimal digits. Returns true and sets *val when a number is found.
static bool int_after(const char *buf, const char *key, int *val) {
    const char *p = strstr(buf, key);
    if (!p)
        return false;
    p += strlen(key);
    while (*p == ':' || *p == '=' || *p == ' ')
        p++;
    int sign = 1;
    if (*p == '-') {
        sign = -1;
        p++;
    }
    if (*p < '0' || *p > '9')
        return false;
    int n = 0;
    int digits = 0;
    // Cap the digit count so untrusted input cannot overflow the accumulator (UB). Six
    // digits is far more than any real RSSI/channel; extra digits are ignored.
    while (*p >= '0' && *p <= '9' && digits < 6) {
        n = n * 10 + (*p - '0');
        p++;
        digits++;
    }
    *val = sign * n;
    return true;
}

static int8_t clamp_i8(int v) {
    if (v > 127)
        return 127;
    if (v < -128)
        return -128;
    return (int8_t)v;
}

// Extract a directed SSID after an "SSID" label into out (max WC_SSID_MAX_LEN). Surrounding
// double quotes are stripped. Leaves out[0]=0 when absent or empty (a wildcard probe).
static void extract_ssid(const char *buf, char *out) {
    out[0] = '\0';
    const char *p = strstr(buf, "SSID");
    if (!p)
        return;
    p += 4;
    while (*p == ':' || *p == '=' || *p == ' ')
        p++;
    bool quoted = false;
    if (*p == '"') {
        quoted = true;
        p++;
    }
    size_t n = 0;
    while (*p && n < WC_SSID_MAX_LEN) {
        if (quoted && *p == '"')
            break;
        if (!quoted && (*p == '\r' || *p == '\n'))
            break;
        out[n++] = *p++;
    }
    // Trim trailing spaces from an unquoted SSID.
    if (!quoted) {
        while (n > 0 && out[n - 1] == ' ')
            n--;
    }
    out[n] = '\0';
}

bool wc_parse_summary_line(const char *line, size_t len, WcObservation *out) {
    char buf[WC_LINE_MAX];
    size_t n = len < (WC_LINE_MAX - 1) ? len : (WC_LINE_MAX - 1);
    memcpy(buf, line, n);
    buf[n] = '\0';

    memset(out, 0, sizeof(*out));
    if (!find_mac(buf, n, out->mac)) {
        return false;
    }
    out->mac_random = wc_mac_is_random(out->mac);

    int rssi = 0;
    if (int_after(buf, "RSSI", &rssi)) {
        out->rssi = clamp_i8(rssi);
    }
    int ch = 0;
    if (int_after(buf, "CH", &ch)) {
        if (ch < 0)
            ch = 0;
        if (ch > 14)
            ch = 14;
        out->channel = (uint8_t)ch;
    }
    extract_ssid(buf, out->probed_ssid);

    // Marauder's beacon-list output labels access points; probe-sniff lines are clients.
    out->is_beacon = (strstr(buf, "BEACON") != NULL) || (strstr(buf, "AP:") != NULL);
    return true;
}
