#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stdint.h>

#include "../helpers/gk_ndef.h"
#include "../helpers/gk_reader.h"

typedef struct ScanView ScanView;

typedef struct {
    GkReaderState state;
    bool demo;
    const char* demo_name; /* which scripted tag is standing in for a real one */
} ScanSnapshot;

ScanView* scan_view_alloc(void);
void scan_view_free(ScanView* v);
View* scan_view_get_view(ScanView* v);

void scan_view_set(ScanView* v, const ScanSnapshot* snap);
void scan_view_tick(ScanView* v);
