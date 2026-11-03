#!/bin/bash
# Iron Harbor - Batocera port launcher
DIR="$(dirname "$(readlink -f "$0")")/ironharbor"
BIN="$DIR/ironharbor.x86_64"
LOG=/userdata/system/logs/ironharbor.log
mkdir -p /userdata/system/logs
cd "$DIR" || { echo "missing $DIR" > "$LOG"; exit 1; }
# paired Wii remotes are read from hid-wiimote by the game itself; keep SDL from also showing them as joysticks
export SDL_JOYSTICK_HIDAPI_WII=0
{
  echo "=== Iron Harbor launch $(date) ==="
  # copying over the network share (SMB) can drop the executable bit: restore it
  [ -x "$BIN" ] || chmod +x "$BIN" 2>&1
} > "$LOG" 2>&1
if [ -x "$BIN" ]; then
  IH_HOME="$DIR" "$BIN" "$@" >> "$LOG" 2>&1
else
  IH_HOME="$DIR" /lib64/ld-linux-x86-64.so.2 "$BIN" "$@" >> "$LOG" 2>&1
fi
rc=$?
echo "=== exit code $rc ===" >> "$LOG"
exit $rc
