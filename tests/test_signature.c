#include "include/domain/wc_observation.h"
#include "include/domain/wc_signature.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static WcObservation obs_of(const uint8_t mac[6], int8_t rssi, const char *ssid, bool beacon) {
    WcObservation o;
    memset(&o, 0, sizeof(o));
    memcpy(o.mac, mac, 6);
    o.mac_random = wc_mac_is_random(mac);
    o.rssi = rssi;
    o.channel = 6;
    o.is_beacon = beacon;
    if (ssid) {
        strncpy(o.probed_ssid, ssid, WC_SSID_MAX_LEN);
    }
    return o;
}

static void test_mac_is_random(void) {
    uint8_t stable[6] = {0xB8, 0x27, 0xEB, 0x01, 0x02, 0x03}; // LAA clear
    uint8_t random[6] = {0xDA, 0x00, 0x00, 0x00, 0x00, 0x00}; // 0xDA -> bit1 set
    assert(!wc_mac_is_random(stable));
    assert(wc_mac_is_random(random));
}

static void test_oui_vendor(void) {
    uint8_t pi[6] = {0xB8, 0x27, 0xEB, 0xAA, 0xBB, 0xCC};
    uint8_t intel[6] = {0x00, 0x1B, 0x21, 0x11, 0x22, 0x33};
    uint8_t random[6] = {0xB8 | 0x02, 0x27, 0xEB, 0, 0, 0}; // even a known-OUI prefix, but LAA
    assert(wc_oui_vendor(pi) == WcVendorRaspberryPi);
    assert(wc_oui_vendor(intel) == WcVendorIntel);
    assert(wc_oui_vendor(random) == WcVendorUnknown); // randomized -> no vendor
    assert(strcmp(wc_oui_vendor_name(pi), "Raspberry Pi") == 0);
    assert(strcmp(wc_oui_vendor_name(intel), "Intel") == 0);
    assert(wc_oui_vendor_name(random)[0] == '\0'); // randomized -> blank
    uint8_t apple[6] = {0x3C, 0x22, 0xFB, 1, 2, 3};
    assert(strcmp(wc_oui_vendor_name(apple), "Apple") == 0);
}

static void test_device_type_guess(void) {
    uint8_t ap[6] = {0x00, 0x1A, 0x11, 1, 2, 3};
    uint8_t phone_rand[6] = {0xDA, 0, 0, 0, 0, 1};
    uint8_t intel[6] = {0x00, 0x1B, 0x21, 1, 2, 3};
    uint8_t esp[6] = {0x24, 0x0A, 0xC4, 1, 2, 3};

    WcObservation beacon = obs_of(ap, -40, NULL, true);
    WcObservation phone = obs_of(phone_rand, -50, NULL, false);
    WcObservation laptop = obs_of(intel, -60, NULL, false);
    WcObservation iot = obs_of(esp, -70, "HomeNet", false);

    assert(wc_device_type_guess(&beacon) == WcDeviceAp);
    assert(wc_device_type_guess(&phone) == WcDevicePhone);
    assert(wc_device_type_guess(&laptop) == WcDeviceLaptop);
    assert(wc_device_type_guess(&iot) == WcDeviceIot);
}

static void test_signature_from_and_merge(void) {
    uint8_t mac[6] = {0x24, 0x0A, 0xC4, 1, 2, 3};
    WcObservation first = obs_of(mac, -70, "HomeNet", false);
    WcSignature sig;
    wc_signature_from_obs(&sig, &first, 1000);

    assert(memcmp(sig.mac, mac, 6) == 0);
    assert(sig.type == WcDeviceIot);
    assert(sig.rssi_max == -70);
    assert(sig.obs_count == 1);
    assert(sig.first_seen == 1000 && sig.last_seen == 1000);
    assert(sig.ssid_count == 1);
    assert(wc_signature_has_ssid(&sig, "HomeNet"));

    // A stronger, later sighting with a new SSID.
    WcObservation second = obs_of(mac, -55, "Office", false);
    wc_signature_merge(&sig, &second, 1005);
    assert(sig.obs_count == 2);
    assert(sig.rssi_max == -55); // kept the stronger
    assert(sig.last_seen == 1005 && sig.first_seen == 1000);
    assert(sig.ssid_count == 2);

    // A weaker, repeated SSID does not grow the list nor lower rssi_max.
    WcObservation third = obs_of(mac, -80, "HomeNet", false);
    wc_signature_merge(&sig, &third, 1010);
    assert(sig.obs_count == 3);
    assert(sig.rssi_max == -55);
    assert(sig.ssid_count == 2);
}

static void test_signature_ssid_cap(void) {
    uint8_t mac[6] = {0x24, 0x0A, 0xC4, 9, 9, 9};
    WcObservation o = obs_of(mac, -60, "A", false);
    WcSignature sig;
    wc_signature_from_obs(&sig, &o, 0);
    assert(wc_signature_add_ssid(&sig, "B"));
    assert(wc_signature_add_ssid(&sig, "C"));
    assert(wc_signature_add_ssid(&sig, "D"));
    assert(sig.ssid_count == WC_SIG_MAX_SSIDS);
    assert(!wc_signature_add_ssid(&sig, "E")); // full
    assert(!wc_signature_add_ssid(&sig, ""));  // empty ignored
}

int main(void) {
    test_mac_is_random();
    test_oui_vendor();
    test_device_type_guess();
    test_signature_from_and_merge();
    test_signature_ssid_cap();
    printf("test_signature: OK\n");
    return 0;
}
