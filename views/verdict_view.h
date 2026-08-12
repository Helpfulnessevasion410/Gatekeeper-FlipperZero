#pragma once

#include <gui/view.h>

#include "../helpers/gk_ndef.h"
#include "../helpers/gk_verdict.h"

typedef struct VerdictView VerdictView;

/* What the user asked for from the verdict screen. The scene turns these
 * into scene transitions; the view does not know what a scene is. */
typedef enum {
    VerdictActionFindings = 0,
    VerdictActionUrl,
    VerdictActionTag,
    VerdictActionRescan,
} VerdictAction;

typedef void (*VerdictViewCallback)(void* context, VerdictAction action);

VerdictView* verdict_view_alloc(void);
void verdict_view_free(VerdictView* v);
View* verdict_view_get_view(VerdictView* v);

/** The view holds pointers, not copies: together these two are the best part
 *  of two kilobytes and they are owned by the application for its lifetime. */
void verdict_view_set_data(VerdictView* v, const GkTag* tag, const GkVerdictResult* result);
void verdict_view_set_callback(VerdictView* v, VerdictViewCallback cb, void* context);
