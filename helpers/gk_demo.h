/* Twelve tags off a wall, without the wall.
 *
 * Every demo tag here is assembled as real NDEF bytes -- the TLV wrapper,
 * the record headers, the URI prefix codes -- and then handed to the same
 * gk_ndef_parse_tlv() the radio feeds. Nothing takes a shortcut into the
 * parsed structure. That matters for two reasons: the demo cannot drift away
 * from what the application really does with a tag, and the README
 * screenshots are renderings of genuinely parsed and genuinely graded tags
 * rather than pictures somebody drew of a good outcome.
 *
 * It also means the host tests get twelve realistic end-to-end cases for
 * free, which is how the Smart Poster nesting bug was found.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gk_ndef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name; /* menu label */
    const char* where; /* where you would meet this tag */
} GkDemoInfo;

uint8_t gk_demo_count(void);
const GkDemoInfo* gk_demo_info(uint8_t index);

/** Assemble demo tag `index` as bytes and parse it. Returns false if the
 *  index is out of range. */
bool gk_demo_build(uint8_t index, GkTag* tag);

#ifdef __cplusplus
}
#endif
