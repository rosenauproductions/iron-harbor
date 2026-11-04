// Iron Harbor - drawing: the tactical map, battle panels, HUD (included from main.cpp)

static const char* btnName(int s, int b);   // ui.inc: button label for this side's controller
struct View { float s = 1; V2 o; V2 w2s(V2 p) const { return o + p * s; } V2 s2w(V2 q) const { return (q - o) / s; } };
static View VMAP; static bool MAPSHOWN = true; static SDL_Rect ROSTER = { 0, 0, 0, 0 };
static View fitView(SDL_Rect r, float w, float h) { View v; v.s = std::min(r.w / w, r.h / h); v.o = V2(r.x + (r.w - w * v.s) / 2, r.y + (r.h - h * v.s) / 2); return v; }

static void clip(const SDL_Rect* r) { G.cv.flush(G.ren); SDL_RenderSetClipRect(G.ren, r); }
static void panel(float x, float y, float w, float h, Col edge, float u, int alpha = 225) { G.cv.rect(x, y, w, h, Col(4, 12, 24, alpha)); G.cv.rectLine(x, y, w, h, 1.3f * u, edge, 0.5f); }
static void text(const std::string& s, float x, float y, float size, Col c, int align = 0) { FONT.draw(G.cv, s.c_str(), V2(x, y), size, c, align); }

static void layoutPanels() {
    int nb = 0, ids[4]; for (int i = 0; i < 4; i++) if (G.bt[i].live) ids[nb++] = i;
    float W = (float)G.W, H = (float)G.H; ROSTER = { 0, 0, 0, 0 };
    MAPSHOWN = true; G.mapVp = { 0, 0, G.W, G.H };
    if (nb == 0) return;
    if (nb == 1) {
        if (G.mapRunning) {
            int bw = (int)(W * 0.66f); G.bt[ids[0]].vp = { 0, 0, bw, G.H };
            int mw = G.W - bw, mh = (int)(mw * MH / MW); G.mapVp = { bw, 0, mw, mh }; ROSTER = { bw, mh, mw, G.H - mh };
        } else { G.bt[ids[0]].vp = { 0, 0, G.W, G.H }; MAPSHOWN = false; }
        return;
    }
    if (nb == 2 && !G.mapRunning) { G.bt[ids[0]].vp = { 0, 0, G.W / 2, G.H }; G.bt[ids[1]].vp = { G.W / 2, 0, G.W - G.W / 2, G.H }; MAPSHOWN = false; return; }
    SDL_Rect q[4] = { { 0, 0, G.W / 2, G.H / 2 }, { G.W / 2, 0, G.W - G.W / 2, G.H / 2 }, { 0, G.H / 2, G.W / 2, G.H - G.H / 2 }, { G.W / 2, G.H / 2, G.W - G.W / 2, G.H - G.H / 2 } };
    for (int k = 0; k < nb && k < 3; k++) G.bt[ids[k]].vp = q[k];
    if (G.mapRunning && nb < 4) { G.mapVp = q[nb]; if (nb < 3) ROSTER = q[3]; } else MAPSHOWN = false;
    (void)H;
}

// ---------------------------------------------------------------- islands (shared by map and battle)
static void drawLand(const std::vector<V2>& pts, V2 c, const View& v, float u, bool detail) {
    std::vector<V2> s; s.reserve(pts.size()); for (auto& p : pts) s.push_back(v.w2s(p));
    V2 sc = v.w2s(c);
    for (size_t i = 0; i < s.size(); i++) G.cv.tri(G.cv.fillV, sc, s[i], s[(i + 1) % s.size()], Col(16, 52, 40), Col(14, 44, 34), Col(14, 44, 34));
    // shallows
    std::vector<V2> sh; for (auto& p : s) sh.push_back(sc + (p - sc) * 1.0f + norm(p - sc) * (detail ? 10 : 5) * u);
    G.cv.poly(sh.data(), (int)sh.size(), 1.0f * u, Col(60, 150, 200, 90), 0.3f);
    G.cv.poly(s.data(), (int)s.size(), 2.0f * u, C_LAND, 0.9f);
    std::vector<V2> in; for (auto& p : s) in.push_back(sc + (p - sc) * 0.68f);
    G.cv.poly(in.data(), (int)in.size(), 1.0f * u, Col(70, 170, 110, 120), 0.3f);
    if (detail) { std::vector<V2> in2; for (auto& p : s) in2.push_back(sc + (p - sc) * 0.36f); G.cv.poly(in2.data(), (int)in2.size(), 1.0f * u, Col(90, 190, 120, 90), 0.2f); }
}

static void drawParts(int layer, const View& v, float u) {
    for (auto& p : G.parts) {
        if (p.layer != layer) continue;
        float k = p.life / p.max; V2 s = v.w2s(p.p);
        switch (p.kind) {
        case P_SPARK: G.cv.line(s, s - p.v * (0.04f * v.s), 1.5f * u, withA(p.c, k), 0.6f); break;
        case P_DOT: G.cv.glowDisc(s, p.a * v.s * (0.6f + 0.4f * k), withA(p.c, k)); break;
        case P_RING: G.cv.circle(s, lerpf(p.b, p.a, k) * v.s, 1.6f * u, withA(p.c, k), 0.6f); break;
        case P_SMOKE: G.cv.disc(s, lerpf(p.b, p.a, k) * v.s, withA(p.c, k * 0.35f), 12); break;
        default: break;
        }
    }
}

// ---------------------------------------------------------------- map
static int contactOf(int fi) { for (auto& c : G.con) if (!c.passed && (c.a == fi || c.b == fi) && !G.fl[c.a].dead && !G.fl[c.b].dead) return (int)(&c - &G.con[0]); return -1; }
static bool showOwn(int owner) { return G.openInfo && !G.side[owner].cpu; }

static void drawFleetIcon(const Fleet& f, int fi, const View& v, float u, bool small) {
    Side& S = G.side[f.owner]; V2 s = v.w2s(f.p); float sz = (small ? 8 : 13) * u;
    bool contact = contactOf(fi) >= 0;
    Col c = S.col;
    if (contact && std::fmod(G.time * 6, 1.f) < 0.5f) c = C_WHITE;
    else if (!contact && S.sel == fi && !S.cpu && std::fmod(G.time * 1.5f, 1.f) < 0.25f) c = mixc(S.col, C_WHITE, 0.6f);   // the active fleet flashes slowly
    // wake trail
    for (size_t i = 1; i < f.trail.size(); i++) G.cv.line(v.w2s(f.trail[i - 1]), v.w2s(f.trail[i]), 1.2f * u, withA(S.col, 0.12f + 0.2f * i / f.trail.size()), 0.2f);
    V2 fw = fromAng(f.ang), sd = perp(fw);
    V2 pts[4] = { s + fw * sz, s - fw * sz * 0.7f + sd * sz * 0.7f, s - fw * sz * 0.35f, s - fw * sz * 0.7f - sd * sz * 0.7f };
    G.cv.fillPoly(pts, 4, withA(c, 0.35f)); G.cv.poly(pts, 4, 2 * u, c, 1);
    // size pips (every admiral sees how many ships, not which)
    int n = aliveIn(f);
    for (int k = 0; k < n; k++) G.cv.disc(s + V2((k - (n - 1) * 0.5f) * 7 * u, sz + 6 * u), 2.4f * u, c, 8);
    if (f.battle >= 0) { float r = sz * (1.6f + 0.3f * std::sin(G.time * 8)); G.cv.circle(s, r, 1.5f * u, C_WARN, 0.8f); G.cv.line(s + V2(-r, -r) * 0.6f, s + V2(r, r) * 0.6f, 1.5f * u, C_WARN); G.cv.line(s + V2(r, -r) * 0.6f, s + V2(-r, r) * 0.6f, 1.5f * u, C_WARN); }
    if (showOwn(f.owner) && !small) {
        std::string t; for (int k = 0; k < f.n; k++) if (shipOf(f, k).st == 1) t += SD[shipOf(f, k).t].letter;
        text(t, s.x, s.y + sz + 12 * u, 11 * u, withA(S.col, 0.9f), 1);
        float hp = fleetHpFrac(f); G.cv.rect(s.x - 14 * u, s.y - sz - 9 * u, 28 * u, 3 * u, Col(40, 40, 50, 200)); G.cv.rect(s.x - 14 * u, s.y - sz - 9 * u, 28 * u * hp, 3 * u, hp > 0.5f ? C_GOOD : hp > 0.25f ? C_WARN : C_BAD);
        if (f.sweep && hasType(f, T_SWEEPER)) G.cv.circle(s, sz * 2.2f, 1 * u, withA(S.col, 0.5f), 0.3f, 18);
    }
}

static void drawMap(SDL_Rect vp, float u) {
    bool small = vp.w < G.W * 0.6f;
    clip(&vp);
    View v = fitView(vp, MW, MH); VMAP = v;
    G.cv.rect((float)vp.x, (float)vp.y, (float)vp.w, (float)vp.h, Col(2, 8, 18));
    G.cv.rect(v.o.x, v.o.y, MW * v.s, MH * v.s, Col(5, 20, 40));
    for (float x = 200; x < MW; x += 200) G.cv.line(v.w2s(V2(x, 0)), v.w2s(V2(x, MH)), 1 * u, Col(40, 90, 140, 45), 0);
    for (float y = 200; y < MH; y += 200) G.cv.line(v.w2s(V2(0, y)), v.w2s(V2(MW, y)), 1 * u, Col(40, 90, 140, 45), 0);
    G.cv.rectLine(v.o.x, v.o.y, MW * v.s, MH * v.s, 1.5f * u, Col(60, 140, 200, 140), 0.4f);
    for (auto& is : G.isl) drawLand(is.pts, is.c, v, u, false);
    // harbors
    for (int si = 0; si < 4; si++) {
        Side& S = G.side[si]; if (!S.used) continue;
        V2 h = v.w2s(S.harbor); float a = std::atan2(S.hdir.y, S.hdir.x), r = HARBOR_R * v.s;
        Col c = S.alive ? S.col : Col(110, 110, 120);
        G.cv.arc(h, r, a + 0.8f, a + 2 * PI - 0.8f, 3 * u, c, 1);
        G.cv.disc(h, r * 0.85f, withA(c, 0.12f), 24);
        for (int k = -1; k <= 1; k++) { V2 q = h + perp(S.hdir) * (k * r * 0.35f); G.cv.line(q - S.hdir * r * 0.5f, q + S.hdir * r * 0.15f, 2 * u, withA(c, 0.8f), 0.4f); }
        text("P" + std::to_string(si + 1), h.x - S.hdir.x * r * 0.1f, h.y - r - 22 * u, 14 * u, c, 1);
        if (!small) for (int k = 0; k < DOCKS; k++) G.cv.circle(h + V2((k - 1) * 10 * u, -r - 3 * u), 3 * u, 1.2f * u, k < S.docks ? c : withA(c, 0.25f), 0.4f, 8);
        if (!S.alive) { G.cv.line(h + V2(-r, -r) * 0.7f, h + V2(r, r) * 0.7f, 3 * u, C_BAD); G.cv.line(h + V2(r, -r) * 0.7f, h + V2(-r, r) * 0.7f, 3 * u, C_BAD); }
    }
    // mines: invisible, except fields that have gone off, and (open info) your own
    for (auto& m : G.mines) {
        if (m.dead) continue;
        bool own = showOwn(m.owner);
        if (!m.revealed && !own) continue;
        Col c = m.revealed ? Col(255, 90, 70, 150) : withA(G.side[m.owner].col, 0.45f);
        V2 s = v.w2s(m.p); float r = m.r * v.s;
        for (int k = 0; k < 16; k += 2) G.cv.arc(s, r, k * PI / 8 + G.time * 0.2f, (k + 1) * PI / 8 + G.time * 0.2f, 1.3f * u, c, 0.4f);
        if (m.revealed) for (int k = 0; k < 5; k++) { V2 q = s + fromAng(k * 1.26f + 0.3f) * r * 0.5f; G.cv.circle(q, 2.5f * u, 1 * u, c, 0.3f, 6); }
    }
    // orders
    for (int si = 0; si < 4; si++) {
        Side& S = G.side[si]; if (!S.used || S.cpu || S.sel < 0) continue;
        Fleet& f = G.fl[S.sel]; if (f.dead || !f.moving) continue;
        V2 a = v.w2s(f.p); for (auto& q : f.path) { V2 b = v.w2s(q); G.cv.dash(a, b, 6 * u, 6 * u, 1.2f * u, withA(S.col, 0.6f), G.time * 20); a = b; }
        G.cv.circle(a, 5 * u, 1.2f * u, S.col, 0.5f, 10);
    }
    for (int i = 0; i < (int)G.fl.size(); i++) if (!G.fl[i].dead) drawFleetIcon(G.fl[i], i, v, u, small);
    // contact windows
    for (auto& c : G.con) {
        if (c.passed) continue; Fleet& a = G.fl[c.a]; Fleet& b = G.fl[c.b]; if (a.dead || b.dead || a.battle >= 0 || b.battle >= 0) continue;
        V2 m = v.w2s((a.p + b.p) * 0.5f);
        G.cv.dash(v.w2s(a.p), v.w2s(b.p), 4 * u, 4 * u, 1.2f * u, C_WARN, G.time * 30);
        if (!small) text("CONTACT " + std::to_string((int)std::ceil(c.t)), m.x, m.y - 30 * u, 12 * u, C_WARN, 1);
    }
    drawParts(-1, v, u);
    // cursors
    for (int si = 0; si < 4; si++) {
        Side& S = G.side[si]; if (!S.used || S.cpu || !S.alive || sideInBattle(si)) continue;
        V2 c = v.w2s(S.cur); float r = 14 * u;
        G.cv.circle(c, r, 1.8f * u, S.col, 0.8f, 20);
        for (int k = 0; k < 4; k++) { V2 d = fromAng(k * PI / 2); G.cv.line(c + d * r * 0.45f, c + d * r * 1.5f, 1.8f * u, S.col, 0.6f); }
        text("P" + std::to_string(si + 1), c.x + r * 1.2f, c.y - r * 1.6f, 11 * u, S.col);
        if (S.toastT > 0) text(S.toast, c.x, c.y + r * 2, 13 * u, withA(S.col, std::min(1.f, S.toastT * 2)), 1);
    }
    clip(nullptr);
}

// ---------------------------------------------------------------- harbor menu (form a fleet)
static void drawHarborMenu(int si, float u) {
    Side& S = G.side[si]; if (!S.menu || !MAPSHOWN) return;
    float w = 390 * u, h = (NT + 5) * 26 * u + 40 * u;
    V2 hp = VMAP.w2s(S.harbor + S.hdir * 160);
    float x = clampf(hp.x - (S.hdir.x < 0 ? w : 0), G.mapVp.x + 8 * u, G.mapVp.x + G.mapVp.w - w - 8 * u), y = clampf(hp.y - h / 2, G.mapVp.y + 50 * u, G.mapVp.y + G.mapVp.h - h - 8 * u);
    panel(x, y, w, h, S.col, u, 238);
    text("P" + std::to_string(si + 1) + " HARBOR", x + 16 * u, y + 14 * u, 17 * u, S.col);
    text("FLEETS AT SEA " + std::to_string(fleetsAtSea(si)) + "/4   REPAIRS " + std::to_string(S.docks), x + 16 * u, y + 40 * u, 11 * u, C_DIM);
    float ly = y + 66 * u;
    for (int t = 0; t < NT; t++) {
        int have = 0, total = 0; for (auto& sh : S.ship) if (sh.t == t) { total++; if (sh.st == 0) have++; }
        int order = -1; for (int k = 0; k < S.npick; k++) if (S.ship[S.pick[k]].t == t) order = k;
        bool sel = S.menuSel == t;
        Col c = have ? C_WHITE : Col(90, 90, 100);
        if (sel) G.cv.rect(x + 6 * u, ly - 5 * u, w - 12 * u, 24 * u, withA(S.col, 0.22f));
        text(SD[t].name, x + 40 * u, ly, 13 * u, order >= 0 ? S.col : c);
        text(std::to_string(have) + "/" + std::to_string(total), x + w - 20 * u, ly, 13 * u, c, 2);
        if (order >= 0) text(std::to_string(order + 1), x + 18 * u, ly, 13 * u, S.col);
        ly += 26 * u;
    }
    const char* items[2] = { "LAUNCH FLEET", "CLOSE" };
    for (int k = 0; k < 2; k++) {
        bool sel = S.menuSel == NT + k;
        if (sel) G.cv.rect(x + 6 * u, ly - 5 * u, w - 12 * u, 24 * u, withA(S.col, 0.22f));
        text(items[k], x + 40 * u, ly, 14 * u, k == 0 && S.npick == 0 ? Col(90, 90, 100) : sel ? S.col : C_WHITE);
        ly += 26 * u;
    }
    text("FLAGSHIP = FIRST PICKED. ONE OF EACH TYPE.", x + 16 * u, ly + 6 * u, 10 * u, C_DIM);
}

// ---------------------------------------------------------------- HUD
static void drawSideBox(int si, float x, float y, float w, float u) {
    Side& S = G.side[si]; Col c = S.alive ? S.col : Col(110, 110, 120);
    int afloat = 0, inH = 0; for (auto& sh : S.ship) { if (sh.st != 2) afloat++; if (sh.st == 0) inH++; }
    int mines = 0; for (auto& m : G.mines) if (!m.dead && m.owner == si) mines++;
    text("P" + std::to_string(si + 1) + (S.cpu ? " CPU " + std::string(DIFFN[S.diff]) : " ADMIRAL"), x, y, 13 * u, c);
    if (!S.alive) { text(S.outMsg, x, y + 20 * u, 10 * u, C_BAD); return; }
    std::string l2 = "SHIPS " + std::to_string(afloat) + "  FLEETS " + std::to_string(fleetsAtSea(si)) + "/4  REPAIRS " + std::to_string(S.docks);
    text(l2, x, y + 20 * u, 10 * u, C_DIM);
    if (showOwn(si)) text("IN HARBOR " + std::to_string(inH) + "  MINES LEFT " + std::to_string(MAXMINES - mines), x, y + 36 * u, 10 * u, C_DIM);
    if (sideInBattle(si)) text("IN BATTLE", x + w - 10 * u, y, 11 * u, C_WARN, 2);
    (void)w;
}
static void drawHud(float u) {
    if (MAPSHOWN && ROSTER.w == 0) {
        float bh = 56 * u; G.cv.rect(0, 0, (float)G.W, bh, Col(2, 8, 18, 200)); G.cv.line(V2(0, bh), V2((float)G.W, bh), 1 * u, Col(60, 140, 200, 120), 0.3f);
        int n = 0; for (auto& s : G.side) if (s.used) n++;
        float w = G.W / (float)std::max(n, 1); int k = 0;
        for (int si = 0; si < 4; si++) if (G.side[si].used) { drawSideBox(si, k * w + 20 * u, 10 * u, w - 30 * u, u); k++; }
        // controls hint for local players
        int hy = 0;
        for (int si = 0; si < 4; si++) {
            Side& S = G.side[si]; if (!S.used || S.cpu || !S.alive || sideInBattle(si)) continue;
            std::string h = "P" + std::to_string(si + 1) + ": " + btnName(si, B_A) + " SELECT/MOVE  " + btnName(si, B_Y) + " ENGAGE  " + btnName(si, B_X) + " MINES  " + btnName(si, B_RT) + " SWEEP  " + btnName(si, B_RB) + " NEXT FLEET  HARBOR = FORM FLEET";
            text(h, 16 * u, G.H - (24 + hy * 20) * u, 10 * u, withA(S.col, 0.75f)); hy++;
        }
    } else if (ROSTER.w > 0) {
        clip(&ROSTER);
        G.cv.rect((float)ROSTER.x, (float)ROSTER.y, (float)ROSTER.w, (float)ROSTER.h, Col(3, 10, 20));
        float y = ROSTER.y + 14 * u;
        for (int si = 0; si < 4; si++) if (G.side[si].used) { drawSideBox(si, ROSTER.x + 16 * u, y, ROSTER.w - 30 * u, u); y += 62 * u; }
        text("MAP AT HALF SPEED DURING BATTLES", ROSTER.x + 16 * u, y + 6 * u, 10 * u, C_DIM);
        clip(nullptr);
    }
}

// ---------------------------------------------------------------- battle
static void hullPts(int t, std::vector<V2>& o) {
    float L = SD[t].L, W = SD[t].Wd; o.clear();
    switch (t) {
    case T_CARRIER: o = { V2(L * .5f, 0), V2(L * .42f, W * .5f), V2(-L * .5f, W * .5f), V2(-L * .5f, -W * .5f), V2(L * .42f, -W * .5f) }; break;
    case T_SUB: o = { V2(L * .5f, 0), V2(L * .3f, W * .5f), V2(-L * .35f, W * .45f), V2(-L * .5f, 0), V2(-L * .35f, -W * .45f), V2(L * .3f, -W * .5f) }; break;
    case T_PT: o = { V2(L * .5f, 0), V2(L * .1f, W * .5f), V2(-L * .5f, W * .45f), V2(-L * .5f, -W * .45f), V2(L * .1f, -W * .5f) }; break;
    default: o = { V2(L * .5f, 0), V2(L * .25f, W * .5f), V2(-L * .42f, W * .46f), V2(-L * .5f, W * .3f), V2(-L * .5f, -W * .3f), V2(-L * .42f, -W * .46f), V2(L * .25f, -W * .5f) };
    }
}
static void drawShip(const BShip& s, const View& v, float u, bool active) {
    const SDef& d = SD[s.t]; Col c = G.side[s.owner].col;
    float sink = s.sinkT < 0 ? 0 : clampf(s.sinkT / 2.2f, 0, 1); if (sink >= 1) return;
    Col cc = s.flash > 0 ? mixc(c, C_WHITE, s.flash) : c; cc = withA(cc, 1 - sink);
    V2 f = fromAng(s.ang), sd = perp(f); float k = v.s * (1 - sink * 0.3f);
    auto P = [&](V2 l) { return v.w2s(s.p) + (f * l.x + sd * l.y) * k; };
    // wake
    for (size_t i = 1; i < s.wake.size(); i++) G.cv.line(v.w2s(s.wake[i - 1]), v.w2s(s.wake[i]), (1 + i * 0.15f) * u, Col(200, 230, 255, (int)(10 + 70.f * i / s.wake.size())), 0.2f);
    std::vector<V2> h; hullPts(s.t, h); std::vector<V2> sp; for (auto& q : h) sp.push_back(P(q));
    G.cv.fillPoly(sp.data(), (int)sp.size(), withA(Col(cc.r / 4, cc.g / 4, cc.b / 4), (1 - sink) * 0.9f));
    G.cv.poly(sp.data(), (int)sp.size(), 1.8f * u, cc, 0.9f);
    float L = d.L, W = d.Wd;
    auto turret = [&](float x, float dir) { V2 c0 = P(V2(x, 0)); G.cv.circle(c0, W * 0.28f * k, 1.2f * u, cc, 0.4f, 10); G.cv.line(c0, P(V2(x + dir * W * 0.7f, 0)), 1.6f * u, cc, 0.4f); };
    switch (s.t) {
    case T_CARRIER: G.cv.dash(P(V2(L * .4f, 0)), P(V2(-L * .45f, 0)), 5 * k, 4 * k, 1 * u, withA(cc, 0.6f)); { V2 q[4] = { P(V2(L * .1f, -W * .5f)), P(V2(-L * .15f, -W * .5f)), P(V2(-L * .15f, -W * .3f)), P(V2(L * .1f, -W * .3f)) }; G.cv.poly(q, 4, 1.3f * u, cc, 0.4f); } break;
    case T_TRANSPORT: for (int i = 0; i < 3; i++) { float x = L * (0.25f - i * 0.17f); V2 q[4] = { P(V2(x, -W * .25f)), P(V2(x - L * .1f, -W * .25f)), P(V2(x - L * .1f, W * .25f)), P(V2(x, W * .25f)) }; G.cv.poly(q, 4, 1 * u, withA(cc, 0.7f), 0.3f); } { V2 q[4] = { P(V2(-L * .28f, -W * .35f)), P(V2(-L * .45f, -W * .35f)), P(V2(-L * .45f, W * .35f)), P(V2(-L * .28f, W * .35f)) }; G.cv.poly(q, 4, 1.3f * u, cc, 0.4f); } break;
    case T_BATTLESHIP: turret(L * .3f, 1); turret(L * .15f, 1); turret(-L * .3f, -1); { V2 q[4] = { P(V2(L * .05f, -W * .3f)), P(V2(-L * .15f, -W * .3f)), P(V2(-L * .15f, W * .3f)), P(V2(L * .05f, W * .3f)) }; G.cv.poly(q, 4, 1.2f * u, cc, 0.4f); } break;
    case T_DESTROYER: turret(L * .28f, 1); turret(-L * .32f, -1); G.cv.circle(P(V2(0, 0)), W * .22f * k, 1 * u, cc, 0.3f, 8); G.cv.circle(P(V2(-L * .12f, 0)), W * .22f * k, 1 * u, cc, 0.3f, 8); break;
    case T_SUB: G.cv.circle(P(V2(L * .08f, 0)), W * .35f * k, 1.2f * u, cc, 0.4f, 10); break;
    case T_PT: G.cv.line(P(V2(0, -W * .25f)), P(V2(-L * .3f, -W * .25f)), 1 * u, cc, 0.3f); G.cv.line(P(V2(0, W * .25f)), P(V2(-L * .3f, W * .25f)), 1 * u, cc, 0.3f); break;
    case T_LAYER: turret(L * .3f, 1); for (int i = 0; i < 4; i++) G.cv.disc(P(V2(-L * (0.2f + i * 0.07f), -W * .25f)), 2 * k, cc, 6), G.cv.disc(P(V2(-L * (0.2f + i * 0.07f), W * .25f)), 2 * k, cc, 6); break;
    case T_SWEEPER: turret(L * .3f, 1); G.cv.line(P(V2(-L * .5f, 0)), P(V2(-L * .9f, -W * 1.4f)), 1 * u, withA(cc, 0.5f), 0.2f); G.cv.line(P(V2(-L * .5f, 0)), P(V2(-L * .9f, W * 1.4f)), 1 * u, withA(cc, 0.5f), 0.2f); break;
    }
    if (sink > 0) return;
    // label + armor bar
    float hp = G.side[s.owner].ship[s.ship].hp / d.armor; V2 top = v.w2s(s.p) + V2(0, -(L * 0.5f * k + 16 * u));
    float bw = 34 * u; G.cv.rect(top.x - bw / 2, top.y, bw, 4 * u, Col(30, 30, 40, 220)); G.cv.rect(top.x - bw / 2, top.y, bw * hp, 4 * u, hp > 0.5f ? C_GOOD : hp > 0.25f ? C_WARN : C_BAD);
    text(d.shortName, top.x, top.y - 15 * u, 10 * u, withA(c, active ? 1.f : 0.7f), 1);
    if (active) {
        float r = L * 0.62f * k + 8 * u;
        for (int q = 0; q < 4; q++) G.cv.arc(v.w2s(s.p), r, q * PI / 2 + 0.25f + G.time, q * PI / 2 + 1.1f + G.time, 2 * u, c, 0.8f);
    }
}

static void drawBattle(int bi, float u) {
    Battle& b = G.bt[bi]; SDL_Rect vp = b.vp; clip(&vp);
    View v = fitView(vp, BW, BH);
    if (b.shake > 0.5f) v.o += V2(R.range(-1, 1), R.range(-1, 1)) * b.shake * u * 0.5f;
    G.cv.rect((float)vp.x, (float)vp.y, (float)vp.w, (float)vp.h, Col(2, 6, 14));
    G.cv.rect(v.o.x, v.o.y, BW * v.s, BH * v.s, Col(6, 28, 56));
    // waves
    for (float y = 40; y < BH; y += 70) for (float x = 30 + std::fmod(y * 0.7f, 90.f); x < BW; x += 110) {
        float ph = G.time * 0.8f + x * 0.013f + y * 0.021f; V2 c = v.w2s(V2(x + std::sin(ph) * 8, y));
        G.cv.arc(c, 9 * v.s, PI * 1.15f, PI * 1.85f, 1 * u, Col(70, 140, 200, (int)(40 + 30 * std::sin(ph * 1.3f))), 0);
    }
    for (size_t i = 0; i < b.land.size(); i++) drawLand(b.land[i], b.landC[i], v, u * 1.2f, true);
    G.cv.rectLine(v.o.x, v.o.y, BW * v.s, BH * v.s, 2 * u, Col(80, 160, 220, 160), 0.4f);
    for (auto& s : b.ships) drawShip(s, v, u, controlled(b, (int)(&s - &b.ships[0])) && !G.side[s.owner].cpu);
    for (auto& sh : b.shots) {
        if (sh.torp) { V2 a = v.w2s(sh.p), t0 = v.w2s(sh.p - sh.dir * 40); G.cv.line(t0, a, 1.5f * u, Col(220, 240, 255, 140), 0.4f); G.cv.line(a - (a - t0) * 0.2f, a, 3 * u, C_WHITE, 0.6f); }
        else {
            float k = clampf(sh.t / sh.dur, 0, 1), hgt = std::sin(k * PI) * dist(sh.from, sh.to) * 0.18f;
            V2 g = v.w2s(sh.p); G.cv.disc(g, 2 * u, Col(0, 0, 0, 120), 6);
            V2 air = g - V2(0, hgt * v.s); G.cv.glowDisc(air, 7 * u, Col(255, 220, 150, 220)); G.cv.disc(air, 2.4f * u, C_WHITE, 8);
        }
    }
    drawParts(bi, v, u);
    // crosshairs and range rings
    for (int sd = 0; sd < 2; sd++) {
        int k = b.active[sd]; if (k < 0) continue; BShip& s = b.ships[k]; Side& S = G.side[s.owner]; if (S.cpu) continue;
        if (b.aiming[sd]) {
            V2 c = v.w2s(b.cross[sd]); bool ready = false;
            for (int j = 0; j < (int)b.ships.size(); j++) {
                BShip& q = b.ships[j]; if (q.side != sd || !bAlive(q) || !controlled(b, j)) continue;
                V2 sp = v.w2s(q.p); bool inR = dist(q.p, b.cross[sd]) <= SD[q.t].range; if (inR && q.reload <= 0) ready = true;
                G.cv.circle(sp, SD[q.t].range * v.s, 1 * u, withA(S.col, 0.25f), 0.2f, 64);
                G.cv.dash(sp, c, 6 * u, 6 * u, 1 * u, withA(inR ? S.col : C_DIM, inR ? 0.6f : 0.25f), G.time * 30);
            }
            float r = 12 * u; G.cv.circle(c, r, 1.8f * u, ready ? S.col : C_BAD, 0.9f, 16);
            for (int q = 0; q < 4; q++) { V2 d = fromAng(q * PI / 2); G.cv.line(c + d * r * 0.4f, c + d * r * 1.6f, 1.8f * u, S.col, 0.7f); }
        }
    }
    // header and per-side status
    float hdr = 30 * u;
    G.cv.rect((float)vp.x, (float)vp.y, (float)vp.w, hdr, Col(2, 8, 18, 200));
    int o0 = G.fl[b.fl[0]].owner, o1 = G.fl[b.fl[1]].owner;
    text("P" + std::to_string(o0 + 1), vp.x + 14 * u, vp.y + 8 * u, 14 * u, G.side[o0].col);
    text("BATTLE", vp.x + vp.w / 2.f, vp.y + 8 * u, 14 * u, C_WHITE, 1);
    text("P" + std::to_string(o1 + 1), vp.x + vp.w - 14 * u, vp.y + 8 * u, 14 * u, G.side[o1].col, 2);
    for (int sd = 0; sd < 2; sd++) {
        int owner = sd ? o1 : o0; Side& S = G.side[owner]; int k = b.active[sd];
        float x = sd ? vp.x + vp.w - 14 * u : vp.x + 14 * u; int al = sd ? 2 : 0; float y = vp.y + vp.h - 54 * u;
        if (k >= 0 && !S.cpu) {
            if (b.fleetMode[sd]) {
                int n = 0, rdy = 0, tel = 0; for (int j = 0; j < (int)b.ships.size(); j++) { BShip& q = b.ships[j]; if (q.side == sd && bAlive(q)) { n++; if (q.reload <= 0) rdy++; tel = q.telegraph; } }
                text("WHOLE FLEET (" + std::to_string(n) + (n == 1 ? " SHIP)" : " SHIPS)"), x, y, 12 * u, S.col, al);
                text(std::to_string(rdy) + "/" + std::to_string(n) + " READY   ENGINE " + std::to_string(tel) + "/3", x, y + 18 * u, 11 * u, rdy ? C_GOOD : C_DIM, al);
            } else {
                BShip& s = b.ships[k]; const SDef& d = SD[s.t];
                text(std::string(d.name) + "  " + (d.torp ? "TORPEDO" : "GUNS") + "  (OTHERS HOLD)", x, y, 12 * u, S.col, al);
                text(std::string(s.reload > 0 ? "RELOADING" : "READY") + "   ENGINE " + std::to_string(s.telegraph) + "/3", x, y + 18 * u, 11 * u, s.reload > 0 ? C_DIM : C_GOOD, al);
            }
            text("HOLD " + std::string(btnName(owner, B_A)) + " AIM+FIRE  " + btnName(owner, B_B) + " FLEET/NEXT SHIP  " + btnName(owner, B_Y) + " RETREAT", x, y + 36 * u, 9 * u, withA(S.col, 0.65f), al);
        } else if (S.cpu) text("CPU " + std::string(DIFFN[S.diff]), x, y + 18 * u, 11 * u, withA(S.col, 0.7f), al);
        if (b.retreat[sd] >= 0) text("RETREATING " + std::to_string((int)std::ceil(b.retreat[sd])), x, vp.y + 40 * u, 13 * u, std::fmod(G.time * 3, 1.f) < 0.6f ? C_WARN : withA(C_WARN, 0.4f), al);
        if (S.toastT > 0 && !S.cpu) text(S.toast, x, vp.y + 60 * u, 11 * u, withA(S.col, std::min(1.f, S.toastT * 2)), al);
    }
    if (b.overT >= 0) {
        std::string m;
        if (b.fled >= 0) m = "P" + std::to_string(G.fl[b.fl[b.fled]].owner + 1) + " WITHDRAWS";
        else if (b.winner >= 0) m = "P" + std::to_string(G.fl[b.fl[1 - b.winner]].owner + 1) + " FLEET SUNK";
        else m = "BOTH FLEETS SUNK";
        text(m, vp.x + vp.w / 2.f, vp.y + vp.h / 2.f - 20 * u, 30 * u, C_WHITE, 1);
    }
    clip(nullptr);
}

static void drawGame() {
    float u = G.cv.px; layoutPanels();
    if (MAPSHOWN) drawMap(G.mapVp, u);
    for (int i = 0; i < 4; i++) if (G.bt[i].live) drawBattle(i, u);
    for (int i = 0; i < 4; i++) {   // panel separators
        if (!G.bt[i].live) continue; SDL_Rect r = G.bt[i].vp; G.cv.rectLine((float)r.x, (float)r.y, (float)r.w, (float)r.h, 2 * u, Col(60, 120, 170), 0.3f);
    }
    drawHud(u);
    for (int si = 0; si < 4; si++) if (G.side[si].used && !G.side[si].cpu) drawHarborMenu(si, u);
    if (G.banT > 0) { float a = std::min(1.f, G.banT); text(G.ban, G.W / 2.f, G.H * 0.42f, 40 * u, withA(G.banCol, a), 1); }
}
