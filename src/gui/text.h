// text.h - one font family (Archivo, OFL licence) in three sizes, plus small
// drawing helpers used by every panel: text, uppercase labels, wrapping, icons.
#pragma once

#include <string>

#include "raylib.h"

#include "core/dynamic_array.h"

enum class Weight { Regular, SemiBold };

class Fonts {
public:
    // Loads assets/fonts/Archivo-*.ttf at 13 / 16 / 22 px. Falls back to
    // raylib's built-in font if a file is missing (the app still runs).
    void load();
    void unload();
    const Font& get(float size, Weight w) const;

private:
    Font regular_[3]{};
    Font semibold_[3]{};
    bool loaded_ = false;
};

namespace ui {

void setFonts(const Fonts* fonts);  // called once by App

// Normal text. Top-left at pos.
void text(const std::string& s, Vector2 pos, float size, Color c, Weight w = Weight::Regular);
// UPPERCASE label with wide letter spacing (the Ink & signal look).
void caps(const std::string& s, Vector2 pos, float size, Color c, Weight w = Weight::SemiBold);
// Right-aligned: the text ENDS at rightX.
void textRight(const std::string& s, float rightX, float y, float size, Color c, Weight w = Weight::Regular);
float measure(const std::string& s, float size, Weight w = Weight::Regular);
float measureCaps(const std::string& s, float size, Weight w = Weight::SemiBold);
// Cuts the text and adds "..." so it fits in maxWidth.
std::string fit(const std::string& s, float maxWidth, float size, Weight w = Weight::Regular);
// Splits s into lines no wider than maxWidth (breaks at spaces).
DynamicArray<std::string> wrap(const std::string& s, float maxWidth, float size, Weight w = Weight::Regular);
// Draws wrapped text; returns the height used.
float paragraph(const std::string& s, Vector2 pos, float maxWidth, float size, Color c, float lineGap = 4);
std::string upper(const std::string& s);

// Hairline helpers.
void hline(float x1, float x2, float y, Color c);
void vline(float x, float y1, float y2, Color c);

enum class Icon { Select, Block, Add, Next, All, Home };
void icon(Icon kind, Vector2 center, float size, Color c);

}  // namespace ui
