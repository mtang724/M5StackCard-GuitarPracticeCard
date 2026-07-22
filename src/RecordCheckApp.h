#pragma once
#include "App.h"
#include <stdlib.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Record & Check — grades how steadily you hold a tempo.
//
// Flow: a 4-beat count-in clicks on the speaker, then it goes silent and
// records the mic while you keep playing steady quarter notes. We detect note
// onsets and analyse the spacing between them, so the score is independent of
// mic start-up latency (we measure your steadiness, not absolute alignment).
//
// Mic and speaker share I2S, so we hand off between them around the recording.
// ---------------------------------------------------------------------------
class RecordCheckApp : public App {
  static const int SR       = 8000;
  static const int MAXSAMP  = 40000;          // 5 s ceiling
  static const int F        = 64;             // onset-envelope frame (8 ms)
  static const int MAXFRAME = MAXSAMP / F + 2;

  enum State { SETUP, COUNTIN, RECORD, RESULT } state = SETUP;
  int  field = 0;                             // 0 BPM, 1 Beats
  int  beats = 8;

  int16_t* buf = nullptr;
  int      totalSamp = 0;
  bool     micOk = true;

  uint32_t countStart = 0; int lastCount = -1;
  uint32_t recStart = 0;

  float env[MAXFRAME];
  float onsets[64]; int nOnsets = 0;          // onset times in ms
  float yourBpm = 0, steadyMs = 0; int hits = 0;

  int beatMs() const { return 60000 / (g.bpm < 1 ? 1 : g.bpm); }

  void click(bool strong) { M5Cardputer.Speaker.tone(strong ? 2093.0f : 1046.5f, strong ? 45 : 28); }

  void startCountIn() { state = COUNTIN; countStart = millis(); lastCount = -1; dirty = true; }

  void startRecording() {
    M5Cardputer.Speaker.stop(); M5Cardputer.Speaker.end();
    micOk = M5Cardputer.Mic.begin();
    totalSamp = (int)(beats * (60.0f / g.bpm) * SR);
    if (totalSamp > MAXSAMP) totalSamp = MAXSAMP;
    buf = (int16_t*)malloc(sizeof(int16_t) * totalSamp);
    if (!micOk || !buf) { if (buf) { free(buf); buf = nullptr; } M5Cardputer.Speaker.begin(); M5Cardputer.Speaker.setVolume(g.volume); state = RESULT; hits = -1; dirty = true; return; }
    M5Cardputer.Mic.record(buf, totalSamp, SR);
    recStart = millis(); state = RECORD; dirty = true;
  }

  void finishRecording() {
    M5Cardputer.Mic.end();
    M5Cardputer.Speaker.begin(); M5Cardputer.Speaker.setVolume(g.volume);
    analyze();
    if (buf) { free(buf); buf = nullptr; }
    state = RESULT; dirty = true;
  }

  void analyze() {
    int nf = totalSamp / F;
    if (nf > MAXFRAME) nf = MAXFRAME;
    float maxE = 0;
    for (int i = 0; i < nf; i++) {
      long acc = 0;
      for (int j = 0; j < F; j++) { int v = buf[i * F + j]; acc += (long)v * v; }
      env[i] = sqrtf((float)acc / F);
      if (env[i] > maxE) maxE = env[i];
    }
    nOnsets = 0;
    if (maxE < 60) { hits = 0; return; }        // essentially silence
    float gate = 0.15f * maxE;
    const int REFR = 100 / (F * 1000 / SR);      // ~100 ms refractory in frames
    int lastFrame = -REFR * 4;
    for (int i = 1; i < nf && nOnsets < 64; i++) {
      if (env[i] > gate && env[i] > env[i - 1] * 1.6f && (i - lastFrame) > REFR) {
        onsets[nOnsets++] = (float)i * F * 1000.0f / SR;
        lastFrame = i;
      }
    }
    hits = nOnsets;
    if (nOnsets < 3) return;                      // not enough to judge
    // intervals -> mean & std
    float sum = 0; int n = nOnsets - 1;
    for (int i = 1; i < nOnsets; i++) sum += onsets[i] - onsets[i - 1];
    float mean = sum / n;
    float var = 0;
    for (int i = 1; i < nOnsets; i++) { float d = (onsets[i] - onsets[i - 1]) - mean; var += d * d; }
    steadyMs = sqrtf(var / n);
    yourBpm  = mean > 0 ? 60000.0f / mean : 0;
  }

  const char* rating() const {
    if (steadyMs < 15) return "Tight!";
    if (steadyMs < 35) return "Good";
    if (steadyMs < 60) return "Getting there";
    return "Keep at it";
  }

public:
  const char* title() const override { return "Record & Check"; }

  void onEnter() override { state = SETUP; field = 0; dirty = true; }
  void onExit() override { if (buf) { free(buf); buf = nullptr; } }

  void handle(const KeyEvent& k) override {
    if (state == SETUP) {
      if      (k.space || k.enter) startCountIn();
      else if (k.left)  field = 0;
      else if (k.right) field = 1;
      else if (k.up || k.down) {
        int dir = k.up ? 1 : -1;
        if (field == 0) g.bpm = constrain(g.bpm + dir * 5, 40, 240);
        else { const int opt[] = {4, 8, 12, 16}; int idx = 0; for (int i = 0; i < 4; i++) if (opt[i] == beats) idx = i; idx = constrain(idx + dir, 0, 3); beats = opt[idx]; }
      }
    } else if (state == RESULT) {
      if (k.space || k.enter) { state = SETUP; }
    }
    dirty = true;
  }

  void tick() override {
    if (state == COUNTIN) {
      int b = (int)((millis() - countStart) / beatMs());
      if (b != lastCount) {
        lastCount = b;
        if (b < 4) { click(b == 0); dirty = true; }
        else startRecording();
      }
    } else if (state == RECORD) {
      if (!M5Cardputer.Mic.isRecording()) finishRecording();
      else { static uint32_t last = 0; if (millis() - last > 60) { last = millis(); dirty = true; } }
    }
  }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();

    if (state == SETUP) {
      ui::header("Record & Check");
      d.setFont(&fonts::Font2); d.setTextColor(ui::COL_FG, ui::COL_BG); d.setTextDatum(top_left);
      ui::paragraph("Count-in of 4, then play steady quarter notes. We grade how even your tempo is.", 8, 28, 224, 15);
      char v0[8], v1[8]; snprintf(v0, 8, "%d", g.bpm); snprintf(v1, 8, "%d", beats);
      ui::field(52,  84, "BPM",   v0, field == 0);
      ui::field(132, 84, "Beats", v1, field == 1);
      ui::footer("L/R field  UP/DN change  SPACE start  `:back");
      return;
    }

    if (state == COUNTIN) {
      ui::header("Get ready...");
      int b = (int)((millis() - countStart) / beatMs());
      d.setTextDatum(middle_center); d.setFont(&fonts::Font7);
      d.setTextColor(ui::COL_ACCENT, ui::COL_BG);
      d.drawString(String(4 - min(b, 3)).c_str(), 120, 60);
      d.setFont(&fonts::Font2); d.setTextColor(ui::COL_DIM, ui::COL_BG);
      d.drawString("count-in", 120, 100);
      ui::footer("");
      return;
    }

    if (state == RECORD) {
      ui::header("Recording");
      d.setTextDatum(middle_center); d.setFont(&fonts::Font4); d.setTextSize(2);
      d.setTextColor(0xF800, ui::COL_BG); d.drawString("REC", 120, 44); d.setTextSize(1);
      d.setFont(&fonts::Font2); d.setTextColor(ui::COL_FG, ui::COL_BG);
      d.drawString("keep playing steadily", 120, 74);
      // progress bar
      uint32_t total = (uint32_t)totalSamp * 1000 / SR, el = millis() - recStart;
      int w = (int)(constrain((float)el / total, 0.0f, 1.0f) * 200);
      d.drawRect(20, 96, 200, 10, ui::COL_DIM); d.fillRect(20, 96, w, 10, ui::COL_STRONG);
      ui::footer("");
      return;
    }

    // RESULT
    ui::header("Result");
    if (hits < 0) {
      d.setFont(&fonts::Font2); d.setTextColor(ui::COL_ACCENT, ui::COL_BG); d.setTextDatum(middle_center);
      d.drawString("Not enough memory / mic", 120, 60);
      ui::footer("SPACE retry  `:back");
      return;
    }
    if (hits < 3) {
      d.setTextDatum(middle_center); d.setFont(&fonts::Font4); d.setTextColor(ui::COL_DIM, ui::COL_BG);
      d.drawString("--", 120, 44);
      d.setFont(&fonts::Font2); d.setTextColor(ui::COL_FG, ui::COL_BG);
      d.drawString("play clearer plucks", 120, 78);
      char h[24]; snprintf(h, sizeof(h), "%d hits detected", hits);
      d.setFont(&fonts::Font0); d.setTextColor(ui::COL_DIM, ui::COL_BG); d.drawString(h, 120, 98);
      ui::footer("SPACE retry  `:back");
      return;
    }

    // your BPM (big)
    d.setTextDatum(middle_center); d.setFont(&fonts::Font7);
    d.setTextColor(ui::COL_FG, ui::COL_BG);
    d.drawString(String((int)lroundf(yourBpm)).c_str(), 120, 40);
    d.setFont(&fonts::Font2); d.setTextColor(ui::COL_DIM, ui::COL_BG);
    int drift = (int)lroundf(yourBpm) - g.bpm;
    char sub[40];
    snprintf(sub, sizeof(sub), "your BPM  (target %d, %+d %s)", g.bpm, drift, drift > 1 ? "rush" : (drift < -1 ? "drag" : "ok"));
    d.drawString(sub, 120, 66);

    bool tight = steadyMs < 35;
    d.setTextColor(tight ? 0x07E0 : ui::COL_ACCENT, ui::COL_BG);
    char st[40]; snprintf(st, sizeof(st), "steady +/-%d ms   %s", (int)lroundf(steadyMs), rating());
    d.drawString(st, 120, 86);

    // deviation dots: each interval relative to the mean
    float mean = yourBpm > 0 ? 60000.0f / yourBpm : 0;
    int y0 = 106, x0 = 20, x1 = 220;
    d.drawFastHLine(x0, y0, x1 - x0, ui::COL_DIM);
    int ni = hits - 1;
    for (int i = 0; i < ni; i++) {
      float dev = (onsets[i + 1] - onsets[i]) - mean;           // + = slower, - = faster
      int x = x0 + (ni > 1 ? (x1 - x0) * i / (ni - 1) : (x1 - x0) / 2);
      int yy = y0 - (int)constrain(dev / 2.0f, -14.0f, 14.0f);  // 2 ms per pixel
      d.fillCircle(x, yy, 2, fabsf(dev) < 20 ? 0x07E0 : ui::COL_ACCENT);
    }
    ui::footer("SPACE retry  `:back");
  }
};
