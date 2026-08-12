#pragma once

#include <gui/view.h>
#include <stdbool.h>

typedef struct SplashView SplashView;
typedef void (*SplashViewCallback)(void* context);

SplashView* splash_view_alloc(void);
void splash_view_free(SplashView* v);
View* splash_view_get_view(SplashView* v);

/** Advance one frame. Returns true when the intro has finished. */
bool splash_view_tick(SplashView* v);
void splash_view_set_skip_callback(SplashView* v, SplashViewCallback cb, void* context);
