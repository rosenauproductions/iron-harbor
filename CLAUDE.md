# Iron Harbor: notes for Claude sessions

Chris's original naval strategy game (working title) for his Batocera box (ARCADE-21, x86_64) and the browser.
C++17 + SDL2, no other libraries; vector art drawn in code, synthesized sound. Rules and controls: `docs/DESIGN.md`.
Build, test switches and status: `README.md`.

## Working rules (from Chris)
- May be sold later: never name, link or describe the game as based on any other game, in code, docs, store text or commits.
  Keep `THIRD_PARTY_NOTICES.md` current; no outside art, fonts or sounds.
- Controllers: designed for Xbox 360 pads first, Wii remotes (held sideways) second. Wii code = the proven Dead Center
  `wiidirect` (src/wiidirect.*) + hid-wiimote path (src/wii.*). Search for remotes only on title/lobby screens.
- After controller changes, add a check-in to the shared controller board
  (`/Volumes/share/roms/ports/wiilab/docs/controller-advances-oct2026.md`, project copy `claude/controller-advances-oct2026.md`).
- Show screenshots for anything visual before calling it done. Keep README Status current in plain language.
- Deploy to the box yourself through the linked Mac (Game Dev doc `claude/batocera-deploy-from-cloud.md`):
  `roms/ports/IronHarbor.sh` (+ .keys), `roms/ports/ironharbor/ironharbor.x86_64`, back up the old binary, check md5.

## Build
- `./build.sh native` -> `build/ih_test` (needs `libsdl2-dev`); `./build.sh` -> `build/ironharbor.x86_64` (zig, glibc 2.28).
- Web: Ubuntu emscripten 3.1.6 with a config copy (`FROZEN_CACHE = False`, `NODE_JS` set) as `EM_CONFIG`, and
  `EMCC_LOCAL_PORTS="sdl2=<SDL release-2.0.20 checkout>"`, then `./web/build.sh` -> `web/dist` (committed).
  The claude.ai artifact build adds `-sWASM=0 -sSINGLE_FILE=1` (its content policy may block WebAssembly).

## Test headless
`xvfb-run -a -s "-screen 0 1920x1080x24" env IH_MUTE=1 SDL_AUDIODRIVER=dummy IH_NOWII=1 IH_WINDOW=1920x1080 IH_STATE=game IH_PLAYERS=4 IH_FAST=60 IH_UNTILOVER=1 build/ih_test`
