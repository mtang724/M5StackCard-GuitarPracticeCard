#pragma once
#include "App.h"

// A placeholder screen for a feature that is on the roadmap but not built yet.
// Each real feature will replace one of these with its own App subclass — this
// is the template to copy when you add the tuner, trainers, chord charts, etc.
class StubApp : public App {
  const char* t_;
  const char* desc_;
public:
  StubApp(const char* title, const char* desc) : t_(title), desc_(desc) {}
  const char* title() const override { return t_; }

  void draw() override {
    auto& d = ui::canvas;
    ui::clear();
    ui::header(t_);
    d.setFont(&fonts::Font2);
    d.setTextSize(1);
    d.setTextColor(ui::COL_FG, ui::COL_BG);
    ui::paragraph(desc_, 8, 30, 224, 16);
    d.setTextColor(ui::COL_STRONG, ui::COL_BG);
    d.setTextDatum(top_left);
    d.drawString("Planned - not built yet", 8, 110);
    ui::footer("`:back");
  }
};
