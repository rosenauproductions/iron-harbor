// Iron Harbor - synthesized sound effects and an ambient sea loop, mixed in an SDL audio callback.
#pragma once
#include <SDL.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include "gfx.h"

enum Snd { SND_GUN, SND_SPLASH, SND_TORP, SND_BOOM, SND_HIT, SND_PING, SND_KLAXON, SND_ERR, SND_UI, SND_UI_OK,
           SND_WIN, SND_LOSE, SND_AGROUND, SND_MINE, SND_LAUNCH, SND_DOCK, SND_SWEEP, SND_GQ, SND_N };

struct Audio {
    static const int RATE = 44100;
    SDL_AudioDeviceID dev = 0;
    std::vector<float> snd[SND_N];
    std::vector<float> music;
    struct Inst { int s; double pos; float rate, vol, pan; };
    std::vector<Inst> inst;
    float sfxVol = 0.8f, musVol = 0.5f;
    Uint32 lastPlay[SND_N] = {};
    bool musicOn = false;

    static float sq(float ph) { return std::sin(ph) >= 0 ? 1.f : -1.f; }
    std::vector<float> gen(float dur, float (*f)(float t, float d, Rng& r, float& st)) {
        int n = (int)(dur * RATE); std::vector<float> v(n); Rng r; r.s = 99; float st = 0;
        for (int i = 0; i < n; i++) v[i] = f(i / (float)RATE, dur, r, st);
        return v;
    }
    void build() {
        snd[SND_GUN] = gen(0.9f, [](float t, float, Rng& r, float& lp) { float k = 0.3f * std::exp(-t * 6) + 0.015f; lp += (r.range(-1, 1) - lp) * k; float sub = std::sin(2 * PI * (70 - 30 * t) * t) * std::exp(-t * 7); return (lp * 2.6f * std::exp(-t * 5) + sub * 0.9f) * 0.55f * std::min(1.f, t * 400); });
        snd[SND_SPLASH] = gen(0.7f, [](float t, float, Rng& r, float& lp) { float n = r.range(-1, 1); float hp = n - lp; lp += (n - lp) * 0.3f; return (hp * 0.7f + lp * 0.5f) * std::exp(-t * 5) * std::min(1.f, t * 50) * 0.4f; });
        snd[SND_TORP] = gen(0.8f, [](float t, float, Rng& r, float& lp) { lp += (r.range(-1, 1) - lp) * 0.05f; float bub = std::sin(2 * PI * (300 + 80 * std::sin(t * 40)) * t) * 0.15f; return (lp * 2.0f + bub) * std::min(1.f, t * 8) * std::exp(-t * 2.5f) * 0.5f; });
        snd[SND_BOOM] = gen(1.8f, [](float t, float, Rng& r, float& lp) { float k = 0.12f * std::exp(-t * 3) + 0.008f; lp += (r.range(-1, 1) - lp) * k; float sub = std::sin(2 * PI * (50 - 14 * t) * t) * std::exp(-t * 2.2f); return (lp * 3.0f * std::exp(-t * 2.2f) + sub * 0.8f) * 0.6f; });
        snd[SND_HIT] = gen(0.3f, [](float t, float, Rng& r, float& lp) { lp += (r.range(-1, 1) - lp) * 0.2f; float ring = std::sin(2 * PI * 420 * t) * std::exp(-t * 18); return (lp * 1.5f + ring * 0.4f) * std::exp(-t * 12) * 0.5f; });
        snd[SND_PING] = gen(1.4f, [](float t, float, Rng&, float&) { float e = std::exp(-t * 3.2f); float echo = t > 0.5f ? std::sin(2 * PI * 1180 * (t - 0.5f)) * std::exp(-(t - 0.5f) * 5) * 0.25f : 0; return (std::sin(2 * PI * 1180 * t) * e + echo) * 0.18f * std::min(1.f, t * 200); });
        snd[SND_KLAXON] = gen(1.5f, [](float t, float, Rng&, float& ph) { float f = std::fmod(t, 0.5f) < 0.38f ? 330 + 120 * std::fmod(t, 0.5f) / 0.38f : 0; if (f <= 0) return 0.f; ph += 2 * PI * f / RATE; float saw = std::fmod(ph / (2 * PI), 1.f) * 2 - 1; return (saw * 0.6f + sq(ph) * 0.3f) * 0.16f; });
        snd[SND_ERR] = gen(0.2f, [](float t, float, Rng&, float&) { return (sq(2 * PI * 140 * t) * 0.5f + sq(2 * PI * 147 * t) * 0.5f) * 0.16f * std::exp(-t * 7); });
        snd[SND_UI] = gen(0.04f, [](float t, float, Rng&, float&) { return std::sin(2 * PI * 1300 * t) * 0.2f * std::exp(-t * 70); });
        snd[SND_UI_OK] = gen(0.14f, [](float t, float, Rng&, float&) { float f = t < 0.05f ? 880.f : 1320.f; return std::sin(2 * PI * f * t) * 0.26f * std::exp(-std::fmod(t, 0.05f) * 25); });
        snd[SND_WIN] = gen(1.8f, [](float t, float, Rng&, float&) { const float nf[5] = { 392, 523, 659, 784, 1046 }; int i = std::min(4, (int)(t / 0.18f)); float v = 0; for (int k = 0; k <= i; k++) { float tt = t - k * 0.18f; v += (std::sin(2 * PI * nf[k] * t) + 0.3f * sq(2 * PI * nf[k] * t)) * std::exp(-tt * 1.8f); } return v * 0.08f; });
        snd[SND_LOSE] = gen(1.8f, [](float t, float, Rng&, float& ph) { float f = 260 * std::exp(-t * 0.7f); ph += 2 * PI * f / RATE; return (std::sin(ph) + 0.3f * sq(ph)) * 0.18f * std::exp(-t * 1.3f); });
        snd[SND_AGROUND] = gen(0.5f, [](float t, float, Rng& r, float& lp) { lp += (r.range(-1, 1) - lp) * 0.04f; float grind = sq(2 * PI * (60 + 20 * std::sin(t * 30)) * t) * 0.2f; return (lp * 2.5f + grind) * std::exp(-t * 4) * 0.45f; });
        snd[SND_MINE] = gen(1.4f, [](float t, float, Rng& r, float& lp) { float k = 0.05f * std::exp(-t * 2) + 0.004f; lp += (r.range(-1, 1) - lp) * k; float sub = std::sin(2 * PI * (38 - 8 * t) * t) * std::exp(-t * 2); return (lp * 4.0f * std::exp(-t * 2) + sub) * 0.6f; });
        snd[SND_LAUNCH] = gen(0.9f, [](float t, float, Rng&, float&) { float f = t < 0.45f ? 196 : 262; return (std::sin(2 * PI * f * t) + 0.5f * std::sin(2 * PI * f * 1.5f * t)) * 0.16f * std::min(1.f, t * 30) * std::exp(-std::fmod(t, 0.45f) * 3); });
        snd[SND_DOCK] = gen(0.6f, [](float t, float, Rng&, float&) { float f = t < 0.2f ? 523.f : t < 0.4f ? 440.f : 392.f; return std::sin(2 * PI * f * t) * 0.22f * std::exp(-std::fmod(t, 0.2f) * 10); });
        snd[SND_SWEEP] = gen(0.35f, [](float t, float, Rng&, float& ph) { float f = 600 + 900 * t / 0.35f; ph += 2 * PI * f / RATE; return std::sin(ph) * 0.18f * std::min(1.f, (0.35f - t) * 12); });
        // general quarters: a ship's alarm bell, struck 4 times (bong-bong-bong-bong)
        snd[SND_GQ] = gen(1.6f, [](float t, float, Rng&, float&) {
            float lt = std::fmod(t, 0.34f); if (t > 1.36f) lt = t - 1.02f;
            float e = std::exp(-lt * 7.5f) * std::min(1.f, lt * 600);
            const float f0 = 560; float v = std::sin(2 * PI * f0 * lt) + 0.55f * std::sin(2 * PI * f0 * 2.76f * lt) + 0.3f * std::sin(2 * PI * f0 * 5.4f * lt) * std::exp(-lt * 14);
            return v * e * 0.22f; });
        buildMusic();
    }
    // 48 s ambient loop: low swells (the sea) under slow minor chords.
    void buildMusic() {
        const float secs = 48; int n = (int)(secs * RATE); music.assign(n * 2, 0.f);
        const float chords[4][3] = { { 73.42f, 110.0f, 174.61f }, { 65.41f, 98.0f, 155.56f }, { 58.27f, 87.31f, 146.83f }, { 65.41f, 98.0f, 164.81f } };
        Rng rg; rg.s = 777; float lp = 0, lp2 = 0;
        for (int i = 0; i < n; i++) {
            float t = i / (float)RATE; int c = (int)(t / 12) % 4; float lt = std::fmod(t, 12.f);
            int cn = (c + 1) % 4; float xf = lt > 9 ? (lt - 9) / 3 : 0;
            float v = 0;
            for (int k = 0; k < 3; k++) {
                float a = chords[c][k], b = chords[cn][k];
                float va = std::sin(2 * PI * a * t) + 0.25f * std::sin(2 * PI * a * 2.002f * t);
                float vb = std::sin(2 * PI * b * t) + 0.25f * std::sin(2 * PI * b * 2.002f * t);
                v += (va * (1 - xf) + vb * xf) * (0.55f + 0.45f * std::sin(t * (0.17f + k * 0.05f)));
            }
            // waves: filtered noise with a slow swell
            lp += (rg.range(-1, 1) - lp) * 0.02f; lp2 += (lp - lp2) * 0.05f;
            float swell = 0.5f + 0.5f * std::sin(2 * PI * t / 8.0f);
            float w = lp2 * 6.0f * swell;
            music[i * 2] = v * 0.04f + w * 0.07f; music[i * 2 + 1] = v * 0.04f + w * 0.06f;
        }
        float pk = 0; for (float v : music) pk = std::max(pk, std::fabs(v));
        if (pk > 0.5f) for (float& v : music) v *= 0.5f / pk;
    }
    static void cb(void* ud, Uint8* stream, int bytes) {
        Audio* a = (Audio*)ud; float* out = (float*)stream; int frames = bytes / 8;
        for (int i = 0; i < frames * 2; i++) out[i] = 0;
        if (a->musicOn && !a->music.empty()) {
            static double mpos = 0; int mn = (int)a->music.size() / 2;
            for (int i = 0; i < frames; i++) { int p = (int)mpos; out[i * 2] += a->music[p * 2] * a->musVol; out[i * 2 + 1] += a->music[p * 2 + 1] * a->musVol; mpos += 1; if (mpos >= mn) mpos -= mn; }
        }
        for (size_t k = 0; k < a->inst.size(); ) {
            Inst& in = a->inst[k]; const std::vector<float>& s = a->snd[in.s];
            float vl = in.vol * a->sfxVol * (1 - std::max(0.f, in.pan)), vr = in.vol * a->sfxVol * (1 + std::min(0.f, in.pan));
            bool done = false;
            for (int i = 0; i < frames; i++) {
                int p = (int)in.pos; if (p >= (int)s.size()) { done = true; break; }
                float v = s[p]; out[i * 2] += v * vl; out[i * 2 + 1] += v * vr; in.pos += in.rate;
            }
            if (done) { a->inst[k] = a->inst.back(); a->inst.pop_back(); } else k++;
        }
        for (int i = 0; i < frames * 2; i++) { float v = out[i]; out[i] = v / (1 + std::fabs(v) * 0.6f); }
    }
    bool open() {
        build();
        SDL_AudioSpec want = {}, have = {};
        want.freq = RATE; want.format = AUDIO_F32SYS; want.channels = 2; want.samples =
#ifdef __EMSCRIPTEN__
            2048;
#else
            1024;
#endif
        want.callback = cb; want.userdata = this;
        dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
        if (!dev) { printf("audio: open failed: %s\n", SDL_GetError()); return false; }
        printf("audio: %d Hz, %d ch\n", have.freq, have.channels);
        musicOn = true;
        SDL_PauseAudioDevice(dev, 0);
        return true;
    }
    void play(int s, float vol = 1, float pan = 0, float pitch = 1, Uint32 minGapMs = 30) {
        if (!dev || vol <= 0.01f) return;
        Uint32 now = SDL_GetTicks(); if (now - lastPlay[s] < minGapMs) return; lastPlay[s] = now;
        SDL_LockAudioDevice(dev);
        if (inst.size() < 48) inst.push_back({ s, 0.0, pitch, vol, clampf(pan, -1, 1) });
        SDL_UnlockAudioDevice(dev);
    }
    void close() { if (dev) SDL_CloseAudioDevice(dev); dev = 0; }
};
