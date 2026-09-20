#include "include/application/wc_import_service.h"
#include "include/domain/wc_timefmt.h"
#include "include/scenes/wc_scenes_common.h"

#include <dialogs/dialogs.h>
#include <storage/storage.h>

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
            wc_show_message(app,
                            "Import failed.\nThe pcap may be too large\n(over 64 KB), not a "
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
