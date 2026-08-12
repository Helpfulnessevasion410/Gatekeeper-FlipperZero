/* NDEF: turning a pocketful of bytes into "your phone will open this".
 *
 * A tag stuck to a lamp post is not a trusted input. It is a few hundred
 * bytes chosen by whoever held it last, and everything in this file runs on
 * those bytes before anybody has decided whether to believe them. So the
 * parser is written the defensive way round: every length is checked against
 * what is actually left in the buffer, nesting is bounded, and a field that
 * does not make sense truncates the parse instead of being trusted -- a
 * malformed tag is a finding, never a crash.
 *
 * There is no NDEF parser in the Flipper SDK, so this is hand-rolled, the
 * same way Moneta hand-rolls EMV. It covers what tags in the street actually
 * contain: the NFC Forum Type 2 TLV wrapper (NTAG213/215/216, Ultralight),
 * bare messages as a Type 4 tag hands them over, URI records with the
 * well-known prefix table, Text records, Smart Posters with their nested
 * message, and Android Application Records.
 *
 * furi-free and allocation-free: it is host-tested, including against
 * deliberately broken input.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gk_url.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An NTAG216 holds 888 user bytes; nothing on a poster is bigger. Anything
 * that is gets read up to here and flagged truncated rather than dropped. */
#define GK_NDEF_MAX 924
#define GK_REC_MAX 8
#define GK_TITLE_MAX 64
#define GK_AAR_MAX 48
#define GK_TYPE_MAX 20
#define GK_SUMMARY_MAX 34
#define GK_UID_MAX 10
#define GK_TECH_MAX 22

typedef enum {
    GkTechUnknown = 0,
    GkTechType2, /* NTAG21x / Ultralight -- what posters actually use */
    GkTechType4, /* ISO-DEP: DESFire, Java Card, phone card emulation */
    GkTechClassic, /* Mifare Classic: NDEF exists but needs keys */
    GkTechFelica,
    GkTechOther,
} GkTech;

typedef enum {
    GkRecUnknown = 0,
    GkRecEmpty,
    GkRecUri, /* well-known "U" */
    GkRecText, /* well-known "T" */
    GkRecSmartPoster, /* well-known "Sp" -- carries a nested message */
    GkRecAar, /* external "android.com:pkg" */
    GkRecMime,
    GkRecAbsUri,
    GkRecExternal,
} GkRecKind;

typedef struct {
    GkRecKind kind;
    uint8_t tnf;
    bool nested; /* found inside a Smart Poster */
    bool chunked;
    uint16_t payload_len;
    char type[GK_TYPE_MAX];
    char summary[GK_SUMMARY_MAX]; /* one line for the record list */
} GkRecord;

/* Why there is no URL to grade. Each of these is its own screen, because
 * "this tag is empty" and "this tag is not readable" are different pieces of
 * news for the person holding it. */
typedef enum {
    GkTagOk = 0,
    GkTagNoNdef, /* readable, formatted or not, but carries no NDEF */
    GkTagNoUrl, /* NDEF present, but nothing that opens a link */
    GkTagUnreadable, /* the technology is fine, the data is not */
    GkTagNeedsKeys, /* Mifare Classic: NDEF is there, behind a key */
} GkTagStatus;

typedef struct {
    GkTech tech;
    char tech_name[GK_TECH_MAX]; /* "NTAG215", "MIFARE DESFire" */
    uint8_t uid[GK_UID_MAX];
    uint8_t uid_len;

    GkTagStatus status;

    uint16_t capacity; /* bytes of NDEF space the tag advertises */
    uint16_t used; /* bytes the current message occupies */
    bool size_known;

    bool writable; /* anyone can overwrite this tag, right now */
    bool write_known; /* we were able to determine it at all */
    bool pwd_protected; /* part of the tag is behind a password */

    uint8_t record_count;
    uint8_t record_total; /* including ones past GK_REC_MAX */
    GkRecord rec[GK_REC_MAX];

    bool has_url;
    char url[GK_URL_MAX];
    uint16_t url_len;
    bool url_truncated;

    bool has_title; /* Smart Poster title: the words next to the link */
    char title[GK_TITLE_MAX];

    bool has_aar; /* the tag also names an app to open or install */
    char aar[GK_AAR_MAX];

    bool malformed; /* something did not add up on the way in */
} GkTag;

/** Reset a tag record to the "nothing read yet" state. */
void gk_tag_init(GkTag* tag);

/** Parse the Type 2 TLV area (everything from page 4 onward).
 *
 *  Walks the TLV chain, skipping lock and memory control blocks, and hands
 *  the NDEF TLV's value to the message parser. Returns false when no NDEF
 *  TLV was found -- which is a perfectly ordinary state for a blank tag, and
 *  sets tag->status accordingly.
 */
bool gk_ndef_parse_tlv(const uint8_t* data, size_t len, GkTag* tag);

/** Parse a bare NDEF message, as a Type 4 tag hands it over after NLEN.
 *  `depth` guards the Smart Poster recursion; callers pass 0. */
bool gk_ndef_parse_message(const uint8_t* msg, size_t len, GkTag* tag, uint8_t depth);

/** Expand a URI record payload (prefix byte + remainder) into a full URL.
 *  Returns the number of bytes written, and sets *truncated when the record
 *  was longer than the destination. */
size_t gk_ndef_expand_uri(
    const uint8_t* payload,
    size_t len,
    char* dst,
    size_t cap,
    bool* truncated);

/** The NFC Forum well-known URI prefix for a code, or "" if out of range. */
const char* gk_ndef_uri_prefix(uint8_t code);

/** Human names, for the tag-details screen. */
const char* gk_tech_name(GkTech t);
const char* gk_rec_kind_name(GkRecKind k);

#ifdef __cplusplus
}
#endif
