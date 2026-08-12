/* Everything that was held against this tag, worst first -- and then, below
 * the line, the ceilings.
 *
 * The two halves are genuinely different things and the screen keeps them
 * apart. A finding is something observed about this tag, and it cost points.
 * A ceiling is something that is true about *any* tag of this kind, and it
 * did not cost points: it put a lid on the score. Blurring them would let
 * the application take credit for a low grade it did not earn, or hide the
 * fact that a clean tag is capped at A for reasons that have nothing to do
 * with the tag at all.
 */
#include "findings_view.h"
#include "gk_ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

/* Four rows have to finish above the hint separator at y=53, so the row
 * pitch is set by the space rather than the other way round: 14 + 4*10 = 54,
 * with the last row's box ending at 52. The mockup renderer draws from these
 * same numbers, which is how the original 12px pitch was caught sitting on
 * top of the hint bar. */
#define FV_TOP 14
#define FV_ROW_H 10
#define FV_ROWS 4
#define FV_LIST_H (FV_ROWS * FV_ROW_H - 2)

struct FindingsView {
    View* view;
    FindingsViewCallback cb;
    void* ctx;
};

typedef struct {
    const GkVerdictResult* result;
    uint8_t selected;
    uint8_t top;
} FindingsModel;

/* Findings and ceilings share one scrolling list, because they are one
 * answer to one question. The index space runs findings first, then a
 * separator, then ceilings. */
static uint8_t total_rows(const GkVerdictResult* r) {
    if(!r) return 0;
    return (uint8_t)(r->find_count + r->cap_count);
}

static void draw_row(Canvas* canvas, const GkVerdictResult* r, uint8_t index, int y, bool sel) {
    bool is_cap = index >= r->find_count;

    if(sel) {
        canvas_draw_rbox(canvas, 0, y, 124, FV_ROW_H - 1, 2);
        canvas_set_color(canvas, ColorWhite);
    }

    char right[10] = {0};
    const char* label;

    if(!is_cap) {
        const GkFinding* f = &r->find[index];
        const GkSignalInfo* si = gk_signal_info(f->id);
        label = si->label;
        if(f->points > 0) {
            snprintf(right, sizeof(right), "-%u", (unsigned)f->points);
        } else {
            snprintf(right, sizeof(right), "note");
        }
    } else {
        GkCapId cap = r->caps[index - r->find_count];
        label = gk_cap_label(cap);
        snprintf(right, sizeof(right), "<%u", (unsigned)gk_cap_value(cap));
    }

    canvas_set_font(canvas, FontSecondary);
    uint8_t right_w = (uint8_t)canvas_string_width(canvas, right);

    char shown[40];
    gk_ui_elide(canvas, label, (uint8_t)(118 - right_w - 6), shown, sizeof(shown));
    canvas_draw_str(canvas, 3, y + FV_ROW_H - 3, shown);
    canvas_draw_str_aligned(canvas, 120, y + FV_ROW_H - 3, AlignRight, AlignBottom, right);

    if(sel) canvas_set_color(canvas, ColorBlack);
}

static void findings_view_draw(Canvas* canvas, void* model) {
    FindingsModel* m = model;
    canvas_clear(canvas);

    const GkVerdictResult* r = m->result;
    if(!r) return;

    char head[24];
    snprintf(head, sizeof(head), "%u/100", (unsigned)r->score);
    gk_ui_header(canvas, "Why this grade", head);

    uint8_t rows = total_rows(r);
    if(rows == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, "Nothing was held against it");
        return;
    }

    for(uint8_t i = 0; i < FV_ROWS; i++) {
        uint8_t index = (uint8_t)(m->top + i);
        if(index >= rows) break;
        draw_row(canvas, r, index, FV_TOP + i * FV_ROW_H, index == m->selected);
    }

    if(rows > FV_ROWS) {
        elements_scrollbar_pos(canvas, 126, FV_TOP, FV_LIST_H, m->selected, rows);
    }

    gk_ui_hint(canvas, "OK  Explain this", "Back");
}

static bool findings_view_input(InputEvent* event, void* context) {
    FindingsView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = false;
    bool open = false;
    uint8_t sel = 0;

    with_view_model(
        v->view,
        FindingsModel * m,
        {
            uint8_t rows = total_rows(m->result);
            if(rows == 0) {
                consumed = false;
            } else if(event->key == InputKeyDown) {
                if(m->selected + 1 < rows) m->selected++;
                consumed = true;
            } else if(event->key == InputKeyUp) {
                if(m->selected > 0) m->selected--;
                consumed = true;
            } else if(event->key == InputKeyOk) {
                open = true;
                sel = m->selected;
                consumed = true;
            }

            /* Keep the selection inside the window, scrolling by one. */
            if(m->selected < m->top) m->top = m->selected;
            if(m->selected >= m->top + FV_ROWS) m->top = (uint8_t)(m->selected - FV_ROWS + 1);
        },
        true);

    if(open && v->cb) v->cb(v->ctx, sel);
    return consumed;
}

FindingsView* findings_view_alloc(void) {
    FindingsView* v = malloc(sizeof(FindingsView));
    memset(v, 0, sizeof(FindingsView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, findings_view_draw);
    view_set_input_callback(v->view, findings_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(FindingsModel));
    return v;
}

void findings_view_free(FindingsView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* findings_view_get_view(FindingsView* v) {
    furi_assert(v);
    return v->view;
}

void findings_view_set_data(FindingsView* v, const GkVerdictResult* result) {
    furi_assert(v);
    with_view_model(
        v->view,
        FindingsModel * m,
        {
            m->result = result;
            m->selected = 0;
            m->top = 0;
        },
        true);
}

void findings_view_set_callback(FindingsView* v, FindingsViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}

uint8_t findings_view_selected(FindingsView* v) {
    furi_assert(v);
    uint8_t sel = 0;
    with_view_model(v->view, FindingsModel * m, { sel = m->selected; }, false);
    return sel;
}
