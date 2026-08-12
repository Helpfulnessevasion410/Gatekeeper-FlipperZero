/* The portcullis goes up, and the name is behind it. Under two seconds, and
 * any key skips it. */
#include "splash_view.h"

#include <furi.h>
#include <gui/gui.h>
#include <string.h>

#define SP_GX 34
#define SP_GY 2
#define SP_GW 60
#define SP_GH 36
#define SP_LIFT_START 2 /* ticks before the gate begins to rise */
#define SP_LIFT_TICKS 9 /* ticks it takes to open */
#define SP_TOTAL 19 /* ticks before the scene moves on by itself */
#define SP_TITLE_BASE 51
#define SP_TAG_BASE 62

struct SplashView {
    View* view;
    SplashViewCallback skip_cb;
    void* skip_ctx;
};

typedef struct {
    uint8_t anim;
} SplashModel;

/* There is no clipping in the canvas API, so the portcullis is drawn by the
 * segment rather than drawn and masked: as it rises, the part that has left
 * the archway simply is not drawn. */
static void vline_clip(Canvas* c, int x, int y0, int y1, int top, int bottom) {
    if(y0 < top) y0 = top;
    if(y1 > bottom) y1 = bottom;
    if(y1 >= y0) canvas_draw_line(c, x, y0, x, y1);
}

static void hline_clip(Canvas* c, int x0, int x1, int y, int top, int bottom) {
    if(y < top || y > bottom) return;
    canvas_draw_line(c, x0, y, x1, y);
}

static void splash_view_draw(Canvas* canvas, void* model) {
    SplashModel* m = model;
    canvas_clear(canvas);

    const int in_l = SP_GX + 3;
    const int in_r = SP_GX + SP_GW - 4;
    const int in_t = SP_GY + 3;
    const int in_b = SP_GY + SP_GH - 4;

    /* How far the grid has travelled upward. Decelerating, because a gate
     * this heavy does not stop dead. */
    int lift = 0;
    if(m->anim > SP_LIFT_START) {
        int t = m->anim - SP_LIFT_START;
        if(t > SP_LIFT_TICKS) t = SP_LIFT_TICKS;
        int span = in_b - in_t + 6;
        int remain = SP_LIFT_TICKS - t;
        /* Ease out: travel the whole span, but arrive slowly. */
        lift = span - (span * remain * remain) / (SP_LIFT_TICKS * SP_LIFT_TICKS);
    }

    /* The archway itself never moves. */
    canvas_draw_rframe(canvas, SP_GX, SP_GY, SP_GW, SP_GH, 4);
    canvas_draw_line(canvas, SP_GX + 1, SP_GY + SP_GH - 1, SP_GX + SP_GW - 2, SP_GY + SP_GH - 1);

    /* Five bars and two rails, sliding up out of the arch. */
    for(int i = 0; i < 5; i++) {
        int x = in_l + 2 + i * ((in_r - in_l - 4) / 4);
        vline_clip(canvas, x, in_t - lift, in_b - lift + 4, in_t, in_b);
    }
    hline_clip(canvas, in_l, in_r, in_t + 8 - lift, in_t, in_b);
    hline_clip(canvas, in_l, in_r, in_t + 22 - lift, in_t, in_b);

    /* Once it is open, the name is underneath. */
    if(m->anim >= SP_LIFT_START + SP_LIFT_TICKS) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, SP_TITLE_BASE, AlignCenter, AlignBottom, "GATEKEEPER");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, 64, SP_TAG_BASE, AlignCenter, AlignBottom, "Scan before you tap");
    }
}

static bool splash_view_input(InputEvent* event, void* context) {
    SplashView* v = context;
    if(event->type == InputTypeShort || event->type == InputTypePress) {
        if(v->skip_cb) v->skip_cb(v->skip_ctx);
        return true;
    }
    return false;
}

SplashView* splash_view_alloc(void) {
    SplashView* v = malloc(sizeof(SplashView));
    memset(v, 0, sizeof(SplashView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, splash_view_draw);
    view_set_input_callback(v->view, splash_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(SplashModel));
    return v;
}

void splash_view_free(SplashView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* splash_view_get_view(SplashView* v) {
    furi_assert(v);
    return v->view;
}

bool splash_view_tick(SplashView* v) {
    furi_assert(v);
    bool done = false;
    with_view_model(
        v->view,
        SplashModel * m,
        {
            if(m->anim < SP_TOTAL) m->anim++;
            done = m->anim >= SP_TOTAL;
        },
        true);
    return done;
}

void splash_view_set_skip_callback(SplashView* v, SplashViewCallback cb, void* context) {
    furi_assert(v);
    v->skip_cb = cb;
    v->skip_ctx = context;
}
