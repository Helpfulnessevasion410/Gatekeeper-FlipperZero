#include "../gatekeeper_i.h"

static void gatekeeper_scene_findings_cb(void* context, uint8_t index) {
    GatekeeperApp* app = context;
    app->selected_finding = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, GkEventOpenFinding);
}

void gatekeeper_scene_findings_on_enter(void* context) {
    GatekeeperApp* app = context;

    findings_view_set_data(app->findings_view, &app->verdict);
    findings_view_set_callback(app->findings_view, gatekeeper_scene_findings_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewFindings);
}

bool gatekeeper_scene_findings_on_event(void* context, SceneManagerEvent event) {
    GatekeeperApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == GkEventOpenFinding) {
        gatekeeper_click(app);
        scene_manager_next_scene(app->scene_manager, GatekeeperSceneDetail);
        return true;
    }
    return false;
}

void gatekeeper_scene_findings_on_exit(void* context) {
    UNUSED(context);
}
