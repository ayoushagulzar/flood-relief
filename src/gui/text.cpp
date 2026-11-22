// text.cpp - font loading and text/icon helpers.
#include "gui/text.h"

#include <cctype>

#include "gui/theme.h"

namespace {

const float kSizes[3] = {theme::kSmall, theme::kBody, theme::kTitle};

int sizeIndex(float size) {
    if (size >= theme::kTitle) return 2;
    if (size >= theme::kBody) return 1;
    return 0;
}

// ASCII plus the few extra symbols the UI uses (· – — › ‹ © ° × − …).
// (Poppins has no arrow glyphs, so paths use › instead of an arrow.)
// LoadFontEx only bakes the glyphs we ask for, so anything missing here
// would show up as '?'.
Font loadOne(const char* path, int size) {
    static int codepoints[95 + 11];
    static int count = 0;
    if (count == 0) {
        for (int c = 32; c <= 126; ++c) codepoints[count++] = c;
        const int extra[] = {0xB7, 0x2013, 0x2014, 0x203A, 0x2039, 0xA9, 0xB0, 0xD7, 0x2212, 0x2026, 0xF7};
        for (int c : extra) codepoints[count++] = c;
    }
    if (!FileExists(path)) return GetFontDefault();
    Font f = LoadFontEx(path, size, codepoints, count);
    if (f.texture.id == 0) return GetFontDefault();
    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
    return f;
}

const Fonts* gFonts = nullptr;

const Font& fontFor(float size, Weight w) {
    static Font fallback = GetFontDefault();
    return gFonts ? gFonts->get(size, w) : fallback;
}

}  // namespace

void Fonts::load() {
    for (int i = 0; i < 3; ++i) {
        regular_[i] = loadOne("assets/fonts/Poppins-Regular.ttf", static_cast<int>(kSizes[i]));
        semibold_[i] = loadOne("assets/fonts/Poppins-SemiBold.ttf", static_cast<int>(kSizes[i]));
    }
    loaded_ = true;
}

void Fonts::unload() {
    if (!loaded_) return;
    Font def = GetFontDefault();
    for (int i = 0; i < 3; ++i) {
        if (regular_[i].texture.id != def.texture.id) UnloadFont(regular_[i]);
        if (semibold_[i].texture.id != def.texture.id) UnloadFont(semibold_[i]);
    }
    loaded_ = false;
}

const Font& Fonts::get(float size, Weight w) const {
    int i = sizeIndex(size);
    return w == Weight::SemiBold ? semibold_[i] : regular_[i];
}

namespace ui {

void setFonts(const Fonts* fonts) { gFonts = fonts; }

std::string upper(const std::string& s) {
    std::string out = s;
    for (char& ch : out) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return out;
}

void text(const std::string& s, Vector2 pos, float size, Color c, Weight w) {
    DrawTextEx(fontFor(size, w), s.c_str(), pos, size, theme::kTextSpacing, c);
}

void caps(const std::string& s, Vector2 pos, float size, Color c, Weight w) {
    DrawTextEx(fontFor(size, w), upper(s).c_str(), pos, size, theme::kCapsSpacing, c);
}

float measure(const std::string& s, float size, Weight w) {
    return MeasureTextEx(fontFor(size, w), s.c_str(), size, theme::kTextSpacing).x;
}

float measureCaps(const std::string& s, float size, Weight w) {
    return MeasureTextEx(fontFor(size, w), upper(s).c_str(), size, theme::kCapsSpacing).x;
}

void textRight(const std::string& s, float rightX, float y, float size, Color c, Weight w) {
    text(s, {rightX - measure(s, size, w), y}, size, c, w);
}

std::string fit(const std::string& s, float maxWidth, float size, Weight w) {
    if (measure(s, size, w) <= maxWidth) return s;
    std::string cut = s;
    while (!cut.empty() && measure(cut + "...", size, w) > maxWidth) cut.pop_back();
    return cut + "...";
}

DynamicArray<std::string> wrap(const std::string& s, float maxWidth, float size, Weight w) {
    DynamicArray<std::string> lines;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string candidate = line.empty() ? word : line + " " + word;
        if (!line.empty() && measure(candidate, size, w) > maxWidth) {
            lines.pushBack(line);
            line = word;
        } else {
            line = candidate;
        }
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flushWord();
        else word += ch;
    }
    flushWord();
    if (!line.empty()) lines.pushBack(line);
    return lines;
}

float paragraph(const std::string& s, Vector2 pos, float maxWidth, float size, Color c, float lineGap) {
    DynamicArray<std::string> lines = wrap(s, maxWidth, size);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        text(lines[i], {pos.x, pos.y + static_cast<float>(i) * (size + lineGap)}, size, c);
    }
    return static_cast<float>(lines.size()) * (size + lineGap);
}

void hline(float x1, float x2, float y, Color c) { DrawRectangleRec({x1, y, x2 - x1, theme::kLine}, c); }
void vline(float x, float y1, float y2, Color c) { DrawRectangleRec({x, y1, theme::kLine, y2 - y1}, c); }

// raylib's "roundness" is radius / (half of the shorter side).
static float roundness(Rectangle r, float radius) {
    float shorter = r.width < r.height ? r.width : r.height;
    if (shorter <= 0) return 0;
    float k = radius * 2 / shorter;
    return k > 1 ? 1 : k;
}

void roundRect(Rectangle r, float radius, Color fill) { DrawRectangleRounded(r, roundness(r, radius), 8, fill); }

void roundBorder(Rectangle r, float radius, Color line, float thick) {
    DrawRectangleRoundedLinesEx(r, roundness(r, radius), 8, thick, line);
}

void card(Rectangle r, float radius) {
    roundRect({r.x, r.y + 2, r.width, r.height}, radius, theme::kShadow);  // shadow: 2 px lower
    roundRect(r, radius, theme::kCard);
    roundBorder(r, radius, theme::kRule);
}

// Simple line icons drawn with raylib shapes (no image files needed).
// size = the icon's box; h = half of it; t = stroke width.
void icon(Icon kind, Vector2 c, float s, Color col) {
    float h = s / 2;
    float t = 1.6f;
    auto L = [&](float x1, float y1, float x2, float y2) {  // line in "h units" around the centre
        DrawLineEx({c.x + x1 * h, c.y + y1 * h}, {c.x + x2 * h, c.y + y2 * h}, t, col);
    };
    switch (kind) {
        case Icon::Select:  // mouse arrow
            L(-0.55f, -0.8f, 0.7f, 0.1f);
            L(0.7f, 0.1f, 0.0f, 0.25f);
            L(0.0f, 0.25f, -0.25f, 0.9f);
            L(-0.25f, 0.9f, -0.55f, -0.8f);
            break;
        case Icon::Block:  // circle with a slash
            DrawRing(c, h * 0.8f - t, h * 0.8f, 0, 360, 36, col);
            L(-0.56f, -0.56f, 0.56f, 0.56f);
            break;
        case Icon::Add:  // plus
            L(-0.7f, 0, 0.7f, 0);
            L(0, -0.7f, 0, 0.7f);
            break;
        case Icon::Next:  // play + bar
            L(-0.6f, -0.7f, -0.6f, 0.7f);
            L(-0.6f, 0.7f, 0.4f, 0);
            L(0.4f, 0, -0.6f, -0.7f);
            L(0.7f, -0.7f, 0.7f, 0.7f);
            break;
        case Icon::All:  // fast-forward
            for (int k = 0; k < 2; ++k) {
                float ox = -0.75f + static_cast<float>(k) * 0.75f;
                L(ox, -0.65f, ox, 0.65f);
                L(ox, 0.65f, ox + 0.75f, 0);
                L(ox + 0.75f, 0, ox, -0.65f);
            }
            break;
        case Icon::Home:  // house
            L(-0.75f, -0.05f, 0, -0.75f);
            L(0, -0.75f, 0.75f, -0.05f);
            DrawRectangleLinesEx({c.x - h * 0.5f, c.y - h * 0.05f, h, h * 0.75f}, t, col);
            break;
        case Icon::Map:  // folded map
            L(-0.8f, -0.55f, -0.27f, -0.75f);
            L(-0.27f, -0.75f, 0.27f, -0.55f);
            L(0.27f, -0.55f, 0.8f, -0.75f);
            L(0.8f, -0.75f, 0.8f, 0.55f);
            L(0.8f, 0.55f, 0.27f, 0.75f);
            L(0.27f, 0.75f, -0.27f, 0.55f);
            L(-0.27f, 0.55f, -0.8f, 0.75f);
            L(-0.8f, 0.75f, -0.8f, -0.55f);
            L(-0.27f, -0.75f, -0.27f, 0.55f);
            L(0.27f, -0.55f, 0.27f, 0.75f);
            break;
        case Icon::List:  // three dots + three lines
            for (int k = -1; k <= 1; ++k) {
                DrawCircleV({c.x - h * 0.7f, c.y + static_cast<float>(k) * h * 0.55f}, 1.3f, col);
                L(-0.35f, static_cast<float>(k) * 0.55f, 0.8f, static_cast<float>(k) * 0.55f);
            }
            break;
        case Icon::Box:  // parcel
            L(-0.75f, -0.4f, 0, -0.8f);
            L(0, -0.8f, 0.75f, -0.4f);
            L(0.75f, -0.4f, 0, 0);
            L(0, 0, -0.75f, -0.4f);
            L(-0.75f, -0.4f, -0.75f, 0.45f);
            L(-0.75f, 0.45f, 0, 0.85f);
            L(0, 0.85f, 0.75f, 0.45f);
            L(0.75f, 0.45f, 0.75f, -0.4f);
            L(0, 0, 0, 0.85f);
            break;
        case Icon::Help:  // circled "?"
            DrawRing(c, h * 0.85f - t, h * 0.85f, 0, 360, 36, col);
            DrawRing({c.x, c.y - h * 0.22f}, h * 0.28f - t * 0.9f, h * 0.28f, -90, 160, 16, col);
            L(0.05f, 0.05f, 0, 0.2f);
            DrawCircleV({c.x, c.y + h * 0.48f}, 1.2f, col);
            break;
        case Icon::Log:  // page with lines
            DrawRectangleLinesEx({c.x - h * 0.65f, c.y - h * 0.8f, h * 1.3f, h * 1.6f}, t, col);
            L(-0.35f, -0.35f, 0.35f, -0.35f);
            L(-0.35f, 0.0f, 0.35f, 0.0f);
            L(-0.35f, 0.35f, 0.15f, 0.35f);
            break;
        case Icon::Drop: {  // water drop (filled)
            DrawCircleV({c.x, c.y + h * 0.25f}, h * 0.52f, col);
            DrawTriangle({c.x, c.y - h * 0.85f}, {c.x - h * 0.47f, c.y + h * 0.05f}, {c.x + h * 0.47f, c.y + h * 0.05f},
                         col);
            break;
        }
        case Icon::Search:  // magnifier
            DrawRing({c.x - h * 0.15f, c.y - h * 0.15f}, h * 0.5f - t, h * 0.5f, 0, 360, 32, col);
            L(0.22f, 0.22f, 0.75f, 0.75f);
            break;
        case Icon::Clock:
            DrawRing(c, h * 0.85f - t, h * 0.85f, 0, 360, 36, col);
            L(0, -0.5f, 0, 0);
            L(0, 0, 0.35f, 0.25f);
            break;
        case Icon::Check:
            L(-0.6f, 0.0f, -0.15f, 0.45f);
            L(-0.15f, 0.45f, 0.65f, -0.45f);
            break;
        case Icon::Road:  // two edges + centre dashes
            L(-0.75f, 0.85f, -0.3f, -0.85f);
            L(0.75f, 0.85f, 0.3f, -0.85f);
            L(0, -0.6f, 0, -0.3f);
            L(0, 0.05f, 0, 0.35f);
            break;
    }
}

}  // namespace ui
