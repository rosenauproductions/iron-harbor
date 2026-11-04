# Iron Harbor — design (working title)

Original naval strategy/battle game by Rosenau Productions for the Batocera box and the browser.
1-4 admirals, any mix of local players and CPU. C++17 + SDL2, vector art drawn in code, synthesized sound.

## Rules
- Every admiral has a harbor and the same 13 ships:

| Ship | # | Knots | Handling | Armor | Weapon | Dmg | Range |
|---|---|---|---|---|---|---|---|
| Aircraft carrier | 1 | 30 | slow | 25 | 2 planes | 10 | longest (aircraft) |
| Troop transport | 1 | 30 | sluggish | 25 | gun | 7 | very short |
| Battleship | 2 | 35 | average | 30 | gun | 12 | long |
| Submarine | 1 | 35 | quick | 15 | torpedo | 20 | very long |
| Destroyer | 3 | 40 | quick | 15 | gun | 9 | moderate |
| PT boat | 2 | 40 | very quick | 5 | torpedo | 12 | short |
| Mine layer | 1 | 20 | average | 15 | gun (+mines 20) | 6 | very short |
| Mine sweeper | 2 | 30 (15 sweeping) | average | 20 | gun | 6 | very long |

- Fleets: 1-3 ships, no two of a type, first ship = flagship. Max 4 fleets at sea. Fleet speed = slowest ship.
- Map: enemy fleets show only their size (CLASSIC info). With one local human, info is OPEN (no one to hide from).
- Mines: the mine layer drops up to 4 invisible fields. An enemy fleet entering one: flagship -20 HP, again every 3 s inside.
  A sweeper in SWEEP mode (15 kn) clears enemy fields it crosses, silently.
- Repairs: sail a fleet into your own harbor = docked, fully repaired, ships back in the harbor. 3 dockings per game.
- Contact: hostile fleets that come close flash for 6 s; either side can ENGAGE. Otherwise they pass.
  With a local player involved: general-quarters alarm bell (repeats every 1.75 s), "ALL HANDS ON DECK" over the contact, red edge pulse, rumble.
- Battle: zoomed view of that patch of sea (real islands). Every ship starts stopped. Each admiral (CPU too) controls ONE ship
  at a time; NEXT SHIP picks another. The others sit where they are and don't fire. If yours sinks, you take the next one.
  Hold FIRE to aim (the ship stops driving), release to shoot. One shot per gun mount, each from its own turret:
  battleship 3 (triangle), destroyer 2, carrier 2 planes, sub 2 bow tubes, PT 2 side tubes, others 1; salvo damage = the ship's damage.
  The reticles show the group; RT/LT (right stick, Wii +/-, keyboard T/F) open or tighten it.
  Carrier: planes roll down the deck, fly to the target, bomb, fly back and land; no new strike until both are back.
  Crosshair on an enemy ship = TARGET LOCKED: the planes chase it, but a fast, nimble ship can still dodge (up to ~60%).
  Two carriers' planes that cross paths in the air dogfight: each has a 35% chance to be shot down (spirals into the sea); 3 s before they can tangle again. Shells land on the crosshair; torpedoes hit anything on their path.
  Land: 12 HP every 2 s. RETREAT: 15 s alarm, then the fleet slips away.
- Win: sail the carrier or the troop transport into an enemy harbor (any ship, once both are sunk). Captured = out.
  Free-for-all or 2v2 (P1+P3 vs P2+P4). Last admiral (or team) standing wins.

## Screen
One shared TV. No battle: map full screen. While battles run, the map pauses if no free human is left to play it
(the classic way); otherwise it keeps running at half speed in a side panel.

## Controls (Xbox 360 pad first, Wii remote held sideways second)
| Action | Xbox 360 | Wii remote (sideways) | Keyboard |
|---|---|---|---|
| Move cursor / steer | left stick or d-pad | d-pad | arrows / WASD, mouse |
| Select / order / hold to aim, release to fire | A | 2 | Space / left click |
| Back / battle: next ship | B | 1 | Backspace / right click |
| Engage / retreat | Y | A | R |
| Lay mines | X | - (minus) | F |
| Sweep mode on/off | RT | + (plus) | T |
| Previous / next fleet or ship | LB / RB | B trigger = next | Q / E |
| Pause | START | HOME | P / Esc |

Battle steering: the stick points where the ship should head (push = throttle). The d-pad (and the Wii remote) work like a
helm: left/right = rudder, up/down = engine telegraph (stop, 1/3, 2/3, full).
Wii remotes: press 1+2 (or red SYNC) on the title or lobby screen; unpair them in Batocera first for the direct connection.
