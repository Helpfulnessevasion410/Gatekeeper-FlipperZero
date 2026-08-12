/* The radio half: the only file in Gatekeeper that touches a tag.
 *
 * Everything here reads. There is no command in this file that writes a
 * page, locks a tag, authenticates against one, or changes a single byte of
 * anybody's NFC tag -- the point of the application is to look at a tag
 * without giving it your phone, and writing to a stranger's tag would be a
 * different and much worse program.
 *
 * Two families of tag carry URLs in the street, and both are handled:
 *
 *   Type 2  - NTAG213/215/216 and MIFARE Ultralight. This is what is on the
 *             back of a poster or under a table. The NDEF message sits in a
 *             TLV wrapper starting at page 4, with a capability container at
 *             page 3 that says how big the data area is and whether the tag
 *             is still writable.
 *
 *   Type 4  - DESFire and other ISO-DEP smartcards, and the card emulation
 *             mode a phone uses. There are no pages here; the message is
 *             behind a small file system reached with APDUs.
 *
 * The work happens on its own thread because a tag read takes as long as it
 * takes and the GUI must keep drawing. The result crosses back under a mutex.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gk_ndef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GkReaderIdle = 0,
    GkReaderSearching, /* field is up, nothing in it yet */
    GkReaderReading, /* a tag is present and being read */
    GkReaderDone, /* result is available */
} GkReaderState;

typedef struct GkReader GkReader;

GkReader* gk_reader_alloc(void);
void gk_reader_free(GkReader* r);

/** Raise the field and start looking. Any previous run is joined first. */
void gk_reader_start(GkReader* r);
/** Ask the worker to stop and wait for it. Safe to call when not running. */
void gk_reader_stop(GkReader* r);

GkReaderState gk_reader_state(GkReader* r);

/** Copy out the parsed tag. Returns false until the state is GkReaderDone. */
bool gk_reader_get(GkReader* r, GkTag* out);

#ifdef __cplusplus
}
#endif
