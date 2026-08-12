/* Grades the scripted tags with the real engine and prints the result as
 * JSON, so the README mockups are renderings of what the application
 * actually decides rather than pictures somebody drew of a good outcome.
 *
 *      make -C test dump
 */
#include "helpers/gk_demo.h"
#include "helpers/gk_ndef.h"
#include "helpers/gk_url.h"
#include "helpers/gk_verdict.h"

#include <stdio.h>
#include <string.h>

static void json_str(const char* s) {
    putchar('"');
    for(const unsigned char* p = (const unsigned char*)s; *p; p++) {
        switch(*p) {
        case '"':
            fputs("\\\"", stdout);
            break;
        case '\\':
            fputs("\\\\", stdout);
            break;
        case '\n':
            fputs("\\n", stdout);
            break;
        default:
            if(*p < 0x20 || *p > 0x7E) {
                printf("\\u%04x", *p);
            } else {
                putchar(*p);
            }
        }
    }
    putchar('"');
}

static void field(const char* key, const char* value) {
    json_str(key);
    fputs(": ", stdout);
    json_str(value);
}

int main(void) {
    printf("[\n");

    for(uint8_t i = 0; i < gk_demo_count(); i++) {
        GkTag tag;
        if(!gk_demo_build(i, &tag)) continue;

        GkVerdictResult r;
        gk_verdict_grade(&tag, &r);

        printf("  {\n    ");
        field("name", gk_demo_info(i)->name);
        printf(",\n    ");
        field("where", gk_demo_info(i)->where);
        printf(",\n    ");
        field("tech", tag.tech_name[0] ? tag.tech_name : gk_tech_name(tag.tech));
        printf(",\n    ");
        field("url", tag.has_url ? tag.url : "");
        printf(",\n    ");
        field("title", tag.has_title ? tag.title : "");
        printf(",\n    ");
        field("aar", tag.has_aar ? tag.aar : "");
        printf(",\n    ");
        field("host", r.host);
        printf(",\n    ");
        field("registrable", r.registrable);
        printf(",\n    ");
        field("grade", gk_grade_name(r.grade));
        printf(",\n    ");
        field("verdict", gk_verdict_name(r.verdict));
        printf(",\n    ");
        field("action", gk_verdict_action(r.verdict));

        printf(",\n    \"score\": %u", (unsigned)r.score);
        printf(",\n    \"raw_score\": %u", (unsigned)r.raw_score);
        printf(",\n    \"find_count\": %u", (unsigned)r.find_count);
        printf(",\n    \"find_total\": %u", (unsigned)r.find_total);
        printf(",\n    \"capacity\": %u", (unsigned)tag.capacity);
        printf(",\n    \"used\": %u", (unsigned)tag.used);
        printf(",\n    \"writable\": %s", tag.writable ? "true" : "false");
        printf(",\n    \"write_known\": %s", tag.write_known ? "true" : "false");

        /* The UID, so the tag-details mockup shows the same serial the demo
         * tag really carries. */
        char uid[GK_UID_MAX * 2 + 1] = {0};
        static const char* hexd = "0123456789ABCDEF";
        for(uint8_t k = 0; k < tag.uid_len && k < GK_UID_MAX; k++) {
            uid[k * 2] = hexd[tag.uid[k] >> 4];
            uid[k * 2 + 1] = hexd[tag.uid[k] & 0x0F];
        }
        printf(",\n    ");
        field("uid", uid);

        /* The display zones, straight out of the parser, so the highlight in
         * the mockup lands on exactly the characters it lands on in the app. */
        GkUrl u;
        GkUrlZones z;
        memset(&z, 0, sizeof(z));
        if(tag.has_url && gk_url_parse(tag.url, 0, &u)) gk_url_zones(&u, &z);
        printf(
            ",\n    \"zones\": {\"pre\": %u, \"sub_off\": %u, \"sub_len\": %u, "
            "\"reg_off\": %u, \"reg_len\": %u, \"post_off\": %u, \"post_len\": %u}",
            z.pre_len,
            z.sub_off,
            z.sub_len,
            z.reg_off,
            z.reg_len,
            z.post_off,
            z.post_len);

        printf(",\n    \"findings\": [");
        for(uint8_t f = 0; f < r.find_count; f++) {
            const GkSignalInfo* si = gk_signal_info(r.find[f].id);
            if(f) printf(",");
            printf("\n      {");
            field("label", si->label);
            printf(", ");
            field("what", si->what);
            printf(", ");
            field("why", si->why);
            printf(", ");
            field("advice", si->advice);
            printf(", ");
            field("evidence", r.find[f].evidence);
            printf(", ");
            field("family", gk_family_name(si->family));
            printf(", \"points\": %u}", (unsigned)r.find[f].points);
        }
        printf("\n    ]");

        printf(",\n    \"caps\": [");
        for(uint8_t c = 0; c < r.cap_count; c++) {
            if(c) printf(",");
            printf("\n      {");
            field("label", gk_cap_label(r.caps[c]));
            printf(", ");
            field("reason", gk_cap_reason(r.caps[c]));
            printf(", \"value\": %u}", (unsigned)gk_cap_value(r.caps[c]));
        }
        printf("\n    ]");

        printf(",\n    \"records\": [");
        for(uint8_t k = 0; k < tag.record_count; k++) {
            if(k) printf(",");
            printf("\n      {");
            field("kind", gk_rec_kind_name(tag.rec[k].kind));
            printf(", ");
            field("summary", tag.rec[k].summary);
            printf("}");
        }
        printf("\n    ]");

        printf("\n  }%s\n", (i + 1 < gk_demo_count()) ? "," : "");
    }

    printf("]\n");
    return 0;
}
