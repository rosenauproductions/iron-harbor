# Iron Harbor: HUD data reference

Everything the game knows that a HUD could show, grouped by where it lives. Use it to design layouts. "Now" marks what
the current HUD shows; everything else is available but not yet on screen.

---

## 1. Screens and layouts

**Resolution.** The game renders at 720p, 1080p, 1440p or 4K (Options > Resolution); the box TV is 4K.
All HUD sizes scale with the screen height (1 unit = height / 1080), so design at 1920 x 1080.

**Screens:** Title, Lobby (admirals), Options, How to Play, the Game itself, Pause, Results.

**In the game, the screen is split according to how many battles are running.** One shared TV, 1-4 local players.

| Situation | Layout |
|---|---|
| No battle | Map full screen. Top bar (side boxes), bottom left (control hints per human), cursors and messages on the map |
| 1 battle, no free human (the map pauses) | Battle full screen |
| 1 battle, another human still free | Battle on the left 66% of the width. Map top-right (34% wide, 16:9). Roster panel under the map |
| 2 battles, map paused | Two battles side by side (half width each) |
| 2 battles, map running (CPU-only games) | Quadrants: battle, battle, map, roster |

**Shapes.** The map is 3200 x 1800 world units (16:9) and the battle sea is 1600 x 900 (16:9), so both letterbox into odd-shaped panels.

**Each battle panel holds:**
- a header bar (30 units tall);
- one corner per side for HUD: the first fleet's admiral bottom-left, the second's bottom-right;
- a centre area for big messages.

---

## 2. Match (global)

| Data | Values | Now |
|---|---|---|
| Mode | free for all / teams (P1+P3 vs P2+P4) | lobby only |
| Map | classic / random sea, seed number | no |
| Game time | seconds of map time | results screen only |
| Map running | running / paused / half speed (during battles) | roster note |
| Game speed | relaxed / normal / fast | options |
| Hidden-info rule | classic (hide fleet contents) / open (one human) | no |
| Sonar ping | every 3.2 s, rings from each harbor | yes, on the map |
| Live battles | 0-2 (each admiral fights one at a time) | yes, as panels |
| Banner text | e.g. GENERAL QUARTERS, P2 HARBOR CAPTURED BY P1 | yes, centre |
| Winner | admiral or team, plus a message | results |

---

## 3. Per admiral (P1-P4)

| Data | Values | Now |
|---|---|---|
| Number and colour | P1 blue, P2 orange, P3 green, P4 pink | yes |
| Controller | Xbox pad / Switch-style pad / Wii remote / keyboard, plus its name | lobby only |
| Human or CPU (and difficulty) | ADMIRAL / CPU EASY / NORMAL / HARD | top bar |
| Team | A / B (teams mode) | lobby only |
| Status | at sea / out (harbor captured / lost every ship) + the time it happened | top bar |
| Ships afloat | 0-13 | top bar |
| Ships in harbor | 0-13 | top bar (open info only) |
| Ships sunk | 0-13 | no |
| Per-type count | e.g. battleships 1 of 2 afloat; 8 types | harbor menu only |
| Fleets at sea | 0-4 | top bar |
| Repairs (dockings) left | 0-3 | top bar + 3 pips at the harbor |
| Minefields left to lay | 0-4 | top bar (open info only) |
| In battle | yes / no | top bar |
| Carrier / transport alive | each yes/no (they decide who can capture) | no |
| Can capture with any ship | yes once carrier and transport are both sunk | no |
| Selected fleet | which one | highlight on the map |
| Cursor position | map coordinates | yes |
| Last message ("toast") | text, fades over 2.2 s | next to the cursor |
| Harbor position / facing | fixed per seat | yes, on the map |

---

## 4. Per fleet (on the map)

| Data | Values | Who may see it (classic rule) | Now |
|---|---|---|---|
| Owner | P1-P4 | everyone | colour |
| Position, heading | map coordinates, angle | everyone | icon |
| Size | 1-3 ships | everyone | pips |
| Ship types, in order (first = flagship) | e.g. CARRIER, BATTLESHIP, DESTROYER | owner only (open info) | letters under the icon (open info) |
| Each ship's armor | 0 to max | owner only | one combined bar (open info) |
| Fleet speed | knots of its slowest ship (15 while sweeping) | owner | no |
| Moving / destination / route | waypoints | everyone (dashed line, owner's selected fleet) | yes |
| Docking order | heading home to repair | owner | no |
| Sweep mode | on / off (needs a sweeper) | owner | ring (open info) |
| In a mine field | which one; next 20 damage in n s | owner | no |
| In battle | yes | everyone | crossed-lines icon |
| Contact | open window, seconds left | everyone | flashing + label |
| Wake trail | last ~7 s of positions | everyone | yes |
| Can capture | has carrier/transport (or any ship once both are sunk) | owner | no |

---

## 5. Contacts (fleets close enough to fight)

| Data | Values | Now |
|---|---|---|
| The two fleets | ids, owners | dashed line between them |
| Engage window | 6 s countdown | "ENGAGE WINDOW n" |
| Human involved | yes / no: alarm bell, ALL HANDS ON DECK, red edge pulse, rumble | yes |
| Passed | the window closed without a fight | no |

---

## 6. Mines

| Data | Values | Who may see it | Now |
|---|---|---|---|
| Owner, position, radius (62) | | owner only (open info), everyone once it has gone off | dashed ring |
| Revealed | went off at least once | everyone | red ring + dots |
| Laid at | time | owner | no |
| Damage | 20 to the flagship on entry, again every 3 s inside | | no |

---

## 7. Harbor menu (forming a fleet)

| Data | Values | Now |
|---|---|---|
| Per ship type: in harbor / total | e.g. 2/3 destroyers | yes |
| Picked so far, in order | 0-3, the first = flagship | number next to the type |
| Fleets at sea | n/4 | yes |
| Repairs left | n | yes |
| Per type stats (see table 9) | speed, armor, weapon... | no |

---

## 8. Per battle

| Data | Values | Now |
|---|---|---|
| The two admirals | P-numbers, colours | header |
| Battle time | seconds | no |
| Ships alive per side | 0-3 | "n OTHER SHIPS STOPPED" line |
| Total armor / firepower per side | sums | no |
| Retreat | counting down 15 s per side | "RETREATING n" |
| Result | won / fleet sunk / withdrew / both sunk | centre text |
| Land in view | island outlines | yes |
| Shells, torpedoes, planes in flight | positions, owners, targets | yes, drawn in the scene |

---

## 9. Per ship (fixed stats, by type)

| Ship | Count | Top speed (kn) | Accel (0-1.2) | Turn (0-1.6) | Armor | Weapon | Mounts | Damage per salvo | Range | Reload (s) |
|---|---|---|---|---|---|---|---|---|---|---|
| Aircraft carrier | 1 | 30 | 0.35 | 0.55 | 25 | planes | 2 | 10 | 1000 | 2.6 + until both planes land |
| Troop transport | 1 | 30 | 0.25 | 0.50 | 25 | gun | 1 | 7 | 260 | 2.2 |
| Battleship | 2 | 35 | 0.50 | 0.70 | 30 | guns | 3 | 12 | 620 | 3.0 |
| Submarine | 1 | 35 | 0.80 | 1.00 | 15 | torpedoes | 2 | 20 | 780 | 4.5 |
| Destroyer | 3 | 40 | 0.80 | 1.10 | 15 | guns | 2 | 9 | 480 | 2.0 |
| PT boat | 2 | 40 | 1.20 | 1.60 | 5 | torpedoes | 2 | 12 | 360 | 3.5 |
| Mine layer | 1 | 20 | 0.50 | 0.80 | 15 | gun (+ mines) | 1 | 6 | 260 | 2.2 |
| Mine sweeper | 2 | 30 (15 sweeping) | 0.50 | 0.90 | 20 | gun | 1 | 6 | 780 | 2.4 |

Each shot does (salvo damage / mounts). The ship's letter on the map: C T B S D P M W.

## 10. Per ship (live, in battle)

| Data | Values | Now |
|---|---|---|
| Armor left | 0 to max | bar over every ship + stats panel |
| Speed | 0 to top speed in knots (negative = backing off after hitting land) | stats panel |
| Heading | angle | the ship's drawing |
| Engine telegraph | STOP / 1/3 / 2/3 / FULL (stick = any value) | stats panel |
| Aiming | yes (engines stop while aiming) | stats panel + crosshair |
| Reload | 0-100% | stats panel |
| Planes out (carrier) | 0-2, each: deck run / attacking / returning / shot down | stats panel ("PLANES OUT") + planes drawn |
| Target lock (carrier) | which enemy ship is under the crosshair | brackets + "TARGET LOCKED" |
| Salvo spread | 0.3x to 3x | reticle group size |
| In weapon range of the crosshair | yes / no | reticle colour (red = out of range or reloading) |
| Controlled | the one ship this admiral steers | ring + stats panel |
| Aground | touching land: 12 damage every 2 s | no (sparks only) |
| Damaged / smoking | under 50% armor | smoke |
| Sinking | 2.2 s animation | yes |
| Hits taken, shots fired, hit rate | not counted yet (easy to add) | no |
| Current target (CPU ships) | enemy ship | no |

---

## 11. Controls (labels change with the controller)

| Action | Xbox | Switch-style | Wii remote (sideways) | Keyboard |
|---|---|---|---|---|
| Select / move / hold to aim | A | B (same position) | 2 | SPACE |
| Back / next ship | B | A | 1 | BKSP |
| Engage / retreat | Y | X | A | R |
| Lay mines (map) / tighten spread (battle) | X / LT | Y / ZL | - | F |
| Sweep (map) / widen spread (battle) | RT | ZR | + | T |
| Next fleet / ship | RB (LB back) | R / L | B | E / Q |
| Pause | START | + | HOME | ESC |

Move: left stick or d-pad (Wii: d-pad). In battle the stick points the ship; the d-pad is rudder + telegraph.

---

## 12. Messages the game sends (current wording)

- **Banners (centre):** GENERAL QUARTERS · Pn HARBOR CAPTURED BY Pm · Pn LOST EVERY SHIP.
- **Toasts (per admiral):**
  - Contact and repairs: Y TO ENGAGE · RETURNING TO DOCK FOR REPAIRS · DOCKED AND REPAIRED (n LEFT) · NO REPAIRS LEFT.
  - Fleets: FLEET LAUNCHED · 4 FLEETS ARE ALREADY AT SEA · PICK 1-3 SHIPS FIRST · 3 SHIPS PER FLEET · NONE IN HARBOR · NO WAY THROUGH.
  - Mines and sweeping: MINEFIELD LAID (n LEFT) · NEEDS THE MINE LAYER · ALL 4 MINEFIELDS ARE OUT · TOO CLOSE TO YOUR HARBOR · <SHIP> HIT A MINE / SUNK BY A MINE · FLEET LOST TO A MINEFIELD · SWEEPING (15 KNOTS) / SWEEP OFF.
  - Battle: RETREAT IN 15 S · RETREAT CANCELLED · ENEMY IS BUSY IN ANOTHER BATTLE · NO ENEMY IN CONTACT.
- **Battle results:** Pn FLEET SUNK · Pn WITHDRAWS · BOTH FLEETS SUNK.

## 13. Rules a design must respect

- **One shared TV.** With 2+ local humans, never show a fleet's ship types, a fleet's armor or anyone's mines to the
  other players (the classic rule). With a single human it's all fine to show (open info).
- **The map can be as small as a third of the screen** (the side panel during a battle), so map labels need a small version.
- **Up to 4 cursors and 4 message streams** can be on the map at once.
- **Every battle panel can be half the screen wide**, so the per-admiral corners have to fit about 900 x 540 at 1080p.

## 14. Colours in use

- **Players:** P1 (70,200,255), P2 (255,150,50), P3 (120,255,110), P4 (255,90,200).
- **Map and status:** land (90,230,150), warning (255,220,90), bad (255,80,80), good (110,255,140).
- **Backgrounds:** dim text (120,140,165), panel (4,12,24, 85% opaque).
- **Style:** glowing vector lines, stroke font (capitals only); the logo badge art is also available.
