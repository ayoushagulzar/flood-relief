// this file declares the fonts (Poppins) and small drawing helpers used in every panel
#pragma once // this prevents multiple inclusions of the header file.

#include <string>

#include "raylib.h"

#include "core/dynamic_array.h"

enum class Weight { Regular, SemiBold };

// loads the fonts in 3 sizes and 2 weights
class Fonts {
public:
    void load(); // loads from assets/fonts, uses raylib's font if file is missing
    void unload();
    const Font& get(float size, Weight w) const;

private:
    Font regular_[3]{};
    Font semibold_[3]{};
    bool loaded_ = false;
};

// drawing helpers
namespace ui {

void setFonts(const Fonts* fonts); // called once by App

// normal text / small CAPITAL label / text that ends at rightX
void text(const std::string& s, Vector2 pos, float size, Color c, Weight w = Weight::Regular);
void caps(const std::string& s, Vector2 pos, float size, Color c, Weight w = Weight::SemiBold);
void textRight(const std::string& s, float rightX, float y, float size, Color c, Weight w = Weight::Regular);
// width of the text in pixels
float measure(const std::string& s, float size, Weight w = Weight::Regular);
float measureCaps(const std::string& s, float size, Weight w = Weight::SemiBold);
// cuts long text and adds "..."
std::string fit(const std::string& s, float maxWidth, float size, Weight w = Weight::Regular);
// splits text into lines / draws wrapped text and returns its height
DynamicArray<std::string> wrap(const std::string& s, float maxWidth, float size, Weight w = Weight::Regular);
float paragraph(const std::string& s, Vector2 pos, float maxWidth, float size, Color c, float lineGap = 4);
std::string upper(const std::string& s);

// thin lines
void hline(float x1, float x2, float y, Color c);
void vline(float x, float y1, float y2, Color c);

// rounded boxes (radius in pixels). card = white box with shadow and border
void roundRect(Rectangle r, float radius, Color fill);
void roundBorder(Rectangle r, float radius, Color line, float thick = 1);
void card(Rectangle r, float radius = 10);

// icons drawn with lines (no image files)
enum class Icon { Select, Block, Add, Next, All, Home, Map, List, Box, Help, Log, Drop, Search, Clock, Check, Road };
void icon(Icon kind, Vector2 center, float size, Color c);

}
