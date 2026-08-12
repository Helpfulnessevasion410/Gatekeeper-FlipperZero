#include "../gatekeeper_i.h"

/* Hold the field up until something lands in it, then hand off to the
 * verdict. Every outcome goes to the verdict screen, including the ones with
 * no link on them -- "this tag is blank" is a result, and explaining it is
 * that screen's job rather than this one's. */

#define GK_DEMO_TICKS 12 /* ~1.2 s of animation before a scripted tag "arrives" */

void gatekeeper_scene_scan_on_enter(void* context) {
    GatekeeperApp* app = context;

    app->demo_ticks = 0;
    gatekeeper_scan_start(app);

    ScanSnapshot snap = {
        .state = GkReaderSearching,
        .demo = app->settings.demo,
        .demo_name = app->settings.demo ? gk_demo_info(app->settings.demo_index)->name : NULL,
    };
    scan_view_set(app->scan_view, &snap);
    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewScan);
}

bool gatekeeper_scene_scan_on_event(void* context, SceneManagerEvent event) {
    GatekeeperApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        scan_view_tick(app->scan_view);

        if(app->settings.demo) {
            /* No radio: the scripted tag turns up on the clock instead. */
            app->demo_ticks++;
            if(app->demo_ticks >= GK_DEMO_TICKS) {
                GkTag tag;
                gk_demo_build(app->settings.demo_index, &tag);
                gatekeeper_accept_result(app, &tag);
                /* Walk the set, so repeated scans in demo mode show what the
                 * grader can actually do rather than the same tag forever. */
                app->settings.demo_index =
                    (uint8_t)((app->settings.demo_index + 1) % gk_demo_count());
                view_dispatcher_send_custom_event(app->view_dispatcher, GkEventScanDone);
            }
        } else {
            GkReaderState state = gk_reader_state(app->reader);
            GkTag tag;
            if(state == GkReaderDone && gk_reader_get(app->reader, &tag)) {
                gatekeeper_accept_result(app, &tag);
                view_dispatcher_send_custom_event(app->view_dispatcher, GkEventScanDone);
            } else {
                ScanSnapshot snap = {.state = state, .demo = false, .demo_name = NULL};
                scan_view_set(app->scan_view, &snap);
            }
        }
        consumed = true;

    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GkEventScanDone) {
            /* on_enter must not navigate, so the transition happens here, on
             * the next dispatch, rather than inside the tick that found it. */
            scene_manager_next_scene(app->scene_manager, GatekeeperSceneVerdict);
            consumed = true;
        }
    }

    return consumed;
}

void gatekeeper_scene_scan_on_exit(void* context) {
    GatekeeperApp* app = context;
    gatekeeper_scan_stop(app);
}
