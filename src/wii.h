// Wii remotes for Iron Harbor: buttons, player LEDs and rumble. Two paths, same game code:
//  - direct (wiidirect: the game connects the remote itself; press 1+2), proven in Dead Center / Null Arena
//  - a remote paired in Batocera, read from the Linux hid-wiimote driver (/dev/input/event*, "Nintendo Wii Remote")
// Not built for the browser.
#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "wiidirect.h"

enum WiiBtn : uint32_t { WB_UP = 1, WB_DOWN = 2, WB_LEFT = 4, WB_RIGHT = 8, WB_A = 16, WB_B = 32, WB_1 = 64, WB_2 = 128, WB_PLUS = 256, WB_MINUS = 512, WB_HOME = 1024 };

struct WiiRemote {
    int slot = -1;                       // stable number for this remote in this run (the game's device id is 1000 + slot)
    std::string hid, hidBase, uid, pathCore;
    int fdCore = -1, ffId = -1;
    bool connected = false, fake = false;
    uint32_t raw = 0, latched = 0;       // latched: presses seen since the game last read them (quick taps never lost)
    bool direct = false; std::string mac; int fdCtl = -1, fdInt = -1; std::atomic<bool> rumbleOn{false}; double rumbleUntil = 0;
    int player = -1;                     // LEDs show player + 1
};

class WiiInput {
public:
    std::vector<std::unique_ptr<WiiRemote>> remotes;
    WiiDirect& direct = *new WiiDirect();   // never freed on purpose (see wiidirect.cpp stop())
    void start();
    void update();                          // scan for paired remotes (once a second), adopt direct ones, read buttons
    uint32_t take(WiiRemote* r) { uint32_t v = r->raw | r->latched; r->latched = 0; return v; }
    void rumble(WiiRemote* r, int ms);
    void setLeds(WiiRemote* r, int player);
    void shutdown();
    WiiRemote* addFake();                   // tests (IH_WIIFAKE): a remote driven by setting raw directly
private:
    double scanT = 0; int scans = 0, nextSlot = 0;
    std::map<std::string, int> slotOf;      // uid -> slot, so a remote that drops comes back as the same device
    void scan();
    void close(WiiRemote* r);
    void read(WiiRemote* r);
    void readDirect(WiiRemote* r);
    int slotFor(const std::string& uid);
};
