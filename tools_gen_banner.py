#!/usr/bin/env python3
"""Render the Gatekeeper GitHub banner + social-preview card.

The motif is the product in one picture: an address, with the part your phone
actually obeys picked out of it and everything the attacker wrote for free
left dim around it. Behind it, the portcullis -- the thing you look through
before you go through it.

Everything is cold and instrument-like except the destination, which is the
one hot thing on the page, because it is the one thing that matters.

Supersampled, then LANCZOS-downsampled.
"""
from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os

OUT = os.path.join(os.path.dirname(__file__), "images")
os.makedirs(OUT, exist_ok=True)

BOLD = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
BLACK_F = "/System/Library/Fonts/Supplemental/Arial Black.ttf"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
REG = "/System/Library/Fonts/Supplemental/Arial.ttf"

# palette - cold instrument, one hot destination
BG_TOP = (8, 10, 15)
BG_BOT = (14, 17, 27)
INSTR = (88, 196, 255)  # the reader: cold, measured
THREAT = (255, 78, 66)  # where you actually land
GRAY = (146, 156, 172)
WHITE = (240, 246, 252)
DIM = (32, 42, 58)

SS = 2  # supersample


def font(path, px):
    try:
        return ImageFont.truetype(path, px)
    except OSError:
        return ImageFont.truetype(BOLD, px)


def vgradient(w, h):
    img = Image.new("RGB", (w, h), BG_TOP)
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line(
            [(0, y), (w, y)],
            fill=tuple(int(BG_TOP[i] + (BG_BOT[i] - BG_TOP[i]) * t) for i in range(3)),
        )
    return img


def build_gate(size, cx, cy, gw):
    """The portcullis: an arch, two posts, bars and rails, half raised."""
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    gh = int(gw * 1.15)
    x0, y0 = cx - gw // 2, cy - gh // 2
    lw = max(2, int(gw * 0.035))

    # the arch
    d.rounded_rectangle(
        [x0, y0, x0 + gw, y0 + gh], radius=int(gw * 0.12), outline=INSTR + (235,), width=lw
    )

    # the grid, lifted clear of the bottom third: the gate is open
    in_l, in_r = x0 + lw * 2, x0 + gw - lw * 2
    in_t, in_b = y0 + lw * 2, y0 + int(gh * 0.62)
    for i in range(5):
        bx = in_l + int((in_r - in_l) * i / 4)
        d.line([bx, in_t, bx, in_b], fill=INSTR + (200,), width=lw)
    for k in (0.28, 0.60):
        by = in_t + int((in_b - in_t) * k)
        d.line([in_l, by, in_r, by], fill=INSTR + (200,), width=lw)
    # the spiked bottom edge of the raised grid
    for i in range(5):
        bx = in_l + int((in_r - in_l) * i / 4)
        d.polygon(
            [(bx - lw, in_b), (bx + lw, in_b), (bx, in_b + int(gw * 0.05))],
            fill=INSTR + (200,),
        )

    # a tag being held up to it, in the opening underneath
    tw_, th_ = int(gw * 0.30), int(gw * 0.20)
    tx, ty = cx - tw_ // 2, y0 + int(gh * 0.76)
    d.rounded_rectangle([tx, ty, tx + tw_, ty + th_], radius=lw, outline=THREAT + (240,), width=lw)
    d.rounded_rectangle(
        [tx + lw * 2, ty + lw * 2, tx + tw_ - lw * 2, ty + th_ - lw * 2],
        radius=lw,
        outline=THREAT + (150,),
        width=max(1, lw // 2),
    )
    return layer


def build_url_strip(size, x, y, mono, scale):
    """The address, with the destination in a hot box and the rest dimmed.

    This is the verdict screen's whole idea, at poster size.
    """
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    pre, dest, post = "https://apple.com@", "id-verify.top", "/signin"
    cw = mono.getlength("0")

    # dim prefix
    d.text((x, y), pre, font=mono, fill=GRAY + (255,), anchor="ls")
    dx = x + cw * len(pre)

    # The hot box. It starts where the prefix ends rather than overlapping it:
    # the whole point of the picture is that the two are different pieces of
    # the same string, so the boundary between them has to be exact.
    pad = int(10 * scale)
    box_w = cw * len(dest)
    top = y - int(mono.size * 0.80) - pad // 2
    bot = y + int(mono.size * 0.22) + pad // 2
    right = dx + box_w + 2 * pad
    d.rounded_rectangle([dx, top, right, bot], radius=int(6 * scale), fill=THREAT + (255,))
    d.text((dx + pad, y), dest, font=mono, fill=(12, 10, 12, 255), anchor="ls")

    d.text((right + pad, y), post, font=mono, fill=GRAY + (255,), anchor="ls")
    return layer, (dx, top, right, bot)


def render(path, W, H, layout="wide"):
    w, h = W * SS, H * SS
    img = vgradient(w, h).convert("RGBA")
    scale = SS

    if layout == "wide":
        gate = build_gate((w, h), int(w * 0.82), int(h * 0.42), int(h * 0.44))
    else:
        gate = build_gate((w, h), int(w * 0.5), int(h * 0.22), int(h * 0.24))

    img.alpha_composite(gate.filter(ImageFilter.GaussianBlur(6 * SS)))
    img.alpha_composite(gate)

    tx = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    td = ImageDraw.Draw(tx)

    # Explicit vertical rhythm per layout rather than fractions of the height:
    # the address strip and the strapline are the two blocks that collide, and
    # they only stay apart if both are pinned.
    if layout == "wide":
        x, kicker_y, title_y, title_px = 70 * SS, 48 * SS, 72 * SS, 92 * SS
        anchor = "la"
        url_y, foot_y = 322 * SS, 368 * SS
    else:
        x, kicker_y, title_y, title_px = w // 2, 296 * SS, 322 * SS, 100 * SS
        anchor = "ma"
        url_y, foot_y = 552 * SS, 606 * SS

    f_kick = font(MONO, 21 * SS)
    f_title = font(BLACK_F, title_px)
    f_tag = font(BOLD, 33 * SS)
    f_sub = font(REG, 22 * SS)
    f_foot = font(MONO, 20 * SS)

    td.text(
        (x, kicker_y),
        "FLIPPER ZERO  ·  NFC TAG PHISHING SCANNER",
        font=f_kick,
        fill=INSTR,
        anchor=anchor,
    )
    td.text((x + 4 * SS, title_y + 4 * SS), "GATEKEEPER", font=f_title, fill=THREAT + (130,),
            anchor=anchor)
    td.text((x, title_y), "GATEKEEPER", font=f_title, fill=WHITE, anchor=anchor)

    tag_y = title_y + title_px + 22 * SS
    td.text((x, tag_y), "Scan before you tap.", font=f_tag, fill=INSTR, anchor=anchor)
    td.text(
        (x, tag_y + 42 * SS),
        "Reads the tag's link and shows you where your phone really goes.",
        font=f_sub,
        fill=GRAY,
        anchor=anchor,
    )

    img.alpha_composite(tx)

    # the address strip
    mono_px = 29 * SS if layout == "wide" else 27 * SS
    f_url = font(MONO, mono_px)
    url_x = 70 * SS if layout == "wide" else int(w * 0.5 - f_url.getlength("0") * 19)
    strip, boxrect = build_url_strip((w, h), url_x, url_y, f_url, scale)
    img.alpha_composite(strip.filter(ImageFilter.GaussianBlur(5 * SS)))
    img.alpha_composite(strip)

    # a caption under the hot box, so the picture explains itself
    cd = ImageDraw.Draw(img)
    f_cap = font(MONO, 17 * SS)
    cx_mid = (boxrect[0] + boxrect[2]) // 2
    cd.text(
        (cx_mid, boxrect[3] + 14 * SS),
        "where your phone actually goes",
        font=f_cap,
        fill=THREAT,
        anchor="ma",
    )

    fd = ImageDraw.Draw(img)
    fd.line([(70 * SS, foot_y), (w - 70 * SS, foot_y)], fill=DIM, width=2 * SS)
    fd.text(
        (70 * SS, foot_y + 10 * SS),
        "github.com/at0m-b0mb/Gatekeeper-FlipperZero",
        font=f_foot,
        fill=GRAY,
    )
    fd.text((w - 70 * SS, foot_y + 10 * SS), "MIT · by at0m-b0mb", font=f_foot, fill=GRAY,
            anchor="ra")

    img.convert("RGB").resize((W, H), Image.LANCZOS).save(path)
    print("wrote", path)


if __name__ == "__main__":
    render(os.path.join(OUT, "banner.png"), 1280, 420, layout="wide")
    render(os.path.join(OUT, "social-preview.png"), 1280, 640, layout="card")
