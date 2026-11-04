static bool XBOXLOOK = getenv("IH_XBOXLOOK") != nullptr;   // screenshots: show keyboard players as an Xbox pad
// Iron Harbor - drawing: the tactical map, battle panels, HUD (included from main.cpp)

static const char* btnName(int s, int b);   // ui.inc: button label for this side's controller
struct View { float s = 1; V2 o; V2 w2s(V2 p) const { return o + p * s; } V2 s2w(V2 q) const { return (q - o) / s; } };
static View VMAP; static bool MAPSHOWN = true; static SDL_Rect ROSTER = { 0, 0, 0, 0 };
static View fitView(SDL_Rect r, float w, float h) { View v; v.s = std::min(r.w / w, r.h / h); v.o = V2(r.x + (r.w - w * v.s) / 2, r.y + (r.h - h * v.s) / 2); return v; }

static void clip(const SDL_Rect* r) { G.cv.flush(G.ren); SDL_RenderSetClipRect(G.ren, r); }
static void panel(float x, float y, float w, float h, Col edge, float u, int alpha = 225) { G.cv.rect(x, y, w, h, Col(4, 12, 24, alpha)); G.cv.rectLine(x, y, w, h, 1.3f * u, edge, 0.5f); }
static void text(const std::string& s, float x, float y, float size, Col c, int align = 0) { FONT.draw(G.cv, s.c_str(), V2(x, y), size, c, align); }

// ---------------------------------------------------------------- button prompts
// A bar of "[button] ACTION" pairs: each button is a filled chip in its pad colour (A green, B red, X blue, Y yellow,
// triggers/bumpers grey; Wii and keyboard keys light grey) so the controls read at a glance from the couch.
struct Prompt { std::string key; int b; std::string label; };
static Col chipCol(int si, int b) {
    Dev* d = sideDev(si);
    if (d && (d->gc || (XBOXLOOK && d->id == -1)) && b >= 0) switch (b) {
        case B_A: return Col(70, 190, 70); case B_B: return Col(220, 70, 60); case B_X: return Col(60, 120, 230); case B_Y: return Col(235, 190, 40);
    }
    return Col(200, 208, 220);
}
static float chipW(const std::string& k, float sz) { return k.size() <= 1 ? sz * 2.2f : k.size() * sz + sz * 0.8f; }
static float promptsWidth(const std::vector<Prompt>& ps, float sz) {
    float w = 0;
    for (auto& p : ps) { float kw = chipW(p.key, sz); w += kw + sz * 0.7f + FONT.width(p.label.c_str(), sz) + sz * 1.6f; }
    return w - sz * 1.6f;
}
// draws the bar with its left edge at x (align 0), centre (1) or right edge (2); y = top of the chips. Returns its width.
static float drawPrompts(int si, const std::string& tag, const std::vector<Prompt>& ps, float x, float y, float sz, int align, Col tagCol) {
    float pad = sz * 0.55f, tw = tag.empty() ? 0 : FONT.width(tag.c_str(), sz) + sz * 1.2f;
    float w = tw + promptsWidth(ps, sz), h = sz * 2.0f;
    float x0 = x - (align == 1 ? w * 0.5f : align == 2 ? w : 0);
    G.cv.rect(x0 - pad * 1.6f, y - pad, w + pad * 3.2f, h + pad * 2, Col(2, 8, 18, 215));
    G.cv.rectLine(x0 - pad * 1.6f, y - pad, w + pad * 3.2f, h + pad * 2, std::max(1.f, sz * 0.09f), withA(tagCol, 0.55f), 0.4f);
    float cx = x0, ty = y + (h - sz) * 0.5f;
    if (!tag.empty()) { text(tag, cx, ty, sz, tagCol); cx += tw; }
    for (auto& p : ps) {
        float kw = chipW(p.key, sz); Col cc = chipCol(si, p.b);
        Col dark(cc.r * 0.28f, cc.g * 0.28f, cc.b * 0.28f); float lw = std::max(1.5f, sz * 0.14f);   // pad-style: bright letter on a dark button, coloured rim
        if (p.key.size() <= 1) { V2 c(cx + kw * 0.5f, y + h * 0.5f); G.cv.disc(c, h * 0.55f, dark); G.cv.circle(c, h * 0.55f, lw, cc, 0.8f); }
        else { G.cv.rect(cx, y, kw, h, dark); G.cv.rectLine(cx, y, kw, h, lw, cc, 0.8f); }
        FONT.draw(G.cv, p.key.c_str(), V2(cx + kw * 0.5f, ty), sz, cc, 1, std::max(1.5f, sz * 0.13f), 1.0f);
        cx += kw + sz * 0.7f;
        text(p.label, cx, ty, sz, C_WHITE); cx += FONT.width(p.label.c_str(), sz) + sz * 1.6f;
    }
    return w;
}

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

static bool ALARM = false;   // a contact with a local player is open: red edge pulse
static void drawMap(SDL_Rect vp, float u) {
    ALARM = false;
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
        bool human = !G.side[a.owner].cpu || !G.side[b.owner].cpu;
        if (human) {
            bool on = std::fmod(G.time * 2.6f, 1.f) < 0.6f;
            float top = std::min(v.w2s(a.p).y, v.w2s(b.p).y);
            text("ALL HANDS ON DECK", m.x, top - (small ? 26 : 70) * u, (small ? 10 : 18) * u, on ? C_BAD : withA(C_BAD, 0.35f), 1);
            if (!small) text("ENGAGE WINDOW " + std::to_string((int)std::ceil(c.t)), m.x, top - 44 * u, 11 * u, C_WARN, 1);
            float r = (28 + 10 * std::sin(G.time * 9)) * u; G.cv.circle(m, r, 1.5f * u, withA(C_BAD, 0.6f), 0.6f, 24);
            ALARM = true;
        } else if (!small) text("CONTACT " + std::to_string((int)std::ceil(c.t)), m.x, m.y - 30 * u, 12 * u, C_WARN, 1);
    }
    drawParts(-1, v, u);
    if (ALARM) {   // red pulse around the edge of the map
        float k = 0.5f + 0.5f * std::sin(G.time * 8); Col rc = Col(255, 40, 40, (int)(40 + 70 * k)); float w = (6 + 6 * k) * u;
        G.cv.rect((float)vp.x, (float)vp.y, (float)vp.w, w, rc); G.cv.rect((float)vp.x, (float)(vp.y + vp.h) - w, (float)vp.w, w, rc);
        G.cv.rect((float)vp.x, (float)vp.y, w, (float)vp.h, rc); G.cv.rect((float)(vp.x + vp.w) - w, (float)vp.y, w, (float)vp.h, rc);
    }
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
    float w = 390 * u, h = (NT + 6) * 26 * u + 40 * u;
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
    bool canDock = S.sel >= 0 && S.sel < (int)G.fl.size() && !G.fl[S.sel].dead && G.fl[S.sel].battle < 0;
    const char* items[3] = { "LAUNCH FLEET", "DOCK SELECTED FLEET", "CLOSE" };
    for (int k = 0; k < 3; k++) {
        bool sel = S.menuSel == NT + k, grey = (k == 0 && S.npick == 0) || (k == 1 && !canDock);
        if (sel) G.cv.rect(x + 6 * u, ly - 5 * u, w - 12 * u, 24 * u, withA(S.col, 0.22f));
        text(items[k], x + 40 * u, ly, 14 * u, grey ? Col(90, 90, 100) : sel ? S.col : C_WHITE);
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
            std::vector<Prompt> ps = { { btnName(si, B_A), B_A, "SELECT/MOVE" }, { btnName(si, B_Y), B_Y, "ENGAGE" }, { btnName(si, B_X), B_X, "MINES" },
                                       { btnName(si, B_RT), B_RT, "SWEEP" }, { btnName(si, B_RB), B_RB, "NEXT FLEET" } };
            float sz = 17 * u;
            float maxW = G.W - 60 * u, need = promptsWidth(ps, sz) + FONT.width("P1", sz) + sz * 1.2f;
            if (need > maxW) sz *= maxW / need;
            drawPrompts(si, "P" + std::to_string(si + 1), ps, G.W * 0.5f, G.H - (50 + hy * 54) * u, sz, 1, S.col); hy++;
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
static void drawShip(const BShip& s, const View& v, float u, bool active, const V2* aimAt = nullptr, float aimSpread = 1, const bool* deckEmpty = nullptr) {
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
    // gun turrets and torpedo tubes: guns swing toward the crosshair while this ship is aiming
    // the carrier has no turrets: planes parked on the deck stand in for its 'mounts' (gone while they're flying)
    if (s.t == T_CARRIER) {
        for (int i = 0; i < MOUNTS[s.t].n; i++) {
            if (deckEmpty && deckEmpty[i]) continue;
            const Mount& m = MOUNTS[s.t].m[i]; V2 c0 = P(V2(m.x * L, m.y * W)), f = fromAng(s.ang), sd = perp(f); float z = W * 0.32f * k;
            G.cv.line(c0 + f * z, c0 - f * z * 0.9f, 1.4f * u, cc, 0.4f); G.cv.line(c0 + f * z * 0.15f + sd * z, c0 + f * z * 0.15f - sd * z, 1.4f * u, cc, 0.4f);
        }
    }
    else for (int i = 0; i < MOUNTS[s.t].n; i++) {
        const Mount& m = MOUNTS[s.t].m[i]; V2 c0 = P(V2(m.x * L, m.y * W));
        if (d.torp) { G.cv.line(P(V2(m.x * L - L * 0.12f, m.y * W)), P(V2(m.x * L + L * 0.04f, m.y * W)), 2.2f * u, cc, 0.4f); continue; }
        float ang = s.ang + (m.rest < 0 ? PI : 0);
        if (aimAt) { V2 t = v.w2s(*aimAt + reticleOffset(s.t, i, norm(*aimAt - s.p), aimSpread)); ang = std::atan2(t.y - c0.y, t.x - c0.x); }
        G.cv.circle(c0, std::max(W * 0.28f, 3.f) * k, 1.2f * u, cc, 0.4f, 10);
        G.cv.line(c0, c0 + fromAng(ang) * (std::max(W * 0.75f, 7.f) * k), 1.8f * u, cc, 0.5f);
    }
    switch (s.t) {
    case T_CARRIER: G.cv.dash(P(V2(L * .4f, 0)), P(V2(-L * .45f, 0)), 5 * k, 4 * k, 1 * u, withA(cc, 0.6f)); { V2 q[4] = { P(V2(L * .1f, -W * .5f)), P(V2(-L * .15f, -W * .5f)), P(V2(-L * .15f, -W * .3f)), P(V2(L * .1f, -W * .3f)) }; G.cv.poly(q, 4, 1.3f * u, cc, 0.4f); } break;
    case T_TRANSPORT: for (int i = 0; i < 3; i++) { float x = L * (0.25f - i * 0.17f); V2 q[4] = { P(V2(x, -W * .25f)), P(V2(x - L * .1f, -W * .25f)), P(V2(x - L * .1f, W * .25f)), P(V2(x, W * .25f)) }; G.cv.poly(q, 4, 1 * u, withA(cc, 0.7f), 0.3f); } { V2 q[4] = { P(V2(-L * .28f, -W * .35f)), P(V2(-L * .45f, -W * .35f)), P(V2(-L * .45f, W * .35f)), P(V2(-L * .28f, W * .35f)) }; G.cv.poly(q, 4, 1.3f * u, cc, 0.4f); } break;
    case T_BATTLESHIP: { V2 q[4] = { P(V2(L * .05f, -W * .3f)), P(V2(-L * .15f, -W * .3f)), P(V2(-L * .15f, W * .3f)), P(V2(L * .05f, W * .3f)) }; G.cv.poly(q, 4, 1.2f * u, cc, 0.4f); } break;
    case T_DESTROYER: G.cv.circle(P(V2(0, 0)), W * .22f * k, 1 * u, cc, 0.3f, 8); G.cv.circle(P(V2(-L * .12f, 0)), W * .22f * k, 1 * u, cc, 0.3f, 8); break;
    case T_SUB: G.cv.circle(P(V2(L * .08f, 0)), W * .35f * k, 1.2f * u, cc, 0.4f, 10); break;
    case T_PT: G.cv.line(P(V2(0, -W * .25f)), P(V2(-L * .3f, -W * .25f)), 1 * u, cc, 0.3f); G.cv.line(P(V2(0, W * .25f)), P(V2(-L * .3f, W * .25f)), 1 * u, cc, 0.3f); break;
    case T_LAYER: for (int i = 0; i < 4; i++) G.cv.disc(P(V2(-L * (0.2f + i * 0.07f), -W * .25f)), 2 * k, cc, 6), G.cv.disc(P(V2(-L * (0.2f + i * 0.07f), W * .25f)), 2 * k, cc, 6); break;
    case T_SWEEPER: G.cv.line(P(V2(-L * .5f, 0)), P(V2(-L * .9f, -W * 1.4f)), 1 * u, withA(cc, 0.5f), 0.2f); G.cv.line(P(V2(-L * .5f, 0)), P(V2(-L * .9f, W * 1.4f)), 1 * u, withA(cc, 0.5f), 0.2f); break;
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

// ---------------------------------------------------------------- battle HUD: engine telegraph + live silhouette (Chris's pick, 4 Nov)
// icons are drawn in "icon units" (about 24 across) at scale k pixels per unit, centred on c, turned by ang
static V2 icp(V2 c, float k, float ang, float x, float y) { return c + rotv(V2(x, y), ang) * k; }
static void iconTurret(V2 c, float k, float ang, Col col, bool ready, float reloadFrac, float u) {
    V2 h[7] = { icp(c, k, ang, -9, -7), icp(c, k, ang, 4, -7), icp(c, k, ang, 9, -2), icp(c, k, ang, 9, 2), icp(c, k, ang, 4, 7), icp(c, k, ang, -9, 7), icp(c, k, ang, -12, 0) };
    if (ready) G.cv.fillPoly(h, 7, withA(col, 0.28f));
    G.cv.poly(h, 7, 1.3f * u, col, 0.6f);
    for (float y : { -3.f, 3.f }) { G.cv.line(icp(c, k, ang, 8, y), icp(c, k, ang, 19, y), 2.0f * u, col, 0.6f); G.cv.line(icp(c, k, ang, 19, y - 1.6f), icp(c, k, ang, 19, y + 1.6f), 1.3f * u, col, 0.4f); }
    G.cv.circle(icp(c, k, ang, -3, 0), 2.2f * k, 1 * u, col, 0.3f, 8);
    if (!ready) { G.cv.circle(c, 14 * k, 1.6f * u, withA(col, 0.18f), 0, 28); G.cv.arc(c, 14 * k, -PI / 2, -PI / 2 + 2 * PI * clampf(reloadFrac, 0.01f, 1), 1.8f * u, C_WARN, 0.7f); }
}
static void iconTorp(V2 c, float k, float ang, Col col, float loaded, float u) {   // loaded: 1 in the tube, 0..1 sliding in, <0 empty
    G.cv.line(icp(c, k, ang, -16, -5.5f), icp(c, k, ang, 14, -5.5f), 1.1f * u, withA(col, 0.55f), 0.3f);
    G.cv.line(icp(c, k, ang, -16, 5.5f), icp(c, k, ang, 14, 5.5f), 1.1f * u, withA(col, 0.55f), 0.3f);
    G.cv.line(icp(c, k, ang, -16, -5.5f), icp(c, k, ang, -16, 5.5f), 1.1f * u, withA(col, 0.55f), 0.3f);
    G.cv.line(icp(c, k, ang, 14, -7), icp(c, k, ang, 14, 7), 1.5f * u, col, 0.5f);
    if (loaded <= 0) return;
    float off = -24 * (1 - loaded); Col c2 = loaded >= 1 ? col : C_WARN;
    V2 f[5] = { icp(c, k, ang, -12 + off, -3), icp(c, k, ang, 7 + off, -3), icp(c, k, ang, 12 + off, 0), icp(c, k, ang, 7 + off, 3), icp(c, k, ang, -12 + off, 3) };
    G.cv.fillPoly(f, 5, withA(c2, 0.35f)); G.cv.poly(f, 5, 1.2f * u, c2, 0.6f);
    G.cv.line(icp(c, k, ang, -12 + off, -3), icp(c, k, ang, -15 + off, -5.5f), 1.1f * u, c2, 0.3f); G.cv.line(icp(c, k, ang, -12 + off, 3), icp(c, k, ang, -15 + off, 5.5f), 1.1f * u, c2, 0.3f);
    G.cv.line(icp(c, k, ang, -15 + off, -2), icp(c, k, ang, -15 + off, 2), 1.5f * u, c2, 0.3f);
}
enum { PL_DECK, PL_ATTACK, PL_RETURN, PL_DOWN };
static void iconPlane(V2 c, float k, float ang, Col col, int st, float u) {
    Col c2 = st == PL_ATTACK ? C_GOOD : st == PL_RETURN ? C_WARN : st == PL_DOWN ? C_BAD : col; if (st == PL_RETURN) ang += PI;
    V2 body[7] = { icp(c, k, ang, 13, 0), icp(c, k, ang, 9, -1.8f), icp(c, k, ang, -9, -1.4f), icp(c, k, ang, -12, -1), icp(c, k, ang, -12, 1), icp(c, k, ang, -9, 1.4f), icp(c, k, ang, 9, 1.8f) };
    V2 w1[4] = { icp(c, k, ang, 4, -1.6f), icp(c, k, ang, -3, -13), icp(c, k, ang, -6, -13), icp(c, k, ang, -3, -1.6f) }, w2[4] = { icp(c, k, ang, 4, 1.6f), icp(c, k, ang, -3, 13), icp(c, k, ang, -6, 13), icp(c, k, ang, -3, 1.6f) };
    V2 t1[4] = { icp(c, k, ang, -9, -1.2f), icp(c, k, ang, -13, -5.5f), icp(c, k, ang, -14.5f, -5.5f), icp(c, k, ang, -12, -1) }, t2[4] = { icp(c, k, ang, -9, 1.2f), icp(c, k, ang, -13, 5.5f), icp(c, k, ang, -14.5f, 5.5f), icp(c, k, ang, -12, 1) };
    float fa = st == PL_DECK ? 0.4f : 0.22f;
    for (auto* q : { w1, w2, t1, t2 }) { G.cv.fillPoly(q, 4, withA(c2, fa)); G.cv.poly(q, 4, 1.0f * u, c2, 0.5f); }
    G.cv.fillPoly(body, 7, withA(c2, fa)); G.cv.poly(body, 7, 1.0f * u, c2, 0.5f);
    if (st == PL_ATTACK) for (float y : { -8.f, 0.f, 8.f }) G.cv.line(icp(c, k, ang, -17, y), icp(c, k, ang, -24, y), 1 * u, withA(c2, 0.6f), 0.2f);
    if (st == PL_DOWN) { G.cv.line(c + V2(-9, -9) * k, c + V2(9, 9) * k, 2 * u, C_BAD, 0.6f); G.cv.line(c + V2(9, -9) * k, c + V2(-9, 9) * k, 2 * u, C_BAD, 0.6f); }
}
// keep the part of a convex polygon right of x = cut (armor fill drains from the stern)
static std::vector<V2> clipRightOf(const std::vector<V2>& p, float cut) {
    std::vector<V2> o; size_t n = p.size();
    for (size_t i = 0; i < n; i++) {
        V2 a = p[i], b = p[(i + 1) % n]; bool ia = a.x >= cut, ib = b.x >= cut;
        if (ia) o.push_back(a);
        if (ia != ib) { float t = (cut - a.x) / (b.x - a.x); o.push_back(a + (b - a) * t); }
    }
    return o;
}
static void ladderUI(float x, float y, float v, float mx, const char* label, float S, float u) {
    text(label, x, y + 0.5f * S, 5 * S, withA(C_WHITE, 0.55f));
    int n = (int)std::lround(clampf(v / mx, 0, 1) * 5);
    for (int i = 0; i < 5; i++) { float bx = x + 22 * S + i * 7 * S; if (i < n) G.cv.rect(bx, y - 1 * S, 5 * S, 8 * S, Col(92, 225, 255)); else G.cv.rectLine(bx, y - 1 * S, 5 * S, 8 * S, 1 * u, Col(92, 225, 255, 80), 0); }
}
static void drawShipStats(const Battle& b, int k, const Side& S0, float ax, float bottom, bool right, float u) {
    const BShip& s = b.ships[k]; const SDef& d = SD[s.t]; const Ship& sh = G.side[s.owner].ship[s.ship];
    const float S = 1.3f * u; Col pc = S0.col;
    float W = 340 * S, H = 122 * S, x = right ? ax - W : ax, y = bottom - H;
    G.cv.rect(x, y, W, H, Col(3, 14, 28, 235)); G.cv.rectLine(x, y, W, H, 1.5f * u, pc, 0.6f);
    bool aim = b.aiming[s.side]; int eng = aim ? 0 : (int)std::lround(clampf(s.thr, 0, 1) * 3);
    float hp = clampf(sh.hp / d.armor, 0, 1); Col ac = hp > 0.5f ? C_GOOD : hp > 0.25f ? C_WARN : C_BAD;
    // ---- engine-order telegraph (left)
    V2 c(x + 62 * S, y + 66 * S); float R = 46 * S;
    static const char* LBL[4] = { "STOP", "1/3", "2/3", "FULL" };
    for (int i = 0; i < 4; i++) {
        float a0 = PI + i * PI / 4, a1 = a0 + PI / 4; bool on = !aim && i == eng, am = aim && i == 0;
        if (on || am) { Col fc = withA(am ? C_WARN : pc, 0.2f); for (int q = 0; q < 6; q++) { float t0 = a0 + (a1 - a0) * q / 6, t1 = a0 + (a1 - a0) * (q + 1) / 6; G.cv.tri(G.cv.fillV, c, c + fromAng(t0) * R, c + fromAng(t1) * R, fc, fc, fc); } }
        G.cv.line(c, c + fromAng(a0) * R, 1 * u, withA(pc, 0.3f), 0);
        float m = a0 + PI / 8; text(LBL[i], c.x + std::cos(m) * (R - 15 * S), c.y + std::sin(m) * (R - 15 * S) - 2.4f * S, 4.8f * S, (on || am) ? (am ? C_WARN : pc) : withA(pc, 0.45f), 1);
    }
    G.cv.line(c, c + V2(R, 0), 1 * u, withA(pc, 0.3f), 0);
    G.cv.arc(c, R + 3 * S, PI, 2 * PI, 2 * u, pc, 0.8f);
    float lev = PI + PI / 4 * (eng + 0.5f); Col lc = aim ? C_WARN : pc;
    G.cv.line(c, c + fromAng(lev) * (R + 7 * S), 3 * u, lc, 0.9f); G.cv.disc(c + fromAng(lev) * (R + 7 * S), 3.6f * S, lc, 10);
    G.cv.disc(c, 13 * S, Col(4, 16, 31), 20); G.cv.circle(c, 13 * S, 1.2f * u, pc, 0.4f, 20);
    int kn = (int)std::lround(std::fabs(s.spd) / BKN);
    text(std::to_string(kn), c.x, c.y - 3.6f * S, 7.2f * S, C_WHITE, 1);
    text(aim ? "AIMING" : "KN  TOP " + std::to_string((int)d.knots), c.x, c.y + 17 * S, 4.6f * S, aim ? C_WARN : withA(pc, 0.6f), 1);
    // armor plates (5 armor each) and the number
    int np = (int)std::ceil(d.armor / 5); float pw = std::min(14.f, (104.f - (np - 1) * 2) / np) * S, gap = 2 * S, px = c.x - (np * pw + (np - 1) * gap) / 2, left = std::max(0.f, sh.hp) / 5;
    for (int i = 0; i < np; i++) { float f = clampf(left - i, 0, 1), bx = px + i * (pw + gap); G.cv.rectLine(bx, y + 96 * S, pw, 6 * S, 1 * u, withA(ac, 0.35f), 0); if (f > 0) G.cv.rect(bx, y + 96 * S, pw * f, 6 * S, ac); }
    text(std::to_string((int)std::ceil(std::max(0.f, sh.hp))) + "/" + std::to_string((int)d.armor), c.x, y + 106 * S, 5.4f * S, ac, 1);
    G.cv.line(V2(x + 122 * S, y + 12 * S), V2(x + 122 * S, y + H - 12 * S), 1 * u, withA(pc, 0.18f), 0);
    // ---- the live ship (right)
    float x1 = x + 132 * S, xr = x + W - 12 * S;
    text(d.name, x1, y + 12 * S, 6.6f * S, pc);
    int lk = s.t == T_CARRIER && aim ? shipUnder(b, s.side, b.cross[s.side], 14) : -1;
    if (lk >= 0) text("TARGET LOCKED", xr, y + 13 * S, 5 * S, C_WARN, 2);
    float L = std::min(170.f, 70 + d.L * 1.15f) * S, Wd = std::max(14.f, d.Wd * 1.15f) * S, x0 = xr - L - 4 * S, cy = y + 56 * S;
    float kf = clampf(std::fabs(s.spd) / (40 * BKN), 0, 1), wk = (6 + 46 * kf) * S;
    for (float dy : { -0.35f, 0.f, 0.35f }) G.cv.line(V2(x0 - 3 * S, cy + dy * Wd), V2(std::max(x1, x0 - 3 * S - wk * (dy != 0 ? 0.7f : 1.f)), cy + dy * Wd * 1.7f), 1.2f * u, withA(pc, 0.18f + 0.45f * kf), 0.2f);
    std::vector<V2> hl; hullPts(s.t, hl); std::vector<V2> hs;
    for (auto& q : hl) hs.push_back(V2(x0 + (q.x / d.L + 0.5f) * L, cy + (q.y / d.Wd) * Wd));
    std::vector<V2> fill = clipRightOf(hs, x0 + L * (1 - hp));
    if (fill.size() >= 3) G.cv.fillPoly(fill.data(), (int)fill.size(), withA(ac, 0.22f));
    G.cv.poly(hs.data(), (int)hs.size(), 1.5f * u, ac, 0.8f);
    if (s.t == T_CARRIER) G.cv.dash(V2(x0 + L * 0.1f, cy), V2(x0 + L * 0.88f, cy), 5 * S, 4 * S, 1 * u, withA(pc, 0.35f));
    // mounts at their real spots
    int n = MOUNTS[s.t].n; float rf = 1 - clampf(s.reload / d.reload, 0, 1); bool ready = s.reload <= 0;
    for (int i = 0; i < n; i++) {
        const Mount& m = MOUNTS[s.t].m[i]; V2 mc(x0 + (m.x + 0.5f) * L, cy + m.y * Wd * 1.6f);
        if (s.t == T_CARRIER) {
            int st = PL_DECK; bool out = false;
            for (auto& q : b.shots) if (q.plane && q.shooter == k && q.mount == i && !q.dead && q.wait <= 0) { out = true; st = q.st == 2 ? PL_RETURN : q.st == 3 ? PL_DOWN : PL_ATTACK; }
            if (!out && planesOut(b, k)) st = PL_DOWN;   // the other plane is still out; this one didn't come back
            iconPlane(mc, 0.62f * S, 0, pc, st, u);
        } else if (d.torp) iconTorp(mc, 0.7f * S, 0, pc, ready ? 1.f : rf < 0.15f ? -1.f : rf, u);
        else iconTurret(mc, 0.7f * S, m.rest < 0 ? PI : 0, pc, ready, rf, u);
    }
    // ---- bottom: weapon, handling, spread
    std::string wname = s.t == T_CARRIER ? std::to_string(n) + " PLANES" : d.torp ? std::to_string(n) + (n > 1 ? " TUBES" : " TUBE") : std::to_string(n) + (n > 1 ? " GUNS" : " GUN");
    char wb[80]; snprintf(wb, sizeof wb, "%s  %d DMG  R %d", wname.c_str(), (int)d.dmg, (int)d.range);
    text(wb, x1, y + H - 33 * S, 5.6f * S, pc);
    ladderUI(x1, y + H - 18 * S, d.accel, 1.2f, "ACC", S, u); ladderUI(x1 + 66 * S, y + H - 18 * S, d.turn, 1.6f, "TRN", S, u);
    if (n > 1) {
        int ks = (int)std::lround(clampf((b.spread[s.side] - 0.3f) / 2.7f, 0, 1) * 4) + 1;
        for (int i = 0; i < 5; i++) { V2 q(xr - 34 * S + i * 8 * S, y + H - 14 * S); if (i < ks) G.cv.disc(q, 2.4f * S, pc, 8); else G.cv.circle(q, 2.4f * S, 1 * u, withA(pc, 0.35f), 0, 8); }
        text("SPREAD", xr - 40 * S, y + H - 16.3f * S, 4.6f * S, withA(pc, 0.55f), 2);
    }
    int alive = 0; for (auto& q : b.ships) if (q.side == s.side && bAlive(q)) alive++;
    if (alive > 1) text(std::to_string(alive - 1) + (alive > 2 ? " OTHER SHIPS STOPPED" : " OTHER SHIP STOPPED"), right ? x + W : x, y - 15 * u, 9 * u, C_DIM, right ? 2 : 0);
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
    for (auto& s : b.ships) { int k = (int)(&s - &b.ships[0]); bool c = controlled(b, k) && !G.side[s.owner].cpu;
        bool gone[3] = { false, false, false }; for (auto& sh : b.shots) if (sh.plane && sh.shooter == k && sh.wait <= 0) gone[sh.mount] = true;
        drawShip(s, v, u, c, c && b.aiming[s.side] ? &b.cross[s.side] : nullptr, b.spread[s.side], gone); }
    auto planeShape = [&](V2 c, float a, float sz, Col col, float w, float glow) {
        V2 f = fromAng(a), sd = perp(f);
        G.cv.line(c + f * sz, c - f * sz * 0.9f, w, col, glow);                                  // fuselage
        G.cv.line(c + f * sz * 0.15f + sd * sz, c + f * sz * 0.15f - sd * sz, w, col, glow);    // wings
        G.cv.line(c - f * sz * 0.8f + sd * sz * 0.4f, c - f * sz * 0.8f - sd * sz * 0.4f, w, col, glow);   // tail
    };
    for (auto& sh : b.shots) if (sh.plane && sh.wait <= 0) {   // shadows first, under everything in the air
        V2 g = v.w2s(sh.p) + V2(1, 0.8f) * (sh.alt * 22 * v.s); planeShape(g, sh.ang, 9 * v.s, Col(0, 0, 0, 110), 2.4f * u, 0);
    }
    for (auto& sh : b.shots) {
        if (sh.wait > 0) continue;
        if (sh.plane) {
            Col pc = G.side[b.ships[sh.shooter].owner].col; V2 c = v.w2s(sh.p); float sz = (8 + 5 * sh.alt) * v.s;
            planeShape(c, sh.ang, sz, mixc(pc, C_WHITE, 0.12f), 2.0f * u, 0.8f);
            if (sh.st == 3) G.cv.glowDisc(c, 6 * u, Col(255, 150, 60, 160));
            if (sh.st == 1 && sh.alt > 0.5f) G.cv.line(c - fromAng(sh.ang) * sz, c - fromAng(sh.ang) * sz * 2.6f, 1 * u, withA(C_WHITE, 0.25f), 0.2f);
            continue;
        }
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
            V2 c = v.w2s(b.cross[sd]); bool inR = dist(s.p, b.cross[sd]) <= SD[s.t].range, ready = inR && s.reload <= 0;
            Col rc = ready ? S.col : C_BAD; V2 dir = norm(b.cross[sd] - s.p);
            G.cv.circle(v.w2s(s.p), SD[s.t].range * v.s, 1 * u, withA(S.col, 0.25f), 0.2f, 64);
            for (int i = 0; i < MOUNTS[s.t].n; i++) {
                V2 r = v.w2s(b.cross[sd] + reticleOffset(s.t, i, dir, b.spread[sd])), from = v.w2s(mountPos(s, i));
                G.cv.dash(from, r, 5 * u, 6 * u, 1 * u, withA(rc, 0.45f), G.time * 30);
                float rr = (SD[s.t].torp ? 7 : 8) * u;
                G.cv.circle(r, rr, 1.6f * u, rc, 0.8f, 14);
                for (int q = 0; q < 4; q++) { V2 dd = fromAng(q * PI / 2 + PI / 4); G.cv.line(r + dd * rr * 0.45f, r + dd * rr * 1.35f, 1.4f * u, rc, 0.6f); }
            }
            if (MOUNTS[s.t].n > 1) G.cv.circle(c, (14 + 16 * b.spread[sd]) * v.s + 10 * u, 1 * u, withA(rc, 0.35f), 0.3f, 28);   // the group
            std::string lbl = std::to_string(MOUNTS[s.t].n) + (s.t == T_CARRIER ? " PLANES" : SD[s.t].torp ? (MOUNTS[s.t].n > 1 ? " TUBES" : " TUBE") : (MOUNTS[s.t].n > 1 ? " GUNS" : " GUN"));
            if (s.t == T_CARRIER) {
                int lk = shipUnder(b, sd, b.cross[sd], 14);
                if (lk >= 0) {   // locked: brackets around the target ship
                    V2 tp = v.w2s(b.ships[lk].p); float rr = SD[b.ships[lk].t].L * 0.6f * v.s + 8 * u;
                    for (int q = 0; q < 4; q++) G.cv.arc(tp, rr, q * PI / 2 - 0.35f, q * PI / 2 + 0.35f, 2.2f * u, C_WARN, 0.9f);
                    lbl += " - TARGET LOCKED";
                } else if (planesOut(b, k)) lbl += " - PLANES OUT";
            }
            text(lbl, c.x + 36 * u, c.y - 6 * u, 10 * u, withA(rc, 0.8f));
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
            drawShipStats(b, k, S, sd ? vp.x + vp.w - 14 * u : vp.x + 14 * u, vp.y + vp.h - 62 * u, sd == 1, u);
            { Dev* dd = sideDev(owner); std::string sp = dd && (dd->gc || XBOXLOOK) ? (dd->nintendo ? "ZR/ZL" : "RT/LT") : dd && dd->wii ? "+/-" : "T/F";
              std::vector<Prompt> ps = { { btnName(owner, B_A), B_A, "HOLD: AIM+FIRE" }, { sp, B_RT, "SPREAD" }, { btnName(owner, B_B), B_B, "NEXT SHIP" }, { btnName(owner, B_Y), B_Y, "RETREAT" } };
              float sz = 12 * u, maxW = vp.w * 0.48f - 30 * u, need = promptsWidth(ps, sz); if (need > maxW) sz *= maxW / need;
              drawPrompts(owner, "", ps, x + (sd ? -1 : 1) * sz * 0.9f, vp.y + vp.h - 40 * u, sz, al, S.col); }
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
