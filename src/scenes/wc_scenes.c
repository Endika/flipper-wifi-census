#include "include/app/wc_app.h"
#include "include/application/wc_files.h"
#include "include/application/wc_import_service.h"
#include "include/application/wc_merge_service.h"
#include "include/domain/wc_observation.h"
#include "include/domain/wc_timefmt.h"
#include "include/domain/wc_version_info.h"
#include "include/scenes/wc_scene.h"

#include <furi.h>

// ---------------------------------------------------------------------------
// Shared callbacks and helpers
// ---------------------------------------------------------------------------

static void wc_submenu_cb(void *context, uint32_t index) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void wc_text_input_cb(void *context) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WcCustomEventTextDone);
}

// Replace the capture extension on `base` (bare label) to build "<base>.csv".
static void csv_name_of(const char *base_with_ext, char *out, size_t cap) {
    size_t n = strlen(base_with_ext);
    size_t el = strlen(WC_CAP_EXT);
    size_t stem = (n > el) ? n - el : n;
    snprintf(out, cap, "%.*s%s", (int)stem, base_with_ext, WC_CSV_EXT);
}

static void files_list_cb(void *context, const char *name) {
    WcApp *app = context;
    size_t n = strlen(name);
    size_t el = strlen(app->list_ext);
    if (n > el && strcmp(name + n - el, app->list_ext) == 0 && app->list_count < WC_MAX_LIST) {
        strncpy(app->list_names[app->list_count], name, WC_TEXT_BUF_SIZE - 1);
        app->list_names[app->list_count][WC_TEXT_BUF_SIZE - 1] = '\0';
        submenu_add_item(app->submenu, app->list_names[app->list_count], app->list_count,
                         wc_submenu_cb, app);
        app->list_count++;
    }
}

static void populate_list(WcApp *app, const char *header, const char *ext, const char *empty_msg) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, header);
    app->list_ext = ext;
    app->list_count = 0;
    app->store.list(app->store.self, files_list_cb, app);
    if (app->list_count == 0) {
        submenu_add_item(app->submenu, empty_msg, WC_MAX_LIST, wc_submenu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewSubmenu);
}

static void populate_files(WcApp *app, const char *header) {
    populate_list(app, header, WC_CAP_EXT, "(no captures)");
}

static const char *known_label_for(WcApp *app, const WcMatch *m) {
    WcSignature sig;
    memset(&sig, 0, sizeof(sig));
    memcpy(sig.mac, m->mac, 6);
    sig.mac_random = wc_mac_is_random(m->mac);
    if (m->reason == WcMatchBySsid) {
        strncpy(sig.ssids[0], m->detail, WC_SSID_MAX_LEN);
        sig.ssid_count = 1;
    }
    const WcKnown *k = wc_known_match(&app->known, &sig);
    return k ? k->label : NULL;
}

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

typedef enum {
    StartScan,
    StartFiles,
    StartCompare,
    StartMerge,
    StartImport,
    StartKnown,
    StartSettings,
    StartAbout,
} StartItem;

void wc_scene_start_on_enter(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "WiFi Census");
    submenu_add_item(app->submenu, "Scan", StartScan, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Files", StartFiles, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Compare", StartCompare, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Merge", StartMerge, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Import pcap", StartImport, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Known devices", StartKnown, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Settings", StartSettings, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "About", StartAbout, wc_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewSubmenu);
}

bool wc_scene_start_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type != SceneManagerEventTypeCustom) {
        return false;
    }
    switch (event.event) {
        case StartScan:
            scene_manager_next_scene(app->scene_manager, WcSceneScan);
            return true;
        case StartFiles:
            scene_manager_next_scene(app->scene_manager, WcSceneFiles);
            return true;
        case StartCompare:
            scene_manager_next_scene(app->scene_manager, WcSceneCompareA);
            return true;
        case StartMerge:
            scene_manager_next_scene(app->scene_manager, WcSceneMergeA);
            return true;
        case StartImport:
            scene_manager_next_scene(app->scene_manager, WcSceneImportPick);
            return true;
        case StartKnown:
            scene_manager_next_scene(app->scene_manager, WcSceneKnown);
            return true;
        case StartSettings:
            scene_manager_next_scene(app->scene_manager, WcSceneSettings);
            return true;
        case StartAbout:
            scene_manager_next_scene(app->scene_manager, WcSceneAbout);
            return true;
        default:
            return false;
    }
}

void wc_scene_start_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

// ---------------------------------------------------------------------------
// Scan (live counters)
// ---------------------------------------------------------------------------

static void scan_timer_cb(void *context) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WcCustomEventScanTick);
}

static void scan_render(WcApp *app) {
    WcCensusStats s = wc_scan_stats(&app->scan);
    char buf[256];
    snprintf(
        buf, sizeof(buf),
        "Scanning 2.4GHz...\nUnique: %u (stable %u)\nRandom: %u (%u%%)\nPhone %u Lap %u IoT %u\n"
        "Networks sought: %u\nBack = stop & save",
        s.total, s.unique_stable, s.random_count, s.pct_random, s.by_type[WcDevicePhone],
        s.by_type[WcDeviceLaptop], s.by_type[WcDeviceIot], s.networks);
    widget_reset(app->widget);
    widget_add_string_multiline_element(app->widget, 0, 0, AlignLeft, AlignTop, FontSecondary, buf);
}

void wc_scene_scan_on_enter(void *context) {
    WcApp *app = context;
    wc_scan_init(&app->scan, app->clock);
    app->scan_started = wc_clock_now(&app->clock);

    app->serial = wc_serial_furi_alloc(app->baud);
    WcSerialPort port = wc_serial_furi_port(app->serial);
    port.start(port.self, wc_scan_on_line, &app->scan);

    scan_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);

    app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->scan_timer, furi_ms_to_ticks(500));
}

bool wc_scene_scan_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventScanTick) {
        scan_render(app);
        return true;
    }
    if (event.type == SceneManagerEventTypeBack) {
        // Stop and go to the save screen instead of dropping the scan.
        scene_manager_next_scene(app->scene_manager, WcSceneSave);
        return true;
    }
    return false;
}

void wc_scene_scan_on_exit(void *context) {
    WcApp *app = context;
    if (app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    if (app->serial) {
        WcSerialPort port = wc_serial_furi_port(app->serial);
        port.stop(port.self);
        wc_serial_furi_free(app->serial);
        app->serial = NULL;
    }
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

void wc_scene_save_on_enter(void *context) {
    WcApp *app = context;
    // Prefill a default name from the scan's start time so OK-without-typing saves at once.
    wc_default_capture_name(app->scan_started, app->text_buf, sizeof(app->text_buf));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Capture name");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_BASENAME_MAX, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_save_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        WcCaptureMeta meta;
        memset(&meta, 0, sizeof(meta));
        strncpy(meta.label, app->text_buf, WC_LABEL_MAX);
        meta.epoch = app->scan_started;
        meta.duration_s = wc_clock_now(&app->clock) - app->scan_started;
        meta.channels_mask = 0x3FFF; // channels 1-14 hopped
        wc_capture_service_save(&app->store, app->text_buf, &meta, &app->scan.census);
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        return true;
    }
    return false;
}

void wc_scene_save_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

void wc_scene_files_on_enter(void *context) {
    WcApp *app = context;
    if (app->browse_census) {
        free(app->browse_census);
        app->browse_census = NULL;
    }
    populate_files(app, "Captures");
}

bool wc_scene_files_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->selected_file, app->list_names[event.event], WC_TEXT_BUF_SIZE - 1);
        app->selected_file[WC_TEXT_BUF_SIZE - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, WcSceneFileActions);
        return true;
    }
    return false;
}

void wc_scene_files_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

// ---------------------------------------------------------------------------
// File actions
// ---------------------------------------------------------------------------

typedef enum {
    ActionDevices,
    ActionNetworks,
    ActionRename,
    ActionDelete,
} FileAction;

// Load the selected capture into browse_census (allocating once). Returns true on success.
static bool ensure_browse_loaded(WcApp *app) {
    if (!app->browse_census) {
        app->browse_census = malloc(sizeof(WcCensus));
    }
    return app->browse_census && wc_capture_service_load(&app->store, app->selected_file,
                                                         &app->browse_meta, app->browse_census);
}

void wc_scene_file_actions_on_enter(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->selected_file);
    submenu_add_item(app->submenu, "Devices", ActionDevices, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Networks sought", ActionNetworks, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Rename", ActionRename, wc_submenu_cb, app);
    submenu_add_item(app->submenu, "Delete", ActionDelete, wc_submenu_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewSubmenu);
}

bool wc_scene_file_actions_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type != SceneManagerEventTypeCustom) {
        return false;
    }
    switch (event.event) {
        case ActionDevices:
            if (ensure_browse_loaded(app)) {
                scene_manager_next_scene(app->scene_manager, WcSceneDevices);
            }
            return true;
        case ActionNetworks:
            if (ensure_browse_loaded(app)) {
                scene_manager_next_scene(app->scene_manager, WcSceneNetworks);
            }
            return true;
        case ActionRename:
            scene_manager_next_scene(app->scene_manager, WcSceneRename);
            return true;
        case ActionDelete: {
            char csv[WC_TEXT_BUF_SIZE];
            csv_name_of(app->selected_file, csv, sizeof(csv));
            app->store.delete_file(app->store.self, app->selected_file);
            app->store.delete_file(app->store.self, csv);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        default:
            return false;
    }
}

void wc_scene_file_actions_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

// ---------------------------------------------------------------------------
// Devices (of a loaded capture)
// ---------------------------------------------------------------------------

void wc_scene_devices_on_enter(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Mark as known");
    WcCensus *c = app->browse_census;
    uint16_t n = c ? c->count : 0;
    for (uint16_t i = 0; i < n && i < WC_MAX_LIST; i++) {
        const WcSignature *d = &c->devices[i];
        const char *vendor = wc_signature_vendor(d);
        char label[64];
        snprintf(label, sizeof(label), "%s%s%s %02X:%02X:%02X:%02X:%02X:%02X",
                 wc_device_type_name(d->type), vendor[0] ? " " : "", vendor, d->mac[0], d->mac[1],
                 d->mac[2], d->mac[3], d->mac[4], d->mac[5]);
        submenu_add_item(app->submenu, label, i, wc_submenu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewSubmenu);
}

bool wc_scene_devices_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    uint16_t n = app->browse_census ? app->browse_census->count : 0;
    if (event.type == SceneManagerEventTypeCustom && event.event < n) {
        app->selected_device = (uint16_t)event.event;
        scene_manager_next_scene(app->scene_manager, WcSceneMarkLabel);
        return true;
    }
    return false;
}

void wc_scene_devices_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

// ---------------------------------------------------------------------------
// Networks sought (directed SSIDs devices are probing for)
// ---------------------------------------------------------------------------

void wc_scene_networks_on_enter(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Networks sought");
    if (app->browse_census) {
        WcSsidTally *t = malloc(sizeof(WcSsidTally) * WC_MAX_LIST);
        if (t) {
            uint16_t n = wc_census_ssid_tally(app->browse_census, t, WC_MAX_LIST);
            for (uint16_t i = 0; i < n && i < WC_MAX_LIST; i++) {
                char label[64];
                snprintf(label, sizeof(label), "%s (%u)", t[i].ssid, t[i].devices);
                submenu_add_item(app->submenu, label, i, wc_submenu_cb, app);
            }
            if (n == 0) {
                submenu_add_item(app->submenu, "(none sought)", WC_MAX_LIST, wc_submenu_cb, app);
            }
            free(t);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewSubmenu);
}

bool wc_scene_networks_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_networks_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

// ---------------------------------------------------------------------------
// Mark label (turn a device into a known rule)
// ---------------------------------------------------------------------------

void wc_scene_mark_label_on_enter(void *context) {
    WcApp *app = context;
    app->text_buf[0] = '\0';
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Label for device");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_KNOWN_LABEL_MAX, true);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_mark_label_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        if (app->browse_census && app->selected_device < app->browse_census->count) {
            const WcSignature *sig = &app->browse_census->devices[app->selected_device];
            wc_known_service_mark(&app->store, sig, app->text_buf);
            wc_known_service_load(&app->store, &app->known);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void wc_scene_mark_label_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

// ---------------------------------------------------------------------------
// Rename
// ---------------------------------------------------------------------------

void wc_scene_rename_on_enter(void *context) {
    WcApp *app = context;
    // Prefill with the current stem (without the extension).
    size_t el = strlen(WC_CAP_EXT);
    size_t n = strlen(app->selected_file);
    size_t stem = (n > el) ? n - el : n;
    snprintf(app->text_buf, sizeof(app->text_buf), "%.*s", (int)stem, app->selected_file);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "New name");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_BASENAME_MAX, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_rename_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        char old_csv[WC_TEXT_BUF_SIZE + 8], new_bin[WC_TEXT_BUF_SIZE + 8],
            new_csv[WC_TEXT_BUF_SIZE + 8];
        csv_name_of(app->selected_file, old_csv, sizeof(old_csv));
        snprintf(new_bin, sizeof(new_bin), "%s%s", app->text_buf, WC_CAP_EXT);
        snprintf(new_csv, sizeof(new_csv), "%s%s", app->text_buf, WC_CSV_EXT);
        app->store.rename_file(app->store.self, app->selected_file, new_bin);
        app->store.rename_file(app->store.self, old_csv, new_csv);
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneFiles);
        return true;
    }
    return false;
}

void wc_scene_rename_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

// ---------------------------------------------------------------------------
// Compare: pick A, pick B, show result
// ---------------------------------------------------------------------------

void wc_scene_compare_a_on_enter(void *context) {
    WcApp *app = context;
    populate_files(app, "Compare: pick A");
}

bool wc_scene_compare_a_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->compare_a, app->list_names[event.event], WC_TEXT_BUF_SIZE - 1);
        app->compare_a[WC_TEXT_BUF_SIZE - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, WcSceneCompareB);
        return true;
    }
    return false;
}

void wc_scene_compare_a_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

static void format_compare(WcApp *app, const WcCompareResult *r) {
    size_t len = 0;
    len += snprintf(app->result_text + len, WC_RESULT_TEXT_SIZE - len,
                    "A:%u B:%u  common:%u\nrandom(not crossable) A:%u B:%u\n", r->na, r->nb,
                    r->intersection, r->random_a, r->random_b);
    for (uint16_t i = 0; i < r->match_count && len < WC_RESULT_TEXT_SIZE - 1; i++) {
        const WcMatch *m = &r->matches[i];
        const char *label = known_label_for(app, m);
        if (m->reason == WcMatchBySsid) {
            len += snprintf(app->result_text + len, WC_RESULT_TEXT_SIZE - len, "- %s [ssid:%s]\n",
                            label ? label : "device", m->detail);
        } else {
            len += snprintf(app->result_text + len, WC_RESULT_TEXT_SIZE - len,
                            "- %s %02X:%02X:%02X:%02X:%02X:%02X\n", label ? label : "device",
                            m->mac[0], m->mac[1], m->mac[2], m->mac[3], m->mac[4], m->mac[5]);
        }
    }
}

void wc_scene_compare_b_on_enter(void *context) {
    WcApp *app = context;
    populate_files(app, "Pick B");
}

bool wc_scene_compare_b_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        char name_b[WC_TEXT_BUF_SIZE];
        strncpy(name_b, app->list_names[event.event], sizeof(name_b) - 1);
        name_b[sizeof(name_b) - 1] = '\0';
        WcCompareResult *r = malloc(sizeof(WcCompareResult));
        if (r) {
            if (wc_compare_service_run(&app->store, app->compare_a, name_b, r)) {
                format_compare(app, r);
            } else {
                snprintf(app->result_text, WC_RESULT_TEXT_SIZE, "Could not load captures.");
            }
            free(r);
            scene_manager_next_scene(app->scene_manager, WcSceneCompareResult);
        }
        return true;
    }
    return false;
}

void wc_scene_compare_b_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

void wc_scene_compare_result_on_enter(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);
}

bool wc_scene_compare_result_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_compare_result_on_exit(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
}

// ---------------------------------------------------------------------------
// Merge: pick A, pick B, name the accumulated capture
// ---------------------------------------------------------------------------

void wc_scene_merge_a_on_enter(void *context) {
    WcApp *app = context;
    populate_files(app, "Merge: pick A");
}

bool wc_scene_merge_a_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->merge_a, app->list_names[event.event], WC_TEXT_BUF_SIZE - 1);
        app->merge_a[WC_TEXT_BUF_SIZE - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, WcSceneMergeB);
        return true;
    }
    return false;
}

void wc_scene_merge_a_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

void wc_scene_merge_b_on_enter(void *context) {
    WcApp *app = context;
    populate_files(app, "Merge: pick B");
}

bool wc_scene_merge_b_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->merge_b, app->list_names[event.event], WC_TEXT_BUF_SIZE - 1);
        app->merge_b[WC_TEXT_BUF_SIZE - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, WcSceneMergeName);
        return true;
    }
    return false;
}

void wc_scene_merge_b_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

void wc_scene_merge_name_on_enter(void *context) {
    WcApp *app = context;
    wc_default_capture_name(wc_clock_now(&app->clock), app->text_buf, sizeof(app->text_buf));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Merged name");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_BASENAME_MAX, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_merge_name_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        wc_merge_service_run(&app->store, app->merge_a, app->merge_b, app->text_buf);
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        return true;
    }
    return false;
}

void wc_scene_merge_name_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

// ---------------------------------------------------------------------------
// Import: pick a .pcap on the SD, name the census, build it
// ---------------------------------------------------------------------------

void wc_scene_import_pick_on_enter(void *context) {
    WcApp *app = context;
    populate_list(app, "Import pcap (in app folder)", ".pcap", "(no .pcap here)");
}

bool wc_scene_import_pick_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->selected_file, app->list_names[event.event], WC_TEXT_BUF_SIZE - 1);
        app->selected_file[WC_TEXT_BUF_SIZE - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, WcSceneImportName);
        return true;
    }
    return false;
}

void wc_scene_import_pick_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

void wc_scene_import_name_on_enter(void *context) {
    WcApp *app = context;
    wc_default_capture_name(wc_clock_now(&app->clock), app->text_buf, sizeof(app->text_buf));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Census name");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_BASENAME_MAX, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_import_name_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        wc_import_service_run(&app->store, app->clock, app->selected_file, app->text_buf);
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        return true;
    }
    return false;
}

void wc_scene_import_name_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

// ---------------------------------------------------------------------------
// Known devices (list)
// ---------------------------------------------------------------------------

void wc_scene_known_on_enter(void *context) {
    WcApp *app = context;
    wc_known_service_load(&app->store, &app->known);
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Known devices");
    for (uint16_t i = 0; i < app->known.count; i++) {
        const WcKnown *k = &app->known.items[i];
        char label[64];
        if (k->type == WcRuleBySsid) {
            snprintf(label, sizeof(label), "%s [ssid:%s]", k->label, k->ssid);
        } else {
            snprintf(label, sizeof(label), "%s [%02X:%02X:%02X]", k->label, k->mac[0], k->mac[1],
                     k->mac[2]);
        }
        submenu_add_item(app->submenu, label, i, wc_submenu_cb, app);
    }
    if (app->known.count == 0) {
        submenu_add_item(app->submenu, "(none yet)", WC_MAX_LIST, wc_submenu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewSubmenu);
}

bool wc_scene_known_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_known_on_exit(void *context) {
    WcApp *app = context;
    submenu_reset(app->submenu);
}

// ---------------------------------------------------------------------------
// Settings (UART baud)
// ---------------------------------------------------------------------------

static const char *const k_baud_names[] = {"115200", "230400"};
static const uint32_t k_baud_values[] = {115200, 230400};

static void settings_baud_changed(VariableItem *item) {
    WcApp *app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, k_baud_names[idx]);
    app->baud = k_baud_values[idx];
}

void wc_scene_settings_on_enter(void *context) {
    WcApp *app = context;
    variable_item_list_reset(app->var_item_list);
    VariableItem *item =
        variable_item_list_add(app->var_item_list, "UART baud", 2, settings_baud_changed, app);
    uint8_t idx = (app->baud == k_baud_values[1]) ? 1 : 0;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, k_baud_names[idx]);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewVarList);
}

bool wc_scene_settings_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_settings_on_exit(void *context) {
    WcApp *app = context;
    variable_item_list_reset(app->var_item_list);
}

// ---------------------------------------------------------------------------
// About
// ---------------------------------------------------------------------------

void wc_scene_about_on_enter(void *context) {
    WcApp *app = context;
    snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
             "WiFi Census\nv%s  by Endika\ngithub.com/Endika/flipper-wifi-census\n\n"
             "Passive 2.4GHz device census via an ESP32 Marauder board. Counts and types nearby "
             "devices, saves each scan, and compares captures across places.\n\n"
             "Honest limits: stable-MAC gear and devices probing a named network cross reliably; "
             "modern phones randomize their MAC and are counted but not crossable (use Known "
             "devices).\n\nPassive only. Everything stays on the SD card.",
             wc_version());
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);
}

bool wc_scene_about_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_about_on_exit(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
}
