#pragma once
#include "App.h"
#include "MetronomeEngine.h"

// ---------------------------------------------------------------------------
// Dropout Trainer — the click plays for `Play` bars, then goes silent for
// `Mute` bars. Keep the tempo on your own; see if you drifted when it returns.
//   LEFT / RIGHT : pick a field    UP / DOWN : change it
//   SPACE        : start / stop     ` : back
// ---------------------------------------------------------------------------
class DropoutTrainerApp : public App {
  MetronomeEngine e;
  int  playBars = 2, muteBars = 2;
  int  field = 0;            // 0 BPM, 1 Play, 2 Mute
  bool muted = false;

  void adjust(int dir) {
    switch (field) {
      case 0: e.setBpm(e.bpm() + dir * 5); g.bpm = e.bpm(); break;
      case 1: playBars = constrain(playBars + dir, 1, 8); break;
      case 2: muteBars = constrain(muteBars + dir, 1, 8); break;
    }
  }

  void begin() { e.setBpm(g.bpm); e.setSigFromSettings(); muted = false; e.start(); }

public:
  const char* title() const override { return "Dropout Trainer"; }

  void onEnter() override { e.stop(); e.setBpm(g.bpm); e.setSigFromSettings(); field = 0; muted = false; dirty = true; }

  void handle(const KeyEvent& k) override {
    if      (k.space || k.enter) e.running ? e.stop() : begin();
    else if (k.left)  field = (field + 2) % 3;
    else if (k.right) field = (field + 1) % 3;
    else if (k.up)    adjust(+1);
    else if (k.down)  adjust(-1);
    dirty = true;
  }

  void tick() override {
    int cycle = playBars + muteBars;
    int pos = (int)(e.bars % cycle);     // which bar of the cycle we're in
    bool nowMuted = pos >= playBars;
    if (nowMuted != muted) { muted = nowMuted; dirty = true; }
    e.tick(!muted);
    if (e.redraw) { dirty = true; e.redraw = false; }
  }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();
    ui::header(e.running ? "Dropout Trainer [RUN]" : "Dropout Trainer");

    // big state word
    d.setTextDatum(middle_center);
    d.setFont(&fonts::Font4);
    if (!e.running)   { d.setTextColor(ui::COL_DIM, ui::COL_BG);    d.drawString("READY", 120, 36); }
    else if (muted)   { d.setTextColor(ui::COL_DIM, ui::COL_BG);    d.drawString("MUTED", 120, 36); }
    else              { d.setTextColor(ui::COL_ACCENT, ui::COL_BG); d.drawString("PLAY",  120, 36); }

    d.setFont(&fonts::Font2); d.setTextColor(ui::COL_STRONG, ui::COL_BG);
    if (e.running) {
      int cycle = playBars + muteBars, pos = (int)(e.bars % cycle);
      char sub[40]; snprintf(sub, sizeof(sub), "%d BPM   bar %d/%d", e.bpm(), pos + 1, cycle);
      d.drawString(sub, 120, 56);
    } else {
      d.drawString("keep time when it drops out", 120, 56);
    }

    metro::drawDots(e, 74, /*dim=*/muted);

    char v0[8], v1[8], v2[8];
    snprintf(v0, 8, "%d", e.bpm()); snprintf(v1, 8, "%d", playBars); snprintf(v2, 8, "%d", muteBars);
    ui::field(35,  90, "BPM",  v0, field == 0);
    ui::field(93,  90, "Play", v1, field == 1);
    ui::field(151, 90, "Mute", v2, field == 2);

    ui::footer("L/R field  UP/DN change  SPACE run  `:back");
  }
};
