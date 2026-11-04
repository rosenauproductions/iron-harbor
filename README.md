# Iron Harbor (working title)

Naval strategy for 1-4 admirals on one screen, any mix of local players and CPU admirals. Form fleets from 13 ships,
lay hidden minefields, fight close battles at sea, and sail your carrier or troop transport into an enemy harbor.
Original game by Rosenau Productions. C++17 + SDL2, vector art drawn in code, synthesized sound. Rules and controls: `docs/DESIGN.md`.

## Status (3 Nov 2026)
- Playable start to finish: lobby (4 seats: admiral / CPU easy-normal-hard / open; free for all or 2v2; classic or random sea),
  strategy map, harbor menu, fleets, mines, sweeping, docking (3 repairs), contacts, battles, captures, results.
- Maps: generator with mirrored islands (2 admirals west/east, 3-4 in the corners), checked so every harbor can reach every other.
- Battles: the real islands of that patch of sea. All ships start stopped; every admiral (CPU too) controls one ship at a time,
  the others sit still; B / Wii 1 picks the next ship. Hold to aim. (4 Nov)
  Up to two battles at once (four admirals). The map pauses when no free human is left, else runs at half speed beside the battle.
- Controllers: Xbox 360 pads (SDL; pads without a mapping get the xpad layout), Switch-style pads (button names by position),
  Wii remotes held sideways (direct 1+2 connect or Batocera-paired), keyboard + mouse. Wii remotes: LEDs show the seat
  (all four = connected, no seat yet), rumble on hits/sinkings.
- Tested in the cloud only: many CPU-vs-CPU matches (2/3/4 admirals, teams, random seas) to the end; simulated Wii remotes
  joining the lobby; browser build in headless Chromium. **Not yet run on the box or with real controllers.**

## Build
- Native test: `./build.sh native` -> `build/ih_test` (needs `libsdl2-dev`).
- Batocera: `./build.sh` -> `build/ironharbor.x86_64` (zig c++, glibc 2.28 target, links the box's libSDL2).
- Web: `EM_CONFIG=<copy of /usr/share/emscripten/.emscripten with FROZEN_CACHE=False, NODE_JS set> EMCC_LOCAL_PORTS="sdl2=<SDL release-2.0.20 checkout>" ./web/build.sh`
  -> `web/dist` (wasm). The claude.ai artifact uses a pure-JS single file (`-sWASM=0 -sSINGLE_FILE=1`) because of its content policy.

## Box layout
`roms/ports/IronHarbor.sh` (+ `.keys`: HOTKEY+START quits), `roms/ports/ironharbor/ironharbor.x86_64`,
images in `roms/ports/images/`, entry from `batocera/gamelist-entry.xml`. Log: `/userdata/system/logs/ironharbor.log`.

## Test switches (env)
`IH_STATE=game IH_PLAYERS=n` (all CPU) · `IH_HUMAN=1` (P1 = keyboard) · `IH_TEAMS=1` · `IH_MAPRANDOM=1` · `IH_BATTLE=1` (instant battle)
· `IH_MENU=1` (harbor menu open) · `IH_FAST=n` (steps per frame) · `IH_UNTILOVER=1` · `IH_MAXTIME=s` · `IH_SHOT=frame IH_SHOT_PATH=x.bmp`
· `IH_WINDOW=WxH` · `IH_MUTE=1` · `IH_NOWII=1` · `IH_WIIFAKE=n` (simulated remotes that press 2 in the lobby) · `IH_LOBBY=1` · `IH_BTEST=1` (with IH_HUMAN IH_BATTLE: scripted fire, ship switch, steer, fire) · `IH_SEED=n` · `IH_SEED_RNG=n`.
Headless: `xvfb-run -a -s "-screen 0 1920x1080x24" env IH_MUTE=1 SDL_AUDIODRIVER=dummy IH_NOWII=1 ... build/ih_test`.

## Next
- First box run with Xbox pads and Wii remotes (unpair remotes in Batocera for the direct path), read `ironharbor.log`.
- Balance: CPU NORMAL can rush a carrier into your harbor in ~1.5 min of map time on NORMAL speed.
- Ideas: phones as private screens for hidden info (fleet forming, mine laying), online play, Wii speaker cues, Switch Pro gyro aim.
