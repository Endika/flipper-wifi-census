#pragma once

#include "include/domain/wc_observation.h"

#include <stddef.h>

// Parse one raw 802.11 frame (linktype 105, no radiotap — as Marauder writes to pcap) into
// an observation. Only probe requests are accepted; anything else returns false. Fills MAC,
// randomized flag, sequence number, directed SSID, channel (from the DS-param IE), and an
// IE fingerprint (wc_observation.ie_hash) that is stable per device model/OS and survives
// MAC randomization. RSSI is left 0 (not present without radiotap). Bounds-checked against
// untrusted input. Returns true on a probe request.
bool wc_parse_probe_frame(const uint8_t *frame, size_t len, WcObservation *out);
