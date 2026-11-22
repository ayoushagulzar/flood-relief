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

// ASCII plus the few extra symbols the UI uses (· – — → ↑ ↓ © ° × − …).
// LoadFontEx only bakes the glyphs we ask for, so anything missing here
// would show up as '?'.
Font loadOne(const char* path, int size) {
    static int codepoints[95 + 12];
    static int count = 0;
    if (count == 0) {
        for (int c = 32; c <= 126; ++c) codepoints[count++] = c;
        const int extra[] = {0xB7, 0x2013, 0x2014, 0x2192, 0x2191, 0x2193, 0xA9, 0xB0, 0xD7, 0x2212, 0x2026, 0x2039};
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
        regular_[i] = loadOne("assets/fonts/Archivo-Regular.ttf", static_cast<int>(kSizes[i]));
        semibold_[i] = loadOne("assets/fonts/Archivo-SemiBold.ttf", static_cast<int>(kSizes[i]));
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

// Simple line icons drawn with raylib shapes (no image files needed).
void icon(Icon kind, Vector2 c, float s, Color col) {
    float h = s / 2;
    float t = 1.6f;  // stroke width
    switch (kind) {
        case Icon::Select: {  // mouse arrow
            Vector2 a{c.x - h * 0.55f, c.y - h * 0.8f}, b{c.x + h * 0.7f, c.y + h * 0.1f},
                m{c.x + h * 0.0f, c.y + h * 0.25f}, d{c.x - h * 0.25f, c.y + h * 0.9f};
            DrawLineEx(a, b, t, col);
            DrawLineEx(b, m, t, col);
            DrawLineEx(m, d, t, col);
            DrawLineEx(d, a, t, col);
            break;
        }
        case Icon::Block:  // circle with a slash
            DrawRing(c, h * 0.8f - t, h * 0.8f, 0, 360, 36, col);
            DrawLineEx({c.x - h * 0.56f, c.y - h * 0.56f}, {c.x + h * 0.56f, c.y + h * 0.56f}, t, col);
            break;
        case Icon::Add:  // plus
            DrawLineEx({c.x - h * 0.7f, c.y}, {c.x + h * 0.7f, c.y}, t, col);
            DrawLineEx({c.x, c.y - h * 0.7f}, {c.x, c.y + h * 0.7f}, t, col);
            break;
        case Icon::Next: {  // play + bar
            Vector2 a{c.x - h * 0.6f, c.y - h * 0.7f}, b{c.x - h * 0.6f, c.y + h * 0.7f}, p{c.x + h * 0.4f, c.y};
            DrawLineEx(a, b, t, col);
            DrawLineEx(b, p, t, col);
            DrawLineEx(p, a, t, col);
            DrawLineEx({c.x + h * 0.7f, c.y - h * 0.7f}, {c.x + h * 0.7f, c.y + h * 0.7f}, t, col);
            break;
        }
        case Icon::All:  // fast-forward
            for (int k = 0; k < 2; ++k) {
                float ox = c.x - h * 0.75f + static_cast<float>(k) * h * 0.75f;
                Vector2 a{ox, c.y - h * 0.65f}, b{ox, c.y + h * 0.65f}, p{ox + h * 0.75f, c.y};
                DrawLineEx(a, b, t, col);
                DrawLineEx(b, p, t, col);
                DrawLineEx(p, a, t, col);
            }
            break;
        case Icon::Home: {  // house
            Vector2 l{c.x - h * 0.75f, c.y - h * 0.05f}, r{c.x + h * 0.75f, c.y - h * 0.05f}, top{c.x, c.y - h * 0.75f};
            DrawLineEx(l, top, t, col);
            DrawLineEx(top, r, t, col);
            DrawRectangleLinesEx({c.x - h * 0.5f, c.y - h * 0.05f, h, h * 0.75f}, t, col);
            break;
        }
    }
}

}  // namespace ui
