#include "include/app/wc_app.h"
#include "include/application/wc_files.h"
#include "include/application/wc_import_service.h"
#include "include/application/wc_merge_service.h"
#include "include/domain/wc_observation.h"
#include "include/domain/wc_timefmt.h"
#include "include/domain/wc_version_info.h"
#include "include/scenes/wc_scene.h"

#include <dialogs/dialogs.h>
#include <furi.h>
#include <storage/storage.h>

// ---------------------------------------------------------------------------
// Shared callbacks and helpers
// ---------------------------------------------------------------------------

static void wc_list_cb(void *context, uint32_t index) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Persist the user options after any change, so the next run of the app starts where this one
// left off instead of silently reverting to the defaults.
static void wc_settings_persist(WcApp *app) {
    WcSettings s = {.baud = app->baud, .autosave = app->autosave};
    wc_settings_service_save(&app->store, &s);
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
        wc_scroll_list_add_item(app->list_view, app->list_names[app->list_count], app->list_count,
                                wc_list_cb, app);
        app->list_count++;
    }
}

static void populate_list(WcApp *app, const char *header, const char *ext, const char *empty_msg) {
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, header);
    app->list_ext = ext;
    app->list_count = 0;
    app->store.list(app->store.self, files_list_cb, app);
    if (app->list_count == 0) {
        wc_scroll_list_add_item(app->list_view, empty_msg, WC_MAX_LIST, wc_list_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
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
    StartAutoSave,
    StartFiles,
    StartCompare,
    StartMerge,
    StartImport,
    StartKnown,
    StartSettings,
    StartSerialDebug,
    StartAbout,
} StartItem;

// Auto-save lives in Settings too, but it is the one option you decide right before scanning,
// so the menu both shows it and toggles it.
static void start_populate(WcApp *app) {
    char autosave_label[24];
    snprintf(autosave_label, sizeof(autosave_label), "Auto-save: %s", app->autosave ? "On" : "Off");
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "WiFi Census");
    wc_scroll_list_add_item(app->list_view, "Scan", StartScan, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, autosave_label, StartAutoSave, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Files", StartFiles, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Compare", StartCompare, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Merge", StartMerge, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Import pcap", StartImport, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Known devices", StartKnown, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Settings", StartSettings, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Serial debug", StartSerialDebug, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "About", StartAbout, wc_list_cb, app);
}

void wc_scene_start_on_enter(void *context) {
    WcApp *app = context;
    start_populate(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
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
        case StartAutoSave:
            app->autosave = !app->autosave;
            wc_settings_persist(app);
            start_populate(app);
            wc_scroll_list_set_selected_item(app->list_view, StartAutoSave);
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
        case StartSerialDebug:
            scene_manager_next_scene(app->scene_manager, WcSceneSerialDebug);
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
    wc_scroll_list_reset(app->list_view);
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
    char tail[40];
    if (app->autosave) {
        snprintf(tail, sizeof(tail), "Auto-save: file %u", app->autosave_idx);
    } else if (app->scan.census.dropped > 0) {
        snprintf(tail, sizeof(tail), "FULL +%u dropped", app->scan.census.dropped);
    } else {
        snprintf(tail, sizeof(tail), "Back = stop & save");
    }
    char buf[256];
    snprintf(
        buf, sizeof(buf),
        "Scanning 2.4GHz...\nUnique: %u (stable %u)\nRandom: %u (%u%%)\nPhone %u Lap %u IoT %u\n"
        "Networks sought: %u\n%s",
        s.total, s.unique_stable, s.random_count, s.pct_random, s.by_type[WcDevicePhone],
        s.by_type[WcDeviceLaptop], s.by_type[WcDeviceIot], s.networks, tail);
    widget_reset(app->widget);
    widget_add_string_multiline_element(app->widget, 0, 0, AlignLeft, AlignTop, FontSecondary, buf);
}

// Returns false when the USART is busy; the adapter is freed again so nothing dangles.
static bool scan_serial_start(WcApp *app) {
    app->serial = wc_serial_furi_alloc(app->baud);
    WcSerialPort port = wc_serial_furi_port(app->serial);
    if (!port.start(port.self, wc_scan_on_line, &app->scan)) {
        wc_serial_furi_free(app->serial);
        app->serial = NULL;
        return false;
    }
    return true;
}

static const char *const k_uart_busy_text =
    "UART busy\n\nAnother service holds the\nGPIO serial port.\n\nDisable Settings >\nExpansion "
    "Modules\n(or close the CLI)\nand try again.\n\nBack = menu";

static void scan_show_link_error(WcApp *app) {
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, k_uart_busy_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);
}

static void scan_serial_stop(WcApp *app) {
    if (app->serial) {
        WcSerialPort port = wc_serial_furi_port(app->serial);
        port.stop(port.self);
        wc_serial_furi_free(app->serial);
        app->serial = NULL;
    }
}

static void scan_reset_census(WcApp *app) {
    wc_census_free(&app->scan.census);
    wc_scan_init(&app->scan, app->clock);
    // Reserve up front: a busy scan then never reallocs (the transient 2x copy is what
    // exhausted the heap and rebooted the Flipper mid-scan).
    wc_census_reserve(&app->scan.census, WC_CENSUS_MAX_DEVICES);
}

// Save the current census as "<base>_<idx>" (used by auto-save). Serial must be stopped so the
// worker thread is not writing the census while we read it.
static void scan_save_chunk(WcApp *app) {
    char name[WC_TEXT_BUF_SIZE + 8];
    snprintf(name, sizeof(name), "%s_%u", app->autosave_base, app->autosave_idx);
    WcCaptureMeta meta;
    memset(&meta, 0, sizeof(meta));
    strncpy(meta.label, name, WC_LABEL_MAX);
    meta.epoch = app->scan_started;
    meta.duration_s = wc_clock_now(&app->clock) - app->scan_started;
    meta.channels_mask = 0x3FFF;
    wc_capture_service_save(&app->store, name, &meta, &app->scan.census);
    app->autosave_idx++;
}

void wc_scene_scan_on_enter(void *context) {
    WcApp *app = context;
    scan_reset_census(app);
    app->scan_started = wc_clock_now(&app->clock);
    if (app->autosave) {
        wc_default_name(app->scan_started, "auto", app->autosave_base, sizeof(app->autosave_base));
        app->autosave_idx = 1;
    }
    app->scan_link_ok = scan_serial_start(app);
    if (!app->scan_link_ok) {
        // Nothing was captured and nothing is running: release the census we just reserved and
        // explain the problem instead of showing an empty scan that never counts anything.
        wc_census_free(&app->scan.census);
        scan_show_link_error(app);
        return;
    }

    scan_render(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);

    app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->scan_timer, furi_ms_to_ticks(500));
}

bool wc_scene_scan_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventScanTick) {
        scan_render(app);
        // Auto-save rotation: near the ceiling, save this chunk and start a fresh census so a
        // huge venue spans several files instead of dropping devices. Stop serial first so the
        // worker thread is not touching the census while we save + reset it.
        if (app->autosave && app->scan.census.count >= WC_AUTOSAVE_ROTATE_AT) {
            scan_serial_stop(app);
            scan_save_chunk(app);
            scan_reset_census(app);
            if (!scan_serial_start(app)) {
                // The port went away mid-run: stop cleanly and say so rather than freezing.
                app->scan_link_ok = false;
                if (app->scan_timer) {
                    furi_timer_stop(app->scan_timer);
                    furi_timer_free(app->scan_timer);
                    app->scan_timer = NULL;
                }
                scan_show_link_error(app);
            }
        }
        return true;
    }
    if (event.type == SceneManagerEventTypeBack) {
        if (!app->scan_link_ok) {
            scene_manager_previous_scene(app->scene_manager); // never started: nothing to save
            return true;
        }
        if (app->autosave) {
            // Save the final partial chunk automatically and return to the menu.
            scan_serial_stop(app);
            if (app->scan.census.count > 0) {
                scan_save_chunk(app);
            }
            wc_census_free(&app->scan.census);
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        } else {
            // Stop and go to the save screen instead of dropping the scan.
            scene_manager_next_scene(app->scene_manager, WcSceneSave);
        }
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
    scan_serial_stop(app);
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
        wc_census_free(&app->scan.census); // done with it; free until the next scan
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
        wc_census_free(app->browse_census);
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
    wc_scroll_list_reset(app->list_view);
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

// Show app->result_text on a full screen (used for load errors and other one-off messages).
static void show_message(WcApp *app, const char *text) {
    snprintf(app->result_text, sizeof(app->result_text), "%s", text);
    scene_manager_next_scene(app->scene_manager, WcSceneMsg);
}

void wc_scene_msg_on_enter(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);
}

bool wc_scene_msg_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_msg_on_exit(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
}

void wc_scene_file_actions_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, app->selected_file);
    wc_scroll_list_add_item(app->list_view, "Devices", ActionDevices, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Networks sought", ActionNetworks, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Rename", ActionRename, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Delete", ActionDelete, wc_list_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
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
            } else {
                show_message(app, "Can't open this capture.\nIt may be too large (over\n100 "
                                  "devices) or corrupt.\nCombine it on a PC\nwith wc_merge.");
            }
            return true;
        case ActionNetworks:
            if (ensure_browse_loaded(app)) {
                scene_manager_next_scene(app->scene_manager, WcSceneNetworks);
            } else {
                show_message(app, "Can't open this capture.\nIt may be too large (over\n100 "
                                  "devices) or corrupt.\nCombine it on a PC\nwith wc_merge.");
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
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Devices (of a loaded capture)
// ---------------------------------------------------------------------------

void wc_scene_devices_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "Devices (select)");
    WcCensus *c = app->browse_census;
    uint16_t n = c ? c->count : 0;
    for (uint16_t i = 0; i < n && i < WC_MAX_LIST; i++) {
        const WcSignature *d = &c->devices[i];
        char label[64];
        // Lead with the most identifying bit so the truncated row still says something: the
        // probed network if any (the key signal), else the vendor, else the MAC tail. Full
        // detail (whole MAC, all networks) is one click away in the detail scene.
        if (d->ssid_count > 0) {
            snprintf(label, sizeof(label), "%s >%s", wc_device_type_name(d->type), d->ssids[0]);
        } else {
            const char *vendor = wc_signature_vendor(d);
            if (vendor[0]) {
                snprintf(label, sizeof(label), "%s %s %02X:%02X:%02X", wc_device_type_name(d->type),
                         vendor, d->mac[3], d->mac[4], d->mac[5]);
            } else {
                snprintf(label, sizeof(label), "%s %02X:%02X:%02X:%02X:%02X:%02X",
                         wc_device_type_name(d->type), d->mac[0], d->mac[1], d->mac[2], d->mac[3],
                         d->mac[4], d->mac[5]);
            }
        }
        wc_scroll_list_add_item(app->list_view, label, i, wc_list_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_devices_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    uint16_t n = app->browse_census ? app->browse_census->count : 0;
    if (event.type == SceneManagerEventTypeCustom && event.event < n) {
        app->selected_device = (uint16_t)event.event;
        scene_manager_next_scene(app->scene_manager, WcSceneDeviceDetail);
        return true;
    }
    return false;
}

void wc_scene_devices_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Device detail (full info + probed networks; mark known from here)
// ---------------------------------------------------------------------------

static void device_detail_button_cb(GuiButtonType result, InputType type, void *context) {
    if (result == GuiButtonTypeCenter && type == InputTypeShort) {
        WcApp *app = context;
        view_dispatcher_send_custom_event(app->view_dispatcher, WcCustomEventButton);
    }
}

// Bounded append: snprintf into buf+*len, advancing *len and never running past cap.
static void detail_catf(char *buf, size_t *len, size_t cap, const char *s) {
    if (*len >= cap) {
        return;
    }
    int r = snprintf(buf + *len, cap - *len, "%s", s);
    if (r > 0) {
        *len += (size_t)r;
    }
    if (*len >= cap) {
        *len = cap - 1;
    }
}

void wc_scene_device_detail_on_enter(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
    WcCensus *c = app->browse_census;
    if (c && app->selected_device < c->count) {
        const WcSignature *d = &c->devices[app->selected_device];
        const char *vendor = wc_signature_vendor(d);
        bool markable = !d->mac_random || d->ssid_count > 0;

        char buf[320];
        size_t len = 0;
        char line[96];
        snprintf(line, sizeof(line), "%s%s%s\n", wc_device_type_name(d->type),
                 vendor[0] ? " - " : "", vendor);
        detail_catf(buf, &len, sizeof(buf), line);
        snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X:%02X (%s)\n", d->mac[0], d->mac[1],
                 d->mac[2], d->mac[3], d->mac[4], d->mac[5], d->mac_random ? "random" : "stable");
        detail_catf(buf, &len, sizeof(buf), line);
        snprintf(line, sizeof(line), "RSSI %d  seen %lu\n", (int)d->rssi_max,
                 (unsigned long)d->obs_count);
        detail_catf(buf, &len, sizeof(buf), line);

        if (d->ssid_count > 0) {
            detail_catf(buf, &len, sizeof(buf), "Asks for:");
            for (uint8_t i = 0; i < d->ssid_count; i++) {
                snprintf(line, sizeof(line), "\n %s", d->ssids[i]);
                detail_catf(buf, &len, sizeof(buf), line);
            }
        } else {
            detail_catf(buf, &len, sizeof(buf), "Asks for: (wildcard only)");
        }
        if (!markable) {
            detail_catf(buf, &len, sizeof(buf),
                        "\nRandom MAC, no named net:\ncan't save. Tag its network.");
        }

        widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, buf);
        if (markable) {
            widget_add_button_element(app->widget, GuiButtonTypeCenter, "Mark known",
                                      device_detail_button_cb, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);
}

bool wc_scene_device_detail_on_event(void *context, SceneManagerEvent event) {
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventButton) {
        WcApp *app = context;
        app->mark_mode = WcMarkDevice;
        scene_manager_next_scene(app->scene_manager, WcSceneMarkLabel);
        return true;
    }
    return false;
}

void wc_scene_device_detail_on_exit(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
}

// ---------------------------------------------------------------------------
// Networks sought (directed SSIDs devices are probing for)
// ---------------------------------------------------------------------------

void wc_scene_networks_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "Networks (select to tag)");
    app->list_count = 0;
    if (app->browse_census) {
        WcSsidTally *t = malloc(sizeof(WcSsidTally) * WC_MAX_LIST);
        if (t) {
            uint16_t n = wc_census_ssid_tally(app->browse_census, t, WC_MAX_LIST);
            for (uint16_t i = 0; i < n && i < WC_MAX_LIST; i++) {
                strncpy(app->list_names[i], t[i].ssid, WC_TEXT_BUF_SIZE - 1);
                app->list_names[i][WC_TEXT_BUF_SIZE - 1] = '\0';
                char label[64];
                snprintf(label, sizeof(label), "%s (%u)", t[i].ssid, t[i].devices);
                wc_scroll_list_add_item(app->list_view, label, i, wc_list_cb, app);
                app->list_count++;
            }
            if (n == 0) {
                wc_scroll_list_add_item(app->list_view, "(none sought)", WC_MAX_LIST, wc_list_cb,
                                        app);
            }
            free(t);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_networks_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->selected_ssid, app->list_names[event.event], WC_SSID_MAX_LEN);
        app->selected_ssid[WC_SSID_MAX_LEN] = '\0';
        app->mark_mode = WcMarkSsid;
        scene_manager_next_scene(app->scene_manager, WcSceneMarkLabel);
        return true;
    }
    return false;
}

void wc_scene_networks_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Mark label (turn a device into a known rule)
// ---------------------------------------------------------------------------

void wc_scene_mark_label_on_enter(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
    // Pre-fill a generic name so the user can just confirm without typing.
    const char *header = "Label for device";
    switch (app->mark_mode) {
        case WcMarkSsid:
            header = "Label for network";
            snprintf(app->text_buf, sizeof(app->text_buf), "net_%s", app->selected_ssid);
            break;
        case WcMarkRename:
            header = "Rename";
            if (app->selected_known < app->known.count) {
                snprintf(app->text_buf, sizeof(app->text_buf), "%s",
                         app->known.items[app->selected_known].label);
            } else {
                app->text_buf[0] = '\0';
            }
            break;
        case WcMarkDevice:
        default:
            if (app->browse_census && app->selected_device < app->browse_census->count) {
                const WcSignature *d = &app->browse_census->devices[app->selected_device];
                if (d->ssid_count > 0) {
                    snprintf(app->text_buf, sizeof(app->text_buf), "net_%s", d->ssids[0]);
                } else {
                    snprintf(app->text_buf, sizeof(app->text_buf), "dev_%02X%02X%02X", d->mac[3],
                             d->mac[4], d->mac[5]);
                }
            } else {
                app->text_buf[0] = '\0';
            }
            break;
    }
    app->text_buf[WC_KNOWN_LABEL_MAX] = '\0'; // labels are stored clamped to this length
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_KNOWN_LABEL_MAX + 1, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_mark_label_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        switch (app->mark_mode) {
            case WcMarkSsid:
                wc_known_service_mark_ssid(&app->store, app->selected_ssid, app->text_buf);
                break;
            case WcMarkRename:
                wc_known_service_rename(&app->store, app->selected_known, app->text_buf);
                break;
            case WcMarkDevice:
            default:
                if (app->browse_census && app->selected_device < app->browse_census->count) {
                    const WcSignature *sig = &app->browse_census->devices[app->selected_device];
                    wc_known_service_mark(&app->store, sig, app->text_buf);
                }
                break;
        }
        wc_known_service_load(&app->store, &app->known);
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
    wc_scroll_list_reset(app->list_view);
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
    wc_scroll_list_reset(app->list_view);
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
    populate_files(app, "Merge - pick 1st capture");
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
    wc_scroll_list_reset(app->list_view);
}

void wc_scene_merge_b_on_enter(void *context) {
    WcApp *app = context;
    char header[WC_TEXT_BUF_SIZE + 20];
    snprintf(header, sizeof(header), "2nd to add to %s", app->merge_a);
    populate_files(app, header);
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
    wc_scroll_list_reset(app->list_view);
}

void wc_scene_merge_name_on_enter(void *context) {
    WcApp *app = context;
    wc_default_name(wc_clock_now(&app->clock), "merge", app->text_buf, sizeof(app->text_buf));
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
        uint16_t devices = 0;
        bool ok =
            wc_merge_service_run(&app->store, app->merge_a, app->merge_b, app->text_buf, &devices);
        if (ok) {
            snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
                     "Merged\n%s\n+ %s\n=> %s%s\n\n%u unique devices\n(back = menu)", app->merge_a,
                     app->merge_b, app->text_buf, WC_CAP_EXT, devices);
        } else {
            snprintf(app->result_text, WC_RESULT_TEXT_SIZE, "Merge failed (could not read files).");
        }
        scene_manager_next_scene(app->scene_manager, WcSceneMergeResult);
        return true;
    }
    return false;
}

void wc_scene_merge_name_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

void wc_scene_merge_result_on_enter(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);
}

bool wc_scene_merge_result_on_event(void *context, SceneManagerEvent event) {
    if (event.type == SceneManagerEventTypeBack) {
        WcApp *app = context;
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        return true;
    }
    return false;
}

void wc_scene_merge_result_on_exit(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
}

// ---------------------------------------------------------------------------
// Import: pick a .pcap on the SD, name the census, build it
// ---------------------------------------------------------------------------

void wc_scene_import_pick_on_enter(void *context) {
    WcApp *app = context;
    // Native SD file browser: opens in the app's folder but lets you navigate anywhere under
    // /ext (e.g. Marauder's own capture folder) and pick a .pcap.
    DialogsApp *dialogs = furi_record_open(RECORD_DIALOGS);
    FuriString *path = furi_string_alloc_set(STORAGE_APP_DATA_PATH_PREFIX);
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, ".pcap", NULL);
    opts.base_path = STORAGE_EXT_PATH_PREFIX;
    bool picked = dialog_file_browser_show(dialogs, path, path, &opts);
    if (picked) {
        strncpy(app->import_path, furi_string_get_cstr(path), sizeof(app->import_path) - 1);
        app->import_path[sizeof(app->import_path) - 1] = '\0';
    }
    furi_string_free(path);
    furi_record_close(RECORD_DIALOGS);

    if (picked) {
        scene_manager_next_scene(app->scene_manager, WcSceneImportName);
    } else {
        scene_manager_previous_scene(app->scene_manager); // cancelled
    }
}

bool wc_scene_import_pick_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_import_pick_on_exit(void *context) {
    UNUSED(context);
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
        if (wc_import_service_run(&app->store, app->clock, app->import_path, app->text_buf)) {
            scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        } else {
            show_message(app, "Import failed.\nThe pcap may be too large\n(over 64 KB), not a "
                              "raw-802.11\ncapture, or unreadable.\nUse the PC tool for big ones.");
        }
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
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "Known (select to edit)");
    for (uint16_t i = 0; i < app->known.count; i++) {
        const WcKnown *k = &app->known.items[i];
        char label[64];
        if (k->type == WcRuleBySsid) {
            snprintf(label, sizeof(label), "%s [ssid:%s]", k->label, k->ssid);
        } else {
            snprintf(label, sizeof(label), "%s [%02X:%02X:%02X]", k->label, k->mac[0], k->mac[1],
                     k->mac[2]);
        }
        wc_scroll_list_add_item(app->list_view, label, i, wc_list_cb, app);
    }
    if (app->known.count == 0) {
        wc_scroll_list_add_item(app->list_view, "(none yet)", WC_MAX_LIST, wc_list_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_known_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->known.count) {
        app->selected_known = (uint16_t)event.event;
        scene_manager_next_scene(app->scene_manager, WcSceneKnownActions);
        return true;
    }
    return false;
}

void wc_scene_known_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Known actions (rename / delete the selected known entry)
// ---------------------------------------------------------------------------

enum {
    WcKnownActionRename = 0,
    WcKnownActionDelete = 1,
};

void wc_scene_known_actions_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    const char *lbl = (app->selected_known < app->known.count)
                          ? app->known.items[app->selected_known].label
                          : "?";
    wc_scroll_list_set_header(app->list_view, lbl);
    wc_scroll_list_add_item(app->list_view, "Rename", WcKnownActionRename, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Delete", WcKnownActionDelete, wc_list_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_known_actions_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == WcKnownActionRename) {
            app->mark_mode = WcMarkRename;
            scene_manager_next_scene(app->scene_manager, WcSceneMarkLabel);
            return true;
        }
        if (event.event == WcKnownActionDelete) {
            wc_known_service_remove(&app->store, app->selected_known);
            wc_known_service_load(&app->store, &app->known);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    return false;
}

void wc_scene_known_actions_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
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

static const char *const k_onoff[] = {"Off", "On"};

static void settings_autosave_changed(VariableItem *item) {
    WcApp *app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, k_onoff[idx]);
    app->autosave = (idx == 1);
}

void wc_scene_settings_on_enter(void *context) {
    WcApp *app = context;
    variable_item_list_reset(app->var_item_list);
    VariableItem *item =
        variable_item_list_add(app->var_item_list, "UART baud", 2, settings_baud_changed, app);
    uint8_t idx = (app->baud == k_baud_values[1]) ? 1 : 0;
    variable_item_set_current_value_index(item, idx);
    variable_item_set_current_value_text(item, k_baud_names[idx]);

    VariableItem *as = variable_item_list_add(app->var_item_list, "Auto-save (rotate)", 2,
                                              settings_autosave_changed, app);
    uint8_t as_idx = app->autosave ? 1 : 0;
    variable_item_set_current_value_index(as, as_idx);
    variable_item_set_current_value_text(as, k_onoff[as_idx]);

    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewVarList);
}

bool wc_scene_settings_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_settings_on_exit(void *context) {
    WcApp *app = context;
    wc_settings_persist(app);
    variable_item_list_reset(app->var_item_list);
}

// ---------------------------------------------------------------------------
// Serial debug (show the raw lines Marauder sends, to learn its format)
// ---------------------------------------------------------------------------

// Runs in the serial worker thread. Appends the raw line to a rolling buffer; wraps to keep
// the most recent lines. The GUI timer renders it — a torn read is harmless here.
static void debug_on_line(void *ctx, const char *line, size_t len) {
    WcApp *app = ctx;
    size_t cur = strlen(app->debug_buf);
    if (cur + len + 2 >= sizeof(app->debug_buf)) {
        cur = 0; // wrap: drop older lines to keep the newest
    }
    size_t room = sizeof(app->debug_buf) - cur - 2;
    size_t n = len < room ? len : room;
    memcpy(app->debug_buf + cur, line, n);
    app->debug_buf[cur + n] = '\n';
    app->debug_buf[cur + n + 1] = '\0';
}

void wc_scene_serial_debug_on_enter(void *context) {
    WcApp *app = context;
    app->serial = wc_serial_furi_alloc(app->baud);
    WcSerialPort port = wc_serial_furi_port(app->serial);
    bool ok = port.start(port.self, debug_on_line, app);
    if (!ok) {
        wc_serial_furi_free(app->serial);
        app->serial = NULL;
        snprintf(app->debug_buf, sizeof(app->debug_buf), "%s", k_uart_busy_text);
    } else {
        snprintf(app->debug_buf, sizeof(app->debug_buf), "Waiting for serial...\n");
    }

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->debug_buf);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);

    if (ok) {
        app->scan_timer = furi_timer_alloc(scan_timer_cb, FuriTimerTypePeriodic, app);
        furi_timer_start(app->scan_timer, furi_ms_to_ticks(400));
    }
}

bool wc_scene_serial_debug_on_event(void *context, SceneManagerEvent event) {
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventScanTick) {
        WcApp *app = context;
        text_box_set_text(app->text_box, app->debug_buf);
        return true;
    }
    return false;
}

void wc_scene_serial_debug_on_exit(void *context) {
    WcApp *app = context;
    if (app->scan_timer) {
        furi_timer_stop(app->scan_timer);
        furi_timer_free(app->scan_timer);
        app->scan_timer = NULL;
    }
    scan_serial_stop(app);
    text_box_reset(app->text_box);
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
