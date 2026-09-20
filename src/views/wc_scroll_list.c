#include "include/views/wc_scroll_list.h"

#include <furi.h>
#include <gui/elements.h>

// How often the selected item's text advances one step. Matches the firmware's own
// VariableItemList so the two read as the same motion.
#define WC_SCROLL_LIST_TICK_MS 333

typedef struct {
    char label[WC_SCROLL_LIST_LABEL_MAX];
    uint32_t index;
    WcScrollListCb callback;
    void *callback_context;
} WcScrollListItem;

typedef struct {
    WcScrollListItem items[WC_SCROLL_LIST_MAX];
    size_t count;
    size_t position;
    size_t window_position;
    size_t scroll_counter;
    bool selected_overflows; // set while drawing: whether the selected label needs to move
    FuriString *header;
    FuriString *scratch; // the firmware element takes a FuriString, the labels are char[]
} WcScrollListModel;

struct WcScrollList {
    View *view;
    FuriTimer *scroll_timer;
};

static size_t items_on_screen(const WcScrollListModel *model) {
    return furi_string_empty(model->header) ? 4 : 3;
}

static void wc_scroll_list_draw_callback(Canvas *canvas, void *_model) {
    WcScrollListModel *model = _model;
    const uint8_t item_height = 16;
    const uint8_t item_width = canvas_width(canvas) - 5;

    canvas_clear(canvas);

    if (!furi_string_empty(model->header)) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 4, 11, furi_string_get_cstr(model->header));
    }

    canvas_set_font(canvas, FontSecondary);

    const size_t on_screen = items_on_screen(model);
    const uint8_t y_offset = furi_string_empty(model->header) ? 0 : item_height;

    for (size_t position = 0; position < model->count; position++) {
        const size_t item_position = position - model->window_position;
        if (item_position >= on_screen) {
            continue;
        }
        const uint8_t item_y = y_offset + (uint8_t)(item_position * item_height);
        const bool selected = (position == model->position);
        size_t scroll = 0;

        if (selected) {
            canvas_set_color(canvas, ColorBlack);
            elements_slightly_rounded_box(canvas, 0, item_y + 1, item_width, item_height - 2);
            canvas_set_color(canvas, ColorWhite);
            // Hold on the start of the text for one extra tick before it starts moving.
            scroll = (model->scroll_counter < 1) ? 0 : model->scroll_counter - 1;
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        furi_string_set_str(model->scratch, model->items[position].label);
        const size_t text_width = item_width - 11;
        if (selected) {
            model->selected_overflows =
                canvas_string_width(canvas, furi_string_get_cstr(model->scratch)) > text_width;
        }
        elements_scrollable_text_line(canvas, 6, item_y + item_height - 4, text_width,
                                      model->scratch, scroll, !selected);
    }

    elements_scrollbar(canvas, model->position, model->count);
}

static void wc_scroll_list_process_up(WcScrollList *list) {
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            const size_t on_screen = items_on_screen(model);
            if (model->position > 0) {
                model->position--;
                if ((model->position == model->window_position) && (model->window_position > 0)) {
                    model->window_position--;
                }
            } else {
                model->position = model->count - 1;
                if (model->position > on_screen - 1) {
                    model->window_position = model->position - (on_screen - 1);
                }
            }
            model->scroll_counter = 0;
        },
        true);
}

static void wc_scroll_list_process_down(WcScrollList *list) {
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            const size_t on_screen = items_on_screen(model);
            if (model->position < model->count - 1) {
                model->position++;
                if ((model->position - model->window_position > on_screen - 2) &&
                    (model->window_position < model->count - on_screen)) {
                    model->window_position++;
                }
            } else {
                model->position = 0;
                model->window_position = 0;
            }
            model->scroll_counter = 0;
        },
        true);
}

static void wc_scroll_list_process_ok(WcScrollList *list) {
    WcScrollListCb callback = NULL;
    void *context = NULL;
    uint32_t index = 0;

    with_view_model(
        list->view, WcScrollListModel * model,
        {
            if (model->position < model->count) {
                callback = model->items[model->position].callback;
                context = model->items[model->position].callback_context;
                index = model->items[model->position].index;
            }
        },
        false);

    if (callback) {
        callback(context, index);
    }
}

// cppcheck-suppress constParameterCallback // the firmware's ViewInputCallback is not const
static bool wc_scroll_list_input_callback(InputEvent *event, void *context) {
    WcScrollList *list = context;
    furi_assert(list);

    size_t count = 0;
    with_view_model(list->view, const WcScrollListModel *model, { count = model->count; }, false);
    if (count == 0) {
        return false;
    }

    bool consumed = false;
    if (event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if (event->key == InputKeyUp) {
            wc_scroll_list_process_up(list);
            consumed = true;
        } else if (event->key == InputKeyDown) {
            wc_scroll_list_process_down(list);
            consumed = true;
        }
    }
    if (!consumed && event->key == InputKeyOk && event->type == InputTypeShort) {
        wc_scroll_list_process_ok(list);
        consumed = true;
    }
    return consumed;
}

static void wc_scroll_list_timer_callback(void *context) {
    WcScrollList *list = context;
    with_view_model(list->view, WcScrollListModel * model, { model->scroll_counter++; }, true);
}

// The timer only runs while the list is the visible view: a list off screen has nothing to
// animate, and a periodic timer left running keeps waking the CPU.
static void wc_scroll_list_enter_callback(void *context) {
    WcScrollList *list = context;
    with_view_model(list->view, WcScrollListModel * model, { model->scroll_counter = 0; }, true);
    furi_timer_start(list->scroll_timer, furi_ms_to_ticks(WC_SCROLL_LIST_TICK_MS));
}

static void wc_scroll_list_exit_callback(void *context) {
    WcScrollList *list = context;
    furi_timer_stop(list->scroll_timer);
}

WcScrollList *wc_scroll_list_alloc(void) {
    WcScrollList *list = malloc(sizeof(WcScrollList));
    list->view = view_alloc();
    view_set_context(list->view, list);
    view_allocate_model(list->view, ViewModelTypeLocking, sizeof(WcScrollListModel));
    view_set_draw_callback(list->view, wc_scroll_list_draw_callback);
    view_set_input_callback(list->view, wc_scroll_list_input_callback);
    view_set_enter_callback(list->view, wc_scroll_list_enter_callback);
    view_set_exit_callback(list->view, wc_scroll_list_exit_callback);

    list->scroll_timer =
        furi_timer_alloc(wc_scroll_list_timer_callback, FuriTimerTypePeriodic, list);

    with_view_model(
        list->view, WcScrollListModel * model,
        {
            model->count = 0;
            model->position = 0;
            model->window_position = 0;
            model->scroll_counter = 0;
            model->selected_overflows = false;
            model->header = furi_string_alloc();
            model->scratch = furi_string_alloc();
        },
        true);
    return list;
}

void wc_scroll_list_free(WcScrollList *list) {
    furi_check(list);
    furi_timer_stop(list->scroll_timer);
    furi_timer_free(list->scroll_timer);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            furi_string_free(model->header);
            furi_string_free(model->scratch);
        },
        false);
    view_free(list->view);
    free(list);
}

View *wc_scroll_list_get_view(WcScrollList *list) {
    furi_check(list);
    return list->view;
}

void wc_scroll_list_reset(WcScrollList *list) {
    furi_check(list);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            model->count = 0;
            model->position = 0;
            model->window_position = 0;
            model->scroll_counter = 0;
            furi_string_reset(model->header);
        },
        true);
}

void wc_scroll_list_set_header(WcScrollList *list, const char *header) {
    furi_check(list);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            if (header) {
                furi_string_set_str(model->header, header);
            } else {
                furi_string_reset(model->header);
            }
        },
        true);
}

void wc_scroll_list_add_item(WcScrollList *list, const char *label, uint32_t index,
                             WcScrollListCb callback, void *callback_context) {
    furi_check(list);
    furi_check(label);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            if (model->count < WC_SCROLL_LIST_MAX) {
                WcScrollListItem *item = &model->items[model->count];
                snprintf(item->label, sizeof(item->label), "%s", label);
                item->index = index;
                item->callback = callback;
                item->callback_context = callback_context;
                model->count++;
            }
        },
        true);
}

void wc_scroll_list_set_selected_item(WcScrollList *list, uint32_t index) {
    furi_check(list);
    with_view_model(
        list->view, WcScrollListModel * model,
        {
            size_t position = 0;
            for (size_t i = 0; i < model->count; i++) {
                if (model->items[i].index == index) {
                    position = i;
                    break;
                }
            }
            model->position = position;
            model->window_position = (position > 0) ? position - 1 : 0;
            model->scroll_counter = 0;

            const size_t on_screen = items_on_screen(model);
            if (model->count <= on_screen) {
                model->window_position = 0;
            } else if (model->window_position > model->count - on_screen) {
                model->window_position = model->count - on_screen;
            }
        },
        true);
}
