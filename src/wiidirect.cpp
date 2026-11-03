// Direct Wii remote connection (copied from the Dead Center build tested on ARCADE-21, 23 Oct 2026; IR sensitivity
// setters dropped because Iron Harbor doesn't point at the screen). Search: kernel HCIINQUIRY -> raw HCI inquiry -> bluetoothctl.
#include "wiidirect.h"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <endian.h>

#ifndef AF_BLUETOOTH
#define AF_BLUETOOTH 31
#endif
#define BTPROTO_L2CAP 0
#define BTPROTO_HCI 1
#define HCIINQUIRY _IOR('H', 240, int)
#define IREQ_CACHE_FLUSH 0x0001
struct bdaddr6 { uint8_t b[6]; };
struct sockaddr_l2x { sa_family_t l2_family; unsigned short l2_psm; bdaddr6 l2_bdaddr; unsigned short l2_cid; uint8_t l2_bdaddr_type; };
struct hci_inquiry_reqx { uint16_t dev_id; uint16_t flags; uint8_t lap[3]; uint8_t length; uint8_t num_rsp; };
struct __attribute__((packed)) inquiry_infox { bdaddr6 bdaddr; uint8_t pscan_rep_mode, pscan_period_mode, pscan_mode; uint8_t dev_class[3]; uint16_t clock_offset; };
#define HCIDEVUP _IOW('H', 201, int)
#define SOL_HCI 0
#define HCI_FILTER 2
struct sockaddr_hcix { sa_family_t hci_family; unsigned short hci_dev; unsigned short hci_channel; };
struct hci_filterx { uint32_t type_mask; uint32_t event_mask[2]; uint16_t opcode; };

static double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static std::string macStr(const bdaddr6& a) { char s[18]; snprintf(s, sizeof s, "%02X:%02X:%02X:%02X:%02X:%02X", a.b[5], a.b[4], a.b[3], a.b[2], a.b[1], a.b[0]); return s; }
static bool macParse(const std::string& s, bdaddr6& a) {
    unsigned v[6]; if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) != 6) return false;
    for (int i = 0; i < 6; i++) a.b[5 - i] = (uint8_t)v[i]; return true;
}

namespace wiiproto {
bool out(int fd, const uint8_t* rep, size_t n) {
    uint8_t b[32]; if (n + 1 > sizeof b) return false;
    b[0] = 0xA2; memcpy(b + 1, rep, n);
    return send(fd, b, n + 1, MSG_NOSIGNAL) == (ssize_t)(n + 1);
}
void setLeds(int fd, int mask, bool rumble) { uint8_t r[2] = {0x11, (uint8_t)(((mask & 15) << 4) | (rumble ? 1 : 0))}; out(fd, r, 2); }
void setRumble(int fd, bool on) { uint8_t r[2] = {0x10, (uint8_t)(on ? 1 : 0)}; out(fd, r, 2); }
void setMode(int fd, bool rumble) { uint8_t r[3] = {0x12, (uint8_t)(0x04 | (rumble ? 1 : 0)), 0x33}; out(fd, r, 3); }
}
static const uint8_t SENS1[9] = {0x02, 0x00, 0x00, 0x71, 0x01, 0x00, 0xaa, 0x00, 0x64}, SENS2[2] = {0x63, 0x03};   // level 3

static void waitAck(int fd) {
    double end = nowS() + 0.25; uint8_t b[64];
    while (nowS() < end) {
        pollfd p{fd, POLLIN, 0}; if (poll(&p, 1, 50) <= 0) continue;
        ssize_t n = recv(fd, b, sizeof b, MSG_DONTWAIT);
        if (n >= 2 && b[0] == 0xA1 && b[1] == 0x22) return;
    }
}
static void writeRegAck(int fd, uint32_t addr, const uint8_t* d, int n) {
    uint8_t r[22] = {0x16, 0x04, (uint8_t)(addr >> 16), (uint8_t)(addr >> 8), (uint8_t)addr, (uint8_t)n};
    memcpy(r + 6, d, n); wiiproto::out(fd, r, sizeof r); waitAck(fd);
}
static int l2connect(const bdaddr6& a, unsigned short psm, int timeoutMs) {
    int s = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP); if (s < 0) return -1;
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
    sockaddr_l2x addr; memset(&addr, 0, sizeof addr); addr.l2_family = AF_BLUETOOTH; addr.l2_psm = htole16(psm); addr.l2_bdaddr = a;
    int r = connect(s, (sockaddr*)&addr, sizeof addr);
    if (r < 0 && errno != EINPROGRESS) { close(s); return -1; }
    if (r < 0) {
        pollfd p{s, POLLOUT, 0};
        if (poll(&p, 1, timeoutMs) <= 0) { close(s); return -1; }
        int err = 0; socklen_t l = sizeof err; getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &l);
        if (err) { close(s); errno = err; return -1; }
    }
    return s;
}

bool WiiDirect::connectAndInit(const std::string& mac) {
    bdaddr6 a; if (!macParse(mac, a)) return false;
    int ctl = l2connect(a, 0x11, 4000);
    if (ctl < 0) { fprintf(stderr, "wiidirect: %s control channel: %s\n", mac.c_str(), strerror(errno)); return false; }
    int intr = l2connect(a, 0x13, 4000);
    if (intr < 0) { fprintf(stderr, "wiidirect: %s data channel: %s\n", mac.c_str(), strerror(errno)); close(ctl); return false; }
    // same proven init as Dead Center (camera on + mode 0x33); buttons are all Iron Harbor reads
    uint8_t pix[2] = {0x13, 0x04}, logic[2] = {0x1a, 0x04}, eight = 0x08, mode = 0x03;
    wiiproto::setLeds(intr, 0, false);
    wiiproto::out(intr, pix, 2); usleep(30000);
    wiiproto::out(intr, logic, 2); usleep(30000);
    writeRegAck(intr, 0xb00030, &eight, 1);
    writeRegAck(intr, 0xb00000, SENS1, 9);
    writeRegAck(intr, 0xb0001a, SENS2, 2);
    writeRegAck(intr, 0xb00033, &mode, 1);
    writeRegAck(intr, 0xb00030, &eight, 1);
    wiiproto::setMode(intr, false);
    fprintf(stderr, "wiidirect: %s connected\n", mac.c_str());
    std::lock_guard<std::mutex> g(m);
    fresh.push_back({mac, ctl, intr}); live[mac] = true; lostAt.erase(mac);
    return true;
}

void WiiDirect::setStatus(const std::string& s) { std::lock_guard<std::mutex> g(m); st = s; }
std::string WiiDirect::status() { std::lock_guard<std::mutex> g(m); return st; }
std::vector<DirectConn> WiiDirect::takeNew() { std::lock_guard<std::mutex> g(m); std::vector<DirectConn> v; v.swap(fresh); return v; }
void WiiDirect::lost(const std::string& mac) { std::lock_guard<std::mutex> g(m); live[mac] = false; lostAt[mac] = nowS(); }
void WiiDirect::start() { if (run) return; run = true; th = std::thread([this] { loop(); }); }
// detached on purpose: an inquiry can block for seconds. The owner allocates this object once and never frees it
// (Null Arena's exit-crash fix: a detached thread waking into freed memory).
void WiiDirect::stop() { run = false; if (th.joinable()) th.detach(); }

static std::string sh(const char* cmd) {
    std::string out; FILE* f = popen(cmd, "r"); if (!f) return out;
    char b[512]; while (fgets(b, sizeof b, f)) out += b; pclose(f); return out;
}
static bool isWiiClass(const uint8_t* c) {
    return (c[0] == 0x04 && c[1] == 0x25 && c[2] == 0x00) || (c[0] == 0x08 && c[1] == 0x05 && c[2] == 0x00);   // RVL-CNT-01 / -TR
}
static int rawInquiry(int dev, std::vector<std::string>& found, std::string& why) {
    int s = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (s < 0) { why = std::string("socket: ") + strerror(errno); return -1; }
    sockaddr_hcix a{}; a.hci_family = AF_BLUETOOTH; a.hci_dev = (unsigned short)dev; a.hci_channel = 0;
    if (bind(s, (sockaddr*)&a, sizeof a) < 0) { why = std::string("bind hci") + std::to_string(dev) + ": " + strerror(errno); close(s); return -1; }
    hci_filterx f{}; f.type_mask = 1u << 4; f.event_mask[0] = f.event_mask[1] = 0xffffffffu;
    if (setsockopt(s, SOL_HCI, HCI_FILTER, &f, sizeof f) < 0) { why = std::string("filter: ") + strerror(errno); close(s); return -1; }
    uint8_t cmd[9] = {0x01, 0x01, 0x04, 0x05, 0x33, 0x8b, 0x9e, 0x03, 0x00};
    if (write(s, cmd, sizeof cmd) != (ssize_t)sizeof cmd) { why = std::string("send: ") + strerror(errno); close(s); return -1; }
    double end = nowS() + 6.0; bool started = false; int rc = 0;
    while (nowS() < end) {
        pollfd p{s, POLLIN, 0}; if (poll(&p, 1, 100) <= 0) continue;
        uint8_t b[300]; ssize_t n = read(s, b, sizeof b);
        if (n < 3 || b[0] != 0x04) continue;
        uint8_t ev = b[1]; const uint8_t* q = b + 3; int len = b[2];
        if (ev == 0x0F && len >= 4 && q[2] == 0x01 && q[3] == 0x04) {
            if (q[0]) { char t[64]; snprintf(t, sizeof t, "controller refused inquiry, status 0x%02X", q[0]); why = t; rc = -1; break; }
            started = true;
        } else if (ev == 0x01) break;
        else if (ev == 0x02 && len >= 1) {
            for (int i = 0; i < q[0] && 1 + (i + 1) * 14 <= len; i++) { const uint8_t* e = q + 1 + i * 14; if (isWiiClass(e + 9)) found.push_back(macStr(*(const bdaddr6*)e)); }
        } else if (ev == 0x22 && len >= 1) {
            for (int i = 0; i < q[0] && 1 + (i + 1) * 14 <= len; i++) { const uint8_t* e = q + 1 + i * 14; if (isWiiClass(e + 8)) found.push_back(macStr(*(const bdaddr6*)e)); }
        } else if (ev == 0x2F && len >= 15) {
            if (isWiiClass(q + 9)) found.push_back(macStr(*(const bdaddr6*)(q + 1)));
        }
    }
    if (rc == 0 && !started) { why = "no answer from controller"; rc = -1; }
    close(s);
    return rc;
}
static void bluezScan(std::vector<std::string>& found) {
    std::string seen = sh("timeout 7 bluetoothctl --timeout 5 scan on 2>&1");
    std::string devs = sh("timeout 3 bluetoothctl devices 2>&1");
    size_t pos = 0;
    while ((pos = devs.find("Device ", pos)) != std::string::npos) {
        size_t eol = devs.find('\n', pos); std::string line = devs.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
        pos += 7;
        if (line.size() < 24) continue;
        std::string mac = line.substr(7, 17);
        bool wii = line.find("RVL-CNT") != std::string::npos || line.find("Nintendo") != std::string::npos;
        if (wii && seen.find(mac) != std::string::npos) found.push_back(mac);
    }
}
static void logLines(const char* tag, const std::string& out) {
    size_t a = 0;
    while (a < out.size()) { size_t e = out.find('\n', a); if (e == std::string::npos) e = out.size();
        if (e > a) fprintf(stderr, "wiidirect: %s| %s\n", tag, out.substr(a, e - a).c_str()); a = e + 1; }
}
void WiiDirect::diag() {
    std::string k = sh("uname -r"); while (!k.empty() && k.back() == '\n') k.pop_back();
    std::string a = sh("ls /sys/class/bluetooth 2>&1 | tr '\\n' ' '");
    fprintf(stderr, "wiidirect: kernel %s, adapters: %s\n", k.c_str(), a.c_str());
    logLines("rfkill", sh("rfkill list 2>&1 | head -8"));
    logLines("show", sh("timeout 4 bluetoothctl show 2>&1 | head -8"));
}
static void powerUp(int hci) {
    logLines("unblock", sh("rfkill unblock bluetooth 2>&1"));
    if (ioctl(hci, HCIDEVUP, 0) < 0 && errno != EALREADY) fprintf(stderr, "wiidirect: switching hci0 on failed: %s\n", strerror(errno));
    else fprintf(stderr, "wiidirect: hci0 switched on\n");
    logLines("power", sh("timeout 4 bluetoothctl power on 2>&1 | tail -2"));
    usleep(500000);
}

void WiiDirect::loop() {
    hci = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    if (hci < 0) { ok = false; fprintf(stderr, "wiidirect: no Bluetooth (%s)\n", strerror(errno)); setStatus("NO BLUETOOTH ADAPTER FOUND"); return; }
    fprintf(stderr, "wiidirect: started\n");
    diag();
    int method = 0; const char* names[3] = {"kernel inquiry", "raw HCI inquiry", "bluetoothctl scan"};
    std::vector<uint8_t> buf(sizeof(hci_inquiry_reqx) + 255 * sizeof(inquiry_infox));
    bool lastErr = false, triedUp = false;
    while (run) {
        std::vector<std::string> retry;
        { std::lock_guard<std::mutex> g(m); double t = nowS();
          for (auto& kv : lostAt) if (t - kv.second < 120 && t - lastTry[kv.first] > 3) { retry.push_back(kv.first); lastTry[kv.first] = t; } }
        for (auto& mac : retry) if (run) connectAndInit(mac);
        if (!scan) { for (int i = 0; i < 5 && run && !scan; i++) usleep(100000); continue; }
        setStatus("WII REMOTE: PRESS 1+2 (OR RED SYNC) TO JOIN");
        std::vector<std::string> found;
        if (method == 0) {
            memset(buf.data(), 0, buf.size());
            auto* req = (hci_inquiry_reqx*)buf.data();
            req->dev_id = 0; req->flags = IREQ_CACHE_FLUSH; req->lap[0] = 0x33; req->lap[1] = 0x8b; req->lap[2] = 0x9e; req->length = 2; req->num_rsp = 255;
            if (ioctl(hci, HCIINQUIRY, buf.data()) < 0) { fprintf(stderr, "wiidirect: %s failed: %s -> trying %s\n", names[0], strerror(errno), names[1]); method = 1; continue; }
            auto* info = (inquiry_infox*)(buf.data() + sizeof(hci_inquiry_reqx));
            for (int i = 0; i < req->num_rsp; i++) if (isWiiClass(info[i].dev_class)) found.push_back(macStr(info[i].bdaddr));
        } else if (method == 1) {
            std::string why;
            int r = rawInquiry(0, found, why);
            if (r < 0 && why.find("Network is down") != std::string::npos && !triedUp) { fprintf(stderr, "wiidirect: hci0 is switched off -> switching it on\n"); triedUp = true; powerUp(hci); continue; }
            if (r < 0) { fprintf(stderr, "wiidirect: %s failed: %s -> trying %s\n", names[1], why.c_str(), names[2]); method = 2; continue; }
        } else bluezScan(found);
        if (!lastErr) { fprintf(stderr, "wiidirect: searching with %s\n", names[method]); lastErr = true; }
        for (auto& mac : found) {
            if (!run) break;
            { std::lock_guard<std::mutex> g(m); if (live[mac]) continue; }
            setStatus("CONNECTING WII REMOTE...");
            fprintf(stderr, "wiidirect: found %s (%s)\n", mac.c_str(), names[method]);
            connectAndInit(mac);
        }
    }
}
