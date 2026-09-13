#pragma once
#include "App.h"

// ---------------------------------------------------------------------------
// Reusable metronome engine: beat scheduling + click sound. The Metronome app
// and all three trainers drive one of these. Call tick() every loop; it fires
// clicks on time and reports what happened via the public fields below.
// ---------------------------------------------------------------------------
class MetronomeEngine {
  int            bpm_  = 100;
  const TimeSig* sig_  = &TIME_SIGS[0];
  TimeSig        custom_;          // backing store for setBeats()
  char           customName_[6];   // "12/4"
  uint32_t       nextBeat = 0;

  void useSig(const TimeSig* s) {
    sig_ = s;
    if (curBeat >= s->beats) curBeat = 0;   // bar got shorter mid-run
  }

public:
  bool     running    = false;
  int      curBeat    = 0;    // next beat index to fire
  int      litBeat    = -1;   // beat currently flashing (for the dots)
  uint32_t flashUntil = 0;
  long     bars       = 0;    // completed bars since start()
  int      lastBeat   = -1;   // beat fired on THIS tick, else -1
  bool     redraw     = false;// set when the screen should refresh

  void setBpm(int b) { bpm_ = constrain(b, 30, 300); }
  int  bpm() const   { return bpm_; }
  void setSigIndex(int i) {
    int n = TIME_SIGS_COUNT;
    useSig(&TIME_SIGS[((i % n) + n) % n]);
  }
  // A custom n/4 bar, accent on beat 1.
  void setBeats(int n) {
    n = constrain(n, 1, MAX_BEATS);
    snprintf(customName_, sizeof(customName_), "%d/4", n);
    custom_ = { customName_, (uint8_t)n, 1u };
    useSig(&custom_);
  }
  // Whatever was last picked in the Metronome: a preset or a custom bar.
  void setSigFromSettings() {
    if (g.sigIndex < 0) setBeats(g.beats);
    else                setSigIndex(g.sigIndex);
  }
  const TimeSig& sig() const { return *sig_; }
  uint32_t intervalMs() const { return 60000UL / (bpm_ < 1 ? 1 : bpm_); }

  void start() {
    running = true; curBeat = 0; bars = 0; lastBeat = -1;
    nextBeat = millis(); redraw = true;
  }
  void stop() {
    running = false; M5Cardputer.Speaker.stop();
    litBeat = -1; lastBeat = -1; redraw = true;
  }

  // audible=false plays a silent beat (used by the Dropout Trainer).
  void tick(bool audible) {
    lastBeat = -1;
    if (!running) return;
    uint32_t now = millis();
    if ((int32_t)(now - nextBeat) >= 0) {
      int b = curBeat;
      bool strong = sig_->accentMask & (1u << b);
      if (audible) M5Cardputer.Speaker.tone(strong ? 2093.0f : 1046.5f, strong ? 45 : 28);
      litBeat = b; flashUntil = now + 90; lastBeat = b;
      curBeat++;
      if (curBeat >= sig_->beats) { curBeat = 0; bars++; }
      nextBeat += intervalMs();
      if ((int32_t)(now - nextBeat) > (int32_t)intervalMs()) nextBeat = now + intervalMs();
      redraw = true;
    }
    if (litBeat >= 0 && now > flashUntil) { litBeat = -1; redraw = true; }
  }
};

namespace metro {
  // Just the beat dots for signature/engine e, centred on row y.
  // dim=true draws them greyed out (used when the click is muted).
  inline void drawDots(MetronomeEngine& e, int y, bool dim = false) {
    auto& d = ui::canvas;
    int beats = e.sig().beats;
    int gap = beats > 6 ? 18 : 22;
    int x0  = 120 - (beats - 1) * gap / 2;
    bool flashing = e.running && !dim && (millis() < e.flashUntil);
    for (int i = 0; i < beats; i++) {
      bool strong = e.sig().accentMask & (1u << i);
      bool on = flashing && (i == e.litBeat);
      int r = strong ? 7 : 5;
      uint16_t ring = dim ? ui::COL_DIM : (strong ? ui::COL_STRONG : ui::COL_DIM);
      if (on) d.fillCircle(x0 + i * gap, y, r + 1, ui::COL_ACCENT);
      else    d.drawCircle(x0 + i * gap, y, r, ring);
    }
  }

  // Full metronome face: big BPM number, time-signature name, and beat dots.
  inline void drawFace(MetronomeEngine& e, int dotsY = 110) {
    auto& d = ui::canvas;
    d.setTextDatum(middle_center);
    d.setTextColor(ui::COL_FG, ui::COL_BG);
    d.setFont(&fonts::Font7); d.setTextSize(1);
    char buf[8]; snprintf(buf, sizeof(buf), "%d", e.bpm());
    d.drawString(buf, 120, 48);
    d.setFont(&fonts::Font2);
    d.setTextColor(ui::COL_DIM, ui::COL_BG);
    d.drawString("BPM", 120, 74);
    d.setTextColor(ui::COL_STRONG, ui::COL_BG);
    d.drawString(e.sig().name, 214, 44);
    drawDots(e, dotsY);
  }
}
