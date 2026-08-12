#include "gk_reader.h"

#include <furi.h>
#include <bit_buffer.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#include <string.h>

#define POLL_MS 20u

struct GkReader {
    Nfc* nfc;
    FuriThread* thread;
    FuriMutex* mutex;

    volatile bool stop;
    GkReaderState state;

    /* scanner -> worker */
    bool detected;
    size_t scan_num;
    NfcProtocol scan_stack[NfcProtocolNum];

    /* poller -> worker */
    NfcPoller* active_poller;
    bool poll_done;
    bool poll_ok;

    /* The raw NDEF area, filled by whichever path ran. It lives here rather
     * than on the poller thread's stack, which is not a kilobyte deep. */
    uint8_t ndef[GK_NDEF_MAX];
    uint16_t ndef_len;
    bool ndef_is_message; /* Type 4 hands over a bare message, not a TLV */

    GkTag scratch; /* metadata gathered inside the callback */
    GkTag result;
};

static void gk_lock(GkReader* r) {
    furi_mutex_acquire(r->mutex, FuriWaitForever);
}
static void gk_unlock(GkReader* r) {
    furi_mutex_release(r->mutex);
}

/* ------------------------------------------------------------- scanning */

static void gk_scanner_cb(NfcScannerEvent event, void* context) {
    GkReader* r = context;
    if(event.type != NfcScannerEventTypeDetected) return;

    gk_lock(r);
    r->scan_num = event.data.protocol_num;
    if(r->scan_num > NfcProtocolNum) r->scan_num = NfcProtocolNum;
    for(size_t i = 0; i < r->scan_num; i++) {
        r->scan_stack[i] = event.data.protocols[i];
    }
    r->detected = true;
    gk_unlock(r);
}

/* --------------------------------------------------------------- Type 2 */

/* The capability container at page 3 is four bytes: a magic number, a
 * version, the size of the data area in units of eight bytes, and an access
 * byte. The low nibble of the access byte is write permission -- 0 means
 * anybody may still write to this tag, 0x0F means it has been locked
 * forever. That single nibble is the difference between a B and an A here,
 * so it is read rather than assumed. */
static void gk_read_cc(GkReader* r, const MfUltralightData* data) {
    if(data->pages_read < 4) return;
    const uint8_t* cc = data->page[3].data;

    if(cc[0] == 0xE1) {
        r->scratch.capacity = (uint16_t)(cc[2] * 8u);
        r->scratch.size_known = r->scratch.capacity > 0;
        r->scratch.write_known = true;
        r->scratch.writable = (cc[3] & 0x0Fu) == 0x00u;
    } else {
        /* No capability container: the tag was never formatted for NDEF. It
         * may still hold something, but nothing about it is standard. */
        r->scratch.size_known = false;
    }

    /* Static lock bits live in the last two bytes of page 2 and cover the
     * capability container and the first user pages. Any of them set means
     * part of the tag can no longer change, whatever the access nibble says. */
    if(data->pages_read >= 3) {
        const uint8_t* lock = data->page[2].data;
        if(lock[2] != 0x00 || lock[3] != 0x00) {
            r->scratch.write_known = true;
            r->scratch.writable = false;
        }
    }
}

static void gk_read_config(GkReader* r, const MfUltralightData* data) {
    MfUltralightConfigPages* cfg = NULL;
    if(!mf_ultralight_get_config_page(data, &cfg) || !cfg) return;

    /* AUTH0 is the first page that needs a password. When it sits inside the
     * tag, part of the memory is behind a password we will not be asking
     * for -- which means this grade is based on less than the whole tag, and
     * the user is told so. */
    uint16_t total = mf_ultralight_get_pages_total(data->type);
    if(cfg->auth0 < total) r->scratch.pwd_protected = true;
}

static NfcCommand gk_ultralight_cb(NfcGenericEvent event, void* context) {
    GkReader* r = context;
    MfUltralightPollerEvent* ev = event.event_data;
    NfcCommand cmd = NfcCommandContinue;

    switch(ev->type) {
    case MfUltralightPollerEventTypeRequestMode:
        /* Read. Never anything else. */
        ev->data->poller_mode = MfUltralightPollerModeRead;
        break;

    case MfUltralightPollerEventTypeAuthRequest:
        /* Gatekeeper does not carry passwords and does not guess them. If
         * part of the tag is locked away, that is a finding, not a puzzle. */
        ev->data->auth_context.skip_auth = true;
        break;

    case MfUltralightPollerEventTypeCardLocked:
        gk_lock(r);
        r->scratch.pwd_protected = true;
        gk_unlock(r);
        break;

    case MfUltralightPollerEventTypeReadSuccess: {
        const MfUltralightData* data =
            (const MfUltralightData*)nfc_poller_get_data(r->active_poller);

        gk_lock(r);
        r->scratch.tech = GkTechType2;
        const char* name = mf_ultralight_get_device_name(data, NfcDeviceNameTypeShort);
        if(name) {
            strncpy(r->scratch.tech_name, name, sizeof(r->scratch.tech_name) - 1);
        }

        size_t uid_len = 0;
        const uint8_t* uid = mf_ultralight_get_uid(data, &uid_len);
        if(uid && uid_len) {
            if(uid_len > GK_UID_MAX) uid_len = GK_UID_MAX;
            memcpy(r->scratch.uid, uid, uid_len);
            r->scratch.uid_len = (uint8_t)uid_len;
        }

        gk_read_cc(r, data);
        gk_read_config(r, data);

        /* The user data area starts at page 4. Read what the capability
         * container advertised, bounded by what actually came back -- a tag
         * that claims more than it handed over does not get to decide how
         * far we walk. */
        uint16_t avail = 0;
        if(data->pages_read > 4) {
            avail = (uint16_t)((data->pages_read - 4) * MF_ULTRALIGHT_PAGE_SIZE);
        }
        uint16_t want = r->scratch.size_known ? r->scratch.capacity : avail;
        if(want > avail) want = avail;
        if(want > GK_NDEF_MAX) want = GK_NDEF_MAX;

        for(uint16_t i = 0; i < want; i++) {
            r->ndef[i] = data->page[4 + (i / MF_ULTRALIGHT_PAGE_SIZE)]
                             .data[i % MF_ULTRALIGHT_PAGE_SIZE];
        }
        r->ndef_len = want;
        r->ndef_is_message = false;

        r->poll_ok = true;
        r->poll_done = true;
        gk_unlock(r);
        cmd = NfcCommandStop;
        break;
    }

    case MfUltralightPollerEventTypeReadFailed:
        gk_lock(r);
        r->poll_ok = false;
        r->poll_done = true;
        gk_unlock(r);
        cmd = NfcCommandStop;
        break;

    default:
        break;
    }

    return cmd;
}

/* --------------------------------------------------------------- Type 4
 *
 * No pages: a tiny file system, reached with the same APDUs a phone uses.
 * Select the NDEF application, read the capability container file to find
 * out which file holds the message and whether it is still writable, then
 * read the message itself. Every one of these is a read command.
 */

#define GK_APDU_OK(sw1, sw2) ((sw1) == 0x90 && (sw2) == 0x00)

static bool gk_apdu(
    Iso14443_4aPoller* poller,
    BitBuffer* tx,
    BitBuffer* rx,
    const uint8_t* cmd,
    size_t cmd_len,
    size_t* out_len) {
    bit_buffer_reset(tx);
    bit_buffer_copy_bytes(tx, cmd, cmd_len);
    if(iso14443_4a_poller_send_block(poller, tx, rx) != Iso14443_4aErrorNone) return false;

    size_t n = bit_buffer_get_size_bytes(rx);
    if(n < 2) return false;
    uint8_t sw1 = bit_buffer_get_byte(rx, n - 2);
    uint8_t sw2 = bit_buffer_get_byte(rx, n - 1);
    if(out_len) *out_len = n - 2;
    return GK_APDU_OK(sw1, sw2);
}

static bool gk_read_binary(
    Iso14443_4aPoller* poller,
    BitBuffer* tx,
    BitBuffer* rx,
    uint16_t offset,
    uint8_t len,
    uint8_t* dst,
    size_t* got) {
    const uint8_t cmd[5] = {0x00, 0xB0, (uint8_t)(offset >> 8), (uint8_t)(offset & 0xFF), len};
    size_t n = 0;
    if(!gk_apdu(poller, tx, rx, cmd, sizeof(cmd), &n)) return false;
    if(n > len) n = len;
    for(size_t i = 0; i < n; i++) dst[i] = bit_buffer_get_byte(rx, i);
    if(got) *got = n;
    return true;
}

static bool gk_type4_read(GkReader* r, Iso14443_4aPoller* poller) {
    BitBuffer* tx = bit_buffer_alloc(64);
    BitBuffer* rx = bit_buffer_alloc(256);
    bool ok = false;

    do {
        /* 1. SELECT the NDEF tag application by name. */
        static const uint8_t select_app[] = {
            0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
        if(!gk_apdu(poller, tx, rx, select_app, sizeof(select_app), NULL)) break;

        /* 2. SELECT the capability container file, E103. */
        static const uint8_t select_cc[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03};
        if(!gk_apdu(poller, tx, rx, select_cc, sizeof(select_cc), NULL)) break;

        /* 3. Read it. The file control TLV at offset 7 gives the NDEF file's
         *    id, its maximum size, and its read and write permissions. */
        uint8_t cc[15] = {0};
        size_t cc_len = 0;
        if(!gk_read_binary(poller, tx, rx, 0, sizeof(cc), cc, &cc_len)) break;
        if(cc_len < 15) break;

        uint16_t file_id = (uint16_t)((cc[9] << 8) | cc[10]);
        uint16_t max_size = (uint16_t)((cc[11] << 8) | cc[12]);
        uint8_t write_access = cc[14];

        gk_lock(r);
        r->scratch.capacity = max_size;
        r->scratch.size_known = max_size > 0;
        r->scratch.write_known = true;
        /* 0x00 is "anyone may write"; 0xFF is "nobody, ever". Anything else
         * is a proprietary condition we will not pretend to understand, so
         * it is treated as still writable -- the cautious direction. */
        r->scratch.writable = (write_access != 0xFF);
        gk_unlock(r);

        /* 4. SELECT the NDEF file itself. */
        const uint8_t select_ndef[] = {
            0x00, 0xA4, 0x00, 0x0C, 0x02, (uint8_t)(file_id >> 8), (uint8_t)(file_id & 0xFF)};
        if(!gk_apdu(poller, tx, rx, select_ndef, sizeof(select_ndef), NULL)) break;

        /* 5. The first two bytes are the message length. */
        uint8_t nlen_buf[2] = {0};
        size_t got = 0;
        if(!gk_read_binary(poller, tx, rx, 0, 2, nlen_buf, &got) || got < 2) break;
        uint16_t nlen = (uint16_t)((nlen_buf[0] << 8) | nlen_buf[1]);
        if(nlen == 0) {
            gk_lock(r);
            r->ndef_len = 0;
            r->ndef_is_message = true;
            gk_unlock(r);
            ok = true;
            break;
        }
        if(nlen > GK_NDEF_MAX) nlen = GK_NDEF_MAX;

        /* 6. Read the message, in chunks a single APDU can carry. */
        uint16_t done = 0;
        while(done < nlen) {
            uint16_t chunk = (uint16_t)(nlen - done);
            if(chunk > 0xF0) chunk = 0xF0;
            uint8_t buf[0xF0];
            size_t n = 0;
            if(!gk_read_binary(poller, tx, rx, (uint16_t)(2 + done), (uint8_t)chunk, buf, &n)) {
                break;
            }
            if(n == 0) break;
            gk_lock(r);
            for(size_t i = 0; i < n && done + i < GK_NDEF_MAX; i++) r->ndef[done + i] = buf[i];
            gk_unlock(r);
            done = (uint16_t)(done + n);
        }

        gk_lock(r);
        r->ndef_len = done;
        r->ndef_is_message = true;
        gk_unlock(r);
        ok = done > 0;
    } while(false);

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return ok;
}

static NfcCommand gk_iso4a_cb(NfcGenericEvent event, void* context) {
    GkReader* r = context;
    const Iso14443_4aPollerEvent* ev = event.event_data;
    NfcCommand cmd = NfcCommandContinue;

    if(ev->type == Iso14443_4aPollerEventTypeReady) {
        const Iso14443_4aData* data =
            (const Iso14443_4aData*)nfc_poller_get_data(r->active_poller);
        const Iso14443_3aData* base = iso14443_4a_get_base_data(data);

        gk_lock(r);
        r->scratch.tech = GkTechType4;
        strncpy(r->scratch.tech_name, "ISO-DEP", sizeof(r->scratch.tech_name) - 1);
        size_t uid_len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(base, &uid_len);
        if(uid && uid_len) {
            if(uid_len > GK_UID_MAX) uid_len = GK_UID_MAX;
            memcpy(r->scratch.uid, uid, uid_len);
            r->scratch.uid_len = (uint8_t)uid_len;
        }
        gk_unlock(r);

        /* Still inside the callback: this is where talking to the card is
         * legal, and the only place it is. */
        bool ok = gk_type4_read(r, (Iso14443_4aPoller*)event.instance);

        gk_lock(r);
        r->poll_ok = ok;
        r->poll_done = true;
        gk_unlock(r);
        cmd = NfcCommandStop;
    } else if(ev->type == Iso14443_4aPollerEventTypeError) {
        gk_lock(r);
        r->poll_ok = false;
        r->poll_done = true;
        gk_unlock(r);
        cmd = NfcCommandStop;
    }
    return cmd;
}

/* --------------------------------------------------------------- worker */

static bool gk_wait_flag(GkReader* r, const bool* flag) {
    for(;;) {
        if(r->stop) return false;
        gk_lock(r);
        bool done = *flag;
        gk_unlock(r);
        if(done) return true;
        furi_delay_ms(POLL_MS);
    }
}

static void gk_run_poller(GkReader* r, NfcProtocol protocol, NfcGenericCallback cb) {
    gk_lock(r);
    r->poll_done = false;
    r->poll_ok = false;
    gk_unlock(r);

    r->active_poller = nfc_poller_alloc(r->nfc, protocol);
    nfc_poller_start(r->active_poller, cb, r);
    gk_wait_flag(r, &r->poll_done);
    nfc_poller_stop(r->active_poller);
    nfc_poller_free(r->active_poller);
    r->active_poller = NULL;
}

static bool gk_stack_has(const NfcProtocol* stack, size_t num, NfcProtocol want) {
    for(size_t i = 0; i < num; i++) {
        if(stack[i] == want) return true;
    }
    return false;
}

static int32_t gk_reader_worker(void* context) {
    GkReader* r = context;

    r->nfc = nfc_alloc();

    gk_lock(r);
    r->detected = false;
    r->state = GkReaderSearching;
    gk_tag_init(&r->scratch);
    r->ndef_len = 0;
    gk_unlock(r);

    /* --- 1. sweep for a tag --- */
    NfcScanner* scanner = nfc_scanner_alloc(r->nfc);
    nfc_scanner_start(scanner, gk_scanner_cb, r);
    bool got = gk_wait_flag(r, &r->detected);
    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    if(!got) {
        nfc_free(r->nfc);
        r->nfc = NULL;
        return 0;
    }

    gk_lock(r);
    size_t num = r->scan_num;
    NfcProtocol stack[NfcProtocolNum];
    for(size_t i = 0; i < num; i++) stack[i] = r->scan_stack[i];
    r->state = GkReaderReading;
    gk_unlock(r);

    /* --- 2. read it, by whichever route its technology needs --- */
    bool read_ok = false;
    if(gk_stack_has(stack, num, NfcProtocolMfUltralight)) {
        gk_run_poller(r, NfcProtocolMfUltralight, gk_ultralight_cb);
        gk_lock(r);
        read_ok = r->poll_ok;
        gk_unlock(r);
    } else if(gk_stack_has(stack, num, NfcProtocolIso14443_4a)) {
        gk_run_poller(r, NfcProtocolIso14443_4a, gk_iso4a_cb);
        gk_lock(r);
        read_ok = r->poll_ok;
        gk_unlock(r);
    } else if(gk_stack_has(stack, num, NfcProtocolMfClassic)) {
        /* Classic can hold NDEF, but only behind sector keys. Gatekeeper does
         * not crack keys -- that is a different application with a different
         * conscience -- so it says so and stops. */
        gk_lock(r);
        r->scratch.tech = GkTechClassic;
        strncpy(r->scratch.tech_name, "MIFARE Classic", sizeof(r->scratch.tech_name) - 1);
        r->scratch.status = GkTagNeedsKeys;
        gk_unlock(r);
    } else {
        gk_lock(r);
        r->scratch.tech = GkTechOther;
        const char* pname = nfc_device_get_protocol_name(num ? stack[0] : NfcProtocolInvalid);
        if(pname) strncpy(r->scratch.tech_name, pname, sizeof(r->scratch.tech_name) - 1);
        r->scratch.status = GkTagUnreadable;
        gk_unlock(r);
    }

    /* --- 3. parse what came back --- */
    gk_lock(r);
    GkTag tag = r->scratch;
    uint16_t len = r->ndef_len;
    bool is_message = r->ndef_is_message;
    gk_unlock(r);

    if(read_ok) {
        if(len == 0) {
            tag.status = GkTagNoNdef;
        } else if(is_message) {
            if(gk_ndef_parse_message(r->ndef, len, &tag, 0)) {
                tag.used = len;
                tag.status = tag.has_url ? GkTagOk : GkTagNoUrl;
            } else {
                tag.status = GkTagNoNdef;
            }
        } else {
            gk_ndef_parse_tlv(r->ndef, len, &tag);
        }
    } else if(tag.status == GkTagOk) {
        tag.status = GkTagUnreadable;
    }

    gk_lock(r);
    r->result = tag;
    r->state = GkReaderDone;
    gk_unlock(r);

    nfc_free(r->nfc);
    r->nfc = NULL;
    return 0;
}

/* ------------------------------------------------------------ lifecycle */

GkReader* gk_reader_alloc(void) {
    GkReader* r = malloc(sizeof(GkReader));
    memset(r, 0, sizeof(GkReader));
    r->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    r->state = GkReaderIdle;
    gk_tag_init(&r->result);
    return r;
}

void gk_reader_free(GkReader* r) {
    furi_assert(r);
    gk_reader_stop(r);
    furi_mutex_free(r->mutex);
    free(r);
}

void gk_reader_start(GkReader* r) {
    furi_assert(r);
    gk_reader_stop(r); /* join any prior run */

    r->stop = false;
    gk_lock(r);
    r->state = GkReaderSearching;
    r->detected = false;
    r->poll_done = false;
    r->ndef_len = 0;
    gk_tag_init(&r->scratch);
    gk_unlock(r);

    /* 8 KB: the grader's working set is the best part of two, and it runs on
     * this thread rather than blocking the GUI with it. */
    r->thread = furi_thread_alloc_ex("GatekeeperReader", 8 * 1024, gk_reader_worker, r);
    furi_thread_start(r->thread);
}

void gk_reader_stop(GkReader* r) {
    furi_assert(r);
    r->stop = true;
    if(r->thread) {
        furi_thread_join(r->thread);
        furi_thread_free(r->thread);
        r->thread = NULL;
    }
    gk_lock(r);
    if(r->state != GkReaderDone) r->state = GkReaderIdle;
    gk_unlock(r);
}

GkReaderState gk_reader_state(GkReader* r) {
    furi_assert(r);
    gk_lock(r);
    GkReaderState s = r->state;
    gk_unlock(r);
    return s;
}

bool gk_reader_get(GkReader* r, GkTag* out) {
    furi_assert(r);
    furi_assert(out);
    gk_lock(r);
    bool ready = (r->state == GkReaderDone);
    if(ready) *out = r->result;
    gk_unlock(r);
    return ready;
}
