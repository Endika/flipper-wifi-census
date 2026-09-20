#pragma once

#include <gui/view.h>

#include <stddef.h>
#include <stdint.h>

// A selection list shaped like the firmware's Submenu, with one difference: the selected item's
// text scrolls when it does not fit instead of being cut with an ellipsis. Capture names, MACs
// and SSIDs are routinely wider than the screen, and the ellipsis hides exactly the part that
// tells two of them apart.
//
// Labels are NOT stored. An item either points at a string the caller keeps alive (a literal,
// or a buffer inside WcApp), or has its label generated on demand while drawing. At a hundred
// devices a stored copy costs 64 bytes each - more than the WcSignature it describes - and
// that cost is what pins the device ceiling, so the list reads the census instead of duplicating
// it.
typedef struct WcScrollList WcScrollList;

typedef void (*WcScrollListCb)(void *context, uint32_t index);

// Writes the label for `index` into `out`. Called only for the rows actually on screen, and
// `const` on purpose: rendering a row must never change what is being rendered.
typedef void (*WcScrollListLabelFn)(const void *context, uint32_t index, char *out, size_t cap);

// The selected item was nudged left (-1) or right (+1).
typedef void (*WcScrollListNudgeFn)(void *context, uint32_t index, int8_t delta);

// Ceiling on items in one list, and the widest label drawn. Covers the longest list the app
// builds (WC_MAX_LIST entries plus an "empty" placeholder); extra items are dropped and a
// longer label is truncated, rather than overflowing. The array is flat and preallocated: a
// Flipper heap fragmented by a live census is not a place to grow a list.
#define WC_SCROLL_LIST_MAX 110
#define WC_SCROLL_LIST_LABEL_MAX 64

WcScrollList *wc_scroll_list_alloc(void);
void wc_scroll_list_free(WcScrollList *list);
View *wc_scroll_list_get_view(WcScrollList *list);

void wc_scroll_list_reset(WcScrollList *list);
// The header is copied, so a caller may build it in a temporary buffer.
void wc_scroll_list_set_header(WcScrollList *list, const char *header);
// An item labelled by a string the caller owns and keeps alive while the list is on screen.
void wc_scroll_list_add_item(WcScrollList *list, const char *label, uint32_t index,
                             WcScrollListCb callback, void *callback_context);

// `count` items carrying indices 0..count-1, whose labels `label_fn` writes on demand. Nothing
// is copied: the rows are rendered straight from whatever the caller already holds in memory.
void wc_scroll_list_add_generated(WcScrollList *list, uint16_t count, WcScrollListLabelFn label_fn,
                                  WcScrollListCb callback, void *callback_context);
// Give ONE item a value drawn on its right and changed with left/right, leaving OK to do what
// the row already did. One is enough for the "decide it right here" option a menu needs, and
// keeping it off the item struct costs nothing per row. `value` is borrowed like a label.
void wc_scroll_list_set_item_value(WcScrollList *list, uint32_t index, const char *value,
                                   WcScrollListNudgeFn on_nudge, void *context);

// Entries the caller could not even offer, because its own buffers filled first (captures past
// WC_MAX_LIST, SSIDs past the tally). Added to what the list itself had to refuse; the total is
// drawn on the header row. The count is what makes a short list honest, so the view draws it
// itself rather than trusting every scene to remember.
void wc_scroll_list_note_hidden(WcScrollList *list, uint16_t n);
