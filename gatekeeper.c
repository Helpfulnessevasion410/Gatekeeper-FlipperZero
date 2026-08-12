#include "gatekeeper_i.h"

#include <string.h>

/* ---------------- feedback ----------------
 *
 * The screen is not always where the user is looking -- the whole point is
 * to check a tag before the phone comes out, often with the Flipper still
 * half in a pocket. So the three outcomes that matter sound different
 * enough to tell apart without looking: a single soft note for "nothing
 * odd", a rising pair for "look at this", and a hard three-note alarm with
 * the buzzer for anything at the bottom of the scale.
 */

static const NotificationSequence seq_ok = {
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_check = {
    &message_note_e5,
    &message_delay_50,
    &message_note_a5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};
static const NotificationSequence seq_bad = {
    &message_note_a5,
    &message_delay_100,
    &message_note_c4,
    &message_delay_100,
    &message_note_a5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_led_ok = {
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    NULL,
};
static const NotificationSequence seq_led_check = {
    &message_red_255,
    &message_green_255,
    &message_delay_250,
    &message_red_0,
    &message_green_0,
    NULL,
};
static const NotificationSequence seq_led_bad = {
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    &message_delay_100,
    &message_red_255,
    &message_delay_250,
    &message_red_0,
    NULL,
};

static const NotificationSequence seq_vibro = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

void gatekeeper_alarm(GatekeeperApp* app, GkVerdict verdict) {
    furi_assert(app);

    /* Notification sequences of different lengths cannot share a ternary --
     * the pointer types differ -- so this is written out. */
    if(app->settings.sound) {
        if(verdict >= GkVerdictSuspicious) {
            notification_message(app->notifications, &seq_bad);
        } else if(verdict >= GkVerdictCheck) {
            notification_message(app->notifications, &seq_check);
        } else {
            notification_message(app->notifications, &seq_ok);
        }
    }
    if(app->settings.led) {
        if(verdict >= GkVerdictSuspicious) {
            notification_message(app->notifications, &seq_led_bad);
        } else if(verdict >= GkVerdictCheck) {
            notification_message(app->notifications, &seq_led_check);
        } else {
            notification_message(app->notifications, &seq_led_ok);
        }
    }
    if(app->settings.vibro && verdict >= GkVerdictSuspicious) {
        notification_message(app->notifications, &seq_vibro);
    }
}

void gatekeeper_click(GatekeeperApp* app) {
    furi_assert(app);
    if(app->settings.sound) notification_message(app->notifications, &sequence_semi_success);
}

/* ---------------- the scan ---------------- */

void gatekeeper_scan_start(GatekeeperApp* app) {
    furi_assert(app);
    if(app->scanning) return;
    app->scanning = true;
    /* In demo mode there is no field to raise: the scripted tag arrives on
     * the scene's tick instead. */
    if(!app->settings.demo) gk_reader_start(app->reader);
}

void gatekeeper_scan_stop(GatekeeperApp* app) {
    furi_assert(app);
    if(!app->scanning) return;
    app->scanning = false;
    if(!app->settings.demo) gk_reader_stop(app->reader);
}

void gatekeeper_accept_result(GatekeeperApp* app, const GkTag* tag) {
    furi_assert(app);
    furi_assert(tag);

    app->tag = *tag;
    gk_verdict_grade(&app->tag, &app->verdict);
    app->have_result = true;
    app->selected_finding = 0;

    /* Newest first, oldest pushed off the end. */
    if(app->history_count < GK_HISTORY_MAX) app->history_count++;
    for(uint8_t i = (uint8_t)(app->history_count - 1); i > 0; i--) {
        app->history[i] = app->history[i - 1];
    }
    gk_history_fill(&app->history[0], &app->tag, &app->verdict);

    if(app->settings.log) gk_store_log(&app->tag, &app->verdict);

    gatekeeper_alarm(app, app->verdict.verdict);
}

/* ---------------- view dispatcher plumbing ---------------- */

static bool gatekeeper_custom_event_callback(void* context, uint32_t event) {
    GatekeeperApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool gatekeeper_back_event_callback(void* context) {
    GatekeeperApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void gatekeeper_tick_event_callback(void* context) {
    GatekeeperApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* ---------------- lifecycle ---------------- */

static GatekeeperApp* gatekeeper_app_alloc(void) {
    GatekeeperApp* app = malloc(sizeof(GatekeeperApp));
    memset(app, 0, sizeof(GatekeeperApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&gatekeeper_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, gatekeeper_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, gatekeeper_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, gatekeeper_tick_event_callback, GATEKEEPER_TICK_MS);

    /* Defaults first, then whatever was saved last run. */
    app->settings.sound = true;
    app->settings.vibro = true;
    app->settings.led = true;
    app->settings.demo = false;
    app->settings.log = false;
    gk_store_settings_load(&app->settings);
    if(app->settings.demo_index >= gk_demo_count()) app->settings.demo_index = 0;

    gk_tag_init(&app->tag);
    app->reader = gk_reader_alloc();

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GkViewSubmenu, submenu_get_view(app->submenu));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GkViewSettings, variable_item_list_get_view(app->var_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GkViewAbout, widget_get_view(app->widget));

    app->splash_view = splash_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GkViewSplash, splash_view_get_view(app->splash_view));

    app->scan_view = scan_view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GkViewScan, scan_view_get_view(app->scan_view));

    app->verdict_view = verdict_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GkViewVerdict, verdict_view_get_view(app->verdict_view));

    app->url_view = url_view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GkViewUrl, url_view_get_view(app->url_view));

    app->findings_view = findings_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GkViewFindings, findings_view_get_view(app->findings_view));

    app->detail_view = detail_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GkViewDetail, detail_view_get_view(app->detail_view));

    app->tag_view = tag_view_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GkViewTag, tag_view_get_view(app->tag_view));

    app->learn_view = learn_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GkViewLearn, learn_view_get_view(app->learn_view));

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    return app;
}

static void gatekeeper_app_free(GatekeeperApp* app) {
    furi_assert(app);

    gatekeeper_scan_stop(app);
    gk_store_settings_save(&app->settings);

    view_dispatcher_remove_view(app->view_dispatcher, GkViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewSettings);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewAbout);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewSplash);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewVerdict);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewUrl);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewFindings);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewDetail);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewTag);
    view_dispatcher_remove_view(app->view_dispatcher, GkViewLearn);

    submenu_free(app->submenu);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    splash_view_free(app->splash_view);
    scan_view_free(app->scan_view);
    verdict_view_free(app->verdict_view);
    url_view_free(app->url_view);
    findings_view_free(app->findings_view);
    detail_view_free(app->detail_view);
    tag_view_free(app->tag_view);
    learn_view_free(app->learn_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    gk_reader_free(app->reader);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t gatekeeper_app(void* p) {
    UNUSED(p);
    GatekeeperApp* app = gatekeeper_app_alloc();
    scene_manager_next_scene(app->scene_manager, GatekeeperSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    gatekeeper_app_free(app);
    return 0;
}
