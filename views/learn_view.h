#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct LearnView LearnView;

LearnView* learn_view_alloc(void);
void learn_view_free(LearnView* v);
View* learn_view_get_view(LearnView* v);

/** Advance the animation on the current panel. */
void learn_view_tick(LearnView* v);
/** True when Back should leave the walkthrough rather than step back a panel. */
bool learn_view_at_start(LearnView* v);
void learn_view_reset(LearnView* v);
