#pragma once
#include "App.h"
#include "MetronomeEngine.h"

// ---------------------------------------------------------------------------
// Tempo Trainer — start slow, ramp up automatically.
// Every `Every` bars the BPM increases by `Step` until it reaches `Target`.
//   LEFT / RIGHT : pick a field    UP / DOWN : change it
//   SPACE        : start / stop     ` : back
// ---------------------------------------------------------------------------
class TempoTrainerApp : public App {
  MetronomeEngine e;
  int  startBpm = 80, targetBpm = 140, step = 5, everyBars = 4;
  int  field = 0;            // 0 Start, 1 Target, 2 Step, 3 Every
  long lastApplied = -1;

  void adjust(int dir) {
    switch (field) {
      case 0: startBpm  = constrain(startBpm  + dir * 5, 30, 300); break;
      case 1: targetBpm = constrain(targetBpm + dir * 5, 30, 300); break;
      case 2: step      = constrain(step      + dir,      1,  30); break;
      case 3: everyBars = constrain(everyBars + dir,      1,  16); break;
    }
  }

  void begin() {
    if (targetBpm < startBpm) targetBpm = startBpm;
    e.setBpm(startBpm); e.setSigIndex(g.sigIndex); lastApplied = -1; e.start();
  }

public:
  const char* title() const override { return "Tempo Trainer"; }

  void onEnter() override { e.stop(); startBpm = g.bpm; if (targetBpm < startBpm) targetBpm = startBpm + 40; field = 0; dirty = true; }

  void handle(const KeyEvent& k) override {
    if      (k.space || k.enter) e.running ? e.stop() : begin();
    else if (k.left)  field = (field + 3) % 4;
    else if (k.right) field = (field + 1) % 4;
    else if (k.up)    adjust(+1);
    else if (k.down)  adjust(-1);
    dirty = true;
  }

  void tick() override {
    e.tick(true);
    // at the top of each bar, if we've completed a multiple of everyBars, step up
    if (e.lastBeat == 0 && e.bars > 0 && e.bars != lastApplied && (e.bars % everyBars) == 0) {
      lastApplied = e.bars;
      if (e.bpm() < targetBpm) e.setBpm(min(targetBpm, e.bpm() + step));
    }
    if (e.redraw) { dirty = true; e.redraw = false; }
  }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();
    bool done = e.running && e.bpm() >= targetBpm;
    ui::header(!e.running ? "Tempo Trainer" : (done ? "Tempo Trainer [TARGET]" : "Tempo Trainer [RUN]"));

    // live BPM
    d.setTextDatum(middle_center);
    d.setTextColor(done ? ui::COL_STRONG : ui::COL_FG, ui::COL_BG);
    d.setFont(&fonts::Font7); d.drawString(String(e.bpm()).c_str(), 120, 42);
    d.setFont(&fonts::Font2); d.setTextColor(ui::COL_DIM, ui::COL_BG);
    char sub[32]; snprintf(sub, sizeof(sub), "bar %ld  ->  %d BPM", e.running ? e.bars : 0L, targetBpm);
    d.drawString(sub, 120, 68);

    char v0[8], v1[8], v2[8], v3[8];
    snprintf(v0, 8, "%d", startBpm);  snprintf(v1, 8, "%d", targetBpm);
    snprintf(v2, 8, "+%d", step);     snprintf(v3, 8, "%d", everyBars);
    ui::field(6,   84, "Start",  v0, field == 0);
    ui::field(64,  84, "Target", v1, field == 1);
    ui::field(122, 84, "Step",   v2, field == 2);
    ui::field(180, 84, "Every",  v3, field == 3);

    ui::footer("L/R field  UP/DN change  SPACE run  `:back");
  }
};
