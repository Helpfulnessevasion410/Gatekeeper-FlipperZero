#include "../gatekeeper_i.h"

/* Twelve tags off a wall, without the wall. Each one is assembled as real
 * NDEF bytes and pushed through the same parser and the same grader the
 * radio feeds, so this menu is a way of exercising the application rather
 * than a set of canned screens. */

static void gatekeeper_scene_demo_cb(void* context, uint32_t index) {
    GatekeeperApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void gatekeeper_scene_demo_on_enter(void* context) {
    GatekeeperApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Demo tags");
    for(uint8_t i = 0; i < gk_demo_count(); i++) {
        submenu_add_item(submenu, gk_demo_info(i)->name, i, gatekeeper_scene_demo_cb, app);
    }
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GatekeeperSceneDemo));

    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewSubmenu);
}

bool gatekeeper_scene_demo_on_event(void* context, SceneManagerEvent event) {
    GatekeeperApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event < gk_demo_count()) {
        scene_manager_set_scene_state(app->scene_manager, GatekeeperSceneDemo, event.event);

        GkTag tag;
        gk_demo_build((uint8_t)event.event, &tag);
        gatekeeper_accept_result(app, &tag);
        scene_manager_next_scene(app->scene_manager, GatekeeperSceneVerdict);
        return true;
    }
    return false;
}

void gatekeeper_scene_demo_on_exit(void* context) {
    GatekeeperApp* app = context;
    submenu_reset(app->submenu);
}
