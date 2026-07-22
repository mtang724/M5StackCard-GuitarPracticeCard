#pragma once
#include "App.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Tuner — detects the pitch you play on the built-in microphone and shows the
// nearest note, how many cents sharp/flat, and which guitar string it is.
//
// Pitch detection uses the YIN algorithm (cumulative mean normalized
// difference function) which is accurate and robust for low guitar notes.
//
// The mic and speaker share I2S, so we stop the speaker on entry and restore
// it on exit — the metronome still clicks after you leave the tuner.
// ---------------------------------------------------------------------------
class TunerApp : public App {
  static const int SR      = 16000;                 // sample rate
  static const int N       = 2048;                  // samples per analysis (128 ms)
  static const int TAU_MAX = SR / 70;               // 228 -> lowest ~70 Hz
  static const int TAU_MIN = SR / 400;              // 40  -> highest ~400 Hz

  int16_t buf[N];
  float   x[N];
  float   d[TAU_MAX + 1];
  float   cmnd[TAU_MAX + 1];

  bool  capturing = false;
  bool  micOk     = true;
  float freq      = 0;      // detected frequency, 0 = none
  float level     = 0;      // RMS input level
  int   midi      = -1;     // nearest MIDI note
  float cents     = 0;      // deviation from that note

  // last confirmed note, kept on screen briefly after you stop playing
  int      lastMidi  = -1;
  float    lastCents = 0, lastFreq = 0;
  uint32_t lastGoodMs = 0;
  static const uint32_t HOLD_MS = 2000;

  static float log2f_(float v) { return logf(v) * 1.44269504f; }

  void analyze() {
    // DC removal + level
    long sum = 0;
    for (int i = 0; i < N; i++) sum += buf[i];
    float mean = (float)sum / N;
    float energy = 0;
    for (int i = 0; i < N; i++) { x[i] = buf[i] - mean; energy += x[i] * x[i]; }
    level = sqrtf(energy / N);

    if (level < 90) { freq = 0; midi = -1; return; }   // treat as silence

    // YIN difference function over the comparison window
    const int W = N - TAU_MAX;
    d[0] = 0;
    for (int tau = 1; tau <= TAU_MAX; tau++) {
      float s = 0;
      for (int i = 0; i < W; i++) { float diff = x[i] - x[i + tau]; s += diff * diff; }
      d[tau] = s;
    }
    // cumulative mean normalized difference
    cmnd[0] = 1; float run = 0;
    for (int tau = 1; tau <= TAU_MAX; tau++) { run += d[tau]; cmnd[tau] = run > 0 ? d[tau] * tau / run : 1; }

    // absolute-threshold pick, then descend to the local minimum
    const float THRESH = 0.15f;
    int tau = -1;
    for (int t = TAU_MIN; t <= TAU_MAX; t++) {
      if (cmnd[t] < THRESH) {
        while (t + 1 <= TAU_MAX && cmnd[t + 1] < cmnd[t]) t++;
        tau = t; break;
      }
    }
    if (tau < 0) {   // nothing below threshold: take global min if clear enough
      float m = 1e30f;
      for (int t = TAU_MIN; t <= TAU_MAX; t++) if (cmnd[t] < m) { m = cmnd[t]; tau = t; }
      if (m > 0.5f) { freq = 0; midi = -1; return; }
    }

    // parabolic interpolation for sub-sample precision
    float betterTau = tau;
    if (tau > 1 && tau < TAU_MAX) {
      float a = d[tau - 1], b = d[tau], c = d[tau + 1], denom = a + c - 2 * b;
      if (fabsf(denom) > 1e-6f) betterTau = tau + 0.5f * (a - c) / denom;
    }

    freq = (float)SR / betterTau;
    midi = (int)lroundf(69 + 12 * log2f_(freq / 440.0f));
    float ref = 440.0f * powf(2.0f, (midi - 69) / 12.0f);
    cents = 1200.0f * log2f_(freq / ref);

    lastMidi = midi; lastCents = cents; lastFreq = freq; lastGoodMs = millis();
  }

public:
  const char* title() const override { return "Tuner"; }

  void onEnter() override {
    M5Cardputer.Speaker.end();               // free I2S for the mic
    micOk = M5Cardputer.Mic.begin();
    capturing = false; freq = 0; midi = -1; dirty = true;
  }

  void onExit() override {
    M5Cardputer.Mic.end();
    M5Cardputer.Speaker.begin();             // restore click for the metronome apps
    M5Cardputer.Speaker.setVolume(g.volume);
  }

  void tick() override {
    if (!micOk) return;
    if (capturing) {
      if (!M5Cardputer.Mic.isRecording()) { analyze(); capturing = false; dirty = true; }
    } else {
      if (M5Cardputer.Mic.record(buf, N, SR)) capturing = true;
    }
  }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();
    ui::header("Tuner");

    if (!micOk) {
      d.setFont(&fonts::Font2); d.setTextColor(ui::COL_ACCENT, ui::COL_BG);
      d.setTextDatum(middle_center); d.drawString("Mic init failed", 120, 60);
      ui::footer("`:back");
      return;
    }

    // Pick what to show: the live note, or the last one held briefly after you
    // stop playing (dimmed to signal it's stale), or nothing.
    int   dMidi;  float dCents, dFreq;  bool held;
    if (midi >= 0) {
      dMidi = midi; dCents = cents; dFreq = freq; held = false;
    } else if (lastMidi >= 0 && millis() - lastGoodMs < HOLD_MS) {
      dMidi = lastMidi; dCents = lastCents; dFreq = lastFreq; held = true;
    } else {
      dMidi = -1; dCents = 0; dFreq = 0; held = false;
    }

    if (dMidi < 0) {
      d.setTextDatum(middle_center);
      d.setFont(&fonts::Font4); d.setTextColor(ui::COL_DIM, ui::COL_BG);
      d.drawString("--", 120, 46);
      d.setFont(&fonts::Font2);
      d.drawString("play a single string", 120, 78);
      drawLevel();
      ui::footer("`:back");
      return;
    }

    static const char* NAMES[12] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    int octave = dMidi / 12 - 1;
    char note[8]; snprintf(note, sizeof(note), "%s%d", NAMES[dMidi % 12], octave);

    bool inTune = !held && fabsf(dCents) <= 5.0f;
    uint16_t good = 0x07E0;  // green
    // colours dim while holding the last note
    uint16_t noteCol   = held ? ui::COL_DIM : (inTune ? good : ui::COL_FG);
    uint16_t needleCol = held ? ui::COL_DIM : (inTune ? good : ui::COL_ACCENT);

    // big note name
    d.setTextDatum(middle_center);
    d.setFont(&fonts::Font4); d.setTextSize(2);
    d.setTextColor(noteCol, ui::COL_BG);
    d.drawString(note, 120, 46);
    d.setTextSize(1);

    // nearest guitar string label + frequency
    d.setFont(&fonts::Font2); d.setTextColor(ui::COL_DIM, ui::COL_BG);
    char info[40]; snprintf(info, sizeof(info), "%s   %.1f Hz", nearestString(dMidi), dFreq);
    d.drawString(info, 120, 76);

    // cents meter: track with centre mark and a moving needle
    int cx = 120, y = 100, half = 100;
    d.drawFastHLine(cx - half, y, half * 2, ui::COL_DIM);
    d.drawFastVLine(cx, y - 8, 16, ui::COL_STRONG);           // centre (in tune)
    float c = constrain(dCents, -50.0f, 50.0f);
    int nx = cx + (int)(c / 50.0f * half);
    d.fillTriangle(nx, y - 10, nx - 5, y - 2, nx + 5, y - 2, needleCol);
    d.fillRect(nx - 1, y - 2, 3, 12, needleCol);

    d.setFont(&fonts::Font0); d.setTextColor(ui::COL_DIM, ui::COL_BG);
    d.setTextDatum(top_left);  d.drawString("flat", cx - half, y + 6);
    d.setTextDatum(top_right); d.drawString("sharp", cx + half, y + 6);
    d.setTextDatum(top_center);
    char cbuf[12]; snprintf(cbuf, sizeof(cbuf), "%+d cents", (int)lroundf(dCents));
    d.setTextColor(held ? ui::COL_DIM : (inTune ? good : ui::COL_FG), ui::COL_BG);
    d.drawString(cbuf, cx, y + 6);

    ui::footer("`:back");
  }

private:
  void drawLevel() {
    auto& d = ui::canvas;
    int w = (int)constrain(level / 20.0f, 0.0f, 200.0f);
    d.drawRect(20, 108, 200, 8, ui::COL_DIM);
    d.fillRect(20, 108, w, 8, ui::COL_STRONG);
  }

  static const char* nearestString(int m) {
    struct S { int midi; const char* name; };
    static const S strs[6] = {
      {40, "6th (E2)"}, {45, "5th (A2)"}, {50, "4th (D3)"},
      {55, "3rd (G3)"}, {59, "2nd (B3)"}, {64, "1st (E4)"},
    };
    int best = 0, bd = 999;
    for (int i = 0; i < 6; i++) { int diff = abs(strs[i].midi - m); if (diff < bd) { bd = diff; best = i; } }
    return strs[best].name;
  }
};
