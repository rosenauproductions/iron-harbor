# Iron Harbor logo generator: wordmark from the game's own stroke font (src/gfx.h), emblem drawn in SVG.
import re, sys
src = open('src/gfx.h').read()
glyph = {}
for ch, s in re.findall(r"def\('(.)',\s*\"([^\"]*)\"\)", src):
    strokes = []
    for part in s.split(' '):
        pts = [(int(part[i]), int(part[i+1])) for i in range(0, len(part) - 1, 2)]
        if len(pts) == 1: pts.append((pts[0][0], pts[0][1] + 0.2))
        if pts: strokes.append(pts)
    glyph[ch] = strokes

def word(text, x, y, size, track=1.0):
    u = size / 6.0; out = []
    for c in text:
        for st in glyph.get(c, []):
            out.append('M' + ' L'.join(f'{x + px*u:.1f},{y + py*u:.1f}' for px, py in st))
        x += size * track
    return ' '.join(out), x

def width(text, size, track=1.0): return len(text) * size * track - size * track / 3

W, H = 1180, 440
cx, cy, R = 215, 220, 165
title, tsz = 'HARBOR', 124
tx = 430
p1, _ = word('IRON', tx, 40, tsz, 1.02)
p2, _ = word(title, tx, 192, tsz, 1.02)
p_title = p1 + ' ' + p2
tag = 'NAVAL STRATEGY  1-4 ADMIRALS'
tagsz = 30
tagsz = 25
p_tag, _ = word(tag, tx + 4, 378, tagsz, 0.95)
tw = width(title, tsz, 1.02)

# anchor: ring at top, shank, stock, curved arms with flukes
a = []
a.append(f'M{cx},{cy-95} L{cx},{cy+78}')                          # shank
a.append(f'M{cx-48},{cy-55} L{cx+48},{cy-55}')                    # stock
a.append(f'M{cx-92},{cy+18} Q{cx-80},{cy+104} {cx},{cy+92} Q{cx+80},{cy+104} {cx+92},{cy+18}')   # arms
a.append(f'M{cx-92},{cy+18} L{cx-112},{cy+40} M{cx-92},{cy+18} L{cx-70},{cy+30}')   # left fluke
a.append(f'M{cx+92},{cy+18} L{cx+112},{cy+40} M{cx+92},{cy+18} L{cx+70},{cy+30}')   # right fluke
anchor = ' '.join(a)

svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
<defs>
 <filter id="glow" x="-20%" y="-40%" width="140%" height="180%">
  <feGaussianBlur in="SourceGraphic" stdDeviation="7" result="b1"/>
  <feGaussianBlur in="SourceGraphic" stdDeviation="16" result="b2"/>
  <feMerge><feMergeNode in="b2"/><feMergeNode in="b1"/><feMergeNode in="SourceGraphic"/></feMerge>
 </filter>
 <linearGradient id="steel" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#d8f6ff"/><stop offset="1" stop-color="#4fcfff"/></linearGradient>
 <radialGradient id="disc" cx="0.5" cy="0.45" r="0.6"><stop offset="0" stop-color="#0e3a66" stop-opacity="0.95"/><stop offset="1" stop-color="#031226" stop-opacity="0.95"/></radialGradient>
 <clipPath id="c"><circle cx="{cx}" cy="{cy}" r="{R-6}"/></clipPath>
</defs>
<!-- emblem: sonar disc -->
<circle cx="{cx}" cy="{cy}" r="{R}" fill="url(#disc)"/>
<g clip-path="url(#c)">
 <path d="M{cx},{cy} L{cx+R*0.94:.0f},{cy-R*0.34:.0f} A{R} {R} 0 0 0 {cx+R*0.55:.0f},{cy-R*0.83:.0f} Z" fill="#46d2ff" opacity="0.16"/>
 <g stroke="#3aa8e0" stroke-width="2" fill="none" opacity="0.45">
  <circle cx="{cx}" cy="{cy}" r="{R*0.33:.0f}"/><circle cx="{cx}" cy="{cy}" r="{R*0.66:.0f}"/>
  <path d="M{cx-R},{cy} L{cx+R},{cy} M{cx},{cy-R} L{cx},{cy+R}"/>
 </g>
 <path d="M0,{cy+120} Q{cx-110},{cy+100} {cx-40},{cy+122} T{cx+120},{cy+120} T{cx+260},{cy+118}" stroke="#5fd9ff" stroke-width="3" fill="none" opacity="0.5"/>
 <circle cx="{cx+70}" cy="{cy-82}" r="14" fill="#ffb04a" opacity="0.25"/><circle cx="{cx+70}" cy="{cy-82}" r="6" fill="#ffc46a"/>
 <circle cx="{cx-88}" cy="{cy-40}" r="5" fill="#ffb04a" opacity="0.8"/>
</g>
<g filter="url(#glow)" fill="none" stroke-linecap="round" stroke-linejoin="round">
 <circle cx="{cx}" cy="{cy}" r="{R}" stroke="#46d2ff" stroke-width="7"/>
 <circle cx="{cx}" cy="{cy}" r="{R+16}" stroke="#46d2ff" stroke-width="2" stroke-dasharray="10 12" opacity="0.7"/>
 <circle cx="{cx}" cy="{cy-112}" r="17" stroke="url(#steel)" stroke-width="10"/>
 <path d="{anchor}" stroke="url(#steel)" stroke-width="14"/>
</g>
<!-- wordmark: dark outline first so it reads on light themes too -->
<g fill="none" stroke-linecap="round" stroke-linejoin="round" stroke="#04101e" opacity="0.85">
 <path d="{p_title}" stroke-width="27"/>
 <path d="M{tx},{352} L{tx+tw:.0f},{352}" stroke-width="13"/>
 <path d="{p_tag}" stroke-width="11"/>
</g>
<g filter="url(#glow)" fill="none" stroke-linecap="round" stroke-linejoin="round">
 <path d="{p_title}" stroke="url(#steel)" stroke-width="15"/>
 <path d="M{tx},{352} L{tx+tw:.0f},{352}" stroke="#ffb04a" stroke-width="5"/>
 <path d="{p_tag}" stroke="#ffcf7a" stroke-width="4.5"/>
</g>
</svg>'''
open('art/logo.svg', 'w').write(svg)
print('wordmark width', round(tw), 'ends at', round(tx + tw))
