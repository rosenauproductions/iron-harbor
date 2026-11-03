#!/bin/bash
# Browser build -> web/dist (static files). Uses Ubuntu's emscripten 3.1.6 with a config copy (EM_CONFIG) and a local SDL
# release-2.0.20 checkout (EMCC_LOCAL_PORTS), like Belt Command. No Wii remotes in the browser.
set -e
cd "$(dirname "$0")/.."
mkdir -p web/dist
em++ -std=c++17 -O2 -Wno-misleading-indentation src/main.cpp -o web/dist/index.html \
    -sUSE_SDL=2 -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web -sEXPORTED_RUNTIME_METHODS=FS,UTF8ToString \
    -sEXPORTED_FUNCTIONS=_main,_ih_main -sMINIFY_HTML=0 --shell-file web/shell.html
V=$(date +%Y%m%d%H%M%S); sed -i -e "s/__BUILD__/$V/g" -e "s/src=\"index.js\"/src=\"index.js?v=$V\"/" web/dist/index.html
grep -q "index.js?v=$V" web/dist/index.html || { echo "build stamp missing"; exit 1; }
ls -la web/dist
