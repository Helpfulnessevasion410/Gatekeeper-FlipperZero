#include "../gatekeeper_i.h"

void gatekeeper_scene_tag_on_enter(void* context) {
    GatekeeperApp* app = context;
    tag_view_set_tag(app->tag_view, &app->tag);
    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewTag);
}

bool gatekeeper_scene_tag_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gatekeeper_scene_tag_on_exit(void* context) {
    UNUSED(context);
}
