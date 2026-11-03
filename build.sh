#!/bin/bash
# Build Iron Harbor.
#   ./build.sh          release binary for Batocera (x86_64, glibc 2.28) -> build/ironharbor.x86_64
#   ./build.sh native   quick native build for testing on this machine   -> build/ih_test
set -e
cd "$(dirname "$0")"
mkdir -p build
SRC="src/main.cpp src/wii.cpp src/wiidirect.cpp"
if [ "$1" = "native" ]; then
    g++ -std=c++17 -O2 -Wall -Wno-misleading-indentation -Wno-unused-function $(sdl2-config --cflags) $SRC -o build/ih_test $(sdl2-config --libs) -ldl -lpthread
    echo "built build/ih_test"; exit 0
fi
python3 -c "import ziglang" 2>/dev/null || pip install --break-system-packages ziglang
[ -f /usr/include/SDL2/SDL.h ] || { echo "install libsdl2-dev first"; exit 1; }
mkdir -p build/sdlinc/SDL2
cp /usr/include/x86_64-linux-gnu/SDL2/_real_SDL_config.h build/sdlinc/SDL2/ 2>/dev/null || true
python3 -m ziglang c++ -target x86_64-linux-gnu.2.28 -O2 -std=c++17 -Wno-misleading-indentation \
    -I/usr/include/SDL2 -Ibuild/sdlinc -D_REENTRANT $SRC \
    /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 -ldl -lpthread -o build/ironharbor.x86_64
md5sum build/ironharbor.x86_64
