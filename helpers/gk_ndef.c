#include "gk_ndef.h"

#include <string.h>

/* The NFC Forum URI record does not store "https://" -- it stores a one-byte
 * code for it, and the rest of the URL after that. It is a compression
 * scheme for tags with 48 bytes of memory, and it is also the reason a tag
 * can hold a URL that shares no visible bytes with the one your phone opens:
 * the prefix never appears in the tag's memory at all. */
static const char* const gk_uri_prefixes[] = {
    "", /* 0x00 - the URL is stored whole */
    "http://www.",
    "https://www.",
    "http://",
    "https://",
    "tel:",
    "mailto:",
    "ftp://anonymous:anonymous@",
    "ftp://ftp.",
    "ftps://",
    "sftp://",
    "smb://",
    "nfs://",
    "ftp://",
    "dav://",
    "news:",
    "telnet://",
    "imap:",
    "rtsp://",
    "urn:",
    "pop:",
    "sip:",
    "sips:",
    "tftp:",
    "btspp://",
    "btl2cap://",
    "btgoep://",
    "tcpobex://",
    "irdaobex://",
    "file://",
    "urn:epc:id:",
    "urn:epc:tag:",
    "urn:epc:pat:",
    "urn:epc:raw:",
    "urn:epc:",
    "urn:nfc:",
};
#define GK_URI_PREFIX_COUNT (sizeof(gk_uri_prefixes) / sizeof(gk_uri_prefixes[0]))

const char* gk_ndef_uri_prefix(uint8_t code) {
    if(code >= GK_URI_PREFIX_COUNT) return "";
    return gk_uri_prefixes[code];
}

const char* gk_tech_name(GkTech t) {
    switch(t) {
    case GkTechType2:
        return "NFC Type 2";
    case GkTechType4:
        return "NFC Type 4";
    case GkTechClassic:
        return "MIFARE Classic";
    case GkTechFelica:
        return "FeliCa";
    case GkTechOther:
        return "Other";
    case GkTechUnknown:
    default:
        return "Unknown";
    }
}

const char* gk_rec_kind_name(GkRecKind k) {
    switch(k) {
    case GkRecUri:
        return "URI";
    case GkRecText:
        return "Text";
    case GkRecSmartPoster:
        return "Smart Poster";
    case GkRecAar:
        return "Android app";
    case GkRecMime:
        return "MIME";
    case GkRecAbsUri:
        return "Absolute URI";
    case GkRecExternal:
        return "External";
    case GkRecEmpty:
        return "Empty";
    case GkRecUnknown:
    default:
        return "Unknown";
    }
}

void gk_tag_init(GkTag* tag) {
    if(!tag) return;
    memset(tag, 0, sizeof(*tag));
    tag->status = GkTagNoNdef;
}

/* --------------------------------------------------------------- strings
 *
 * Text off a tag goes straight onto a 128x64 screen, so control bytes are
 * replaced rather than drawn, and a run of unrenderable bytes collapses to a
 * single '?' -- a Devanagari title should read as "?" and not as forty
 * question marks pushing the URL off the screen.
 */
static void gk_copy_display(const uint8_t* src, size_t len, char* dst, size_t cap) {
    if(!dst || cap == 0) return;
    size_t o = 0;
    bool last_sub = false;
    for(size_t i = 0; i < len && o + 1 < cap; i++) {
        uint8_t c = src[i];
        if(c >= 0x20 && c < 0x7F) {
            dst[o++] = (char)c;
            last_sub = false;
        } else {
            if(!last_sub) {
                dst[o++] = '?';
                last_sub = true;
            }
        }
    }
    /* Trailing whitespace on a title is invisible and pushes text around. */
    while(o > 0 && dst[o - 1] == ' ') o--;
    dst[o] = '\0';
}

/* ------------------------------------------------------------------ URI */

size_t gk_ndef_expand_uri(
    const uint8_t* payload,
    size_t len,
    char* dst,
    size_t cap,
    bool* truncated) {
    if(truncated) *truncated = false;
    if(!dst || cap == 0) return 0;
    dst[0] = '\0';
    if(!payload || len == 0) return 0;

    const char* prefix = gk_ndef_uri_prefix(payload[0]);
    size_t plen = strlen(prefix);
    size_t rest = len - 1;

    size_t o = 0;
    for(size_t i = 0; i < plen && o + 1 < cap; i++) dst[o++] = prefix[i];

    /* Control bytes inside a URL are not a rendering problem, they are the
     * attack: a NUL or a newline truncates the string for one consumer and
     * not another. They are replaced here so that what Gatekeeper grades and
     * what Gatekeeper draws are the same string. */
    for(size_t i = 0; i < rest && o + 1 < cap; i++) {
        uint8_t c = payload[1 + i];
        dst[o++] = (c < 0x20 || c == 0x7F) ? '?' : (char)c;
    }
    dst[o] = '\0';

    if(truncated && (plen + rest) > (cap - 1)) *truncated = true;
    return o;
}

/* ------------------------------------------------------------- records */

typedef struct {
    uint8_t tnf;
    bool mb, me, cf, il;
    const uint8_t* type;
    uint8_t type_len;
    const uint8_t* payload;
    uint32_t payload_len;
} GkRawRecord;

/* Pull one record out of `buf`, advancing *pos. Returns false the moment
 * anything does not fit -- which stops the walk rather than reading past the
 * end of a tag that lied about its lengths. */
static bool gk_read_record(const uint8_t* buf, size_t len, size_t* pos, GkRawRecord* out) {
    size_t p = *pos;
    if(p + 2 > len) return false;

    uint8_t hdr = buf[p++];
    out->tnf = hdr & 0x07;
    out->mb = (hdr & 0x80) != 0;
    out->me = (hdr & 0x40) != 0;
    out->cf = (hdr & 0x20) != 0;
    bool sr = (hdr & 0x10) != 0;
    out->il = (hdr & 0x08) != 0;

    out->type_len = buf[p++];

    if(sr) {
        if(p + 1 > len) return false;
        out->payload_len = buf[p++];
    } else {
        if(p + 4 > len) return false;
        out->payload_len = ((uint32_t)buf[p] << 24) | ((uint32_t)buf[p + 1] << 16) |
                           ((uint32_t)buf[p + 2] << 8) | (uint32_t)buf[p + 3];
        p += 4;
    }

    uint8_t id_len = 0;
    if(out->il) {
        if(p + 1 > len) return false;
        id_len = buf[p++];
    }

    if(p + out->type_len > len) return false;
    out->type = buf + p;
    p += out->type_len;

    if(p + id_len > len) return false;
    p += id_len; /* the ID is not something we grade on */

    /* The one check that matters most: a 4-byte length field can claim four
     * gigabytes inside a 137-byte tag. */
    if(out->payload_len > len || p + out->payload_len > len) return false;
    out->payload = buf + p;
    p += out->payload_len;

    *pos = p;
    return true;
}

static bool gk_type_is(const GkRawRecord* r, const char* s) {
    size_t n = strlen(s);
    if(r->type_len != n) return false;
    return memcmp(r->type, s, n) == 0;
}

static GkRecKind gk_classify(const GkRawRecord* r) {
    switch(r->tnf) {
    case 0x00:
        return GkRecEmpty;
    case 0x01:
        if(gk_type_is(r, "U")) return GkRecUri;
        if(gk_type_is(r, "T")) return GkRecText;
        if(gk_type_is(r, "Sp")) return GkRecSmartPoster;
        return GkRecUnknown;
    case 0x02:
        return GkRecMime;
    case 0x03:
        return GkRecAbsUri;
    case 0x04:
        if(r->type_len == 15 && memcmp(r->type, "android.com:pkg", 15) == 0) return GkRecAar;
        return GkRecExternal;
    default:
        return GkRecUnknown;
    }
}

/* A Text record is a status byte, a language tag, then the text. The status
 * byte's low six bits are the language length, so a value larger than the
 * payload is a malformed record and not a very long language name. */
static bool gk_text_body(
    const GkRawRecord* r,
    const uint8_t** text,
    size_t* text_len,
    const uint8_t** lang,
    size_t* lang_len) {
    if(r->payload_len < 1) return false;
    uint8_t status = r->payload[0];
    size_t ll = status & 0x3F;
    if(1 + ll > r->payload_len) return false;
    *lang = r->payload + 1;
    *lang_len = ll;
    *text = r->payload + 1 + ll;
    *text_len = r->payload_len - 1 - ll;
    return true;
}

static void gk_add_record(GkTag* tag, const GkRawRecord* r, GkRecKind kind, bool nested) {
    tag->record_total++;
    if(tag->record_count >= GK_REC_MAX) return;

    GkRecord* rec = &tag->rec[tag->record_count++];
    memset(rec, 0, sizeof(*rec));
    rec->kind = kind;
    rec->tnf = r->tnf;
    rec->nested = nested;
    rec->chunked = r->cf;
    rec->payload_len = (uint16_t)(r->payload_len > 0xFFFF ? 0xFFFF : r->payload_len);
    gk_copy_display(r->type, r->type_len, rec->type, sizeof(rec->type));

    switch(kind) {
    case GkRecUri: {
        char tmp[GK_SUMMARY_MAX + 24];
        bool trunc = false;
        gk_ndef_expand_uri(r->payload, r->payload_len, tmp, sizeof(tmp), &trunc);
        gk_copy_display((const uint8_t*)tmp, strlen(tmp), rec->summary, sizeof(rec->summary));
        break;
    }
    case GkRecText: {
        const uint8_t *txt = NULL, *lang = NULL;
        size_t tlen = 0, llen = 0;
        if(gk_text_body(r, &txt, &tlen, &lang, &llen)) {
            gk_copy_display(txt, tlen, rec->summary, sizeof(rec->summary));
        } else {
            gk_copy_display((const uint8_t*)"(malformed)", 11, rec->summary, sizeof(rec->summary));
        }
        break;
    }
    case GkRecAar:
    case GkRecExternal:
    case GkRecMime:
    case GkRecAbsUri:
        gk_copy_display(r->payload, r->payload_len, rec->summary, sizeof(rec->summary));
        break;
    default:
        rec->summary[0] = '\0';
        break;
    }
}

bool gk_ndef_parse_message(const uint8_t* msg, size_t len, GkTag* tag, uint8_t depth) {
    if(!tag) return false;
    if(!msg || len == 0) return false;
    /* One level of nesting is all a Smart Poster is allowed, and all any real
     * tag uses. Bounding it here means a tag cannot recurse the stack away. */
    if(depth > 1) return false;

    size_t pos = 0;
    bool any = false;
    uint8_t guard = 0;

    while(pos < len) {
        GkRawRecord r;
        if(!gk_read_record(msg, len, &pos, &r)) {
            tag->malformed = true;
            break;
        }
        if(++guard > 32) { /* a message with more records than this is not a poster */
            tag->malformed = true;
            break;
        }
        any = true;

        GkRecKind kind = gk_classify(&r);
        gk_add_record(tag, &r, kind, depth > 0);

        switch(kind) {
        case GkRecUri:
            /* First URI wins. A Smart Poster's own URI is parsed before the
             * outer walk continues, so the link the poster is "about" is the
             * one graded, not a decoy appended after it. */
            if(!tag->has_url) {
                bool trunc = false;
                tag->url_len = (uint16_t)gk_ndef_expand_uri(
                    r.payload, r.payload_len, tag->url, sizeof(tag->url), &trunc);
                tag->url_truncated = trunc;
                tag->has_url = tag->url_len > 0;
            }
            break;

        case GkRecText:
            if(!tag->has_title) {
                const uint8_t *txt = NULL, *lang = NULL;
                size_t tlen = 0, llen = 0;
                if(gk_text_body(&r, &txt, &tlen, &lang, &llen) && tlen > 0) {
                    gk_copy_display(txt, tlen, tag->title, sizeof(tag->title));
                    tag->has_title = tag->title[0] != '\0';
                }
            }
            break;

        case GkRecSmartPoster:
            gk_ndef_parse_message(r.payload, r.payload_len, tag, (uint8_t)(depth + 1));
            break;

        case GkRecAar:
            if(!tag->has_aar) {
                gk_copy_display(r.payload, r.payload_len, tag->aar, sizeof(tag->aar));
                tag->has_aar = tag->aar[0] != '\0';
            }
            break;

        case GkRecAbsUri:
            /* TNF 3 stores the URI as the *type* field, with no prefix byte. */
            if(!tag->has_url && r.type_len > 0) {
                gk_copy_display(r.type, r.type_len, tag->url, sizeof(tag->url));
                tag->url_len = (uint16_t)strlen(tag->url);
                tag->has_url = tag->url_len > 0;
            }
            break;

        default:
            break;
        }

        if(r.me) break; /* Message End */
    }

    return any;
}

/* ------------------------------------------------------------------ TLV */

bool gk_ndef_parse_tlv(const uint8_t* data, size_t len, GkTag* tag) {
    if(!tag) return false;
    if(!data || len == 0) {
        if(tag->status == GkTagOk) tag->status = GkTagNoNdef;
        return false;
    }

    size_t pos = 0;
    bool found = false;

    while(pos < len) {
        uint8_t t = data[pos++];

        if(t == 0x00) continue; /* NULL TLV: padding */
        if(t == 0xFE) break; /* Terminator */

        if(pos >= len) {
            tag->malformed = true;
            break;
        }

        /* Length is one byte, unless it is 0xFF, in which case it is the two
         * that follow. A three-byte form claiming 0xFFFF inside a 144-byte
         * tag is exactly the kind of thing that gets checked below. */
        size_t l = data[pos++];
        if(l == 0xFF) {
            if(pos + 2 > len) {
                tag->malformed = true;
                break;
            }
            l = ((size_t)data[pos] << 8) | data[pos + 1];
            pos += 2;
        }

        if(pos + l > len) {
            /* The tag says the value is longer than the memory we read. Grade
             * what we have and say the read was short. */
            tag->malformed = true;
            l = len - pos;
            if(t == 0x03 && l > 0) {
                gk_ndef_parse_message(data + pos, l, tag, 0);
                found = true;
            }
            break;
        }

        if(t == 0x03) { /* NDEF message */
            found = true;
            tag->used = (uint16_t)l;
            if(l > 0) gk_ndef_parse_message(data + pos, l, tag, 0);
        }
        /* 0x01 lock control, 0x02 memory control, 0xFD proprietary: skipped
         * deliberately -- they describe the memory map, not the message. */

        pos += l;
    }

    if(!found) {
        tag->status = GkTagNoNdef;
        return false;
    }
    tag->status = tag->has_url ? GkTagOk : GkTagNoUrl;
    return true;
}
