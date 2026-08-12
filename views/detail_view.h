#pragma once

#include <gui/view.h>
#include <stdbool.h>

#include "../helpers/gk_verdict.h"

typedef struct DetailView DetailView;

DetailView* detail_view_alloc(void);
void detail_view_free(DetailView* v);
View* detail_view_get_view(DetailView* v);

/** Show one finding, with the text it was observed on. */
void detail_view_set_finding(DetailView* v, GkSignalId id, const char* evidence, uint8_t points);
/** Show one ceiling instead: same screen, different source of words. */
void detail_view_set_cap(DetailView* v, GkCapId cap);
