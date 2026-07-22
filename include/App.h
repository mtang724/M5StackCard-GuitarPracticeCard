#pragma once
//
// Core framework for the Guitar Practice Card.
//
// Everything on screen is an "App": the menu is an App, the metronome is an App,
// each future feature (tuner, trainers, chord charts...) is an App. To add a new
// feature you write one class deriving from App and register it in buildApps()
// in main.cpp. That's the whole extension model.
//
#include <M5Cardputer.h>
#include <vector>

// ---------------------------------------------------------------------------
// Input: one decoded key event per loop (press edges only).
// Cardputer arrow legends live on ; . , /  and Esc is the backtick key.
// ---------------------------------------------------------------------------
struct KeyEvent {
  bool any   = false;
  bool enter = false;
  bool back  = false;  // ` (Esc)
  bool space = false;
  bool del   = false;
  bool up    = false;  // ;
  bool down  = false;  // .
  bool left  = false;  // ,
  bool right = false;  // /
  std::vector<char> chars;               // printable keys pressed this event
  bool has(char c) const {               // did the user press character c?
    for (char x : chars) if (x == c) return true;
    return false;
  }
};

// ---------------------------------------------------------------------------
// Settings shared across apps (e.g. BPM set in the metronome is reused by the
// trainers). Kept tiny and global on purpose for an embedded app.
// ---------------------------------------------------------------------------
struct Settings {
  int     bpm      = 100;   // 30..300
  int     sigIndex = 0;     // index into TIME_SIGS[]
  uint8_t volume   = 140;   // 0..255
};
extern Settings g;

// A time signature: how many beats per bar, and which beats get the strong
// (accented, higher-pitched) click. accentMask bit i set => beat i is strong.
struct TimeSig { const char* name; uint8_t beats; uint32_t accentMask; };
extern const TimeSig TIME_SIGS[];
extern const int     TIME_SIGS_COUNT;

// ---------------------------------------------------------------------------
// App base class.
// ---------------------------------------------------------------------------
class App {
public:
  bool dirty = true;                       // set true to request a redraw
  virtual const char* title() const = 0;
  virtual void onEnter() { dirty = true; } // becomes the active screen
  virtual void onExit()  {}                // leaving this screen
  virtual void handle(const KeyEvent&) {}  // react to a key press
  virtual void tick()    {}                // called every loop (timing/audio)
  virtual void draw()    = 0;              // repaint when dirty
  virtual ~App() {}
};

// The registered feature apps (filled in main.cpp). The menu iterates this.
extern std::vector<App*> APPS;

// Navigation (defined in main.cpp).
void openApp(App* a);
void openMenu();

// ---------------------------------------------------------------------------
// Small shared drawing helpers. Screen is 240 x 135 in rotation 1.
// ---------------------------------------------------------------------------
namespace ui {
  extern uint16_t COL_BG, COL_FG, COL_ACCENT, COL_DIM, COL_STRONG;
  // Full-screen back buffer. ALL drawing goes here, then main pushes it to the
  // display in one blit — this is what makes the UI flicker-free.
  extern M5Canvas canvas;
  void clear();
  void header(const char* title);
  void footer(const char* hint);
  // word-wrapped paragraph using the current font; returns y after last line
  int  paragraph(const char* text, int x, int y, int w, int lineH);
  // a labelled value box, highlighted when selected (for editable settings)
  void field(int x, int y, const char* label, const char* value, bool selected);
}
