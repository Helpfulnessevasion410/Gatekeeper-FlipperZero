#include "gk_store.h"

#include <furi.h>
#include <furi_hal_rtc.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#include <stdio.h>
#include <string.h>

#define GK_SETTINGS_PATH APP_DATA_PATH("settings.bin")
#define GK_SETTINGS_MAGIC 0x6B
#define GK_SETTINGS_VERSION 1
#define GK_LOG_PATH APP_DATA_PATH("scans.csv")

const char* const gk_log_path_display = "apps_data/gatekeeper/scans.csv";

static void ensure_dir(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);
}

void gk_store_settings_save(const GkSettings* s) {
    furi_assert(s);
    ensure_dir();
    saved_struct_save(
        GK_SETTINGS_PATH, s, sizeof(GkSettings), GK_SETTINGS_MAGIC, GK_SETTINGS_VERSION);
}

void gk_store_settings_load(GkSettings* s) {
    furi_assert(s);
    GkSettings loaded;
    if(!saved_struct_load(
           GK_SETTINGS_PATH,
           &loaded,
           sizeof(GkSettings),
           GK_SETTINGS_MAGIC,
           GK_SETTINGS_VERSION)) {
        return; /* nothing valid on the card: the caller keeps its defaults */
    }
    /* Never let a file on the SD card index an array. */
    if(loaded.demo_index >= 32) loaded.demo_index = 0;
    *s = loaded;
}

/* ---------------------------------------------------------------- the log */

static void append_str(File* file, const char* s) {
    storage_file_write(file, s, strlen(s));
}

/* CSV has no way to escape a comma that everyone agrees on, and a URL may
 * legitimately contain one, so quotes go round it and any quote inside is
 * doubled -- the one rule every spreadsheet does agree on. */
static void csv_quote(const char* src, char* dst, size_t cap) {
    size_t o = 0;
    if(cap == 0) return;
    if(o + 1 < cap) dst[o++] = '"';
    for(size_t i = 0; src[i] && o + 2 < cap; i++) {
        if(src[i] == '"') {
            if(o + 3 >= cap) break;
            dst[o++] = '"';
        }
        dst[o++] = src[i];
    }
    if(o + 1 < cap) dst[o++] = '"';
    dst[o] = '\0';
}

bool gk_store_log(const GkTag* tag, const GkVerdictResult* r) {
    furi_assert(tag);
    furi_assert(r);

    ensure_dir();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    bool fresh = !storage_file_exists(storage, GK_LOG_PATH);
    if(storage_file_open(file, GK_LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        if(fresh) {
            append_str(file, "when,tech,uid,locked,grade,score,verdict,domain,url,findings\r\n");
        }

        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);

        char uid[GK_UID_MAX * 2 + 1] = {0};
        static const char* hexd = "0123456789ABCDEF";
        for(uint8_t i = 0; i < tag->uid_len && i < GK_UID_MAX; i++) {
            uid[i * 2] = hexd[tag->uid[i] >> 4];
            uid[i * 2 + 1] = hexd[tag->uid[i] & 0x0F];
        }

        /* The findings, short and machine-readable, because a grade six weeks
         * later is useless without the reasons behind it. */
        char sigs[160];
        sigs[0] = '\0';
        size_t n = 0;
        for(uint8_t i = 0; i < r->find_count; i++) {
            const GkSignalInfo* si = gk_signal_info(r->find[i].id);
            int wrote = snprintf(
                sigs + n, (n < sizeof(sigs)) ? sizeof(sigs) - n : 0, "%s%s",
                (n > 0) ? "|" : "", si->label);
            if(wrote < 0) break;
            n += (size_t)wrote;
            if(n >= sizeof(sigs)) {
                sigs[sizeof(sigs) - 1] = '\0';
                break;
            }
        }

        /* Written field by field rather than through one big snprintf. A URL
         * can be five hundred bytes on its own, so a single format string
         * would need a kilobyte of stack -- on the GUI thread -- and the
         * compiler is right to refuse to prove it always fits.
         *
         * The small buffers below are sized for what the compiler must be
         * able to prove rather than for what a clock can produce:
         * -Werror=format-truncation assumes every %u is ten digits wide and
         * will not take a range on trust. */
        char stamp[72];
        snprintf(
            stamp,
            sizeof(stamp),
            "%04u-%02u-%02u %02u:%02u:%02u",
            (unsigned)dt.year,
            (unsigned)dt.month,
            (unsigned)dt.day,
            (unsigned)dt.hour,
            (unsigned)dt.minute,
            (unsigned)dt.second);

        char score[12];
        snprintf(score, sizeof(score), "%u", (unsigned)r->score);

        char quoted[GK_URL_MAX + 8];

        append_str(file, stamp);
        append_str(file, ",");
        append_str(file, tag->tech_name[0] ? tag->tech_name : gk_tech_name(tag->tech));
        append_str(file, ",");
        append_str(file, uid);
        append_str(file, ",");
        append_str(file, tag->write_known ? (tag->writable ? "no" : "yes") : "unknown");
        append_str(file, ",");
        append_str(file, gk_grade_name(r->grade));
        append_str(file, ",");
        append_str(file, score);
        append_str(file, ",");
        append_str(file, gk_verdict_name(r->verdict));
        append_str(file, ",");
        append_str(file, r->registrable);
        append_str(file, ",");
        csv_quote(tag->has_url ? tag->url : "", quoted, sizeof(quoted));
        append_str(file, quoted);
        append_str(file, ",");
        csv_quote(sigs, quoted, sizeof(quoted));
        append_str(file, quoted);
        append_str(file, "\r\n");
        ok = true;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* ------------------------------------------------------------- history */

void gk_history_fill(GkHistoryEntry* e, const GkTag* tag, const GkVerdictResult* r) {
    furi_assert(e);
    memset(e, 0, sizeof(*e));
    e->used = true;
    e->grade = r->grade;
    e->verdict = r->verdict;
    e->score = r->score;
    e->findings = r->find_count;

    if(r->registrable[0]) {
        strncpy(e->domain, r->registrable, sizeof(e->domain) - 1);
    } else if(tag->status == GkTagNoNdef) {
        strncpy(e->domain, "(blank tag)", sizeof(e->domain) - 1);
    } else if(tag->status == GkTagNeedsKeys) {
        strncpy(e->domain, "(needs keys)", sizeof(e->domain) - 1);
    } else {
        strncpy(e->domain, "(no link)", sizeof(e->domain) - 1);
    }

    const char* tech = tag->tech_name[0] ? tag->tech_name : gk_tech_name(tag->tech);
    strncpy(e->tech, tech, sizeof(e->tech) - 1);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    e->hour = dt.hour;
    e->minute = dt.minute;
}
