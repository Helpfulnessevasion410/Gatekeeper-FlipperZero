#include "gk_demo.h"

#include <string.h>

/* ------------------------------------------------------------- assembly */

typedef struct {
    uint8_t buf[GK_NDEF_MAX];
    uint16_t len;
} GkBuf;

static void gb_u8(GkBuf* b, uint8_t v) {
    if(b->len < sizeof(b->buf)) b->buf[b->len++] = v;
}

static void gb_mem(GkBuf* b, const void* src, uint16_t n) {
    const uint8_t* p = src;
    for(uint16_t i = 0; i < n; i++) gb_u8(b, p[i]);
}

static void gb_str(GkBuf* b, const char* s) {
    gb_mem(b, s, (uint16_t)strlen(s));
}

/* A short URI record: header, type length, payload length, "U", then the
 * one-byte prefix code and the rest of the address. This is exactly the
 * layout an NFC writing app produces. */
static void gb_uri(GkBuf* b, uint8_t flags, uint8_t prefix, const char* rest) {
    uint16_t rl = (uint16_t)strlen(rest);
    gb_u8(b, (uint8_t)(flags | 0x11)); /* SR | TNF=1 (well known) */
    gb_u8(b, 1); /* type length */
    gb_u8(b, (uint8_t)(rl + 1)); /* payload length */
    gb_u8(b, 'U');
    gb_u8(b, prefix);
    gb_str(b, rest);
}

static void gb_text(GkBuf* b, uint8_t flags, const char* text) {
    uint16_t tl = (uint16_t)strlen(text);
    gb_u8(b, (uint8_t)(flags | 0x11));
    gb_u8(b, 1);
    gb_u8(b, (uint8_t)(tl + 3)); /* status byte + "en" + text */
    gb_u8(b, 'T');
    gb_u8(b, 0x02); /* UTF-8, two-character language code */
    gb_str(b, "en");
    gb_str(b, text);
}

/* An Android Application Record: external type, "android.com:pkg", payload
 * is the package name. Android acts on this before it acts on the URL. */
static void gb_aar(GkBuf* b, uint8_t flags, const char* pkg) {
    uint16_t pl = (uint16_t)strlen(pkg);
    gb_u8(b, (uint8_t)(flags | 0x14)); /* SR | TNF=4 (external) */
    gb_u8(b, 15);
    gb_u8(b, (uint8_t)pl);
    gb_str(b, "android.com:pkg");
    gb_str(b, pkg);
}

/* A Smart Poster wraps a whole message inside one record's payload: a title
 * and the link it belongs to, travelling together. Phones display the title. */
static void gb_smart_poster(GkBuf* b, uint8_t flags, const char* title, uint8_t prefix,
                            const char* rest) {
    GkBuf inner = {0};
    gb_text(&inner, 0x80, title); /* MB */
    gb_uri(&inner, 0x40, prefix, rest); /* ME */

    gb_u8(b, (uint8_t)(flags | 0x11));
    gb_u8(b, 2);
    gb_u8(b, (uint8_t)inner.len);
    gb_str(b, "Sp");
    gb_mem(b, inner.buf, inner.len);
}

/* Wrap a finished message in the Type 2 TLV the tag's memory actually holds. */
static void gb_wrap_tlv(const GkBuf* msg, GkBuf* out) {
    out->len = 0;
    gb_u8(out, 0x03); /* NDEF message TLV */
    if(msg->len < 0xFF) {
        gb_u8(out, (uint8_t)msg->len);
    } else {
        gb_u8(out, 0xFF);
        gb_u8(out, (uint8_t)(msg->len >> 8));
        gb_u8(out, (uint8_t)(msg->len & 0xFF));
    }
    gb_mem(out, msg->buf, msg->len);
    gb_u8(out, 0xFE); /* terminator */
}

/* ---------------------------------------------------------------- tags */

static const GkDemoInfo gk_demos[] = {
    {"Cafe menu", "Table tent, locked NTAG213"},
    {"Museum label", "Exhibit caption, Smart Poster"},
    {"Parking meter", "Council meter, tag left unlocked"},
    {"Meter, overlaid", "A sticker on top of the real one"},
    {"Poster, @ trick", "Concert flyer, bus shelter"},
    {"Flyer, shortened", "Handbill, free-coffee offer"},
    {"Bank look-alike", "Card left in a cash machine lobby"},
    {"Crypto claim", "Sticker on a lamp post"},
    {"App installer", "Games poster, student union"},
    {"Bank Smart Poster", "Caption and link disagree"},
    {"Premium-rate call", "Prize-draw card"},
    {"Blank tag", "Unwritten, straight from the roll"},
};
#define GK_DEMO_COUNT (sizeof(gk_demos) / sizeof(gk_demos[0]))

uint8_t gk_demo_count(void) {
    return (uint8_t)GK_DEMO_COUNT;
}

const GkDemoInfo* gk_demo_info(uint8_t index) {
    if(index >= GK_DEMO_COUNT) return &gk_demos[0];
    return &gk_demos[index];
}

/* Serials are made up but well-formed: NXP's manufacturer byte 0x04 first,
 * because a UID mirror only means anything if the UID looks real. */
static void gk_demo_uid(GkTag* tag, uint8_t seed) {
    static const uint8_t base[7] = {0x04, 0x9A, 0x2C, 0x11, 0x00, 0x00, 0x00};
    memcpy(tag->uid, base, sizeof(base));
    tag->uid[4] = (uint8_t)(0x40 + seed * 7);
    tag->uid[5] = (uint8_t)(0x80 + seed * 13);
    tag->uid[6] = (uint8_t)(0xC0 + seed * 3);
    tag->uid_len = 7;
}

bool gk_demo_build(uint8_t index, GkTag* tag) {
    if(!tag || index >= GK_DEMO_COUNT) return false;

    gk_tag_init(tag);
    tag->tech = GkTechType2;
    tag->write_known = true;
    tag->writable = true; /* the common case in the street; overridden below */
    tag->size_known = true;
    gk_demo_uid(tag, index);

    GkBuf msg = {0};

    switch(index) {
    case 0: /* an ordinary, well-run tag: locked, https, its own domain */
        memcpy(tag->tech_name, "NTAG213", 8);
        tag->capacity = 144;
        tag->writable = false;
        gb_uri(&msg, 0xC0, 0x04, "menu.thegoodpub.co.uk/");
        break;

    case 1: /* the good Smart Poster: caption and link agree */
        memcpy(tag->tech_name, "NTAG215", 8);
        tag->capacity = 504;
        tag->writable = false;
        gb_smart_poster(&msg, 0xC0, "Hall 3: Bronze Age hoard", 0x04,
                        "collection.citymuseum.org.uk/hoard");
        break;

    case 2: /* the real meter: fine, but anyone can rewrite it */
        memcpy(tag->tech_name, "NTAG215", 8);
        tag->capacity = 504;
        gb_uri(&msg, 0xC0, 0x04, "payments.westcity.gov.uk/bay/4471");
        break;

    case 3: /* the sticker over the top of it */
        memcpy(tag->tech_name, "NTAG216", 8);
        tag->capacity = 888;
        gb_smart_poster(&msg, 0xC0, "West City Council - pay for parking", 0x03,
                        "westcity-parking.pay-fine.top/bay/4471/secure-payment");
        break;

    case 4: /* everything before the @ is thrown away by the phone */
        memcpy(tag->tech_name, "NTAG213", 8);
        tag->capacity = 144;
        gb_uri(&msg, 0xC0, 0x04, "apple.com@id-verify.top/signin");
        break;

    case 5: /* the destination simply is not on the tag */
        memcpy(tag->tech_name, "NTAG213", 8);
        tag->capacity = 144;
        gb_smart_poster(&msg, 0xC0, "Free coffee - scan me!", 0x04, "bit.ly/3xKq9dA");
        break;

    case 6: /* a one instead of an l, and a hyphen doing the rest */
        memcpy(tag->tech_name, "NTAG213", 8);
        tag->capacity = 144;
        gb_uri(&msg, 0xC0, 0x04, "paypa1-secure.com/account/verify");
        break;

    case 7: /* the expensive one */
        memcpy(tag->tech_name, "NTAG216", 8);
        tag->capacity = 888;
        gb_uri(&msg, 0xC0, 0x04, "claim.metamask-airdrop.xyz/connectwallet?ref=9910");
        break;

    case 8: /* a link and an app, arriving together */
        memcpy(tag->tech_name, "NTAG216", 8);
        tag->capacity = 888;
        gb_uri(&msg, 0x90, 0x04, "cdn.freegames.top/install/game.apk"); /* MB */
        gb_aar(&msg, 0x40, "com.freegames.installer"); /* ME */
        break;

    case 9: /* the caption is the bait, the link is the hook */
        memcpy(tag->tech_name, "NTAG215", 8);
        tag->capacity = 504;
        gb_smart_poster(&msg, 0xC0, "HDFC Bank NetBanking - reactivate", 0x04,
                        "hdfc-verify.icu/login/confirm");
        break;

    case 10: /* not a website at all */
        memcpy(tag->tech_name, "NTAG213", 8);
        tag->capacity = 144;
        gb_smart_poster(&msg, 0xC0, "You have won! Call to claim", 0x05, "+449098790123");
        break;

    case 11: /* nothing on it, which is its own answer */
        memcpy(tag->tech_name, "NTAG213", 8);
        tag->capacity = 144;
        tag->status = GkTagNoNdef;
        return true;

    default:
        return false;
    }

    GkBuf tlv = {0};
    gb_wrap_tlv(&msg, &tlv);
    gk_ndef_parse_tlv(tlv.buf, tlv.len, tag);
    return true;
}
