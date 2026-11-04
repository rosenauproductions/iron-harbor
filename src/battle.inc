// Iron Harbor - battles: a zoomed patch of the real map, every ship of both fleets, one ship steered per admiral
// (included from main.cpp)

static Dev* sideDev(int s);                 // ui.inc
static bool pr(const Dev& d, int b);        // ui.inc
static float AIERR[3] = { 0.15f, 0.09f, 0.05f };

static V2 toB(const Battle& b, V2 m) { return (m - b.center) * BK + V2(BW / 2, BH / 2); }
static float bLandDist(const Battle& b, V2 q) {
    float best = 1e9f;
    for (auto& p : b.land) { float d = polyDist(p, q); if (inPoly(p, q)) return -d; best = std::min(best, d); }
    return best;
}
static bool bBlocked(const Battle& b, V2 q, float pad) { return q.x < pad || q.y < pad || q.x > BW - pad || q.y > BH - pad || bLandDist(b, q) < pad; }
static V2 bFwd(const BShip& s) { return fromAng(s.ang); }
static bool bAlive(const BShip& s) { return s.sinkT < 0; }
static bool hullHit(const BShip& s, V2 q, float pad) {
    V2 d = q - s.p, f = bFwd(s); float lx = dot(d, f), ly = dot(d, perp(f));
    return std::fabs(lx) < SD[s.t].L * 0.5f + pad && std::fabs(ly) < SD[s.t].Wd * 0.5f + pad;
}
static int shipUnder(const Battle& b, int side, V2 q, float pad) {
    for (int i = 0; i < (int)b.ships.size(); i++) if (b.ships[i].side != side && bAlive(b.ships[i]) && hullHit(b.ships[i], q, pad)) return i;
    return -1;
}
static int sideAlive(const Battle& b, int sd) { int n = 0; for (auto& s : b.ships) if (s.side == sd && bAlive(s)) n++; return n; }
static float sideHp(const Battle& b, int sd) { float h = 0; for (auto& s : b.ships) if (s.side == sd && bAlive(s)) h += G.side[s.owner].ship[s.ship].hp; return h; }
static float sidePower(const Battle& b, int sd) { float h = 0; for (auto& s : b.ships) if (s.side == sd && bAlive(s)) h += G.side[s.owner].ship[s.ship].hp * SD[s.t].dmg / SD[s.t].reload; return h; }
static int bIndexOf(int bi) { return bi; }

static void nextActive(Battle& b, int sd, int dir = 1) {
    int n = (int)b.ships.size(), cur = b.active[sd];
    for (int k = 1; k <= n; k++) {
        int i = ((cur < 0 ? 0 : cur) + dir * k + n * 4) % n;
        if (b.ships[i].side == sd && bAlive(b.ships[i])) { if (b.active[sd] >= 0) b.ships[b.active[sd]].aiming = false; b.active[sd] = i; b.aiming[sd] = false; return; }
    }
    b.active[sd] = -1;
}
static bool controlled(const Battle& b, int k) { return b.active[b.ships[k].side] == k; }

static int startBattle(int fa, int fb) {
    int bi = -1; for (int i = 0; i < 4; i++) if (!G.bt[i].live) { bi = i; break; }
    if (bi < 0) return -1;
    Battle& b = G.bt[bi]; b = Battle(); b.live = true; b.fl[0] = fa; b.fl[1] = fb;
    Fleet& A = G.fl[fa]; Fleet& Bf = G.fl[fb];
    float hw = BW / BK / 2, hh = BH / BK / 2;
    b.center = (A.p + Bf.p) * 0.5f; b.center.x = clampf(b.center.x, hw, MW - hw); b.center.y = clampf(b.center.y, hh, MH - hh);
    for (auto& is : G.isl) {
        if (is.maxx < b.center.x - hw - 20 || is.minx > b.center.x + hw + 20 || is.maxy < b.center.y - hh - 20 || is.miny > b.center.y + hh + 20) continue;
        std::vector<V2> p; for (auto& q : is.pts) p.push_back(toB(b, q));
        b.land.push_back(p); b.landC.push_back(toB(b, is.c));
    }
    for (int sd = 0; sd < 2; sd++) {
        Fleet& f = G.fl[b.fl[sd]];
        V2 d = toB(b, f.p) - V2(BW / 2, BH / 2); if (len(d) < 30) d = V2(sd ? 1.f : -1.f, 0); d = norm(d);
        V2 base = V2(BW / 2, BH / 2) + V2(d.x * 520, d.y * 300);
        int k = 0;
        for (int i = 0; i < f.n; i++) {
            Ship& sh = shipOf(f, i); if (sh.st != 1) continue;
            BShip s; s.side = sd; s.owner = f.owner; s.fleet = b.fl[sd]; s.ship = f.s[i]; s.t = sh.t;
            float off = (k - (aliveIn(f) - 1) * 0.5f) * 120; k++;
            V2 want = base + perp(d) * off;
            float pad = SD[s.t].L * 0.6f + 10;
            V2 at = want; bool ok = !bBlocked(b, at, pad);
            for (float r = 20; r < 600 && !ok; r += 20) for (int a = 0; a < 16 && !ok; a++) { V2 q = want + fromAng(a * PI / 8) * r; if (!bBlocked(b, q, pad)) { at = q; ok = true; } }
            s.p = at; s.ang = std::atan2(-d.y, -d.x); s.spd = 0; s.thr = 0; s.telegraph = 0;   // every ship starts stopped s.reload = R.range(0.5f, 1.5f); s.wobble = R.f() < 0.5f ? 1.f : -1.f;
            b.ships.push_back(s);
            if (i == flagship(f)) b.active[sd] = (int)b.ships.size() - 1;
        }
        f.battle = bi; f.moving = f.moving; // keeps its orders for after the battle
    }
    b.msg = "P" + std::to_string(A.owner + 1) + "  VS  P" + std::to_string(Bf.owner + 1);
    AU.play(SND_KLAXON, 0.55f);
    rumbleSide(A.owner, 250); rumbleSide(Bf.owner, 250);
    printf("battle %d: P%d fleet %d (%d ships) vs P%d fleet %d (%d ships) at %.0f,%.0f\n", bi, A.owner + 1, fa, aliveIn(A), Bf.owner + 1, fb, aliveIn(Bf), b.center.x, b.center.y);
    return bi;
}

static void bDamage(Battle& b, int bi, int k, float dmg, bool aground = false) {
    BShip& s = b.ships[k]; if (!bAlive(s)) return;
    Ship& sh = G.side[s.owner].ship[s.ship]; sh.hp -= dmg; s.flash = 1;
    if (!aground) { sparks(bi, s.p, 10, Col(255, 190, 90), 140); AU.play(SND_HIT, 0.5f); }
    if (sh.hp <= 0) {
        sh.hp = 0; sh.st = 2; s.sinkT = 0;
        explode(bi, s.p, 26, G.side[s.owner].col); AU.play(SND_BOOM, 0.75f); b.shake = std::max(b.shake, 14.f);
        rumbleSide(s.owner, 400);
        printf("battle %d: P%d %s sunk\n", bi, s.owner + 1, SD[s.t].name);
        if (b.active[s.side] == k) nextActive(b, s.side);
    } else rumbleSide(s.owner, 90);
}

// where each mount's shot lands around the aim point: a tight group laid out across the line of fire
static V2 reticleOffset(int t, int i, V2 dir, float spr = 1) {
    int n = MOUNTS[t].n; V2 side = perp(dir) * spr; dir = dir * spr;
    if (SD[t].torp) return side * ((i - (n - 1) * 0.5f) * 26);          // torpedoes: side by side
    if (n == 2) return side * (i ? 15.f : -15.f);
    if (n == 3) return i == 0 ? dir * 14 : dir * -9 + side * (i == 1 ? -16.f : 16.f);   // battleship: a triangle
    return V2();
}
static V2 mountPos(const BShip& s, int i) {
    const Mount& m = MOUNTS[s.t].m[i]; V2 f = bFwd(s);
    return s.p + f * (m.x * SD[s.t].L) + perp(f) * (m.y * SD[s.t].Wd);
}
// carrier aircraft: roll down the deck, take off, fly to the target (or chase the locked ship), bomb, fly back and land
static const float PLANE_SPEED = 270, PLANE_TURN = 2.6f;
static void stepPlane(Battle& b, int bi, Shot& sh, float dt) {
    BShip& cv = b.ships[sh.shooter]; const SDef& d = SD[cv.t]; sh.t += dt;
    auto steer = [&](V2 goal, float spd) {
        float want = std::atan2(goal.y - sh.p.y, goal.x - sh.p.x);
        sh.ang += clampf(angDiff(sh.ang, want), -PLANE_TURN * dt, PLANE_TURN * dt);
        sh.spd += (spd - sh.spd) * std::min(1.f, dt * 2); sh.p += fromAng(sh.ang) * sh.spd * dt;
    };
    if (sh.st == 0) {   // the deck run
        if (!bAlive(cv)) { sh.dead = true; return; }
        float k = clampf(sh.t / 0.8f, 0, 1); V2 f = bFwd(cv);
        V2 a = mountPos(cv, sh.mount), z = cv.p + f * (d.L * 0.55f) + perp(f) * (MOUNTS[cv.t].m[sh.mount].y * d.Wd);
        sh.p = a + (z - a) * (k * k); sh.ang = cv.ang; sh.spd = cv.spd + 40 + 230 * k; sh.alt = std::max(0.f, (k - 0.7f) / 0.3f) * 0.25f;
        if (k >= 1) { sh.st = 1; AU.play(SND_TORP, 0.25f, 0, 1.8f, 0); }
        return;
    }
    if (sh.st == 3) {   // shot down: spiral into the sea
        sh.ang += 5 * dt; sh.spd *= std::pow(0.5f, dt); sh.p += fromAng(sh.ang) * sh.spd * dt; sh.alt -= dt * 0.6f;
        if (R.f() < dt * 20) addPart(bi, P_SMOKE, sh.p, V2(R.range(-5, 5), R.range(-5, 5)), 1.2f, Col(60, 60, 70), 3, 10);
        if (sh.alt <= 0) { splash(bi, sh.p, 10); AU.play(SND_SPLASH, 0.35f, 0, 1.3f); sh.dead = true; }
        return;
    }
    sh.cool -= dt;
    sh.alt = std::min(1.f, sh.alt + dt * 0.8f);
    if (sh.st == 1) {
        if (sh.tgt >= 0 && !bAlive(b.ships[sh.tgt])) sh.tgt = -1;   // the target sank: bomb where it was
        V2 goal = sh.tgt >= 0 ? b.ships[sh.tgt].p + b.ships[sh.tgt].v * 0.25f : sh.to;
        if (sh.tgt >= 0) sh.to = b.ships[sh.tgt].p;
        steer(goal, PLANE_SPEED);
        if (dist(sh.p, goal) < 26 || sh.t > 20) {
            int hit = sh.tgt >= 0 ? sh.tgt : shipUnder(b, sh.side, sh.p, 8);
            if (sh.tgt >= 0) {   // a locked target can still dodge: the faster and nimbler it is moving, the better its chance
                const BShip& t = b.ships[sh.tgt]; const SDef& td = SD[t.t];
                float speed = clampf(std::fabs(t.spd) / (td.knots * BKN), 0, 1);
                float dodge = clampf(speed * (0.25f + 0.35f * td.accel) * (0.5f + 0.5f * clampf(td.turn / 1.6f, 0, 1)) + 0.05f, 0.05f, 0.6f);
                if (R.f() < dodge) { hit = -1; sh.p += fromAng(R.range(0, 6.28f)) * 28; printf("battle %d: %s dodged a plane (%.0f%%)\n", bi, td.shortName, dodge * 100); }
            }
            printf("battle %d: P%d plane %s\n", bi, b.ships[sh.shooter].owner + 1, hit >= 0 ? (std::string("hit the ") + SD[b.ships[hit].t].shortName).c_str() : "missed");
            if (hit >= 0) bDamage(b, bi, hit, sh.dmg);
            else if (bLandDist(b, sh.p) < 0) sparks(bi, sh.p, 8, Col(200, 180, 120), 60);
            else { splash(bi, sh.p, 12); AU.play(SND_SPLASH, 0.3f, 0, 1.1f); }
            ring(bi, sh.p, 3, 30, Col(255, 200, 120), 0.4f);
            sh.st = 2; sh.t = 0;
        }
        return;
    }
    // home: line up behind the carrier and land on the deck; with the carrier gone, the plane ditches
    if (!bAlive(cv)) { steer(sh.p + fromAng(sh.ang) * 100, PLANE_SPEED); if (sh.t > 3) { splash(bi, sh.p, 8); sh.dead = true; } return; }
    V2 f = bFwd(cv), deck = cv.p - f * (d.L * 0.2f);
    steer(dist(sh.p, deck) > 160 ? deck - f * 120 : deck, dist(sh.p, deck) < 140 ? 150 : PLANE_SPEED);
    if (dist(sh.p, deck) < 140) sh.alt = std::max(0.f, sh.alt - dt * 1.6f);
    if (dist(sh.p, deck) < 24 || sh.t > 25) { sh.dead = true; AU.play(SND_UI, 0.3f); printf("battle %d: P%d plane landed\n", bi, cv.owner + 1); }
}
static bool planesOut(const Battle& b, int k) { for (auto& sh : b.shots) if (sh.plane && sh.shooter == k && !sh.dead) return true; return false; }

static void fire(Battle& b, int bi, int k, V2 at, float spr = 1) {
    BShip& s = b.ships[k]; const SDef& d = SD[s.t]; int n = MOUNTS[s.t].n;
    V2 v = at - s.p; if (len(v) > d.range) at = s.p + norm(v) * d.range;
    V2 dir = norm(at - s.p);
    for (int i = 0; i < n; i++) {
        Shot sh; sh.side = s.side; sh.from = mountPos(s, i); sh.to = at + reticleOffset(s.t, i, dir, spr); sh.p = sh.from;
        sh.dmg = d.dmg / n; sh.torp = d.torp; sh.range = d.range; sh.shooter = k; sh.mount = i; sh.wait = i * (d.torp ? 0.18f : 0.11f);
        if (s.t == T_CARRIER) { sh.plane = true; sh.tgt = shipUnder(b, s.side, at, 14); sh.wait = i * 0.55f; sh.ang = s.ang; }   // crosshair on a ship: the planes go for that ship
        else if (d.torp) sh.dir = norm(sh.to - sh.from); else sh.dur = dist(sh.from, sh.to) / GUN_SPEED + 0.25f;
        b.shots.push_back(sh);
    }
    s.reload = d.reload;
}

// --- one ship's helm: a human's controls or the CPU's
struct Helm { bool use = false; V2 stick; bool analog = false; int rudder = 0, tele = 0; bool aimHeld = false; V2 aimMove; };

static void cpuHelm(Battle& b, int bi, int k, float err, float dt) {
    BShip& s = b.ships[k]; const SDef& d = SD[s.t];
    s.retarget -= dt;
    if (s.target < 0 || s.retarget <= 0 || !bAlive(b.ships[s.target])) {
        s.target = -1; float bd = 1e9f;
        for (int i = 0; i < (int)b.ships.size(); i++) if (b.ships[i].side != s.side && bAlive(b.ships[i])) { float dd = dist(b.ships[i].p, s.p); if (dd < bd) { bd = dd; s.target = i; } }
        s.retarget = 1.5f;
    }
    if (s.target < 0) { s.thr = 0.3f; return; }
    BShip& t = b.ships[s.target];
    V2 dv = t.p - s.p; float dd = len(dv), base = std::atan2(dv.y, dv.x);
    float dr = d.range * (d.torp ? (s.t == T_PT ? 0.4f : 0.5f) : 0.62f);
    float want; float thr;
    if (dd > dr * 1.15f) { want = base; thr = 1; }
    else if (dd < dr * 0.6f) { want = base + s.wobble * 2.2f; thr = 0.9f; }
    else { want = base + s.wobble * PI / 2; thr = 0.65f; }
    if (std::fmod(b.t + k * 3.1f, 17.f) < dt) s.wobble = -s.wobble;
    // steer around land and the edge of the battle sea
    static const float OFFS[9] = { 0, 0.45f, -0.45f, 0.9f, -0.9f, 1.5f, -1.5f, 2.3f, -2.3f };
    float pad = d.Wd + 14, chosen = want; bool found = false;
    for (float o : OFFS) {
        float a = want + o; V2 f = fromAng(a); bool ok = true;
        for (float pr2 : { d.L * 0.6f + 40, d.L * 0.6f + 110, d.L * 0.6f + 190 }) if (bBlocked(b, s.p + f * pr2, pad)) { ok = false; break; }
        if (ok) { chosen = a; found = true; break; }
    }
    if (!found) { chosen = s.ang + PI * 0.7f * s.wobble; thr = 0.4f; }
    float steer = clampf(angDiff(s.ang, chosen) * 2.2f, -1, 1);
    // aiming: like a human, the ship stops driving while the gunners lay the guns
    if (s.aiming) {
        s.aimT -= dt; s.thr = 0;
        if (s.aimT <= 0) {
            s.aiming = false;
            V2 aim = t.p;
            float spdT = d.torp ? TORP_SPEED : GUN_SPEED;
            for (int it = 0; it < 3; it++) { float ft = dist(s.p, aim) / spdT + (d.torp ? 0 : 0.25f); aim = t.p + t.v * ft; }
            float e = err * dd; aim += V2(R.range(-1, 1), R.range(-1, 1)) * e;
            if (dist(s.p, aim) <= d.range * 1.02f) fire(b, bi, k, aim);
        }
    } else {
        s.thr = thr;
        float turn = d.turn * 1.15f * clampf(0.3f + s.spd / (d.knots * BKN), 0, 1);
        s.ang += steer * turn * dt;
        if (s.reload <= 0 && dd < d.range * 0.95f) {
            // torpedoes: only when the target is roughly ahead (they run straight from the bow)
            if (!d.torp || std::fabs(angDiff(s.ang, base)) < 1.2f) { s.aiming = true; s.aimT = 0.45f + err * 4; }
        }
    }
}

// hold FIRE: crosshair for the controlled ship (it stops driving); release: it fires if loaded and in range
static void humanAim(Battle& b, int bi, int sd, const Helm& h, float dt) {
    float maxR = 0; V2 c; float fx = 0, fy = 0; int n = 0;
    for (int k = 0; k < (int)b.ships.size(); k++) if (b.ships[k].side == sd && bAlive(b.ships[k]) && controlled(b, k)) {
        BShip& s = b.ships[k]; maxR = std::max(maxR, SD[s.t].range); c += s.p; fx += std::cos(s.ang); fy += std::sin(s.ang); n++;
    }
    if (!n) { b.aiming[sd] = false; return; }
    c = c / (float)n; V2 fw = norm(V2(fx, fy));
    if (h.aimHeld) {
        if (!b.aiming[sd]) { b.aiming[sd] = true; b.cross[sd] = c + fw * std::min(maxR * 0.55f, 260.f); }
        b.cross[sd] += h.aimMove * (720 * dt);
        V2 v = b.cross[sd] - c; if (len(v) > maxR) b.cross[sd] = c + norm(v) * maxR;
        b.cross[sd].x = clampf(b.cross[sd].x, 0, BW); b.cross[sd].y = clampf(b.cross[sd].y, 0, BH);
        return;
    }
    if (!b.aiming[sd]) return;
    b.aiming[sd] = false; int fired = 0;
    for (int k = 0; k < (int)b.ships.size(); k++) {
        BShip& s = b.ships[k]; if (s.side != sd || !bAlive(s) || !controlled(b, k) || s.reload > 0) continue;
        if (dist(s.p, b.cross[sd]) > SD[s.t].range) continue;   // out of this ship's range: it holds fire
        fire(b, bi, k, b.cross[sd], b.spread[sd]); fired++;
    }
    if (!fired) AU.play(SND_ERR, 0.4f);
    printf("battle %d: P%d fired %d ship%s (%s)\n", bi, G.fl[b.fl[sd]].owner + 1, fired, fired == 1 ? "" : "s", SD[b.ships[b.active[sd]].t].shortName);
}
static void humanDrive(Battle& b, int k, const Helm& h, float dt) {
    BShip& s = b.ships[k]; const SDef& d = SD[s.t];
    if (b.aiming[s.side]) { s.thr = 0; return; }   // aiming: engines stop, the ship coasts
    float turn = d.turn * 1.15f * clampf(0.3f + s.spd / (d.knots * BKN), 0, 1);
    if (h.analog && len(h.stick) > 0.3f) {
        float want = std::atan2(h.stick.y, h.stick.x);
        s.ang += clampf(angDiff(s.ang, want) * 2.5f, -1, 1) * turn * dt;
        s.thr = clampf((len(h.stick) - 0.3f) / 0.6f, 0.2f, 1);
        s.telegraph = (int)std::lround(s.thr * 3);
    } else {
        if (h.tele) s.telegraph = std::max(0, std::min(3, s.telegraph + h.tele));
        s.thr = s.telegraph / 3.f;
        s.ang += h.rudder * turn * dt;
    }
}
// a ship the human isn't controlling right now: it sits (engines stop, no firing)
static void sitShip(BShip& s) { s.thr = 0; s.aiming = false; }

static void stepBattle(int bi, float dt) {
    Battle& b = G.bt[bi]; b.t += dt; b.shake *= std::pow(0.03f, dt);
    if (b.overT >= 0) { b.overT += dt; }
    for (int sd = 0; sd < 2 && b.overT < 0; sd++) {
        int owner = G.fl[b.fl[sd]].owner; Side& S = G.side[owner];
        if (b.active[sd] >= 0 && !bAlive(b.ships[b.active[sd]])) nextActive(b, sd);
        Dev* dv = S.cpu ? nullptr : sideDev(owner);
        Helm h;
        if (dv) {
            h.use = true; h.analog = dv->analog; h.stick = dv->mv; h.aimHeld = dv->b[B_A];
            h.aimMove = dv->mv;
            h.rudder = (dv->b[B_RIGHT] ? 1 : 0) - (dv->b[B_LEFT] ? 1 : 0);
            if (pr(*dv, B_UP)) h.tele = 1; if (pr(*dv, B_DOWN)) h.tele = -1;
            if (!dv->analog && !dv->b[B_A]) h.stick = V2();
            if (pr(*dv, B_B) || pr(*dv, B_RB)) { nextActive(b, sd, 1); AU.play(SND_UI, 0.5f); }
            if (pr(*dv, B_LB)) { nextActive(b, sd, -1); AU.play(SND_UI, 0.5f); }
            if (h.tele) AU.play(SND_UI, 0.4f);
            {   // open / tighten the salvo group: triggers or right stick on pads; + / - on Wii remotes and T / F on the keyboard
                float dz = dv->gc ? (dv->rt - dv->lt - dv->ry) : ((dv->b[B_RT] ? 1.f : 0.f) - (dv->b[B_X] ? 1.f : 0.f));
                b.spread[sd] = clampf(b.spread[sd] + dz * 1.4f * dt, 0.3f, 3.0f);
            }
            if (pr(*dv, B_Y)) {
                if (b.retreat[sd] < 0) { b.retreat[sd] = 15; AU.play(SND_KLAXON, 0.4f); toast(S, "RETREAT IN 15 S - Y AGAIN TO STAY"); }
                else { b.retreat[sd] = -1; AU.play(SND_UI_OK, 0.5f); toast(S, "RETREAT CANCELLED"); }
            }
        } else if (b.retreat[sd] < 0 && b.t > 6) {
            float mine = sidePower(b, sd), theirs = sidePower(b, 1 - sd);
            bool precious = fleetCanCapture(G.fl[b.fl[sd]]);
            if (mine < theirs * (precious ? 0.55f : 0.3f)) { b.retreat[sd] = 15; printf("battle %d: P%d orders a retreat\n", bi, owner + 1); }
        }
        if (b.retreat[sd] >= 0) { b.retreat[sd] -= dt; if (b.retreat[sd] <= 0) { b.fled = sd; b.overT = 0; b.winner = -1; } }
        if (h.use) humanAim(b, bi, sd, h, dt);
        for (int k = 0; k < (int)b.ships.size(); k++) {
            BShip& s = b.ships[k]; if (s.side != sd || !bAlive(s)) continue;
            if (!controlled(b, k)) sitShip(s);   // one ship at a time, for CPU admirals too
            else if (h.use) humanDrive(b, k, h, dt);
            else cpuHelm(b, bi, k, S.cpu ? AIERR[S.diff] : 0.12f, dt);
        }
    }
    // physics
    for (int k = 0; k < (int)b.ships.size(); k++) {
        BShip& s = b.ships[k]; const SDef& d = SD[s.t];
        s.flash = std::max(0.f, s.flash - dt * 3); s.reload -= dt;
        if (s.t == T_CARRIER && planesOut(b, k)) s.reload = std::max(s.reload, 0.5f);   // waits for its planes
        if (!bAlive(s)) { s.sinkT += dt; s.spd *= std::pow(0.3f, dt); s.p += bFwd(s) * s.spd * dt; continue; }
        if (b.overT >= 0) s.thr = std::min(s.thr, 0.4f);
        float maxS = d.knots * BKN, want = s.thr * maxS;
        float acc = d.accel * 42; if (want < s.spd) acc *= 1.4f;
        s.spd += clampf(want - s.spd, -acc * dt, acc * dt);
        V2 old = s.p; s.p += bFwd(s) * s.spd * dt;
        // run aground: the bow (or the hull centre) touching land
        V2 bow = s.p + bFwd(s) * d.L * 0.45f;
        bool hitLand = bLandDist(b, bow) < 2 || bLandDist(b, s.p) < d.Wd * 0.5f;
        if (hitLand) {
            s.p = old; s.spd = -s.spd * 0.25f;
            if (s.landT <= 0) { bDamage(b, bi, k, LAND_DMG, true); AU.play(SND_AGROUND, 0.6f); sparks(bi, bow, 8, Col(200, 180, 120), 80); s.landT = 2; }
        }
        s.landT -= dt;
        s.p.x = clampf(s.p.x, 16, BW - 16); s.p.y = clampf(s.p.y, 16, BH - 16);
        s.v = (s.p - old) / std::max(dt, 1e-4f);
        s.wakeT -= dt; if (s.wakeT <= 0 && s.spd > 8) { s.wakeT = 0.12f; s.wake.push_back(s.p - bFwd(s) * d.L * 0.5f); if (s.wake.size() > 26) s.wake.erase(s.wake.begin()); }
        float frac = G.side[s.owner].ship[s.ship].hp / d.armor;
        s.smokeT -= dt; if (frac < 0.5f && s.smokeT <= 0) { s.smokeT = 0.18f + frac * 0.4f; addPart(bi, P_SMOKE, s.p, V2(R.range(-6, 6), R.range(-14, -4)), R.range(1.2f, 2.0f), Col(70, 70, 80), 5, 18); }
    }
    // ships push each other apart
    for (size_t i = 0; i < b.ships.size(); i++) for (size_t j = i + 1; j < b.ships.size(); j++) {
        BShip& a = b.ships[i]; BShip& c = b.ships[j]; if (!bAlive(a) || !bAlive(c)) continue;
        float r = (SD[a.t].L + SD[c.t].L) * 0.3f; V2 dv = c.p - a.p; float dd = len(dv);
        if (dd < r && dd > 0.01f) { V2 push = dv / dd * (r - dd) * 0.5f; a.p -= push; c.p += push; }
    }
    // shots
    for (auto& sh : b.shots) {
        if (sh.dead) continue;
        if (sh.wait > 0) {
            sh.wait -= dt;
            BShip& src = b.ships[sh.shooter];
            if (bAlive(src)) { V2 np = mountPos(src, sh.mount); if (!sh.plane) sh.to += np - sh.from; sh.from = np; sh.p = np; sh.ang = src.ang; if (sh.torp) sh.dir = norm(sh.to - sh.from); }
            if (sh.wait > 0) continue;
            if (!bAlive(src)) { sh.dead = true; continue; }
            if (sh.plane) AU.play(SND_LAUNCH, 0.3f, 0, 1.5f, 0);
            else if (sh.torp) AU.play(SND_TORP, 0.4f, 0, R.range(0.9f, 1.1f), 0);
            else { AU.play(SND_GUN, 0.4f, 0, R.range(0.88f, 1.12f), 0); ring(bi, sh.from, 2, 16, Col(255, 230, 160), 0.25f); addPart(bi, P_DOT, sh.from, V2(), 0.12f, Col(255, 220, 150), 9); }
            sh.wait = 0; sh.t = 0;
        }
        if (sh.plane) { stepPlane(b, bi, sh, dt); continue; }
        if (!sh.torp) {
            sh.t += dt; float u = clampf(sh.t / sh.dur, 0, 1); sh.p = sh.from + (sh.to - sh.from) * u;
            if (u >= 1) {
                sh.dead = true; int hit = -1;
                for (int k = 0; k < (int)b.ships.size(); k++) if (b.ships[k].side != sh.side && bAlive(b.ships[k]) && hullHit(b.ships[k], sh.to, 7)) { hit = k; break; }
                if (hit >= 0) bDamage(b, bi, hit, sh.dmg);
                else if (bLandDist(b, sh.to) < 0) { sparks(bi, sh.to, 8, Col(200, 180, 120), 60); }
                else { splash(bi, sh.to, 14); AU.play(SND_SPLASH, 0.35f, 0, R.range(0.9f, 1.2f)); }
            }
        } else {
            float step = TORP_SPEED * dt; sh.p += sh.dir * step; sh.gone += step;
            for (int k = 0; k < (int)b.ships.size(); k++) if (b.ships[k].side != sh.side && bAlive(b.ships[k]) && hullHit(b.ships[k], sh.p, 3)) { bDamage(b, bi, k, sh.dmg); sh.dead = true; ring(bi, sh.p, 4, 40, C_WHITE, 0.5f); break; }
            if (!sh.dead && (bLandDist(b, sh.p) < 0 || sh.gone > sh.range || sh.p.x < 0 || sh.p.y < 0 || sh.p.x > BW || sh.p.y > BH)) { sh.dead = true; splash(bi, sh.p, 8); }
        }
    }
    // dogfights: enemy planes that cross paths in the air trade fire; each may be shot down
    for (size_t i = 0; i < b.shots.size(); i++) for (size_t j = i + 1; j < b.shots.size(); j++) {
        Shot& a = b.shots[i]; Shot& c = b.shots[j];
        if (!a.plane || !c.plane || a.dead || c.dead || a.side == c.side || a.wait > 0 || c.wait > 0) continue;
        if (a.st < 1 || a.st > 2 || c.st < 1 || c.st > 2 || a.alt < 0.6f || c.alt < 0.6f || a.cool > 0 || c.cool > 0) continue;
        if (dist(a.p, c.p) > 48) continue;
        a.cool = c.cool = 3;
        for (int t = 0; t < 6; t++) { V2 q = a.p + (c.p - a.p) * R.f(); addPart(bi, P_SPARK, q, fromAng(R.range(0, 6.28f)) * 40, 0.3f, Col(255, 230, 140)); }
        AU.play(SND_HIT, 0.4f, 0, 1.6f, 0);
        for (Shot* pl : { &a, &c }) if (R.f() < 0.35f) {
            pl->st = 3; explode(bi, pl->p, 6, Col(255, 180, 90));
            printf("battle %d: dogfight - a P%d plane was shot down\n", bi, b.ships[pl->shooter].owner + 1);
        }
        printf("battle %d: dogfight\n", bi);
    }
    b.shots.erase(std::remove_if(b.shots.begin(), b.shots.end(), [](const Shot& s) { return s.dead; }), b.shots.end());
    // end?
    if (b.overT < 0) {
        bool ownerGone[2] = { !G.side[G.fl[b.fl[0]].owner].alive, !G.side[G.fl[b.fl[1]].owner].alive };
        int a0 = ownerGone[0] ? 0 : sideAlive(b, 0), a1 = ownerGone[1] ? 0 : sideAlive(b, 1);
        if (a0 == 0 || a1 == 0) { b.overT = 0; b.winner = a0 ? 0 : a1 ? 1 : -1; AU.play(SND_UI_OK, 0.5f); }
    }
}

static void finishBattle(int bi) {
    Battle& b = G.bt[bi];
    for (int sd = 0; sd < 2; sd++) {
        Fleet& f = G.fl[b.fl[sd]]; f.battle = -1; compactFleet(f);
        if (!G.side[f.owner].alive) f.dead = true;
        if (f.dead) explode(-1, f.p, 12, G.side[f.owner].col);
    }
    if (b.fled >= 0) {
        Fleet& f = G.fl[b.fl[b.fled]]; Fleet& o = G.fl[b.fl[1 - b.fled]];
        if (!f.dead) { V2 away = norm(f.p - o.p); if (len(f.p - o.p) < 1) away = V2(1, 0); f.p = nearestFree(f.p + away * 50); }
    }
    for (auto& c : G.con) if ((c.a == b.fl[0] && c.b == b.fl[1]) || (c.a == b.fl[1] && c.b == b.fl[0])) c.passed = true;
    int o0 = G.fl[b.fl[0]].owner, o1 = G.fl[b.fl[1]].owner;
    printf("battle %d over: %s\n", bi, b.winner >= 0 ? (b.winner == 0 ? "side A won" : "side B won") : b.fled >= 0 ? "a fleet withdrew" : "both sunk");
    (void)o0; (void)o1;
    for (int i = (int)G.parts.size() - 1; i >= 0; i--) if (G.parts[i].layer == bi) { G.parts[i] = G.parts.back(); G.parts.pop_back(); }
    b.live = false;
}
