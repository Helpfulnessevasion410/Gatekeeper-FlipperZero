#include "../gatekeeper_i.h"

static void gatekeeper_scene_verdict_cb(void* context, VerdictAction action) {
    GatekeeperApp* app = context;
    switch(action) {
    case VerdictActionFindings:
        view_dispatcher_send_custom_event(app->view_dispatcher, GkEventOpenFinding);
        break;
    case VerdictActionUrl:
        view_dispatcher_send_custom_event(app->view_dispatcher, GkEventOpenUrl);
        break;
    case VerdictActionTag:
        view_dispatcher_send_custom_event(app->view_dispatcher, GkEventOpenTag);
        break;
    case VerdictActionRescan:
        view_dispatcher_send_custom_event(app->view_dispatcher, GkEventRescan);
        break;
    }
}

void gatekeeper_scene_verdict_on_enter(void* context) {
    GatekeeperApp* app = context;

    verdict_view_set_data(app->verdict_view, &app->tag, &app->verdict);
    verdict_view_set_callback(app->verdict_view, gatekeeper_scene_verdict_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewVerdict);
}

bool gatekeeper_scene_verdict_on_event(void* context, SceneManagerEvent event) {
    GatekeeperApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GkEventOpenFinding:
            gatekeeper_click(app);
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneFindings);
            consumed = true;
            break;
        case GkEventOpenUrl:
            gatekeeper_click(app);
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneUrl);
            consumed = true;
            break;
        case GkEventOpenTag:
            gatekeeper_click(app);
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneTag);
            consumed = true;
            break;
        case GkEventRescan:
            /* Straight back to the field, without going through the menu:
             * the common case is several tags in a row on the same street. */
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, GatekeeperSceneScan);
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        /* Back from a verdict goes to the menu rather than the scan screen,
         * which would immediately raise the field again and read the same
         * tag the user is still holding. */
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, GatekeeperSceneStart);
        consumed = true;
    }

    return consumed;
}

void gatekeeper_scene_verdict_on_exit(void* context) {
    UNUSED(context);
}
