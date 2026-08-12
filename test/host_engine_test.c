/* The engine, on the host, under ASan and UBSan.
 *
 * Gatekeeper's whole output is a rendering of what these three files decided,
 * and a screenshot cannot vouch for any of it. So the parser is fed hostile
 * bytes, the grader is pinned to its bands, and the promises the README makes
 * -- A+ is unreachable, a shortener never beats C, an unlocked tag never
 * beats B -- are asserted here rather than believed.
 *
 *      make -C test
 */
#include "helpers/gk_demo.h"
#include "helpers/gk_ndef.h"
#include "helpers/gk_url.h"
#include "helpers/gk_verdict.h"

#include <stdio.h>
#include <string.h>

static int checks = 0;
static int failures = 0;

#define CHECK(cond, ...)                                     \
    do {                                                     \
        checks++;                                            \
        if(!(cond)) {                                        \
            failures++;                                      \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);    \
            printf(__VA_ARGS__);                             \
            printf("\n");                                    \
        }                                                    \
    } while(0)

#define SECTION(name) printf("\n== %s ==\n", name)

/* ====================================================================== */
/*  URL parsing                                                           */
/* ====================================================================== */

static void expect_registrable(const char* url, const char* want_host, const char* want_reg) {
    GkUrl u;
    CHECK(gk_url_parse(url, 0, &u), "parse failed: %s", url);
    char host[96], reg[96];
    gk_span_copy(u.raw, u.host, host, sizeof(host));
    gk_span_copy(u.raw, u.registrable, reg, sizeof(reg));
    CHECK(strcmp(host, want_host) == 0, "%s: host %s, wanted %s", url, host, want_host);
    CHECK(strcmp(reg, want_reg) == 0, "%s: registrable %s, wanted %s", url, reg, want_reg);
}

static void test_url_parse(void) {
    SECTION("URL: components");

    GkUrl u;
    CHECK(!gk_url_parse("", 0, &u), "empty string should not parse");
    CHECK(!gk_url_parse(NULL, 0, &u), "NULL should not parse");

    CHECK(gk_url_parse("https://a.com/p?q=1#f", 0, &u), "parse");
    CHECK(u.scheme == GkSchemeHttps, "scheme");
    CHECK(gk_span_eq(u.raw, u.path, "/p"), "path");
    CHECK(gk_span_eq(u.raw, u.query, "q=1"), "query");
    CHECK(gk_span_eq(u.raw, u.fragment, "f"), "fragment");

    /* Ports */
    CHECK(gk_url_parse("https://a.com:8443/", 0, &u), "parse port");
    CHECK(u.has_port && u.port == 8443, "port 8443, got %u", u.port);
    CHECK(gk_span_eq(u.raw, u.host, "a.com"), "host excludes port");

    /* A colon that is not a port must not eat the host. */
    CHECK(gk_url_parse("https://a.com:notaport/", 0, &u), "parse bad port");
    CHECK(!u.has_port, "':notaport' is not a port");

    /* IPv6 */
    CHECK(gk_url_parse("http://[2001:db8::1]:8080/x", 0, &u), "parse v6");
    CHECK(u.host_form == GkHostIpv6, "v6 host form");
    CHECK(gk_span_eq(u.raw, u.host, "2001:db8::1"), "v6 host without brackets");
    CHECK(u.has_port && u.port == 8080, "v6 port");

    /* IPv4 vs a name that merely contains digits */
    CHECK(gk_url_parse("http://192.168.1.1/", 0, &u), "parse v4");
    CHECK(u.host_form == GkHostIpv4, "v4 detected");
    CHECK(gk_url_parse("http://192.168.1.1.evil.com/", 0, &u), "parse v4-lookalike");
    CHECK(u.host_form == GkHostName, "trailing labels make it a name");
    CHECK(gk_url_parse("http://999.1.1.1/", 0, &u), "parse out-of-range octet");
    CHECK(u.host_form == GkHostName, "999 is not an octet");

    /* The last @ wins -- a kit that writes several is counting on the first
     * one being believed. */
    CHECK(gk_url_parse("https://real.com@also.com@evil.tld/x", 0, &u), "parse multi-@");
    CHECK(u.has_userinfo, "userinfo present");
    CHECK(gk_span_eq(u.raw, u.host, "evil.tld"), "last @ decides the host");

    /* Trailing dot resolves the same and defeats string comparison. */
    CHECK(gk_url_parse("https://example.com./x", 0, &u), "parse trailing dot");
    CHECK(u.host_trailing_dot, "trailing dot flagged");
    CHECK(gk_span_eq(u.raw, u.host, "example.com"), "dot trimmed from host");

    CHECK(gk_url_parse("https://xn--80ak6aa92e.com/", 0, &u), "parse punycode");
    CHECK(u.host_has_punycode, "punycode flagged");

    /* No scheme at all */
    CHECK(gk_url_parse("example.com/path", 0, &u), "parse schemeless");
    CHECK(u.scheme == GkSchemeNone, "no scheme");

    /* Schemes without an authority */
    CHECK(gk_url_parse("tel:+441234", 0, &u), "parse tel");
    CHECK(u.scheme == GkSchemeTel && !u.has_authority, "tel has no authority");
    CHECK(gk_url_parse("javascript:alert(1)", 0, &u), "parse js");
    CHECK(u.scheme == GkSchemeJavascript, "javascript scheme");
    CHECK(gk_url_parse("JavaScript:alert(1)", 0, &u), "parse js mixed case");
    CHECK(u.scheme == GkSchemeJavascript, "scheme match is case-insensitive");
}

static void test_url_registrable(void) {
    SECTION("URL: registrable domain");

    expect_registrable("https://example.com/", "example.com", "example.com");
    expect_registrable("https://www.example.com/", "www.example.com", "example.com");
    expect_registrable("https://a.b.c.example.com/", "a.b.c.example.com", "example.com");
    expect_registrable("https://shop.example.co.uk/", "shop.example.co.uk", "example.co.uk");
    expect_registrable("https://www.tfl.gov.uk/", "www.tfl.gov.uk", "tfl.gov.uk");
    expect_registrable("https://a.b.example.com.au/", "a.b.example.com.au", "example.com.au");
    expect_registrable("https://foo.co.in/", "foo.co.in", "foo.co.in");

    /* Hosting suffixes: a stranger's subdomain is a stranger's site. */
    expect_registrable("https://evil.github.io/x", "evil.github.io", "evil.github.io");
    expect_registrable("https://a.b.evil.github.io/x", "a.b.evil.github.io", "evil.github.io");
    /* Longest match wins over the shorter suffix that also matches. */
    expect_registrable(
        "https://bucket.s3.amazonaws.com/k", "bucket.s3.amazonaws.com", "bucket.s3.amazonaws.com");

    /* Degenerate hosts */
    expect_registrable("http://localhost/x", "localhost", "localhost");
    expect_registrable("http://10.0.0.1/x", "10.0.0.1", "10.0.0.1");

    GkUrl u;
    CHECK(gk_url_parse("https://evil.github.io/", 0, &u), "parse hosting");
    CHECK(u.suffix_is_hosting, "github.io is a hosting suffix");
    CHECK(gk_url_parse("https://example.co.uk/", 0, &u), "parse icann");
    CHECK(!u.suffix_is_hosting, "co.uk is not a hosting suffix");
    CHECK(u.suffix_multi, "co.uk is multi-label");
}

static void test_url_zones(void) {
    SECTION("URL: display zones");

    /* The verdict screen draws the URL as four bands and inverts exactly one.
     * If the bands ever stop tiling the string, the highlight is drawn over
     * the wrong characters -- so the partition is checked, not assumed. */
    static const char* corpus[] = {
        "https://www.example.com/path?q=1#f",
        "http://192.168.0.1:8080/x",
        "https://apple.com@evil.tld/signin",
        "https://a.b.c.d.e.example.co.uk/",
        "javascript:alert(1)",
        "tel:+441234567890",
        "https://evil.github.io/",
        "example.com",
        "https://[2001:db8::1]/x",
        "h",
    };
    for(size_t i = 0; i < sizeof(corpus) / sizeof(corpus[0]); i++) {
        GkUrl u;
        CHECK(gk_url_parse(corpus[i], 0, &u), "parse %s", corpus[i]);
        GkUrlZones z;
        gk_url_zones(&u, &z);

        size_t total = (size_t)z.pre_len + z.sub_len + z.reg_len + z.post_len;
        CHECK(total == u.raw_len, "%s: zones cover %zu of %u bytes", corpus[i], total, u.raw_len);
        if(z.sub_len) CHECK(z.sub_off == z.pre_len, "%s: sub follows pre", corpus[i]);
        if(z.reg_len) {
            CHECK(z.reg_off == z.sub_off + z.sub_len, "%s: reg follows sub", corpus[i]);
            CHECK(z.post_off == z.reg_off + z.reg_len, "%s: post follows reg", corpus[i]);
        }
    }

    /* The highlighted band must be the registrable domain and nothing else. */
    GkUrl u;
    gk_url_parse("https://apple.com.id-check.top/signin", 0, &u);
    GkUrlZones z;
    gk_url_zones(&u, &z);
    CHECK(z.reg_len == 12 && memcmp(u.raw + z.reg_off, "id-check.top", 12) == 0,
          "highlight lands on id-check.top, got '%.*s'", z.reg_len, u.raw + z.reg_off);
}

static void test_url_helpers(void) {
    SECTION("URL: helpers");

    char buf[128];
    bool esc = false, dbl = false;

    gk_percent_decode("%68%74%74%70", 12, buf, sizeof(buf), &esc, &dbl);
    CHECK(strcmp(buf, "http") == 0, "percent decode gave '%s'", buf);
    CHECK(esc, "escape flagged");
    CHECK(!dbl, "single encoding is not double encoding");

    gk_percent_decode("%2568", 5, buf, sizeof(buf), &esc, &dbl);
    CHECK(dbl, "%%25 is double encoding");

    /* A truncated escape at the end must be copied through, not read past. */
    gk_percent_decode("abc%4", 5, buf, sizeof(buf), &esc, &dbl);
    CHECK(strcmp(buf, "abc%4") == 0, "truncated escape passes through, got '%s'", buf);

    /* Output is bounded even when the input is not. */
    char tiny[4];
    gk_percent_decode("aaaaaaaaaa", 10, tiny, sizeof(tiny), NULL, NULL);
    CHECK(strlen(tiny) == 3, "decode respects the cap");

    char a[64], b[64];
    gk_confusable_fold("paypal", 6, a, sizeof(a));
    gk_confusable_fold("paypa1", 6, b, sizeof(b));
    CHECK(strcmp(a, b) == 0, "paypal and paypa1 share a skeleton");
    gk_confusable_fold("pay-pal", 7, b, sizeof(b));
    CHECK(strcmp(a, b) == 0, "a hyphen does not make a different word");
    gk_confusable_fold("amazon", 6, a, sizeof(a));
    gk_confusable_fold("arnazon", 7, b, sizeof(b));
    CHECK(strcmp(a, b) == 0, "rn folds to m");
    gk_confusable_fold("google", 6, a, sizeof(a));
    gk_confusable_fold("g00gle", 6, b, sizeof(b));
    CHECK(strcmp(a, b) == 0, "zero folds to o");
    /* And it must not collapse genuinely different words. */
    gk_confusable_fold("apple", 5, a, sizeof(a));
    gk_confusable_fold("apply", 5, b, sizeof(b));
    CHECK(strcmp(a, b) != 0, "apple and apply are different words");

    CHECK(gk_is_transposition("paypla", 6, "paypal", 6), "adjacent swap detected");
    CHECK(!gk_is_transposition("paypal", 6, "paypal", 6), "identical is not a transposition");
    CHECK(!gk_is_transposition("aaaa", 4, "bbbb", 4), "unrelated is not a transposition");
}

/* ====================================================================== */
/*  NDEF                                                                  */
/* ====================================================================== */

static void test_ndef_uri(void) {
    SECTION("NDEF: URI records");

    char buf[GK_URL_MAX];
    bool trunc = false;

    uint8_t p04[] = {0x04, 'a', '.', 'c', 'o', 'm'};
    gk_ndef_expand_uri(p04, sizeof(p04), buf, sizeof(buf), &trunc);
    CHECK(strcmp(buf, "https://a.com") == 0, "prefix 0x04 gave '%s'", buf);

    uint8_t p00[] = {0x00, 'h', 't', 't', 'p', ':', '/', '/', 'x'};
    gk_ndef_expand_uri(p00, sizeof(p00), buf, sizeof(buf), &trunc);
    CHECK(strcmp(buf, "http://x") == 0, "prefix 0x00 stores the URL whole");

    /* Every code in the table must be reachable and none out of it. */
    for(uint8_t code = 0; code < 0x24; code++) {
        CHECK(gk_ndef_uri_prefix(code) != NULL, "prefix %u exists", code);
    }
    CHECK(gk_ndef_uri_prefix(0x24)[0] == '\0', "out-of-range prefix is empty");
    CHECK(gk_ndef_uri_prefix(255)[0] == '\0', "prefix 255 is empty");

    /* A prefix code the tag invented must not produce a URL out of nowhere. */
    uint8_t pbad[] = {0xEE, 'x'};
    gk_ndef_expand_uri(pbad, sizeof(pbad), buf, sizeof(buf), &trunc);
    CHECK(strcmp(buf, "x") == 0, "unknown prefix contributes nothing, got '%s'", buf);

    /* Control bytes inside a URL are replaced, not carried through. */
    uint8_t pctl[] = {0x04, 'a', 0x00, 'b', '\n', 'c'};
    gk_ndef_expand_uri(pctl, sizeof(pctl), buf, sizeof(buf), &trunc);
    CHECK(strcmp(buf, "https://a?b?c") == 0, "control bytes replaced, got '%s'", buf);

    /* Truncation is reported, not silently performed. */
    char small[10];
    uint8_t plong[64];
    plong[0] = 0x04;
    memset(plong + 1, 'a', sizeof(plong) - 1);
    gk_ndef_expand_uri(plong, sizeof(plong), small, sizeof(small), &trunc);
    CHECK(trunc, "truncation reported");
    CHECK(strlen(small) == sizeof(small) - 1, "truncated to the cap");

    /* Nothing at all */
    gk_ndef_expand_uri(NULL, 0, buf, sizeof(buf), &trunc);
    CHECK(buf[0] == '\0', "NULL payload gives an empty string");
}

static void test_ndef_hostile(void) {
    SECTION("NDEF: hostile input");

    GkTag tag;

    /* A short record claiming more payload than the tag holds. */
    uint8_t over[] = {0x03, 0x08, 0xD1, 0x01, 0xFF, 'U', 0x04, 'a', '.', 'b', 0xFE};
    gk_tag_init(&tag);
    gk_ndef_parse_tlv(over, sizeof(over), &tag);
    CHECK(!tag.has_url, "a record that overruns the buffer yields no URL");

    /* A long record whose 4-byte length claims four gigabytes. */
    uint8_t huge[] = {0x03, 0x0A, 0x01, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 'U', 0x04, 'a', 0xFE};
    gk_tag_init(&tag);
    gk_ndef_parse_tlv(huge, sizeof(huge), &tag);
    CHECK(!tag.has_url, "a 4GB payload length is refused");

    /* A TLV length longer than the memory that was read. */
    uint8_t tlv_over[] = {0x03, 0x40, 0xD1, 0x01, 0x03, 'U', 0x04, 'a'};
    gk_tag_init(&tag);
    gk_ndef_parse_tlv(tlv_over, sizeof(tlv_over), &tag);
    CHECK(tag.malformed, "a short read is reported as malformed");

    /* Header alone, nothing after it. */
    uint8_t stub[] = {0x03};
    gk_tag_init(&tag);
    gk_ndef_parse_tlv(stub, sizeof(stub), &tag);
    CHECK(!tag.has_url, "a bare TLV tag byte yields nothing");

    /* Empty and NULL */
    gk_tag_init(&tag);
    CHECK(!gk_ndef_parse_tlv(NULL, 0, &tag), "NULL buffer refused");
    gk_tag_init(&tag);
    CHECK(!gk_ndef_parse_tlv((const uint8_t*)"", 0, &tag), "empty buffer refused");

    /* A Text record claiming a language code longer than its payload. */
    uint8_t badtext[] = {0x03, 0x06, 0xD1, 0x01, 0x02, 'T', 0x3F, 'e', 0xFE};
    gk_tag_init(&tag);
    gk_ndef_parse_tlv(badtext, sizeof(badtext), &tag);
    CHECK(!tag.has_title, "an impossible language length yields no title");

    /* Smart Poster nested inside Smart Poster inside Smart Poster: the depth
     * guard has to stop this before the stack does. */
    uint8_t deep[128];
    size_t n = 0;
    deep[n++] = 0x03;
    size_t len_at = n++;
    for(int d = 0; d < 5; d++) {
        deep[n++] = 0xD1;
        deep[n++] = 0x02;
        deep[n++] = (uint8_t)(4 * (4 - d) + 8);
        deep[n++] = 'S';
        deep[n++] = 'p';
    }
    deep[n++] = 0xFE;
    deep[len_at] = (uint8_t)(n - 2);
    gk_tag_init(&tag);
    gk_ndef_parse_tlv(deep, n, &tag);
    CHECK(true, "deep nesting returned without exploding");

    /* Fuzz: random bytes must never crash, never overrun (ASan is watching)
     * and never invent a URL longer than the buffer it came from. */
    uint32_t state = 0x1234567u;
    for(int iter = 0; iter < 20000; iter++) {
        uint8_t blob[96];
        size_t blen = 1 + (size_t)(iter % (sizeof(blob) - 1));
        for(size_t i = 0; i < blen; i++) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            blob[i] = (uint8_t)(state >> 7);
        }
        /* Half the time, start it with a plausible NDEF TLV so the fuzzer
         * gets past the first byte and into the record walker. */
        if(iter & 1) {
            blob[0] = 0x03;
            if(blen > 1) blob[1] = (uint8_t)(blen - 2);
        }
        gk_tag_init(&tag);
        gk_ndef_parse_tlv(blob, blen, &tag);
        if(tag.has_url) {
            CHECK(tag.url_len < GK_URL_MAX, "fuzz url length in range");
            CHECK(strlen(tag.url) == tag.url_len, "fuzz url is NUL-terminated correctly");
        }
        CHECK(tag.record_count <= GK_REC_MAX, "fuzz record count bounded");

        /* And whatever came out must survive being graded. */
        GkVerdictResult r;
        gk_verdict_grade(&tag, &r);
        CHECK(r.score <= 100, "fuzz score in range");
    }
}

/* ====================================================================== */
/*  Grading                                                               */
/* ====================================================================== */

static GkVerdictResult grade(const char* url) {
    GkVerdictResult r;
    gk_verdict_grade_url(url, &r);
    return r;
}

static bool has_sig(const GkVerdictResult* r, GkSignalId id) {
    for(uint8_t i = 0; i < r->find_count; i++) {
        if(r->find[i].id == id) return true;
    }
    return false;
}

static void test_signals(void) {
    SECTION("Grading: individual signals");

    GkVerdictResult r;

    r = grade("https://apple.com@id-verify.top/x");
    CHECK(has_sig(&r, GkSigUserinfo), "@ trick detected");
    CHECK(strcmp(r.registrable, "id-verify.top") == 0, "destination is the part after the @");

    r = grade("https://apple.com.id-check.top/signin");
    CHECK(has_sig(&r, GkSigDomainInside), "buried real domain detected");
    r = grade("https://apple.com/signin");
    CHECK(!has_sig(&r, GkSigDomainInside), "the real apple.com is not buried in itself");
    r = grade("https://www.apple.com/signin");
    CHECK(!has_sig(&r, GkSigDomainInside), "www.apple.com is still apple.com");

    r = grade("https://paypa1.com/");
    CHECK(has_sig(&r, GkSigHomoglyph), "paypa1 detected");
    r = grade("https://arnazon.co.uk/");
    CHECK(has_sig(&r, GkSigHomoglyph), "arnazon detected");

    r = grade("https://paypal.secure-login.xyz/");
    CHECK(has_sig(&r, GkSigBrandSubdomain), "brand as subdomain detected");

    r = grade("https://paypal-verify.com/");
    CHECK(has_sig(&r, GkSigBrandCore), "brand glued into a domain detected");

    r = grade("https://netflix.github.io/");
    CHECK(has_sig(&r, GkSigBrandBadTld), "brand on free hosting detected");
    r = grade("https://apple.tk/");
    CHECK(has_sig(&r, GkSigBrandBadTld), "brand on a bargain TLD detected");

    r = grade("https://unrelated.example/paypal/login");
    CHECK(has_sig(&r, GkSigBrandPath), "brand in the path detected");

    r = grade("http://198.51.100.7/");
    CHECK(has_sig(&r, GkSigIpHost), "IP host detected");
    r = grade("https://xn--pple-43d.com/");
    CHECK(has_sig(&r, GkSigPunycode), "punycode detected");

    r = grade("https://ok.com/go?next=https://evil.tld/x");
    CHECK(has_sig(&r, GkSigOpenRedirect), "open redirect detected");
    r = grade("https://ok.com/go?next=%68%74%74%70%73%3A%2F%2Fevil.tld");
    CHECK(has_sig(&r, GkSigOpenRedirect), "encoded open redirect detected");
    r = grade("https://ok.com/go?next=/local/page");
    CHECK(!has_sig(&r, GkSigOpenRedirect), "a relative next= is not a redirect off-site");

    r = grade("https://ok.com/files/update.apk");
    CHECK(has_sig(&r, GkSigDirectDownload), ".apk detected");
    r = grade("https://ok.com/page?file=update.apk");
    CHECK(!has_sig(&r, GkSigDirectDownload), "a filename in a parameter is not the destination");

    r = grade("javascript:alert(1)");
    CHECK(has_sig(&r, GkSigDangerousScheme), "javascript: detected");
    r = grade("tel:+449098790123");
    CHECK(has_sig(&r, GkSigNonWebAction), "tel: detected");

    r = grade("https://claim.metamask-airdrop.xyz/connectwallet");
    CHECK(has_sig(&r, GkSigCryptoKeywords), "wallet words detected");

    r = grade("https://ok.com/parking/pay");
    CHECK(has_sig(&r, GkSigMoneyContext), "money context noted");
    CHECK(gk_signal_info(GkSigMoneyContext)->points == 0, "money context costs nothing");

    r = grade("https://a.b.c.d.example.com/");
    CHECK(has_sig(&r, GkSigDeepSubdomain), "deep subdomain detected");
    r = grade("https://www.shop.example.com/");
    CHECK(!has_sig(&r, GkSigDeepSubdomain), "two labels is ordinary");

    r = grade("https://bit.ly/3xKq9");
    CHECK(has_sig(&r, GkSigShortener), "shortener detected");

    r = grade("https://update.zip/");
    CHECK(has_sig(&r, GkSigFilenameTld), ".zip detected");

    /* A signal must never fire twice for the same fact. */
    r = grade("https://paypal.paypal.paypal.evil.xyz/paypal/paypal");
    uint8_t seen[GkSigCount];
    memset(seen, 0, sizeof(seen));
    for(uint8_t i = 0; i < r.find_count; i++) {
        CHECK(seen[r.find[i].id] == 0, "signal %u fired twice", r.find[i].id);
        seen[r.find[i].id] = 1;
    }
}

static void test_false_positives(void) {
    SECTION("Grading: ordinary tags must survive");

    /* The tags this application will mostly meet are honest ones. If it cries
     * wolf on a pub menu it will be uninstalled before it ever meets a real
     * malicious tag, so these are as important as the detections. */
    static const char* ordinary[] = {
        "https://menu.thegoodpub.co.uk/",
        "https://www.tfl.gov.uk/parking",
        "https://collection.citymuseum.org.uk/hoard",
        "https://www.gov.uk/pay-parking",
        "https://order.pizzaplace.com/table/12",
        "https://www.nationalrail.co.uk/",
        "https://en.wikipedia.org/wiki/Near-field_communication",
        "https://www.apple.com/uk/",
        "https://accounts.google.com/",
        "https://www.amazon.co.uk/",
        "https://open.spotify.com/album/1234",
        "https://maps.google.com/?q=51.5,-0.12",
        "https://www.paypal.com/uk/signin",
        "https://irctc.co.in/booking",
        "https://www.hdfcbank.com/",
    };
    for(size_t i = 0; i < sizeof(ordinary) / sizeof(ordinary[0]); i++) {
        GkVerdictResult r = grade(ordinary[i]);
        CHECK(r.grade <= GkGradeB && r.grade != GkGradeNone,
              "%s graded %s (score %u) -- too harsh for an ordinary link",
              ordinary[i], gk_grade_name(r.grade), r.score);
    }

    /* Domains that contain a brand name legitimately. */
    GkVerdictResult r = grade("https://s3.amazonaws.com/bucket/key");
    CHECK(!has_sig(&r, GkSigBrandCore), "amazonaws is not an impersonation of amazon");
    r = grade("https://fonts.googleapis.com/css");
    CHECK(!has_sig(&r, GkSigHomoglyph) && !has_sig(&r, GkSigBrandCore),
          "googleapis is not an impersonation of google");
}

static void test_bands_and_ceilings(void) {
    SECTION("Grading: bands and ceilings");

    /* The promise the README makes, asserted structurally rather than by
     * example: the always-on ceiling sits below the A+ band, so no input of
     * any kind can reach A+. */
    CHECK(gk_cap_value(GkCapNoSite) < gk_grade_floor(GkGradeAPlus),
          "A+ must be unreachable: ceiling %u vs floor %u",
          gk_cap_value(GkCapNoSite), gk_grade_floor(GkGradeAPlus));

    /* Ceilings must be ordered the way the explanations claim they are. */
    CHECK(gk_cap_value(GkCapShortener) < gk_grade_floor(GkGradeB),
          "a shortener must not reach B");
    CHECK(gk_cap_value(GkCapRewritable) < gk_grade_floor(GkGradeA),
          "an unlocked tag must not reach A");
    CHECK(gk_cap_value(GkCapPlainHttp) < gk_grade_floor(GkGradeB), "plain http must not reach B");
    CHECK(gk_cap_value(GkCapImpersonation) < gk_grade_floor(GkGradeC),
          "impersonation must not reach C");
    CHECK(gk_cap_value(GkCapHostile) < gk_grade_floor(GkGradeD), "a hostile scheme must be F");
    CHECK(gk_cap_value(GkCapPayload) < gk_grade_floor(GkGradeD), "a download must be F");

    /* The spotless case, to show where the top of the scale really is. */
    GkVerdictResult r = grade("https://menu.thegoodpub.co.uk/");
    CHECK(r.raw_score == 100, "a clean URL loses no points, got %u", r.raw_score);
    CHECK(r.score == gk_cap_value(GkCapNoSite), "and is still held at the ceiling");
    CHECK(r.grade == GkGradeA, "which is an A, not an A+");

    /* Every band must be reachable, or the scale is decoration. */
    bool reached[GkGradeF + 1];
    memset(reached, 0, sizeof(reached));
    static const char* spread[] = {
        "https://menu.thegoodpub.co.uk/",
        "https://ok.com/go?next=https://elsewhere.tld",
        "https://bit.ly/3xKq9",
        "http://198.51.100.7/pay",
        "javascript:alert(1)",
    };
    for(size_t i = 0; i < sizeof(spread) / sizeof(spread[0]); i++) {
        reached[grade(spread[i]).grade] = true;
    }
    CHECK(reached[GkGradeA], "A reachable");
    CHECK(reached[GkGradeB], "B reachable");
    CHECK(reached[GkGradeC], "C reachable");
    CHECK(reached[GkGradeD], "D reachable");
    CHECK(reached[GkGradeF], "F reachable");
}

static void test_tag_signals(void) {
    SECTION("Grading: the tag itself");

    GkTag tag;
    GkVerdictResult r;

    /* The same spotless URL, locked and unlocked. This is the whole reason
     * Gatekeeper reads tags rather than strings. */
    gk_demo_build(0, &tag); /* locked cafe menu */
    gk_verdict_grade(&tag, &r);
    CHECK(r.grade == GkGradeA, "a locked, clean tag reaches A, got %s", gk_grade_name(r.grade));
    CHECK(has_sig(&r, GkSigLocked), "locked noted");

    tag.writable = true;
    gk_verdict_grade(&tag, &r);
    CHECK(r.grade == GkGradeB, "the same tag unlocked drops to B, got %s",
          gk_grade_name(r.grade));
    CHECK(r.score <= gk_cap_value(GkCapRewritable), "and is held by the rewritable ceiling");

    /* No URL is a state, not a grade. */
    gk_demo_build(11, &tag); /* blank */
    gk_verdict_grade(&tag, &r);
    CHECK(r.grade == GkGradeNone, "a blank tag has no grade");
    CHECK(r.verdict == GkVerdictNothing, "and no verdict");
    CHECK(r.status == GkTagNoNdef, "and says why");

    /* An AAR is extra instructions arriving with the link. */
    gk_demo_build(8, &tag);
    gk_verdict_grade(&tag, &r);
    CHECK(has_sig(&r, GkSigAar), "AAR detected");
    CHECK(tag.has_aar && strcmp(tag.aar, "com.freegames.installer") == 0,
          "package name read, got '%s'", tag.aar);

    /* The caption says one company, the link goes to another. */
    gk_demo_build(9, &tag);
    gk_verdict_grade(&tag, &r);
    CHECK(tag.has_title, "Smart Poster title parsed");
    CHECK(has_sig(&r, GkSigTitleMismatch), "caption/destination mismatch detected");

    /* A tag whose serial is written into its own URL. */
    gk_tag_init(&tag);
    tag.uid_len = 7;
    static const uint8_t uid[7] = {0x04, 0x9A, 0x2C, 0x11, 0x40, 0x80, 0xC0};
    memcpy(tag.uid, uid, sizeof(uid));
    tag.has_url = true;
    strcpy(tag.url, "https://track.example.com/t?id=049A2C114080C0");
    tag.url_len = (uint16_t)strlen(tag.url);
    tag.status = GkTagOk;
    gk_verdict_grade(&tag, &r);
    CHECK(has_sig(&r, GkSigUidMirror), "UID mirror detected");
}

static void test_demo_tags(void) {
    SECTION("Grading: the demo set");

    /* Each scripted tag exists to demonstrate one thing, and the screenshots
     * in the README are renderings of these results. If a grade moves, the
     * documentation is wrong, so the expected band is pinned here. */
    struct {
        uint8_t index;
        GkGrade at_most; /* the grade must be this or worse */
        GkGrade at_least; /* ...and this or better */
    } expect[] = {
        {0, GkGradeA, GkGradeA}, /* locked cafe menu */
        {1, GkGradeA, GkGradeA}, /* museum Smart Poster */
        {2, GkGradeB, GkGradeB}, /* real meter, unlocked */
        {3, GkGradeF, GkGradeD}, /* the overlay */
        {4, GkGradeF, GkGradeF}, /* @ trick */
        {5, GkGradeC, GkGradeC}, /* shortener */
        {6, GkGradeF, GkGradeD}, /* homoglyph bank */
        {7, GkGradeF, GkGradeD}, /* crypto drainer */
        {8, GkGradeF, GkGradeF}, /* apk installer */
        {9, GkGradeF, GkGradeD}, /* caption mismatch */
        {10, GkGradeC, GkGradeB}, /* premium-rate call */
    };

    for(size_t i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
        GkTag tag;
        CHECK(gk_demo_build(expect[i].index, &tag), "demo %u builds", expect[i].index);
        GkVerdictResult r;
        gk_verdict_grade(&tag, &r);
        CHECK(r.grade >= expect[i].at_least && r.grade <= expect[i].at_most,
              "demo %u (%s) graded %s, expected %s..%s", expect[i].index,
              gk_demo_info(expect[i].index)->name, gk_grade_name(r.grade),
              gk_grade_name(expect[i].at_least), gk_grade_name(expect[i].at_most));
        CHECK(r.find_count > 0, "demo %u produced findings to show", expect[i].index);
    }

    /* Every demo tag must build, parse and grade without exception. */
    for(uint8_t i = 0; i < gk_demo_count(); i++) {
        GkTag tag;
        CHECK(gk_demo_build(i, &tag), "demo %u builds", i);
        CHECK(gk_demo_info(i)->name[0] != '\0', "demo %u is named", i);
        GkVerdictResult r;
        gk_verdict_grade(&tag, &r);
        CHECK(r.score <= 100, "demo %u score in range", i);
    }
}

static void test_invariants(void) {
    SECTION("Grading: invariants over a corpus");

    static const char* corpus[] = {
        "https://menu.thegoodpub.co.uk/",
        "http://plain.example.com/",
        "https://bit.ly/x",
        "https://apple.com@evil.tld/",
        "https://paypa1.com/login",
        "javascript:alert(1)",
        "data:text/html,<script>x</script>",
        "https://a.b.c.d.e.f.g.example.com/very/long/path?a=1&b=2&c=3&d=4&e=5&f=6&g=7",
        "https://198.51.100.7:8443/verify/account/login/secure/update",
        "tel:+441234",
        "mailto:a@b.com",
        "example.com",
        "https://xn--80ak6aa92e.com/",
        "https://evil.github.io/paypal/signin",
        "https://update.zip/invoice",
        "https://ok.com/x.apk",
        "https://claim.wallet-airdrop.top/connectwallet?url=https://evil.tld",
        "//",
        ":",
        "https://",
        "h",
        "https://a.com/%25%25%25",
    };

    for(size_t i = 0; i < sizeof(corpus) / sizeof(corpus[0]); i++) {
        GkVerdictResult r = grade(corpus[i]);

        CHECK(r.score <= r.raw_score, "%s: ceiling raised the score (%u > %u)", corpus[i],
              r.score, r.raw_score);
        CHECK(r.score <= gk_cap_value(GkCapNoSite), "%s: score %u exceeded the always-on ceiling",
              corpus[i], r.score);
        CHECK(r.grade != GkGradeAPlus, "%s: reached A+, which must be unreachable", corpus[i]);
        CHECK(r.find_count <= GK_FIND_MAX, "%s: finding count bounded", corpus[i]);
        CHECK(r.find_total >= r.find_count, "%s: total is not less than shown", corpus[i]);
        CHECK(r.cap_count <= GK_CAP_MAX, "%s: ceiling count bounded", corpus[i]);

        if(r.grade != GkGradeNone) {
            CHECK(r.score >= gk_grade_floor(r.grade), "%s: score %u below its own band %s",
                  corpus[i], r.score, gk_grade_name(r.grade));
        }

        /* Findings are shown worst-first. */
        for(uint8_t f = 0; f + 1 < r.find_count; f++) {
            CHECK(r.find[f].points >= r.find[f + 1].points, "%s: findings out of order",
                  corpus[i]);
        }

        /* Every listed ceiling must be one that actually bound the score. */
        for(uint8_t k = 0; k < r.cap_count; k++) {
            CHECK(gk_cap_value(r.caps[k]) <= r.raw_score,
                  "%s: listed a ceiling that never bit", corpus[i]);
        }

        /* Every finding must have text to show for itself. */
        for(uint8_t f = 0; f < r.find_count; f++) {
            const GkSignalInfo* si = gk_signal_info(r.find[f].id);
            CHECK(si->label[0] != '\0', "%s: finding %u has no label", corpus[i], f);
            CHECK(si->what[0] != '\0', "%s: finding %u has no explanation", corpus[i], f);
            CHECK(si->why[0] != '\0', "%s: finding %u has no reason", corpus[i], f);
        }
    }
}

static void test_metadata(void) {
    SECTION("Metadata: every signal is presentable");

    /* Nothing may reach the screen without words to explain it. */
    for(int id = 1; id < GkSigCount; id++) {
        const GkSignalInfo* si = gk_signal_info((GkSignalId)id);
        CHECK(si->label[0] != '\0', "signal %d has no label", id);
        CHECK(si->what[0] != '\0', "signal %d has no 'what'", id);
        CHECK(si->why[0] != '\0', "signal %d has no 'why'", id);
        CHECK(strlen(si->label) <= 32, "signal %d label too long for the list: '%s'", id,
              si->label);
        CHECK(si->cap <= GkCapCount, "signal %d has an invalid ceiling", id);
        if(si->family == GkFamNote) {
            CHECK(si->points == 0, "signal %d is a note and must cost nothing", id);
        }
    }

    for(int c = 0; c < GkCapCount; c++) {
        CHECK(gk_cap_label((GkCapId)c)[0] != '\0', "ceiling %d has no label", c);
        CHECK(gk_cap_reason((GkCapId)c)[0] != '\0', "ceiling %d has no reason", c);
        CHECK(gk_cap_value((GkCapId)c) <= 100, "ceiling %d out of range", c);
    }

    for(int g = 0; g <= GkGradeF; g++) {
        CHECK(gk_grade_name((GkGrade)g)[0] != '\0', "grade %d has no name", g);
    }
    for(int v = 0; v <= GkVerdictDanger; v++) {
        CHECK(gk_verdict_name((GkVerdict)v)[0] != '\0', "verdict %d has no name", v);
        CHECK(gk_verdict_action((GkVerdict)v)[0] != '\0', "verdict %d has no advice", v);
    }
    for(int f = 0; f <= GkFamNote; f++) {
        CHECK(gk_family_name((GkFamily)f)[0] != '\0', "family %d has no name", f);
    }
    for(int t = 0; t <= GkTechOther; t++) {
        CHECK(gk_tech_name((GkTech)t)[0] != '\0', "tech %d has no name", t);
    }
}

/* ====================================================================== */

int main(void) {
    printf("Gatekeeper engine tests\n");

    test_url_parse();
    test_url_registrable();
    test_url_zones();
    test_url_helpers();
    test_ndef_uri();
    test_ndef_hostile();
    test_signals();
    test_false_positives();
    test_bands_and_ceilings();
    test_tag_signals();
    test_demo_tags();
    test_invariants();
    test_metadata();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
