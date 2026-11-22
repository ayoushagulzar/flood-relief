// widgets.h - tiny immediate-mode widgets (our own, no GUI library).
//
// Immediate mode: a widget is just a function. You call it every frame with
// its rectangle; it draws itself AND returns whether it was clicked/changed.
// No widget objects, no callbacks: the caller keeps the state (an int, a
// bool) and passes it in by reference.
//
//   if (ui::button(r, "Next")) manager.processNext();
#pragma once

#include <string>

#include "raylib.h"

#include "gui/text.h"

namespace ui {

bool hovered(Rectangle r);   // mouse inside r
bool clickedIn(Rectangle r); // left button released inside r this frame

// Text button. primary = filled with the accent colour.
bool button(Rectangle r, const std::string& label, bool primary = false, bool enabled = true);
// Square tool button for the rail (with a 2 px accent bar when active).
bool iconButton(Rectangle r, Icon icon, bool active, bool enabled = true);
// Tab label with an accent underline when active.
bool tab(Rectangle r, const std::string& label, bool active);
// "‹  Water  ›": click the left/right half to step through options.
bool cycler(Rectangle r, const char* const* options, int count, int& index);
// "−  300  +": step value by `step`, clamped to [lo, hi].
bool stepper(Rectangle r, int& value, int step, int lo, int hi);
// On/off box with a label.
bool toggle(Rectangle r, const std::string& label, bool& value);
// Small dark label next to the mouse (drawn last, on top).
void tooltip(const std::string& s, Vector2 at);

}  // namespace ui
