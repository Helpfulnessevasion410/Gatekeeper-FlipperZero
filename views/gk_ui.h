/* Drawing pieces shared by more than one screen.
 *
 * The grade badge in particular has to look identical everywhere it appears
 * -- verdict, history, findings header -- because it is the thing the user
 * actually reads. One implementation, three callers.
 */
#pragma once

#include <gui/gui.h>
#include <stdbool.h>
#include <stdint.h>

#include "../helpers/gk_verdict.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A row or wrapped line beginning with this byte is drawn as a heading.
 *
 * Written as an octal escape and used through string concatenation, both
 * deliberately: "\x01Caption" is not 0x01 followed by 'C' -- a hex escape
 * eats every hex digit it can reach, so 'C' and 'a' are swallowed into it
 * and the result is a completely different character. The octal form stops
 * after three digits and cannot do that. */
#define GK_HEADING_MARK "\001"

/* The stencil letters are 14 x 18 with a 3px stroke: big enough to read
 * across a room, square enough to stay sharp on a 1-bit screen. */
#define GK_GLYPH_W 14
#define GK_GLYPH_H 18

/* The badge that carries one. */
#define GK_BADGE_W 28
#define GK_BADGE_H 30

/** A single stencil grade letter, drawn in the current colour. */
void gk_ui_glyph(Canvas* canvas, int x, int y, char c);

/** The full badge: a gate frame with the grade inside it. D and F come back
 *  inverted, so the two grades that mean "stop" carry visual weight the
 *  others do not. */
void gk_ui_badge(Canvas* canvas, int x, int y, GkGrade grade);

/** A small filled chip with the grade letter, for list rows. Returns the
 *  width drawn so the caller can lay text out after it. */
uint8_t gk_ui_grade_chip(Canvas* canvas, int x, int y, GkGrade grade);

/** Truncate `src` to fit `max_w` pixels in the canvas's current font,
 *  appending an ellipsis when it had to cut. */
void gk_ui_elide(Canvas* canvas, const char* src, uint8_t max_w, char* dst, size_t cap);

/** Word-wrap `src` into `dst_lines`, at most `max_lines` of `max_w` pixels in
 *  the current font. Returns the number of lines used. Long unbreakable runs
 *  -- which is what a URL is -- are split rather than overflowed. */
uint8_t gk_ui_wrap(
    Canvas* canvas,
    const char* src,
    uint8_t max_w,
    char dst_lines[][40],
    uint8_t max_lines);

/** A header bar: filled strip, title on the left, an optional right-aligned
 *  note, drawn inverted. */
void gk_ui_header(Canvas* canvas, const char* title, const char* right);

/** The two-tone bottom hint strip. */
void gk_ui_hint(Canvas* canvas, const char* left, const char* right);

#ifdef __cplusplus
}
#endif
