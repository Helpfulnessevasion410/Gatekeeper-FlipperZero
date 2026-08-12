#include "gk_verdict.h"

#include <string.h>

/* ------------------------------------------------------------- ceilings */

typedef struct {
    const char* label;
    const char* reason;
    uint8_t value;
} GkCapInfo;

static const GkCapInfo gk_caps[GkCapCount] = {
    [GkCapNoSite] =
        {"Cannot see the site",
         "Gatekeeper reads the tag, not the website. A perfectly ordinary address can still "
         "lead to a page that asks for your card. The Flipper has no network connection and "
         "never will, so the top of this scale is A. A+ is unreachable on purpose.",
         92},
    [GkCapShortener] =
        {"Shortened link",
         "The address you would actually visit is not stored on this tag. It lives on the "
         "shortening service's server, can be pointed somewhere else after the tag is "
         "printed, and cannot be looked up from here. An unknown destination cannot be "
         "graded better than caution.",
         58},
    [GkCapRewritable] =
        {"Tag is still writable",
         "This tag has not been locked, so any phone with a writing app can replace what is "
         "on it in about two seconds. Whatever it says now is what it says now.",
         84},
    [GkCapPlainHttp] =
        {"Unencrypted link",
         "http:// with no s. Anything you send goes in clear text, and anyone between you "
         "and the server can read it or change the page before it reaches you.",
         66},
    [GkCapImpersonation] =
        {"Dressed as someone else",
         "Part of this address is arranged to read as a name it does not belong to. That is "
         "not a coincidence that happens to honest links.",
         44},
    [GkCapPayload] =
        {"Hands over a file",
         "This link is not a page, it is a download. A tag on a wall has no business "
         "installing anything.",
         36},
    [GkCapHostile] =
        {"Not a web address",
         "This does not open a website. It hands instructions straight to the phone, and "
         "that is the whole reason it was written this way.",
         14},
};

const char* gk_cap_label(GkCapId c) {
    if(c >= GkCapCount) return "";
    return gk_caps[c].label;
}
const char* gk_cap_reason(GkCapId c) {
    if(c >= GkCapCount) return "";
    return gk_caps[c].reason;
}
uint8_t gk_cap_value(GkCapId c) {
    if(c >= GkCapCount) return 100;
    return gk_caps[c].value;
}

/* --------------------------------------------------------------- signals
 *
 * Points are deliberately not round numbers of severity. They were set by
 * walking the bands backwards: what combination *should* land a tag in D,
 * and what should it take to reach F. The host tests pin the answers.
 */

static const GkSignalInfo gk_signals[GkSigCount] = {
    [GkSigNone] = {"", "", "", "", GkFamNote, 0, GkCapCount},

    /* ---------------------------------------------------------- deception */
    [GkSigUserinfo] =
        {"Hidden real destination",
         "There is an @ inside the address. Everything before it is ignored by the phone.",
         "This is the oldest trick there is, and still the most effective: the part that "
         "reads like a familiar company is a username, not a destination. The phone throws "
         "it away and goes to whatever follows the @.",
         "Read the address backwards from the first single slash. That is where you go.",
         GkFamDeception, 46, GkCapImpersonation},
    [GkSigDomainInside] =
        {"Real name buried inside",
         "A well-known domain appears inside this hostname without being the hostname.",
         "apple.com.id-check.top is not Apple. Only the last two labels before the first "
         "slash decide where you land; everything to the left of them is decoration the "
         "attacker chose, and putting a famous domain there is free.",
         "Look at the highlighted part on the verdict screen. That is the whole address.",
         GkFamDeception, 36, GkCapImpersonation},
    [GkSigHomoglyph] =
        {"Look-alike spelling",
         "The domain is spelled to read as a well-known name at a glance.",
         "A zero for an o, a one for an l, rn for m, or a hyphen dropped into the middle. "
         "None of it survives being read letter by letter, and none of it is ever meant to "
         "be: it is meant to be glanced at in the street.",
         "Read the highlighted domain out loud, one character at a time.",
         GkFamDeception, 40, GkCapImpersonation},
    [GkSigBrandSubdomain] =
        {"Brand used as a subdomain",
         "A well-known name is a subdomain here, not the domain.",
         "Anyone who owns a domain can create paypal.theirs.com in seconds, for nothing, "
         "and it is not connected to PayPal in any way. Subdomains are given away by the "
         "owner of the domain, and the owner is not who the subdomain says.",
         "The name that matters is the one immediately before the final .com or .co.uk.",
         GkFamDeception, 34, GkCapImpersonation},
    [GkSigBrandCore] =
        {"Brand inside the domain",
         "A well-known name is glued into a domain that is not theirs.",
         "paypal-secure-login.com was bought by somebody, and it was not PayPal. Real "
         "companies send you to their own domain; they do not register a new one that "
         "contains their name plus a reassuring word.",
         "Check the company's own app or the address printed on your card.",
         GkFamDeception, 30, GkCapImpersonation},
    [GkSigBrandBadTld] =
        {"Brand somewhere it would not be",
         "A well-known name, on a free host or a bargain-bin domain ending.",
         "The name is spelled correctly, but no bank runs its login page on a giveaway "
         "domain or a free hosting subdomain. The spelling is right and the address is "
         "still not theirs.",
         "Go to the address you already know for this company instead.",
         GkFamDeception, 32, GkCapImpersonation},
    [GkSigBrandPath] =
        {"Brand only in the path",
         "The well-known name appears after the domain, where it means nothing.",
         "Everything after the first single slash is chosen by whoever owns the domain. "
         "They can write /paypal/login/ or /your-bank/ because it costs nothing and it is "
         "the part people read first.",
         "Ignore everything after the domain when deciding who you are talking to.",
         GkFamDeception, 18, GkCapCount},
    [GkSigTitleMismatch] =
        {"Words do not match link",
         "The text stored on the tag names somebody other than the destination.",
         "A Smart Poster carries a caption next to its link, and phones show the caption. "
         "When the caption says one company and the link goes to another, the caption is "
         "the bait.",
         "The caption is written by the same person as the link. Believe the link.",
         GkFamDeception, 30, GkCapImpersonation},
    [GkSigPunycode] =
        {"Not the letters you see",
         "A label begins xn--, meaning it renders as something other than it is stored.",
         "Internationalised domains let a hostname contain letters from any alphabet, "
         "including ones drawn identically to Latin letters. The rendered name can be "
         "pixel-for-pixel identical to a real one and be a completely different domain.",
         "Treat a rendered name you cannot verify as unknown, however right it looks.",
         GkFamDeception, 30, GkCapImpersonation},
    [GkSigNonAscii] =
        {"Unusual characters",
         "The address contains bytes outside plain ASCII.",
         "Web addresses are ASCII. Anything else arrived either through a mangled encoding "
         "or because somebody wanted a character that draws like a letter it is not.",
         "Do not type this address in by hand either; the characters will not match.",
         GkFamDeception, 20, GkCapImpersonation},
    [GkSigIpHost] =
        {"A number, not a name",
         "The destination is a raw IP address.",
         "No business puts an address like this on a poster. There is no domain to check, "
         "nothing to look up, no certificate that means anything, and nothing tying the "
         "machine to anybody.",
         "There is no legitimate reason for a public tag to do this.",
         /* No ceiling: a bare IP is a bad sign, not a disguise. It reaches D
          * on points alone, and calling it impersonation would put the wrong
          * words on the why-screen. */
         GkFamDeception, 32, GkCapCount},
    [GkSigOpenRedirect] =
        {"Bounces somewhere else",
         "A parameter in this link carries a second, complete web address.",
         "The link opens one site and that site immediately forwards you to the address "
         "carried in the parameter. The name you checked is real; it is just not where you "
         "end up.",
         "The address after the parameter is the destination. Grade that one.",
         GkFamDeception, 26, GkCapCount},
    [GkSigEncodedAuthority] =
        {"Escaped characters in host",
         "Percent-escapes appear inside the hostname itself.",
         "Hostnames do not need escaping. When one contains %XX it is to stop the part you "
         "would recognise from being readable until the phone has already decoded it.",
         "Anything that has to hide its own hostname has a reason.",
         GkFamDeception, 26, GkCapImpersonation},
    [GkSigDoubleEncoded] =
        {"Encoded twice over",
         "The address contains escapes of escapes (%25 and friends).",
         "Encoding something twice is not how links are written; it is how they are slipped "
         "past a filter that only decodes once.",
         "Treat double encoding as intent, not as an accident.",
         GkFamDeception, 24, GkCapCount},
    [GkSigDeepSubdomain] =
        {"Very deep subdomain",
         "There are several labels stacked in front of the real domain.",
         "Long chains of subdomains are used to push the real domain off the right-hand "
         "edge of a phone's address bar, so the part that is visible is the part the "
         "attacker wrote.",
         "Scroll the address to the end of the host; the last two labels are the answer.",
         GkFamDeception, 10, GkCapCount},
    [GkSigTrailingDot] =
        {"Trailing dot on host",
         "The hostname ends in a dot.",
         "example.com. and example.com go to the same place, but they are different strings, "
         "which is exactly why the form appears: it slips past checks that compare text.",
         "Harmless by itself, deliberate in combination with anything else here.",
         GkFamDeception, 12, GkCapCount},

    /* -------------------------------------------------------------- route */
    [GkSigPlainHttp] =
        {"No encryption",
         "The link is http://, not https://.",
         "Everything sent to this site travels in clear text. On public Wi-Fi that means "
         "anyone nearby can read what you send and rewrite what comes back before it "
         "reaches your screen.",
         "Never enter anything into a page reached over plain http.",
         GkFamRoute, 20, GkCapPlainHttp},
    [GkSigShortener] =
        {"Shortened link",
         "The host is a link-shortening service.",
         "The tag does not contain the destination -- it contains a lookup key for one. The "
         "owner can change where it points at any time, including after the tag has been "
         "inspected and stuck to a wall.",
         "Use a shortener-expanding site on a machine you do not mind, or just do not.",
         GkFamRoute, 16, GkCapShortener},
    [GkSigFreeHosting] =
        {"Free hosting subdomain",
         "The domain hands out subdomains to anyone, usually free and instantly.",
         "These services are legitimate and heavily used by phishing kits for the same "
         "reason: a working https site under a plausible-looking name, in under a minute, "
         "with no identity check and nothing to trace.",
         "The company you think this is would use its own domain.",
         GkFamRoute, 22, GkCapCount},
    [GkSigRiskyTld] =
        {"Bargain domain ending",
         "The domain ends in one of the cheap or free endings abuse concentrates in.",
         "Plenty of honest sites use these. The point is the ratio: when a domain costs "
         "pennies and needs no identity, it gets used once and abandoned, which is exactly "
         "what a scam campaign wants.",
         "Worth very little alone. Worth a lot next to anything else on this list.",
         GkFamRoute, 14, GkCapCount},
    [GkSigFilenameTld] =
        {"Ending looks like a file",
         "The domain ends .zip or .mov -- endings that are also filenames.",
         "update.zip can be a file you were sent or a website somebody registered. Chat "
         "apps and mail clients turn the text into a link automatically, and the two are "
         "indistinguishable to a reader.",
         "Nothing on a poster needs a domain that reads as an attachment.",
         GkFamRoute, 22, GkCapCount},
    [GkSigNonStdPort] =
        {"Unusual port",
         "The address names a port other than the standard web ports.",
         "Ordinary sites do not need this. It usually means something is being served off a "
         "machine that is not set up as a public web server -- someone's box, or a tunnel "
         "out of one.",
         "",
         GkFamRoute, 14, GkCapCount},
    [GkSigNoScheme] =
        {"No scheme on the link",
         "The stored address does not say http, https or anything else.",
         "The tag leaves it to the phone to guess, and phones guess http. It is sloppy on an "
         "honest tag and useful on a dishonest one, because the stored text no longer "
         "matches what gets opened.",
         "",
         GkFamRoute, 10, GkCapCount},

    /* ------------------------------------------------------------- intent */
    [GkSigDangerousScheme] =
        {"Not a web link at all",
         "The scheme is one that hands instructions to the phone directly.",
         "javascript:, data: and intent: do not fetch a page. They run something, or hand a "
         "whole document straight to the browser with no server involved and no address to "
         "check. A tag on a wall has no honest use for any of them.",
         "Do not tap this. There is no version of this that is fine.",
         GkFamIntent, 60, GkCapHostile},
    [GkSigNonWebAction] =
        {"Dials, texts or emails",
         "This tag does not open a page: it starts a call, a message or an email.",
         "Premium-rate numbers and callback scams work exactly like this. The number is "
         "already filled in, the confirmation is one tap away, and nobody checks a number "
         "they did not type.",
         "Read the number or address in full before confirming anything.",
         GkFamIntent, 18, GkCapCount},
    [GkSigDirectDownload] =
        {"Downloads a file",
         "The link ends in an installer or executable file type.",
         "A poster that hands you an .apk or an .exe is not offering you a menu. Android "
         "will warn you and the warning is the last thing standing between you and an "
         "install you did not choose.",
         "Install from the official store, never from a link on a wall.",
         GkFamIntent, 34, GkCapPayload},
    [GkSigCredKeywords] =
        {"Asks about your account",
         "The address contains login, verify, secure, suspended or similar.",
         "Real companies do use these words in their URLs. The reason it counts here is "
         "that a tag is a strange way to be sent to a login page at all -- nobody signs in "
         "to their bank because a sticker asked them to.",
         "Reach a login page the way you normally would, not from a tag.",
         GkFamIntent, 14, GkCapCount},
    [GkSigCryptoKeywords] =
        {"Wallet or seed phrase",
         "The address mentions wallets, seed phrases, claims or airdrops.",
         "This is the single most expensive category of tag-and-code scam there is, because "
         "the transaction cannot be reversed and there is no bank to call. A page that asks "
         "you to connect a wallet or type a recovery phrase is a theft in progress.",
         "No legitimate service will ever ask for a seed phrase. Not once, not ever.",
         GkFamIntent, 30, GkCapCount},
    [GkSigVeryLong] =
        {"Very long address",
         "The address is long enough that no phone will show it all.",
         "Length is how the important part gets pushed out of sight. Whatever is visible in "
         "a notification is the part chosen to be visible.",
         "",
         GkFamIntent, 8, GkCapCount},
    [GkSigManyParams] =
        {"Loaded with parameters",
         "The address carries an unusual number of parameters.",
         "Not suspicious by itself -- tracking looks like this too -- but it is a good place "
         "to hide a second address, and it makes the link impossible to read at a glance.",
         "",
         GkFamIntent, 6, GkCapCount},

    /* ---------------------------------------------------------------- tag */
    [GkSigRewritable] =
        {"Tag is not locked",
         "The tag can still be written to by anyone.",
         "Most tags in the street are left like this, so it is common rather than damning. "
         "It does mean that what this tag says is not a property of the tag -- it is "
         "whatever the last person to hold a phone against it decided.",
         "A business that cares about its tags locks them.",
         GkFamTag, 8, GkCapRewritable},
    [GkSigAar] =
        {"Also names an app",
         "The tag carries an Android Application Record.",
         "As well as the link, the tag names a specific app. If it is installed the tag "
         "opens it; if it is not, Android offers to go and install it. The name in the "
         "record is not checked against anything.",
         "Look at the package name on the tag details screen before you let it install.",
         GkFamTag, 16, GkCapCount},
    [GkSigExtraRecords] =
        {"Extra records on the tag",
         "There is more on this tag than the link.",
         "A poster tag normally holds one URL, sometimes with a caption. Extra records are "
         "not automatically sinister, but they are extra instructions arriving with the "
         "link, and they are worth seeing.",
         "The tag details screen lists every record.",
         GkFamTag, 8, GkCapCount},
    [GkSigUidMirror] =
        {"Tag serial in the link",
         "The tag's own serial number appears inside the address.",
         "The tag writes its unique serial into the URL as you read it, so the site knows "
         "precisely which physical tag was tapped, and can tell you apart from everyone "
         "else who tapped a different one.",
         "Ordinary for stock control and marketing. Still worth knowing about.",
         GkFamTag, 8, GkCapCount},
    [GkSigPwdProtected] =
        {"Part of the tag is locked away",
         "The tag has password protection configured.",
         "Some of this tag's memory could not be read. That is unusual on a public poster, "
         "and it means this grade is based on less than the whole tag.",
         "",
         GkFamTag, 8, GkCapCount},
    [GkSigMalformed] =
        {"Tag structure is broken",
         "The tag's own record lengths do not add up.",
         "Either it was written badly, or it was written to be parsed differently by "
         "different readers -- which is a technique in itself. Either way, what your phone "
         "makes of this tag may not be what Gatekeeper made of it.",
         "",
         GkFamTag, 14, GkCapCount},
    [GkSigUrlTruncated] =
        {"Address too long to read",
         "The address on this tag is longer than Gatekeeper will hold.",
         "The grade covers the part that was read. The rest was not, and the end of a URL "
         "is a perfectly good place to keep the interesting part.",
         "",
         GkFamTag, 10, GkCapCount},

    /* -------------------------------------------------------------- notes */
    [GkSigHttps] =
        {"Encrypted connection",
         "The link is https://.",
         "The connection is encrypted and the server proved it owns that name. That is all "
         "it proves. A padlock says nobody is listening in; it does not say who you are "
         "talking to, and criminals get certificates for free in about a minute.",
         "",
         GkFamNote, 0, GkCapCount},
    [GkSigKnownBrand] =
        {"Recognised domain",
         "The domain is one Gatekeeper knows by name.",
         "The spelling is exact and the ending is an ordinary one. This is the good case -- "
         "but it is recognition of a name, not a verdict on a website.",
         "",
         GkFamNote, 0, GkCapCount},
    [GkSigLocked] =
        {"Tag is locked",
         "This tag can no longer be rewritten.",
         "Nobody can change what is on it now, including its owner. Worth remembering that "
         "locking happens once and whoever did it chose the contents -- a locked tag is "
         "permanent, not trustworthy.",
         "",
         GkFamNote, 0, GkCapCount},
    [GkSigMoneyContext] =
        {"This link is about money",
         "The address mentions payment, billing, parking, a fine or a refund.",
         "Perfectly normal for a parking meter or a restaurant bill -- and that is the "
         "point. These are the tags worth being certain about, because they are the ones "
         "where being wrong costs something.",
         "",
         GkFamNote, 0, GkCapCount},
};

const GkSignalInfo* gk_signal_info(GkSignalId id) {
    if(id >= GkSigCount) return &gk_signals[GkSigNone];
    return &gk_signals[id];
}

const char* gk_family_name(GkFamily f) {
    switch(f) {
    case GkFamDeception:
        return "Looks like elsewhere";
    case GkFamRoute:
        return "Where it goes";
    case GkFamIntent:
        return "What it wants";
    case GkFamTag:
        return "The tag itself";
    case GkFamNote:
    default:
        return "Worth knowing";
    }
}

const char* gk_grade_name(GkGrade g) {
    switch(g) {
    case GkGradeAPlus:
        return "A+";
    case GkGradeA:
        return "A";
    case GkGradeB:
        return "B";
    case GkGradeC:
        return "C";
    case GkGradeD:
        return "D";
    case GkGradeF:
        return "F";
    case GkGradeNone:
    default:
        return "-";
    }
}

const char* gk_verdict_name(GkVerdict v) {
    switch(v) {
    case GkVerdictOrdinary:
        return "NOTHING ODD";
    case GkVerdictCheck:
        return "WORTH A LOOK";
    case GkVerdictCaution:
        return "CAUTION";
    case GkVerdictSuspicious:
        return "SUSPICIOUS";
    case GkVerdictDanger:
        return "DO NOT TAP";
    case GkVerdictNothing:
    default:
        return "NO LINK";
    }
}

const char* gk_verdict_action(GkVerdict v) {
    switch(v) {
    case GkVerdictOrdinary:
        return "Nothing here argues against it";
    case GkVerdictCheck:
        return "Read the domain before you tap";
    case GkVerdictCaution:
        return "Do not sign in or pay here";
    case GkVerdictSuspicious:
        return "Type the address yourself instead";
    case GkVerdictDanger:
        return "Walk away. Report the tag.";
    case GkVerdictNothing:
    default:
        return "There is no link on this tag";
    }
}

/* The bands. Deliberately not evenly spaced: the distance between C and D is
 * the distance between "be careful" and "something is wrong here", and it
 * should take real evidence to cross it. */
uint8_t gk_grade_floor(GkGrade g) {
    switch(g) {
    case GkGradeAPlus:
        return 95;
    case GkGradeA:
        return 85;
    case GkGradeB:
        return 72;
    case GkGradeC:
        return 58;
    case GkGradeD:
        return 40;
    case GkGradeF:
    case GkGradeNone:
    default:
        return 0;
    }
}

static GkGrade gk_grade_of(uint8_t score) {
    if(score >= gk_grade_floor(GkGradeAPlus)) return GkGradeAPlus;
    if(score >= gk_grade_floor(GkGradeA)) return GkGradeA;
    if(score >= gk_grade_floor(GkGradeB)) return GkGradeB;
    if(score >= gk_grade_floor(GkGradeC)) return GkGradeC;
    if(score >= gk_grade_floor(GkGradeD)) return GkGradeD;
    return GkGradeF;
}

static GkVerdict gk_verdict_of(GkGrade g) {
    switch(g) {
    case GkGradeAPlus:
    case GkGradeA:
        return GkVerdictOrdinary;
    case GkGradeB:
        return GkVerdictCheck;
    case GkGradeC:
        return GkVerdictCaution;
    case GkGradeD:
        return GkVerdictSuspicious;
    case GkGradeF:
        return GkVerdictDanger;
    case GkGradeNone:
    default:
        return GkVerdictNothing;
    }
}

/* ------------------------------------------------------------- the lists */

/* Names worth impersonating. A brand earns its place here by being something
 * a stranger might plausibly be sent to in the street -- banks, deliveries,
 * platforms and wallets -- not by being large. */
static const char* const gk_brands[] = {
    "paypal",     "apple",      "icloud",    "appleid",   "google",    "gmail",
    "microsoft",  "outlook",    "office",    "onedrive",  "amazon",    "netflix",
    "facebook",   "instagram",  "whatsapp",  "linkedin",  "telegram",  "discord",
    "spotify",    "steam",      "roblox",    "twitch",    "tiktok",    "snapchat",
    "dhl",        "fedex",      "usps",      "ups",       "royalmail", "evri",
    "dpd",        "hermes",     "bluedart",  "delhivery", "chase",     "hsbc",
    "barclays",   "natwest",    "lloyds",    "santander", "halifax",   "monzo",
    "revolut",    "starling",   "wise",      "wellsfargo", "citibank", "coinbase",
    "binance",    "metamask",   "ledger",    "trezor",    "blockchain", "kraken",
    "ebay",       "alibaba",    "aliexpress", "shopify",  "etsy",      "walmart",
    "target",     "costco",     "uber",      "airbnb",    "booking",   "expedia",
    "ryanair",    "easyjet",    "lufthansa", "emirates",  "flipkart",  "myntra",
    "paytm",      "phonepe",    "razorpay",  "irctc",     "aadhaar",   "uidai",
    "sbi",        "hdfc",       "icici",     "axisbank",  "kotak",     "bhim",
    "swiggy",     "zomato",     "zerodha",   "groww",     "jio",       "airtel",
    "vodafone",   "verizon",    "tmobile",   "dropbox",   "adobe",     "zoom",
    "docusign",   "intuit",     "turbotax",  "hmrc",      "dvla",      "irs",
    "medicare",   "nhs",
};
#define GK_BRAND_COUNT (sizeof(gk_brands) / sizeof(gk_brands[0]))

/* Domains that legitimately contain a brand name plus more. Without these,
 * amazonaws.com reads as an impersonation of amazon. */
static const char* const gk_brand_exempt[] = {
    "amazonaws",     "googleapis",   "googleusercontent", "googlesource",
    "googletagmanager", "googlemail", "microsoftonline",   "microsoftstore",
    "paypalobjects", "appleiphonecell", "office365",       "outlookmobile",
    "netflixinvestor", "uberinternal", "applemusic",       "icloudservice",
};
#define GK_BRAND_EXEMPT_COUNT (sizeof(gk_brand_exempt) / sizeof(gk_brand_exempt[0]))

static const char* const gk_shorteners[] = {
    "bit.ly",       "t.co",        "tinyurl.com", "goo.gl",      "ow.ly",
    "is.gd",        "buff.ly",     "adf.ly",      "bit.do",      "cutt.ly",
    "rb.gy",        "shorturl.at", "rebrand.ly",  "t.ly",        "s.id",
    "v.gd",         "tiny.cc",     "lnkd.in",     "db.tt",       "qr.ae",
    "po.st",        "bc.vc",       "u.to",        "x.co",        "mcaf.ee",
    "clck.ru",      "chilp.it",    "ity.im",      "q.gs",        "urlz.fr",
    "tny.im",       "cli.gs",      "shorte.st",   "gg.gg",       "tr.im",
    "soo.gd",       "short.io",    "qrco.de",     "qrs.ly",      "linktr.ee",
    "snip.ly",      "shrtco.de",   "1link.in",    "wa.link",
};
#define GK_SHORTENER_COUNT (sizeof(gk_shorteners) / sizeof(gk_shorteners[0]))

/* Endings where abuse concentrates. Honest sites live on all of them; the
 * signal is worth 14 points precisely because it is weak on its own. */
static const char* const gk_risky_tlds[] = {
    "tk",     "ml",       "ga",      "cf",      "gq",     "top",    "xyz",
    "click",  "link",     "rest",    "country", "gdn",    "work",   "loan",
    "download", "review", "party",   "science", "stream", "racing", "win",
    "bid",    "date",     "faith",   "cricket", "accountant", "men", "trade",
    "webcam", "mom",      "buzz",    "icu",     "cyou",   "sbs",    "quest",
    "monster", "autos",   "boats",   "cfd",     "beauty", "hair",   "skin",
    "makeup", "lol",      "cam",     "uno",     "kim",    "fit",    "run",
};
#define GK_RISKY_TLD_COUNT (sizeof(gk_risky_tlds) / sizeof(gk_risky_tlds[0]))

static const char* const gk_cred_words[] = {
    "login",    "signin",   "log-in",     "sign-in",  "verify",   "verification",
    "authenticate", "auth", "account",    "myaccount", "secure",  "security",
    "update",   "confirm",  "validate",   "unlock",   "suspended", "restricted",
    "recover",  "recovery", "reset",      "password", "passwd",   "credential",
    "otp",      "2fa",      "mfa",        "kyc",      "netbanking", "webscr",
};
#define GK_CRED_WORD_COUNT (sizeof(gk_cred_words) / sizeof(gk_cred_words[0]))

static const char* const gk_crypto_words[] = {
    "wallet",   "seedphrase", "seed-phrase", "mnemonic", "privatekey", "private-key",
    "airdrop",  "connectwallet", "metamask", "walletconnect", "restore-wallet",
    "claimtokens", "presale", "staking",  "defi",     "nftclaim", "recoveryphrase",
};
#define GK_CRYPTO_WORD_COUNT (sizeof(gk_crypto_words) / sizeof(gk_crypto_words[0]))

/* Deliberately zero points. A parking meter that says "parking" is doing its
 * job; the note exists to raise the reader's attention, not the score. */
static const char* const gk_money_words[] = {
    "payment", "billing", "invoice", "refund",  "parking", "penalty",
    "toll",    "fastag",  "checkout", "donate", "recharge", "topup",
    "fine",    "upi",     "paynow",  "settle",
};
#define GK_MONEY_WORD_COUNT (sizeof(gk_money_words) / sizeof(gk_money_words[0]))

static const char* const gk_download_ext[] = {
    ".apk", ".exe", ".msi", ".dmg", ".pkg", ".scr", ".bat",
    ".cmd", ".ps1", ".jar", ".vbs", ".iso", ".img", ".deb",
};
#define GK_DOWNLOAD_EXT_COUNT (sizeof(gk_download_ext) / sizeof(gk_download_ext[0]))

static const char* const gk_redirect_params[] = {
    "url",     "redirect", "redirect_uri", "redir",  "next",  "target",
    "dest",    "destination", "continue",  "return", "returnurl", "goto",
    "out",     "link",     "forward",      "r",      "u",     "to",
};
#define GK_REDIRECT_PARAM_COUNT (sizeof(gk_redirect_params) / sizeof(gk_redirect_params[0]))

/* ----------------------------------------------------------- small utils */

static char gk_low(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static void gk_cat(char* dst, size_t cap, const char* s) {
    if(!dst || cap == 0 || !s) return;
    size_t o = strlen(dst);
    while(*s && o + 1 < cap) dst[o++] = *s++;
    dst[o] = '\0';
}

static void gk_cat_n(char* dst, size_t cap, const char* s, size_t n) {
    if(!dst || cap == 0 || !s) return;
    size_t o = strlen(dst);
    for(size_t i = 0; i < n && o + 1 < cap; i++) dst[o++] = s[i];
    dst[o] = '\0';
}

/* Case-insensitive substring search over a bounded buffer. */
static bool gk_find_ci(const char* hay, size_t hlen, const char* needle) {
    size_t n = strlen(needle);
    if(n == 0 || n > hlen) return false;
    for(size_t i = 0; i + n <= hlen; i++) {
        size_t j = 0;
        while(j < n && gk_low(hay[i + j]) == gk_low(needle[j])) j++;
        if(j == n) return true;
    }
    return false;
}

/* Same, but the match must start and end on a label or word boundary, so
 * "ups" does not match "backups". */
static bool gk_find_word(const char* hay, size_t hlen, const char* needle) {
    size_t n = strlen(needle);
    if(n == 0 || n > hlen) return false;
    for(size_t i = 0; i + n <= hlen; i++) {
        size_t j = 0;
        while(j < n && gk_low(hay[i + j]) == gk_low(needle[j])) j++;
        if(j != n) continue;
        char before = (i == 0) ? '\0' : hay[i - 1];
        char after = (i + n >= hlen) ? '\0' : hay[i + n];
        bool lb = (before == '\0') || !((before >= 'a' && before <= 'z') ||
                                        (before >= 'A' && before <= 'Z') ||
                                        (before >= '0' && before <= '9'));
        bool rb = (after == '\0') || !((after >= 'a' && after <= 'z') ||
                                       (after >= 'A' && after <= 'Z') ||
                                       (after >= '0' && after <= '9'));
        if(lb && rb) return true;
    }
    return false;
}

static bool gk_starts_with_ci(const char* hay, size_t hlen, const char* head) {
    size_t n = strlen(head);
    if(n > hlen) return false;
    for(size_t i = 0; i < n; i++) {
        if(gk_low(hay[i]) != gk_low(head[i])) return false;
    }
    return true;
}

static bool gk_ends_with_ci(const char* hay, size_t hlen, const char* tail) {
    size_t n = strlen(tail);
    if(n > hlen) return false;
    for(size_t i = 0; i < n; i++) {
        if(gk_low(hay[hlen - n + i]) != gk_low(tail[i])) return false;
    }
    return true;
}

/* ------------------------------------------------------------- findings */

typedef struct {
    GkVerdictResult* out;
    const GkUrl* u;
    /* Scratch, filled once per grade so the passes can share it. */
    char host[96];
    size_t host_len;
    char core[80];
    size_t core_len;
    char core_fold[80];
    size_t core_fold_len;
    char path_query[GK_URL_MAX];
    size_t pq_len;
    bool brand_hit; /* some brand signal already fired */
} GkCtx;

static void gk_add(GkCtx* c, GkSignalId id, const char* evidence) {
    GkVerdictResult* out = c->out;
    /* A signal never fires twice: the second observation of the same fact is
     * not new evidence, it is the same fact seen from another angle. */
    for(uint8_t i = 0; i < out->find_count; i++) {
        if(out->find[i].id == id) return;
    }
    out->find_total++;
    if(out->find_count >= GK_FIND_MAX) return;

    GkFinding* f = &out->find[out->find_count++];
    memset(f, 0, sizeof(*f));
    f->id = id;
    f->points = gk_signals[id].points;
    if(evidence) gk_cat(f->evidence, sizeof(f->evidence), evidence);
}

/* ------------------------------------------------------------ the passes */

static void gk_pass_scheme(GkCtx* c) {
    const GkUrl* u = c->u;
    switch(u->scheme) {
    case GkSchemeJavascript:
    case GkSchemeData:
    case GkSchemeFile:
    case GkSchemeIntent:
        gk_add(c, GkSigDangerousScheme, gk_scheme_name(u->scheme));
        break;
    case GkSchemeTel:
    case GkSchemeSms:
    case GkSchemeMailto:
    case GkSchemeGeo:
    case GkSchemeMarket: {
        char ev[GK_EVIDENCE_MAX] = {0};
        gk_cat(ev, sizeof(ev), gk_scheme_name(u->scheme));
        gk_cat(ev, sizeof(ev), ":");
        gk_cat_n(ev, sizeof(ev), u->raw + u->path.off, u->path.len);
        gk_add(c, GkSigNonWebAction, ev);
        break;
    }
    case GkSchemeHttp:
        gk_add(c, GkSigPlainHttp, NULL);
        break;
    case GkSchemeHttps:
        gk_add(c, GkSigHttps, NULL);
        break;
    case GkSchemeNone:
        gk_add(c, GkSigNoScheme, NULL);
        break;
    default:
        break;
    }

    if(u->has_port && u->port != 80 && u->port != 443) {
        char ev[GK_EVIDENCE_MAX] = {0};
        char num[8] = {0};
        uint16_t p = u->port;
        int i = 0;
        if(p == 0) num[i++] = '0';
        while(p && i < 6) {
            num[i++] = (char)('0' + (p % 10));
            p = (uint16_t)(p / 10);
        }
        gk_cat(ev, sizeof(ev), "port ");
        for(int k = i - 1; k >= 0; k--) gk_cat_n(ev, sizeof(ev), &num[k], 1);
        gk_add(c, GkSigNonStdPort, ev);
    }
}

static void gk_pass_host_shape(GkCtx* c) {
    const GkUrl* u = c->u;

    if(u->has_userinfo) {
        char ev[GK_EVIDENCE_MAX] = {0};
        gk_cat(ev, sizeof(ev), "goes to ");
        gk_cat_n(ev, sizeof(ev), u->raw + u->registrable.off, u->registrable.len);
        gk_add(c, GkSigUserinfo, ev);
    }

    if(u->host_form == GkHostIpv4 || u->host_form == GkHostIpv6) {
        gk_add(c, GkSigIpHost, c->host);
    }
    if(u->host_has_punycode) gk_add(c, GkSigPunycode, c->host);
    if(u->host_has_escape) gk_add(c, GkSigEncodedAuthority, NULL);
    if(u->host_trailing_dot) gk_add(c, GkSigTrailingDot, NULL);

    /* Labels stacked in front of the registrable domain. Two is ordinary
     * (www.shop.example.com); four is somebody making the real domain scroll
     * off the edge of a notification. */
    if(u->registrable.len && u->host.len > u->registrable.len) {
        uint8_t sub_labels = 1;
        for(uint16_t i = u->host.off; i < u->registrable.off; i++) {
            if(u->raw[i] == '.') sub_labels++;
        }
        sub_labels--; /* the trailing separator is not a label */
        if(sub_labels >= 3) gk_add(c, GkSigDeepSubdomain, NULL);
    }

    if(u->suffix_is_hosting) {
        char ev[GK_EVIDENCE_MAX] = {0};
        gk_cat_n(ev, sizeof(ev), u->raw + u->suffix.off, u->suffix.len);
        gk_add(c, GkSigFreeHosting, ev);
    }

    /* Shorteners are matched on the whole registrable domain: bit.ly is the
     * domain, not a label inside one. */
    for(size_t i = 0; i < GK_SHORTENER_COUNT; i++) {
        if(gk_span_eq(u->raw, u->registrable, gk_shorteners[i])) {
            gk_add(c, GkSigShortener, gk_shorteners[i]);
            break;
        }
    }

    /* The TLD is the last label of the suffix. */
    if(u->host_form == GkHostName && u->suffix.len) {
        const char* sfx = u->raw + u->suffix.off;
        uint16_t slen = u->suffix.len;
        uint16_t last = 0;
        for(uint16_t i = 0; i < slen; i++) {
            if(sfx[i] == '.') last = (uint16_t)(i + 1);
        }
        const char* tld = sfx + last;
        size_t tld_len = (size_t)(slen - last);

        bool filename_tld = (tld_len == 3) && (gk_find_word(tld, tld_len, "zip") ||
                                               gk_find_word(tld, tld_len, "mov"));
        if(filename_tld) {
            char ev[GK_EVIDENCE_MAX] = {0};
            gk_cat(ev, sizeof(ev), ".");
            gk_cat_n(ev, sizeof(ev), tld, tld_len);
            gk_add(c, GkSigFilenameTld, ev);
        } else {
            for(size_t i = 0; i < GK_RISKY_TLD_COUNT; i++) {
                if(strlen(gk_risky_tlds[i]) == tld_len &&
                   gk_find_word(tld, tld_len, gk_risky_tlds[i])) {
                    char ev[GK_EVIDENCE_MAX] = {0};
                    gk_cat(ev, sizeof(ev), ".");
                    gk_cat(ev, sizeof(ev), gk_risky_tlds[i]);
                    gk_add(c, GkSigRiskyTld, ev);
                    break;
                }
            }
        }
    }

    for(uint16_t i = 0; i < u->raw_len; i++) {
        if((unsigned char)u->raw[i] >= 0x80) {
            gk_add(c, GkSigNonAscii, NULL);
            break;
        }
    }
}

/* Is this TLD or suffix one a bank would never be found on? */
static bool gk_suffix_is_cheap(const GkCtx* c) {
    const GkUrl* u = c->u;
    if(u->suffix_is_hosting) return true;
    for(uint8_t i = 0; i < c->out->find_count; i++) {
        GkSignalId id = c->out->find[i].id;
        if(id == GkSigRiskyTld || id == GkSigFilenameTld) return true;
    }
    return false;
}

static void gk_pass_brands(GkCtx* c) {
    const GkUrl* u = c->u;
    if(u->host_form != GkHostName || u->host.len == 0) return;

    char brand_fold[64];
    char reg[80];
    gk_span_copy(u->raw, u->registrable, reg, sizeof(reg));

    /* Pass 1: a famous domain sitting inside a hostname that is not it.
     * apple.com.id-check.top, paypal.com.secure.xyz. This runs first because
     * it also decides whether the "recognised domain" note is allowed.
     *
     * The test is positional, and it has to be. "amazon.co" appears inside
     * amazon.co.uk too -- the difference is not the text, it is that in the
     * real one the match is part of the registrable domain rather than
     * stranded in front of it. So a match only counts when it ends before
     * the registrable domain begins: entirely inside the subdomain, which is
     * the half the attacker writes for free. */
    size_t reg_rel = (size_t)(u->registrable.off - u->host.off);
    static const char* const common_sfx[] = {"com", "net", "org", "co", "io", "co.uk", "in"};
    for(size_t b = 0; b < GK_BRAND_COUNT && !c->brand_hit; b++) {
        for(size_t s = 0; s < sizeof(common_sfx) / sizeof(common_sfx[0]); s++) {
            char cand[48] = {0};
            gk_cat(cand, sizeof(cand), gk_brands[b]);
            gk_cat(cand, sizeof(cand), ".");
            gk_cat(cand, sizeof(cand), common_sfx[s]);
            size_t clen = strlen(cand);
            if(clen > c->host_len) continue;

            for(size_t i = 0; i + clen <= c->host_len; i++) {
                if(i > 0 && c->host[i - 1] != '.') continue;
                if(i + clen > reg_rel) break; /* it is part of the real domain */
                size_t j = 0;
                while(j < clen && gk_low(c->host[i + j]) == cand[j]) j++;
                if(j != clen) continue;
                /* It must be followed by another label, or it is the domain. */
                if(i + clen >= c->host_len || c->host[i + clen] != '.') continue;

                gk_add(c, GkSigDomainInside, cand);
                c->brand_hit = true;
                break;
            }
            if(c->brand_hit) break;
        }
    }

    for(size_t i = 0; i < GK_BRAND_EXEMPT_COUNT; i++) {
        if(c->core_len == strlen(gk_brand_exempt[i]) &&
           gk_find_word(c->core, c->core_len, gk_brand_exempt[i])) {
            return; /* amazonaws is not amazon wearing a hat */
        }
    }

    for(size_t b = 0; b < GK_BRAND_COUNT; b++) {
        const char* brand = gk_brands[b];
        size_t blen = strlen(brand);
        size_t fold_len = gk_confusable_fold(brand, blen, brand_fold, sizeof(brand_fold));

        /* --- exact: this really is the brand's own domain --- */
        bool exact = (c->core_len == blen) && gk_find_word(c->core, c->core_len, brand);
        if(exact) {
            if(gk_suffix_is_cheap(c)) {
                char ev[GK_EVIDENCE_MAX] = {0};
                gk_cat(ev, sizeof(ev), reg);
                gk_add(c, GkSigBrandBadTld, ev);
                c->brand_hit = true;
            } else if(!c->brand_hit) {
                gk_add(c, GkSigKnownBrand, reg);
            }
            return;
        }

        if(c->brand_hit) continue;

        /* --- folded: spelled to read as the brand --- */
        bool looks_like = (c->core_fold_len == fold_len) &&
                          (memcmp(c->core_fold, brand_fold, fold_len) == 0);
        if(!looks_like && c->core_fold_len == fold_len) {
            looks_like = gk_is_transposition(c->core_fold, c->core_fold_len, brand_fold, fold_len);
        }
        if(looks_like) {
            char ev[GK_EVIDENCE_MAX] = {0};
            gk_cat(ev, sizeof(ev), "reads as ");
            gk_cat(ev, sizeof(ev), brand);
            gk_add(c, GkSigHomoglyph, ev);
            c->brand_hit = true;
            continue;
        }

        /* --- the brand as a whole label in front of the real domain --- */
        if(u->registrable.off > u->host.off) {
            size_t sub_len = (size_t)(u->registrable.off - u->host.off);
            if(gk_find_word(c->host, sub_len, brand)) {
                char ev[GK_EVIDENCE_MAX] = {0};
                gk_cat(ev, sizeof(ev), brand);
                gk_cat(ev, sizeof(ev), " -> ");
                gk_cat(ev, sizeof(ev), reg);
                gk_add(c, GkSigBrandSubdomain, ev);
                c->brand_hit = true;
                continue;
            }
        }

        /* --- the brand glued into the domain itself --- */
        if(blen >= 5 && c->core_fold_len > fold_len &&
           gk_find_ci(c->core_fold, c->core_fold_len, brand_fold)) {
            char ev[GK_EVIDENCE_MAX] = {0};
            gk_cat(ev, sizeof(ev), brand);
            gk_cat(ev, sizeof(ev), " in ");
            gk_cat(ev, sizeof(ev), reg);
            gk_add(c, GkSigBrandCore, ev);
            c->brand_hit = true;
            continue;
        }

        /* --- the brand only after the slash, where it is free --- */
        if(blen >= 5 && c->pq_len && gk_find_word(c->path_query, c->pq_len, brand)) {
            char ev[GK_EVIDENCE_MAX] = {0};
            gk_cat(ev, sizeof(ev), "/");
            gk_cat(ev, sizeof(ev), brand);
            gk_cat(ev, sizeof(ev), " on ");
            gk_cat(ev, sizeof(ev), reg);
            gk_add(c, GkSigBrandPath, ev);
            /* Not a brand_hit: a brand in the path is weak, and something
             * stronger may still be waiting in a later brand. */
        }
    }
}

static void gk_pass_path(GkCtx* c) {
    const GkUrl* u = c->u;

    /* Past about a hundred characters no phone shows the whole thing, which
     * is the point at which length stops being untidy and starts being
     * useful to somebody. */
    if(u->raw_len > 100) gk_add(c, GkSigVeryLong, NULL);

    if(u->query.len) {
        uint8_t amps = 1;
        for(uint16_t i = 0; i < u->query.len; i++) {
            if(u->raw[u->query.off + i] == '&') amps++;
        }
        if(amps >= 6) gk_add(c, GkSigManyParams, NULL);
    }

    /* Percent-decode the path and query once and look at the result: a
     * redirect hidden as %68%74%74%70%73 is the same finding as one written
     * in plain text, and only one of the two survives a naive search. */
    char decoded[GK_URL_MAX];
    bool doubled = false;
    size_t dlen =
        gk_percent_decode(c->path_query, c->pq_len, decoded, sizeof(decoded), NULL, &doubled);
    if(doubled) gk_add(c, GkSigDoubleEncoded, NULL);

    /* A second absolute address carried inside a parameter. */
    if(u->query.len) {
        for(size_t p = 0; p < GK_REDIRECT_PARAM_COUNT; p++) {
            char key[24] = {0};
            gk_cat(key, sizeof(key), gk_redirect_params[p]);
            gk_cat(key, sizeof(key), "=");
            size_t klen = strlen(key);

            for(size_t i = 0; i + klen <= dlen; i++) {
                if(i > 0 && decoded[i - 1] != '?' && decoded[i - 1] != '&') continue;
                size_t j = 0;
                while(j < klen && gk_low(decoded[i + j]) == key[j]) j++;
                if(j != klen) continue;

                const char* val = decoded + i + klen;
                size_t vlen = dlen - i - klen;
                /* "Absolute" means it replaces the host, not just the path --
                 * http://, https://, or a protocol-relative //host. */
                bool absolute = gk_starts_with_ci(val, vlen, "http://") ||
                                gk_starts_with_ci(val, vlen, "https://") ||
                                (vlen > 2 && val[0] == '/' && val[1] == '/');
                if(absolute) {
                    char ev[GK_EVIDENCE_MAX] = {0};
                    gk_cat(ev, sizeof(ev), gk_redirect_params[p]);
                    gk_cat(ev, sizeof(ev), "=");
                    gk_cat_n(ev, sizeof(ev), val, vlen);
                    gk_add(c, GkSigOpenRedirect, ev);
                }
                break;
            }
        }
    }

    /* Downloads are judged on the path only: a .apk in a query string is a
     * filename being passed around, not the thing being fetched. */
    if(u->path.len) {
        size_t plen = u->path.len;
        const char* path = u->raw + u->path.off;
        /* Ignore a trailing slash so /file.apk/ still counts. */
        while(plen > 0 && path[plen - 1] == '/') plen--;
        for(size_t i = 0; i < GK_DOWNLOAD_EXT_COUNT; i++) {
            if(gk_ends_with_ci(path, plen, gk_download_ext[i])) {
                gk_add(c, GkSigDirectDownload, gk_download_ext[i]);
                break;
            }
        }
    }

    /* Words. The whole host and path are searched, because "secure-login" in
     * the domain and "/secure/login" in the path are the same claim. */
    char haystack[GK_URL_MAX + 96];
    haystack[0] = '\0';
    gk_cat_n(haystack, sizeof(haystack), c->host, c->host_len);
    gk_cat(haystack, sizeof(haystack), "/");
    gk_cat_n(haystack, sizeof(haystack), decoded, dlen);
    size_t hlen = strlen(haystack);

    for(size_t i = 0; i < GK_CRYPTO_WORD_COUNT; i++) {
        if(gk_find_word(haystack, hlen, gk_crypto_words[i])) {
            gk_add(c, GkSigCryptoKeywords, gk_crypto_words[i]);
            break;
        }
    }
    for(size_t i = 0; i < GK_CRED_WORD_COUNT; i++) {
        if(gk_find_word(haystack, hlen, gk_cred_words[i])) {
            gk_add(c, GkSigCredKeywords, gk_cred_words[i]);
            break;
        }
    }
    for(size_t i = 0; i < GK_MONEY_WORD_COUNT; i++) {
        if(gk_find_word(haystack, hlen, gk_money_words[i])) {
            gk_add(c, GkSigMoneyContext, gk_money_words[i]);
            break;
        }
    }
}

static void gk_pass_tag(GkCtx* c, const GkTag* tag) {
    if(!tag) return;

    if(tag->write_known) {
        if(tag->writable) {
            gk_add(c, GkSigRewritable, NULL);
        } else {
            gk_add(c, GkSigLocked, NULL);
        }
    }
    if(tag->pwd_protected) gk_add(c, GkSigPwdProtected, NULL);
    if(tag->malformed) gk_add(c, GkSigMalformed, NULL);
    if(tag->url_truncated) gk_add(c, GkSigUrlTruncated, NULL);
    if(tag->has_aar) gk_add(c, GkSigAar, tag->aar);

    /* One URL, optionally with a caption, is the shape of an honest poster
     * tag. The Smart Poster wrapper itself does not count as an extra. */
    uint8_t interesting = 0;
    for(uint8_t i = 0; i < tag->record_count; i++) {
        GkRecKind k = tag->rec[i].kind;
        if(k == GkRecUri || k == GkRecText || k == GkRecSmartPoster) continue;
        interesting++;
    }
    if(interesting > (tag->has_aar ? 1 : 0)) gk_add(c, GkSigExtraRecords, NULL);

    /* The tag's own serial, written into the link as hex by the tag itself. */
    if(tag->uid_len >= 4 && c->pq_len >= tag->uid_len * 2) {
        static const char* hexd = "0123456789ABCDEF";
        char uid_hex[GK_UID_MAX * 2 + 1] = {0};
        for(uint8_t i = 0; i < tag->uid_len && i < GK_UID_MAX; i++) {
            char pair[3] = {hexd[tag->uid[i] >> 4], hexd[tag->uid[i] & 0x0F], '\0'};
            gk_cat(uid_hex, sizeof(uid_hex), pair);
        }
        if(gk_find_ci(c->path_query, c->pq_len, uid_hex)) {
            gk_add(c, GkSigUidMirror, NULL);
        }
    }

    /* A caption naming somebody the link does not go to. */
    if(tag->has_title && c->core_len) {
        size_t tlen = strlen(tag->title);
        for(size_t b = 0; b < GK_BRAND_COUNT; b++) {
            if(!gk_find_word(tag->title, tlen, gk_brands[b])) continue;
            /* The caption is only vindicated by the destination being that
             * brand's own domain -- an exact core. "HDFC Bank" over a link to
             * hdfc-verify.icu is the mismatch, not the match: containing the
             * name is what the impersonation does. */
            bool destination_is_theirs =
                (c->core_len == strlen(gk_brands[b])) &&
                gk_find_word(c->core, c->core_len, gk_brands[b]);
            if(destination_is_theirs) break;
            char ev[GK_EVIDENCE_MAX] = {0};
            gk_cat(ev, sizeof(ev), "says ");
            gk_cat(ev, sizeof(ev), gk_brands[b]);
            gk_add(c, GkSigTitleMismatch, ev);
            break;
        }
    }
}

/* ------------------------------------------------------------ the answer */

static void gk_finish(GkCtx* c) {
    GkVerdictResult* out = c->out;

    int32_t raw = 100;
    uint32_t fam_points[5] = {0};
    for(uint8_t i = 0; i < out->find_count; i++) {
        const GkSignalInfo* info = &gk_signals[out->find[i].id];
        raw -= (int32_t)info->points;
        if(info->family < 5) fam_points[info->family] += info->points;
    }
    if(raw < 0) raw = 0;
    out->raw_score = (uint8_t)raw;

    uint8_t worst = GkFamNote;
    uint32_t worst_points = 0;
    for(uint8_t f = 0; f < 4; f++) {
        if(fam_points[f] > worst_points) {
            worst_points = fam_points[f];
            worst = f;
        }
    }
    out->worst_family = worst;

    /* Ceilings. GkCapNoSite is always in play; the rest arrive with their
     * signals. Only the ones that actually bind get listed, because a
     * ceiling above the score did not do anything and saying it did would be
     * padding the explanation. */
    uint8_t score = out->raw_score;
    bool active[GkCapCount];
    memset(active, 0, sizeof(active));
    active[GkCapNoSite] = true;
    for(uint8_t i = 0; i < out->find_count; i++) {
        GkCapId cap = gk_signals[out->find[i].id].cap;
        if(cap < GkCapCount) active[cap] = true;
    }

    for(uint8_t cap = 0; cap < GkCapCount; cap++) {
        if(!active[cap]) continue;
        uint8_t v = gk_caps[cap].value;
        if(v < score) score = v;
    }

    out->score = score;
    out->cap_count = 0;
    for(uint8_t cap = 0; cap < GkCapCount; cap++) {
        if(!active[cap]) continue;
        if(gk_caps[cap].value > out->raw_score) continue; /* it never bit */
        if(out->cap_count < GK_CAP_MAX) out->caps[out->cap_count++] = (GkCapId)cap;
    }

    out->grade = gk_grade_of(out->score);
    out->verdict = gk_verdict_of(out->grade);

    /* Findings are shown worst-first: the reason for the grade should be the
     * first thing under it, and a zero-point note should never be. */
    for(uint8_t i = 0; i + 1 < out->find_count; i++) {
        for(uint8_t j = 0; j + 1 < out->find_count - i; j++) {
            if(out->find[j].points < out->find[j + 1].points) {
                GkFinding t = out->find[j];
                out->find[j] = out->find[j + 1];
                out->find[j + 1] = t;
            }
        }
    }
}

static void gk_grade_common(const GkTag* tag, const char* url, GkVerdictResult* out) {
    memset(out, 0, sizeof(*out));
    out->status = GkTagOk;
    out->grade = GkGradeNone;
    out->verdict = GkVerdictNothing;
    out->raw_score = 100;
    out->score = 0;

    if(!url || url[0] == '\0') {
        out->status = tag ? tag->status : GkTagNoUrl;
        if(out->status == GkTagOk) out->status = GkTagNoUrl;
        return;
    }
    out->have_url = true;

    GkUrl u;
    if(!gk_url_parse(url, 0, &u)) {
        out->status = GkTagUnreadable;
        return;
    }

    GkCtx c;
    memset(&c, 0, sizeof(c));
    c.out = out;
    c.u = &u;

    gk_span_copy(u.raw, u.host, c.host, sizeof(c.host));
    c.host_len = strlen(c.host);
    gk_span_copy(u.raw, u.core, c.core, sizeof(c.core));
    c.core_len = strlen(c.core);
    c.core_fold_len = gk_confusable_fold(c.core, c.core_len, c.core_fold, sizeof(c.core_fold));

    /* Path and query as one string: every word search wants both, and the
     * boundary between them is not meaningful to any of them. */
    c.path_query[0] = '\0';
    gk_cat_n(c.path_query, sizeof(c.path_query), u.raw + u.path.off, u.path.len);
    if(u.query.len) {
        gk_cat(c.path_query, sizeof(c.path_query), "?");
        gk_cat_n(c.path_query, sizeof(c.path_query), u.raw + u.query.off, u.query.len);
    }
    if(u.fragment.len) {
        gk_cat(c.path_query, sizeof(c.path_query), "#");
        gk_cat_n(c.path_query, sizeof(c.path_query), u.raw + u.fragment.off, u.fragment.len);
    }
    c.pq_len = strlen(c.path_query);

    gk_cat(out->host, sizeof(out->host), c.host);
    gk_span_copy(u.raw, u.registrable, out->registrable, sizeof(out->registrable));

    gk_pass_scheme(&c);
    if(gk_scheme_is_web(u.scheme) || u.scheme == GkSchemeNone) {
        gk_pass_host_shape(&c);
        gk_pass_brands(&c);
    }
    gk_pass_path(&c);
    gk_pass_tag(&c, tag);
    gk_finish(&c);
}

void gk_verdict_grade(const GkTag* tag, GkVerdictResult* out) {
    if(!out) return;
    if(!tag) {
        memset(out, 0, sizeof(*out));
        out->status = GkTagNoNdef;
        return;
    }
    gk_grade_common(tag, tag->has_url ? tag->url : NULL, out);
    if(!tag->has_url) out->status = tag->status;
}

void gk_verdict_grade_url(const char* url, GkVerdictResult* out) {
    if(!out) return;
    gk_grade_common(NULL, url, out);
}
