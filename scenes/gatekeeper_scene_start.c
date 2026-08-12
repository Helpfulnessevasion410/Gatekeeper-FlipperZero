#include "../gatekeeper_i.h"

/* The intro plays for a shade under two seconds at the 100 ms tick, then
 * hands off to the menu; any key skips it. It lives inside the root scene
 * rather than on the scene stack, so coming back to the menu from a scan
 * never replays it, and Back from the menu still exits the app cleanly. */

typedef enum {
    StartIndexScan,
    StartIndexHistory,
    StartIndexLearn,
    StartIndexDemo,
    StartIndexSettings,
    StartIndexAbout,
} StartIndex;

static void gatekeeper_scene_start_submenu_cb(void* context, uint32_t index) {
    GatekeeperApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void gatekeeper_scene_start_show_menu(GatekeeperApp* app) {
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Gatekeeper");
    submenu_add_item(
        submenu, "Scan a tag", StartIndexScan, gatekeeper_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Recent scans", StartIndexHistory, gatekeeper_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "How the tricks work", StartIndexLearn, gatekeeper_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Demo tags", StartIndexDemo, gatekeeper_scene_start_submenu_cb, app);
    submenu_add_item(
        submenu, "Settings", StartIndexSettings, gatekeeper_scene_start_submenu_cb, app);
    submenu_add_item(submenu, "About", StartIndexAbout, gatekeeper_scene_start_submenu_cb, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, GatekeeperSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewSubmenu);
}

static void gatekeeper_scene_start_skip_splash(void* context) {
    GatekeeperApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GkEventSkipSplash);
}

void gatekeeper_scene_start_on_enter(void* context) {
    GatekeeperApp* app = context;

    if(!app->splash_done) {
        splash_view_set_skip_callback(app->splash_view, gatekeeper_scene_start_skip_splash, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, GkViewSplash);
    } else {
        gatekeeper_scene_start_show_menu(app);
    }
}

bool gatekeeper_scene_start_on_event(void* context, SceneManagerEvent event) {
    GatekeeperApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(!app->splash_done) {
            if(splash_view_tick(app->splash_view)) {
                app->splash_done = true;
                gatekeeper_scene_start_show_menu(app);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(!app->splash_done && event.event == GkEventSkipSplash) {
            app->splash_done = true;
            gatekeeper_scene_start_show_menu(app);
            return true;
        }

        scene_manager_set_scene_state(app->scene_manager, GatekeeperSceneStart, event.event);
        switch(event.event) {
        case StartIndexScan:
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneScan);
            consumed = true;
            break;
        case StartIndexHistory:
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneHistory);
            consumed = true;
            break;
        case StartIndexLearn:
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneLearn);
            consumed = true;
            break;
        case StartIndexDemo:
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneDemo);
            consumed = true;
            break;
        case StartIndexSettings:
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneSettings);
            consumed = true;
            break;
        case StartIndexAbout:
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneAbout);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void gatekeeper_scene_start_on_exit(void* context) {
    GatekeeperApp* app = context;
    submenu_reset(app->submenu);
}
