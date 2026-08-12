#pragma once

#include <gui/view.h>

#include "../helpers/gk_verdict.h"

typedef struct FindingsView FindingsView;

typedef void (*FindingsViewCallback)(void* context, uint8_t index);

FindingsView* findings_view_alloc(void);
void findings_view_free(FindingsView* v);
View* findings_view_get_view(FindingsView* v);

void findings_view_set_data(FindingsView* v, const GkVerdictResult* result);
void findings_view_set_callback(FindingsView* v, FindingsViewCallback cb, void* context);

/** Which row is selected, so the detail scene knows what to open. */
uint8_t findings_view_selected(FindingsView* v);
