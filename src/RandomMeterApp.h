#pragma once
#include "App.h"
#include "MetronomeEngine.h"
#include <esp_random.h>

// ---------------------------------------------------------------------------
// Random Meter — randomly switches time signature every phrase, to train your
// feel for changing meters.
//   UP / DOWN    : BPM +/- 5     LEFT / RIGHT : phrase length (bars)
//   SPACE        : start / stop   ` : back
// ---------------------------------------------------------------------------
class RandomMeterApp : public App {
  MetronomeEngine e;
  int  phraseBars   = 2;
  long nextChange   = 0;
  int  prevSig      = -1;

  void pickSig() {
    int i;
    do { i = (int)(esp_random() % TIME_SIGS_COUNT); }
    while (TIME_SIGS_COUNT > 1 && i == prevSig);   // avoid repeating the same one
    prevSig = i;
    e.setSigIndex(i);
  }

  void begin() {
    e.setBpm(g.bpm); prevSig = -1; e.start(); pickSig(); nextChange = phraseBars;
  }

public:
  const char* title() const override { return "Random Meter"; }

  void onEnter() override { e.stop(); e.setBpm(g.bpm); e.setSigFromSettings(); dirty = true; }

  void handle(const KeyEvent& k) override {
    if      (k.space || k.enter) e.running ? e.stop() : begin();
    else if (k.up)    { e.setBpm(e.bpm() + 5); g.bpm = e.bpm(); }
    else if (k.down)  { e.setBpm(e.bpm() - 5); g.bpm = e.bpm(); }
    else if (k.right) phraseBars = constrain(phraseBars + 1, 1, 8);
    else if (k.left)  phraseBars = constrain(phraseBars - 1, 1, 8);
    dirty = true;
  }

  void tick() override {
    e.tick(true);
    if (e.lastBeat == 0 && e.bars >= nextChange) { pickSig(); nextChange = e.bars + phraseBars; }
    if (e.redraw) { dirty = true; e.redraw = false; }
  }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();
    ui::header(e.running ? "Random Meter [RUN]" : "Random Meter");

    // big current signature
    d.setTextDatum(middle_center);
    d.setTextColor(ui::COL_ACCENT, ui::COL_BG);
    d.setFont(&fonts::Font4); d.setTextSize(2);
    d.drawString(e.sig().name, 120, 46);
    d.setTextSize(1);

    d.setFont(&fonts::Font2); d.setTextColor(ui::COL_DIM, ui::COL_BG);
    char sub[40];
    if (e.running) {
      long left = nextChange - e.bars;
      snprintf(sub, sizeof(sub), "%d BPM   change in %ld", e.bpm(), left < 1 ? 1 : left);
    } else {
      snprintf(sub, sizeof(sub), "%d BPM   phrase %d bars", e.bpm(), phraseBars);
    }
    d.drawString(sub, 120, 82);

    metro::drawDots(e, 108);
    ui::footer("SPACE run  UP/DN bpm  L/R phrase  `:back");
  }
};
