#include "../gatekeeper_i.h"

#include <stdio.h>

/* Read-only on purpose. Keeping every scanned tag in full would be twelve
 * copies of two kilobytes for a screen nobody opens twice; keeping the grade,
 * the destination and the time is enough to answer the only question this
 * list gets asked, which is "what was that one outside the station?". The
 * full record, if it was wanted, is the CSV. */

void gatekeeper_scene_history_on_enter(void* context) {
    GatekeeperApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    if(app->history_count == 0) {
        widget_add_string_element(
            widget, 64, 26, AlignCenter, AlignBottom, FontPrimary, "No scans yet");
        widget_add_string_element(
            widget, 64, 40, AlignCenter, AlignBottom, FontSecondary, "Scanned tags land here.");
    } else {
        FuriString* text = furi_string_alloc();
        for(uint8_t i = 0; i < app->history_count; i++) {
            const GkHistoryEntry* e = &app->history[i];
            if(!e->used) continue;
            furi_string_cat_printf(
                text,
                "\e#%s  %s\e#\n%02u:%02u  %s\n%u/100, %u finding%s\n\n",
                gk_grade_name(e->grade),
                gk_verdict_name(e->verdict),
                (unsigned)e->hour,
                (unsigned)e->minute,
                e->domain,
                (unsigned)e->score,
                (unsigned)e->findings,
                e->findings == 1 ? "" : "s");
        }
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
        furi_string_free(text);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewAbout);
}

bool gatekeeper_scene_history_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gatekeeper_scene_history_on_exit(void* context) {
    GatekeeperApp* app = context;
    widget_reset(app->widget);
}
