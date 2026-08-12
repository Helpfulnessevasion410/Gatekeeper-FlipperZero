#include "../gatekeeper_i.h"

/* The findings list and the ceiling list share one index space, findings
 * first. Which half the selection landed in decides where the words on this
 * screen come from. */

void gatekeeper_scene_detail_on_enter(void* context) {
    GatekeeperApp* app = context;
    const GkVerdictResult* r = &app->verdict;
    uint8_t index = app->selected_finding;

    if(index < r->find_count) {
        const GkFinding* f = &r->find[index];
        detail_view_set_finding(app->detail_view, f->id, f->evidence, f->points);
    } else {
        uint8_t cap_index = (uint8_t)(index - r->find_count);
        if(cap_index >= r->cap_count) cap_index = 0;
        detail_view_set_cap(app->detail_view, r->caps[cap_index]);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewDetail);
}

bool gatekeeper_scene_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gatekeeper_scene_detail_on_exit(void* context) {
    UNUSED(context);
}
