#pragma once

#include <gui/view.h>

#include <stdint.h>

// A selection list shaped like the firmware's Submenu, with one difference: the selected item's
// text scrolls when it does not fit instead of being cut with an ellipsis. Capture names, MACs
// and SSIDs are routinely wider than the screen, and the ellipsis hides exactly the part that
// tells two of them apart.
//
// Labels are copied, like Submenu's: half the scenes build theirs in a stack buffer.
typedef struct WcScrollList WcScrollList;

typedef void (*WcScrollListCb)(void *context, uint32_t index);

// Ceiling on items in one list, and on a label. Covers the longest list the app builds
// (WC_MAX_LIST entries plus an "empty" placeholder) and its widest label; extra items are
// dropped and a longer label is truncated, rather than overflowing. The array is flat and
// preallocated: a Flipper heap fragmented by a live census is not a place to grow a list.
#define WC_SCROLL_LIST_MAX 110
#define WC_SCROLL_LIST_LABEL_MAX 64

WcScrollList *wc_scroll_list_alloc(void);
void wc_scroll_list_free(WcScrollList *list);
View *wc_scroll_list_get_view(WcScrollList *list);

void wc_scroll_list_reset(WcScrollList *list);
// The header is copied, so a caller may build it in a temporary buffer.
void wc_scroll_list_set_header(WcScrollList *list, const char *header);
void wc_scroll_list_add_item(WcScrollList *list, const char *label, uint32_t index,
                             WcScrollListCb callback, void *callback_context);
// Selects the item carrying `index` (the value passed to add_item), scrolling it into view.
void wc_scroll_list_set_selected_item(WcScrollList *list, uint32_t index);
