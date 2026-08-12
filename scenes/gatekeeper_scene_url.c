#include "../gatekeeper_i.h"

void gatekeeper_scene_url_on_enter(void* context) {
    GatekeeperApp* app = context;
    url_view_set_url(app->url_view, app->tag.has_url ? app->tag.url : NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewUrl);
}

bool gatekeeper_scene_url_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gatekeeper_scene_url_on_exit(void* context) {
    UNUSED(context);
}
