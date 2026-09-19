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

// A curated table of common OUIs, each verified against the IEEE registry (via macvendors).
// Not exhaustive — a hint, not a full DB; an unlisted stable MAC returns "" (unknown).
static const OuiEntry k_oui_table[] = {
    {{0x3C, 0x22, 0xFB}, WcVendorApple},       {{0xF0, 0x18, 0x98}, WcVendorApple},
    {{0xA4, 0x83, 0xE7}, WcVendorApple},       {{0x00, 0x16, 0x6C}, WcVendorSamsung},
    {{0x5C, 0x0A, 0x5B}, WcVendorSamsung},     {{0x78, 0x1F, 0xDB}, WcVendorSamsung},
    {{0x00, 0xE0, 0xFC}, WcVendorHuawei},      {{0x48, 0x46, 0xFB}, WcVendorHuawei},
    {{0x28, 0x6C, 0x07}, WcVendorXiaomi},      {{0x64, 0x09, 0x80}, WcVendorXiaomi},
    {{0x34, 0xCE, 0x00}, WcVendorXiaomi},      {{0x00, 0x1A, 0x11}, WcVendorGoogle},
    {{0x3C, 0x5A, 0xB4}, WcVendorGoogle},      {{0xF4, 0xF5, 0xE8}, WcVendorGoogle},
    {{0x00, 0x1B, 0x21}, WcVendorIntel},       {{0x34, 0x13, 0xE8}, WcVendorIntel},
    {{0x3C, 0xA9, 0xF4}, WcVendorIntel},       {{0x00, 0x14, 0x22}, WcVendorDell},
    {{0x18, 0x03, 0x73}, WcVendorDell},        {{0x28, 0x18, 0x78}, WcVendorMicrosoft},
    {{0x24, 0x0A, 0xC4}, WcVendorEspressif},   {{0xA4, 0xCF, 0x12}, WcVendorEspressif},
    {{0x7C, 0xDF, 0xA1}, WcVendorEspressif},   {{0xB8, 0x27, 0xEB}, WcVendorRaspberryPi},
    {{0xDC, 0xA6, 0x32}, WcVendorRaspberryPi}, {{0xE4, 0x5F, 0x01}, WcVendorRaspberryPi},
    {{0xD8, 0x3A, 0xDD}, WcVendorRaspberryPi},
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

const char *wc_oui_vendor_name(const uint8_t mac[6]) {
    switch (wc_oui_vendor(mac)) {
        case WcVendorApple:
            return "Apple";
        case WcVendorSamsung:
            return "Samsung";
        case WcVendorHuawei:
            return "Huawei";
        case WcVendorXiaomi:
            return "Xiaomi";
        case WcVendorGoogle:
            return "Google";
        case WcVendorIntel:
            return "Intel";
        case WcVendorDell:
            return "Dell";
        case WcVendorMicrosoft:
            return "Microsoft";
        case WcVendorEspressif:
            return "Espressif";
        case WcVendorRaspberryPi:
            return "Raspberry Pi";
        case WcVendorUnknown:
        default:
            return "";
    }
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
        case WcVendorHuawei:
        case WcVendorXiaomi:
        case WcVendorGoogle:
            return WcDevicePhone;
        case WcVendorIntel:
        case WcVendorDell:
        case WcVendorMicrosoft:
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
