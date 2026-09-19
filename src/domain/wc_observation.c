#include "include/domain/wc_observation.h"

#include <string.h>

bool wc_mac_is_random(const uint8_t mac[6]) {
    // Locally-administered bit (bit 1 of the first octet). Randomized client MACs set it.
    return (mac[0] & 0x02) != 0;
}

typedef struct {
    uint8_t oui[3];
    WcVendor vendor;
} OuiEntry;

// A deliberately small table of common OUIs. Enough to hint device class; not a full DB.
static const OuiEntry k_oui_table[] = {
    {{0x00, 0x1A, 0x11}, WcVendorApple},       {{0x3C, 0x22, 0xFB}, WcVendorApple},
    {{0xAC, 0xDE, 0x48}, WcVendorApple},       {{0x00, 0x16, 0x6C}, WcVendorSamsung},
    {{0x34, 0x23, 0x87}, WcVendorSamsung},     {{0x00, 0x1B, 0x21}, WcVendorIntel},
    {{0x34, 0x13, 0xE8}, WcVendorIntel},       {{0x24, 0x0A, 0xC4}, WcVendorEspressif},
    {{0x7C, 0xDF, 0xA1}, WcVendorEspressif},   {{0xB8, 0x27, 0xEB}, WcVendorRaspberryPi},
    {{0xDC, 0xA6, 0x32}, WcVendorRaspberryPi},
};

WcVendor wc_oui_vendor(const uint8_t mac[6]) {
    if (wc_mac_is_random(mac)) {
        return WcVendorUnknown;
    }
    for (unsigned i = 0; i < sizeof(k_oui_table) / sizeof(k_oui_table[0]); i++) {
        if (memcmp(mac, k_oui_table[i].oui, 3) == 0) {
            return k_oui_table[i].vendor;
        }
    }
    return WcVendorUnknown;
}

WcDeviceType wc_device_type_guess(const WcObservation *obs) {
    if (obs->is_beacon) {
        return WcDeviceAp;
    }
    if (obs->mac_random) {
        // Randomized MAC probing: the modern-phone signature.
        return WcDevicePhone;
    }
    switch (wc_oui_vendor(obs->mac)) {
        case WcVendorApple:
        case WcVendorSamsung:
            return WcDevicePhone;
        case WcVendorIntel:
            return WcDeviceLaptop;
        case WcVendorEspressif:
        case WcVendorRaspberryPi:
            return WcDeviceIot;
        default:
            return WcDeviceUnknown;
    }
}

const char *wc_device_type_name(WcDeviceType type) {
    switch (type) {
        case WcDevicePhone:
            return "phone";
        case WcDeviceLaptop:
            return "laptop";
        case WcDeviceIot:
            return "iot";
        case WcDeviceAp:
            return "ap";
        case WcDeviceUnknown:
        default:
            return "unknown";
    }
}
