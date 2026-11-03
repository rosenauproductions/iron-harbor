// Iron Harbor - CPU admirals (included from main.cpp)
// Each CPU sends out fleets from a few plans: a strike group around the carrier, a hunter group, a mine group that seeds the
// approaches to its own harbor, and a second landing group around the troop transport. Ships come home to repair.

enum { R_ATTACK = 1, R_MINE, R_HUNT, R_HOME };
static const int PLANS[4][3] = {
    { T_CARRIER, T_BATTLESHIP, T_DESTROYER },
    { T_BATTLESHIP, T_SUB, T_DESTROYER },
    { T_LAYER, T_SWEEPER, T_PT },
    { T_TRANSPORT, T_DESTROYER, T_SWEEPER },
};

static int nearestEnemyHarbor(int si, V2 from) {
    int best = -1; float bd = 1e9f;
    for (int s = 0; s < 4; s++) if (G.side[s].used && G.side[s].alive && hostile(s, si)) { float d = dist(from, G.side[s].harbor); if (d < bd) { bd = d; best = s; } }
    return best;
}

static void aiInit(int si) {
    Side& S = G.side[si]; S.mineSpots.clear(); S.deployT = 1 + si * 0.7f; S.plan = R.i(2);
    // mines go on the sea lanes into this harbor: points along the paths enemies would take
    for (int s = 0; s < 4; s++) {
        if (!G.side[s].used || !hostile(s, si)) continue;
        std::vector<V2> p; if (!navPath(S.harbor + S.hdir * 90, G.side[s].harbor + G.side[s].hdir * 90, p)) continue;
        V2 cur = S.harbor + S.hdir * 90; float L = 0; float want[2] = { 330, 560 }; int wi = 0;
        for (auto& q : p) {
            float seg = dist(cur, q);
            while (wi < 2 && L + seg >= want[wi]) { V2 at = cur + (q - cur) * ((want[wi] - L) / std::max(seg, 1e-3f)); bool far = true; for (auto& m : S.mineSpots) if (dist(m, at) < 170) far = false; if (far) S.mineSpots.push_back(at); wi++; }
            L += seg; cur = q;
        }
    }
    while (S.mineSpots.size() > MAXMINES) S.mineSpots.pop_back();
}

static float shipPower(const Ship& s) { return s.st == 1 ? std::max(0.f, s.hp) * SD[s.t].dmg / SD[s.t].reload : 0; }
static float fleetPower(const Fleet& f) { float p = 0; for (int k = 0; k < f.n; k++) p += shipPower(shipOf(f, k)); return p; }

static void aiDeploy(int si) {
    Side& S = G.side[si];
    if (fleetsAtSea(si) >= MAXFLEETS) return;
    int pick[3], n = 0;
    static const int ORDER[2][4] = { { 1, 0, 2, 3 }, { 2, 1, 0, 3 } };   // plan 0: hunters lead; plan 1: mines first
    for (int pi = 0; pi < 4 && n == 0; pi++) {
        const int* pl = PLANS[ORDER[S.plan][pi]];
        if (shipInHarbor(si, pl[0]) < 0) continue;
        for (int k = 0; k < 3; k++) { int i = shipInHarbor(si, pl[k]); if (i >= 0) pick[n++] = i; }
    }
    if (n == 0) {   // whatever is left: capture ships first, then the strongest, one of each type
        bool used[NT] = {};
        int order[NT] = { T_CARRIER, T_TRANSPORT, T_BATTLESHIP, T_SUB, T_DESTROYER, T_SWEEPER, T_LAYER, T_PT };
        for (int t : order) { if (n >= 3) break; int i = shipInHarbor(si, t); if (i >= 0 && !used[t]) { pick[n++] = i; used[t] = true; } }
    }
    if (n > 0) { int fi = launchFleet(si, pick, n); if (fi >= 0) printf("cpu P%d: launched fleet %d (%d ships)\n", si + 1, fi, n); }
}

static void aiThink(int si, float dt) {
    Side& S = G.side[si];
    S.thinkT -= dt; if (S.thinkT > 0) return; S.thinkT = 0.5f;
    S.deployT -= 0.5f;
    if (S.deployT <= 0) { aiDeploy(si); S.deployT = S.diff == 0 ? 12 : S.diff == 1 ? 7 : 5; }
    int minesOut = 0; for (auto& m : G.mines) if (!m.dead && m.owner == si) minesOut++;
    int attackFleet = -1;
    for (int i = 0; i < (int)G.fl.size(); i++) { Fleet& f = G.fl[i]; if (!f.dead && f.owner == si && f.battle < 0 && fleetCanCapture(f)) { attackFleet = i; break; } }
    for (int i = 0; i < (int)G.fl.size(); i++) {
        Fleet& f = G.fl[i]; if (f.dead || f.owner != si || f.battle >= 0) continue;
        f.aiT -= 0.5f;
        int tgt = nearestEnemyHarbor(si, f.p); if (tgt < 0) return;
        V2 th = G.side[tgt].harbor;
        // come home to repair
        if (f.role != R_HOME && fleetHpFrac(f) < 0.45f && S.docks > 0 && dist(f.p, th) > 650) { f.role = R_HOME; orderMove(i, S.harbor, true); continue; }
        if (f.role == R_HOME) { if (!f.moving) orderMove(i, S.harbor, true); continue; }
        bool layer = hasType(f, T_LAYER);
        if (fleetCanCapture(f)) f.role = R_ATTACK;
        else if (layer && minesOut < MAXMINES && (int)S.mineSpots.size() > 0) f.role = R_MINE;
        else f.role = R_HUNT;
        if (f.role == R_ATTACK) {
            bool sw = hasType(f, T_SWEEPER) && dist(f.p, th) < 1300; if (sw != f.sweep) f.sweep = sw;
            if (G.time < (S.diff == 2 ? 14 : 20)) continue;   // let the escorts get out first
            if (!f.moving || f.aiT <= 0) { orderMove(i, th, false); f.aiT = 6; }
        } else if (f.role == R_MINE) {
            V2 spot = S.mineSpots.front();
            if (dist(f.p, spot) < 28) { layMine(i); S.mineSpots.erase(S.mineSpots.begin()); f.aiT = 0; }
            else if (!f.moving || f.aiT <= 0) { orderMove(i, spot, false); f.aiT = 5; }
        } else {
            // hunt: enemies near our harbor first, then whatever is nearest; otherwise escort the landing group
            int best = -1; float bd = 1e9f;
            for (int j = 0; j < (int)G.fl.size(); j++) {
                Fleet& e = G.fl[j]; if (e.dead || !hostile(e.owner, si) || e.battle >= 0) continue;
                float threat = dist(e.p, S.harbor) < 900 ? 0.4f : 1.f;
                float d = dist(e.p, f.p) * threat; if (d < bd) { bd = d; best = j; }
            }
            if (f.aiT <= 0 || !f.moving) {
                if (best >= 0 && (bd < 1300 || attackFleet < 0)) orderMove(i, G.fl[best].p, false);
                else if (attackFleet >= 0) orderMove(i, G.fl[attackFleet].p + perp(norm(th - G.fl[attackFleet].p)) * 90, false);
                else orderMove(i, S.harbor + S.hdir * 420, false);
                f.aiT = 3;
            }
        }
    }
}

static void aiContacts(float dt) {
    for (auto& c : G.con) {
        if (c.passed) continue;
        for (int k = 0; k < 2; k++) {
            int fi = k ? c.b : c.a, oi = k ? c.a : c.b; Fleet& f = G.fl[fi]; Fleet& o = G.fl[oi]; Side& S = G.side[f.owner];
            if (!S.cpu || f.dead || o.dead) continue;
            c.cpuT[k] -= dt; if (c.cpuT[k] > 0) continue;
            float mine = fleetPower(f), theirs = fleetPower(o) * R.range(0.85f, 1.15f);
            bool defend = dist(o.p, S.harbor) < 750 || fleetCanCapture(o);
            float need = f.role == R_ATTACK ? 1.5f : S.diff == 2 ? 0.6f : 0.8f;
            if (defend) need *= 0.6f;
            if (S.diff == 0) need *= R.range(0.7f, 1.4f);
            if (mine > theirs * need) {
                int idx = (int)(&c - &G.con[0]);
                if (!engage(idx)) { c.cpuT[k] = 0.5f; continue; }
                printf("cpu P%d engages (power %.0f vs %.0f)\n", f.owner + 1, mine, theirs);
                return;   // the contact list may have changed
            }
            c.cpuT[k] = 1e9f;
        }
    }
}
