/* The tag itself, rather than the link on it.
 *
 * Two things on this screen are worth the trip. "Writable" is the one that
 * changes a grade, and it is the one nobody expects: most tags in the street
 * have never been locked, which means their contents are a statement about
 * who touched them last and not about who put them there. And the record
 * list is where an Android Application Record shows up by name, so a tag
 * that quietly offers to install something has to do it in the open.
 */
#include "tag_view.h"
#include "gk_ui.h"

#include <furi.h>
#include <gui/elements.h>
#include <gui/gui.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Same constraint as the findings list: the last row's baseline must clear
 * the hint separator at y=53. 14 + 3*9 + 7 = 48. */
#define TV_TOP 14
#define TV_ROW_H 9
#define TV_ROWS 4
#define TV_MAX_ROWS 22
#define TV_ROW_CHARS 34

struct TagView {
    View* view;
};

typedef struct {
    char rows[TV_MAX_ROWS][TV_ROW_CHARS];
    uint8_t row_count;
    uint8_t top;
} TagModel;

static void add_row(TagModel* m, const char* fmt, ...) {
    if(m->row_count >= TV_MAX_ROWS) return;
    va_list args;
    va_start(args, fmt);
    vsnprintf(m->rows[m->row_count], TV_ROW_CHARS, fmt, args);
    va_end(args);
    m->row_count++;
}

static void tag_view_draw(Canvas* canvas, void* model) {
    TagModel* m = model;
    canvas_clear(canvas);
    gk_ui_header(canvas, "The tag itself", NULL);

    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < TV_ROWS; i++) {
        uint8_t index = (uint8_t)(m->top + i);
        if(index >= m->row_count) break;
        const char* text = m->rows[index];
        int y = TV_TOP + i * TV_ROW_H + TV_ROW_H - 1;
        /* A leading marker makes a row a section heading. */
        if(text[0] == GK_HEADING_MARK[0]) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, y, text + 1);
            canvas_set_font(canvas, FontSecondary);
        } else {
            canvas_draw_str(canvas, 2, y, text);
        }
    }

    if(m->row_count > TV_ROWS) {
        elements_scrollbar_pos(
            canvas, 126, TV_TOP, TV_ROWS * TV_ROW_H, m->top, (uint8_t)(m->row_count - TV_ROWS + 1));
    }

    gk_ui_hint(canvas, "^v Scroll", "Back");
}

static bool tag_view_input(InputEvent* event, void* context) {
    TagView* v = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;
    if(event->key != InputKeyUp && event->key != InputKeyDown) return false;

    with_view_model(
        v->view,
        TagModel * m,
        {
            if(event->key == InputKeyDown) {
                if(m->row_count > TV_ROWS && m->top < m->row_count - TV_ROWS) m->top++;
            } else if(m->top > 0) {
                m->top--;
            }
        },
        true);
    return true;
}

TagView* tag_view_alloc(void) {
    TagView* v = malloc(sizeof(TagView));
    memset(v, 0, sizeof(TagView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, tag_view_draw);
    view_set_input_callback(v->view, tag_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(TagModel));
    return v;
}

void tag_view_free(TagView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* tag_view_get_view(TagView* v) {
    furi_assert(v);
    return v->view;
}

void tag_view_set_tag(TagView* v, const GkTag* tag) {
    furi_assert(v);
    furi_assert(tag);

    with_view_model(
        v->view,
        TagModel * m,
        {
            memset(m, 0, sizeof(*m));

            add_row(m, GK_HEADING_MARK "%s", tag->tech_name[0] ? tag->tech_name : gk_tech_name(tag->tech));

            if(tag->uid_len) {
                char uid[GK_UID_MAX * 2 + 1] = {0};
                static const char* hexd = "0123456789ABCDEF";
                for(uint8_t i = 0; i < tag->uid_len && i < GK_UID_MAX; i++) {
                    uid[i * 2] = hexd[tag->uid[i] >> 4];
                    uid[i * 2 + 1] = hexd[tag->uid[i] & 0x0F];
                }
                add_row(m, "Serial  %s", uid);
            }

            if(tag->size_known) {
                add_row(m, "Space   %u bytes", (unsigned)tag->capacity);
            }
            if(tag->used) {
                add_row(m, "Used    %u bytes", (unsigned)tag->used);
            }

            if(tag->write_known) {
                add_row(m, "Locked  %s", tag->writable ? "no - anyone can rewrite" : "yes");
            } else {
                add_row(m, "Locked  unknown");
            }
            if(tag->pwd_protected) {
                add_row(m, "Password  part unreadable");
            }
            if(tag->malformed) {
                add_row(m, "Structure  does not add up");
            }

            if(tag->has_title) {
                add_row(m, GK_HEADING_MARK "Caption on the tag");
                add_row(m, "%s", tag->title);
            }
            if(tag->has_aar) {
                add_row(m, GK_HEADING_MARK "Android app named");
                add_row(m, "%s", tag->aar);
            }

            if(tag->record_total) {
                add_row(m, GK_HEADING_MARK "Records (%u)", (unsigned)tag->record_total);
                for(uint8_t i = 0; i < tag->record_count; i++) {
                    const GkRecord* rec = &tag->rec[i];
                    if(rec->summary[0]) {
                        add_row(m, "%s: %s", gk_rec_kind_name(rec->kind), rec->summary);
                    } else {
                        add_row(
                            m,
                            "%s (%u bytes)",
                            gk_rec_kind_name(rec->kind),
                            (unsigned)rec->payload_len);
                    }
                }
                if(tag->record_total > tag->record_count) {
                    add_row(m, "...and %u more", (unsigned)(tag->record_total - tag->record_count));
                }
            }
        },
        true);
}
