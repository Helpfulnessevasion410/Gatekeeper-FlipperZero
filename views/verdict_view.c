/* The screen the whole application exists to draw.
 *
 * A phishing URL works because people read the wrong part of it. The eye goes
 * to the first familiar word -- "apple", "paypal", "hdfc" -- and stops, and
 * the attacker put that word there precisely because it is free to put it
 * anywhere except the one place that decides where you land.
 *
 * So this screen does not show the address. It shows the *destination*: the
 * registrable domain, on its own, in a black box, under the words "your
 * phone goes to". The full string is one button away for anyone who wants
 * it, with the same eight characters highlighted inside it -- but the first
 * thing on screen is the only part that matters, with nothing around it to
 * read by mistake.
 */
#include "verdict_view.h"
#include "gk_ui.h"

#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

#define VD_BADGE_X 3
#define VD_BADGE_Y 15
#define VD_COL_X 36
#define VD_LABEL_BASE 24
#define VD_DOMAIN_Y 26
#define VD_DOMAIN_H 15
#define VD_INFO_BASE 51

struct VerdictView {
    View* view;
    VerdictViewCallback cb;
    void* ctx;
};

typedef struct {
    const GkTag* tag;
    const GkVerdictResult* result;
} VerdictModel;

static void draw_nothing_to_grade(Canvas* canvas, const GkTag* tag, const GkVerdictResult* r) {
    gk_ui_header(canvas, "No link", NULL);
    gk_ui_badge(canvas, VD_BADGE_X, VD_BADGE_Y, GkGradeNone);

    const char* what;
    const char* why;
    switch(r->status) {
    case GkTagNoNdef:
        what = "Tag is empty";
        why = "Nothing is written on it.";
        break;
    case GkTagNoUrl:
        what = "No address on it";
        why = "It holds data, but no link.";
        break;
    case GkTagNeedsKeys:
        what = "Locked behind keys";
        why = "MIFARE Classic. Not read.";
        break;
    case GkTagUnreadable:
    default:
        what = "Could not be read";
        why = "Hold it steadier and retry.";
        break;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, VD_COL_X, 30, what);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, VD_COL_X, 42, why);

    if(tag->tech_name[0]) {
        canvas_draw_str(canvas, VD_BADGE_X, VD_INFO_BASE, tag->tech_name);
    }
    gk_ui_hint(canvas, "OK  Tag details", "^ Again");
}

static void verdict_view_draw(Canvas* canvas, void* model) {
    VerdictModel* m = model;
    canvas_clear(canvas);

    if(!m->result || !m->tag) return;
    const GkVerdictResult* r = m->result;
    const GkTag* tag = m->tag;

    if(r->grade == GkGradeNone) {
        draw_nothing_to_grade(canvas, tag, r);
        return;
    }

    /* --- header: the verdict, and the number behind it --- */
    char score[12];
    snprintf(score, sizeof(score), "%u/100", (unsigned)r->score);
    gk_ui_header(canvas, gk_verdict_name(r->verdict), score);

    /* --- the grade --- */
    gk_ui_badge(canvas, VD_BADGE_X, VD_BADGE_Y, r->grade);

    /* --- the destination --- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, VD_COL_X, VD_LABEL_BASE, "Your phone goes to:");

    const char* domain = r->registrable[0] ? r->registrable : r->host;
    if(!domain[0]) domain = "(no host)";

    canvas_draw_rbox(canvas, VD_COL_X - 2, VD_DOMAIN_Y, 128 - VD_COL_X, VD_DOMAIN_H, 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);

    char shown[64];
    gk_ui_elide(canvas, domain, (uint8_t)(128 - VD_COL_X - 6), shown, sizeof(shown));
    canvas_draw_str(canvas, VD_COL_X + 1, VD_DOMAIN_Y + 11, shown);
    canvas_set_color(canvas, ColorBlack);

    /* --- what is behind the grade --- */
    canvas_set_font(canvas, FontSecondary);
    char info[40];
    if(r->cap_count > 0) {
        snprintf(
            info,
            sizeof(info),
            "%u finding%s, %u ceiling%s",
            (unsigned)r->find_total,
            r->find_total == 1 ? "" : "s",
            (unsigned)r->cap_count,
            r->cap_count == 1 ? "" : "s");
    } else {
        snprintf(
            info,
            sizeof(info),
            "%u finding%s",
            (unsigned)r->find_total,
            r->find_total == 1 ? "" : "s");
    }
    canvas_draw_str(canvas, VD_BADGE_X, VD_INFO_BASE, info);

    gk_ui_hint(canvas, "OK  Why", "> URL   v Tag");
}

static bool verdict_view_input(InputEvent* event, void* context) {
    VerdictView* v = context;
    if(event->type != InputTypeShort) return false;
    if(!v->cb) return false;

    switch(event->key) {
    case InputKeyOk:
        v->cb(v->ctx, VerdictActionFindings);
        return true;
    case InputKeyRight:
        v->cb(v->ctx, VerdictActionUrl);
        return true;
    case InputKeyDown:
        v->cb(v->ctx, VerdictActionTag);
        return true;
    case InputKeyUp:
        v->cb(v->ctx, VerdictActionRescan);
        return true;
    default:
        return false;
    }
}

VerdictView* verdict_view_alloc(void) {
    VerdictView* v = malloc(sizeof(VerdictView));
    memset(v, 0, sizeof(VerdictView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, verdict_view_draw);
    view_set_input_callback(v->view, verdict_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(VerdictModel));
    return v;
}

void verdict_view_free(VerdictView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* verdict_view_get_view(VerdictView* v) {
    furi_assert(v);
    return v->view;
}

void verdict_view_set_data(VerdictView* v, const GkTag* tag, const GkVerdictResult* result) {
    furi_assert(v);
    with_view_model(
        v->view,
        VerdictModel * m,
        {
            m->tag = tag;
            m->result = result;
        },
        true);
}

void verdict_view_set_callback(VerdictView* v, VerdictViewCallback cb, void* context) {
    furi_assert(v);
    v->cb = cb;
    v->ctx = context;
}
