/* Six panels on how the attack actually works.
 *
 * Every one of them animates the same idea from a different angle: the
 * string you read and the string the phone obeys are not the same string.
 * Watching the highlight jump to the part nobody looks at does more in two
 * seconds than a paragraph does in twenty, which is why the tricks are drawn
 * here rather than described.
 */
#include "learn_view.h"
#include "gk_ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <string.h>

#define LV_PANELS 6

struct LearnView {
    View* view;
};

typedef struct {
    uint8_t panel;
    uint8_t anim;
} LearnModel;

/* A monospace run with one section inverted -- the same idea as the URL
 * screen, in miniature, so the two teach the same visual grammar. */
static void
    draw_marked(Canvas* c, int x, int y, const char* text, uint8_t from, uint8_t to, bool mark) {
    canvas_set_font(c, FontKeyboard);
    uint8_t cw = (uint8_t)canvas_string_width(c, "0");
    size_t n = strlen(text);

    if(mark && to > from) {
        canvas_draw_box(c, x + from * cw, y - 8, (int)(to - from) * cw, 10);
    }
    for(size_t i = 0; i < n; i++) {
        char ch[2] = {text[i], '\0'};
        bool inside = mark && i >= from && i < to;
        if(inside) canvas_set_color(c, ColorWhite);
        canvas_draw_str(c, x + (int)i * cw, y, ch);
        if(inside) canvas_set_color(c, ColorBlack);
    }
}

static void two_lines(Canvas* c, const char* a, const char* b) {
    canvas_set_font(c, FontSecondary);
    if(a) canvas_draw_str_aligned(c, 64, 54, AlignCenter, AlignBottom, a);
    if(b) canvas_draw_str_aligned(c, 64, 62, AlignCenter, AlignBottom, b);
}

/* --- 1. what a tag does at all --- */
static void panel_basics(Canvas* c, uint8_t anim) {
    gk_ui_header(c, "1/6  What a tag does", NULL);

    canvas_draw_rframe(c, 6, 20, 20, 24, 2); /* tag */
    canvas_draw_rframe(c, 10, 24, 12, 16, 1);
    canvas_draw_rframe(c, 96, 18, 24, 30, 3); /* phone */
    canvas_draw_line(c, 100, 44, 116, 44);

    /* The URL crossing the gap, one step at a time. */
    int travel = 30 + (anim % 8) * 6;
    if(travel > 88) travel = 88;
    canvas_set_font(c, FontKeyboard);
    canvas_draw_str(c, travel, 34, "URL");
    canvas_draw_line(c, 28, 38, 94, 38);

    two_lines(c, "A tag holds an address.", "Tapping it opens that address.");
}

/* --- 2. the part that decides --- */
static void panel_destination(Canvas* c, uint8_t anim) {
    gk_ui_header(c, "2/6  Which part counts", NULL);

    bool mark = (anim % 10) < 7;
    draw_marked(c, 4, 30, "shop.example.co.uk", 5, 18, mark);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 4, 44, mark ? "This is the owner." : "Everything else is free.");

    two_lines(c, "Only the last labels before", "the first / decide where you go.");
}

/* --- 3. the @ trick --- */
static void panel_at(Canvas* c, uint8_t anim) {
    gk_ui_header(c, "3/6  The @ trick", NULL);

    /* Phase 1 highlights what the eye reads, phase 2 what the phone obeys. */
    bool phase = (anim % 16) >= 8;
    if(phase) {
        draw_marked(c, 2, 30, "apple.com@evil.tld", 10, 18, true);
    } else {
        draw_marked(c, 2, 30, "apple.com@evil.tld", 0, 9, true);
    }
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 44, phase ? "...the phone obeys this." : "You read this...");

    two_lines(c, "Everything before the @ is", "a username. It is thrown away.");
}

/* --- 4. the subdomain trick --- */
static void panel_subdomain(Canvas* c, uint8_t anim) {
    gk_ui_header(c, "4/6  Name in the wrong place", NULL);

    bool phase = (anim % 16) >= 8;
    if(phase) {
        draw_marked(c, 2, 30, "paypal.com.pay-x.top", 11, 20, true);
    } else {
        draw_marked(c, 2, 30, "paypal.com.pay-x.top", 0, 10, true);
    }
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 44, phase ? "...but you land here." : "It says paypal.com...");

    two_lines(c, "Anyone can put any name in", "front of their own domain.");
}

/* --- 5. look-alike letters --- */
static void panel_lookalike(Canvas* c, uint8_t anim) {
    gk_ui_header(c, "5/6  Look-alike letters", NULL);

    canvas_set_font(c, FontKeyboard);
    canvas_draw_str(c, 12, 28, "paypal.com");
    canvas_draw_str(c, 12, 42, "paypa1.com");

    /* A pointer that ticks back and forth under the character that changed,
     * because the whole difficulty is that it does not stand out. */
    if((anim % 8) < 5) {
        canvas_draw_line(c, 42, 44, 46, 44);
        canvas_draw_line(c, 44, 45, 44, 47);
    }
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 74, 42, "<- a one");

    two_lines(c, "Read the domain one letter", "at a time, or not at all.");
}

/* --- 6. what a tag cannot tell you --- */
static void panel_limits(Canvas* c, uint8_t anim) {
    gk_ui_header(c, "6/6  What this cannot see", NULL);

    canvas_draw_rframe(c, 8, 19, 30, 24, 2);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 13, 34, "tag");
    canvas_draw_rframe(c, 84, 19, 34, 24, 2);
    canvas_draw_str(c, 92, 34, "website");

    /* A dashed line, broken in the middle: the half that is reachable and
     * the half that is not. */
    for(int x = 40; x < 60; x += 4) canvas_draw_line(c, x, 31, x + 2, 31);
    if((anim % 10) < 5) {
        canvas_draw_line(c, 66, 25, 78, 37);
        canvas_draw_line(c, 78, 25, 66, 37);
    }

    two_lines(c, "Gatekeeper reads the tag.", "The page behind it is unknown.");
}

static void learn_view_draw(Canvas* canvas, void* model) {
    LearnModel* m = model;
    canvas_clear(canvas);

    switch(m->panel) {
    case 0:
        panel_basics(canvas, m->anim);
        break;
    case 1:
        panel_destination(canvas, m->anim);
        break;
    case 2:
        panel_at(canvas, m->anim);
        break;
    case 3:
        panel_subdomain(canvas, m->anim);
        break;
    case 4:
        panel_lookalike(canvas, m->anim);
        break;
    default:
        panel_limits(canvas, m->anim);
        break;
    }
}

static bool learn_view_input(InputEvent* event, void* context) {
    LearnView* v = context;
    if(event->type != InputTypeShort) return false;
    if(event->key != InputKeyLeft && event->key != InputKeyRight && event->key != InputKeyOk) {
        return false;
    }

    with_view_model(
        v->view,
        LearnModel * m,
        {
            if(event->key == InputKeyLeft) {
                if(m->panel > 0) m->panel--;
            } else {
                if(m->panel + 1 < LV_PANELS) m->panel++;
            }
            m->anim = 0;
        },
        true);
    return true;
}

LearnView* learn_view_alloc(void) {
    LearnView* v = malloc(sizeof(LearnView));
    memset(v, 0, sizeof(LearnView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, learn_view_draw);
    view_set_input_callback(v->view, learn_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(LearnModel));
    return v;
}

void learn_view_free(LearnView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* learn_view_get_view(LearnView* v) {
    furi_assert(v);
    return v->view;
}

void learn_view_tick(LearnView* v) {
    furi_assert(v);
    with_view_model(v->view, LearnModel * m, { m->anim++; }, true);
}

bool learn_view_at_start(LearnView* v) {
    furi_assert(v);
    bool first = true;
    with_view_model(v->view, LearnModel * m, { first = (m->panel == 0); }, false);
    return first;
}

void learn_view_reset(LearnView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        LearnModel * m,
        {
            m->panel = 0;
            m->anim = 0;
        },
        true);
}
