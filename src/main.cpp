// Iron Harbor - naval strategy for 1-4 admirals (local players and CPU) on one screen.
// Form fleets, lay hidden minefields, fight close battles at sea, and sail a carrier or troop transport into an enemy harbor.
// C++17 / SDL2, vector art drawn in code, synthesized sound. (c) Rosenau Productions.
#include "gfx.h"
#include "audio.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <cstring>
#include <ctime>
#include <queue>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define IH_WII 1
#include "wii.h"
#endif

static Font FONT;
static Audio AU;
static Rng R;

// ---------------------------------------------------------------- ships
enum { T_CARRIER, T_TRANSPORT, T_BATTLESHIP, T_SUB, T_DESTROYER, T_PT, T_LAYER, T_SWEEPER, NT };
struct SDef { const char* name; const char* shortName; char letter; int count; float knots, accel, turn, armor, dmg, range, reload; bool torp; float L, Wd; };
// range in battle units (the battle sea is 1600 x 900); accel/turn are handling multipliers
static const SDef SD[NT] = {
    { "AIRCRAFT CARRIER", "CARRIER", 'C', 1, 30, 0.35f, 0.55f, 25, 10, 1000, 2.6f, false, 96, 24 },
    { "TROOP TRANSPORT", "TRANSPORT", 'T', 1, 30, 0.25f, 0.50f, 25, 7, 260, 2.2f, false, 76, 20 },
    { "BATTLESHIP", "BATTLESHIP", 'B', 2, 35, 0.50f, 0.70f, 30, 12, 620, 3.0f, false, 88, 18 },
    { "SUBMARINE", "SUBMARINE", 'S', 1, 35, 0.80f, 1.00f, 15, 20, 780, 4.5f, true, 54, 10 },
    { "DESTROYER", "DESTROYER", 'D', 3, 40, 0.80f, 1.10f, 15, 9, 480, 2.0f, false, 62, 12 },
    { "PT BOAT", "PT BOAT", 'P', 2, 40, 1.20f, 1.60f, 5, 12, 360, 3.5f, true, 32, 9 },
    { "MINE LAYER", "MINE LAYER", 'M', 1, 20, 0.50f, 0.80f, 15, 6, 260, 2.2f, false, 56, 14 },
    { "MINE SWEEPER", "SWEEPER", 'W', 2, 30, 0.50f, 0.90f, 20, 6, 780, 2.4f, false, 48, 12 },
};
// gun turrets / torpedo tubes per ship: position on the hull (x = fraction of length, forward +; y = fraction of beam) and
// the way the gun rests (+1 forward, -1 aft). One salvo = one shot per mount; each shot does dmg / mounts.
struct Mount { float x, y, rest; };
struct MountSet { int n; Mount m[3]; };
static const MountSet MOUNTS[8] = {
    { 2, { { -0.40f, -0.18f, 1 }, { -0.40f, 0.18f, 1 } } },            // carrier: two planes, starting at the stern of the deck
    { 1, { { 0.34f, 0, 1 } } },                                          // transport
    { 3, { { 0.30f, 0, 1 }, { 0.15f, 0, 1 }, { -0.30f, 0, -1 } } },     // battleship: three turrets
    { 2, { { 0.46f, -0.18f, 1 }, { 0.46f, 0.18f, 1 } } },               // submarine: two bow tubes
    { 2, { { 0.28f, 0, 1 }, { -0.32f, 0, -1 } } },                      // destroyer: fore and aft guns
    { 2, { { 0.05f, -0.38f, 1 }, { 0.05f, 0.38f, 1 } } },               // PT boat: two side tubes
    { 1, { { 0.30f, 0, 1 } } },                                          // mine layer
    { 1, { { 0.30f, 0, 1 } } },                                          // mine sweeper
};
static const int NSHIPS = 13, MAXFLEETS = 4, MAXMINES = 4, DOCKS = 3;
static const float MINE_DMG = 20, MINE_R = 62, CONTACT_R = 105, CONTACT_T = 6, HARBOR_R = 62, SWEEP_KN = 15;
static const float MW = 3200, MH = 1800;           // map world
static const float BW = 1600, BH = 900;            // battle sea (16:9)
static const float BK = 3.0f;                      // battle units per map unit
static const float BKN = 3.2f;                     // battle units per second per knot
static const float GUN_SPEED = 640, TORP_SPEED = 230, LAND_DMG = 12;

static const Col PCOL[4] = { Col(70, 200, 255), Col(255, 150, 50), Col(120, 255, 110), Col(255, 90, 200) };
static const Col C_WHITE(255, 255, 255), C_DIM(120, 140, 165), C_GOOD(110, 255, 140), C_BAD(255, 80, 80), C_LAND(90, 230, 150), C_SEA(40, 120, 190), C_WARN(255, 220, 90);

// ---------------------------------------------------------------- world
struct Island { V2 c; std::vector<V2> pts; float r; float minx, miny, maxx, maxy; };
struct Ship { int t; float hp; int st; };          // st: 0 in harbor, 1 at sea, 2 sunk
struct Side {
    bool used = false, alive = true, cpu = false; int team = 0, diff = 1, dev = -9, idx = 0; Col col;
    V2 harbor, hdir; Ship ship[NSHIPS]; int docks = DOCKS; float capturedAt = -1; std::string outMsg;
    // controls
    V2 cur; int sel = -1; bool menu = false; int menuSel = 0, pick[3] = { -1, -1, -1 }, npick = 0; float toastT = 0; std::string toast; bool devLost = false;
    // cpu
    float thinkT = 0, deployT = 2; int plan = 0; std::vector<V2> mineSpots;
};
struct Fleet {
    int owner = 0; int s[3] = { -1, -1, -1 }; int n = 0; V2 p; float ang = 0; std::vector<V2> path; bool moving = false, sweep = false, dock = false, dead = false;
    int battle = -1; float mineCd = 0; int lastMine = -1; float flashT = 0; int role = 0; float aiT = 0; float launched = 0; std::vector<V2> trail; float trailT = 0;
};
struct Mine { int owner; V2 p; float r; bool dead = false, revealed = false; float laidT = 0; };
struct Contact { int a, b; float t; bool passed = false; float cpuT[2] = { 0, 0 }; };
enum { P_SPARK, P_DOT, P_RING, P_DEBRIS, P_SMOKE };
struct Part { V2 p, v; float life, max, a, b; Col c; int kind; int layer; };   // layer -1 map, else battle index

// battle
struct BShip { int side, owner, fleet, ship, t; V2 p, v; float ang, spd, thr = 0, reload = 0, landT = 0, flash = 0, sinkT = -1, aimT = 0, smokeT = 0; bool aiming = false; V2 aimAt; std::vector<V2> wake; float wakeT = 0; int telegraph = 2; int target = -1; float retarget = 0, wobble = 0; };
struct Shot { int side; V2 from, to, p, dir; float t = 0, dur = 1, dmg, range, gone = 0, wait = 0; bool torp, dead = false; int shooter, mount = 0;
              bool plane = false; int tgt = -1, st = 0; float ang = 0, alt = 0, spd = 0; };   // plane: carrier aircraft (st 0 deck run, 1 attack, 2 return)   // wait: salvo stagger before it leaves the mount
struct Battle {
    bool live = false; int fl[2]; V2 center; std::vector<BShip> ships; std::vector<Shot> shots; std::vector<std::vector<V2>> land; std::vector<V2> landC;
    int active[2] = { -1, -1 }; float spread[2] = { 1, 1 }; V2 cross[2];   /* spread: how wide the salvo's reticles are grouped. each admiral controls ships[active[side]] only; the rest sit */ bool aiming[2] = { false, false }; float retreat[2] = { -1, -1 }; float t = 0, overT = -1; int winner = -1; int fled = -1;
    SDL_Rect vp = { 0, 0, 0, 0 }; float shake = 0; std::string msg;
};

// ---------------------------------------------------------------- input devices
enum { B_A, B_B, B_X, B_Y, B_LB, B_RB, B_START, B_BACK, B_UP, B_DOWN, B_LEFT, B_RIGHT, B_RT, NB };
struct Dev {
    int id = -1; SDL_GameController* gc = nullptr; std::string name; bool nintendo = false, wii = false;
    bool b[NB] = {}, pb[NB] = {}, hit[NB] = {}, pend[NB] = {};
    V2 mv; bool analog = false; float lt = 0, rt = 0, ry = 0; /* triggers + right stick (pads) */ int navX = 0, navY = 0, nxLast = 0, nyLast = 0; float nxT = 0, nyT = 0;
    V2 mouse; bool mouseMoved = false, click = false, rclick = false; float rtPrev = 0;
#ifdef IH_WII
    WiiRemote* wr = nullptr;
#endif
};

enum { ST_TITLE, ST_LOBBY, ST_HELP, ST_OPTIONS, ST_GAME };
static const char* RESNAMES[5] = { "AUTO", "1280 X 720", "1920 X 1080", "2560 X 1440", "3840 X 2160" };
static const int RESH[5] = { 0, 720, 1080, 1440, 2160 };
static const char* DIFFN[3] = { "EASY", "NORMAL", "HARD" };
static const char* SPEEDN[3] = { "RELAXED", "NORMAL", "FAST" };
static const float SPEEDV[3] = { 0.8f, 1.0f, 1.4f };
static const char* INFON[3] = { "AUTO", "CLASSIC", "OPEN" };
struct Opts { int res = 0; bool glow = true; int sfx = 8, mus = 5, speed = 1, info = 0, wiiDpad = 0; };

struct Game {
    SDL_Window* win = nullptr; SDL_Renderer* ren = nullptr; SDL_Texture* target = nullptr;
    int outW = 1280, outH = 720, W = 1280, H = 720; bool useTarget = false;
    Canvas cv; Opts opt; std::string cfgPath;
    int state = ST_TITLE, menuSel = 0; float stateT = 0;
    std::vector<Dev> devs;
    // lobby
    int slotKind[4] = { 0, 2, 0, 0 };   // 0 off, 1 human, 2 cpu
    int slotDev[4] = { -9, -9, -9, -9 }, slotDiff[4] = { 1, 1, 1, 1 }; int teams = 0, mapKind = 0; int lobbySel = 0;
    // match
    Side side[4]; std::vector<Island> isl; std::vector<Fleet> fl; std::vector<Mine> mines; std::vector<Contact> con; std::vector<Part> parts;
    Battle bt[4]; float time = 0, pingT = 0; int nSides = 0; unsigned seed = 1980; bool openInfo = false;
    int navW = 0, navH = 0; float navC = 20; std::vector<uint8_t> nav;
    bool paused = false; int pauseSel = 0, pauser = -1; int over = 0; float overT = 0; int winTeam = -1; std::string overMsg; int resultSel = 0;
    SDL_Rect mapVp = { 0, 0, 0, 0 }; bool mapRunning = true; float banT = 0; std::string ban; Col banCol;
    bool quit = false; int frame = 0;
    // tests
    int shotFrame = -1; std::string shotPath = "shot.bmp"; int fast = 1; bool allCpu = false;
#ifdef IH_WII
    WiiInput wii;
#endif
} G;

static int teamOf(int s) { return G.side[s].team; }
static bool hostile(int a, int b) { return teamOf(a) != teamOf(b); }
static int humans() { int n = 0; for (auto& s : G.side) if (s.used && !s.cpu) n++; return n; }

// ---------------------------------------------------------------- config
static void saveCfg() {
    FILE* f = fopen(G.cfgPath.c_str(), "w"); if (!f) return;
    fprintf(f, "res=%d\nglow=%d\nsfx=%d\nmusic=%d\nspeed=%d\ninfo=%d\nwiidpad=%d\n", G.opt.res, G.opt.glow, G.opt.sfx, G.opt.mus, G.opt.speed, G.opt.info, G.opt.wiiDpad);
    fclose(f);
#ifdef __EMSCRIPTEN__
    EM_ASM({ try { localStorage.setItem('ironharbor.cfg', FS.readFile(UTF8ToString($0), { encoding: 'utf8' })); } catch (e) {} }, G.cfgPath.c_str());
#endif
}
static void loadCfg() {
#ifdef __EMSCRIPTEN__
    EM_ASM({ try { var t = localStorage.getItem('ironharbor.cfg'); if (t) FS.writeFile(UTF8ToString($0), t); } catch (e) {} }, G.cfgPath.c_str());
#endif
    FILE* f = fopen(G.cfgPath.c_str(), "r"); if (!f) return;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        char* eq = strchr(line, '='); if (!eq) continue; *eq = 0; std::string s = line; long v = atol(eq + 1);
        if (s == "res") G.opt.res = (int)clampf((float)v, 0, 4); else if (s == "glow") G.opt.glow = v != 0;
        else if (s == "sfx") G.opt.sfx = (int)clampf((float)v, 0, 10); else if (s == "music") G.opt.mus = (int)clampf((float)v, 0, 10);
        else if (s == "speed") G.opt.speed = (int)clampf((float)v, 0, 2); else if (s == "info") G.opt.info = (int)clampf((float)v, 0, 2);
        else if (s == "wiidpad") G.opt.wiiDpad = (int)clampf((float)v, 0, 1);
    }
    fclose(f);
}
static void applyAudioVol() { AU.sfxVol = G.opt.sfx / 10.f; AU.musVol = G.opt.mus / 10.f * 0.9f; }
static void setupTarget() {
    if (G.target) { SDL_DestroyTexture(G.target); G.target = nullptr; }
    SDL_GetRendererOutputSize(G.ren, &G.outW, &G.outH);
    int h = RESH[G.opt.res];
    if (h == 0 || h == G.outH) { G.useTarget = false; G.W = G.outW; G.H = G.outH; }
    else {
        G.H = h; G.W = (int)(h * (float)G.outW / G.outH + 0.5f);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        G.target = SDL_CreateTexture(G.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, G.W, G.H);
        G.useTarget = G.target != nullptr; if (!G.useTarget) { G.W = G.outW; G.H = G.outH; }
    }
    G.cv.px = G.H / 1080.f;
    printf("video: output %dx%d, render %dx%d\n", G.outW, G.outH, G.W, G.H);
}

// ---------------------------------------------------------------- effects
static void addPart(int layer, int kind, V2 p, V2 v, float life, Col c, float a = 0, float b = 0) {
    if (G.parts.size() > 4000) return;
    Part q; q.layer = layer; q.kind = kind; q.p = p; q.v = v; q.life = q.max = life; q.c = c; q.a = a; q.b = b; G.parts.push_back(q);
}
static void sparks(int layer, V2 p, int n, Col c, float spd) { for (int i = 0; i < n; i++) addPart(layer, P_SPARK, p, fromAng(R.range(0, 6.28f)) * R.range(spd * 0.3f, spd), R.range(0.25f, 0.7f), c); }
static void ring(int layer, V2 p, float r0, float r1, Col c, float life) { addPart(layer, P_RING, p, V2(), life, c, r0, r1); }
static void explode(int layer, V2 p, float size, Col c) {
    ring(layer, p, size * 0.3f, size * 2.2f, C_WHITE, 0.35f); ring(layer, p, size * 0.2f, size * 3.0f, c, 0.7f);
    sparks(layer, p, 10 + (int)(size * 0.5f), Col(255, 200, 120), 90 + size * 5); sparks(layer, p, 6, C_WHITE, 70 + size * 3);
    addPart(layer, P_DOT, p, V2(), 0.3f, Col(255, 220, 150), size * 2.2f);
    for (int i = 0; i < 4; i++) addPart(layer, P_SMOKE, p + fromAng(R.range(0, 6.28f)) * size * 0.3f, fromAng(R.range(0, 6.28f)) * 10, R.range(1.5f, 2.6f), Col(90, 90, 100), size * 0.6f, size * 1.8f);
}
static void splash(int layer, V2 p, float size) {
    ring(layer, p, 2, size * 1.6f, Col(200, 235, 255), 0.6f); ring(layer, p, 1, size, C_WHITE, 0.35f);
    for (int i = 0; i < 8; i++) addPart(layer, P_SPARK, p, fromAng(R.range(0, 6.28f)) * R.range(15, 60), R.range(0.3f, 0.6f), Col(190, 230, 255));
}
static void toast(Side& s, const std::string& t) { s.toast = t; s.toastT = 2.2f; }
static void banner(const std::string& t, Col c) { G.ban = t; G.banT = 3.5f; G.banCol = c; }
static void rumbleSide(int s, int ms);   // ui.inc

// ---------------------------------------------------------------- geometry
static bool inPoly(const std::vector<V2>& p, V2 q) {
    bool in = false; int n = (int)p.size();
    for (int i = 0, j = n - 1; i < n; j = i++) if (((p[i].y > q.y) != (p[j].y > q.y)) && (q.x < (p[j].x - p[i].x) * (q.y - p[i].y) / (p[j].y - p[i].y) + p[i].x)) in = !in;
    return in;
}
static float segDist(V2 p, V2 a, V2 b) { V2 ab = b - a; float t = clampf(dot(p - a, ab) / std::max(len2(ab), 1e-6f), 0, 1); return dist(p, a + ab * t); }
static float polyDist(const std::vector<V2>& p, V2 q) { float d = 1e9f; for (size_t i = 0; i < p.size(); i++) d = std::min(d, segDist(q, p[i], p[(i + 1) % p.size()])); return d; }
// > 0 outside land (distance to the nearest coast), < 0 inside
static float landDist(V2 q, float maxCare = 200) {
    float best = maxCare;
    for (auto& is : G.isl) {
        if (q.x < is.minx - maxCare || q.x > is.maxx + maxCare || q.y < is.miny - maxCare || q.y > is.maxy + maxCare) continue;
        float d = polyDist(is.pts, q); if (inPoly(is.pts, q)) return -d; best = std::min(best, d);
    }
    return best;
}
static bool onLand(V2 q, float pad = 0) { return landDist(q, pad + 1) < pad; }

// ---------------------------------------------------------------- map generator
// Islands are smooth radial blobs. Maps are mirrored so every harbor has the same sea around it:
// 2 admirals: harbors west and east, point symmetry. 3-4 admirals: harbors in the corners, mirrored both ways.
static Island makeIsland(Rng& rg, V2 c, float r, float stretch, float rot) {
    Island is; is.c = c; is.r = r; int n = 22 + rg.i(8);
    float ph[3] = { rg.range(0, 6.28f), rg.range(0, 6.28f), rg.range(0, 6.28f) }, am[3] = { rg.range(0.12f, 0.22f), rg.range(0.06f, 0.14f), rg.range(0.03f, 0.08f) };
    for (int i = 0; i < n; i++) {
        float a = i * 2 * PI / n;
        float k = 1 + am[0] * std::sin(2 * a + ph[0]) + am[1] * std::sin(3 * a + ph[1]) + am[2] * std::sin(5 * a + ph[2]);
        V2 v = fromAng(a) * (r * k); v.x *= stretch; v.y /= std::sqrt(stretch);
        is.pts.push_back(c + rotv(v, rot));
    }
    return is;
}
static void finishIsland(Island& is) {
    is.minx = is.miny = 1e9f; is.maxx = is.maxy = -1e9f;
    for (auto& p : is.pts) { is.minx = std::min(is.minx, p.x); is.miny = std::min(is.miny, p.y); is.maxx = std::max(is.maxx, p.x); is.maxy = std::max(is.maxy, p.y); }
}
static Island mirrorIsland(const Island& s, bool mx, bool my) {
    Island o = s; o.c = V2(mx ? MW - s.c.x : s.c.x, my ? MH - s.c.y : s.c.y);
    for (auto& p : o.pts) p = V2(mx ? MW - p.x : p.x, my ? MH - p.y : p.y);
    if (mx != my) std::reverse(o.pts.begin(), o.pts.end());
    return o;
}
static void harborLayout(int nSlots, V2* pos, V2* dir) {
    if (nSlots <= 2) { pos[0] = V2(105, MH / 2); dir[0] = V2(1, 0); pos[1] = V2(MW - 105, MH / 2); dir[1] = V2(-1, 0); return; }
    V2 c[4] = { V2(95, 95), V2(MW - 95, MH - 95), V2(MW - 95, 95), V2(95, MH - 95) };
    for (int i = 0; i < 4; i++) { pos[i] = c[i]; dir[i] = norm(V2(MW / 2, MH / 2) - c[i]); }
}
static void buildNav();
static bool navPath(V2 a, V2 b, std::vector<V2>& out);
static void genMap(unsigned seed, int nSlots) {
    for (int attempt = 0; attempt < 40; attempt++) {
        Rng rg; rg.s = seed * 2654435761u + 12345 + attempt * 7919; for (int i = 0; i < 5; i++) rg.next();
        G.isl.clear();
        V2 hp[4], hd[4]; harborLayout(nSlots, hp, hd);
        bool four = nSlots > 2;
        std::vector<Island> base;
        // headlands around each harbor mouth make a bay (generated for harbor 0, mirrored for the others)
        if (!four) {
            for (int k = -1; k <= 1; k += 2) { Island h = makeIsland(rg, hp[0] + V2(-60, k * 190.f), 120, 0.75f, 0); base.push_back(h); }
        } else {
            base.push_back(makeIsland(rg, V2(330, 20), 125, 1.4f, 0.1f));
            base.push_back(makeIsland(rg, V2(20, 330), 125, 1.4f, PI / 2 - 0.1f));
        }
        // open-sea islands in the generating region (west half / top-left quadrant), away from harbors and each other
        float rx = four ? MW / 2 : MW / 2, ry = four ? MH / 2 : MH;
        int want = four ? 4 : 7;
        for (int tries = 0; tries < 400 && (int)base.size() < want + 2; tries++) {
            float r = rg.range(55, 130); V2 c(rg.range(150, rx - 60), rg.range(90, ry - 90));
            bool ok = true;
            for (int h = 0; h < (four ? 4 : 2); h++) if (dist(c, hp[h]) < r + 330) ok = false;
            for (auto& b : base) if (dist(b.c, c) < b.r + r + 150) ok = false;
            if (!four && dist(c, V2(MW / 2, MH / 2)) < r + 260) ok = false;
            if (four && dist(c, V2(MW / 2, MH / 2)) < r + 330) ok = false;
            // mirrored copies must not touch this one across the mirror lines
            if (c.x > rx - r - 70) ok = false;
            if (four && c.y > ry - r - 70) ok = false;
            if (!ok) continue;
            base.push_back(makeIsland(rg, c, r, rg.range(0.7f, 1.6f), rg.range(0, 6.28f)));
        }
        for (auto& b : base) {
            G.isl.push_back(b);
            if (!four) G.isl.push_back(mirrorIsland(b, true, true));
            else { G.isl.push_back(mirrorIsland(b, true, false)); G.isl.push_back(mirrorIsland(b, false, true)); G.isl.push_back(mirrorIsland(b, true, true)); }
        }
        // a big island in the middle, symmetric by construction
        {
            Island m; m.c = V2(MW / 2, MH / 2); float r = four ? rg.range(150, 200) : rg.range(110, 160); m.r = r; int n = 32;
            float ph = rg.range(0, 6.28f), a2 = rg.range(0.1f, 0.25f), a4 = rg.range(0.05f, 0.15f);
            for (int i = 0; i < n; i++) {
                float a = i * 2 * PI / n;
                float k = 1 + a2 * std::cos(2 * a + (four ? 0 : ph)) + a4 * std::cos(4 * a + (four ? 0 : ph * 2));
                V2 v = fromAng(a) * (r * k); if (!four) v.x *= 0.7f; else v.x *= 1.5f;
                m.pts.push_back(m.c + v);
            }
            if (rg.f() < 0.8f || four) G.isl.push_back(m);
        }
        for (auto& is : G.isl) finishIsland(is);
        buildNav();
        bool ok = true; std::vector<V2> path;
        int nh = four ? 4 : 2;
        for (int a = 0; a < nh && ok; a++) for (int b = a + 1; b < nh && ok; b++) if (!navPath(hp[a] + hd[a] * 80, hp[b] + hd[b] * 80, path)) ok = false;
        if (ok) { printf("map: seed %u attempt %d, %d islands\n", seed, attempt, (int)G.isl.size()); return; }
    }
    printf("map: no connected layout found, using the last one\n");
}

// ---------------------------------------------------------------- navigation (grid A*, then string-pulled)
static void buildNav() {
    G.navW = (int)(MW / G.navC) + 1; G.navH = (int)(MH / G.navC) + 1; G.nav.assign(G.navW * G.navH, 0);
    for (int y = 0; y < G.navH; y++) for (int x = 0; x < G.navW; x++) {
        V2 p((x + 0.5f) * G.navC, (y + 0.5f) * G.navC);
        bool edge = p.x < 18 || p.y < 18 || p.x > MW - 18 || p.y > MH - 18;
        G.nav[y * G.navW + x] = (edge || landDist(p, 40) < 24) ? 1 : 0;
    }
}
static bool navBlocked(int x, int y) { return x < 0 || y < 0 || x >= G.navW || y >= G.navH || G.nav[y * G.navW + x]; }
static bool navFree(V2 p) { return !navBlocked((int)(p.x / G.navC), (int)(p.y / G.navC)); }
static bool lineFree(V2 a, V2 b) {
    float L = dist(a, b); int n = (int)(L / (G.navC * 0.45f)) + 1;
    for (int i = 0; i <= n; i++) if (!navFree(a + (b - a) * (i / (float)n))) return false;
    return true;
}
static V2 nearestFree(V2 p) {
    if (navFree(p)) return p;
    for (float r = G.navC; r < 400; r += G.navC * 0.5f) for (int k = 0; k < 24; k++) { V2 q = p + fromAng(k * PI / 12) * r; if (navFree(q)) return q; }
    return p;
}
static bool navPath(V2 a, V2 b, std::vector<V2>& out) {
    out.clear(); a = nearestFree(a); b = nearestFree(b);
    if (lineFree(a, b)) { out.push_back(b); return true; }
    int W = G.navW, H = G.navH, s = (int)(a.y / G.navC) * W + (int)(a.x / G.navC), g = (int)(b.y / G.navC) * W + (int)(b.x / G.navC);
    std::vector<float> cost(W * H, 1e30f); std::vector<int> par(W * H, -1);
    typedef std::pair<float, int> QE; std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
    auto hfun = [&](int i) { float dx = (float)(i % W - g % W), dy = (float)(i / W - g / W); return std::sqrt(dx * dx + dy * dy); };
    cost[s] = 0; q.push({ hfun(s), s });
    static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    bool found = false;
    while (!q.empty()) {
        QE e = q.top(); q.pop(); int u = e.second; if (u == g) { found = true; break; }
        if (e.first - hfun(u) > cost[u] + 1e-3f) continue;
        int ux = u % W, uy = u / W;
        for (int k = 0; k < 8; k++) {
            int vx = ux + DX[k], vy = uy + DY[k]; if (navBlocked(vx, vy)) continue;
            if (k >= 4 && (navBlocked(ux + DX[k], uy) || navBlocked(ux, uy + DY[k]))) continue;
            int v = vy * W + vx; float c = cost[u] + (k < 4 ? 1.f : 1.4142f);
            if (c < cost[v]) { cost[v] = c; par[v] = u; q.push({ c + hfun(v), v }); }
        }
    }
    if (!found) return false;
    std::vector<V2> raw; for (int u = g; u != -1; u = par[u]) raw.push_back(V2((u % W + 0.5f) * G.navC, (u / W + 0.5f) * G.navC));
    std::reverse(raw.begin(), raw.end()); raw.back() = b;
    V2 cur = a; size_t i = 0;
    while (i < raw.size()) {
        size_t j = raw.size() - 1;
        while (j > i && !lineFree(cur, raw[j])) j--;
        out.push_back(raw[j]); cur = raw[j]; i = j + 1;
    }
    return true;
}
static float pathLen(V2 a, const std::vector<V2>& p) { float L = 0; V2 c = a; for (auto& q : p) { L += dist(c, q); c = q; } return L; }

// ---------------------------------------------------------------- fleets
static Ship& shipOf(const Fleet& f, int k) { return G.side[f.owner].ship[f.s[k]]; }
static int flagship(const Fleet& f) { for (int k = 0; k < f.n; k++) if (shipOf(f, k).st == 1) return k; return -1; }
static bool hasType(const Fleet& f, int t) { for (int k = 0; k < f.n; k++) if (shipOf(f, k).st == 1 && shipOf(f, k).t == t) return true; return false; }
static int aliveIn(const Fleet& f) { int n = 0; for (int k = 0; k < f.n; k++) if (shipOf(f, k).st == 1) n++; return n; }
static float fleetKnots(const Fleet& f) {
    float k = 99; for (int i = 0; i < f.n; i++) if (shipOf(f, i).st == 1) k = std::min(k, SD[shipOf(f, i).t].knots);
    if (f.sweep && hasType(f, T_SWEEPER)) k = std::min(k, SWEEP_KN);
    return k > 90 ? 20 : k;
}
static float fleetHpFrac(const Fleet& f) { float h = 0, m = 0; for (int i = 0; i < f.n; i++) { const Ship& s = shipOf(f, i); m += SD[s.t].armor; if (s.st == 1) h += std::max(0.f, s.hp); } return m > 0 ? h / m : 0; }
static int fleetsAtSea(int s) { int n = 0; for (auto& f : G.fl) if (!f.dead && f.owner == s) n++; return n; }
static bool canCaptureAny(int s) { return G.side[s].ship[0].st == 2 && G.side[s].ship[1].st == 2; }   // carrier and transport both sunk: any ship captures
static bool fleetCanCapture(const Fleet& f) { return hasType(f, T_CARRIER) || hasType(f, T_TRANSPORT) || (canCaptureAny(f.owner) && aliveIn(f) > 0); }
static void compactFleet(Fleet& f) {
    int k = 0; for (int i = 0; i < f.n; i++) if (shipOf(f, i).st == 1) f.s[k++] = f.s[i];
    for (int i = k; i < 3; i++) f.s[i] = -1; f.n = k; if (!k) f.dead = true;
}
static int launchFleet(int si, const int* idx, int n) {
    Side& S = G.side[si]; if (n <= 0 || fleetsAtSea(si) >= MAXFLEETS) return -1;
    Fleet f; f.owner = si; f.n = n; for (int i = 0; i < n; i++) { f.s[i] = idx[i]; S.ship[idx[i]].st = 1; }
    f.p = nearestFree(S.harbor + S.hdir * 95 + perp(S.hdir) * R.range(-30, 30)); f.ang = std::atan2(S.hdir.y, S.hdir.x); f.launched = G.time;
    G.fl.push_back(f); AU.play(SND_LAUNCH, 0.5f);
    return (int)G.fl.size() - 1;
}
// first ship in harbor of type t, or -1
static int shipInHarbor(int si, int t) { for (int i = 0; i < NSHIPS; i++) if (G.side[si].ship[i].t == t && G.side[si].ship[i].st == 0) return i; return -1; }
static void orderMove(int fi, V2 to, bool dock) {
    Fleet& f = G.fl[fi]; if (f.dead || f.battle >= 0) return;
    std::vector<V2> p; if (!navPath(f.p, to, p)) { f.moving = false; return; }
    f.path = p; f.moving = true; f.dock = dock;
}
static void layMine(int fi) {
    Fleet& f = G.fl[fi]; Side& S = G.side[f.owner];
    int n = 0; for (auto& m : G.mines) if (!m.dead && m.owner == f.owner) n++;
    if (!hasType(f, T_LAYER)) { toast(S, "NEEDS THE MINE LAYER"); AU.play(SND_ERR, 0.5f); return; }
    if (n >= MAXMINES) { toast(S, "ALL 4 MINEFIELDS ARE OUT"); AU.play(SND_ERR, 0.5f); return; }
    if (dist(f.p, S.harbor) < HARBOR_R + MINE_R + 20) { toast(S, "TOO CLOSE TO YOUR HARBOR"); AU.play(SND_ERR, 0.5f); return; }
    Mine m; m.owner = f.owner; m.p = f.p; m.r = MINE_R; m.laidT = G.time; G.mines.push_back(m);
    f.lastMine = (int)G.mines.size() - 1;   // the fleet that laid it sits inside: harmless to its own side anyway
    toast(S, "MINEFIELD LAID (" + std::to_string(MAXMINES - n - 1) + " LEFT)"); printf("mine: P%d laid a field at %.0f,%.0f\n", f.owner + 1, f.p.x, f.p.y);
    AU.play(SND_MINE, 0.25f, 0, 1.6f);
}
static void eliminate(int si, const std::string& why) {
    Side& S = G.side[si]; if (!S.alive) return;
    S.alive = false; S.capturedAt = G.time; S.outMsg = why; printf("out: P%d %s at %.0f s\n", si + 1, why.c_str(), G.time);
    for (auto& f : G.fl) if (!f.dead && f.owner == si) { if (f.battle < 0) { explode(-1, f.p, 14, S.col); f.dead = true; } }
    for (auto& m : G.mines) if (m.owner == si) m.dead = true;
    banner("P" + std::to_string(si + 1) + " " + why, S.col);
    AU.play(SND_LOSE, 0.7f);
    rumbleSide(si, 500);
}

#include "battle.inc"
#include "sim.inc"
#include "ai.inc"
#include "render.inc"
#include "ui.inc"
