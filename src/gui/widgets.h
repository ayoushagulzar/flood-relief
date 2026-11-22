// widgets.h - tiny immediate-mode widgets (our own, no GUI library).
//
// Immediate mode: a widget is just a function. You call it every frame with
// its rectangle; it draws itself AND returns whether it was clicked/changed.
// No widget objects, no callbacks: the caller keeps the state (an int, a
// bool) and passes it in by reference.
//
//   if (ui::button(r, "Next request", true)) manager.processNext();
#pragma once

#include <string>

#include "raylib.h"

#include "gui/text.h"

namespace ui {

bool hovered(Rectangle r);   // mouse inside r
bool clickedIn(Rectangle r); // left button released inside r this frame

// Rounded text button.
//   primary = dark burgundy fill (the main action)
//   active  = a tool that is switched on (tinted fill + burgundy border)
bool button(Rectangle r, const std::string& label, bool primary = false, bool enabled = true, bool active = false);
// Same, with an icon left of the label.
bool iconButton(Rectangle r, Icon icon, const std::string& label, bool primary = false, bool enabled = true,
                bool active = false);
// Sidebar row: icon + label, highlighted when active.
bool navItem(Rectangle r, Icon icon, const std::string& label, bool active);
// "‹  Water  ›": click the left/right half to step through options.
bool cycler(Rectangle r, const char* const* options, int count, int& index);
// "−  300  +": step value by `step`, clamped to [lo, hi].
bool stepper(Rectangle r, int& value, int step, int lo, int hi);
// On/off check box with a label.
bool toggle(Rectangle r, const std::string& label, bool& value);
// Small dark label next to the mouse (drawn last, on top).
void tooltip(const std::string& s, Vector2 at);

}  // namespace ui
