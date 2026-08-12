/* Settings that survive a reboot, and a scan log that survives the battery.
 *
 * The log matters more than it looks. A tag on a wall is somebody else's
 * property in a public place, and the useful thing to do with a bad one is
 * report it -- to the council, the shop, the transport operator. That takes
 * the address, the date and the reason, written down, which is exactly what
 * a row of this CSV is.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gk_ndef.h"
#include "gk_verdict.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool sound;
    bool vibro;
    bool led;
    bool demo; /* scripted tags instead of the radio */
    bool log; /* append every scan to the CSV */
    uint8_t demo_index;
} GkSettings;

/* The last few scans, kept in memory so "what was that one before?" is one
 * button away. Deliberately small: this is a pocket instrument, not a
 * database. */
#define GK_HISTORY_MAX 12

typedef struct {
    bool used;
    GkGrade grade;
    GkVerdict verdict;
    uint8_t score;
    uint8_t findings;
    char domain[44];
    char tech[GK_TECH_MAX];
    uint8_t hour;
    uint8_t minute;
} GkHistoryEntry;

void gk_store_settings_save(const GkSettings* s);
void gk_store_settings_load(GkSettings* s);

/** Append one scan to the CSV. Returns false if it could not be written. */
bool gk_store_log(const GkTag* tag, const GkVerdictResult* r);

/** Where the log lives, for the settings screen to show. */
extern const char* const gk_log_path_display;

/** Fill an entry from a finished scan, for the in-memory history list. */
void gk_history_fill(GkHistoryEntry* e, const GkTag* tag, const GkVerdictResult* r);

#ifdef __cplusplus
}
#endif
