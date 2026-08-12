#include "gk_url.h"

#include <string.h>

/* ------------------------------------------------------------------------
 * Public suffixes
 *
 * To find the registrable domain you have to know where the "for sale" part
 * of a hostname stops. There is no rule for it -- `co.uk` is a suffix and
 * `co.com` is a domain -- so it has to be a list. The real Public Suffix
 * List is about ten thousand entries and 200 KB; that is not going on a
 * Flipper, and it does not need to be. What matters is that a wrong answer
 * is *conservative*: if we mistake a suffix for a domain we highlight one
 * label too few, which under-warns nobody, because the label we highlight is
 * still inside the attacker's domain.
 *
 * Two kinds of entry live here:
 *
 *   ICANN suffixes  - country and sector suffixes with more than one label.
 *                     Single-label suffixes (com, org, de) need no entry:
 *                     they are the default.
 *
 *   Hosting suffixes - domains that hand out subdomains to anyone who asks,
 *                     usually free and usually in seconds. These belong here
 *                     for the same reason: `evil.github.io` is not a
 *                     subdomain of GitHub in any sense a user cares about,
 *                     it is a stranger's site, and the highlight has to say
 *                     so. They are also a finding in their own right, which
 *                     is what the `hosting` flag is for.
 */

typedef struct {
    const char* suffix;
    bool hosting;
} GkSuffixEntry;

static const GkSuffixEntry gk_suffixes[] = {
    /* --- United Kingdom --- */
    {"co.uk", false},
    {"org.uk", false},
    {"me.uk", false},
    {"ltd.uk", false},
    {"plc.uk", false},
    {"net.uk", false},
    {"sch.uk", false},
    {"ac.uk", false},
    {"gov.uk", false},
    {"nhs.uk", false},
    {"police.uk", false},
    /* --- Australia / New Zealand --- */
    {"com.au", false},
    {"net.au", false},
    {"org.au", false},
    {"edu.au", false},
    {"gov.au", false},
    {"id.au", false},
    {"asn.au", false},
    {"co.nz", false},
    {"net.nz", false},
    {"org.nz", false},
    {"govt.nz", false},
    {"ac.nz", false},
    /* --- India --- */
    {"co.in", false},
    {"net.in", false},
    {"org.in", false},
    {"gov.in", false},
    {"ac.in", false},
    {"edu.in", false},
    {"res.in", false},
    {"nic.in", false},
    {"firm.in", false},
    {"gen.in", false},
    {"ind.in", false},
    /* --- East Asia --- */
    {"co.jp", false},
    {"ne.jp", false},
    {"or.jp", false},
    {"go.jp", false},
    {"ac.jp", false},
    {"lg.jp", false},
    {"co.kr", false},
    {"or.kr", false},
    {"ne.kr", false},
    {"go.kr", false},
    {"re.kr", false},
    {"com.cn", false},
    {"net.cn", false},
    {"org.cn", false},
    {"gov.cn", false},
    {"edu.cn", false},
    {"ac.cn", false},
    {"com.tw", false},
    {"net.tw", false},
    {"org.tw", false},
    {"gov.tw", false},
    {"edu.tw", false},
    {"com.hk", false},
    {"net.hk", false},
    {"org.hk", false},
    {"gov.hk", false},
    {"edu.hk", false},
    {"idv.hk", false},
    {"com.sg", false},
    {"net.sg", false},
    {"org.sg", false},
    {"gov.sg", false},
    {"edu.sg", false},
    {"com.my", false},
    {"net.my", false},
    {"org.my", false},
    {"gov.my", false},
    {"edu.my", false},
    {"com.ph", false},
    {"net.ph", false},
    {"org.ph", false},
    {"gov.ph", false},
    {"edu.ph", false},
    {"com.vn", false},
    {"net.vn", false},
    {"org.vn", false},
    {"gov.vn", false},
    {"edu.vn", false},
    {"co.th", false},
    {"in.th", false},
    {"go.th", false},
    {"ac.th", false},
    {"co.id", false},
    {"or.id", false},
    {"go.id", false},
    {"ac.id", false},
    {"web.id", false},
    /* --- South Asia / Middle East / Africa --- */
    {"com.pk", false},
    {"net.pk", false},
    {"org.pk", false},
    {"gov.pk", false},
    {"edu.pk", false},
    {"com.bd", false},
    {"net.bd", false},
    {"org.bd", false},
    {"gov.bd", false},
    {"ac.bd", false},
    {"com.np", false},
    {"org.np", false},
    {"gov.np", false},
    {"edu.np", false},
    {"com.lk", false},
    {"org.lk", false},
    {"gov.lk", false},
    {"ac.lk", false},
    {"co.il", false},
    {"net.il", false},
    {"org.il", false},
    {"gov.il", false},
    {"ac.il", false},
    {"com.sa", false},
    {"net.sa", false},
    {"org.sa", false},
    {"gov.sa", false},
    {"edu.sa", false},
    {"co.ae", false},
    {"net.ae", false},
    {"org.ae", false},
    {"gov.ae", false},
    {"ac.ae", false},
    {"com.qa", false},
    {"gov.qa", false},
    {"edu.qa", false},
    {"com.kw", false},
    {"gov.kw", false},
    {"com.tr", false},
    {"net.tr", false},
    {"org.tr", false},
    {"gov.tr", false},
    {"edu.tr", false},
    {"com.eg", false},
    {"gov.eg", false},
    {"edu.eg", false},
    {"com.ng", false},
    {"gov.ng", false},
    {"edu.ng", false},
    {"co.za", false},
    {"org.za", false},
    {"net.za", false},
    {"gov.za", false},
    {"ac.za", false},
    {"co.ke", false},
    {"go.ke", false},
    {"ac.ke", false},
    {"co.tz", false},
    {"co.ug", false},
    {"com.gh", false},
    /* --- Europe --- */
    {"gov.it", false},
    {"edu.it", false},
    {"gouv.fr", false},
    {"ac.be", false},
    {"com.es", false},
    {"org.es", false},
    {"gob.es", false},
    {"com.pt", false},
    {"gov.pt", false},
    {"com.gr", false},
    {"net.gr", false},
    {"org.gr", false},
    {"gov.gr", false},
    {"edu.gr", false},
    {"com.pl", false},
    {"net.pl", false},
    {"org.pl", false},
    {"gov.pl", false},
    {"edu.pl", false},
    {"com.ua", false},
    {"net.ua", false},
    {"org.ua", false},
    {"gov.ua", false},
    {"com.ru", false},
    {"net.ru", false},
    {"org.ru", false},
    /* --- Americas --- */
    {"com.br", false},
    {"net.br", false},
    {"org.br", false},
    {"gov.br", false},
    {"edu.br", false},
    {"com.mx", false},
    {"org.mx", false},
    {"gob.mx", false},
    {"edu.mx", false},
    {"com.ar", false},
    {"net.ar", false},
    {"org.ar", false},
    {"gob.ar", false},
    {"edu.ar", false},
    {"com.co", false},
    {"net.co", false},
    {"org.co", false},
    {"gov.co", false},
    {"edu.co", false},
    {"com.pe", false},
    {"gob.pe", false},
    {"edu.pe", false},
    {"com.ve", false},
    {"com.ec", false},
    {"com.uy", false},
    {"com.do", false},
    {"com.gt", false},
    {"gc.ca", false},
    {"on.ca", false},
    {"qc.ca", false},
    {"bc.ca", false},
    {"ab.ca", false},

    /* --- Anyone can have a subdomain here, usually free, usually today --- */
    {"github.io", true},
    {"gitlab.io", true},
    {"pages.dev", true},
    {"workers.dev", true},
    {"r2.dev", true},
    {"netlify.app", true},
    {"vercel.app", true},
    {"web.app", true},
    {"page.link", true},
    {"firebaseapp.com", true},
    {"appspot.com", true},
    {"herokuapp.com", true},
    {"glitch.me", true},
    {"repl.co", true},
    {"replit.app", true},
    {"ngrok.io", true},
    {"ngrok.app", true},
    {"ngrok-free.app", true},
    {"trycloudflare.com", true},
    {"loca.lt", true},
    {"serveo.net", true},
    {"duckdns.org", true},
    {"no-ip.org", true},
    {"no-ip.biz", true},
    {"ddns.net", true},
    {"hopto.org", true},
    {"zapto.org", true},
    {"sytes.net", true},
    {"myftp.biz", true},
    {"000webhostapp.com", true},
    {"blogspot.com", true},
    {"wordpress.com", true},
    {"wixsite.com", true},
    {"weebly.com", true},
    {"webflow.io", true},
    {"square.site", true},
    {"godaddysites.com", true},
    {"mystrikingly.com", true},
    {"notion.site", true},
    {"framer.website", true},
    {"carrd.co", true},
    {"bio.link", true},
    {"tilda.ws", true},
    {"jimdosite.com", true},
    {"bubbleapps.io", true},
    {"softr.app", true},
    {"azurewebsites.net", true},
    {"cloudapp.net", true},
    {"s3.amazonaws.com", true},
    {"amazonaws.com", true},
    {"googleusercontent.com", true},
    {"dropboxusercontent.com", true},
    {"backblazeb2.com", true},
    /* Google Translate will proxy any page under its own hostname, which is
     * why phishing kits love it: the address bar says google. */
    {"translate.goog", true},
};

#define GK_SUFFIX_COUNT (sizeof(gk_suffixes) / sizeof(gk_suffixes[0]))
#define GK_SUFFIX_MAX_LABELS 3
#define GK_MAX_LABELS 16

/* ---------------------------------------------------------------- ctype
 *
 * Written out rather than pulled from <ctype.h>: the libc versions are
 * locale-dependent and take signed char, which is a classic UB trap on
 * high-bit bytes -- and a hostname full of high-bit bytes is exactly the
 * input this parser is built to survive.
 */

static char gk_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static bool gk_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool gk_is_hex(char c) {
    return gk_is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static uint8_t gk_hex_val(char c) {
    if(gk_is_digit(c)) return (uint8_t)(c - '0');
    if(c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    return (uint8_t)(c - 'A' + 10);
}

/* ---------------------------------------------------------------- spans */

bool gk_span_eq(const char* raw, GkSpan s, const char* str) {
    if(!raw || !str) return false;
    size_t n = strlen(str);
    if(n != s.len) return false;
    for(uint16_t i = 0; i < s.len; i++) {
        if(gk_lower(raw[s.off + i]) != gk_lower(str[i])) return false;
    }
    return true;
}

bool gk_span_contains(const char* raw, GkSpan s, const char* needle) {
    if(!raw || !needle) return false;
    size_t n = strlen(needle);
    if(n == 0 || n > s.len) return false;
    for(uint16_t i = 0; i + n <= s.len; i++) {
        size_t j = 0;
        while(j < n && gk_lower(raw[s.off + i + j]) == gk_lower(needle[j])) j++;
        if(j == n) return true;
    }
    return false;
}

void gk_span_copy(const char* raw, GkSpan s, char* dst, size_t cap) {
    if(!dst || cap == 0) return;
    size_t n = s.len;
    if(n > cap - 1) n = cap - 1;
    if(raw && n) memcpy(dst, raw + s.off, n);
    dst[n] = '\0';
}

/* --------------------------------------------------------------- scheme */

static const struct {
    const char* name;
    GkScheme scheme;
} gk_scheme_table[] = {
    {"https", GkSchemeHttps},
    {"http", GkSchemeHttp},
    {"tel", GkSchemeTel},
    {"mailto", GkSchemeMailto},
    {"sms", GkSchemeSms},
    {"smsto", GkSchemeSms},
    {"geo", GkSchemeGeo},
    {"ftp", GkSchemeFtp},
    {"ftps", GkSchemeFtp},
    {"file", GkSchemeFile},
    {"javascript", GkSchemeJavascript},
    {"data", GkSchemeData},
    {"intent", GkSchemeIntent},
    {"market", GkSchemeMarket},
};

const char* gk_scheme_name(GkScheme s) {
    switch(s) {
    case GkSchemeHttps:
        return "https";
    case GkSchemeHttp:
        return "http";
    case GkSchemeTel:
        return "tel";
    case GkSchemeMailto:
        return "mailto";
    case GkSchemeSms:
        return "sms";
    case GkSchemeGeo:
        return "geo";
    case GkSchemeFtp:
        return "ftp";
    case GkSchemeFile:
        return "file";
    case GkSchemeJavascript:
        return "javascript";
    case GkSchemeData:
        return "data";
    case GkSchemeIntent:
        return "intent";
    case GkSchemeMarket:
        return "market";
    case GkSchemeOther:
        return "other";
    case GkSchemeNone:
    default:
        return "none";
    }
}

bool gk_scheme_is_web(GkScheme s) {
    return s == GkSchemeHttp || s == GkSchemeHttps;
}

/* ------------------------------------------------------------ host form */

static bool gk_host_is_ipv4(const char* raw, GkSpan h) {
    if(h.len < 7 || h.len > 15) return false;
    uint8_t octets = 0;
    uint16_t i = 0;
    while(i < h.len) {
        uint16_t digits = 0;
        uint32_t val = 0;
        while(i < h.len && gk_is_digit(raw[h.off + i])) {
            val = val * 10 + (uint32_t)(raw[h.off + i] - '0');
            digits++;
            i++;
        }
        if(digits == 0 || digits > 3 || val > 255) return false;
        octets++;
        if(i < h.len) {
            if(raw[h.off + i] != '.') return false;
            i++;
            if(i == h.len) return false; /* trailing dot */
        }
    }
    return octets == 4;
}

/* --------------------------------------------------------------- labels */

typedef struct {
    uint16_t off;
    uint16_t len;
} GkLabel;

static uint8_t gk_split_labels(const char* raw, GkSpan h, GkLabel* out, uint8_t cap) {
    uint8_t n = 0;
    uint16_t start = 0;
    for(uint16_t i = 0; i <= h.len; i++) {
        if(i == h.len || raw[h.off + i] == '.') {
            if(n < cap) {
                out[n].off = (uint16_t)(h.off + start);
                out[n].len = (uint16_t)(i - start);
                n++;
            }
            start = (uint16_t)(i + 1);
        }
    }
    return n;
}

/* Compare the last `k` labels against a dotted suffix string. */
static bool gk_labels_match_suffix(
    const char* raw,
    const GkLabel* labels,
    uint8_t label_count,
    uint8_t k,
    const char* suffix) {
    uint8_t first = (uint8_t)(label_count - k);
    size_t pos = 0;
    size_t slen = strlen(suffix);
    for(uint8_t i = 0; i < k; i++) {
        const GkLabel* l = &labels[first + i];
        if(pos + l->len > slen) return false;
        for(uint16_t j = 0; j < l->len; j++) {
            if(gk_lower(raw[l->off + j]) != gk_lower(suffix[pos + j])) return false;
        }
        pos += l->len;
        if(i + 1 < k) {
            if(pos >= slen || suffix[pos] != '.') return false;
            pos++;
        }
    }
    return pos == slen;
}

/* Longest match wins: `mybucket.s3.amazonaws.com` must resolve against
 * `s3.amazonaws.com` and not stop at `amazonaws.com`. */
static void gk_find_suffix(const char* raw, GkUrl* u, const GkLabel* labels, uint8_t n) {
    u->suffix_is_hosting = false;
    u->suffix_multi = false;

    uint8_t best_k = 0;
    bool best_hosting = false;

    uint8_t max_k = (n < GK_SUFFIX_MAX_LABELS) ? n : GK_SUFFIX_MAX_LABELS;
    for(uint8_t k = max_k; k >= 2; k--) {
        for(size_t s = 0; s < GK_SUFFIX_COUNT; s++) {
            if(gk_labels_match_suffix(raw, labels, n, k, gk_suffixes[s].suffix)) {
                best_k = k;
                best_hosting = gk_suffixes[s].hosting;
                break;
            }
        }
        if(best_k) break;
    }

    if(best_k == 0) best_k = 1; /* the default: a single-label suffix */
    if(best_k > n) best_k = n;

    uint8_t first_suffix = (uint8_t)(n - best_k);
    u->suffix.off = labels[first_suffix].off;
    u->suffix.len =
        (uint16_t)(labels[n - 1].off + labels[n - 1].len - labels[first_suffix].off);
    u->suffix_is_hosting = best_hosting;
    u->suffix_multi = best_k > 1;

    /* registrable = suffix + one more label, when there is one to take */
    uint8_t first_reg = (first_suffix > 0) ? (uint8_t)(first_suffix - 1) : 0;
    u->registrable.off = labels[first_reg].off;
    u->registrable.len =
        (uint16_t)(labels[n - 1].off + labels[n - 1].len - labels[first_reg].off);

    if(first_suffix > 0) {
        u->core.off = labels[first_reg].off;
        u->core.len = labels[first_reg].len;
    } else {
        /* A bare suffix with nothing in front of it -- "http://com/". There
         * is no registrable domain, so the core is empty and the whole thing
         * gets highlighted. */
        u->core.off = labels[0].off;
        u->core.len = 0;
    }
}

/* ---------------------------------------------------------------- parse */

bool gk_url_parse(const char* url, size_t len, GkUrl* out) {
    if(!out) return false;
    memset(out, 0, sizeof(*out));
    if(!url) return false;
    if(len == 0) len = strlen(url);
    if(len == 0) return false;
    if(len > GK_URL_MAX - 1) len = GK_URL_MAX - 1;

    out->raw = url;
    out->raw_len = (uint16_t)len;
    out->host_form = GkHostEmpty;

    uint16_t i = 0;

    /* --- scheme ---
     * RFC 3986: ALPHA *( ALPHA / DIGIT / "+" / "-" / "." ) ":". Anything that
     * does not match is not a scheme, and the whole string is a path. */
    uint16_t colon = 0;
    if(len > 0 && ((url[0] >= 'a' && url[0] <= 'z') || (url[0] >= 'A' && url[0] <= 'Z'))) {
        for(uint16_t k = 1; k < len; k++) {
            char c = url[k];
            if(c == ':') {
                colon = k;
                break;
            }
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || gk_is_digit(c) ||
                      c == '+' || c == '-' || c == '.';
            if(!ok) break;
        }
    }

    if(colon > 0) {
        out->scheme_s.off = 0;
        out->scheme_s.len = colon;
        out->scheme = GkSchemeOther;
        for(size_t s = 0; s < sizeof(gk_scheme_table) / sizeof(gk_scheme_table[0]); s++) {
            if(gk_span_eq(url, out->scheme_s, gk_scheme_table[s].name)) {
                out->scheme = gk_scheme_table[s].scheme;
                break;
            }
        }
        i = (uint16_t)(colon + 1);
    } else {
        out->scheme = GkSchemeNone;
    }

    /* --- authority --- */
    if((size_t)(i + 1) < len && url[i] == '/' && url[i + 1] == '/') {
        out->has_authority = true;
        i += 2;
    }

    uint16_t auth_start = i;
    if(out->has_authority) {
        while(i < len && url[i] != '/' && url[i] != '?' && url[i] != '#') i++;
        out->authority.off = auth_start;
        out->authority.len = (uint16_t)(i - auth_start);

        /* userinfo: everything up to the LAST '@' in the authority. Browsers
         * take the last one, and so must we -- a kit that writes
         * "a@b@real.tld@evil.tld" is counting on a reader stopping at the
         * first. */
        uint16_t at = 0;
        bool have_at = false;
        for(uint16_t k = auth_start; k < i; k++) {
            if(url[k] == '@') {
                at = k;
                have_at = true;
            }
        }

        uint16_t host_start = auth_start;
        if(have_at) {
            out->has_userinfo = true;
            out->userinfo.off = auth_start;
            out->userinfo.len = (uint16_t)(at - auth_start);
            host_start = (uint16_t)(at + 1);
        }

        /* port: the last ':' outside of an IPv6 bracket */
        uint16_t host_end = i;
        bool bracketed = (host_start < i && url[host_start] == '[');
        if(bracketed) {
            uint16_t close = host_start;
            while(close < i && url[close] != ']') close++;
            out->host_form = GkHostIpv6;
            out->host.off = (uint16_t)(host_start + 1);
            out->host.len = (uint16_t)(close > host_start + 1 ? close - host_start - 1 : 0);
            host_end = close < i ? (uint16_t)(close + 1) : i;
            if(host_end < i && url[host_end] == ':') {
                out->has_port = true;
                for(uint16_t k = (uint16_t)(host_end + 1); k < i; k++) {
                    if(!gk_is_digit(url[k])) break;
                    out->port = (uint16_t)(out->port * 10 + (url[k] - '0'));
                }
            }
        } else {
            uint16_t last_colon = 0;
            bool have_colon = false;
            for(uint16_t k = host_start; k < i; k++) {
                if(url[k] == ':') {
                    last_colon = k;
                    have_colon = true;
                }
            }
            if(have_colon) {
                host_end = last_colon;
                out->has_port = true;
                for(uint16_t k = (uint16_t)(last_colon + 1); k < i; k++) {
                    if(!gk_is_digit(url[k])) {
                        out->has_port = false; /* ":not-a-port" is part of a broken host */
                        out->port = 0;
                        host_end = i;
                        break;
                    }
                    out->port = (uint16_t)(out->port * 10 + (url[k] - '0'));
                }
            }
            out->host.off = host_start;
            out->host.len = (uint16_t)(host_end > host_start ? host_end - host_start : 0);
        }
    } else {
        /* No authority: tel:, mailto:, javascript:, or a bare path. */
        out->authority.off = i;
        out->authority.len = 0;
    }

    /* --- path / query / fragment --- */
    uint16_t path_start = i;
    while(i < len && url[i] != '?' && url[i] != '#') i++;
    out->path.off = path_start;
    out->path.len = (uint16_t)(i - path_start);

    if(i < len && url[i] == '?') {
        uint16_t q = (uint16_t)(i + 1);
        while(i < len && url[i] != '#') i++;
        out->query.off = q;
        out->query.len = (uint16_t)(i > q ? i - q : 0);
    }
    if(i < len && url[i] == '#') {
        uint16_t f = (uint16_t)(i + 1);
        out->fragment.off = f;
        out->fragment.len = (uint16_t)(len - f);
    }

    /* --- host shape --- */
    if(out->host.len == 0) {
        if(out->host_form != GkHostIpv6) out->host_form = GkHostEmpty;
        return true;
    }

    for(uint16_t k = 0; k < out->host.len; k++) {
        char c = url[out->host.off + k];
        if(c == '%') out->host_has_escape = true;
    }
    if(out->has_userinfo) {
        for(uint16_t k = 0; k < out->userinfo.len; k++) {
            if(url[out->userinfo.off + k] == '%') out->host_has_escape = true;
        }
    }

    /* A trailing dot is a fully-qualified name; it resolves identically and
     * defeats naive string comparison against a blocklist. */
    if(url[out->host.off + out->host.len - 1] == '.') {
        out->host_trailing_dot = true;
        out->host.len--;
        if(out->host.len == 0) {
            out->host_form = GkHostEmpty;
            return true;
        }
    }

    if(out->host_form != GkHostIpv6) {
        out->host_form = gk_host_is_ipv4(url, out->host) ? GkHostIpv4 : GkHostName;
    }

    if(out->host_form == GkHostIpv4 || out->host_form == GkHostIpv6) {
        out->registrable = out->host;
        out->core = out->host;
        out->suffix.off = out->host.off;
        out->suffix.len = 0;
        out->label_count = 1;
        return true;
    }

    GkLabel labels[GK_MAX_LABELS];
    uint8_t n = gk_split_labels(url, out->host, labels, GK_MAX_LABELS);
    out->label_count = n;
    if(n == 0) {
        out->registrable = out->host;
        out->core = out->host;
        return true;
    }

    for(uint8_t l = 0; l < n; l++) {
        if(labels[l].len >= 4 && gk_lower(url[labels[l].off]) == 'x' &&
           gk_lower(url[labels[l].off + 1]) == 'n' && url[labels[l].off + 2] == '-' &&
           url[labels[l].off + 3] == '-') {
            out->host_has_punycode = true;
        }
    }

    if(n == 1) {
        /* "http://localhost/" or "http://intranet/" -- no suffix at all. */
        out->registrable = out->host;
        out->core = out->host;
        out->suffix.off = out->host.off;
        out->suffix.len = 0;
        return true;
    }

    gk_find_suffix(url, out, labels, n);
    return true;
}

/* ---------------------------------------------------------------- zones */

void gk_url_zones(const GkUrl* u, GkUrlZones* out) {
    if(!out) return;
    memset(out, 0, sizeof(*out));
    if(!u || !u->raw) return;

    if(u->registrable.len == 0) {
        /* Nothing to point at: the whole string is decoration. */
        out->pre_len = u->raw_len;
        return;
    }

    out->reg_off = u->registrable.off;
    out->reg_len = u->registrable.len;

    /* Everything before the host: scheme, "//", userinfo, "@". */
    uint16_t host_start = u->host.off;
    out->pre_len = host_start;

    /* The subdomain sits between the host start and the registrable domain.
     * On an IP or a single-label host these coincide and the band is empty. */
    out->sub_off = host_start;
    out->sub_len = (uint16_t)(u->registrable.off > host_start ? u->registrable.off - host_start : 0);

    out->post_off = (uint16_t)(u->registrable.off + u->registrable.len);
    out->post_len = (uint16_t)(u->raw_len > out->post_off ? u->raw_len - out->post_off : 0);
}

/* ------------------------------------------------------- percent-decode */

size_t gk_percent_decode(
    const char* src,
    size_t src_len,
    char* dst,
    size_t cap,
    bool* found_escape,
    bool* double_encoded) {
    if(found_escape) *found_escape = false;
    if(double_encoded) *double_encoded = false;
    if(!dst || cap == 0) return 0;
    if(!src) {
        dst[0] = '\0';
        return 0;
    }

    size_t o = 0;
    for(size_t i = 0; i < src_len && o + 1 < cap; i++) {
        if(src[i] == '%' && i + 2 < src_len && gk_is_hex(src[i + 1]) && gk_is_hex(src[i + 2])) {
            uint8_t v = (uint8_t)((gk_hex_val(src[i + 1]) << 4) | gk_hex_val(src[i + 2]));
            if(found_escape) *found_escape = true;
            /* %25 decodes to '%': the string was encoded twice, which is
             * something no honest link generator does. */
            if(v == '%' && double_encoded) *double_encoded = true;
            dst[o++] = (char)v;
            i += 2;
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = '\0';
    return o;
}

/* ----------------------------------------------------------- confusables
 *
 * Nobody reads a hostname character by character. They pattern-match it at a
 * glance, in a sans-serif font, on a phone, in the street. So the question
 * is not "are these strings equal" but "would these two look like the same
 * word to someone who is not looking hard". Folding both sides to a
 * skeleton and comparing that answers the right question:
 *
 *      paypal  -> paypai        paypa1 -> paypai        pay-pal -> paypai
 *      amazon  -> amazon        arnazon -> amazon
 *
 * Separators go too, because "pay-pal.com" and "paypal.com" are the same
 * word with a hyphen in it, and the hyphen is the whole attack.
 */
size_t gk_confusable_fold(const char* src, size_t src_len, char* dst, size_t cap) {
    if(!dst || cap == 0) return 0;
    if(!src) {
        dst[0] = '\0';
        return 0;
    }

    /* pass 1: lowercase, drop separators */
    size_t o = 0;
    for(size_t i = 0; i < src_len && o + 1 < cap; i++) {
        char c = gk_lower(src[i]);
        if(c == '-' || c == '_' || c == '.' || c == ' ' || c == '+') continue;
        dst[o++] = c;
    }
    dst[o] = '\0';

    /* pass 2: multigraphs that render as one letter */
    size_t w = 0;
    for(size_t i = 0; i < o;) {
        if(i + 1 < o && dst[i] == 'r' && dst[i + 1] == 'n') {
            dst[w++] = 'm';
            i += 2;
        } else if(i + 1 < o && dst[i] == 'v' && dst[i + 1] == 'v') {
            dst[w++] = 'w';
            i += 2;
        } else {
            dst[w++] = dst[i++];
        }
    }
    o = w;
    dst[o] = '\0';

    /* pass 3: single glyphs that share a shape */
    for(size_t i = 0; i < o; i++) {
        switch(dst[i]) {
        case '0':
            dst[i] = 'o';
            break;
        case '1':
        case 'l':
        case '|':
        case '!':
            dst[i] = 'i';
            break;
        case '3':
            dst[i] = 'e';
            break;
        case '4':
        case '@':
            dst[i] = 'a';
            break;
        case '5':
        case '$':
            dst[i] = 's';
            break;
        case '7':
            dst[i] = 't';
            break;
        case '8':
            dst[i] = 'b';
            break;
        case '9':
            dst[i] = 'g';
            break;
        case '2':
            dst[i] = 'z';
            break;
        default:
            break;
        }
    }
    return o;
}

bool gk_is_transposition(const char* a, size_t alen, const char* b, size_t blen) {
    if(!a || !b || alen != blen || alen < 2) return false;
    size_t i = 0;
    while(i < alen && a[i] == b[i]) i++;
    if(i + 1 >= alen) return false; /* identical, or differs only at the end */
    if(a[i] != b[i + 1] || a[i + 1] != b[i]) return false;
    for(size_t k = i + 2; k < alen; k++) {
        if(a[k] != b[k]) return false;
    }
    return true;
}
