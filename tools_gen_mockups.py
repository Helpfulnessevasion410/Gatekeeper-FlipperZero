#!/usr/bin/env python3
"""Render Flipper-style mock screenshots (128x64, orange backlight) for the README.

Two rules make these worth having:

1. They mirror the on-device draw code in views/*.c constant for constant, so a
   layout collision shows up here before it ships. Text is positioned by
   BASELINE (PIL anchor "ls"/"rs"/"ms") because canvas_draw_str takes y as the
   baseline; canvas_draw_str_aligned with AlignCenter vertically is anchor "mm".

2. The *content* is not made up. `make -C test dump` runs the twelve scripted
   tags through the real parser and the real grader and writes
   test/demo_dump.json; every grade, score, finding, ceiling and highlight
   offset below is read out of that file. In particular the inverted band on
   the URL screen is drawn from the parser's own zone offsets, so a mockup
   showing the highlight over the wrong characters would mean the application
   does too.

    make -C test dump && python3 tools_gen_mockups.py
"""
from PIL import Image, ImageFont, ImageDraw
import json
import os
import subprocess

from tools_gen_icons import GLYPHS

S = 6  # upscale factor
W, H = 128, 64
BG = (255, 130, 0)  # flipper backlight orange
FG = (10, 8, 4)  # near-black pixels
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "images")
DUMP = os.path.join(HERE, "test", "demo_dump.json")
os.makedirs(OUT, exist_ok=True)

MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

f_sec = ImageFont.truetype(MONO, 7 * S - 2)   # FontSecondary
f_pri = ImageFont.truetype(BOLD, 8 * S)       # FontPrimary
f_key = ImageFont.truetype(MONO, 10 * S - 4)  # FontKeyboard: 6px advance


def load_dump():
    if not os.path.exists(DUMP):
        subprocess.run(["make", "-C", "test", "dump"], cwd=HERE, check=True)
    with open(DUMP) as fh:
        return json.load(fh)


TAGS = load_dump()


def tag(name):
    for t in TAGS:
        if t["name"] == name:
            return t
    raise KeyError(f"{name!r} is not in the demo dump: {[t['name'] for t in TAGS]}")


# ---------------- primitives, matching canvas_* semantics ----------------


def canvas():
    img = Image.new("RGB", (W * S, H * S), BG)
    return img, ImageDraw.Draw(img)


def L(v):
    return int(round(v * S))


def line(d, x0, y0, x1, y1, col=FG, w=2):
    d.line([L(x0), L(y0), L(x1), L(y1)], fill=col, width=w)


def box(d, x, y, w, h, col=FG):
    d.rectangle([L(x), L(y), L(x + w) - 1, L(y + h) - 1], fill=col)


def frame(d, x, y, w, h, col=FG, lw=2):
    d.rectangle([L(x), L(y), L(x + w) - 1, L(y + h) - 1], outline=col, width=lw)


def rframe(d, x, y, w, h, r, col=FG, lw=2):
    d.rounded_rectangle(
        [L(x), L(y), L(x + w) - 1, L(y + h) - 1], radius=L(r), outline=col, width=lw
    )


def rbox(d, x, y, w, h, r, col=FG):
    d.rounded_rectangle([L(x), L(y), L(x + w) - 1, L(y + h) - 1], radius=L(r), fill=col)


def circle(d, cx, cy, r, col=FG, lw=2):
    d.ellipse([L(cx - r), L(cy - r), L(cx + r), L(cy + r)], outline=col, width=lw)


def text(d, x, y, s, fnt=f_sec, col=FG, anchor="ls"):
    """y is the BASELINE, matching canvas_draw_str."""
    d.text((L(x), L(y)), s, font=fnt, fill=col, anchor=anchor)


def tw(s, fnt=f_sec):
    """String width in Flipper pixels (mirrors canvas_string_width)."""
    return fnt.getlength(s) / S


def elide(s, max_w, fnt=f_sec):
    """Mirrors gk_ui_elide, tilde and all."""
    if tw(s, fnt) <= max_w:
        return s
    while len(s) > 1 and tw(s + "~", fnt) > max_w:
        s = s[:-1]
    return s + "~"


def wrap(s, max_w, max_lines, fnt=f_sec):
    """Mirrors gk_ui_wrap: break on separators, split unbreakable runs."""
    out, i, n = [], 0, len(s)
    while i < n and len(out) < max_lines:
        take, last_break = 0, 0
        while i + take < n and take < 39:
            probe = s[i : i + take + 1]
            if tw(probe, fnt) > max_w:
                break
            if s[i + take] in " /.-?&":
                last_break = take + 1
            take += 1
        if i + take < n and last_break > 0 and take > last_break:
            take = last_break
        take = max(take, 1)
        out.append(s[i : i + take])
        i += take
        while i < n and s[i] == " ":
            i += 1
    return out


def glyph10(d, x, y, name):
    """The real 10x10 icon pixels fbt compiles into the .fap."""
    for gy, row in enumerate(GLYPHS[name]):
        for gx, ch in enumerate(row[:10]):
            if ch == "#":
                d.rectangle([L(x + gx), L(y + gy), L(x + gx + 1) - 1, L(y + gy + 1) - 1], fill=FG)


# ---------------- shared chrome (views/gk_ui.c) ----------------


def header(d, title, right=None):
    box(d, 0, 0, 128, 13)
    text(d, 3, 10, title, f_pri, BG)
    if right:
        text(d, 125, 10, right, f_sec, BG, anchor="rs")


def hint(d, left=None, right=None):
    line(d, 0, 53, 127, 53)
    if left:
        text(d, 2, 62, left)
    if right:
        text(d, 126, 62, right, anchor="rs")


# gk_ui_glyph: 14x18 stencil letters, 3px stroke.
GW, GH, ST = 14, 18, 3


def stencil(d, x, y, c, col=FG):
    def bx(bx_, by_, bw_, bh_):
        box(d, x + bx_, y + by_, bw_, bh_, col)

    if c == "A":
        for i in range(ST):
            line(d, x + GW // 2 - 1 + i, y, x - 1 + i, y + GH - 1, col)
            line(d, x + GW // 2 - 1 + i, y, x + GW - ST + i, y + GH - 1, col)
        bx(3, GH - 8, GW - 6, ST)
    elif c == "B":
        bx(0, 0, ST, GH)
        bx(0, 0, GW - 3, ST)
        bx(0, (GH - ST) // 2, GW - 3, ST)
        bx(0, GH - ST, GW - 3, ST)
        bx(GW - 3, ST, ST, (GH - ST) // 2 - ST)
        bx(GW - 3, (GH + ST) // 2, ST, (GH - ST) // 2 - ST)
    elif c == "C":
        bx(ST, 0, GW - ST, ST)
        bx(0, ST, ST, GH - 2 * ST)
        bx(ST, GH - ST, GW - ST, ST)
    elif c == "D":
        bx(0, 0, ST, GH)
        bx(0, 0, GW - 3, ST)
        bx(0, GH - ST, GW - 3, ST)
        bx(GW - 3, ST, ST, GH - 2 * ST)
    elif c == "F":
        bx(0, 0, ST, GH)
        bx(0, 0, GW - 1, ST)
        bx(0, (GH - ST) // 2, GW - 5, ST)
    else:
        bx(2, (GH - ST) // 2, GW - 4, ST)


BW, BH = 28, 30


def badge(d, x, y, grade):
    invert = grade in ("D", "F")
    if invert:
        rbox(d, x, y, BW, BH, 3)
        col = BG
    else:
        rframe(d, x, y, BW, BH, 3)
        rframe(d, x + 1, y + 1, BW - 2, BH - 2, 2)
        col = FG
    stencil(d, x + (BW - GW) // 2, y + (BH - GH) // 2, grade[0], col)
    line(d, x + 5, y + 3, x + BW - 6, y + 3, col)
    line(d, x + 5, y + BH - 4, x + BW - 6, y + BH - 4, col)


# ---------------- screens ----------------


def screen_splash():
    img, d = canvas()
    GX, GY, GWD, GHT = 34, 2, 60, 36
    rframe(d, GX, GY, GWD, GHT, 4)
    line(d, GX + 1, GY + GHT - 1, GX + GWD - 2, GY + GHT - 1)
    # Fully lifted: the frame is empty and the name is underneath.
    text(d, 64, 51, "GATEKEEPER", f_pri, anchor="ms")
    text(d, 64, 62, "Scan before you tap", anchor="ms")
    # A hint of the portcullis still clearing the top of the arch.
    for i in range(5):
        bx = GX + 5 + i * ((GX + GWD - 4 - (GX + 3) - 4) // 4)
        line(d, bx, GY + 3, bx, GY + 6)
    return img


def submenu(d, title, items, selected):
    """Flipper's submenu module: header, then 16px rows, selected inverted."""
    text(d, 4, 12, title, f_pri)
    y = 16
    for i, label in enumerate(items):
        if i == selected:
            rbox(d, 0, y, 128, 16, 3)
            text(d, 6, y + 11, label, f_sec, BG)
        else:
            text(d, 6, y + 11, label, f_sec)
        y += 16
        if y > 64:
            break


def screen_menu():
    img, d = canvas()
    submenu(d, "Gatekeeper", ["Scan a tag", "Recent scans", "How the tricks work"], 0)
    return img


def screen_scan():
    img, d = canvas()
    header(d, "Hold tag to the back")
    # draw_flipper(6, 18)
    rframe(d, 6, 18, 26, 30, 3)
    rframe(d, 10, 22, 18, 10, 1)
    circle(d, 19, 40, 5)
    box(d, 19, 40, 1, 1)
    # draw_tag(100, 20)
    rframe(d, 100, 20, 20, 26, 2)
    rframe(d, 103, 24, 14, 18, 1)
    rframe(d, 105, 26, 10, 14, 1)
    line(d, 107, 28, 107, 37)
    # field arcs leaving the Flipper at (34, 33)
    for r in (10, 19, 28):
        d.arc(
            [L(34 - r), L(33 - r), L(34 + r), L(33 + r)],
            start=-60,
            end=60,
            fill=FG,
            width=2,
        )
    text(d, 64, 62, "Searching for a tag", anchor="ms")
    return img


def verdict(t):
    """views/verdict_view.c, constant for constant."""
    img, d = canvas()
    header(d, t["verdict"], f"{t['score']}/100")
    badge(d, 3, 15, t["grade"])
    text(d, 36, 24, "Your phone goes to:")
    rbox(d, 36 - 2, 26, 128 - 36, 15, 2)
    dom = elide(t["registrable"] or t["host"] or "(no host)", 128 - 36 - 6, f_pri)
    text(d, 36 + 1, 26 + 11, dom, f_pri, BG)
    n, c = t["find_total"], len(t["caps"])
    info = f"{n} finding{'' if n == 1 else 's'}"
    if c:
        info += f", {c} ceiling{'' if c == 1 else 's'}"
    text(d, 3, 51, info)
    hint(d, "OK  Why", "> URL   v Tag")
    return img


def screen_url(t):
    """views/url_view.c: 20 monospace columns, the destination inverted."""
    img, d = canvas()
    header(d, "The full address")
    url, z = t["url"], t["zones"]
    cols, cw, top, lh = 20, 6, 15, 9
    reg0, reg1 = z["reg_off"], z["reg_off"] + z["reg_len"]

    for row in range(4):
        start = row * cols
        if start >= len(url):
            break
        end = min(start + cols, len(url))
        for i in range(start, end):
            col = i - start
            x, y = 2 + col * cw, top + row * lh
            inside = reg0 <= i < reg1
            if inside:
                box(d, x, y, cw, lh - 1)
            text(d, x, y + lh - 2, url[i], f_key, BG if inside else FG)

    line(d, 0, 53, 127, 53)
    box(d, 2, 56, 7, 7)
    text(d, 12, 62, "= where your phone goes")
    return img


def screen_findings(t):
    """views/findings_view.c: findings, then the ceilings, worst first."""
    img, d = canvas()
    header(d, "Why this grade", f"{t['score']}/100")
    rows = [(f["label"], f"-{f['points']}" if f["points"] else "note") for f in t["findings"]]
    rows += [(c["label"], f"<{c['value']}") for c in t["caps"]]
    for i, (label, right) in enumerate(rows[:4]):
        y = 14 + i * 10   # FV_TOP, FV_ROW_H
        if i == 0:
            rbox(d, 0, y, 124, 9, 2)
            col = BG
        else:
            col = FG
        rw = tw(right)
        text(d, 3, y + 7, elide(label, 118 - rw - 6), f_sec, col)
        text(d, 120, y + 7, right, f_sec, col, anchor="rs")
    if len(rows) > 4:
        box(d, 126, 14, 2, 38)
    hint(d, "OK  Explain this", "Back")
    return img


def detail(title, right, blocks):
    """views/detail_view.c: evidence, what, why it matters, what to do."""
    img, d = canvas()
    header(d, elide(title, 100, f_pri), right)
    lines = []
    for heading, body in blocks:
        if not body:
            continue
        if lines:
            lines.append(None)
        if heading:
            lines.append(("head", heading))
        lines += [("body", l) for l in wrap(body, 122, 20)]
    for i, item in enumerate(lines[:4]):
        y = 15 + i * 9 + 7
        if item is None:
            continue
        kind, s = item
        text(d, 2, y, s, f_pri if kind == "head" else f_sec)
    if len(lines) > 4:
        box(d, 126, 15, 2, 36 - 8)
    hint(d, "^v Scroll", "Back")
    return img


def screen_tag(t):
    """views/tag_view.c."""
    img, d = canvas()
    header(d, "The tag itself")
    rows = [("head", t["tech"])]
    if t["uid"]:
        rows.append(("body", f"Serial  {t['uid']}"))
    if t["capacity"]:
        rows.append(("body", f"Space   {t['capacity']} bytes"))
    if t["used"]:
        rows.append(("body", f"Used    {t['used']} bytes"))
    rows.append(("body", "Locked  " + ("no - anyone can rewrite" if t["writable"] else "yes")))
    for i, (kind, s) in enumerate(rows[:4]):
        y = 14 + i * 9 + 8   # TV_TOP, TV_ROW_H
        text(d, 2, y, s, f_pri if kind == "head" else f_sec)
    box(d, 126, 14, 2, 36)
    hint(d, "^v Scroll", "Back")
    return img


def marked(d, x, y, s, a, b, mark=True):
    """learn_view's draw_marked: monospace with one section inverted."""
    cw = 6
    if mark and b > a:
        box(d, x + a * cw, y - 8, (b - a) * cw, 10)
    for i, ch in enumerate(s):
        inside = mark and a <= i < b
        text(d, x + i * cw, y, ch, f_key, BG if inside else FG)


def screen_learn_at():
    img, d = canvas()
    header(d, "3/6  The @ trick")
    marked(d, 2, 30, "apple.com@evil.tld", 10, 18)
    text(d, 2, 44, "...the phone obeys this.")
    text(d, 64, 54, "Everything before the @ is", anchor="ms")
    text(d, 64, 62, "a username. It is thrown away.", anchor="ms")
    return img


def screen_learn_sub():
    img, d = canvas()
    header(d, "4/6  Name in the wrong place")
    marked(d, 2, 30, "paypal.com.pay-x.top", 11, 20)
    text(d, 2, 44, "...but you land here.")
    text(d, 64, 54, "Anyone can put any name in", anchor="ms")
    text(d, 64, 62, "front of their own domain.", anchor="ms")
    return img


def screen_learn_limits():
    img, d = canvas()
    header(d, "6/6  What this cannot see")
    rframe(d, 8, 19, 30, 24, 2)
    text(d, 13, 34, "tag")
    rframe(d, 84, 19, 34, 24, 2)
    text(d, 92, 34, "website")
    for x in range(40, 60, 4):
        line(d, x, 31, x + 2, 31)
    line(d, 66, 25, 78, 37)
    line(d, 78, 25, 66, 37)
    text(d, 64, 54, "Gatekeeper reads the tag.", anchor="ms")
    text(d, 64, 62, "The page behind it is unknown.", anchor="ms")
    return img


def screen_settings():
    img, d = canvas()
    text(d, 4, 12, "Settings", f_pri)
    items = [("Sound", "On"), ("Vibrate", "On"), ("LED", "On")]
    y = 16
    for i, (label, value) in enumerate(items):
        if i == 0:
            rbox(d, 0, y, 128, 16, 3)
            col = BG
        else:
            col = FG
        text(d, 6, y + 11, label, f_sec, col)
        text(d, 122, y + 11, "< " + value + " >", f_sec, col, anchor="rs")
        y += 16
    return img


def screen_history():
    img, d = canvas()
    picks = [tag("Poster, @ trick"), tag("Parking meter"), tag("Cafe menu")]
    y = 10
    for i, t in enumerate(picks):
        text(d, 2, y, f"{t['grade']}  {t['verdict']}", f_pri)
        text(d, 2, y + 9, f"1{4+i}:0{i*2+3}  {elide(t['registrable'], 110)}")
        y += 21
    return img


# ---------------- composite ----------------


def contact_sheet(paths, cols=4, pad=10):
    ims = [Image.open(p) for p in paths]
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w + (cols + 1) * pad, rows * h + (rows + 1) * pad), (16, 16, 20))
    for i, im in enumerate(ims):
        r, c = divmod(i, cols)
        sheet.paste(im, (pad + c * (w + pad), pad + r * (h + pad)))
    return sheet


def save(img, name):
    path = os.path.join(OUT, name + ".png")
    img.save(path)
    print("wrote", path)
    return path


if __name__ == "__main__":
    at = tag("Poster, @ trick")
    overlay = tag("Meter, overlaid")
    shortened = tag("Flyer, shortened")

    made = []
    made.append(save(screen_splash(), "screen_splash"))
    made.append(save(screen_menu(), "screen_menu"))
    made.append(save(screen_scan(), "screen_scan"))
    made.append(save(verdict(tag("Cafe menu")), "screen_verdict_a"))
    made.append(save(verdict(tag("Parking meter")), "screen_verdict_b"))
    made.append(save(verdict(overlay), "screen_verdict_d"))
    made.append(save(verdict(at), "screen_verdict_f"))
    made.append(save(screen_url(at), "screen_url"))
    made.append(save(screen_url(overlay), "screen_url_overlay"))
    made.append(save(screen_findings(at), "screen_findings"))

    f0 = at["findings"][0]
    made.append(
        save(
            detail(
                f0["label"],
                f"-{f0['points']}",
                [
                    (None, f'"{f0["evidence"]}"' if f0["evidence"] else None),
                    (None, f0["what"]),
                    ("Why it matters", f0["why"]),
                    ("What to do", f0["advice"]),
                ],
            ),
            "screen_detail",
        )
    )

    cap = shortened["caps"][0]
    made.append(
        save(
            detail(
                cap["label"],
                f"max {cap['value']}",
                [
                    (None, "This is a ceiling, not a deduction."),
                    ("Why it matters", cap["reason"]),
                ],
            ),
            "screen_ceiling",
        )
    )

    made.append(save(screen_tag(overlay), "screen_tag"))
    made.append(save(screen_learn_at(), "screen_learn_at"))
    made.append(save(screen_learn_sub(), "screen_learn_sub"))
    made.append(save(screen_learn_limits(), "screen_learn_limits"))
    made.append(save(screen_history(), "screen_history"))
    made.append(save(screen_settings(), "screen_settings"))

    sheet = contact_sheet(
        [
            os.path.join(OUT, n + ".png")
            for n in (
                "screen_scan",
                "screen_verdict_f",
                "screen_url",
                "screen_findings",
                "screen_verdict_a",
                "screen_detail",
                "screen_tag",
                "screen_learn_at",
            )
        ]
    )
    sheet.save(os.path.join(OUT, "screens.png"))
    print("wrote", os.path.join(OUT, "screens.png"))
