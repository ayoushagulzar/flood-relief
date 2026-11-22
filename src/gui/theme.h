// theme.h - every colour and size the GUI uses. Nothing is hard-coded elsewhere.
//
// Look: "Ink & signal". One black plane, thin grey rules instead of boxes,
// square corners, uppercase labels with wide letter spacing, and very few
// colours: each colour MEANS something (status, road state, route).
#pragma once

#include "raylib.h"

namespace theme {

// ---------- layout (pixels) ----------
constexpr float kTopBar = 48;
constexpr float kRail = 56;
constexpr float kRightPanel = 340;
constexpr float kEventLog = 130;
constexpr float kPad = 8;        // 8 px spacing everywhere
constexpr float kGap = 16;       // 2 x kPad
constexpr float kRadius = 0;     // square corners (Ink & signal)
constexpr float kLine = 1;       // hairline thickness
constexpr int kWindowW = 1280;
constexpr int kWindowH = 720;

// ---------- type ----------
constexpr float kSmall = 13;
constexpr float kBody = 16;
constexpr float kTitle = 22;
constexpr float kCapsSpacing = 1.6f;  // letter spacing for UPPERCASE labels
constexpr float kTextSpacing = 0.0f;

// ---------- surfaces ----------
constexpr Color kBackground{0, 0, 0, 255};
constexpr Color kPanel{0, 0, 0, 255};
constexpr Color kHover{18, 18, 18, 255};
constexpr Color kPressed{28, 28, 28, 255};
constexpr Color kRule{42, 42, 42, 255};       // hairlines between areas
constexpr Color kRuleSoft{26, 26, 26, 255};   // hairlines between rows
constexpr Color kOverlay{0, 0, 0, 220};       // legend / info cards over the map

// ---------- text ----------
constexpr Color kText{244, 244, 239, 255};
constexpr Color kMuted{143, 143, 137, 255};
constexpr Color kFaint{94, 94, 90, 255};

// ---------- signal colours ----------
constexpr Color kAccent{255, 61, 0, 255};     // active tool, active tab, selection
constexpr Color kRoute{255, 212, 0, 255};     // last delivery route (signal yellow)
constexpr Color kBlocked{255, 61, 0, 255};    // blocked road (dashed)
constexpr Color kFlooded{77, 124, 255, 255};  // flooded road: water blue
constexpr Color kDamaged{255, 159, 28, 255};  // damaged road
constexpr Color kFulfilled{0, 200, 83, 255};
constexpr Color kPartial{255, 159, 28, 255};
constexpr Color kDeferred{255, 61, 0, 255};
constexpr Color kPending{143, 143, 137, 255};

// ---------- map ----------
constexpr Color kMapBackground{5, 5, 5, 255};
constexpr Color kGraticule{20, 20, 20, 255};
constexpr Color kRoad{85, 85, 79, 255};
constexpr Color kMotorway{120, 120, 112, 255};
constexpr Color kCity{244, 244, 239, 255};
constexpr Color kDepot{244, 244, 239, 255};
constexpr Color kShelter{244, 244, 239, 255};
constexpr Color kHalo{0, 0, 0, 230};          // outline behind map labels
constexpr float kLabelZoom = 1.6f;            // camera zoom above which labels show
constexpr float kTileDarkness = 0.42f;        // brightness of inverted OSM tiles

// Urgency 1..5: muted grey -> signal red.
inline Color urgencyColor(int u) {
    switch (u) {
        case 1: return Color{94, 94, 90, 255};
        case 2: return Color{143, 143, 137, 255};
        case 3: return Color{255, 212, 0, 255};
        case 4: return Color{255, 159, 28, 255};
        default: return Color{255, 61, 0, 255};
    }
}

}  // namespace theme
