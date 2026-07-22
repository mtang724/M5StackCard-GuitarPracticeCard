#pragma once
#include "App.h"
#include "MetronomeEngine.h"

// ---------------------------------------------------------------------------
// Chord Progression — shows a looping chord chart that advances with the beat,
// with an open-chord fingering diagram for each chord.
//   SPACE : start / stop     UP / DOWN : BPM
//   LEFT / RIGHT : progression    b : bars per chord (1/2/4)     ` : back
// ---------------------------------------------------------------------------

// A chord as fret positions per string, low E (6th) -> high E (1st).
// -1 = muted (x), 0 = open (o), >0 = fret number.
struct Chord { const char* name; int8_t fr[6]; };

enum { CH_G, CH_C, CH_D, CH_Em, CH_Am, CH_E, CH_A, CH_Dm, CH_F };
static const Chord CHORDS[] = {
  { "G",  {  3, 2, 0, 0, 0, 3 } },
  { "C",  { -1, 3, 2, 0, 1, 0 } },
  { "D",  { -1,-1, 0, 2, 3, 2 } },
  { "Em", {  0, 2, 2, 0, 0, 0 } },
  { "Am", { -1, 0, 2, 2, 1, 0 } },
  { "E",  {  0, 2, 2, 1, 0, 0 } },
  { "A",  { -1, 0, 2, 2, 2, 0 } },
  { "Dm", { -1,-1, 0, 2, 3, 1 } },
  { "F",  {  1, 3, 3, 2, 1, 1 } },
};

struct Progression { const char* name; const uint8_t* seq; uint8_t len; };
static const uint8_t PROG1[] = { CH_G, CH_D, CH_Em, CH_C };
static const uint8_t PROG2[] = { CH_C, CH_G, CH_Am, CH_F };
static const uint8_t PROG3[] = { CH_Em, CH_C, CH_G, CH_D };
static const uint8_t PROG4[] = { CH_Am, CH_F, CH_C, CH_G };
static const uint8_t PROG5[] = { CH_A,CH_A,CH_A,CH_A, CH_D,CH_D,CH_A,CH_A, CH_E,CH_D,CH_A,CH_E };
static const Progression PROGS[] = {
  { "G D Em C",     PROG1, 4  },
  { "C G Am F",     PROG2, 4  },
  { "Em C G D",     PROG3, 4  },
  { "Am F C G",     PROG4, 4  },
  { "12-Bar Blues", PROG5, 12 },
};
static const int PROGS_COUNT = sizeof(PROGS) / sizeof(PROGS[0]);

class ChordProgressionApp : public App {
  MetronomeEngine e;
  int  progIdx      = 0;
  int  barsPerChord = 2;
  int  curStep      = 0;

  const Progression& prog() const { return PROGS[progIdx]; }

  void begin() { e.setBpm(g.bpm); e.setSigIndex(0); curStep = 0; e.start(); }

  // Draw a chord fingering diagram inside the given box.
  void drawDiagram(const Chord& c, int x, int y, int w, int h) {
    auto& d = ui::canvas;
    const int strings = 6, frets = 5;
    int maxf = 0, minf = 99;
    for (int i = 0; i < 6; i++) if (c.fr[i] > 0) { maxf = max(maxf, (int)c.fr[i]); minf = min(minf, (int)c.fr[i]); }
    int  base    = (maxf > 4) ? minf : 1;
    bool openPos = (base == 1);

    int gx = x, gy = y + 10, gw = w, gh = h - 14;
    int colStep = gw / (strings - 1);
    int rowStep = gh / frets;

    // frets (nut is thick when in open position)
    for (int f = 0; f <= frets; f++) {
      int yy = gy + f * rowStep;
      d.fillRect(gx, yy, gw + 1, (openPos && f == 0) ? 3 : 1, ui::COL_DIM);
    }
    // strings
    for (int s = 0; s < strings; s++) d.drawFastVLine(gx + s * colStep, gy, gh, ui::COL_DIM);

    // open (o) / muted (x) markers above the nut
    d.setFont(&fonts::Font0); d.setTextDatum(bottom_center);
    for (int s = 0; s < strings; s++) {
      int xx = gx + s * colStep;
      if (c.fr[s] < 0)      { d.setTextColor(ui::COL_DIM, ui::COL_BG); d.drawString("x", xx, gy - 1); }
      else if (c.fr[s] == 0) d.drawCircle(xx, gy - 4, 2, ui::COL_STRONG);
    }
    // finger dots
    for (int s = 0; s < strings; s++) {
      if (c.fr[s] > 0) {
        int rel = c.fr[s] - base + 1;
        if (rel >= 1 && rel <= frets)
          d.fillCircle(gx + s * colStep, gy + rel * rowStep - rowStep / 2, 3, ui::COL_ACCENT);
      }
    }
    // base-fret number when not showing the nut
    if (!openPos) {
      d.setFont(&fonts::Font0); d.setTextColor(ui::COL_DIM, ui::COL_BG); d.setTextDatum(middle_left);
      char b[6]; snprintf(b, sizeof(b), "%d", base);
      d.drawString(b, gx + gw + 3, gy + rowStep / 2);
    }
  }

public:
  const char* title() const override { return "Chord Progression"; }

  void onEnter() override { e.stop(); e.setBpm(g.bpm); curStep = 0; dirty = true; }

  void handle(const KeyEvent& k) override {
    if      (k.space || k.enter) e.running ? e.stop() : begin();
    else if (k.up)    { e.setBpm(e.bpm() + 5); g.bpm = e.bpm(); }
    else if (k.down)  { e.setBpm(e.bpm() - 5); g.bpm = e.bpm(); }
    else if (k.right) { progIdx = (progIdx + 1) % PROGS_COUNT; curStep = 0; if (e.running) begin(); }
    else if (k.left)  { progIdx = (progIdx + PROGS_COUNT - 1) % PROGS_COUNT; curStep = 0; if (e.running) begin(); }
    else if (k.has('b')) { barsPerChord = barsPerChord == 1 ? 2 : (barsPerChord == 2 ? 4 : 1); }
    dirty = true;
  }

  void tick() override {
    e.tick(true);
    if (e.running) {
      int step = (int)((e.bars / barsPerChord) % prog().len);
      if (step != curStep) { curStep = step; dirty = true; }
    }
    if (e.redraw) { dirty = true; e.redraw = false; }
  }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();
    char hdr[40]; snprintf(hdr, sizeof(hdr), "%s%s", prog().name, e.running ? "  [RUN]" : "");
    ui::header(hdr);

    const Chord& cur = CHORDS[prog().seq[curStep]];

    // left: current chord name (big) + info
    d.setTextDatum(middle_center);
    d.setFont(&fonts::Font4); d.setTextSize(2);
    d.setTextColor(ui::COL_ACCENT, ui::COL_BG);
    d.drawString(cur.name, 62, 48);
    d.setTextSize(1);

    d.setFont(&fonts::Font2); d.setTextColor(ui::COL_FG, ui::COL_BG);
    const Chord& nxt = CHORDS[prog().seq[(curStep + 1) % prog().len]];
    char nb[24]; snprintf(nb, sizeof(nb), "next  %s", nxt.name);
    d.drawString(nb, 62, 82);

    d.setFont(&fonts::Font0); d.setTextColor(ui::COL_DIM, ui::COL_BG);
    char ib[32]; snprintf(ib, sizeof(ib), "chord %d/%d   %d bar(s)", curStep + 1, prog().len, barsPerChord);
    d.drawString(ib, 62, 100);

    // right: fingering diagram
    drawDiagram(cur, 132, 24, 90, 84);

    // bottom: whole progression with the current chord highlighted
    d.setFont(&fonts::Font0); d.setTextDatum(top_left);
    int n = prog().len, tx = 6, ty = 113;
    for (int i = 0; i < n; i++) {
      const char* nm = CHORDS[prog().seq[i]].name;
      int w = d.textWidth(nm);
      if (i == curStep) { d.setTextColor(ui::COL_BG, ui::COL_ACCENT); d.fillRect(tx - 1, ty - 1, w + 2, 10, ui::COL_ACCENT); }
      else              { d.setTextColor(ui::COL_DIM, ui::COL_BG); }
      d.drawString(nm, tx, ty);
      tx += w + 6;
    }

    ui::footer("SPACE run  UP/DN bpm  L/R prog  b:bars");
  }
};
