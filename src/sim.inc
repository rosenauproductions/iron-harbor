// Iron Harbor - the strategy map: fleets moving, mines, sweeping, docking, contacts, captures, the end of the game
// (included from main.cpp)

static void aiInit(int si);   // ai.inc
static const char* btnName(int s, int b);   // ui.inc
static void aiThink(int si, float dt);
static void aiContacts(float dt);

static bool sideInBattle(int s) { for (auto& b : G.bt) if (b.live && (G.fl[b.fl[0]].owner == s || G.fl[b.fl[1]].owner == s)) return true; return false; }
static int liveBattles() { int n = 0; for (auto& b : G.bt) if (b.live) n++; return n; }

static void startMatch() {
    int used[4], nu = 0; for (int i = 0; i < 4; i++) if (G.slotKind[i]) used[nu++] = i;
    G.nSides = nu;
    V2 hp[4], hd[4]; harborLayout(nu, hp, hd);
    G.seed = G.mapKind == 0 ? (nu <= 2 ? 1980u : 1981u) : (unsigned)time(nullptr) ^ (unsigned)SDL_GetTicks();
    if (getenv("IH_SEED")) G.seed = (unsigned)atoi(getenv("IH_SEED"));
    genMap(G.seed, nu);
    G.fl.clear(); G.mines.clear(); G.con.clear(); G.parts.clear(); for (auto& b : G.bt) b.live = false;
    for (int i = 0; i < 4; i++) {
        Side& S = G.side[i]; S = Side(); S.idx = i; S.col = PCOL[i];
        if (!G.slotKind[i]) continue;
        S.used = true; S.cpu = G.slotKind[i] == 2 || G.allCpu; S.dev = G.slotDev[i]; S.diff = G.slotDiff[i];
        S.team = G.teams ? i % 2 : i;
        int h = nu <= 2 ? (i == used[0] ? 0 : 1) : i;
        S.harbor = hp[h]; S.hdir = hd[h];
        int k = 0; for (int t = 0; t < NT; t++) for (int c = 0; c < SD[t].count; c++) { S.ship[k].t = t; S.ship[k].hp = SD[t].armor; S.ship[k].st = 0; k++; }
        S.cur = S.harbor + S.hdir * 220;
    }
    G.openInfo = G.opt.info == 2 || (G.opt.info == 0 && humans() <= 1);
    G.time = 0; G.pingT = 1; G.over = 0; G.overT = 0; G.winTeam = -1; G.paused = false; G.banT = 0;
    for (int i = 0; i < 4; i++) if (G.side[i].used && G.side[i].cpu) aiInit(i);
    G.state = ST_GAME; G.stateT = 0;
    banner("GENERAL QUARTERS", C_WARN);
    AU.play(SND_KLAXON, 0.5f);
    printf("match: %d admirals (%d human), %s, map %s seed %u, info %s\n", nu, humans(), G.teams ? "teams" : "free for all", G.mapKind ? "random" : "classic", G.seed, G.openInfo ? "open" : "classic");
}

static void hitFlagship(Fleet& f, float dmg, Col c) {
    int k = flagship(f); if (k < 0) return;
    Ship& s = shipOf(f, k); s.hp -= dmg;
    explode(-1, f.p, 10, Col(255, 200, 120)); AU.play(SND_MINE, 0.7f); rumbleSide(f.owner, 300);
    if (s.hp <= 0) { s.hp = 0; s.st = 2; explode(-1, f.p, 16, c); AU.play(SND_BOOM, 0.6f); printf("mine: P%d %s sunk\n", f.owner + 1, SD[s.t].name); }
    compactFleet(f);
    toast(G.side[f.owner], f.dead ? "FLEET LOST TO A MINEFIELD" : std::string(SD[s.t].shortName) + (s.st == 2 ? " SUNK BY A MINE" : " HIT A MINE"));
}

static void dockFleet(int fi) {
    Fleet& f = G.fl[fi]; Side& S = G.side[f.owner];
    if (S.docks <= 0) { toast(S, "NO REPAIRS LEFT"); f.dock = false; return; }
    for (int k = 0; k < f.n; k++) { Ship& s = shipOf(f, k); if (s.st == 1) { s.st = 0; s.hp = SD[s.t].armor; } }
    f.dead = true; S.docks--; if (S.sel == fi) S.sel = -1;
    toast(S, "DOCKED AND REPAIRED (" + std::to_string(S.docks) + " LEFT)"); AU.play(SND_DOCK, 0.6f);
    ring(-1, S.harbor, 10, 90, S.col, 0.8f);
}

static void moveFleet(int fi, float dt) {
    Fleet& f = G.fl[fi];
    float spd = fleetKnots(f) * 1.25f;
    if (f.moving && !f.path.empty()) {
        float step = spd * dt;
        while (step > 0 && !f.path.empty()) {
            V2 to = f.path.front(); V2 d = to - f.p; float L = len(d);
            if (L <= step) { f.p = to; step -= L; f.path.erase(f.path.begin()); }
            else { f.p += d / L * step; step = 0; float a = std::atan2(d.y, d.x); f.ang += angDiff(f.ang, a) * std::min(1.f, dt * 5); }
        }
        if (f.path.empty()) f.moving = false;
        f.trailT -= dt; if (f.trailT <= 0) { f.trailT = 0.5f; f.trail.push_back(f.p); if (f.trail.size() > 14) f.trail.erase(f.trail.begin()); }
    } else if (!f.trail.empty()) { f.trailT -= dt; if (f.trailT <= 0) { f.trailT = 0.5f; f.trail.erase(f.trail.begin()); } }
    if (!f.moving && f.dock && dist(f.p, G.side[f.owner].harbor) < HARBOR_R + 70) dockFleet(fi);
}

static void stepMap(float dt) {
    G.time += dt;
    G.pingT -= dt;
    if (G.pingT <= 0) { G.pingT = 3.2f; AU.play(SND_PING, 0.28f); for (auto& s : G.side) if (s.used && s.alive) ring(-1, s.harbor, 10, 260, withA(s.col, 0.5f), 2.6f); }
    for (int i = 0; i < (int)G.fl.size(); i++) {
        Fleet& f = G.fl[i]; if (f.dead || f.battle >= 0) continue;
        f.flashT += dt;
        moveFleet(i, dt); if (f.dead) continue;
        // mines and sweeping
        for (int mi = 0; mi < (int)G.mines.size(); mi++) {
            Mine& m = G.mines[mi]; if (m.dead || !hostile(m.owner, f.owner)) continue;
            float d = dist(m.p, f.p);
            if (d < m.r + (f.sweep ? 14 : 0) && f.sweep && hasType(f, T_SWEEPER)) { m.dead = true; printf("sweep: P%d cleared a P%d minefield\n", f.owner + 1, m.owner + 1); if (G.openInfo && !G.side[f.owner].cpu) AU.play(SND_SWEEP, 0.4f); continue; }
            if (d < m.r) {
                if (f.lastMine != mi) { f.lastMine = mi; f.mineCd = 3; m.revealed = true; hitFlagship(f, MINE_DMG, G.side[f.owner].col); }
                else { f.mineCd -= dt; if (f.mineCd <= 0) { f.mineCd = 3; hitFlagship(f, MINE_DMG, G.side[f.owner].col); } }
                if (f.dead) break;
            } else if (f.lastMine == mi) f.lastMine = -1;
        }
        if (f.dead) continue;
        // capture: a carrier or troop transport (any ship once both are sunk) sails into an enemy harbor
        for (int s = 0; s < 4; s++) {
            Side& S = G.side[s]; if (!S.used || !S.alive || !hostile(s, f.owner)) continue;
            if (dist(f.p, S.harbor) < HARBOR_R + 45 && fleetCanCapture(f)) {
                eliminate(s, "HARBOR CAPTURED BY P" + std::to_string(f.owner + 1));
                rumbleSide(f.owner, 600);
                explode(-1, S.harbor, 30, S.col);
            }
        }
    }
    // contacts: hostile fleets close together flash; either admiral may engage while the window is open
    for (int i = 0; i < (int)G.fl.size(); i++) for (int j = i + 1; j < (int)G.fl.size(); j++) {
        Fleet& a = G.fl[i]; Fleet& b = G.fl[j];
        int ci = -1; for (int k = 0; k < (int)G.con.size(); k++) if ((G.con[k].a == i && G.con[k].b == j)) ci = k;
        bool valid = !a.dead && !b.dead && a.battle < 0 && b.battle < 0 && hostile(a.owner, b.owner) && G.side[a.owner].alive && G.side[b.owner].alive;
        float d = valid ? dist(a.p, b.p) : 1e9f;
        if (ci < 0 && d < CONTACT_R) {
            Contact c; c.a = i; c.b = j; c.t = CONTACT_T; c.cpuT[0] = R.range(0.6f, 2.0f); c.cpuT[1] = R.range(0.6f, 2.0f); G.con.push_back(c);
            AU.play(SND_PING, 0.45f, 0, 1.3f);
            if (!G.side[a.owner].cpu || !G.side[b.owner].cpu) { AU.play(SND_GQ, 0.6f, 0, 1, 0); G.con.back().alarmT = 1.75f; }
            for (int o : { a.owner, b.owner }) if (!G.side[o].cpu) { toast(G.side[o], std::string(btnName(o, B_Y)) + " TO ENGAGE"); rumbleSide(o, 160); }
        } else if (ci >= 0) {
            Contact& c = G.con[ci];
            if (d > CONTACT_R * 1.6f) { G.con.erase(G.con.begin() + ci); continue; }
            if (!c.passed) {
                c.t -= dt; if (c.t <= 0) c.passed = true;
                // all hands on deck: the alarm bell rings for the local players in this contact until someone engages or they pass
                bool human = !G.side[a.owner].cpu || !G.side[b.owner].cpu;
                c.alarmT -= dt;
                if (human && !c.passed && c.alarmT <= 0) {
                    c.alarmT = 1.75f; AU.play(SND_GQ, 0.55f, 0, 1, 0);
                    for (int o : { a.owner, b.owner }) if (!G.side[o].cpu) rumbleSide(o, 80);
                }
            }
        }
    }
    aiContacts(dt);
    for (int s = 0; s < 4; s++) if (G.side[s].used && G.side[s].alive && G.side[s].cpu) aiThink(s, dt);
    for (auto& m : G.mines) if (!m.dead && !G.side[m.owner].alive) m.dead = true;
}

// open contacts that involve this side (fleet a or b owned by it), newest first
static int contactFor(int s, int preferFleet) {
    int best = -1;
    for (int k = 0; k < (int)G.con.size(); k++) {
        Contact& c = G.con[k]; if (c.passed) continue;
        Fleet& a = G.fl[c.a]; Fleet& b = G.fl[c.b];
        if (a.dead || b.dead || a.battle >= 0 || b.battle >= 0) continue;
        if (a.owner != s && b.owner != s) continue;
        if (c.a == preferFleet || c.b == preferFleet) return k;
        if (best < 0) best = k;
    }
    return best;
}
static bool engage(int k) {
    Contact& c = G.con[k];
    int oa = G.fl[c.a].owner, ob = G.fl[c.b].owner;
    if (sideInBattle(oa) || sideInBattle(ob)) return false;   // one battle per admiral at a time
    c.passed = true;
    return startBattle(c.a, c.b) >= 0;
}

static void checkWin() {
    if (G.over) return;
    for (int s = 0; s < 4; s++) {
        Side& S = G.side[s]; if (!S.used || !S.alive) continue;
        bool any = false; for (auto& sh : S.ship) if (sh.st != 2) any = true;
        if (!any) eliminate(s, "LOST EVERY SHIP");
    }
    int teamsAlive = 0, last = -1; bool seen[4] = {};
    for (int s = 0; s < 4; s++) if (G.side[s].used && G.side[s].alive && !seen[G.side[s].team]) { seen[G.side[s].team] = true; teamsAlive++; last = G.side[s].team; }
    if (teamsAlive <= 1) {
        G.over = 1; G.overT = 0; G.winTeam = last; G.resultSel = 0;
        std::string who; for (int s = 0; s < 4; s++) if (G.side[s].used && G.side[s].team == last) who += (who.empty() ? "P" : " + P") + std::to_string(s + 1);
        G.overMsg = last < 0 ? "NO ADMIRAL LEFT" : who + " WINS";
        AU.play(SND_WIN, 0.8f);
        printf("game over at %.0f s: %s\n", G.time, G.overMsg.c_str());
    }
}

static void stepWorld(float dt) {
    if (G.over) { G.overT += dt; }
    G.banT = std::max(0.f, G.banT - dt);
    for (auto& s : G.side) s.toastT = std::max(0.f, s.toastT - dt);
    // battles always run at full speed
    for (int i = 0; i < 4; i++) if (G.bt[i].live) { stepBattle(i, dt); if (G.bt[i].overT > 2.6f) finishBattle(i); }
    // the map pauses when nobody is left to play it (the classic way); otherwise it runs at half speed during battles
    int nb = liveBattles(); bool freeHuman = false;
    for (int s = 0; s < 4; s++) if (G.side[s].used && G.side[s].alive && !G.side[s].cpu && !sideInBattle(s)) freeHuman = true;
    G.mapRunning = nb == 0 || freeHuman || humans() == 0;
    if (G.mapRunning && !G.over) stepMap(dt * SPEEDV[G.opt.speed] * (nb ? 0.5f : 1.f));
    for (auto& p : G.parts) { p.life -= dt; p.p += p.v * dt; p.v *= std::pow(0.25f, dt); }
    G.parts.erase(std::remove_if(G.parts.begin(), G.parts.end(), [](const Part& p) { return p.life <= 0; }), G.parts.end());
    checkWin();
}
