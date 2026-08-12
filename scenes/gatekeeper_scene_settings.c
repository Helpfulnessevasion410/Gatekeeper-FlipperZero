#include "../gatekeeper_i.h"

static const char* const on_off[] = {"Off", "On"};

static void set_sound(VariableItem* item) {
    GatekeeperApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[index]);
    app->settings.sound = index == 1;
}

static void set_vibro(VariableItem* item) {
    GatekeeperApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[index]);
    app->settings.vibro = index == 1;
}

static void set_led(VariableItem* item) {
    GatekeeperApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[index]);
    app->settings.led = index == 1;
}

static void set_demo(VariableItem* item) {
    GatekeeperApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[index]);
    app->settings.demo = index == 1;
}

static void set_log(VariableItem* item) {
    GatekeeperApp* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, on_off[index]);
    app->settings.log = index == 1;
}

void gatekeeper_scene_settings_on_enter(void* context) {
    GatekeeperApp* app = context;
    VariableItemList* list = app->var_item_list;
    variable_item_list_reset(list);

    VariableItem* item;

    item = variable_item_list_add(list, "Sound", 2, set_sound, app);
    variable_item_set_current_value_index(item, app->settings.sound ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.sound ? 1 : 0]);

    item = variable_item_list_add(list, "Vibrate", 2, set_vibro, app);
    variable_item_set_current_value_index(item, app->settings.vibro ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.vibro ? 1 : 0]);

    item = variable_item_list_add(list, "LED", 2, set_led, app);
    variable_item_set_current_value_index(item, app->settings.led ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.led ? 1 : 0]);

    /* With this on, "Scan a tag" walks the scripted set instead of raising
     * the field -- for demonstrating the application without carrying a bag
     * of tags around. */
    item = variable_item_list_add(list, "Demo instead of radio", 2, set_demo, app);
    variable_item_set_current_value_index(item, app->settings.demo ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.demo ? 1 : 0]);

    item = variable_item_list_add(list, "Save scan log", 2, set_log, app);
    variable_item_set_current_value_index(item, app->settings.log ? 1 : 0);
    variable_item_set_current_value_text(item, on_off[app->settings.log ? 1 : 0]);

    variable_item_list_set_selected_item(
        list, scene_manager_get_scene_state(app->scene_manager, GatekeeperSceneSettings));

    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewSettings);
}

bool gatekeeper_scene_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gatekeeper_scene_settings_on_exit(void* context) {
    GatekeeperApp* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        GatekeeperSceneSettings,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
    gk_store_settings_save(&app->settings);
}
