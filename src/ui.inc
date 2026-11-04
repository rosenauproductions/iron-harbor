// Iron Harbor - controllers (Xbox 360 pads, Wii remotes, keyboard + mouse), map controls, menus, main loop
// (included from main.cpp)

static std::string GAMEDIR = ".";
static Dev* devById(int id) { for (auto& d : G.devs) if (d.id == id) return &d; return nullptr; }
static bool pr(const Dev& d, int b) { return d.pend[b]; }
static void usePresses() { for (auto& d : G.devs) { memset(d.pend, 0, sizeof d.pend); d.click = false; d.rclick = false; } }
static Dev* sideDev(int s) { Side& S = G.side[s]; if (S.cpu || S.devLost) return nullptr; return devById(S.dev); }

static const char* btnName(int s, int b) {
    Dev* d = sideDev(s);
    if (d && d->wii) { switch (b) { case B_A: return "2"; case B_B: return "1"; case B_Y: return "A"; case B_X: return "-"; case B_RT: return "+"; case B_RB: return "B"; case B_LB: return "B"; case B_START: return "HOME"; } return "?"; }
    if (d && d->id == -1 && !XBOXLOOK) { switch (b) { case B_A: return "SPACE"; case B_B: return "BKSP"; case B_Y: return "R"; case B_X: return "F"; case B_RT: return "T"; case B_RB: return "E"; case B_LB: return "Q"; case B_START: return "ESC"; } return "?"; }
    bool n = d && d->nintendo;   // Switch-style pads: buttons named by where they are
    switch (b) { case B_A: return n ? "B" : "A"; case B_B: return n ? "A" : "B"; case B_Y: return n ? "X" : "Y"; case B_X: return n ? "Y" : "X"; case B_RT: return n ? "ZR" : "RT"; case B_RB: return n ? "R" : "RB"; case B_LB: return n ? "L" : "LB"; case B_START: return n ? "+" : "START"; }
    return "?";
}

static void rumbleSide(int s, int ms) {
    if (s < 0 || s > 3) return; Dev* d = sideDev(s); if (!d) return;
    if (d->gc) SDL_GameControllerRumble(d->gc, 0x6000, 0xB000, (Uint32)ms);
#ifdef IH_WII
    if (d->wr) G.wii.rumble(d->wr, ms);
#endif
}

// ---------------------------------------------------------------- SDL pads
static int padButton(int b) {
    switch (b) {
    case SDL_CONTROLLER_BUTTON_A: return B_A; case SDL_CONTROLLER_BUTTON_B: return B_B; case SDL_CONTROLLER_BUTTON_X: return B_X; case SDL_CONTROLLER_BUTTON_Y: return B_Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return B_LB; case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return B_RB;
    case SDL_CONTROLLER_BUTTON_START: return B_START; case SDL_CONTROLLER_BUTTON_BACK: return B_BACK;
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return B_UP; case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return B_DOWN; case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return B_LEFT; case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return B_RIGHT;
    }
    return -1;
}
static void openPad(int index) {
    if (!SDL_IsGameController(index)) {
        // Null Arena lesson (25 Oct): pads without a gamecontrollerdb entry only raise the joystick event. Give an
        // Xbox-shaped one the Linux xpad layout so it still works.
        SDL_Joystick* j = SDL_JoystickOpen(index); if (!j) return;
        int nb = SDL_JoystickNumButtons(j), na = SDL_JoystickNumAxes(j), nh = SDL_JoystickNumHats(j);
        char guid[64]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(j), guid, sizeof guid);
        printf("joystick: %s guid %s (NO MAPPING) buttons %d axes %d hats %d\n", SDL_JoystickName(j), guid, nb, na, nh);
        if (nb >= 10 && na >= 4) {
#ifdef __EMSCRIPTEN__   // browsers report the W3C "standard gamepad" order
            std::string m = std::string(guid) + "," + (SDL_JoystickName(j) ? SDL_JoystickName(j) : "pad") + ",a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,lefttrigger:b6,righttrigger:b7,back:b8,start:b9,leftstick:b10,rightstick:b11,dpup:b12,dpdown:b13,dpleft:b14,dpright:b15,leftx:a0,lefty:a1,rightx:a2,righty:a3";
#else
            std::string m = std::string(guid) + "," + (SDL_JoystickName(j) ? SDL_JoystickName(j) : "pad") + ",a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,guide:b8,leftstick:b9,rightstick:b10,leftx:a0,lefty:a1,rightx:a3,righty:a4,lefttrigger:a2,righttrigger:a5,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2";
#endif
            SDL_GameControllerAddMapping(m.c_str()); printf("joystick: gave it the xpad layout\n");
        }
        SDL_JoystickClose(j);
        if (!SDL_IsGameController(index)) return;
    }
    SDL_GameController* gc = SDL_GameControllerOpen(index); if (!gc) return;
    int id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    if (devById(id)) return;
    Dev d; d.id = id; d.gc = gc; d.name = SDL_GameControllerName(gc) ? SDL_GameControllerName(gc) : "PAD";
    char guid[64]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(SDL_GameControllerGetJoystick(gc)), guid, sizeof guid);
    SDL_GameControllerType ty = SDL_GameControllerGetType(gc);
    std::string ln = d.name; for (auto& ch : ln) ch = (char)tolower(ch);
    d.nintendo = ty == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO || ln.find("nintendo") != std::string::npos || ln.find("switch") != std::string::npos || ln.find("pro controller") != std::string::npos;
    printf("pad: added id %d \"%s\" guid %s type %d%s\n", id, d.name.c_str(), guid, (int)ty, d.nintendo ? " (nintendo layout)" : "");
    G.devs.push_back(d);
    for (auto& s : G.side) if (s.used && !s.cpu && s.devLost && s.dev < 1000 && s.dev != -1) { s.dev = id; s.devLost = false; printf("pad: reassigned to P%d\n", s.idx + 1); break; }
    for (int i = 0; i < 4; i++) if (G.slotDev[i] == id) SDL_GameControllerSetPlayerIndex(gc, i);
}
static void closePad(int id) {
    for (size_t i = 0; i < G.devs.size(); i++) if (G.devs[i].id == id) { printf("pad: removed id %d \"%s\"\n", id, G.devs[i].name.c_str()); SDL_GameControllerClose(G.devs[i].gc); G.devs.erase(G.devs.begin() + i); break; }
    for (auto& s : G.side) if (s.used && s.dev == id) s.devLost = true;
}

// ---------------------------------------------------------------- Wii remotes become devices 1000 + slot
#ifdef IH_WII
static void syncWii() {
    G.wii.direct.scan = G.state == ST_TITLE || G.state == ST_LOBBY;   // search only in menus (a search lags connected remotes)
    G.wii.update();
    for (auto& r : G.wii.remotes) {
        int id = 1000 + r->slot; Dev* d = devById(id);
        if (r->connected && !d) {
            Dev n; n.id = id; n.wii = true; n.wr = r.get(); n.name = r->direct ? "WII REMOTE" : "WII REMOTE (PAIRED)"; G.devs.push_back(n);
            printf("pad: Wii remote %s as device %d\n", r->uid.c_str(), id);
            int seat = -1; for (int i = 0; i < 4; i++) if (G.slotDev[i] == id) seat = i;
            G.wii.setLeds(r.get(), seat);
            for (auto& s : G.side) if (s.used && s.dev == id) s.devLost = false;
            AU.play(SND_UI_OK, 0.5f);
        } else if (!r->connected && d) {
            for (size_t i = 0; i < G.devs.size(); i++) if (G.devs[i].id == id) { G.devs.erase(G.devs.begin() + i); break; }
            for (auto& s : G.side) if (s.used && s.dev == id) s.devLost = true;
        }
    }
    for (auto& d : G.devs) if (d.wii) for (auto& r : G.wii.remotes) if (1000 + r->slot == d.id) d.wr = r.get();
    // IH_WIIFAKE test: fake remote k taps 2 at frame 30 + 30k (joins in the lobby), and taps the d-pad later
    if (getenv("IH_WIIFAKE")) { int k = 0; for (auto& r : G.wii.remotes) if (r->fake) { if (G.frame == 30 + 30 * k) r->latched |= WB_2; k++; } }
}
#endif

static int navAxis(Dev& d, int axis, float dt) {
    int dir = 0;
    if (axis) { if (d.b[B_UP] || d.mv.y < -0.55f) dir = -1; else if (d.b[B_DOWN] || d.mv.y > 0.55f) dir = 1; }
    else { if (d.b[B_LEFT] || d.mv.x < -0.55f) dir = -1; else if (d.b[B_RIGHT] || d.mv.x > 0.55f) dir = 1; }
    int& last = axis ? d.nyLast : d.nxLast; float& t = axis ? d.nyT : d.nxT;
    if (!dir) { last = 0; return 0; }
    if (dir != last) { last = dir; t = 0.36f; return dir; }
    t -= dt; if (t <= 0) { t = 0.1f; return dir; }
    return 0;
}
static void readDevs(float dt) {
    int mx, my; SDL_GetMouseState(&mx, &my);
    for (auto& d : G.devs) {
        memcpy(d.pb, d.b, sizeof d.b); memset(d.b, 0, sizeof d.b); d.mv = V2(); d.analog = false; d.lt = d.rt = d.ry = 0;
        if (d.id == -1) {
            const Uint8* k = SDL_GetKeyboardState(nullptr);
            d.b[B_UP] = k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]; d.b[B_DOWN] = k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN];
            d.b[B_LEFT] = k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]; d.b[B_RIGHT] = k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT];
            d.b[B_A] = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RETURN] || (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK);
            d.b[B_B] = k[SDL_SCANCODE_BACKSPACE]; d.b[B_Y] = k[SDL_SCANCODE_R]; d.b[B_X] = k[SDL_SCANCODE_F]; d.b[B_RT] = k[SDL_SCANCODE_T];
            d.b[B_LB] = k[SDL_SCANCODE_Q]; d.b[B_RB] = k[SDL_SCANCODE_E]; d.b[B_START] = k[SDL_SCANCODE_ESCAPE] || k[SDL_SCANCODE_P];
            int ww, wh; SDL_GetWindowSize(G.win, &ww, &wh);
            d.mouse = V2(mx * (float)G.W / std::max(ww, 1), my * (float)G.H / std::max(wh, 1));
            if (d.rclick) d.hit[B_B] = true;
        }
#ifdef IH_WII
        else if (d.wii && d.wr) {
            uint32_t lat = d.wr->latched; d.wr->latched = 0; uint32_t w = d.wr->raw | lat;   // lat: pressed since the last read (taps under a frame)
            auto map = [&](uint32_t bits) {
                uint32_t o = 0;
                auto put = [&](uint32_t wb, int k) { if (bits & wb) o |= 1u << k; };
                if (G.opt.wiiDpad == 0) { put(WB_RIGHT, B_UP); put(WB_LEFT, B_DOWN); put(WB_UP, B_LEFT); put(WB_DOWN, B_RIGHT); }   // held sideways, d-pad on the left
                else { put(WB_UP, B_UP); put(WB_DOWN, B_DOWN); put(WB_LEFT, B_LEFT); put(WB_RIGHT, B_RIGHT); }
                put(WB_2, B_A); put(WB_1, B_B); put(WB_A, B_Y); put(WB_B, B_RB); put(WB_MINUS, B_X); put(WB_PLUS, B_RT); put(WB_HOME, B_START);
                return o;
            };
            uint32_t held = map(w), tap = map(lat);
            for (int k = 0; k < NB; k++) { if (held >> k & 1) d.b[k] = true; if (tap >> k & 1) d.hit[k] = true; }
        }
#endif
        else if (d.gc) {
            auto Bt = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(d.gc, b) != 0; };
            d.b[B_A] = Bt(SDL_CONTROLLER_BUTTON_A); d.b[B_B] = Bt(SDL_CONTROLLER_BUTTON_B); d.b[B_X] = Bt(SDL_CONTROLLER_BUTTON_X); d.b[B_Y] = Bt(SDL_CONTROLLER_BUTTON_Y);
            d.b[B_LB] = Bt(SDL_CONTROLLER_BUTTON_LEFTSHOULDER); d.b[B_RB] = Bt(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            d.b[B_START] = Bt(SDL_CONTROLLER_BUTTON_START); d.b[B_BACK] = Bt(SDL_CONTROLLER_BUTTON_BACK);
            d.b[B_UP] = Bt(SDL_CONTROLLER_BUTTON_DPAD_UP); d.b[B_DOWN] = Bt(SDL_CONTROLLER_BUTTON_DPAD_DOWN); d.b[B_LEFT] = Bt(SDL_CONTROLLER_BUTTON_DPAD_LEFT); d.b[B_RIGHT] = Bt(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            float rt = SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.f; d.b[B_RT] = rt > 0.5f;
            d.rt = clampf(rt, 0, 1); d.lt = clampf(SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.f, 0, 1);
            d.ry = SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.f; if (std::fabs(d.ry) < 0.25f) d.ry = 0;
            V2 st(SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f, SDL_GameControllerGetAxis(d.gc, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f);
            float m = len(st); if (m < 0.22f) st = V2(); else { st = st / m * clampf((m - 0.22f) / 0.73f, 0, 1); d.analog = true; d.mv = st; }
        }
        if (!d.analog) { d.mv = V2((d.b[B_RIGHT] ? 1.f : 0.f) - (d.b[B_LEFT] ? 1.f : 0.f), (d.b[B_DOWN] ? 1.f : 0.f) - (d.b[B_UP] ? 1.f : 0.f)); if (len(d.mv) > 1) d.mv = norm(d.mv); }
        for (int k = 0; k < NB; k++) {
            if (d.hit[k]) d.b[k] = true;
            if ((d.b[k] && !d.pb[k]) || d.hit[k]) d.pend[k] = true;
            d.hit[k] = false;
        }
        d.navX = navAxis(d, 0, dt); d.navY = navAxis(d, 1, dt);
    }
}

// ---------------------------------------------------------------- map controls
static int fleetNear(int si, V2 p, float r) {
    int best = -1; float bd = r;
    for (int i = 0; i < (int)G.fl.size(); i++) { Fleet& f = G.fl[i]; if (f.dead || f.owner != si) continue; float d = dist(f.p, p); if (d < bd) { bd = d; best = i; } }
    return best;
}
static void cycleFleet(int si, int dir) {
    Side& S = G.side[si]; int n = (int)G.fl.size(); if (!n) return;
    int cur = S.sel < 0 ? (dir > 0 ? -1 : 0) : S.sel;
    for (int k = 1; k <= n; k++) { int i = ((cur + dir * k) % n + n) % n; if (!G.fl[i].dead && G.fl[i].owner == si) { S.sel = i; S.cur = G.fl[i].p; AU.play(SND_UI, 0.5f); return; } }
    toast(S, "NO FLEETS AT SEA - GO TO YOUR HARBOR");
}
static void updateSide(int si, float dt) {
    Side& S = G.side[si]; if (!S.used || S.cpu || !S.alive) return;
    if (S.sel >= 0 && (S.sel >= (int)G.fl.size() || G.fl[S.sel].dead)) S.sel = -1;
    Dev* d = sideDev(si); if (!d || sideInBattle(si)) { S.menu = false; return; }
    if (S.menu) {
        if (d->navY) { S.menuSel = (S.menuSel + d->navY + NT + 2) % (NT + 2); AU.play(SND_UI, 0.4f); }
        if (pr(*d, B_B) || pr(*d, B_START)) { S.menu = false; AU.play(SND_UI, 0.5f); return; }
        if (pr(*d, B_A)) {
            if (S.menuSel < NT) {
                int t = S.menuSel, at = -1; for (int k = 0; k < S.npick; k++) if (S.ship[S.pick[k]].t == t) at = k;
                if (at >= 0) { for (int k = at; k < 2; k++) S.pick[k] = S.pick[k + 1]; S.npick--; AU.play(SND_UI, 0.5f); }
                else if (S.npick >= 3) { toast(S, "3 SHIPS PER FLEET"); AU.play(SND_ERR, 0.5f); }
                else { int i = shipInHarbor(si, t); if (i < 0) { toast(S, "NONE IN HARBOR"); AU.play(SND_ERR, 0.5f); } else { S.pick[S.npick++] = i; AU.play(SND_UI_OK, 0.5f); } }
            } else if (S.menuSel == NT) {
                if (S.npick == 0) { AU.play(SND_ERR, 0.5f); toast(S, "PICK 1-3 SHIPS FIRST"); }
                else if (fleetsAtSea(si) >= MAXFLEETS) { AU.play(SND_ERR, 0.5f); toast(S, "4 FLEETS ARE ALREADY AT SEA"); }
                else { int fi = launchFleet(si, S.pick, S.npick); S.npick = 0; S.menu = false; if (fi >= 0) { S.sel = fi; S.cur = G.fl[fi].p + S.hdir * 120; toast(S, "FLEET LAUNCHED - MOVE THE CURSOR, PRESS " + std::string(btnName(si, B_A))); } }
            } else { S.menu = false; AU.play(SND_UI, 0.5f); }
        }
        return;
    }
    // cursor: stick / d-pad, or the mouse for the keyboard player
    float sp = 1250 * (d->analog ? len(d->mv) * 0.4f + 0.6f : 0.75f);
    S.cur += d->mv * (sp * dt);
    if (d->id == -1 && d->mouseMoved && MAPSHOWN) S.cur = VMAP.s2w(d->mouse);
    S.cur.x = clampf(S.cur.x, 0, MW); S.cur.y = clampf(S.cur.y, 0, MH);
    if (pr(*d, B_A)) {
        if (d->id == -1 && d->click && MAPSHOWN) S.cur = VMAP.s2w(d->mouse);
        int f = fleetNear(si, S.cur, 45);
        bool atHarbor = dist(S.cur, S.harbor) < HARBOR_R + 45;
        if (f >= 0 && f != S.sel) { S.sel = f; AU.play(SND_UI_OK, 0.5f); }
        else if (atHarbor && S.sel >= 0 && G.fl[S.sel].battle < 0) { orderMove(S.sel, S.harbor, true); toast(S, S.docks > 0 ? "RETURNING TO DOCK FOR REPAIRS" : "NO REPAIRS LEFT"); AU.play(SND_UI_OK, 0.5f); }
        else if (atHarbor) { S.menu = true; S.npick = 0; S.menuSel = 0; AU.play(SND_UI_OK, 0.5f); }
        else if (S.sel >= 0) {
            orderMove(S.sel, S.cur, false);
            if (G.fl[S.sel].moving) AU.play(SND_UI, 0.6f); else { AU.play(SND_ERR, 0.5f); toast(S, "NO WAY THROUGH"); }
        } else { AU.play(SND_ERR, 0.4f); toast(S, "PICK A FLEET, OR YOUR HARBOR TO FORM ONE"); }
    }
    if (pr(*d, B_B)) { if (S.sel >= 0) { S.sel = -1; AU.play(SND_UI, 0.4f); } }
    if (pr(*d, B_RB)) cycleFleet(si, 1);
    if (pr(*d, B_LB)) cycleFleet(si, -1);
    if (pr(*d, B_Y)) {
        int k = contactFor(si, S.sel);
        if (k < 0) { toast(S, "NO ENEMY IN CONTACT"); AU.play(SND_ERR, 0.4f); }
        else if (!engage(k)) { toast(S, "ENEMY IS BUSY IN ANOTHER BATTLE"); AU.play(SND_ERR, 0.4f); }
    }
    if (pr(*d, B_X)) { if (S.sel >= 0) layMine(S.sel); else { toast(S, "SELECT THE FLEET WITH THE MINE LAYER"); AU.play(SND_ERR, 0.4f); } }
    if (pr(*d, B_RT)) {
        if (S.sel < 0 || !hasType(G.fl[S.sel], T_SWEEPER)) { toast(S, "SELECT A FLEET WITH A SWEEPER"); AU.play(SND_ERR, 0.4f); }
        else { Fleet& f = G.fl[S.sel]; f.sweep = !f.sweep; toast(S, f.sweep ? "SWEEPING (15 KNOTS)" : "SWEEP OFF"); AU.play(SND_SWEEP, 0.4f); }
    }
}

// ---------------------------------------------------------------- menus
struct MenuIn { int x = 0, y = 0; bool a = false, b = false, start = false; int devA = -99, devB = -99; };
static MenuIn menuInput() {
    MenuIn m;
    for (auto& d : G.devs) {
        if (d.navX) m.x = d.navX; if (d.navY) m.y = d.navY;
        if (pr(d, B_A)) { m.a = true; m.devA = d.id; }
        if (pr(d, B_B)) { m.b = true; m.devB = d.id; } if (pr(d, B_START)) { m.start = true; if (m.devA == -99) m.devA = d.id; }
    }
    return m;
}
static int seatOf(int dev) { for (int i = 0; i < 4; i++) if (G.slotKind[i] == 1 && G.slotDev[i] == dev) return i; return -1; }
static void seatLeds() {
#ifdef IH_WII
    for (auto& d : G.devs) if (d.wii && d.wr) G.wii.setLeds(d.wr, seatOf(d.id));
#endif
    for (auto& d : G.devs) if (d.gc) { int s = seatOf(d.id); SDL_GameControllerSetPlayerIndex(d.gc, s); }
}
static void joinSeat(int dev) {
    if (seatOf(dev) >= 0) return;
    int s = -1; for (int i = 0; i < 4; i++) if (G.slotKind[i] != 1) { s = i; break; }
    if (s < 0) return;
    G.slotKind[s] = 1; G.slotDev[s] = dev; AU.play(SND_UI_OK, 0.6f);
    printf("lobby: device %d joins as P%d\n", dev, s + 1);
    seatLeds();
    Dev* d = devById(dev); if (d && d->gc) SDL_GameControllerRumble(d->gc, 0x5000, 0x9000, 180);
#ifdef IH_WII
    if (d && d->wr) G.wii.rumble(d->wr, 200);
#endif
}
static int usedSlots() { int n = 0; for (int i = 0; i < 4; i++) if (G.slotKind[i]) n++; return n; }
static bool canStart() {
    if (usedSlots() < 2) return false;
    if (G.teams) { bool t0 = false, t1 = false; for (int i = 0; i < 4; i++) if (G.slotKind[i]) (i % 2 ? t1 : t0) = true; return t0 && t1; }
    return true;
}
static void cycleSlot(int i, int dir) {
    if (G.slotKind[i] == 1) return;
    int v = G.slotKind[i] == 0 ? 0 : 1 + G.slotDiff[i];   // 0 off, 1 easy, 2 normal, 3 hard
    v = (v + dir + 4) % 4;
    if (v == 0) G.slotKind[i] = 0; else { G.slotKind[i] = 2; G.slotDiff[i] = v - 1; }
    AU.play(SND_UI, 0.5f);
}

static const char* HELP[] = {
    "HOW TO PLAY",
    "",
    "SAIL YOUR AIRCRAFT CARRIER OR TROOP TRANSPORT INTO AN ENEMY HARBOR TO CAPTURE IT.",
    "ONCE BOTH ARE SUNK, ANY OF YOUR SHIPS CAN CAPTURE. LAST ADMIRAL (OR TEAM) STANDING WINS.",
    "",
    "PUT THE CURSOR ON YOUR HARBOR AND PRESS SELECT TO FORM A FLEET: 1-3 SHIPS, ONE OF EACH TYPE.",
    "THE FIRST SHIP YOU PICK IS THE FLAGSHIP. UP TO 4 FLEETS AT SEA. A FLEET IS AS FAST AS ITS SLOWEST SHIP.",
    "SELECT A FLEET, THEN PRESS SELECT ON THE SEA TO SEND IT THERE. ENEMIES SEE HOW MANY SHIPS, NOT WHICH.",
    "",
    "THE MINE LAYER LAYS UP TO 4 HIDDEN MINEFIELDS. AN ENEMY FLAGSHIP INSIDE ONE TAKES 20 DAMAGE EVERY 3 S.",
    "A SWEEPER IN SWEEP MODE (15 KNOTS) CLEARS ENEMY FIELDS IT CROSSES - WITHOUT TELLING YOU.",
    "SEND A FLEET INTO YOUR OWN HARBOR TO DOCK: FULL REPAIRS, SHIPS BACK IN PORT. 3 TIMES PER GAME.",
    "",
    "WHEN ENEMY FLEETS MEET, THE ALARM SOUNDS - ALL HANDS ON DECK: ENGAGE WITHIN 6 SECONDS OR SAIL ON.",
    "IN BATTLE EVERY SHIP STARTS STOPPED. YOU CONTROL ONE SHIP AT A TIME - PRESS NEXT SHIP TO CHOOSE.",
    "THE OTHERS SIT WHERE THEY ARE AND DON'T FIRE. IF YOUR SHIP SINKS, YOU TAKE OVER THE NEXT ONE.",
    "HOLD FIRE TO AIM (YOUR SHIP STOPS DRIVING), RELEASE TO SHOOT. ONE RETICLE PER GUN: RT/LT (WII +/-) OPEN OR TIGHTEN",
    "THE GROUP. CARRIER PLANES LOCK ON A SHIP UNDER THE CROSSHAIR - A FAST, NIMBLE TARGET CAN STILL DODGE. ENEMY PLANES",
    "THAT CROSS PATHS DOGFIGHT AND CAN SHOOT EACH OTHER DOWN. SHELLS LAND ON THE CROSSHAIR,",
    "TORPEDOES HIT ANYTHING IN THEIR PATH. LAND HURTS. RETREAT TAKES 15 SECONDS.",
    "",
    "XBOX 360: STICK STEERS (POINT WHERE TO GO)  A FIRE/SELECT  B NEXT SHIP/BACK  Y ENGAGE/RETREAT",
    "          X MINES  RT SWEEP  LB/RB FLEETS  D-PAD = RUDDER + ENGINE TELEGRAPH",
    "WII REMOTE (SIDEWAYS): D-PAD RUDDER/ENGINE  2 FIRE/SELECT  1 NEXT SHIP/BACK  A ENGAGE/RETREAT",
    "          - MINES  + SWEEP  B NEXT FLEET  HOME PAUSE.  PRESS 1+2 ON THE TITLE OR LOBBY TO CONNECT",
    "KEYBOARD: ARROWS  SPACE  BACKSPACE  R  F  T  Q/E  ESC  (MOUSE CLICKS WORK ON THE MAP)",
};

static void drawBackdrop(float u) {
    G.cv.rect(0, 0, (float)G.W, (float)G.H, Col(3, 12, 26));
    float t = SDL_GetTicks() / 1000.f;
    for (float y = 30; y < G.H; y += 60 * u) for (float x = std::fmod(y * 0.7f, 80 * u); x < G.W; x += 100 * u) {
        float ph = t * 0.7f + x * 0.01f + y * 0.017f; G.cv.arc(V2(x + std::sin(ph) * 8 * u, y), 10 * u, PI * 1.15f, PI * 1.85f, 1 * u, Col(60, 130, 200, (int)(35 + 25 * std::sin(ph))), 0);
    }
    V2 c(G.W * 0.5f, G.H * 0.62f); float r = std::fmod(t * 0.35f, 1.f);
    for (int k = 0; k < 3; k++) { float rr = std::fmod(r + k / 3.f, 1.f); G.cv.circle(c, rr * G.H * 0.9f, 1.5f * u, Col(70, 200, 255, (int)(90 * (1 - rr))), 0.4f, 72); }
    float a = t * 0.6f; G.cv.line(c, c + fromAng(a) * G.H * 0.45f, 2 * u, Col(70, 220, 255, 120), 0.6f);
}
static void drawLogo(float cx, float y, float sz) {
    text("IRON HARBOR", cx, y, sz, Col(80, 210, 255), 1);
    text("NAVAL STRATEGY FOR 1-4 ADMIRALS", cx, y + sz * 1.35f, sz * 0.28f, Col(160, 200, 230), 1);
}
static void menuList(const char** items, int n, int sel, float cx, float y, float size, float gap) {
    for (int i = 0; i < n; i++) {
        Col c = i == sel ? Col(255, 230, 120) : Col(170, 190, 210);
        if (i == sel) text(">", cx - Font::width(items[i], size) / 2 - size * 1.5f, y + i * gap, size, c);
        text(items[i], cx, y + i * gap, size, c, 1);
    }
}

static void quitToTitle() { G.state = ST_TITLE; G.menuSel = 0; G.paused = false; for (auto& b : G.bt) b.live = false; }

static void updateMenus(float dt) {
    G.stateT += dt; MenuIn m = menuInput();
    if (G.state == ST_TITLE) {
#ifdef __EMSCRIPTEN__
        int n = 3;
#else
        int n = 4;
#endif
        if (m.y) { G.menuSel = (G.menuSel + m.y + n) % n; AU.play(SND_UI, 0.5f); }
        if (m.a || m.start) {
            AU.play(SND_UI_OK, 0.6f);
            if (G.menuSel == 0) { G.state = ST_LOBBY; G.lobbySel = 6; if (m.devA != -99) joinSeat(m.devA); }
            else if (G.menuSel == 1) G.state = ST_HELP;
            else if (G.menuSel == 2) { G.state = ST_OPTIONS; G.menuSel = 0; }
            else G.quit = true;
        }
    } else if (G.state == ST_HELP) {
        if (m.a || m.b || m.start) { G.state = G.over || G.paused ? ST_GAME : ST_TITLE; AU.play(SND_UI, 0.5f); }
    } else if (G.state == ST_OPTIONS) {
        const int n = 8;
        if (m.y) { G.menuSel = (G.menuSel + m.y + n) % n; AU.play(SND_UI, 0.5f); }
        int dx = m.x + (m.a ? 1 : 0);
        if (dx) {
            switch (G.menuSel) {
            case 0: G.opt.res = (G.opt.res + dx + 5) % 5; setupTarget(); break;
            case 1: G.opt.speed = (G.opt.speed + dx + 3) % 3; break;
            case 2: G.opt.info = (G.opt.info + dx + 3) % 3; break;
            case 3: G.opt.sfx = (int)clampf((float)(G.opt.sfx + (m.a ? (G.opt.sfx >= 10 ? -10 : 1) : dx)), 0, 10); break;
            case 4: G.opt.mus = (int)clampf((float)(G.opt.mus + (m.a ? (G.opt.mus >= 10 ? -10 : 1) : dx)), 0, 10); break;
            case 5: G.opt.glow = !G.opt.glow; break;
            case 6: G.opt.wiiDpad = 1 - G.opt.wiiDpad; break;
            case 7: if (m.a) { G.state = ST_TITLE; G.menuSel = 2; saveCfg(); } break;
            }
            applyAudioVol(); AU.play(SND_UI, 0.5f);
        }
        if (m.b || m.start) { G.state = ST_TITLE; G.menuSel = 2; saveCfg(); AU.play(SND_UI, 0.5f); }
    } else if (G.state == ST_LOBBY) {
        // rows: 0-3 seats, 4 mode, 5 map, 6 start
        if (m.y) { G.lobbySel = (G.lobbySel + m.y + 7) % 7; AU.play(SND_UI, 0.4f); }
        if (m.x) {
            if (G.lobbySel < 4) cycleSlot(G.lobbySel, m.x);
            else if (G.lobbySel == 4) { G.teams = !G.teams; AU.play(SND_UI, 0.5f); }
            else if (G.lobbySel == 5) { G.mapKind = !G.mapKind; AU.play(SND_UI, 0.5f); }
        }
        for (auto& d : G.devs) {
            if (pr(d, B_A) && seatOf(d.id) < 0) { joinSeat(d.id); memset(d.pend, 0, sizeof d.pend); }
            if (pr(d, B_B) && seatOf(d.id) >= 0) { int s = seatOf(d.id); G.slotKind[s] = 0; G.slotDev[s] = -9; seatLeds(); AU.play(SND_UI, 0.5f); memset(d.pend, 0, sizeof d.pend); printf("lobby: P%d left\n", s + 1); }
        }
        m = menuInput();
        if (m.a && seatOf(m.devA) >= 0) {
            if (G.lobbySel < 4) cycleSlot(G.lobbySel, 1);
            else if (G.lobbySel == 4) G.teams = !G.teams;
            else if (G.lobbySel == 5) G.mapKind = !G.mapKind;
            else if (canStart()) { AU.play(SND_UI_OK, 0.7f); startMatch(); return; }
            else AU.play(SND_ERR, 0.5f);
        }
        if (m.start && seatOf(m.devA) >= 0 && canStart()) { startMatch(); return; }
        bool anySeat = false; for (int i = 0; i < 4; i++) if (G.slotKind[i] == 1) anySeat = true;
        if (m.b && !anySeat) { G.state = ST_TITLE; G.menuSel = 0; AU.play(SND_UI, 0.5f); }
    }
}

static void renderMenus() {
    float u = G.cv.px; drawBackdrop(u);
    float cx = G.W / 2.f;
    if (G.state == ST_TITLE) {
        drawLogo(cx, G.H * 0.18f, 74 * u);
        const char* items[4] = { "PLAY", "HOW TO PLAY", "OPTIONS", "QUIT" };
#ifdef __EMSCRIPTEN__
        menuList(items, 3, G.menuSel, cx, G.H * 0.5f, 30 * u, 52 * u);
#else
        menuList(items, 4, G.menuSel, cx, G.H * 0.5f, 30 * u, 52 * u);
#endif
        int np = 0, nw = 0; for (auto& d : G.devs) { if (d.gc) np++; if (d.wii) nw++; }
        text(std::to_string(np) + " PADS  " + std::to_string(nw) + " WII REMOTES CONNECTED", cx, G.H - 70 * u, 13 * u, C_DIM, 1);
#ifdef IH_WII
        std::string ws = G.wii.direct.status(); if (!ws.empty()) text(ws, cx, G.H - 48 * u, 12 * u, Col(120, 200, 255), 1);
#endif
        text("(C) 2026 ROSENAU PRODUCTIONS", cx, G.H - 24 * u, 10 * u, Col(90, 110, 130), 1);
    } else if (G.state == ST_HELP) {
        float y = G.H * 0.08f;
        for (const char* l : HELP) { text(l, cx, y, (l == HELP[0] ? 26 : 13) * u, l == HELP[0] ? Col(80, 210, 255) : C_WHITE, 1); y += (l == HELP[0] ? 40 : 26) * u; }
        text("PRESS ANY BUTTON", cx, G.H - 40 * u, 12 * u, C_DIM, 1);
    } else if (G.state == ST_OPTIONS) {
        text("OPTIONS", cx, G.H * 0.12f, 34 * u, Col(80, 210, 255), 1);
        std::string it[8] = { std::string("RESOLUTION: ") + RESNAMES[G.opt.res], std::string("GAME SPEED: ") + SPEEDN[G.opt.speed], std::string("HIDDEN INFO: ") + INFON[G.opt.info],
            "SOUND: " + std::to_string(G.opt.sfx), "MUSIC: " + std::to_string(G.opt.mus), std::string("GLOW: ") + (G.opt.glow ? "ON" : "OFF"), std::string("WII REMOTE GRIP: ") + (G.opt.wiiDpad ? "UPRIGHT" : "SIDEWAYS"), "BACK" };
        const char* p[8]; for (int i = 0; i < 8; i++) p[i] = it[i].c_str();
        menuList(p, 8, G.menuSel, cx, G.H * 0.3f, 24 * u, 44 * u);
        const char* notes[8] = { "RENDER RESOLUTION FOR YOUR TV", "HOW FAST FLEETS CROSS THE MAP", "AUTO: OPEN WITH ONE PLAYER, CLASSIC (FLEET TYPES HIDDEN) WITH MORE", "", "", "LINE GLOW", "SIDEWAYS: D-PAD ON THE LEFT, 1 AND 2 ON THE RIGHT", "" };
        text(notes[G.menuSel], cx, G.H * 0.3f + 8 * 44 * u + 20 * u, 12 * u, C_DIM, 1);
    } else if (G.state == ST_LOBBY) {
        text("ADMIRALS", cx, G.H * 0.07f, 34 * u, Col(80, 210, 255), 1);
        float cw = std::min(400 * u, G.W / 4.6f), ch = 210 * u, gap = 20 * u, x0 = cx - (4 * cw + 3 * gap) / 2, y0 = G.H * 0.18f;
        for (int i = 0; i < 4; i++) {
            float x = x0 + i * (cw + gap); bool sel = G.lobbySel == i;
            Col c = G.slotKind[i] ? PCOL[i] : Col(80, 90, 110);
            panel(x, y0, cw, ch, sel ? Col(255, 230, 120) : c, u, 230);
            text("P" + std::to_string(i + 1), x + cw / 2, y0 + 18 * u, 30 * u, c, 1);
            std::string k = G.slotKind[i] == 0 ? "OPEN" : G.slotKind[i] == 2 ? std::string("CPU ") + DIFFN[G.slotDiff[i]] : "ADMIRAL";
            text(k, x + cw / 2, y0 + 68 * u, 16 * u, c, 1);
            if (G.slotKind[i] == 1) { Dev* d = devById(G.slotDev[i]); std::string dn = d ? d->name : "DISCONNECTED"; for (auto& ch2 : dn) ch2 = (char)toupper(ch2); if (dn.size() > 22) dn = dn.substr(0, 22); text(dn, x + cw / 2, y0 + 98 * u, 10 * u, C_DIM, 1); text(std::string(d && d->wii ? "1" : "B") + " TO LEAVE", x + cw / 2, y0 + 160 * u, 10 * u, C_DIM, 1); }
            else text("< > CHANGE", x + cw / 2, y0 + 130 * u, 11 * u, sel ? Col(255, 230, 120) : C_DIM, 1);
            if (G.teams && G.slotKind[i]) text(i % 2 ? "TEAM B" : "TEAM A", x + cw / 2, y0 + ch - 26 * u, 12 * u, c, 1);
        }
        std::string rows[3] = { std::string("MODE: ") + (G.teams ? "TEAMS (P1+P3 VS P2+P4)" : "FREE FOR ALL"), std::string("MAP: ") + (G.mapKind ? "RANDOM SEA" : "CLASSIC SEA"), canStart() ? "START" : "START (NEED 2 SIDES)" };
        for (int r = 0; r < 3; r++) { bool sel = G.lobbySel == 4 + r; text((sel ? "> " : "") + rows[r], cx, y0 + ch + (50 + r * 46) * u, 22 * u, sel ? Col(255, 230, 120) : Col(170, 190, 210), 1); }
        text("ANY CONTROLLER: PRESS A (XBOX) OR 2 (WII REMOTE) TO JOIN. KEYBOARD: ENTER.", cx, G.H - 80 * u, 13 * u, C_WHITE, 1);
#ifdef IH_WII
        std::string ws = G.wii.direct.status(); if (!ws.empty()) text(ws + "   (UNPAIR IT IN BATOCERA FIRST)", cx, G.H - 54 * u, 12 * u, Col(120, 200, 255), 1);
#endif
    }
}

// ---------------------------------------------------------------- pause / results
static void updateOverlay(float) {
    MenuIn m = menuInput();
    if (G.over) {
        if (G.overT < 1.5f) return;
        if (m.y) { G.resultSel = 1 - G.resultSel; AU.play(SND_UI, 0.5f); }
        if (m.a || m.start) { AU.play(SND_UI_OK, 0.6f); if (G.resultSel == 0) { startMatch(); } else { G.state = ST_LOBBY; G.lobbySel = 6; for (auto& b : G.bt) b.live = false; } }
        return;
    }
    const int n = 3;
    if (m.y) { G.pauseSel = (G.pauseSel + m.y + n) % n; AU.play(SND_UI, 0.5f); }
    if (m.b || m.start) { G.paused = false; AU.play(SND_UI, 0.5f); return; }
    if (m.a) {
        if (G.pauseSel == 0) G.paused = false;
        else if (G.pauseSel == 1) { G.state = ST_HELP; }
        else quitToTitle();
        AU.play(SND_UI_OK, 0.5f);
    }
}
static void renderOverlay() {
    float u = G.cv.px, cx = G.W / 2.f;
    if (G.over && G.overT > 0.8f) {
        G.cv.rect(0, 0, (float)G.W, (float)G.H, Col(0, 0, 0, 150));
        int ws = 0; for (int s = 0; s < 4; s++) if (G.side[s].used && G.side[s].team == G.winTeam) { ws = s; break; }
        Col c = G.winTeam >= 0 ? G.side[ws].col : C_WHITE;
        text(G.overMsg, cx, G.H * 0.28f, 54 * u, c, 1);
        float y = G.H * 0.42f;
        for (int s = 0; s < 4; s++) {
            Side& S = G.side[s]; if (!S.used) continue; int afloat = 0; for (auto& sh : S.ship) if (sh.st != 2) afloat++;
            text("P" + std::to_string(s + 1) + (S.cpu ? " CPU" : "") + "   SHIPS LEFT " + std::to_string(afloat) + "   " + (S.alive ? "STILL AT SEA" : S.outMsg), cx, y, 16 * u, S.col, 1); y += 30 * u;
        }
        int mins = (int)(G.time / 60), secs = (int)G.time % 60; char tb[32]; snprintf(tb, sizeof tb, "BATTLE TIME %d:%02d", mins, secs);
        text(tb, cx, y + 10 * u, 13 * u, C_DIM, 1);
        if (G.overT > 1.5f) { const char* it[2] = { "PLAY AGAIN", "BACK TO LOBBY" }; menuList(it, 2, G.resultSel, cx, G.H * 0.72f, 24 * u, 44 * u); }
    } else if (G.paused) {
        G.cv.rect(0, 0, (float)G.W, (float)G.H, Col(0, 0, 0, 160));
        text("PAUSED", cx, G.H * 0.3f, 48 * u, C_WHITE, 1);
        const char* it[3] = { "RESUME", "HOW TO PLAY", "QUIT TO TITLE" }; menuList(it, 3, G.pauseSel, cx, G.H * 0.45f, 26 * u, 48 * u);
    }
}

// ---------------------------------------------------------------- frame
static void saveShot() {
    int w = G.W, h = G.H; SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return; SDL_RenderReadPixels(G.ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, G.shotPath.c_str()); SDL_FreeSurface(s); printf("shot: saved %s\n", G.shotPath.c_str());
}
static void renderFrame(bool shot) {
    G.cv.glow = G.opt.glow;
    if (G.useTarget) SDL_SetRenderTarget(G.ren, G.target);
    SDL_SetRenderDrawColor(G.ren, 2, 6, 14, 255); SDL_RenderClear(G.ren);
    if (G.state == ST_GAME) { drawGame(); renderOverlay(); } else renderMenus();
    G.cv.flush(G.ren);
    if (shot) saveShot();
    if (G.useTarget) { SDL_SetRenderTarget(G.ren, nullptr); SDL_RenderCopy(G.ren, G.target, nullptr, nullptr); }
    SDL_RenderPresent(G.ren);
}

static void testSetup() {
    // IH_STATE=game IH_PLAYERS=n IH_BOTS=1 (every side CPU) IH_HUMAN=1 (P1 = keyboard) IH_BATTLE=1 (force a battle at once)
    if (getenv("IH_WIIFAKE")) {
#ifdef IH_WII
        for (int i = 0; i < atoi(getenv("IH_WIIFAKE")); i++) G.wii.addFake();
#endif
    }
    if (getenv("IH_LOBBY")) { G.state = ST_LOBBY; G.lobbySel = 6; }
    if (!getenv("IH_STATE")) return;
    std::string st = getenv("IH_STATE");
    if (st == "lobby") { G.state = ST_LOBBY; return; }
    if (st == "help") { G.state = ST_HELP; return; }
    if (st == "options") { G.state = ST_OPTIONS; return; }
    int n = getenv("IH_PLAYERS") ? atoi(getenv("IH_PLAYERS")) : 2;
    for (int i = 0; i < 4; i++) { G.slotKind[i] = i < n ? 2 : 0; G.slotDiff[i] = 1 + (i % 2); }
    if (getenv("IH_HUMAN")) { G.slotKind[0] = 1; G.slotDev[0] = -1; }
    if (getenv("IH_TEAMS")) G.teams = 1;
    if (getenv("IH_MAPRANDOM")) G.mapKind = 1;
    startMatch();
    if (getenv("IH_BATTLE")) {
        int a[3] = { 0, 2, 4 }; int b2[3] = { 0, 2, 4 };
        int f0 = launchFleet(0, a, 3); int s1 = -1; for (int i = 1; i < 4; i++) if (G.side[i].used) { s1 = i; break; }
        int f1 = launchFleet(s1, b2, 3);
        G.fl[f0].p = V2(MW / 2 - 330, MH / 2 - 260); G.fl[f1].p = V2(MW / 2 - 250, MH / 2 - 240);
        G.fl[f0].p = nearestFree(G.fl[f0].p); G.fl[f1].p = nearestFree(G.fl[f1].p);
        startBattle(f0, f1);
    }
    if (getenv("IH_CONTACT")) {   // test: a P1 fleet and a P2 fleet meet in open sea (the general-quarters alarm)
        int a[2] = { 2, 4 }, b2[2] = { 2, 4 }; int s1 = 1; for (int i = 1; i < 4; i++) if (G.side[i].used) { s1 = i; break; }
        int f0 = launchFleet(0, a, 2), f1 = launchFleet(s1, b2, 2);
        G.fl[f0].p = nearestFree(V2(MW / 2 - 500, MH / 2 - 330)); G.fl[f1].p = nearestFree(G.fl[f0].p + V2(70, 10)); G.side[0].sel = f0;
    }
    if (getenv("IH_MENU")) { G.side[0].menu = true; G.side[0].menuSel = 2; G.side[0].pick[0] = 0; G.side[0].pick[1] = 2; G.side[0].npick = 2; }
}

static struct { double acc = 0; Uint64 last = 0; float fpsT = 0; int fpsN = 0; } LOOP;
static bool frame() {
#ifdef __EMSCRIPTEN__
    { int cw = EM_ASM_INT({ return window.innerWidth; }), ch = EM_ASM_INT({ return window.innerHeight; }); int ww, wh; SDL_GetWindowSize(G.win, &ww, &wh);
      if (cw > 0 && ch > 0 && (cw != ww || ch != wh)) { SDL_SetWindowSize(G.win, cw, ch); setupTarget(); } }
#endif
    for (auto& d : G.devs) d.mouseMoved = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT: G.quit = true; break;
        case SDL_CONTROLLERDEVICEADDED: openPad(e.cdevice.which); break;
        case SDL_JOYDEVICEADDED: if (!SDL_IsGameController(e.jdevice.which)) openPad(e.jdevice.which); break;
        case SDL_CONTROLLERDEVICEREMOVED: closePad(e.cdevice.which); break;
        case SDL_CONTROLLERBUTTONDOWN: { Dev* p = devById(e.cbutton.which); int b = padButton(e.cbutton.button); if (p && b >= 0) p->hit[b] = true; break; }
        case SDL_MOUSEBUTTONDOWN: { Dev* k = devById(-1); if (k) { if (e.button.button == SDL_BUTTON_LEFT) { k->click = true; k->hit[B_A] = true; } else if (e.button.button == SDL_BUTTON_RIGHT) k->rclick = true; } break; }
        case SDL_MOUSEMOTION: { Dev* k = devById(-1); if (k) k->mouseMoved = true; break; }
        case SDL_KEYDOWN: {
            Dev* k = devById(-1); if (!k || e.key.repeat) break;
            switch (e.key.keysym.scancode) {
            case SDL_SCANCODE_SPACE: case SDL_SCANCODE_RETURN: k->hit[B_A] = true; break; case SDL_SCANCODE_BACKSPACE: k->hit[B_B] = true; break;
            case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_P: k->hit[G.state == ST_GAME ? B_START : B_B] = true; break;
            case SDL_SCANCODE_R: k->hit[B_Y] = true; break; case SDL_SCANCODE_F: k->hit[B_X] = true; break; case SDL_SCANCODE_T: k->hit[B_RT] = true; break;
            case SDL_SCANCODE_Q: k->hit[B_LB] = true; break; case SDL_SCANCODE_E: k->hit[B_RB] = true; break;
            case SDL_SCANCODE_UP: case SDL_SCANCODE_W: k->hit[B_UP] = true; break; case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S: k->hit[B_DOWN] = true; break;
            default: break;
            }
            break; }
        case SDL_WINDOWEVENT: if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) setupTarget(); break;
        }
    }
#ifdef IH_WII
    syncWii();
#endif
    Uint64 now = SDL_GetPerformanceCounter(); double dt = (double)(now - LOOP.last) / SDL_GetPerformanceFrequency(); LOOP.last = now;
    if (G.shotFrame >= 0) dt = 1 / 60.0;
    if (dt > 0.1) dt = 0.1;
    LOOP.fpsT += (float)dt; LOOP.fpsN++; if (LOOP.fpsT > 30) { printf("fps: %.1f (fleets %d, particles %d)\n", LOOP.fpsN / LOOP.fpsT, (int)G.fl.size(), (int)G.parts.size()); LOOP.fpsT = 0; LOOP.fpsN = 0; }
    readDevs((float)dt);
    // IH_BTEST (with IH_HUMAN IH_BATTLE): keyboard player fires, switches to the next ship, steers it, fires
    // IH_BTEST=2: carrier strike locked on the nearest enemy ship, then the battleship aims with the group opened wide
    if (getenv("IH_BTEST") && atoi(getenv("IH_BTEST")) == 2 && G.state == ST_GAME && G.bt[0].live) { Dev* k = devById(-1); int f = G.frame; Battle& b = G.bt[0];
        auto lockOn = [&]() { float bd = 1e9f; for (auto& s : b.ships) if (s.side == 1 && bAlive(s) && dist(s.p, b.ships[b.active[0]].p) < bd) { bd = dist(s.p, b.ships[b.active[0]].p); b.cross[0] = s.p; } };
        if (f == 59) for (auto& s : b.ships) if (s.side == 1) s.p.x -= 320;   // bring the enemy inside strike range
        if (f >= 60 && f < 100) { k->b[B_A] = true; if (f > 62) lockOn(); }
        if (f == 330) k->pend[B_B] = true;
        if (f >= 340 && f < 420) { k->b[B_A] = true; k->b[B_RT] = f < 380; if (f > 342) { lockOn(); b.cross[0].x -= 60; } } }
    // IH_BTEST=3: both carriers launch at the same moment at each other (dogfight check)
    else if (getenv("IH_BTEST") && atoi(getenv("IH_BTEST")) == 3 && G.state == ST_GAME && G.bt[0].live) { Battle& b = G.bt[0]; int f = G.frame;
        if (f == 59) for (auto& s : b.ships) if (s.side == 1) s.p.x -= 320;
        if (f == 70) { int c[2] = { -1, -1 }; for (int i = 0; i < (int)b.ships.size(); i++) if (b.ships[i].t == T_CARRIER) c[b.ships[i].side] = i;
            if (c[0] >= 0 && c[1] >= 0) { b.ships[c[0]].reload = b.ships[c[1]].reload = 0; fire(b, 0, c[0], b.ships[c[1]].p); fire(b, 0, c[1], b.ships[c[0]].p); } } }
    else if (getenv("IH_BTEST") && G.state == ST_GAME) { Dev* k = devById(-1); int f = G.frame;
        if (f >= 60 && f < 110) { k->b[B_A] = true; k->mv = V2(0.2f, -0.6f); }
        if (f == 140) k->pend[B_B] = true;
        if (f >= 150 && f < 230) { k->b[B_RIGHT] = true; k->mv = V2(1, 0); }
        if (f >= 240 && f < 290) { k->b[B_A] = true; k->mv = V2(0.3f, -0.5f); } }
    // BACK + START on a pad (Batocera's HOTKEY + START) leaves the match
    for (auto& d : G.devs) if (d.gc && d.b[B_BACK] && d.b[B_START] && (pr(d, B_START) || pr(d, B_BACK))) { if (G.state == ST_GAME) quitToTitle(); else G.quit = true; usePresses(); }
    if (G.state == ST_GAME) {
        if (G.paused || G.over) { updateOverlay((float)dt); usePresses(); }
        if (!G.paused && !G.over) for (int s = 0; s < 4; s++) { Dev* d = G.side[s].used ? sideDev(s) : nullptr; if (d && pr(*d, B_START) && !G.side[s].menu) { G.paused = true; G.pauseSel = 0; G.pauser = s; AU.play(SND_UI_OK, 0.6f); usePresses(); break; } }
        if (!G.paused && G.state == ST_GAME) {
            LOOP.acc += dt * G.fast;
            int guard = 0;
            while (LOOP.acc >= 1 / 60.0 && guard++ < 240) {
                LOOP.acc -= 1 / 60.0;
                if (!G.over) { layoutPanels(); for (int s = 0; s < 4; s++) updateSide(s, 1 / 60.f); }
                stepWorld(1 / 60.f);
                usePresses();
            }
        }
    } else { updateMenus((float)dt); usePresses(); }
    G.frame++;
    bool shot = G.shotFrame >= 0 && G.frame == G.shotFrame;
    renderFrame(shot);
    if (shot) return false;
    if (getenv("IH_UNTILOVER") && G.state == ST_GAME && G.over && G.overT > 2) { printf("test: game finished\n"); return false; }
    if (getenv("IH_MAXTIME") && G.time > atof(getenv("IH_MAXTIME"))) { printf("test: time limit, no winner yet\n"); return false; }
    return !G.quit;
}

static int gameMain() {
    setvbuf(stdout, nullptr, _IOLBF, 0); setvbuf(stderr, nullptr, _IOLBF, 0);
#ifdef __EMSCRIPTEN__
    SDL_SetMainReady();
#endif
    printf("Iron Harbor starting\n");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) { printf("SDL_Init failed: %s\n", SDL_GetError()); return 1; }
    const char* home = getenv("IH_HOME"); char* base = SDL_GetBasePath();
    GAMEDIR = home ? home : (base ? base : "."); while (GAMEDIR.size() > 1 && GAMEDIR.back() == '/') GAMEDIR.pop_back();
    G.cfgPath = GAMEDIR + "/ironharbor.cfg"; if (base) SDL_free(base);
    loadCfg();
    if (getenv("IH_SHOT")) G.shotFrame = atoi(getenv("IH_SHOT"));
    if (getenv("IH_SHOT_PATH")) G.shotPath = getenv("IH_SHOT_PATH");
    if (getenv("IH_FAST")) G.fast = std::max(1, atoi(getenv("IH_FAST")));
    if (getenv("IH_RES")) G.opt.res = atoi(getenv("IH_RES"));
    if (getenv("IH_SEED_RNG")) R.s = (uint32_t)atoi(getenv("IH_SEED_RNG")); else R.s = (uint32_t)time(nullptr) | 1;
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
    Uint32 wf = SDL_WINDOW_RESIZABLE; int ww = EM_ASM_INT({ return window.innerWidth; }), wh = EM_ASM_INT({ return window.innerHeight; });
#else
    Uint32 wf = SDL_WINDOW_FULLSCREEN_DESKTOP; int ww = 1280, wh = 720;
#endif
    if (getenv("IH_WINDOW")) { wf = 0; sscanf(getenv("IH_WINDOW"), "%dx%d", &ww, &wh); }
    G.win = SDL_CreateWindow("Iron Harbor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, wf | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!G.win) { printf("window failed: %s\n", SDL_GetError()); return 1; }
    G.ren = SDL_CreateRenderer(G.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (!G.ren) G.ren = SDL_CreateRenderer(G.win, -1, SDL_RENDERER_TARGETTEXTURE);
    if (!G.ren) { printf("renderer failed: %s\n", SDL_GetError()); return 1; }
    SDL_RendererInfo ri; SDL_GetRendererInfo(G.ren, &ri); printf("renderer: %s\n", ri.name);
    setupTarget();
    if (!getenv("IH_MUTE")) AU.open();
    applyAudioVol();
    Dev kb; kb.id = -1; kb.name = "KEYBOARD"; G.devs.push_back(kb);
#ifdef IH_WII
    if (!getenv("IH_NOWII") && G.shotFrame < 0) G.wii.start();
#endif
    testSetup();
    LOOP.last = SDL_GetPerformanceCounter();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop([] { frame(); }, 0, 0);
    return 0;
#else
    while (frame()) {}
#endif
    saveCfg();
#ifdef IH_WII
    G.wii.shutdown();
#endif
    AU.close();
    SDL_DestroyRenderer(G.ren); SDL_DestroyWindow(G.win); SDL_Quit();
    printf("Iron Harbor exit\n");
    return 0;
}

#ifdef __EMSCRIPTEN__
extern "C" EMSCRIPTEN_KEEPALIVE int ih_main() { static bool started = false; if (started) return 0; started = true; return gameMain(); }
int main() { return 0; }
#else
int main(int, char**) { return gameMain(); }
#endif
