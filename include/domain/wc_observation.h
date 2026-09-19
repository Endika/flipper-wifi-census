#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WC_SSID_MAX_LEN 32 // 802.11 SSID is at most 32 bytes

// One parsed sighting from a single frame reported by Marauder.
typedef struct {
    uint8_t mac[6];
    bool mac_random;                       // locally-administered address bit set
    int8_t rssi;                           // dBm, as reported (negative)
    uint8_t channel;                       // 1..14
    char probed_ssid[WC_SSID_MAX_LEN + 1]; // directed probe SSID, "" if wildcard/beacon
    bool is_beacon;                        // beacon (an AP) vs probe request (a client)
} WcObservation;

typedef enum {
    WcDeviceUnknown = 0,
    WcDevicePhone,
    WcDeviceLaptop,
    WcDeviceIot,
    WcDeviceAp,
} WcDeviceType;

typedef enum {
    WcVendorUnknown = 0,
    WcVendorApple,
    WcVendorSamsung,
    WcVendorIntel,
    WcVendorEspressif,
    WcVendorRaspberryPi,
} WcVendor;

// True when the MAC's locally-administered bit is set — the marker of a randomized
// (privacy) address. Such a MAC carries no vendor and is not stable across sessions.
bool wc_mac_is_random(const uint8_t mac[6]);

// Vendor from the OUI (first 3 bytes). Only meaningful for a stable MAC; a randomized MAC
// always returns WcVendorUnknown. Backed by a small table of well-known OUIs, not exhaustive.
WcVendor wc_oui_vendor(const uint8_t mac[6]);

// Best-effort device class from one observation. A beacon is an AP; a randomized MAC with a
// wildcard probe is the modern-phone pattern; a stable MAC leans on its vendor. Heuristic.
WcDeviceType wc_device_type_guess(const WcObservation *obs);
