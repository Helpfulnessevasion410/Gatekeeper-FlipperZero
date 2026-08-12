# Changelog

## v1.0 — 2026-08-11

First release.

- Reads NFC Type 2 (NTAG213/215/216, MIFARE Ultralight) and Type 4 (ISO-DEP)
  tags on the Flipper's onboard NFC. Read-only: nothing writes, locks or
  authenticates.
- Hand-rolled NDEF parser — TLV wrapper, records, the well-known URI prefix
  table, Smart Posters with their nested message, Android Application Records —
  written defensively, because a tag on a lamp post is not a trusted input.
- 41 grading signals in four families, with points, ceilings and plain-English
  explanations of what was seen, why it matters and what to do.
- Verdict screen leads with the registrable domain rather than the address, and
  the full-address screen highlights the same characters using the parser's own
  offsets.
- Seven ceilings, one of them always in effect: A+ is unreachable because the
  application can read the tag and not the website.
- Tag details, six-panel animated walkthrough, scan history, CSV scan log.
- Twelve demo tags assembled as real NDEF bytes and graded by the real engine.
- 40,912 host checks under ASan/UBSan, including 20,000 rounds of parser fuzz.
