/* One finding, explained.
 *
 * Three questions in the same order every time: what was seen, why it
 * matters, what to do about it. The order is the point -- an explanation
 * that opens with advice is a lecture, and one that never reaches advice is
 * a shrug. The evidence is quoted rather than described, because "reads as
 * paypal" means nothing until you can see which characters did it.
 */
#include "detail_view.h"
#include "gk_ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

#define DV_TOP 15
#define DV_LINE_H 9
#define DV_ROWS 4
#define DV_MAX_LINES 24

struct DetailView {
    View* view;
};

typedef struct {
    char title[36];
    char right[10];
    char lines[DV_MAX_LINES][40];
    uint8_t line_count;
    uint8_t top;
    /* The body is wrapped on first draw, when canvas metrics exist. Until
     * then only the source strings are known. */
    bool wrapped;
    const char* what;
    const char* why;
    const char* advice;
    char evidence[GK_EVIDENCE_MAX + 8];
} DetailModel;

/* Append a block of body text, preceded by a blank line if anything is
 * already there. Returns the new line count. */
static uint8_t add_block(
    Canvas* canvas,
    DetailModel* m,
    uint8_t at,
    const char* heading,
    const char* text) {
    if(!text || !text[0] || at >= DV_MAX_LINES) return at;

    if(at > 0 && at < DV_MAX_LINES) {
        m->lines[at][0] = '\0';
        at++;
    }
    if(heading && at < DV_MAX_LINES) {
        /* Headings are marked with a leading bullet the draw pass turns into
         * bold; keeping the marker in the string keeps wrapping honest. */
        snprintf(m->lines[at], sizeof(m->lines[at]), GK_HEADING_MARK "%s", heading);
        at++;
    }

    char wrapped[DV_MAX_LINES][40];
    uint8_t n = gk_ui_wrap(canvas, text, 122, wrapped, (uint8_t)(DV_MAX_LINES - at));
    for(uint8_t i = 0; i < n && at < DV_MAX_LINES; i++) {
        memcpy(m->lines[at], wrapped[i], sizeof(wrapped[i]));
        at++;
    }
    return at;
}

static void detail_view_draw(Canvas* canvas, void* model) {
    DetailModel* m = model;
    canvas_clear(canvas);
    gk_ui_header(canvas, m->title, m->right);

    if(!m->wrapped) {
        canvas_set_font(canvas, FontSecondary);
        uint8_t at = 0;
        if(m->evidence[0]) at = add_block(canvas, m, at, NULL, m->evidence);
        at = add_block(canvas, m, at, NULL, m->what);
        at = add_block(canvas, m, at, "Why it matters", m->why);
        at = add_block(canvas, m, at, "What to do", m->advice);
        m->line_count = at;
        m->wrapped = true;
    }

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < DV_ROWS; i++) {
        uint8_t index = (uint8_t)(m->top + i);
        if(index >= m->line_count) break;
        const char* text = m->lines[index];
        int y = DV_TOP + i * DV_LINE_H + DV_LINE_H - 2;
        if(text[0] == GK_HEADING_MARK[0]) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, y, text + 1);
            canvas_set_font(canvas, FontSecondary);
        } else {
            canvas_draw_str(canvas, 2, y, text);
        }
    }

    if(m->line_count > DV_ROWS) {
        elements_scrollbar_pos(
            canvas, 126, DV_TOP, DV_ROWS * DV_LINE_H, m->top, (uint8_t)(m->line_count - DV_ROWS + 1));
    }

    gk_ui_hint(canvas, "^v Scroll", "Back");
}

static bool detail_view_input(InputEvent* event, void* context) {
    DetailView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key != InputKeyUp && event->key != InputKeyDown) return false;

    with_view_model(
        v->view,
        DetailModel * m,
        {
            if(event->key == InputKeyDown) {
                if(m->line_count > DV_ROWS && m->top < m->line_count - DV_ROWS) m->top++;
            } else if(m->top > 0) {
                m->top--;
            }
        },
        true);
    return true;
}

DetailView* detail_view_alloc(void) {
    DetailView* v = malloc(sizeof(DetailView));
    memset(v, 0, sizeof(DetailView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, detail_view_draw);
    view_set_input_callback(v->view, detail_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(DetailModel));
    return v;
}

void detail_view_free(DetailView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* detail_view_get_view(DetailView* v) {
    furi_assert(v);
    return v->view;
}

void detail_view_set_finding(DetailView* v, GkSignalId id, const char* evidence, uint8_t points) {
    furi_assert(v);
    const GkSignalInfo* si = gk_signal_info(id);

    with_view_model(
        v->view,
        DetailModel * m,
        {
            memset(m, 0, sizeof(*m));
            strncpy(m->title, si->label, sizeof(m->title) - 1);
            if(points > 0) {
                snprintf(m->right, sizeof(m->right), "-%u", (unsigned)points);
            } else {
                snprintf(m->right, sizeof(m->right), "note");
            }
            if(evidence && evidence[0]) {
                snprintf(m->evidence, sizeof(m->evidence), "\"%s\"", evidence);
            }
            m->what = si->what;
            m->why = si->why;
            m->advice = si->advice;
            m->wrapped = false;
        },
        true);
}

void detail_view_set_cap(DetailView* v, GkCapId cap) {
    furi_assert(v);
    with_view_model(
        v->view,
        DetailModel * m,
        {
            memset(m, 0, sizeof(*m));
            strncpy(m->title, gk_cap_label(cap), sizeof(m->title) - 1);
            snprintf(m->right, sizeof(m->right), "max %u", (unsigned)gk_cap_value(cap));
            m->what = "This is a ceiling, not a deduction.";
            m->why = gk_cap_reason(cap);
            m->advice = "";
            m->wrapped = false;
        },
        true);
}
