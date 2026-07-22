#pragma once
#include "App.h"

// The home screen: a scrollable list of every registered feature.
class MenuApp : public App {
  int sel = 0;
public:
  const char* title() const override { return "Guitar Practice Card"; }

  void handle(const KeyEvent& k) override {
    int n = (int)APPS.size();
    if (n == 0) return;
    if      (k.up)    { sel = (sel - 1 + n) % n; dirty = true; }
    else if (k.down)  { sel = (sel + 1) % n;     dirty = true; }
    else if (k.enter) { openApp(APPS[sel]); }
  }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();
    ui::header("Guitar Practice Card");

    d.setFont(&fonts::Font2);
    d.setTextSize(1);
    d.setTextDatum(top_left);

    const int y0 = 24, rowH = 13;
    const int maxRows = (135 - y0 - 8) / rowH;
    int n = (int)APPS.size();
    int top = sel - maxRows / 2;
    top = constrain(top, 0, (n - maxRows) > 0 ? (n - maxRows) : 0);

    for (int i = 0; i < maxRows && top + i < n; i++) {
      int idx = top + i;
      int y = y0 + i * rowH;
      if (idx == sel) {
        d.fillRect(0, y - 1, 240, rowH, ui::COL_ACCENT);
        d.setTextColor(ui::COL_BG, ui::COL_ACCENT);
      } else {
        d.setTextColor(ui::COL_FG, ui::COL_BG);
      }
      d.drawString(APPS[idx]->title(), 12, y);
    }

    ui::footer("UP/DN move   ENTER open");
  }
};
