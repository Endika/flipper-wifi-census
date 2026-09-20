#include "include/scenes/wc_scenes_common.h"

// ---------------------------------------------------------------------------
// Shared callbacks and helpers
// ---------------------------------------------------------------------------

void wc_list_cb(void *context, uint32_t index) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wc_text_input_cb(void *context) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WcCustomEventTextDone);
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

void wc_populate_files(WcApp *app, const char *header) {
    populate_list(app, header, WC_CAP_EXT, "(no captures)");
}

// ---------------------------------------------------------------------------
// Msg (a one-off message screen)
// ---------------------------------------------------------------------------

// Show app->result_text on a full screen (used for load errors and other one-off messages).
void wc_show_message(WcApp *app, const char *text) {
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
