// theme.h - every colour and size the GUI uses. Nothing is hard-coded elsewhere.
//
// Look: "Burgundy dashboard". A dark burgundy sidebar, a white work area,
// soft rounded cards, and a light map. Colours carry meaning:
//   burgundy  = the app itself (buttons, bars, selection)
//   orange    = needs attention (urgent request, closed road, deferred)
//   blue      = water (flooded road), green = served
#pragma once

#include "raylib.h"

namespace theme {

// ---------- layout (pixels) ----------
constexpr float kSidebar = 220;
constexpr float kTopBar = 66;
constexpr float kRightPanel = 350;
constexpr float kKpiH = 100;     // "Today's highlights" cards under the map
constexpr float kPad = 8;        // 8 px spacing everywhere
constexpr float kGap = 16;       // 2 x kPad
constexpr float kRadius = 10;    // card corners
constexpr float kLine = 1;       // hairline thickness
constexpr int kWindowW = 1280;
constexpr int kWindowH = 720;

// ---------- type (Poppins, OFL) ----------
constexpr float kSmall = 16;
constexpr float kBody = 18;
constexpr float kTitle = 28;
constexpr float kCapsSpacing = 1.0f;  // letter spacing for small UPPERCASE tags
constexpr float kTextSpacing = 0.0f;

// ---------- brand (burgundy) ----------
constexpr Color kBrand{46, 18, 24, 255};       // #2E1218 sidebar, primary button
constexpr Color kBrand2{67, 27, 35, 255};      // #431B23 active sidebar row
constexpr Color kAccent{140, 47, 63, 255};     // #8C2F3F bars, tiles, selection
constexpr Color kAccentHover{160, 62, 79, 255};
constexpr Color kAccentSoft{224, 188, 194, 255};  // #E0BCC2
constexpr Color kAccentTint{245, 232, 234, 255};  // #F5E8EA bar track, active tool
constexpr Color kSideText{224, 198, 202, 255};    // inactive sidebar text

// ---------- surfaces ----------
constexpr Color kBackground{255, 255, 255, 255};
constexpr Color kPanel{252, 250, 250, 255};    // right panel
constexpr Color kField{250, 245, 246, 255};    // search box, dashed areas
constexpr Color kCard{255, 255, 255, 255};
constexpr Color kHover{248, 240, 241, 255};
constexpr Color kPressed{240, 226, 229, 255};
constexpr Color kRule{238, 226, 228, 255};     // #EEE2E4 card borders, dividers
constexpr Color kRuleSoft{245, 237, 238, 255};
constexpr Color kOverlay{255, 255, 255, 235};  // legend / info cards over the map
constexpr Color kShadow{46, 18, 24, 22};

// ---------- text ----------
constexpr Color kText{42, 21, 25, 255};        // #2A1519
constexpr Color kMuted{122, 98, 102, 255};     // #7A6266
constexpr Color kFaint{170, 150, 154, 255};

// ---------- signal colours ----------
constexpr Color kAlert{217, 122, 28, 255};     // #D97A1C urgent / closed / deferred
constexpr Color kRoute{46, 18, 24, 255};       // last delivery route (dark burgundy)
constexpr Color kRouteCasing{255, 255, 255, 255};
constexpr Color kBlocked = kAlert;
constexpr Color kFlooded{61, 125, 216, 255};   // water blue
constexpr Color kDamaged{176, 137, 0, 255};    // mustard
constexpr Color kFulfilled{46, 139, 87, 255};
constexpr Color kPartial{201, 138, 27, 255};
constexpr Color kDeferred = kAlert;
constexpr Color kPending{170, 150, 154, 255};

// ---------- map ----------
constexpr Color kMapBackground{243, 236, 237, 255};  // neighbours (offline)
constexpr Color kLand{255, 255, 255, 255};           // Pakistan (offline)
constexpr Color kSea{216, 224, 228, 255};
constexpr Color kBorderLine{208, 184, 188, 255};
constexpr Color kGraticule{234, 222, 224, 255};
constexpr Color kRoad{185, 122, 132, 255};           // #B97A84
constexpr Color kMotorway{140, 47, 63, 255};         // #8C2F3F
constexpr Color kCity{85, 57, 62, 255};
constexpr Color kDepot{46, 18, 24, 255};
constexpr Color kShelter{46, 18, 24, 255};
constexpr Color kLabel{62, 42, 46, 255};
constexpr Color kHalo{255, 255, 255, 235};           // outline behind map labels
constexpr float kLabelZoom = 1.6f;                   // camera zoom above which labels show
constexpr float kTileFade = 0.6f;                    // 0 = OSM colours, 1 = white

// Urgency 1..5: grey -> burgundy -> orange.
inline Color urgencyColor(int u) {
    switch (u) {
        case 1: return Color{190, 172, 175, 255};
        case 2: return Color{170, 150, 154, 255};
        case 3: return kRoad;
        case 4: return kAccent;
        default: return kAlert;
    }
}

}  // namespace theme
