/* The grader.
 *
 * Everything Gatekeeper shows is a rendering of what gk_verdict_grade()
 * decided, which is why this file is furi-free and why the host tests get to
 * take it apart signal by signal.
 *
 * Two rules shape the whole design.
 *
 * First: a finding is never a verdict on its own. Signals subtract points
 * from 100, and the total decides the grade -- so a tag has to be wrong in
 * several independent ways to reach the bottom, and one unlucky coincidence
 * cannot condemn a real business.
 *
 * Second, and more important: some things are ceilings, not deductions. If
 * the destination is a URL shortener, the tag simply does not contain the
 * address you are going to, and no amount of everything-else-looks-fine can
 * make that knowable -- so it is capped, permanently, at C. The ceilings are
 * the honest part of this application. They are listed on screen with the
 * findings, and one of them is always on:
 *
 *     Gatekeeper reads the tag. It cannot read the website.
 *
 * A URL can be spotless and still lead to a page that asks for your card
 * number. Nothing here can see that page -- the Flipper has no network
 * connection, by design -- so the top of the scale is A, A+ is unreachable,
 * and the word "safe" does not appear anywhere in the application.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gk_ndef.h"
#include "gk_url.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GK_FIND_MAX 14
#define GK_CAP_MAX 4
#define GK_EVIDENCE_MAX 40

typedef enum {
    GkGradeNone = 0, /* nothing to grade: no URL on the tag */
    GkGradeAPlus,
    GkGradeA,
    GkGradeB,
    GkGradeC,
    GkGradeD,
    GkGradeF,
} GkGrade;

typedef enum {
    GkVerdictNothing = 0, /* no link on this tag */
    GkVerdictOrdinary,
    GkVerdictCheck,
    GkVerdictCaution,
    GkVerdictSuspicious,
    GkVerdictDanger,
} GkVerdict;

/* Which kind of evidence a signal is. The families exist so the findings
 * screen can group them, and so a person can see at a glance whether one
 * thing is wrong four times or four things are wrong once. */
typedef enum {
    GkFamDeception = 0, /* the address is dressed up as somewhere else */
    GkFamRoute, /* where it goes and how it gets there */
    GkFamIntent, /* what it wants when you arrive */
    GkFamTag, /* the tag itself, not the link */
    GkFamNote, /* worth knowing, worth no points */
} GkFamily;

typedef enum {
    GkSigNone = 0,

    /* -------- dressed up as somewhere else -------- */
    GkSigUserinfo, /* https://apple.com@evil.tld */
    GkSigDomainInside, /* "apple.com" inside a host that is not apple.com */
    GkSigHomoglyph, /* paypa1 / arnazon / pay-pal */
    GkSigBrandSubdomain, /* paypal.secure-login.xyz */
    GkSigBrandCore, /* paypal-verify.com */
    GkSigBrandBadTld, /* apple.tk, netflix.github.io */
    GkSigBrandPath, /* evil.xyz/paypal/signin */
    GkSigTitleMismatch, /* the poster's words name someone else */
    GkSigPunycode, /* xn-- : the label is not what it renders as */
    GkSigNonAscii, /* raw high bytes in the URL */
    GkSigIpHost, /* a number instead of a name */
    GkSigOpenRedirect, /* ?next=https://... bounces you onward */
    GkSigEncodedAuthority, /* percent-escapes inside the hostname */
    GkSigDoubleEncoded, /* %2520 : encoded twice, to get past something */
    GkSigDeepSubdomain, /* a.b.c.d.e.example.com */
    GkSigTrailingDot, /* example.com. resolves, and defeats string checks */

    /* -------- where it goes -------- */
    GkSigPlainHttp,
    GkSigShortener,
    GkSigFreeHosting,
    GkSigRiskyTld,
    GkSigFilenameTld, /* .zip and .mov look like attachments */
    GkSigNonStdPort,
    GkSigNoScheme,

    /* -------- what it wants -------- */
    GkSigDangerousScheme, /* javascript:, data:, intent:, file: */
    GkSigNonWebAction, /* tel: / sms: / mailto: -- it acts, it does not open */
    GkSigDirectDownload, /* the link is a file, not a page */
    GkSigCredKeywords, /* login / verify / account / suspended */
    GkSigCryptoKeywords, /* wallet / seed phrase / claim / connect */
    GkSigVeryLong,
    GkSigManyParams,

    /* -------- the tag itself -------- */
    GkSigRewritable,
    GkSigAar, /* also names an Android app to open or install */
    GkSigExtraRecords,
    GkSigUidMirror, /* the tag writes its own serial into the URL */
    GkSigPwdProtected,
    GkSigMalformed,
    GkSigUrlTruncated,

    /* -------- notes, worth no points -------- */
    GkSigHttps,
    GkSigKnownBrand,
    GkSigLocked,
    GkSigMoneyContext,

    GkSigCount,
} GkSignalId;

/* Ceilings. Every one of these is shown to the user with its reason -- a
 * score that was held down deserves to say by what. */
typedef enum {
    GkCapNoSite = 0, /* always on: the page itself is not visible from here */
    GkCapShortener, /* the destination is not in the tag */
    GkCapRewritable, /* it can say something else tomorrow */
    GkCapPlainHttp, /* the connection is readable and changeable in transit */
    GkCapImpersonation, /* it is dressed as somebody else */
    GkCapPayload, /* it hands over a file, not a page */
    GkCapHostile, /* it is not a web address at all */
    GkCapCount,
} GkCapId;

typedef struct {
    GkSignalId id;
    uint8_t points; /* subtracted from 100 */
    /* Some signals carry the offending text -- the brand impersonated, the
     * parameter that redirects -- so the detail screen can quote it rather
     * than describe it. */
    char evidence[GK_EVIDENCE_MAX];
} GkFinding;

typedef struct {
    GkTagStatus status;

    uint8_t score; /* 0..100, after ceilings */
    uint8_t raw_score; /* before ceilings, so the why-screen can show both */
    GkGrade grade;
    GkVerdict verdict;

    GkFinding find[GK_FIND_MAX];
    uint8_t find_count;
    uint8_t find_total; /* including any past GK_FIND_MAX */

    GkCapId caps[GK_CAP_MAX]; /* ceilings that actually bound this score */
    uint8_t cap_count;
    uint8_t worst_family; /* GkFamily with the most points against it */

    /* Copied out of the URL so the views never have to re-parse. */
    char host[80];
    char registrable[64];
    bool have_url;
} GkVerdictResult;

/* -------------------------------------------------------------- metadata */

typedef struct {
    const char* label; /* one line for the findings list */
    const char* what; /* what was actually observed */
    const char* why; /* why that matters, in plain words */
    const char* advice; /* what to do about it */
    GkFamily family;
    uint8_t points;
    /* The ceiling this signal brings down with it, or GkCapCount for none.
     * The numeric value lives with the ceiling, in gk_cap_value(), so that
     * two signals arguing for the same ceiling cannot disagree about it. */
    GkCapId cap;
} GkSignalInfo;

const GkSignalInfo* gk_signal_info(GkSignalId id);
const char* gk_family_name(GkFamily f);
const char* gk_grade_name(GkGrade g);
const char* gk_verdict_name(GkVerdict v);
/* The short imperative under the big letter: "DO NOT TAP", "CHECK FIRST". */
const char* gk_verdict_action(GkVerdict v);
const char* gk_cap_label(GkCapId c);
const char* gk_cap_reason(GkCapId c);
/** The highest score this ceiling permits. */
uint8_t gk_cap_value(GkCapId c);
/** The lowest score that still earns this grade -- used by the why-screen to
 *  say "two points from a B" without duplicating the band table. */
uint8_t gk_grade_floor(GkGrade g);

/** Grade a tag. `tag` supplies both the URL and the tag-level evidence.
 *  Safe to call with a tag carrying no URL: the result comes back with
 *  GkGradeNone and a status the caller can explain. */
void gk_verdict_grade(const GkTag* tag, GkVerdictResult* out);

/** Grade a bare URL with no tag context. Used by the host tests and by the
 *  demo generator; the tag-family signals simply never fire. */
void gk_verdict_grade_url(const char* url, GkVerdictResult* out);

#ifdef __cplusplus
}
#endif
