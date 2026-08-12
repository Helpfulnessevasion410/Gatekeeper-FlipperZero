#include "../gatekeeper_i.h"

void gatekeeper_scene_about_on_enter(void* context) {
    GatekeeperApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "\e#Gatekeeper %s\e#\n"
        "Scan before you tap.\n\n"

        "A tag stuck over a real one sends\n"
        "your phone somewhere before you\n"
        "have read anything. This reads the\n"
        "tag first, on the Flipper's own NFC,\n"
        "and grades what is on it.\n\n"

        "\e#The scale\e#\n"
        "A   nothing here argues against it\n"
        "B   worth a look first\n"
        "C   do not sign in or pay\n"
        "D   type the address yourself\n"
        "F   walk away, report the tag\n\n"

        "\e#Why there is no A+\e#\n"
        "Gatekeeper reads the tag. It cannot\n"
        "read the website. A spotless address\n"
        "can still lead to a page that asks\n"
        "for your card, and nothing here can\n"
        "see that page - there is no network\n"
        "connection, by design. So the top of\n"
        "the scale is A, every verdict says so,\n"
        "and the word \"safe\" is not used\n"
        "anywhere in this application.\n\n"

        "\e#What it never does\e#\n"
        "It only reads. It does not write to\n"
        "tags, lock them, crack keys or open\n"
        "anything. It has no radio but the one\n"
        "in your hand.\n\n"

        "\e#Scan log\e#\n"
        "%s\n"
        "One row per scan when the log is on:\n"
        "time, tag, grade, address, reasons.\n"
        "The thing to send a council or a shop\n"
        "when a tag on their property is bad.\n\n"

        "\e#Credits\e#\n"
        "by at0m-b0mb\n"
        "github.com/at0m-b0mb/\n"
        "Gatekeeper-FlipperZero\n"
        "MIT licensed.\n",
        GATEKEEPER_VERSION,
        gk_log_path_display);

    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(text));
    furi_string_free(text);

    view_dispatcher_switch_to_view(app->view_dispatcher, GkViewAbout);
}

bool gatekeeper_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void gatekeeper_scene_about_on_exit(void* context) {
    GatekeeperApp* app = context;
    widget_reset(app->widget);
}
