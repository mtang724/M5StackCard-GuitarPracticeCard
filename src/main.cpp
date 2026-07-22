//
// Guitar Practice Card — M5Stack Cardputer
// A standalone app you install/boot from bmorcelli/Launcher.
//
#include "App.h"
#include "MenuApp.h"
#include "MetronomeApp.h"
#include "TempoTrainerApp.h"
#include "RandomMeterApp.h"
#include "DropoutTrainerApp.h"
#include "TunerApp.h"
#include "ChordProgressionApp.h"
#include "RecordCheckApp.h"

// --------------------------------------------------------------------------
// Global state
// --------------------------------------------------------------------------
Settings g;

const TimeSig TIME_SIGS[] = {
  { "4/4", 4, 0b0001   },
  { "3/4", 3, 0b001    },
  { "2/4", 2, 0b01     },
  { "6/8", 6, 0b001001 },   // strong on beats 1 and 4
};
const int TIME_SIGS_COUNT = sizeof(TIME_SIGS) / sizeof(TIME_SIGS[0]);

std::vector<App*> APPS;

// --------------------------------------------------------------------------
// The apps. Metronome is real; the rest are roadmap stubs for now.
// To add a feature: create an App subclass and push it into APPS below.
// --------------------------------------------------------------------------
static MetronomeApp     metronomeApp;
static TempoTrainerApp  trainerApp;
static RandomMeterApp   randomApp;
static DropoutTrainerApp dropoutApp;
static TunerApp             tunerApp;
static ChordProgressionApp  chordApp;
static RecordCheckApp       recordApp;

static MenuApp menu;

void buildApps() {
  APPS = {
    &metronomeApp, &trainerApp, &randomApp, &dropoutApp,
    &tunerApp, &chordApp, &recordApp,
  };
}

// --------------------------------------------------------------------------
// Navigation
// --------------------------------------------------------------------------
static App* current = nullptr;

void openApp(App* a) {
  if (current) current->onExit();
  current = a;
  a->onEnter();
}
void openMenu() { openApp(&menu); }

// --------------------------------------------------------------------------
// UI helpers
// --------------------------------------------------------------------------
namespace ui {
  uint16_t COL_BG     = 0x0000;   // black
  uint16_t COL_FG     = 0xFFFF;   // white
  uint16_t COL_ACCENT = 0xFD20;   // orange
  uint16_t COL_DIM    = 0x4208;   // grey
  uint16_t COL_STRONG = 0x07FF;   // cyan

  M5Canvas canvas(&M5Cardputer.Display);

  void clear() { canvas.fillScreen(COL_BG); }

  void header(const char* title) {
    auto& d = canvas;
    d.fillRect(0, 0, 240, 20, COL_ACCENT);
    d.setFont(&fonts::Font2);
    d.setTextSize(1);
    d.setTextDatum(middle_left);
    d.setTextColor(COL_BG, COL_ACCENT);
    d.drawString(title, 6, 10);
  }

  void footer(const char* hint) {
    auto& d = canvas;
    d.setFont(&fonts::Font0);
    d.setTextSize(1);
    d.setTextDatum(bottom_left);
    d.setTextColor(COL_DIM, COL_BG);
    d.drawString(hint, 4, 134);
  }

  // Greedy word-wrap using the current font/size.
  int paragraph(const char* text, int x, int y, int w, int lineH) {
    auto& d = canvas;
    d.setTextDatum(top_left);
    String line, word;
    const char* p = text;
    auto flush = [&](const String& s) {
      if (s.length()) { d.drawString(s, x, y); y += lineH; }
    };
    while (true) {
      char c = *p++;
      bool end = (c == '\0');
      if (c == ' ' || end) {
        String cand = line.length() ? line + " " + word : word;
        if (d.textWidth(cand) > w && line.length()) { flush(line); line = word; }
        else line = cand;
        word = "";
        if (end) break;
      } else {
        word += c;
      }
    }
    flush(line);
    return y;
  }

  void field(int x, int y, const char* label, const char* value, bool selected) {
    auto& d = canvas;
    const int w = 56, h = 26;
    if (selected) { d.fillRoundRect(x, y, w, h, 4, COL_ACCENT); d.setTextColor(COL_BG, COL_ACCENT); }
    else          { d.drawRoundRect(x, y, w, h, 4, COL_DIM);    d.setTextColor(COL_DIM, COL_BG); }
    d.setTextDatum(top_center);
    d.setFont(&fonts::Font0); d.drawString(label, x + w / 2, y + 3);
    if (!selected) d.setTextColor(COL_FG, COL_BG);
    d.setFont(&fonts::Font2); d.drawString(value, x + w / 2, y + 11);
  }
}

// --------------------------------------------------------------------------
// Input: decode one press event per loop.
// --------------------------------------------------------------------------
static KeyEvent pollKey() {
  KeyEvent e;
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    auto st = M5Cardputer.Keyboard.keysState();
    e.any   = true;
    e.enter = st.enter;
    e.del   = st.del;
    for (char c : st.word) {
      if (c == ' ') { e.space = true; continue; }
      switch (c) {
        case ';': e.up    = true; break;
        case '.': e.down  = true; break;
        case ',': e.left  = true; break;
        case '/': e.right = true; break;
        case '`': e.back  = true; break;
        default:  e.chars.push_back(c); break;
      }
    }
  }
  return e;
}

// --------------------------------------------------------------------------
// Arduino entry points
// --------------------------------------------------------------------------
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(140);
  M5Cardputer.Speaker.setVolume(g.volume);

  ui::canvas.setColorDepth(16);
  ui::canvas.createSprite(240, 135);   // full-screen back buffer (~63 KB)

  buildApps();
  openMenu();
}

void loop() {
  M5Cardputer.update();
  KeyEvent k = pollKey();

  if (current) {
    current->tick();
    if (k.any) {
      if (k.back && current != &menu) openMenu();
      else                            current->handle(k);
    }
    if (current->dirty) {
      current->draw();
      ui::canvas.pushSprite(0, 0);   // one flicker-free blit to the display
      current->dirty = false;
    }
  }

  delay(2);  // ~2ms loop — plenty of timing resolution up to 300 BPM
}
