#include "wii.h"
#include <cctype>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <set>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static std::string lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static std::set<std::string> g_ignored;

void WiiInput::start() { direct.start(); }

int WiiInput::slotFor(const std::string& uid) {
    auto it = slotOf.find(uid); if (it != slotOf.end()) return it->second;
    int s = nextSlot++; slotOf[uid] = s; return s;
}

WiiRemote* WiiInput::addFake() {
    auto n = std::make_unique<WiiRemote>(); n->fake = true; n->connected = true; n->uid = "wii:fake" + std::to_string(remotes.size());
    n->slot = slotFor(n->uid); n->hid = n->uid; WiiRemote* r = n.get(); remotes.push_back(std::move(n));
    fprintf(stderr, "wiimote %s: fake remote added (slot %d)\n", r->uid.c_str(), r->slot);
    return r;
}

void WiiInput::scan() {
    if (++scans % 20 == 0) g_ignored.clear();
    DIR* dir = opendir("/dev/input"); if (!dir) return;
    std::set<std::string> used; for (auto& r : remotes) used.insert(r->pathCore);
    while (dirent* ent = readdir(dir)) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;
        std::string path = std::string("/dev/input/") + ent->d_name;
        if (used.count(path) || g_ignored.count(path)) continue;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) { g_ignored.insert(path); continue; }
        char name[256] = {0}; ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
        ::close(fd);
        if (strcmp(name, "Nintendo Wii Remote") != 0) { g_ignored.insert(path); continue; }   // buttons only: accel/IR/extension nodes not needed
        fd = open(path.c_str(), O_RDWR | O_NONBLOCK); if (fd < 0) fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) { g_ignored.insert(path); continue; }
        std::string sys = std::string("/sys/class/input/") + ent->d_name + "/device/device";
        char real[PATH_MAX] = {0}; std::string hid = realpath(sys.c_str(), real) ? real : path;
        char uniq[64] = {0}; ioctl(fd, EVIOCGUNIQ(sizeof(uniq) - 1), uniq);
        auto p = hid.rfind('/'); std::string base = p == std::string::npos ? hid : hid.substr(p + 1);
        std::string uid = std::string("wii:") + (uniq[0] ? lower(uniq) : base);
        WiiRemote* r = nullptr; for (auto& o : remotes) if (!o->direct && o->uid == uid) r = o.get();
        if (!r) { auto n = std::make_unique<WiiRemote>(); r = n.get(); remotes.push_back(std::move(n)); }
        r->hid = hid; r->hidBase = base; r->uid = uid; r->slot = slotFor(uid); r->fdCore = fd; r->pathCore = path; r->connected = true; r->raw = r->latched = 0;
        fprintf(stderr, "wiimote %s: paired remote (hid-wiimote) %s, slot %d\n", uid.c_str(), path.c_str(), r->slot);
        setLeds(r, r->player);
    }
    closedir(dir);
}

void WiiInput::close(WiiRemote* r) {
    for (int* fd : {&r->fdCore, &r->fdCtl, &r->fdInt}) if (*fd >= 0) { ::close(*fd); *fd = -1; }
    if (r->direct && !r->mac.empty()) direct.lost(r->mac);
    r->pathCore = ""; r->connected = false; r->raw = r->latched = 0; r->ffId = -1;
}

void WiiInput::readDirect(WiiRemote* r) {
    uint8_t b[64];
    for (;;) {
        ssize_t n = recv(r->fdInt, b, sizeof b, MSG_DONTWAIT);
        if (n < 0 && (errno == EAGAIN || errno == EINTR)) break;
        if (n <= 0) { fprintf(stderr, "wiimote %s: lost\n", r->mac.c_str()); close(r); return; }
        if (n < 4 || b[0] != 0xA1) continue;
        uint8_t id = b[1]; const uint8_t* p = b + 2;
        if (id == 0x20 || id == 0x21 || id == 0x22 || (id >= 0x30 && id <= 0x3f)) {
            uint32_t w = 0;
            if (p[0] & 0x01) w |= WB_LEFT; if (p[0] & 0x02) w |= WB_RIGHT; if (p[0] & 0x04) w |= WB_DOWN; if (p[0] & 0x08) w |= WB_UP; if (p[0] & 0x10) w |= WB_PLUS;
            if (p[1] & 0x01) w |= WB_2; if (p[1] & 0x02) w |= WB_1; if (p[1] & 0x04) w |= WB_B; if (p[1] & 0x08) w |= WB_A; if (p[1] & 0x10) w |= WB_MINUS; if (p[1] & 0x80) w |= WB_HOME;
            r->latched |= w & ~r->raw; r->raw = w;
        }
        if (id == 0x20) wiiproto::setMode(r->fdInt, r->rumbleOn);   // after a status report the remote stops streaming until the mode is set again
    }
}

static uint32_t keyBit(int code) {
    switch (code) {
    case KEY_UP: case BTN_DPAD_UP: return WB_UP;
    case KEY_DOWN: case BTN_DPAD_DOWN: return WB_DOWN;
    case KEY_LEFT: case BTN_DPAD_LEFT: return WB_LEFT;
    case KEY_RIGHT: case BTN_DPAD_RIGHT: return WB_RIGHT;
    case BTN_A: return WB_A;
    case BTN_B: return WB_B;
    case BTN_1: return WB_1;
    case BTN_2: return WB_2;
    case KEY_NEXT: case BTN_START: return WB_PLUS;
    case KEY_PREVIOUS: case BTN_SELECT: return WB_MINUS;
    case BTN_MODE: return WB_HOME;
    }
    return 0;
}

void WiiInput::read(WiiRemote* r) {
    input_event ev[64];
    for (;;) {
        ssize_t n = ::read(r->fdCore, ev, sizeof ev);
        if (n < 0) { if (errno == EAGAIN || errno == EINTR) break; fprintf(stderr, "wiimote %s: lost\n", r->uid.c_str()); close(r); return; }
        if (n == 0) break;
        for (int i = 0; i < (int)(n / sizeof(input_event)); i++) {
            const input_event& e = ev[i];
            if (e.type != EV_KEY) continue;
            uint32_t b = keyBit(e.code);
            if (e.value) { if (!(r->raw & b)) r->latched |= b; r->raw |= b; } else r->raw &= ~b;
        }
    }
}

void WiiInput::update() {
    double t = nowS();
    if (t >= scanT) { scanT = t + 1.0; scan(); }
    for (auto& dc : direct.takeNew()) {
        WiiRemote* r = nullptr;
        for (auto& o : remotes) if (o->direct && o->mac == dc.mac) r = o.get();   // same remote reconnecting: same slot, same player
        if (!r) { auto n = std::make_unique<WiiRemote>(); r = n.get(); remotes.push_back(std::move(n)); }
        r->direct = true; r->mac = dc.mac; r->fdCtl = dc.ctl; r->fdInt = dc.intr; r->hid = "direct:" + dc.mac;
        r->uid = "wii:" + lower(dc.mac); r->slot = slotFor(r->uid); r->connected = true; r->raw = r->latched = 0;
        r->rumbleOn = false; r->rumbleUntil = 0;
        fprintf(stderr, "wiimote %s: connected directly, slot %d\n", dc.mac.c_str(), r->slot);
        setLeds(r, r->player);
    }
    for (auto& r : remotes) {
        if (!r->connected || r->fake) continue;
        if (r->direct) readDirect(r.get()); else read(r.get());
        if (r->direct && r->connected && r->rumbleOn && t >= r->rumbleUntil) { r->rumbleOn = false; wiiproto::setRumble(r->fdInt, false); }
    }
}

void WiiInput::setLeds(WiiRemote* r, int player) {
    if (!r) return; r->player = player;
    // seated players show their number; a connected remote without a seat shows all four (Null Arena's "not seated" cue)
    int mask = player >= 0 && player < 4 ? 1 << player : player == -1 ? 15 : 0;   // -9 (exit): lights off
    if (r->fake) return;
    if (r->direct) { if (r->fdInt >= 0) wiiproto::setLeds(r->fdInt, mask, r->rumbleOn); return; }
    if (r->hidBase.empty()) return;
    for (int i = 0; i < 4; i++) {
        std::string path = "/sys/class/leds/" + r->hidBase + ":blue:p" + std::to_string(i) + "/brightness";
        FILE* f = fopen(path.c_str(), "w"); if (!f) continue;
        fprintf(f, "%d", (mask >> i) & 1); fclose(f);
    }
}

void WiiInput::rumble(WiiRemote* r, int ms) {
    if (!r || !r->connected || r->fake) return;
    if (r->direct) {
        if (r->fdInt < 0) return;
        r->rumbleUntil = std::max(r->rumbleUntil, nowS() + ms / 1000.0);
        if (!r->rumbleOn) { r->rumbleOn = true; wiiproto::setRumble(r->fdInt, true); }
        return;
    }
    if (r->fdCore < 0) return;
    ff_effect e; memset(&e, 0, sizeof e);
    e.type = FF_RUMBLE; e.id = r->ffId; e.u.rumble.strong_magnitude = 0xffff; e.u.rumble.weak_magnitude = 0xffff; e.replay.length = (unsigned short)ms;
    if (ioctl(r->fdCore, EVIOCSFF, &e) < 0) { e.id = -1; if (ioctl(r->fdCore, EVIOCSFF, &e) < 0) return; }
    r->ffId = e.id;
    input_event ev; memset(&ev, 0, sizeof ev); ev.type = EV_FF; ev.code = e.id; ev.value = 1;
    if (::write(r->fdCore, &ev, sizeof ev) < 0) {}
}

void WiiInput::shutdown() {
    direct.stop();
    for (auto& r : remotes) { if (r->direct && r->fdInt >= 0) wiiproto::setRumble(r->fdInt, false); setLeds(r.get(), -9); close(r.get()); }
}
