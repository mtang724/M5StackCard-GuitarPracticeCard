#pragma once
#include "App.h"
#include "MetronomeEngine.h"

// ---------------------------------------------------------------------------
// Metronome
//   SPACE / ENTER : start / stop
//   UP / DOWN     : BPM +/- 1     LEFT / RIGHT : BPM +/- 5
//   1..9 : beats per bar          - / = : beats -/+ 1 (up to 12)
//   s : cycle time signature      t : tap tempo      ` : back
// ---------------------------------------------------------------------------
class MetronomeApp : public App {
  MetronomeEngine e;

  static const int MAXTAPS = 6;
  uint32_t taps[MAXTAPS];
  int      tapCount = 0;
  uint32_t lastTap  = 0;

  void syncBpm(int b) { e.setBpm(b); g.bpm = e.bpm(); }
  void syncBeats(int n) { g.sigIndex = -1; g.beats = constrain(n, 1, MAX_BEATS); e.setSigFromSettings(); }

  static int digitKey(const KeyEvent& k) {
    for (char c : k.chars) if (c >= '1' && c <= '9') return c - '0';
    return 0;
  }

  void tap() {
    uint32_t now = millis();
    if (now - lastTap > 2000) tapCount = 0;
    lastTap = now;
    if (tapCount < MAXTAPS) taps[tapCount++] = now;
    else { for (int i = 1; i < MAXTAPS; i++) taps[i-1] = taps[i]; taps[MAXTAPS-1] = now; }
    if (tapCount >= 2) {
      uint32_t sum = 0; int n = 0;
      for (int i = 1; i < tapCount; i++) { sum += taps[i] - taps[i-1]; n++; }
      if (n > 0 && sum > 0) syncBpm((int)lroundf(60000.0f * n / sum));
    }
  }

public:
  const char* title() const override { return "Metronome"; }

  void onEnter() override {
    e.stop(); e.setBpm(g.bpm); e.setSigFromSettings(); dirty = true;
  }

  void handle(const KeyEvent& k) override {
    if      (k.space || k.enter) e.running ? e.stop() : e.start();
    else if (k.up)    syncBpm(e.bpm() + 1);
    else if (k.down)  syncBpm(e.bpm() - 1);
    else if (k.right) syncBpm(e.bpm() + 5);
    else if (k.left)  syncBpm(e.bpm() - 5);
    else if (k.has('s')) { g.sigIndex = (g.sigIndex + 1) % TIME_SIGS_COUNT; e.setSigFromSettings(); }
    else if (k.has('t')) tap();
    else if (k.has('-')) syncBeats(e.sig().beats - 1);
    else if (k.has('=')) syncBeats(e.sig().beats + 1);
    else if (int n = digitKey(k)) syncBeats(n);
    dirty = true;
  }

  void tick() override { e.tick(true); if (e.redraw) { dirty = true; e.redraw = false; } }

  void draw() override {
    ui::clear();
    ui::header(e.running ? "Metronome  [RUN]" : "Metronome  [stop]");
    metro::drawFace(e);
    ui::footer("SPACE run  1-9 -/= beats  s:sig  t:tap");
  }
};
