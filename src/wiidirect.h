// Wii remotes the Dolphin way (from Dead Center / Null Arena, proven on ARCADE-21, Oct 2026): the game finds them itself
// (Bluetooth inquiry for the Wii remote device class) and opens the two L2CAP channels (control PSM 0x11, data PSM 0x13).
// No pairing, no BlueZ input plugin, no hid-wiimote. Press 1+2 or the red SYNC button and it joins.
#pragma once
#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct DirectConn { std::string mac; int ctl = -1, intr = -1; };

namespace wiiproto {
    bool out(int intr, const uint8_t* rep, size_t n);   // send an output report (adds the 0xA2 header)
    void setLeds(int intr, int mask, bool rumble);
    void setRumble(int intr, bool on);
    void setMode(int intr, bool rumble);                 // continuous 0x33: buttons + accelerometer + IR
}

class WiiDirect {
public:
    std::atomic<bool> scan{false};       // search for new remotes (title + lobby only: a search adds lag to connected remotes)
    void start();
    void stop();
    std::vector<DirectConn> takeNew();   // remotes connected + initialised since the last call
    void lost(const std::string& mac);   // a remote dropped: keep calling it back for two minutes
    std::string status();
    bool available() const { return ok; }
private:
    std::thread th; std::atomic<bool> run{false}; std::atomic<bool> ok{true};
    std::mutex m; std::vector<DirectConn> fresh; std::string st;
    std::map<std::string, double> lostAt, lastTry; std::map<std::string, bool> live;
    int hci = -1;
    void loop();
    void diag();
    bool connectAndInit(const std::string& mac);
    void setStatus(const std::string& s);
};
