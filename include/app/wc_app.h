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
// Most capture files / devices / networks shown in a list. Matches the census ceiling so a full
// capture is browsable end to end — a smaller value silently hides devices you can never reach.
#define WC_MAX_LIST WC_CENSUS_MAX_DEVICES
// Auto-save rotates a bit before the hard ceiling so the ~timer-tick window before rotation
// still has room and does not drop devices.
#define WC_AUTOSAVE_ROTATE_AT (WC_CENSUS_MAX_DEVICES - 20)

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
    WcCustomEventButton = 300,
} WcCustomEvent;

// What a label-entry (MarkLabel) applies to when the user confirms.
typedef enum {
    WcMarkDevice, // create a rule from the selected device
    WcMarkSsid,   // create a rule from the selected probed network
    WcMarkRename, // rename the selected known entry
} WcMarkMode;

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
    bool scan_link_ok;     // false when the scan could not open the USART (busy)
    bool autosave;         // rotate to a new file when the census fills, instead of dropping
    char autosave_base[WC_TEXT_BUF_SIZE]; // base name for the current auto-save run
    uint16_t autosave_idx;                // next auto-save file index

    // Browsing / comparison state.
    char text_buf[WC_TEXT_BUF_SIZE]; // capture/label entry
    char selected_file[WC_TEXT_BUF_SIZE];
    char import_path[128]; // absolute path of a pcap picked in the file browser
    char compare_a[WC_TEXT_BUF_SIZE];
    char merge_a[WC_TEXT_BUF_SIZE];
    char merge_b[WC_TEXT_BUF_SIZE];
    char result_text[WC_RESULT_TEXT_SIZE];
    WcCensus *browse_census; // loaded capture, heap, freed on scene exit
    WcCaptureMeta browse_meta;
    uint16_t selected_device;
    WcKnownDb known;
    WcMarkMode mark_mode;                    // what the next MarkLabel confirm applies to
    char selected_ssid[WC_SSID_MAX_LEN + 1]; // network picked in the Networks list
    uint16_t selected_known;                 // index picked in the Known list

    // Scratch for building list submenus (index -> name).
    char list_names[WC_MAX_LIST][WC_TEXT_BUF_SIZE];
    uint16_t list_count;
    const char *list_ext; // extension filter for the current file listing

    char debug_buf[512]; // rolling raw serial lines for the Serial debug view
} WcApp;

// Entry point body (called from main.c). Runs the whole app and returns 0.
int32_t wc_app_run(void);
