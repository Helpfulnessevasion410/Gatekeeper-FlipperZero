/* "Hold the tag against the back."
 *
 * The Flipper's antenna is behind the shell, and nobody knows that on their
 * first scan, so this screen is mostly a drawing of where to put the tag --
 * a Flipper in profile, a tag against its back, and the field between them
 * pulsing outward so it is obvious which way round it goes.
 */
#include "scan_view.h"
#include "gk_ui.h"

#include <furi.h>
#include <gui/gui.h>
#include <string.h>

struct ScanView {
    View* view;
};

typedef struct {
    ScanSnapshot snap;
    uint8_t anim;
} ScanModel;

/* A quarter-arc, built from short chords: canvas_draw_arc does not exist in
 * this SDK, so curves are made rather than drawn. */
static void field_arc(Canvas* c, int cx, int cy, int r) {
    int prev_x = cx, prev_y = cy - r;
    for(int a = -60; a <= 60; a += 15) {
        /* A small integer sine table beats pulling in libm for six points. */
        static const int sin_t[9] = {-87, -71, -50, -26, 0, 26, 50, 71, 87};
        static const int cos_t[9] = {50, 71, 87, 97, 100, 97, 87, 71, 50};
        int i = (a + 60) / 15;
        int x = cx + (r * cos_t[i]) / 100;
        int y = cy + (r * sin_t[i]) / 100;
        if(a > -60) canvas_draw_line(c, prev_x, prev_y, x, y);
        prev_x = x;
        prev_y = y;
    }
}

/* The device, in profile, facing right. */
static void draw_flipper(Canvas* c, int x, int y) {
    canvas_draw_rframe(c, x, y, 26, 30, 3);
    canvas_draw_rframe(c, x + 4, y + 4, 18, 10, 1); /* screen */
    canvas_draw_circle(c, x + 13, y + 22, 5); /* d-pad */
    canvas_draw_dot(c, x + 13, y + 22);
}

static void draw_tag(Canvas* c, int x, int y, bool present) {
    canvas_draw_rframe(c, x, y, 20, 26, 2);
    if(present) {
        /* The little antenna spiral that makes it read as a tag and not a
         * blank card. */
        canvas_draw_rframe(c, x + 3, y + 4, 14, 18, 1);
        canvas_draw_rframe(c, x + 5, y + 6, 10, 14, 1);
        canvas_draw_line(c, x + 7, y + 8, x + 7, y + 17);
    }
}

static void scan_view_draw(Canvas* canvas, void* model) {
    ScanModel* m = model;
    const ScanSnapshot* s = &m->snap;
    canvas_clear(canvas);

    /* A read that finds nothing is not handled here: it still produces a
     * result, and the verdict screen is the place that explains one. */
    gk_ui_header(canvas, s->demo ? "Demo tag" : "Hold tag to the back", NULL);

    draw_flipper(canvas, 6, 18);
    draw_tag(canvas, 100, 20, true);

    /* Three arcs leaving the Flipper, the innermost brightest. Reading is a
     * faster pulse than searching, so the animation says which is happening
     * without needing the word. */
    uint8_t period = (s->state == GkReaderReading) ? 2 : 4;
    for(int k = 0; k < 3; k++) {
        int phase = (m->anim / period + k) % 3;
        int r = 10 + phase * 9;
        field_arc(canvas, 34, 33, r);
    }

    canvas_set_font(canvas, FontSecondary);
    const char* line;
    if(s->state == GkReaderReading) {
        line = "Reading the tag...";
    } else if(s->demo) {
        line = s->demo_name ? s->demo_name : "Scripted tag";
    } else {
        line = "Searching for a tag";
    }
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, line);
}

static void scan_view_no_input(ScanView* v) {
    /* Input belongs to the scene: Back leaves, and nothing else applies while
     * a read is in flight. */
    view_set_input_callback(v->view, NULL);
}

ScanView* scan_view_alloc(void) {
    ScanView* v = malloc(sizeof(ScanView));
    memset(v, 0, sizeof(ScanView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, scan_view_draw);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ScanModel));
    scan_view_no_input(v);
    return v;
}

void scan_view_free(ScanView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* scan_view_get_view(ScanView* v) {
    furi_assert(v);
    return v->view;
}

void scan_view_set(ScanView* v, const ScanSnapshot* snap) {
    furi_assert(v);
    furi_assert(snap);
    with_view_model(v->view, ScanModel * m, { m->snap = *snap; }, true);
}

void scan_view_tick(ScanView* v) {
    furi_assert(v);
    with_view_model(v->view, ScanModel * m, { m->anim++; }, true);
}
