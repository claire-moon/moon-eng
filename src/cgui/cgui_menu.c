#include "cgui_internal.h"

#include <limits.h>
#include <string.h>

static void cgui_menu_event_clear(CguiEvent *event)
{
    if (event == NULL) {
        return;
    }

    event->type = CGUI_EVENT_NONE;
    event->owner_id = CGUI_ID_NONE;
    event->item_id = CGUI_ID_NONE;
}

static CguiResult cgui_menu_require_context(const CguiContext *context)
{
    if (context == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_internal_context_valid(context)) {
        return CGUI_ERR_STATE;
    }
    return CGUI_OK;
}

static size_t cgui_menu_find_item(const CguiMenuState *menu, CguiId item_id)
{
    size_t index;

    if (menu == NULL || item_id == CGUI_ID_NONE) {
        return (size_t)-1;
    }

    for (index = 0u; index < menu->item_count; ++index) {
        if (menu->items[index].id == item_id) {
            return index;
        }
    }
    return (size_t)-1;
}

static CguiId cgui_menu_first_enabled(const CguiMenuState *menu)
{
    size_t index;

    if (menu == NULL) {
        return CGUI_ID_NONE;
    }
    for (index = 0u; index < menu->item_count; ++index) {
        if (menu->items[index].enabled != 0u) {
            return menu->items[index].id;
        }
    }
    return CGUI_ID_NONE;
}

static CguiId cgui_menu_next_enabled(const CguiMenuState *menu,
                                     size_t after_index)
{
    size_t offset;

    if (menu == NULL || menu->item_count == 0u ||
        after_index >= menu->item_count) {
        return CGUI_ID_NONE;
    }
    for (offset = 1u; offset <= menu->item_count; ++offset) {
        size_t candidate;

        candidate = (after_index + offset) % menu->item_count;
        if (menu->items[candidate].enabled != 0u) {
            return menu->items[candidate].id;
        }
    }
    return CGUI_ID_NONE;
}

static int cgui_menu_state_valid(const CguiMenuState *menu)
{
    size_t index;
    size_t other;
    int enabled_found;
    int focus_found;

    if (menu == NULL || menu->item_count > CGUI_MENU_ITEM_CAPACITY) {
        return 0;
    }
    if (menu->owner_id == CGUI_ID_NONE &&
        (menu->item_count != 0u || menu->focus_id != CGUI_ID_NONE)) {
        return 0;
    }

    enabled_found = 0;
    focus_found = menu->focus_id == CGUI_ID_NONE ? 1 : 0;
    for (index = 0u; index < menu->item_count; ++index) {
        const CguiMenuItem *item;

        item = &menu->items[index];
        if (item->id == CGUI_ID_NONE || item->enabled > 1u ||
            !cgui_internal_text_valid(item->label)) {
            return 0;
        }
        for (other = index + 1u; other < menu->item_count; ++other) {
            if (item->id == menu->items[other].id) {
                return 0;
            }
        }
        if (item->enabled != 0u) {
            enabled_found = 1;
            if (item->id == menu->focus_id) {
                focus_found = 1;
            }
        } else if (item->id == menu->focus_id) {
            return 0;
        }
    }

    if (enabled_found != 0 && menu->focus_id == CGUI_ID_NONE) {
        return 0;
    }
    if (enabled_found == 0 && menu->focus_id != CGUI_ID_NONE) {
        return 0;
    }
    return focus_found;
}

static int cgui_help_spec_valid(const CguiHelpSpec *spec)
{
    return spec != NULL && spec->owner_id != CGUI_ID_NONE &&
           cgui_internal_text_valid(spec->title) &&
           cgui_internal_text_valid(spec->body) &&
           cgui_internal_text_valid(spec->close_label);
}

static int cgui_confirm_spec_valid(const CguiConfirmSpec *spec)
{
    return spec != NULL && spec->owner_id != CGUI_ID_NONE &&
           spec->yes_id != CGUI_ID_NONE && spec->no_id != CGUI_ID_NONE &&
           spec->owner_id != spec->yes_id &&
           spec->owner_id != spec->no_id && spec->yes_id != spec->no_id &&
           cgui_internal_text_valid(spec->title) &&
           cgui_internal_text_valid(spec->message) &&
           cgui_internal_text_valid(spec->yes_label) &&
           cgui_internal_text_valid(spec->no_label);
}

static int cgui_modal_state_valid(const CguiModalState *modal)
{
    if (modal == NULL) {
        return 0;
    }

    switch (modal->kind) {
    case CGUI_MODAL_NONE:
        return modal->focus_id == CGUI_ID_NONE;
    case CGUI_MODAL_HELP:
        return modal->focus_id == CGUI_ID_NONE &&
               cgui_help_spec_valid(&modal->spec.help);
    case CGUI_MODAL_CONFIRM:
        return cgui_confirm_spec_valid(&modal->spec.confirm) &&
               (modal->focus_id == modal->spec.confirm.yes_id ||
                modal->focus_id == modal->spec.confirm.no_id);
    default:
        return 0;
    }
}

static void cgui_modal_clear(CguiModalState *modal)
{
    if (modal == NULL) {
        return;
    }
    memset(&modal->spec, 0, sizeof(modal->spec));
    modal->focus_id = CGUI_ID_NONE;
    modal->kind = CGUI_MODAL_NONE;
}

static int cgui_action_pressed(const CguiInputFrame *input,
                               CguiAction action)
{
    return input->actions[action].pressed != 0u;
}

static int cgui_navigation_triggered(const CguiInputFrame *input,
                                     CguiAction action)
{
    return input->actions[action].pressed != 0u ||
           input->actions[action].repeat != 0u;
}

static void cgui_menu_move_focus(CguiMenuState *menu, int direction)
{
    size_t focus_index;
    size_t offset;

    if (menu == NULL || menu->item_count == 0u ||
        menu->focus_id == CGUI_ID_NONE) {
        return;
    }

    focus_index = cgui_menu_find_item(menu, menu->focus_id);
    if (focus_index == (size_t)-1) {
        return;
    }

    for (offset = 1u; offset <= menu->item_count; ++offset) {
        size_t candidate;

        if (direction < 0) {
            size_t backward;

            backward = offset % menu->item_count;
            candidate = (focus_index + menu->item_count - backward) %
                        menu->item_count;
        } else {
            candidate = (focus_index + offset) % menu->item_count;
        }
        if (menu->items[candidate].enabled != 0u) {
            menu->focus_id = menu->items[candidate].id;
            return;
        }
    }
}

static CguiRect cgui_layout_inset(CguiRect rect, int32_t amount)
{
    CguiRect result;
    int64_t width;
    int64_t height;
    int64_t double_amount;

    result = rect;
    width = (int64_t)rect.x1 - (int64_t)rect.x0;
    height = (int64_t)rect.y1 - (int64_t)rect.y0;
    double_amount = (int64_t)amount * INT64_C(2);

    if (width <= double_amount) {
        result.x1 = result.x0;
    } else {
        result.x0 = (int32_t)((int64_t)rect.x0 + (int64_t)amount);
        result.x1 = (int32_t)((int64_t)rect.x1 - (int64_t)amount);
    }
    if (height <= double_amount) {
        result.y1 = result.y0;
    } else {
        result.y0 = (int32_t)((int64_t)rect.y0 + (int64_t)amount);
        result.y1 = (int32_t)((int64_t)rect.y1 - (int64_t)amount);
    }
    return result;
}

static int cgui_layout_rect_nonempty(CguiRect rect)
{
    return rect.x0 < rect.x1 && rect.y0 < rect.y1;
}

static size_t cgui_layout_text_capacity(const CguiContext *context,
                                        CguiRect rect)
{
    uint32_t width;

    if (!cgui_layout_rect_nonempty(rect) || context->font.advance_x == 0u) {
        return 0u;
    }

    /* Ordered int32 endpoints differ by at most UINT32_MAX. */
    width = (uint32_t)((int64_t)rect.x1 - (int64_t)rect.x0);
    return (size_t)(width / (uint32_t)context->font.advance_x);
}

static CguiResult cgui_layout_draw_bounded_text(CguiContext *context,
                                                CguiRect rect,
                                                CguiText text,
                                                CguiPaletteRole role)
{
    CguiText bounded;
    size_t capacity;

    if (!cgui_layout_rect_nonempty(rect) || text.length == 0u) {
        return CGUI_OK;
    }
    capacity = cgui_layout_text_capacity(context, rect);
    if (capacity == 0u) {
        return CGUI_OK;
    }

    bounded = text;
    if (bounded.length > capacity) {
        bounded.length = capacity;
    }
    return cgui_draw_text(context, rect.x0, rect.y0, bounded, role);
}

static CguiResult cgui_layout_draw_wrapped_text(CguiContext *context,
                                                CguiRect rect,
                                                CguiText text,
                                                CguiPaletteRole role)
{
    size_t position;
    size_t line_capacity;
    int64_t y;
    uint32_t line_advance;

    if (!cgui_layout_rect_nonempty(rect) || text.length == 0u) {
        return CGUI_OK;
    }
    line_capacity = cgui_layout_text_capacity(context, rect);
    if (line_capacity == 0u) {
        return CGUI_OK;
    }

    line_advance = context->font.line_advance != 0u
                       ? (uint32_t)context->font.line_advance
                       : (uint32_t)context->font.glyph_height;
    if (line_advance == 0u) {
        return CGUI_ERR_CONFIG;
    }

    position = 0u;
    y = (int64_t)rect.y0;
    while (position < text.length && y < (int64_t)rect.y1) {
        size_t remaining;
        size_t take;
        size_t newline_offset;
        size_t draw_length;
        size_t consume;
        size_t index;
        CguiText line;
        CguiResult result;

        remaining = text.length - position;
        take = remaining < line_capacity ? remaining : line_capacity;
        newline_offset = take;
        for (index = 0u; index < take; ++index) {
            if (text.data[position + index] == '\n' ||
                text.data[position + index] == '\r') {
                newline_offset = index;
                break;
            }
        }

        draw_length = newline_offset;
        if (newline_offset < take) {
            consume = newline_offset + 1u;
            if (text.data[position + newline_offset] == '\r' &&
                position + consume < text.length &&
                text.data[position + consume] == '\n') {
                ++consume;
            }
        } else if (take < remaining) {
            size_t break_at;

            break_at = take;
            while (break_at > 0u &&
                   text.data[position + break_at - 1u] != ' ') {
                --break_at;
            }
            if (break_at > 0u) {
                draw_length = break_at - 1u;
                consume = break_at;
                while (position + consume < text.length &&
                       text.data[position + consume] == ' ') {
                    ++consume;
                }
            } else {
                draw_length = take;
                consume = take;
            }
        } else {
            consume = take;
        }

        line.data = text.data + position;
        line.length = draw_length;
        if (line.length > 0u) {
            result = cgui_draw_text(context,
                                    rect.x0,
                                    (int32_t)y,
                                    line,
                                    role);
            if (result != CGUI_OK) {
                return result;
            }
        }
        position += consume;
        y += (int64_t)line_advance;
    }
    return CGUI_OK;
}

static CguiResult cgui_layout_finish_clip(CguiContext *context,
                                          CguiResult result)
{
    CguiResult pop_result;

    pop_result = cgui_clip_pop(context);
    if (result == CGUI_OK) {
        return pop_result;
    }
    return result;
}

static CguiResult cgui_layout_modal_shell(CguiContext *context,
                                          CguiRect bounds,
                                          CguiRect *panel)
{
    CguiResult result;

    result = cgui_fill_rect(context, bounds, CGUI_ROLE_MODAL_SHADE);
    if (result != CGUI_OK) {
        return result;
    }
    *panel = cgui_layout_inset(bounds, 2);
    if (!cgui_layout_rect_nonempty(*panel)) {
        return CGUI_OK;
    }
    result = cgui_fill_rect(context, *panel, CGUI_ROLE_PANEL);
    if (result != CGUI_OK) {
        return result;
    }
    return cgui_draw_border(context,
                            *panel,
                            CGUI_ROLE_BORDER_LIGHT,
                            CGUI_ROLE_BORDER_DARK);
}

static CguiRect cgui_layout_bottom_row(CguiRect rect, uint32_t height)
{
    CguiRect row;
    int64_t available;

    row = rect;
    available = (int64_t)rect.y1 - (int64_t)rect.y0;
    if (available <= 0) {
        row.y1 = row.y0;
    } else if ((uint64_t)available > (uint64_t)height) {
        row.y0 = (int32_t)((int64_t)rect.y1 - (int64_t)height);
    }
    return row;
}

static CguiResult cgui_layout_draw_button(CguiContext *context,
                                          CguiRect rect,
                                          CguiText label,
                                          int focused)
{
    CguiRect text_rect;
    CguiResult result;
    CguiPaletteRole fill_role;
    CguiPaletteRole text_role;

    if (!cgui_layout_rect_nonempty(rect)) {
        return CGUI_OK;
    }

    fill_role = focused != 0 ? CGUI_ROLE_SELECTION : CGUI_ROLE_PANEL;
    text_role = focused != 0 ? CGUI_ROLE_SELECTION_TEXT : CGUI_ROLE_TEXT;
    result = cgui_fill_rect(context, rect, fill_role);
    if (result != CGUI_OK) {
        return result;
    }
    result = cgui_draw_border(context,
                              rect,
                              focused != 0 ? CGUI_ROLE_FOCUS
                                           : CGUI_ROLE_BORDER_LIGHT,
                              focused != 0 ? CGUI_ROLE_FOCUS
                                           : CGUI_ROLE_BORDER_DARK);
    if (result != CGUI_OK) {
        return result;
    }

    text_rect = cgui_layout_inset(rect, 2);
    if (cgui_layout_rect_nonempty(text_rect)) {
        text_rect.y1 = rect.y1;
    }
    return cgui_layout_draw_bounded_text(context,
                                         text_rect,
                                         label,
                                         text_role);
}

CguiResult cgui_menu_set_items(CguiContext *context,
                               CguiId owner_id,
                               const CguiMenuItem *items,
                               size_t item_count)
{
    CguiMenuItem staged[CGUI_MENU_ITEM_CAPACITY];
    CguiId new_focus;
    CguiResult result;
    size_t index;
    size_t other;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (owner_id == CGUI_ID_NONE || (items == NULL && item_count != 0u)) {
        return CGUI_ERR_ARGUMENT;
    }
    if (item_count > CGUI_MENU_ITEM_CAPACITY) {
        return CGUI_ERR_CAPACITY;
    }

    memset(staged, 0, sizeof(staged));
    for (index = 0u; index < item_count; ++index) {
        if (items[index].id == CGUI_ID_NONE) {
            return CGUI_ERR_ARGUMENT;
        }
        if (!cgui_internal_text_valid(items[index].label)) {
            return CGUI_ERR_CONFIG;
        }
        if (items[index].enabled > 1u) {
            return CGUI_ERR_CONFIG;
        }
        for (other = 0u; other < index; ++other) {
            if (items[other].id == items[index].id) {
                return CGUI_ERR_DUPLICATE;
            }
        }
        staged[index] = items[index];
    }

    new_focus = CGUI_ID_NONE;
    if (context->menu.owner_id == owner_id &&
        context->menu.focus_id != CGUI_ID_NONE) {
        for (index = 0u; index < item_count; ++index) {
            if (staged[index].id == context->menu.focus_id &&
                staged[index].enabled != 0u) {
                new_focus = staged[index].id;
                break;
            }
        }
    }
    if (new_focus == CGUI_ID_NONE) {
        for (index = 0u; index < item_count; ++index) {
            if (staged[index].enabled != 0u) {
                new_focus = staged[index].id;
                break;
            }
        }
    }

    memset(context->menu.items, 0, sizeof(context->menu.items));
    if (item_count != 0u) {
        memcpy(context->menu.items,
               staged,
               item_count * sizeof(context->menu.items[0]));
    }
    context->menu.item_count = item_count;
    context->menu.owner_id = owner_id;
    context->menu.focus_id = new_focus;
    return CGUI_OK;
}

void cgui_menu_clear(CguiContext *context)
{
    if (cgui_menu_require_context(context) != CGUI_OK) {
        return;
    }
    memset(&context->menu, 0, sizeof(context->menu));
}

CguiResult cgui_menu_set_enabled(CguiContext *context,
                                 CguiId item_id,
                                 int enabled)
{
    CguiResult result;
    size_t index;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (item_id == CGUI_ID_NONE) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_menu_state_valid(&context->menu)) {
        return CGUI_ERR_STATE;
    }

    index = cgui_menu_find_item(&context->menu, item_id);
    if (index == (size_t)-1) {
        return CGUI_ERR_NOT_FOUND;
    }
    context->menu.items[index].enabled = enabled != 0 ? 1u : 0u;
    if (context->menu.focus_id == item_id && enabled == 0) {
        context->menu.focus_id = cgui_menu_next_enabled(&context->menu,
                                                        index);
    } else if (context->menu.focus_id == CGUI_ID_NONE && enabled != 0) {
        context->menu.focus_id = cgui_menu_first_enabled(&context->menu);
    }
    return CGUI_OK;
}

CguiResult cgui_menu_set_focus(CguiContext *context, CguiId item_id)
{
    CguiResult result;
    size_t index;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (item_id == CGUI_ID_NONE) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_menu_state_valid(&context->menu)) {
        return CGUI_ERR_STATE;
    }

    index = cgui_menu_find_item(&context->menu, item_id);
    if (index == (size_t)-1) {
        return CGUI_ERR_NOT_FOUND;
    }
    if (context->menu.items[index].enabled == 0u) {
        return CGUI_ERR_DISABLED;
    }
    context->menu.focus_id = item_id;
    return CGUI_OK;
}

CguiId cgui_menu_focus(const CguiContext *context)
{
    if (cgui_menu_require_context(context) != CGUI_OK ||
        !cgui_menu_state_valid(&context->menu)) {
        return CGUI_ID_NONE;
    }
    return context->menu.focus_id;
}

CguiResult cgui_draw_menu(CguiContext *context, CguiRect bounds)
{
    CguiRect content;
    CguiResult result;
    uint32_t row_height;
    size_t index;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (!cgui_internal_rect_valid(bounds)) {
        return CGUI_ERR_RANGE;
    }
    if (!cgui_menu_state_valid(&context->menu) ||
        context->menu.owner_id == CGUI_ID_NONE) {
        return CGUI_ERR_STATE;
    }

    result = cgui_clip_push(context, bounds);
    if (result != CGUI_OK) {
        return result;
    }
    result = cgui_fill_rect(context, bounds, CGUI_ROLE_PANEL);
    if (result == CGUI_OK) {
        result = cgui_draw_border(context,
                                  bounds,
                                  CGUI_ROLE_BORDER_LIGHT,
                                  CGUI_ROLE_BORDER_DARK);
    }

    content = cgui_layout_inset(bounds, 2);
    row_height = (uint32_t)context->font.line_advance + UINT32_C(2);
    if (row_height < 2u) {
        row_height = 2u;
    }
    for (index = 0u; result == CGUI_OK &&
                     index < context->menu.item_count; ++index) {
        int64_t row_y0;
        int64_t row_y1;
        CguiRect row;
        CguiRect text_rect;
        CguiPaletteRole text_role;

        row_y0 = (int64_t)content.y0 +
                 (int64_t)index * (int64_t)row_height;
        if (row_y0 >= (int64_t)content.y1) {
            break;
        }
        row_y1 = row_y0 + (int64_t)row_height;
        if (row_y1 > (int64_t)content.y1) {
            row_y1 = (int64_t)content.y1;
        }
        row.x0 = content.x0;
        row.x1 = content.x1;
        row.y0 = (int32_t)row_y0;
        row.y1 = (int32_t)row_y1;

        if (context->menu.items[index].id == context->menu.focus_id) {
            result = cgui_fill_rect(context, row, CGUI_ROLE_SELECTION);
            if (result == CGUI_OK) {
                result = cgui_draw_border(context,
                                          row,
                                          CGUI_ROLE_FOCUS,
                                          CGUI_ROLE_FOCUS);
            }
            text_role = CGUI_ROLE_SELECTION_TEXT;
        } else if (context->menu.items[index].enabled == 0u) {
            text_role = CGUI_ROLE_TEXT_DISABLED;
        } else {
            text_role = CGUI_ROLE_TEXT;
        }

        text_rect = cgui_layout_inset(row, 2);
        if (result == CGUI_OK) {
            result = cgui_layout_draw_bounded_text(
                context,
                text_rect,
                context->menu.items[index].label,
                text_role);
        }
    }
    return cgui_layout_finish_clip(context, result);
}

CguiResult cgui_help_open(CguiContext *context, const CguiHelpSpec *spec)
{
    CguiHelpSpec staged;
    CguiResult result;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (spec == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_help_spec_valid(spec)) {
        return CGUI_ERR_CONFIG;
    }
    staged = *spec;
    if (!cgui_modal_state_valid(&context->modal) ||
        context->modal.kind != CGUI_MODAL_NONE) {
        return CGUI_ERR_STATE;
    }

    memset(&context->modal.spec, 0, sizeof(context->modal.spec));
    context->modal.spec.help = staged;
    context->modal.focus_id = CGUI_ID_NONE;
    context->modal.kind = CGUI_MODAL_HELP;
    return CGUI_OK;
}

CguiResult cgui_help_close(CguiContext *context)
{
    CguiResult result;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (!cgui_modal_state_valid(&context->modal) ||
        context->modal.kind != CGUI_MODAL_HELP) {
        return CGUI_ERR_STATE;
    }
    cgui_modal_clear(&context->modal);
    return CGUI_OK;
}

int cgui_help_is_open(const CguiContext *context)
{
    return cgui_menu_require_context(context) == CGUI_OK &&
           cgui_modal_state_valid(&context->modal) &&
           context->modal.kind == CGUI_MODAL_HELP;
}

CguiResult cgui_draw_help(CguiContext *context, CguiRect bounds)
{
    CguiRect panel;
    CguiRect inner;
    CguiRect title_rect;
    CguiRect body_rect;
    CguiRect close_rect;
    CguiResult result;
    uint32_t line_height;
    int64_t body_y0;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (!cgui_internal_rect_valid(bounds)) {
        return CGUI_ERR_RANGE;
    }
    if (!cgui_modal_state_valid(&context->modal) ||
        context->modal.kind != CGUI_MODAL_HELP) {
        return CGUI_ERR_STATE;
    }

    result = cgui_clip_push(context, bounds);
    if (result != CGUI_OK) {
        return result;
    }
    result = cgui_layout_modal_shell(context, bounds, &panel);
    inner = cgui_layout_inset(panel, 2);
    line_height = (uint32_t)context->font.line_advance + UINT32_C(2);
    if (line_height < 2u) {
        line_height = 2u;
    }

    title_rect = inner;
    if ((int64_t)title_rect.y0 + (int64_t)line_height <
        (int64_t)title_rect.y1) {
        title_rect.y1 = (int32_t)((int64_t)title_rect.y0 +
                                  (int64_t)line_height);
    }
    close_rect = cgui_layout_bottom_row(inner, line_height + UINT32_C(2));
    body_rect = inner;
    body_y0 = (int64_t)title_rect.y1 + INT64_C(1);
    if (body_y0 > (int64_t)body_rect.y1) {
        body_y0 = (int64_t)body_rect.y1;
    }
    body_rect.y0 = (int32_t)body_y0;
    if ((int64_t)close_rect.y0 - INT64_C(1) <
        (int64_t)body_rect.y1) {
        int64_t body_y1;

        body_y1 = (int64_t)close_rect.y0 - INT64_C(1);
        if (body_y1 < (int64_t)body_rect.y0) {
            body_y1 = (int64_t)body_rect.y0;
        }
        body_rect.y1 = (int32_t)body_y1;
    }

    if (result == CGUI_OK) {
        result = cgui_layout_draw_bounded_text(
            context,
            title_rect,
            context->modal.spec.help.title,
            CGUI_ROLE_WARNING);
    }
    if (result == CGUI_OK) {
        result = cgui_layout_draw_wrapped_text(
            context,
            body_rect,
            context->modal.spec.help.body,
            CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        result = cgui_layout_draw_button(
            context,
            close_rect,
            context->modal.spec.help.close_label,
            1);
    }
    return cgui_layout_finish_clip(context, result);
}

CguiResult cgui_confirm_open(CguiContext *context,
                             const CguiConfirmSpec *spec)
{
    CguiConfirmSpec staged;
    CguiResult result;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (spec == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    if (!cgui_confirm_spec_valid(spec)) {
        return CGUI_ERR_CONFIG;
    }
    staged = *spec;
    if (!cgui_modal_state_valid(&context->modal) ||
        context->modal.kind != CGUI_MODAL_NONE) {
        return CGUI_ERR_STATE;
    }

    memset(&context->modal.spec, 0, sizeof(context->modal.spec));
    context->modal.spec.confirm = staged;
    context->modal.focus_id = staged.no_id;
    context->modal.kind = CGUI_MODAL_CONFIRM;
    return CGUI_OK;
}

CguiResult cgui_confirm_close(CguiContext *context)
{
    CguiResult result;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (!cgui_modal_state_valid(&context->modal) ||
        context->modal.kind != CGUI_MODAL_CONFIRM) {
        return CGUI_ERR_STATE;
    }
    cgui_modal_clear(&context->modal);
    return CGUI_OK;
}

int cgui_confirm_is_open(const CguiContext *context)
{
    return cgui_menu_require_context(context) == CGUI_OK &&
           cgui_modal_state_valid(&context->modal) &&
           context->modal.kind == CGUI_MODAL_CONFIRM;
}

CguiId cgui_confirm_focus(const CguiContext *context)
{
    if (!cgui_confirm_is_open(context)) {
        return CGUI_ID_NONE;
    }
    return context->modal.focus_id;
}

CguiResult cgui_draw_confirm(CguiContext *context, CguiRect bounds)
{
    CguiRect panel;
    CguiRect inner;
    CguiRect title_rect;
    CguiRect message_rect;
    CguiRect buttons;
    CguiRect yes_button;
    CguiRect no_button;
    CguiResult result;
    uint32_t line_height;
    int64_t message_y0;
    int64_t width;
    int64_t left_width;

    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (!cgui_internal_rect_valid(bounds)) {
        return CGUI_ERR_RANGE;
    }
    if (!cgui_modal_state_valid(&context->modal) ||
        context->modal.kind != CGUI_MODAL_CONFIRM) {
        return CGUI_ERR_STATE;
    }

    result = cgui_clip_push(context, bounds);
    if (result != CGUI_OK) {
        return result;
    }
    result = cgui_layout_modal_shell(context, bounds, &panel);
    inner = cgui_layout_inset(panel, 2);
    line_height = (uint32_t)context->font.line_advance + UINT32_C(2);
    if (line_height < 2u) {
        line_height = 2u;
    }

    title_rect = inner;
    if ((int64_t)title_rect.y0 + (int64_t)line_height <
        (int64_t)title_rect.y1) {
        title_rect.y1 = (int32_t)((int64_t)title_rect.y0 +
                                  (int64_t)line_height);
    }
    buttons = cgui_layout_bottom_row(inner, line_height + UINT32_C(2));
    message_rect = inner;
    message_y0 = (int64_t)title_rect.y1 + INT64_C(1);
    if (message_y0 > (int64_t)message_rect.y1) {
        message_y0 = (int64_t)message_rect.y1;
    }
    message_rect.y0 = (int32_t)message_y0;
    if ((int64_t)buttons.y0 - INT64_C(1) <
        (int64_t)message_rect.y1) {
        int64_t message_y1;

        message_y1 = (int64_t)buttons.y0 - INT64_C(1);
        if (message_y1 < (int64_t)message_rect.y0) {
            message_y1 = (int64_t)message_rect.y0;
        }
        message_rect.y1 = (int32_t)message_y1;
    }

    yes_button = buttons;
    no_button = buttons;
    width = (int64_t)buttons.x1 - (int64_t)buttons.x0;
    if (width <= INT64_C(2)) {
        yes_button.x1 = yes_button.x0;
    } else {
        left_width = (width - INT64_C(2)) / INT64_C(2);
        yes_button.x1 = (int32_t)((int64_t)buttons.x0 + left_width);
        no_button.x0 = (int32_t)((int64_t)yes_button.x1 + INT64_C(2));
    }

    if (result == CGUI_OK) {
        result = cgui_layout_draw_bounded_text(
            context,
            title_rect,
            context->modal.spec.confirm.title,
            CGUI_ROLE_WARNING);
    }
    if (result == CGUI_OK) {
        result = cgui_layout_draw_wrapped_text(
            context,
            message_rect,
            context->modal.spec.confirm.message,
            CGUI_ROLE_TEXT);
    }
    if (result == CGUI_OK) {
        result = cgui_layout_draw_button(
            context,
            yes_button,
            context->modal.spec.confirm.yes_label,
            context->modal.focus_id == context->modal.spec.confirm.yes_id);
    }
    if (result == CGUI_OK) {
        result = cgui_layout_draw_button(
            context,
            no_button,
            context->modal.spec.confirm.no_label,
            context->modal.focus_id == context->modal.spec.confirm.no_id);
    }
    return cgui_layout_finish_clip(context, result);
}

CguiModalKind cgui_modal_kind(const CguiContext *context)
{
    if (cgui_menu_require_context(context) != CGUI_OK ||
        !cgui_modal_state_valid(&context->modal)) {
        return CGUI_MODAL_NONE;
    }
    return context->modal.kind;
}

static CguiResult cgui_update_help(CguiContext *context,
                                   const CguiInputFrame *input,
                                   CguiEvent *event)
{
    if (cgui_action_pressed(input, CGUI_ACTION_ESCAPE) ||
        cgui_action_pressed(input, CGUI_ACTION_F1) ||
        cgui_action_pressed(input, CGUI_ACTION_ENTER) ||
        cgui_action_pressed(input, CGUI_ACTION_SPACE)) {
        event->type = CGUI_EVENT_HELP_CLOSE;
        event->owner_id = context->modal.spec.help.owner_id;
        cgui_modal_clear(&context->modal);
    }
    return CGUI_OK;
}

static CguiResult cgui_update_confirm(CguiContext *context,
                                      const CguiInputFrame *input,
                                      CguiEvent *event)
{
    CguiConfirmSpec *confirm;
    int up;
    int down;

    confirm = &context->modal.spec.confirm;
    if (cgui_action_pressed(input, CGUI_ACTION_ESCAPE)) {
        event->type = CGUI_EVENT_CONFIRM_CANCEL;
        event->owner_id = confirm->owner_id;
        cgui_modal_clear(&context->modal);
        return CGUI_OK;
    }

    if (input->accelerator_id != CGUI_ID_NONE) {
        if (input->accelerator_id == confirm->no_id) {
            event->type = CGUI_EVENT_CONFIRM_NO;
            event->owner_id = confirm->owner_id;
            event->item_id = confirm->no_id;
            cgui_modal_clear(&context->modal);
        } else if (input->accelerator_id == confirm->yes_id) {
            event->type = CGUI_EVENT_CONFIRM_YES;
            event->owner_id = confirm->owner_id;
            event->item_id = confirm->yes_id;
            cgui_modal_clear(&context->modal);
        }
        return CGUI_OK;
    }

    if (cgui_action_pressed(input, CGUI_ACTION_F1)) {
        return CGUI_OK;
    }

    up = cgui_navigation_triggered(input, CGUI_ACTION_UP);
    down = cgui_navigation_triggered(input, CGUI_ACTION_DOWN);
    if (up != 0 || down != 0) {
        if ((up != 0) != (down != 0)) {
            context->modal.focus_id =
                context->modal.focus_id == confirm->yes_id
                    ? confirm->no_id
                    : confirm->yes_id;
        }
        return CGUI_OK;
    }

    if (cgui_action_pressed(input, CGUI_ACTION_ENTER) ||
        cgui_action_pressed(input, CGUI_ACTION_SPACE)) {
        event->owner_id = confirm->owner_id;
        event->item_id = context->modal.focus_id;
        event->type = context->modal.focus_id == confirm->yes_id
                          ? CGUI_EVENT_CONFIRM_YES
                          : CGUI_EVENT_CONFIRM_NO;
        cgui_modal_clear(&context->modal);
    }
    return CGUI_OK;
}

static CguiResult cgui_update_menu(CguiContext *context,
                                   const CguiInputFrame *input,
                                   CguiEvent *event)
{
    CguiMenuState *menu;
    size_t index;
    int up;
    int down;

    menu = &context->menu;
    if (menu->owner_id == CGUI_ID_NONE) {
        return CGUI_OK;
    }

    if (cgui_action_pressed(input, CGUI_ACTION_ESCAPE)) {
        event->type = CGUI_EVENT_MENU_CANCEL;
        event->owner_id = menu->owner_id;
        return CGUI_OK;
    }
    if (cgui_action_pressed(input, CGUI_ACTION_F1)) {
        event->type = CGUI_EVENT_HELP_REQUEST;
        event->owner_id = menu->owner_id;
        return CGUI_OK;
    }
    if (input->accelerator_id != CGUI_ID_NONE) {
        index = cgui_menu_find_item(menu, input->accelerator_id);
        if (index != (size_t)-1 && menu->items[index].enabled != 0u) {
            menu->focus_id = menu->items[index].id;
            event->type = CGUI_EVENT_MENU_ACTIVATE;
            event->owner_id = menu->owner_id;
            event->item_id = menu->items[index].id;
        }
        return CGUI_OK;
    }

    up = cgui_navigation_triggered(input, CGUI_ACTION_UP);
    down = cgui_navigation_triggered(input, CGUI_ACTION_DOWN);
    if (up != 0 || down != 0) {
        if (up != 0 && down == 0) {
            cgui_menu_move_focus(menu, -1);
        } else if (down != 0 && up == 0) {
            cgui_menu_move_focus(menu, 1);
        }
        return CGUI_OK;
    }

    if ((cgui_action_pressed(input, CGUI_ACTION_ENTER) ||
         cgui_action_pressed(input, CGUI_ACTION_SPACE)) &&
        menu->focus_id != CGUI_ID_NONE) {
        event->type = CGUI_EVENT_MENU_ACTIVATE;
        event->owner_id = menu->owner_id;
        event->item_id = menu->focus_id;
    }
    return CGUI_OK;
}

CguiResult cgui_update(CguiContext *context,
                       const CguiInputFrame *input,
                       CguiEvent *event)
{
    CguiResult result;

    cgui_menu_event_clear(event);
    if (context == NULL || input == NULL || event == NULL) {
        return CGUI_ERR_ARGUMENT;
    }
    result = cgui_menu_require_context(context);
    if (result != CGUI_OK) {
        return result;
    }
    if (!cgui_menu_state_valid(&context->menu) ||
        !cgui_modal_state_valid(&context->modal)) {
        return CGUI_ERR_STATE;
    }

    switch (context->modal.kind) {
    case CGUI_MODAL_HELP:
        return cgui_update_help(context, input, event);
    case CGUI_MODAL_CONFIRM:
        return cgui_update_confirm(context, input, event);
    case CGUI_MODAL_NONE:
        return cgui_update_menu(context, input, event);
    default:
        return CGUI_ERR_STATE;
    }
}
