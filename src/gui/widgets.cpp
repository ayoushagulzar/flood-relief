// widgets.cpp - button, icon button, tab, cycler, stepper, toggle (~30 lines each).
#include "gui/widgets.h"

#include "gui/theme.h"

using namespace theme;

namespace ui {

bool hovered(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

// "Released inside" (not "pressed"): the normal desktop feel. You can
// press, change your mind, move away and let go without clicking.
bool clickedIn(Rectangle r) { return hovered(r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT); }

bool button(Rectangle r, const std::string& label, bool primary, bool enabled) {
    bool hot = enabled && hovered(r);
    bool down = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color bg = primary ? kAccent : (down ? kPressed : (hot ? kHover : kPanel));
    if (primary && hot) bg = Color{255, 96, 48, 255};
    if (!enabled) bg = kPanel;
    DrawRectangleRec(r, bg);
    if (!primary) DrawRectangleLinesEx(r, 1, hot ? kMuted : kRule);
    Color fg = !enabled ? kFaint : (primary ? kBackground : kText);
    float w = measureCaps(label, kSmall);
    caps(label, {r.x + (r.width - w) / 2, r.y + (r.height - kSmall) / 2}, kSmall, fg);
    return enabled && clickedIn(r);
}

bool iconButton(Rectangle r, Icon kind, bool active, bool enabled) {
    bool hot = enabled && hovered(r);
    if (active || hot) DrawRectangleRec(r, hot && !active ? kHover : kPressed);
    if (active) DrawRectangleRec({r.x - 6, r.y + 8, 2, r.height - 16}, kAccent);  // bar on the rail edge
    Color c = !enabled ? kFaint : (active ? kAccent : kText);
    icon(kind, {r.x + r.width / 2, r.y + r.height / 2}, 20, c);
    return enabled && clickedIn(r);
}

bool tab(Rectangle r, const std::string& label, bool active) {
    bool hot = hovered(r);
    float w = measureCaps(label, kSmall);
    caps(label, {r.x, r.y + (r.height - kSmall) / 2 - 1}, kSmall, active ? kText : (hot ? kText : kMuted));
    if (active) DrawRectangleRec({r.x, r.y + r.height - 2, w, 2}, kAccent);
    return clickedIn(r);
}

bool cycler(Rectangle r, const char* const* options, int count, int& index) {
    DrawRectangleLinesEx(r, 1, kRule);
    Rectangle left{r.x, r.y, r.height, r.height}, right{r.x + r.width - r.height, r.y, r.height, r.height};
    bool changed = false;
    if (hovered(left)) DrawRectangleRec(left, kHover);
    if (hovered(right)) DrawRectangleRec(right, kHover);
    text("<", {left.x + left.width / 2 - 4, r.y + (r.height - kBody) / 2}, kBody, kText);
    text(">", {right.x + right.width / 2 - 4, r.y + (r.height - kBody) / 2}, kBody, kText);
    if (clickedIn(left)) {
        index = (index + count - 1) % count;
        changed = true;
    }
    if (clickedIn(right)) {
        index = (index + 1) % count;
        changed = true;
    }
    std::string label = options[index];
    float w = measureCaps(label, kSmall);
    caps(label, {r.x + (r.width - w) / 2, r.y + (r.height - kSmall) / 2}, kSmall, kText);
    return changed;
}

bool stepper(Rectangle r, int& value, int step, int lo, int hi) {
    DrawRectangleLinesEx(r, 1, kRule);
    Rectangle minus{r.x, r.y, r.height, r.height}, plus{r.x + r.width - r.height, r.y, r.height, r.height};
    if (hovered(minus)) DrawRectangleRec(minus, kHover);
    if (hovered(plus)) DrawRectangleRec(plus, kHover);
    DrawLineEx({minus.x + 14, minus.y + minus.height / 2}, {minus.x + minus.width - 14, minus.y + minus.height / 2}, 1.6f,
               value > lo ? kText : kFaint);
    Vector2 c{plus.x + plus.width / 2, plus.y + plus.height / 2};
    Color pc = value < hi ? kText : kFaint;
    DrawLineEx({c.x - 7, c.y}, {c.x + 7, c.y}, 1.6f, pc);
    DrawLineEx({c.x, c.y - 7}, {c.x, c.y + 7}, 1.6f, pc);
    int before = value;
    if (clickedIn(minus)) value = value - step < lo ? lo : value - step;
    if (clickedIn(plus)) value = value + step > hi ? hi : value + step;
    std::string s = std::to_string(value);
    float w = measure(s, kBody, Weight::SemiBold);
    text(s, {r.x + (r.width - w) / 2, r.y + (r.height - kBody) / 2}, kBody, kText, Weight::SemiBold);
    return value != before;
}

bool toggle(Rectangle r, const std::string& label, bool& value) {
    Rectangle box{r.x, r.y + (r.height - 16) / 2, 16, 16};
    DrawRectangleLinesEx(box, 1, hovered(r) ? kText : kMuted);
    if (value) DrawRectangleRec({box.x + 4, box.y + 4, 8, 8}, kAccent);
    text(label, {box.x + 26, r.y + (r.height - kSmall) / 2}, kSmall, kText);
    if (clickedIn(r)) {
        value = !value;
        return true;
    }
    return false;
}

void tooltip(const std::string& s, Vector2 at) {
    float w = measure(s, kSmall) + 16;
    Rectangle r{at.x, at.y, w, 26};
    DrawRectangleRec(r, kBackground);
    DrawRectangleLinesEx(r, 1, kRule);
    text(s, {r.x + 8, r.y + 6}, kSmall, kText);
}

}  // namespace ui
