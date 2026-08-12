#include "../gatekeeper_i.h"

void gatekeeper_scene_learn_on_enter(void* context) {
    GatekeeperApp* app = context;
    learn_view_reset(app->learn_view);
    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewLearn);
}

bool gatekeeper_scene_learn_on_event(void* context, SceneManagerEvent event) {
    GatekeeperApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        learn_view_tick(app->learn_view);
        return true;
    }
    return false;
}

void gatekeeper_scene_learn_on_exit(void* context) {
    UNUSED(context);
}
