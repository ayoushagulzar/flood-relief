// widgets.cpp - button, nav item, cycler, stepper, toggle, tooltip.
#include "gui/widgets.h"

#include "gui/theme.h"

using namespace theme;

namespace ui {

namespace {
constexpr float kButtonRadius = 9;
}

bool hovered(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

// "Released inside" (not "pressed"): the normal desktop feel. You can
// press, change your mind, move away and let go without clicking.
bool clickedIn(Rectangle r) { return hovered(r) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT); }

// Shared by button() and iconButton(): draws the box, returns the text colour.
static Color buttonBox(Rectangle r, bool primary, bool enabled, bool active) {
    bool hot = enabled && hovered(r);
    bool down = hot && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (primary) {
        Color bg = !enabled ? kAccentSoft : (hot ? kBrand2 : kBrand);
        roundRect(r, kButtonRadius, bg);
        return kBackground;
    }
    Color bg = active ? kAccentTint : (down ? kPressed : (hot ? kHover : kCard));
    roundRect(r, kButtonRadius, bg);
    roundBorder(r, kButtonRadius, active ? kAccent : (hot ? kAccentSoft : kRule), active ? 1.5f : 1.0f);
    if (!enabled) return kFaint;
    return active ? kAccent : kText;
}

bool button(Rectangle r, const std::string& label, bool primary, bool enabled, bool active) {
    Color fg = buttonBox(r, primary, enabled, active);
    Weight w = primary || active ? Weight::SemiBold : Weight::Regular;
    float tw = measure(label, kSmall, w);
    text(label, {r.x + (r.width - tw) / 2, r.y + (r.height - kSmall) / 2 - 1}, kSmall, fg, w);
    return enabled && clickedIn(r);
}

bool iconButton(Rectangle r, Icon kind, const std::string& label, bool primary, bool enabled, bool active) {
    Color fg = buttonBox(r, primary, enabled, active);
    Weight w = primary || active ? Weight::SemiBold : Weight::Regular;
    float tw = measure(label, kSmall, w);
    float x = r.x + (r.width - tw - 24) / 2;
    icon(kind, {x + 7, r.y + r.height / 2}, 14, fg);
    text(label, {x + 24, r.y + (r.height - kSmall) / 2 - 1}, kSmall, fg, w);
    return enabled && clickedIn(r);
}

bool navItem(Rectangle r, Icon kind, const std::string& label, bool active) {
    bool hot = hovered(r);
    if (active) {
        DrawRectangleRec(r, kBrand2);
        DrawRectangleRec({r.x, r.y, 3, r.height}, kAccent);
    } else if (hot) {
        DrawRectangleRec(r, Color{56, 22, 29, 255});
    }
    Color fg = active || hot ? kBackground : kSideText;
    icon(kind, {r.x + 34, r.y + r.height / 2}, 18, fg);
    text(label, {r.x + 58, r.y + (r.height - kBody) / 2 - 1}, kBody, fg, active ? Weight::SemiBold : Weight::Regular);
    return clickedIn(r);
}

bool cycler(Rectangle r, const char* const* options, int count, int& index) {
    roundRect(r, kButtonRadius, kCard);
    roundBorder(r, kButtonRadius, kRule);
    Rectangle left{r.x, r.y, r.height, r.height}, right{r.x + r.width - r.height, r.y, r.height, r.height};
    bool changed = false;
    if (hovered(left)) roundRect(left, kButtonRadius, kHover);
    if (hovered(right)) roundRect(right, kButtonRadius, kHover);
    text("‹", {left.x + left.width / 2 - 3, r.y + (r.height - kTitle) / 2 - 2}, kTitle, kAccent);
    text("›", {right.x + right.width / 2 - 3, r.y + (r.height - kTitle) / 2 - 2}, kTitle, kAccent);
    if (clickedIn(left)) {
        index = (index + count - 1) % count;
        changed = true;
    }
    if (clickedIn(right)) {
        index = (index + 1) % count;
        changed = true;
    }
    std::string label = options[index];
    float w = measure(label, kBody, Weight::SemiBold);
    text(label, {r.x + (r.width - w) / 2, r.y + (r.height - kBody) / 2 - 1}, kBody, kText, Weight::SemiBold);
    return changed;
}

bool stepper(Rectangle r, int& value, int step, int lo, int hi) {
    roundRect(r, kButtonRadius, kCard);
    roundBorder(r, kButtonRadius, kRule);
    Rectangle minus{r.x, r.y, r.height, r.height}, plus{r.x + r.width - r.height, r.y, r.height, r.height};
    if (hovered(minus)) roundRect(minus, kButtonRadius, kHover);
    if (hovered(plus)) roundRect(plus, kButtonRadius, kHover);
    Vector2 m{minus.x + minus.width / 2, minus.y + minus.height / 2};
    DrawLineEx({m.x - 6, m.y}, {m.x + 6, m.y}, 1.8f, value > lo ? kAccent : kFaint);
    Vector2 c{plus.x + plus.width / 2, plus.y + plus.height / 2};
    Color pc = value < hi ? kAccent : kFaint;
    DrawLineEx({c.x - 6, c.y}, {c.x + 6, c.y}, 1.8f, pc);
    DrawLineEx({c.x, c.y - 6}, {c.x, c.y + 6}, 1.8f, pc);
    int before = value;
    if (clickedIn(minus)) value = value - step < lo ? lo : value - step;
    if (clickedIn(plus)) value = value + step > hi ? hi : value + step;
    std::string s = std::to_string(value);
    float w = measure(s, kBody, Weight::SemiBold);
    text(s, {r.x + (r.width - w) / 2, r.y + (r.height - kBody) / 2 - 1}, kBody, kText, Weight::SemiBold);
    return value != before;
}

bool toggle(Rectangle r, const std::string& label, bool& value) {
    Rectangle box{r.x, r.y + (r.height - 20) / 2, 20, 20};
    if (value) {
        roundRect(box, 5, kAccent);
        icon(Icon::Check, {box.x + 10, box.y + 10}, 13, kBackground);
    } else {
        roundRect(box, 5, kCard);
        roundBorder(box, 5, hovered(r) ? kAccent : kFaint);
    }
    text(label, {box.x + 30, r.y + (r.height - kBody) / 2 - 1}, kBody, kText);
    if (clickedIn(r)) {
        value = !value;
        return true;
    }
    return false;
}

void tooltip(const std::string& s, Vector2 at) {
    float w = measure(s, kSmall) + 18;
    Rectangle r{at.x, at.y, w, 32};
    if (r.x + r.width > static_cast<float>(GetScreenWidth()) - 4) r.x = static_cast<float>(GetScreenWidth()) - 4 - r.width;
    roundRect(r, 7, kBrand);
    text(s, {r.x + 9, r.y + 8}, kSmall, kBackground);
}

}  // namespace ui
