/* Taking a URL apart the way a phone does, not the way a person does.
 *
 * Every URL-based phishing trick in existence is the same trick: the string
 * contains something that reads like a destination and something that *is*
 * the destination, and they are not the same. A browser only ever obeys the
 * authority component -- and inside that, only the registrable domain
 * decides who you are actually talking to. Everything else is decoration
 * that the attacker controls completely.
 *
 *      https://apple.com.id-verify.secure@login-x.top:8443/signin?next=...
 *             \_____________________________/\________/
 *                    all attacker-chosen        this is where you go
 *
 * So this file does two jobs. It splits a URL into its real components, and
 * then it finds the registrable domain -- the "+1" of eTLD+1 -- so the rest
 * of Gatekeeper can point at the eight characters that matter and dim the
 * hundred that do not.
 *
 * No furi, no allocation, no libc beyond string.h: it is built for the host
 * test suite first and the firmware second.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The longest URL Gatekeeper will carry around. An NTAG216 can hold 888
 * bytes of NDEF, so a tag can legally hold a longer URL than this; when that
 * happens we say so rather than grading a truncated string. */
#define GK_URL_MAX 512

/* A slice of the original string. Offsets, not pointers, so the whole thing
 * can be memcpy'd between threads and stored in a settings file. */
typedef struct {
    uint16_t off;
    uint16_t len;
} GkSpan;

typedef enum {
    GkSchemeNone = 0, /* no "x:" prefix at all */
    GkSchemeHttps,
    GkSchemeHttp,
    GkSchemeTel,
    GkSchemeMailto,
    GkSchemeSms,
    GkSchemeGeo,
    GkSchemeFtp,
    GkSchemeFile,
    GkSchemeJavascript,
    GkSchemeData,
    GkSchemeIntent, /* android intent: -- launches an app with parameters */
    GkSchemeMarket, /* market: / play store deep link */
    GkSchemeOther,
} GkScheme;

/* How the hostname is written down. */
typedef enum {
    GkHostName = 0, /* ordinary labels */
    GkHostIpv4,
    GkHostIpv6,
    GkHostEmpty,
} GkHostForm;

typedef struct {
    const char* raw; /* not owned; must outlive the GkUrl */
    uint16_t raw_len;

    GkScheme scheme;
    GkSpan scheme_s; /* "https", without the colon */

    bool has_authority; /* the "//" was present */
    bool has_userinfo;
    GkSpan userinfo; /* everything before the '@' */
    GkSpan host; /* hostname only: no userinfo, no port, no brackets */
    GkSpan authority; /* userinfo + host + port, as written */
    bool has_port;
    uint16_t port;

    GkSpan path;
    GkSpan query;
    GkSpan fragment;

    GkHostForm host_form;
    bool host_has_punycode; /* some label starts "xn--" */
    bool host_has_escape; /* percent-encoding inside the authority */
    bool host_trailing_dot;
    uint8_t label_count;

    /* The answer to "where does this actually go". For an IP host it is the
     * whole address; for a single-label host it is that label. */
    GkSpan registrable;
    GkSpan suffix; /* the public suffix: "com", "co.uk", "github.io" */
    GkSpan core; /* registrable minus suffix: the one label that was bought */
    bool suffix_is_hosting; /* the suffix hands out subdomains to anyone */
    bool suffix_multi; /* the suffix is more than one label */
} GkUrl;

/** Split `url` into components. `len` may be 0 to use strlen.
 *
 *  Returns false only for an empty string; anything else is parsed as far as
 *  it makes sense, because a malformed URL is not an error here -- it is a
 *  finding. Spans of absent components have len 0.
 *
 *  `out->raw` aliases `url`, so `url` must outlive `out`.
 */
bool gk_url_parse(const char* url, size_t len, GkUrl* out);

/* ------------------------------------------------------------------ zones
 *
 * The verdict screen draws the URL in four bands: the part before the host,
 * the subdomain, the registrable domain, and everything after it. Only band
 * 2 is inverted, because only band 2 decides where you land. Views ask for
 * the offsets rather than re-deriving them, so the highlight can never
 * disagree with the grade.
 */
typedef struct {
    uint16_t pre_len; /* scheme://userinfo@   -- dim */
    uint16_t sub_off, sub_len; /* subdomain labels     -- dim */
    uint16_t reg_off, reg_len; /* registrable domain   -- INVERTED */
    uint16_t post_off, post_len; /* :port/path?query#frag -- dim */
} GkUrlZones;

void gk_url_zones(const GkUrl* u, GkUrlZones* out);

/* --------------------------------------------------------------- helpers */

/** Case-insensitive compare of a span against a NUL-terminated string. */
bool gk_span_eq(const char* raw, GkSpan s, const char* str);
/** Case-insensitive "does the span contain this substring". */
bool gk_span_contains(const char* raw, GkSpan s, const char* needle);
/** Copy a span out as a NUL-terminated string, truncating to cap. */
void gk_span_copy(const char* raw, GkSpan s, char* dst, size_t cap);

/** Human name for a scheme, for the tag-details screen. */
const char* gk_scheme_name(GkScheme s);

/** True when the scheme is one a browser will fetch over the network. */
bool gk_scheme_is_web(GkScheme s);

/** Percent-decode in place-ish: writes at most cap-1 bytes plus NUL, and
 *  reports whether any escape was found and whether a decoded byte was
 *  itself a '%' (double encoding, which is only ever done to hide something).
 */
size_t gk_percent_decode(
    const char* src,
    size_t src_len,
    char* dst,
    size_t cap,
    bool* found_escape,
    bool* double_encoded);

/** Fold a hostname label to its confusable skeleton: lowercase, strip
 *  separators, map digits and multigraphs that render like letters
 *  (0->o, 1->l, rn->m, vv->w, ...). Two strings with the same skeleton look
 *  the same on a phone screen at arm's length. */
size_t gk_confusable_fold(const char* src, size_t src_len, char* dst, size_t cap);

/** True when a and b differ only by one adjacent transposition ("paypla"). */
bool gk_is_transposition(const char* a, size_t alen, const char* b, size_t blen);

#ifdef __cplusplus
}
#endif
