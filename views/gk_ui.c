#include "gk_ui.h"

#include <string.h>

/* ---------------------------------------------------------------- glyphs
 *
 * Six letters, built from rectangles and thick diagonals rather than a font,
 * because the largest font on the device is eight pixels tall and the grade
 * is the one thing on the screen that should be readable at arm's length.
 * Rectangles stay crisp on a 1-bit panel at any position; a scaled bitmap
 * font does not.
 */

static void thick_line(Canvas* c, int x0, int y0, int x1, int y1, int w) {
    for(int i = 0; i < w; i++) {
        canvas_draw_line(c, x0 + i, y0, x1 + i, y1);
    }
}

void gk_ui_glyph(Canvas* canvas, int x, int y, char c) {
    const int s = 3; /* stroke */
    const int w = GK_GLYPH_W;
    const int h = GK_GLYPH_H;

    switch(c) {
    case 'A':
        thick_line(canvas, x + w / 2 - 1, y, x - 1, y + h - 1, s);
        thick_line(canvas, x + w / 2 - 1, y, x + w - s, y + h - 1, s);
        canvas_draw_box(canvas, x + 3, y + h - 8, w - 6, s);
        break;
    case 'B':
        canvas_draw_box(canvas, x, y, s, h);
        canvas_draw_box(canvas, x, y, w - 3, s);
        canvas_draw_box(canvas, x, y + (h - s) / 2, w - 3, s);
        canvas_draw_box(canvas, x, y + h - s, w - 3, s);
        canvas_draw_box(canvas, x + w - 3, y + s, s, (h - s) / 2 - s);
        canvas_draw_box(canvas, x + w - 3, y + (h + s) / 2, s, (h - s) / 2 - s);
        break;
    case 'C':
        canvas_draw_box(canvas, x + s, y, w - s, s);
        canvas_draw_box(canvas, x, y + s, s, h - 2 * s);
        canvas_draw_box(canvas, x + s, y + h - s, w - s, s);
        break;
    case 'D':
        canvas_draw_box(canvas, x, y, s, h);
        canvas_draw_box(canvas, x, y, w - 3, s);
        canvas_draw_box(canvas, x, y + h - s, w - 3, s);
        canvas_draw_box(canvas, x + w - 3, y + s, s, h - 2 * s);
        break;
    case 'F':
        canvas_draw_box(canvas, x, y, s, h);
        canvas_draw_box(canvas, x, y, w - 1, s);
        canvas_draw_box(canvas, x, y + (h - s) / 2, w - 5, s);
        break;
    default: /* "-" : nothing to grade */
        canvas_draw_box(canvas, x + 2, y + (h - s) / 2, w - 4, s);
        break;
    }
}

void gk_ui_badge(Canvas* canvas, int x, int y, GkGrade grade) {
    const char* name = gk_grade_name(grade);
    char letter = name[0];

    /* D and F invert. A grade that means "stop" should not look like a grade
     * that means "carry on" with a different letter in it. */
    bool invert = (grade == GkGradeD || grade == GkGradeF);

    if(invert) {
        canvas_draw_rbox(canvas, x, y, GK_BADGE_W, GK_BADGE_H, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, GK_BADGE_W, GK_BADGE_H, 3);
        canvas_draw_rframe(canvas, x + 1, y + 1, GK_BADGE_W - 2, GK_BADGE_H - 2, 2);
    }

    gk_ui_glyph(
        canvas, x + (GK_BADGE_W - GK_GLYPH_W) / 2, y + (GK_BADGE_H - GK_GLYPH_H) / 2, letter);

    /* The portcullis: two bars across the top of the gate, which is the mark
     * on the icon and the banner as well. */
    canvas_draw_line(canvas, x + 5, y + 3, x + GK_BADGE_W - 6, y + 3);
    canvas_draw_line(canvas, x + 5, y + GK_BADGE_H - 4, x + GK_BADGE_W - 6, y + GK_BADGE_H - 4);

    if(invert) canvas_set_color(canvas, ColorBlack);

    /* A+ is unreachable, but the letter still has to render if it is ever
     * asked for, so the plus is drawn rather than special-cased away. */
    if(grade == GkGradeAPlus) {
        canvas_draw_box(canvas, x + GK_BADGE_W - 5, y + 5, 4, 2);
        canvas_draw_box(canvas, x + GK_BADGE_W - 4, y + 4, 2, 4);
    }
}

uint8_t gk_ui_grade_chip(Canvas* canvas, int x, int y, GkGrade grade) {
    const uint8_t w = 11;
    const uint8_t h = 11;
    const char* name = gk_grade_name(grade);
    bool invert = (grade == GkGradeD || grade == GkGradeF);

    if(invert) {
        canvas_draw_rbox(canvas, x, y, w, h, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, w, h, 2);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, x + w / 2, y + h / 2, AlignCenter, AlignCenter, name);
    if(invert) canvas_set_color(canvas, ColorBlack);
    return (uint8_t)(w + 3);
}

/* ------------------------------------------------------------------ text */

void gk_ui_elide(Canvas* canvas, const char* src, uint8_t max_w, char* dst, size_t cap) {
    if(!dst || cap == 0) return;
    dst[0] = '\0';
    if(!src) return;

    if(canvas_string_width(canvas, src) <= max_w) {
        strncpy(dst, src, cap - 1);
        dst[cap - 1] = '\0';
        return;
    }

    size_t n = strlen(src);
    if(n > cap - 1) n = cap - 1;
    while(n > 1) {
        memcpy(dst, src, n - 1);
        dst[n - 1] = '~';
        dst[n] = '\0';
        if(canvas_string_width(canvas, dst) <= max_w) return;
        n--;
    }
    dst[0] = '\0';
}

uint8_t gk_ui_wrap(
    Canvas* canvas,
    const char* src,
    uint8_t max_w,
    char dst_lines[][40],
    uint8_t max_lines) {
    if(!src || max_lines == 0) return 0;

    uint8_t line = 0;
    size_t i = 0;
    size_t n = strlen(src);

    while(i < n && line < max_lines) {
        size_t take = 0;
        size_t last_break = 0;
        char probe[40];

        /* Grow the line one character at a time and remember the last place
         * it could legally be broken. A URL has no spaces in it, so the
         * fallback of breaking mid-run is not an edge case here -- it is the
         * normal path, and it must not overflow the buffer. */
        while(i + take < n && take < sizeof(probe) - 1) {
            probe[take] = src[i + take];
            probe[take + 1] = '\0';
            if(canvas_string_width(canvas, probe) > max_w) break;
            char c = src[i + take];
            if(c == ' ' || c == '/' || c == '.' || c == '-' || c == '?' || c == '&') {
                last_break = take + 1;
            }
            take++;
        }

        if(i + take < n && last_break > 0 && take > last_break) take = last_break;
        if(take == 0) take = 1;

        size_t copy = take;
        if(copy > 39) copy = 39;
        memcpy(dst_lines[line], src + i, copy);
        dst_lines[line][copy] = '\0';

        /* Leading spaces at the start of a wrapped line look like an indent
         * nobody asked for. */
        i += take;
        while(i < n && src[i] == ' ') i++;
        line++;
    }

    return line;
}

void gk_ui_header(Canvas* canvas, const char* title, const char* right) {
    canvas_draw_box(canvas, 0, 0, 128, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 3, 10, title);
    if(right && right[0]) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 125, 10, AlignRight, AlignBottom, right);
    }
    canvas_set_color(canvas, ColorBlack);
}

void gk_ui_hint(Canvas* canvas, const char* left, const char* right) {
    canvas_draw_line(canvas, 0, 53, 127, 53);
    canvas_set_font(canvas, FontSecondary);
    if(left && left[0]) canvas_draw_str(canvas, 2, 62, left);
    if(right && right[0]) canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, right);
}
