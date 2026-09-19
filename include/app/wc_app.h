#pragma once

#include "include/application/wc_capture_service.h"
#include "include/application/wc_compare_service.h"
#include "include/application/wc_known_service.h"
#include "include/application/wc_scan_service.h"
#include "include/platform/wc_serial_furi.h"

#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#define WC_TEXT_BUF_SIZE 64
#define WC_RESULT_TEXT_SIZE 1024
#define WC_MAX_LIST 40 // most capture files / devices shown in a list

typedef enum {
    WcViewSubmenu,
    WcViewTextInput,
    WcViewTextBox,
    WcViewWidget,
    WcViewVarList,
} WcViewId;

typedef enum {
    WcCustomEventScanTick = 100,
    WcCustomEventTextDone = 200,
} WcCustomEvent;

typedef struct {
    Gui *gui;
    ViewDispatcher *view_dispatcher;
    SceneManager *scene_manager;
    Submenu *submenu;
    TextInput *text_input;
    TextBox *text_box;
    Widget *widget;
    VariableItemList *var_item_list;

    // Ports and services.
    WcStorePort store;
    WcClockPort clock;
    WcSerialFuri *serial;
    uint32_t baud;
    WcScanService scan;
    FuriTimer *scan_timer;
    uint32_t scan_started; // epoch when the current scan began (for capture duration)

    // Browsing / comparison state.
    char text_buf[WC_TEXT_BUF_SIZE]; // capture/label entry
    char selected_file[WC_TEXT_BUF_SIZE];
    char compare_a[WC_TEXT_BUF_SIZE];
    char result_text[WC_RESULT_TEXT_SIZE];
    WcCensus *browse_census; // loaded capture, heap, freed on scene exit
    WcCaptureMeta browse_meta;
    uint16_t selected_device;
    WcKnownDb known;

    // Scratch for building list submenus (index -> name).
    char list_names[WC_MAX_LIST][WC_TEXT_BUF_SIZE];
    uint16_t list_count;
} WcApp;

// Entry point body (called from main.c). Runs the whole app and returns 0.
int32_t wc_app_run(void);
