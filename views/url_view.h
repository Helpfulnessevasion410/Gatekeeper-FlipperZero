#pragma once

#include <gui/view.h>

#include "../helpers/gk_url.h"

typedef struct UrlView UrlView;

UrlView* url_view_alloc(void);
void url_view_free(UrlView* v);
View* url_view_get_view(UrlView* v);

/** Point the view at an address. The display zones are recomputed here from
 *  the parser rather than passed in, so the highlight can never disagree
 *  with the grade. */
void url_view_set_url(UrlView* v, const char* url);
