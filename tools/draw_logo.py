#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Sill's logo as SVG (assets/logo.svg): render it at 1024 x 1024 to assets/logo.png (any browser, rsvg-convert or
cairosvg), then run tools/si_logo.py for the power-on screen.
Usage: draw_logo.py [ACCENT_COLOUR] [OUT.svg]"""
# The look (family style):
# black ground, a ring, audio bars, a line-art fish in one accent colour + cream,
# music notes, a little knob box (four knobs in one row, like the FM-1), a wide italic name.
# Sill's own touches: hot magenta accent with a laser-cyan second colour, club laser beams
# fanning out behind the ring, a slim herring with a deeply forked tail, and a fat 90s
# dance-pop wordmark (magenta-to-cyan gradient, white rim, deep drop shadow).
import math, sys

ACC = sys.argv[1] if len(sys.argv) > 1 else "#ff3cc7"   # the accent colour (hot magenta)
OUT = sys.argv[2] if len(sys.argv) > 2 else str(__import__("pathlib").Path(__file__).resolve().parent.parent / "assets" / "logo.svg")
LASER = "#33d6ff"                                       # second colour: laser cyan
CREAM, INK, BG = "#fff0fa", "#07040a", "#000000"
W = H = 1024
CX, CY, R = 512, 440, 362

# ---- letters on a 6 x 7 grid, chamfered like the rest of the family
L = {
    "S": [[(0.9,0),(6,0),(6,1.6),(1.7,1.6),(1.7,2.7),(5.1,2.7),(6,3.6),(6,6.1),(5.1,7),
           (0,7),(0,5.4),(4.3,5.4),(4.3,4.3),(0.9,4.3),(0,3.4),(0,0.9)]],
    "I": [[(0,0),(1.8,0),(1.8,7),(0,7)]],
    "L": [[(0,0),(1.7,0),(1.7,5.4),(5.4,5.4),(5.4,7),(0.9,7),(0,6.1)]],
}
WID = {"S": 6, "I": 1.8, "L": 5.4}
GAP, WORD, SK = 0.8, "SILL", 0.30
total = sum(WID[c] for c in WORD) + GAP * (len(WORD) - 1)
U = 23.0
TY0 = 812


def word_path(dx=0.0, dy=0.0):
    x0 = (W - total * U - 7 * U * SK) / 2
    d, x = [], 0.0
    for c in WORD:
        for poly in L[c]:
            pts = [f"{x0 + (x + px) * U + (7 - py) * U * SK + dx:.1f},{TY0 + py * U + dy:.1f}" for px, py in poly]
            d.append("M" + " L".join(pts) + " Z")
        x += WID[c] + GAP
    return " ".join(d)


# ---- club lasers fanning out from below the ring, behind everything
lasers = []
for i, a in enumerate(range(-160, -19, 14)):
    r = math.radians(a)
    col = LASER if i % 2 else ACC
    lasers.append(f'<line x1="512" y1="800" x2="{512 + 900*math.cos(r):.0f}" y2="{800 + 900*math.sin(r):.0f}" '
                  f'stroke="{col}" stroke-width="3" opacity="0.35"/>')

# ---- audio bars, a waveform round the centre line, clipped to the ring
bars = []
n = 24
for i in range(n):
    x = 176 + i * 29
    t = i / (n - 1)
    h = 60 + 230 * abs(math.sin(t * 9.1 + 0.3)) * (0.55 + 0.45 * math.sin(t * math.pi))
    bars.append(f'<rect x="{x}" y="{CY - 10 - h * 0.62:.0f}" width="13" height="{h:.0f}" rx="2"/>')

# ---- the sill (Atlantic herring), facing right: slim torpedo body, jutting lower jaw, deeply forked tail
body = ("M 886 368 "
        "C 864 336, 824 316, 764 306 "                  # snout up to the crown
        "C 650 288, 500 292, 392 318 "                  # the long straight back
        "C 330 334, 288 348, 258 358 "                  # back down to the tail stem
        "Q 206 300, 140 246 Q 170 318, 194 370 "        # upper tail lobe
        "Q 170 422, 140 494 Q 206 438, 258 384 "        # lower tail lobe
        "C 310 398, 390 424, 490 434 "                  # belly
        "C 610 446, 744 432, 822 402 "                  # throat
        "C 856 390, 876 382, 886 368 Z")
belly = ("M 822 402 C 744 432, 610 446, 490 434 C 390 424, 310 398, 258 384 "
         "C 360 398, 470 408, 580 408 C 690 408, 780 396, 842 380 Z")
back_band = ("M 764 306 C 650 288, 500 292, 392 318 C 330 334, 288 348, 258 358 "
             "C 320 346, 420 330, 520 326 C 640 322, 720 326, 790 332 Z")   # the herring's blue back
dorsal = "M 468 300 Q 476 254, 512 226 Q 540 262, 590 292 Z"
d_rays = "".join(f'<path d="M {x0} {y0} L {x1} {y1}"/>' for x0, y0, x1, y1 in
                 [(482, 296, 500, 240), (506, 294, 512, 232), (530, 292, 526, 244), (554, 291, 544, 258)])
pelvic = "M 520 436 L 494 478 L 556 438 Z"
anal = "M 338 404 L 314 444 L 382 416 Z"
pect = "M 744 386 Q 696 404, 652 398 Q 690 372, 740 370 Z"
gill = "M 752 312 C 730 346, 732 384, 754 414"
lat = "M 740 352 C 620 352, 460 350, 280 368"     # silver lateral flash
mouth = "M 886 368 C 868 376, 848 380, 832 378"   # jutting lower jaw
scales = "".join(
    f'<path d="M {x} {y} q 12 10 0 20"/>'
    for x in range(330, 720, 40) for y in range(330, 410, 28)
    if (x - 530) ** 2 / 210 ** 2 + (y - 365) ** 2 / 48 ** 2 < 1)


def note(x, y, s, double=False):
    """An eighth (or a pair of eighths) note."""
    g = [f'<ellipse cx="{x}" cy="{y}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x} {y})"/>',
         f'<rect x="{x + 9*s:.1f}" y="{y - 58*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>']
    if double:
        x2 = x + 44 * s
        g += [f'<ellipse cx="{x2}" cy="{y - 6*s}" rx="{13*s}" ry="{9.5*s}" transform="rotate(-22 {x2} {y - 6*s})"/>',
              f'<rect x="{x2 + 9*s:.1f}" y="{y - 64*s:.1f}" width="{4.5*s:.1f}" height="{58*s:.1f}"/>',
              f'<polygon points="{x + 9*s:.1f},{y - 58*s:.1f} {x2 + 13.5*s:.1f},{y - 64*s:.1f} {x2 + 13.5*s:.1f},{y - 52*s:.1f} {x + 9*s:.1f},{y - 46*s:.1f}"/>']
    else:
        g.append(f'<path d="M {x + 13.5*s:.1f} {y - 58*s:.1f} q {22*s:.1f} {14*s:.1f} {16*s:.1f} {38*s:.1f} '
                 f'q {-2*s:.1f} {-16*s:.1f} {-16*s:.1f} {-22*s:.1f} z"/>')
    return "".join(g)


def glint(x, y, r):
    """A four-point sparkle."""
    k = r * 0.22
    return (f'<path d="M {x} {y - r} Q {x + k} {y - k} {x + r} {y} Q {x + k} {y + k} {x} {y + r} '
            f'Q {x - k} {y + k} {x - r} {y} Q {x - k} {y - k} {x} {y - r} Z"/>')


def knob(x, y):
    ticks = "".join(
        f'<line x1="{x + 30*math.cos(a):.1f}" y1="{y + 30*math.sin(a):.1f}" x2="{x + 37*math.cos(a):.1f}" y2="{y + 37*math.sin(a):.1f}"/>'
        for a in [math.radians(d) for d in range(135, 406, 45)])
    return (f'<g stroke="{INK}" stroke-width="3" stroke-linecap="round">{ticks}</g>'
            f'<circle cx="{x}" cy="{y}" r="22" fill="{ACC}" stroke="{INK}" stroke-width="5"/>'
            f'<line x1="{x}" y1="{y}" x2="{x}" y2="{y - 16}" stroke="{INK}" stroke-width="5" stroke-linecap="round"/>')


word = word_path()
shadow = word_path(10, 10)
BX, BY, BW, BH = 512 - 170, 624, 340, 104
svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs>
<linearGradient id="pop" gradientUnits="userSpaceOnUse" x1="0" y1="{TY0}" x2="0" y2="{TY0 + 7*U}"><stop offset="0" stop-color="#ffffff"/><stop offset="0.16" stop-color="#ffc4ef"/><stop offset="0.45" stop-color="{ACC}"/><stop offset="0.7" stop-color="#8a3cff"/><stop offset="1" stop-color="{LASER}"/></linearGradient>
<clipPath id="ring"><circle cx="{CX}" cy="{CY}" r="{R - 14}"/></clipPath></defs>
<rect width="{W}" height="{H}" fill="{BG}"/>
<g stroke-linecap="round">{"".join(lasers)}</g>
<circle cx="{CX}" cy="{CY}" r="{R}" fill="#0a0610" stroke="{ACC}" stroke-width="11"/>
<g clip-path="url(#ring)" fill="{ACC}">{"".join(bars)}</g>

<!-- the box with four knobs in one row, wired to the ring -->
<path d="M {BX} {BY + BH/2} C {BX - 60} {BY + BH/2}, {BX - 70} {BY - 40}, {BX - 70} {BY - 100}" fill="none" stroke="{ACC}" stroke-width="7"/>
<path d="M {BX + BW} {BY + BH/2} C {BX + BW + 60} {BY + BH/2}, {BX + BW + 70} {BY - 40}, {BX + BW + 70} {BY - 100}" fill="none" stroke="{ACC}" stroke-width="7"/>
<rect x="{BX}" y="{BY}" width="{BW}" height="{BH}" rx="16" fill="{CREAM}" stroke="{ACC}" stroke-width="9"/>
{"".join(knob(BX + 50 + i * 80, BY + 52) for i in range(4))}

<!-- the sill -->
<g transform="rotate(-6 520 370) translate(-8 0)">
  <path d="{body}" fill="none" stroke="{BG}" stroke-width="26" stroke-linejoin="round"/>
  <path d="{dorsal}" fill="none" stroke="{BG}" stroke-width="20" stroke-linejoin="round"/>
  <path d="{dorsal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <g fill="none" stroke="{CREAM}" stroke-width="3" stroke-linecap="round">{d_rays}</g>
  <path d="{anal}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{pelvic}" fill="{INK}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{body}" fill="{INK}" stroke="{ACC}" stroke-width="10" stroke-linejoin="round"/>
  <path d="{back_band}" fill="{LASER}" opacity="0.55"/>
  <path d="{belly}" fill="{CREAM}"/>
  <g fill="none" stroke="{ACC}" stroke-width="3" opacity="0.6">{scales}</g>
  <path d="{lat}" fill="none" stroke="{CREAM}" stroke-width="4" stroke-linecap="round" opacity="0.8"/>
  <path d="M 244 350 Q 200 300, 162 268" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="M 244 392 Q 200 440, 162 474" fill="none" stroke="{CREAM}" stroke-width="5" stroke-linecap="round"/>
  <path d="{gill}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <path d="{pect}" fill="{CREAM}" stroke="{ACC}" stroke-width="6" stroke-linejoin="round"/>
  <path d="{mouth}" fill="none" stroke="{ACC}" stroke-width="6" stroke-linecap="round"/>
  <circle cx="812" cy="340" r="21" fill="{CREAM}" stroke="{ACC}" stroke-width="5"/>
  <circle cx="817" cy="339" r="10" fill="{INK}"/>
  <circle cx="821" cy="334" r="3" fill="{CREAM}"/>
</g>

<g fill="{CREAM}">{note(756, 186, 0.75)}{note(806, 152, 0.9, True)}{note(880, 226, 0.7)}</g>
<g fill="{LASER}">{glint(724, 120, 16)}{glint(912, 160, 12)}{glint(860, 92, 9)}</g>

<g stroke-linecap="round"><line x1="60" y1="{TY0 + 40}" x2="200" y2="{TY0 + 40}" stroke="{ACC}" stroke-width="7"/><line x1="36" y1="{TY0 + 80}" x2="186" y2="{TY0 + 80}" stroke="{LASER}" stroke-width="5" opacity="0.8"/><line x1="84" y1="{TY0 + 118}" x2="174" y2="{TY0 + 118}" stroke="{ACC}" stroke-width="4" opacity="0.5"/><line x1="836" y1="{TY0 + 50}" x2="976" y2="{TY0 + 50}" stroke="{LASER}" stroke-width="5" opacity="0.8"/><line x1="824" y1="{TY0 + 92}" x2="954" y2="{TY0 + 92}" stroke="{ACC}" stroke-width="4" opacity="0.5"/></g>
<path d="{shadow}" fill="#2a0a4a" stroke="#2a0a4a" stroke-width="22" stroke-linejoin="round"/>
<path d="{word}" fill="none" stroke="{BG}" stroke-width="26" stroke-linejoin="round"/>
<path d="{word}" fill="none" stroke="#ffffff" stroke-width="14" stroke-linejoin="round"/>
<path d="{word}" fill="url(#pop)" stroke="#2a0a4a" stroke-width="3" stroke-linejoin="round"/>
<g fill="#ffffff">{glint(W/2 + 236, TY0 + 6, 32)}{glint(W/2 - 240, TY0 + 150, 18)}{glint(W/2 - 30, TY0 + 4, 12)}</g>
</svg>'''
open(OUT, "w").write(svg)
print("ok", OUT)
