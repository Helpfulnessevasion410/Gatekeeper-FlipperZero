#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include "gatekeeper_icons.h" /* generated from icons/ by fbt */

#include "helpers/gk_demo.h"
#include "helpers/gk_ndef.h"
#include "helpers/gk_reader.h"
#include "helpers/gk_store.h"
#include "helpers/gk_url.h"
#include "helpers/gk_verdict.h"

#include "views/detail_view.h"
#include "views/findings_view.h"
#include "views/learn_view.h"
#include "views/scan_view.h"
#include "views/splash_view.h"
#include "views/tag_view.h"
#include "views/url_view.h"
#include "views/verdict_view.h"

#include "scenes/gatekeeper_scene.h"

#define GATEKEEPER_VERSION "1.0"

/* The GUI ticks at 100 ms: fast enough for the field animation to look like a
 * field and slow enough that the reader thread is not being asked about its
 * progress ten times a second. */
#define GATEKEEPER_TICK_MS 100

typedef enum {
    GkViewSplash,
    GkViewSubmenu,
    GkViewScan,
    GkViewVerdict,
    GkViewUrl,
    GkViewFindings,
    GkViewDetail,
    GkViewTag,
    GkViewLearn,
    GkViewSettings,
    GkViewAbout,
} GkViewId;

typedef enum {
    /* Above any submenu index, so a custom event cannot be mistaken for a
     * menu selection. */
    GkEventSkipSplash = 100,
    GkEventScanDone,
    GkEventRescan,
    GkEventOpenFinding,
    GkEventOpenUrl,
    GkEventOpenTag,
} GkCustomEvent;

/* Scene state on Verdict: set while a child scene is on top, so Verdict's
 * on_exit can tell "the user went to read a finding" from "the user left". */
#define GK_VERDICT_DETOUR 1

typedef struct {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;

    Submenu* submenu;
    VariableItemList* var_item_list;
    Widget* widget;

    SplashView* splash_view;
    ScanView* scan_view;
    VerdictView* verdict_view;
    UrlView* url_view;
    FindingsView* findings_view;
    DetailView* detail_view;
    TagView* tag_view;
    LearnView* learn_view;

    GkReader* reader;
    GkSettings settings;

    /* The current scan. Every view that shows a result holds a pointer to
     * these two rather than a copy: together they are the best part of two
     * kilobytes, and there are five such views. Both are only ever written
     * from the GUI thread's tick handler, which is also the thread that
     * draws, so a view can never catch one half-updated. */
    GkTag tag;
    GkVerdictResult verdict;
    bool have_result;

    GkHistoryEntry history[GK_HISTORY_MAX];
    uint8_t history_count;

    uint8_t selected_finding;
    uint8_t demo_ticks;
    bool scanning;
    bool splash_done;
} GatekeeperApp;

/** Feedback for a finished scan, gated by settings. A worse verdict is
 *  always louder, because the whole point is to be told before you tap. */
void gatekeeper_alarm(GatekeeperApp* app, GkVerdict verdict);
/** A short acknowledgement for something the user did, not something found. */
void gatekeeper_click(GatekeeperApp* app);

/** Arm the radio -- or the scripted tag, in demo mode. */
void gatekeeper_scan_start(GatekeeperApp* app);
void gatekeeper_scan_stop(GatekeeperApp* app);

/** Called from the tick when a read completes: grades it, files it in the
 *  history, writes the log row if that is switched on. */
void gatekeeper_accept_result(GatekeeperApp* app, const GkTag* tag);
