#pragma once

#include <gui/view.h>

#include "../helpers/gk_ndef.h"

typedef struct TagView TagView;

TagView* tag_view_alloc(void);
void tag_view_free(TagView* v);
View* tag_view_get_view(TagView* v);

/** Render the tag's facts into rows. Done once, on entry, rather than on
 *  every draw. */
void tag_view_set_tag(TagView* v, const GkTag* tag);
