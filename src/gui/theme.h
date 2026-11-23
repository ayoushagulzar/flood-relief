// this file has all the colours and sizes of the GUI in one place.
// if i want to change the look i only change this file.
#pragma once // this prevents multiple inclusions of the header file.

#include "raylib.h"

// namespace so the names don't clash with raylib names
namespace theme {

// ================ sizes (in pixels) ================
constexpr float kSidebar = 220;
constexpr float kTopBar = 66;
constexpr float kRightPanel = 350;
constexpr float kKpiH = 100; // height of the cards under the map
constexpr float kPad = 8; // small spacing
constexpr float kGap = 16;
constexpr float kRadius = 10; // rounded corners
constexpr float kLine = 1;
constexpr int kWindowW = 1280;
constexpr int kWindowH = 720;

// ================ text sizes ================
constexpr float kSmall = 16;
constexpr float kBody = 18;
constexpr float kTitle = 28;
constexpr float kCapsSpacing = 1.0f;
constexpr float kTextSpacing = 0.0f;

// ================ colours ================
// Color{r, g, b, alpha}. burgundy = main colour of the app
constexpr Color kBrand{46, 18, 24, 255};
constexpr Color kBrand2{67, 27, 35, 255};
constexpr Color kAccent{140, 47, 63, 255};
constexpr Color kAccentHover{160, 62, 79, 255};
constexpr Color kAccentSoft{224, 188, 194, 255};
constexpr Color kAccentTint{245, 232, 234, 255};
constexpr Color kSideText{224, 198, 202, 255};

// backgrounds
constexpr Color kBackground{255, 255, 255, 255};
constexpr Color kPanel{252, 250, 250, 255};
constexpr Color kField{250, 245, 246, 255};
constexpr Color kCard{255, 255, 255, 255};
constexpr Color kHover{248, 240, 241, 255};
constexpr Color kPressed{240, 226, 229, 255};
constexpr Color kRule{238, 226, 228, 255};
constexpr Color kRuleSoft{245, 237, 238, 255};
constexpr Color kOverlay{255, 255, 255, 235};
constexpr Color kShadow{46, 18, 24, 22};

// text colours
constexpr Color kText{42, 21, 25, 255};
constexpr Color kMuted{122, 98, 102, 255};
constexpr Color kFaint{170, 150, 154, 255};

// status colours (orange = needs attention, blue = flooded, green = served)
constexpr Color kAlert{217, 122, 28, 255};
constexpr Color kRoute{46, 18, 24, 255};
constexpr Color kRouteCasing{255, 255, 255, 255};
constexpr Color kBlocked = kAlert;
constexpr Color kFlooded{61, 125, 216, 255};
constexpr Color kDamaged{176, 137, 0, 255};
constexpr Color kFulfilled{46, 139, 87, 255};
constexpr Color kPartial{201, 138, 27, 255};
constexpr Color kDeferred = kAlert;
constexpr Color kPending{170, 150, 154, 255};

// map colours
constexpr Color kMapBackground{243, 236, 237, 255};
constexpr Color kLand{255, 255, 255, 255};
constexpr Color kSea{216, 224, 228, 255};
constexpr Color kBorderLine{208, 184, 188, 255};
constexpr Color kGraticule{234, 222, 224, 255};
constexpr Color kRoad{185, 122, 132, 255};
constexpr Color kMotorway{140, 47, 63, 255};
constexpr Color kCity{85, 57, 62, 255};
constexpr Color kDepot{46, 18, 24, 255};
constexpr Color kShelter{46, 18, 24, 255};
constexpr Color kLabel{62, 42, 46, 255};
constexpr Color kHalo{255, 255, 255, 235};
constexpr float kLabelZoom = 1.6f; // labels show after this zoom
constexpr float kTileFade = 0.6f; // how pale the map tiles are

// urgency 1..5 -> grey to orange
inline Color urgencyColor(int u) {
    switch (u) {
        case 1: return Color{190, 172, 175, 255};
        case 2: return Color{170, 150, 154, 255};
        case 3: return kRoad;
        case 4: return kAccent;
        default: return kAlert;
    }
}

}
