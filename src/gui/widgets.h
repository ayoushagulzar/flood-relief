// this file declares my own small widgets (button, stepper, toggle ...), no GUI library.
// immediate mode -> a widget is just a function called every frame,
// it draws itself and returns true when it is clicked
#pragma once // this prevents multiple inclusions of the header file.

#include <string>

#include "raylib.h"

#include "gui/text.h"

namespace ui {

bool hovered(Rectangle r); // mouse is inside r
bool clickedIn(Rectangle r); // mouse released inside r

// primary = dark main button, active = tool that is on
bool button(Rectangle r, const std::string& label, bool primary = false, bool enabled = true, bool active = false);
bool iconButton(Rectangle r, Icon icon, const std::string& label, bool primary = false, bool enabled = true,
                bool active = false);
// sidebar row
bool navItem(Rectangle r, Icon icon, const std::string& label, bool active);
// "<  Water  >" pick from a list
bool cycler(Rectangle r, const char* const* options, int count, int& index);
// "-  300  +" number between lo and hi
bool stepper(Rectangle r, int& value, int step, int lo, int hi);
// check box
bool toggle(Rectangle r, const std::string& label, bool& value);
// small label next to the mouse
void tooltip(const std::string& s, Vector2 at);

}
