/* The whole address, with the eight characters that decide where you go
 * picked out of it.
 *
 * Monospace on purpose. A URL is not prose and proportional spacing makes
 * "rn" and "m" line up differently on different lines, which is the exact
 * confusion this screen exists to remove. A fixed grid also means the
 * highlight is a rectangle rather than a measurement, so it can never end up
 * a pixel off the characters it is supposed to be under.
 */
#include "url_view.h"
#include "gk_ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <string.h>

#define UV_TOP 15 /* first text row's top edge */
#define UV_LINE_H 9
#define UV_ROWS 4
#define UV_LEFT 2

struct UrlView {
    View* view;
};

typedef struct {
    char url[GK_URL_MAX];
    uint16_t len;
    GkUrlZones zones;
    uint8_t top_line; /* first visible row */
    uint8_t total_lines;
    uint8_t cols;
} UrlModel;

/* Draw a run of characters starting at absolute offset `from`, length `n`,
 * at column `col` of row `row`, in the given colour. */
static void draw_run(
    Canvas* canvas,
    const UrlModel* m,
    uint16_t from,
    uint16_t n,
    uint8_t col,
    uint8_t row,
    Color color) {
    if(n == 0) return;
    char buf[40];
    if(n > sizeof(buf) - 1) n = sizeof(buf) - 1;
    memcpy(buf, m->url + from, n);
    buf[n] = '\0';

    uint8_t cw = (uint8_t)(canvas_string_width(canvas, "0"));
    int x = UV_LEFT + col * cw;
    int y = UV_TOP + row * UV_LINE_H;

    if(color == ColorWhite) {
        canvas_draw_box(canvas, x, y, (int)n * cw, UV_LINE_H - 1);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, x, y + UV_LINE_H - 2, buf);
    if(color == ColorWhite) canvas_set_color(canvas, ColorBlack);
}

static void url_view_draw(Canvas* canvas, void* model) {
    UrlModel* m = model;
    canvas_clear(canvas);
    gk_ui_header(canvas, "The full address", NULL);

    if(m->len == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, "No address on this tag");
        return;
    }

    canvas_set_font(canvas, FontKeyboard);
    uint8_t cols = m->cols ? m->cols : 20;

    uint16_t reg_start = m->zones.reg_off;
    uint16_t reg_end = (uint16_t)(m->zones.reg_off + m->zones.reg_len);

    for(uint8_t row = 0; row < UV_ROWS; row++) {
        uint16_t line = (uint16_t)(m->top_line + row);
        uint16_t start = (uint16_t)(line * cols);
        if(start >= m->len) break;
        uint16_t end = (uint16_t)(start + cols);
        if(end > m->len) end = m->len;

        /* Up to three runs per row: before the destination, the destination,
         * and after it. Any of them may be empty on a given row. */
        uint16_t a0 = start;
        uint16_t a1 = (reg_start > start) ? (reg_start < end ? reg_start : end) : start;
        uint16_t b0 = a1;
        uint16_t b1 = (reg_end > b0) ? (reg_end < end ? reg_end : end) : b0;

        draw_run(canvas, m, a0, (uint16_t)(a1 - a0), (uint8_t)(a0 - start), row, ColorBlack);
        draw_run(canvas, m, b0, (uint16_t)(b1 - b0), (uint8_t)(b0 - start), row, ColorWhite);
        draw_run(canvas, m, b1, (uint16_t)(end - b1), (uint8_t)(b1 - start), row, ColorBlack);
    }

    if(m->total_lines > UV_ROWS) {
        elements_scrollbar_pos(
            canvas, 126, UV_TOP, UV_ROWS * UV_LINE_H, m->top_line, (uint8_t)(m->total_lines - UV_ROWS + 1));
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 53, 127, 53);
    /* A worked example of the legend, rather than a sentence about it. */
    canvas_draw_box(canvas, 2, 56, 7, 7);
    canvas_draw_str(canvas, 12, 62, "= where your phone goes");
}

static bool url_view_input(InputEvent* event, void* context) {
    UrlView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key != InputKeyUp && event->key != InputKeyDown) return false;

    with_view_model(
        v->view,
        UrlModel * m,
        {
            if(event->key == InputKeyDown) {
                if(m->total_lines > UV_ROWS && m->top_line < m->total_lines - UV_ROWS) {
                    m->top_line++;
                }
            } else if(m->top_line > 0) {
                m->top_line--;
            }
        },
        true);
    return true;
}

UrlView* url_view_alloc(void) {
    UrlView* v = malloc(sizeof(UrlView));
    memset(v, 0, sizeof(UrlView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, url_view_draw);
    view_set_input_callback(v->view, url_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(UrlModel));
    return v;
}

void url_view_free(UrlView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

void url_view_set_url(UrlView* v, const char* url) {
    furi_assert(v);

    GkUrl u;
    bool ok = url && gk_url_parse(url, 0, &u);
    GkUrlZones z;
    memset(&z, 0, sizeof(z));
    if(ok) gk_url_zones(&u, &z);

    with_view_model(
        v->view,
        UrlModel * m,
        {
            memset(m->url, 0, sizeof(m->url));
            if(ok) {
                m->len = u.raw_len;
                memcpy(m->url, url, m->len);
            } else {
                m->len = 0;
            }
            m->zones = z;
            m->top_line = 0;

            /* FontKeyboard is fixed pitch at six pixels. The screen is 128
             * wide; two go to the left margin and the rest of the slack to
             * the scrollbar, which leaves twenty columns. Fixed rather than
             * measured because canvas metrics are only available inside a
             * draw callback, and the wrap has to be known before one runs. */
            m->cols = 20;
            m->total_lines =
                (uint8_t)((m->len + m->cols - 1) / (m->cols ? m->cols : 1));
            if(m->total_lines == 0) m->total_lines = 1;
        },
        true);
}

View* url_view_get_view(UrlView* v) {
    furi_assert(v);
    return v->view;
}
