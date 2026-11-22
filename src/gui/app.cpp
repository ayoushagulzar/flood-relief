// app.cpp - Phase 7: layout, theme, map. (Panels and tools come in Phase 8.)
#include "gui/app.h"

#include "gui/theme.h"

using namespace theme;

Layout App::computeLayout(float w, float h) {
    Layout l;
    l.topBar = {0, 0, w, kTopBar};
    l.eventLog = {0, h - kEventLog, w, kEventLog};
    float midY = kTopBar, midH = h - kTopBar - kEventLog;
    l.rail = {0, midY, kRail, midH};
    l.rightPanel = {w - kRightPanel, midY, kRightPanel, midH};
    l.map = {kRail, midY, w - kRail - kRightPanel, midH};
    return l;
}

int App::run() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(kWindowW, kWindowH, "Flood Relief");
    SetWindowMinSize(1100, 640);
    SetTargetFPS(60);
    // data/, assets/ and cache/ live next to the .exe (CMake copies them there)
    ChangeDirectory(GetApplicationDirectory());

    fonts_.load();
    ui::setFonts(&fonts_);
    map_.init();
    load_ = ScenarioLoader::loadFile(scenarioPath_, manager_);

    while (!WindowShouldClose()) frame();

    map_.unload();
    fonts_.unload();
    CloseWindow();
    return 0;
}

void App::frame() {
    layout_ = computeLayout(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
    Vector2 mouse = GetMousePosition();
    bool mouseOverUI = !CheckCollisionPointRec(mouse, layout_.map);
    map_.update(layout_.map, mouseOverUI);

    BeginDrawing();
    ClearBackground(kBackground);
    if (load_.ok) {
        MapOverlay overlay;  // empty in Phase 7
        map_.draw(manager_, overlay);
    } else {
        DrawRectangleRec(layout_.map, kMapBackground);
        ui::caps("Could not load the scenario", {layout_.map.x + kGap * 2, layout_.map.y + kGap * 2}, kBody,
                 kBlocked);
        ui::paragraph(load_.error, {layout_.map.x + kGap * 2, layout_.map.y + kGap * 2 + 30},
                      layout_.map.width - kGap * 4, kBody, kText);
    }
    drawTopBar(layout_.topBar);
    drawRail(layout_.rail);
    drawRightPanel(layout_.rightPanel);
    drawEventLog(layout_.eventLog);
    EndDrawing();
}

void App::drawTopBar(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::hline(r.x, r.x + r.width, r.y + r.height - 1, kRule);
    float y = r.y + (r.height - kBody) / 2;
    ui::caps("Flood Relief", {r.x + kGap, y}, kBody, kText);
    float x = r.x + kGap + ui::measureCaps("Flood Relief", kBody) + kGap;
    ui::caps("Pakistan · Monsoon 2022", {x, y + 2}, kSmall, kMuted, Weight::Regular);

    // clock on the right
    std::string clock = formatClock(manager_.now());
    ui::textRight(clock, r.x + r.width - kGap, y - 2, kTitle, kText, Weight::SemiBold);
}

void App::drawRail(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::vline(r.x + r.width - 1, r.y, r.y + r.height, kRule);
    const ui::Icon icons[5] = {ui::Icon::Select, ui::Icon::Block, ui::Icon::Add, ui::Icon::Next, ui::Icon::All};
    float y = r.y + kGap;
    for (int i = 0; i < 5; ++i) {
        if (i == 3) {
            ui::hline(r.x + 16, r.x + r.width - 16, y + 2, kRule);
            y += kGap;
        }
        bool active = i == 0;
        Rectangle b{r.x + 6, y, r.width - 12, 44};
        if (active) {
            DrawRectangleRec(b, kHover);
            DrawRectangleRec({r.x, b.y + 8, 2, b.height - 16}, kAccent);
        }
        ui::icon(icons[i], {b.x + b.width / 2, b.y + b.height / 2}, 20, active ? kAccent : kText);
        y += 44 + kPad;
    }
}

void App::drawRightPanel(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::vline(r.x, r.y, r.y + r.height, kRule);
    const char* tabs[3] = {"Requests", "Depots", "Why"};
    float x = r.x + kGap;
    for (int i = 0; i < 3; ++i) {
        ui::caps(tabs[i], {x, r.y + 14}, kSmall, i == 0 ? kText : kMuted);
        if (i == 0) DrawRectangleRec({x, r.y + 40, ui::measureCaps(tabs[i], kSmall), 2}, kAccent);
        x += ui::measureCaps(tabs[i], kSmall) + kGap * 1.5f;
    }
    ui::hline(r.x, r.x + r.width, r.y + 42, kRule);
    if (load_.ok) {
        std::string summary = std::to_string(load_.nodes) + " places · " + std::to_string(load_.roads) + " roads · " +
                              std::to_string(load_.requests) + " requests";
        ui::text(summary, {r.x + kGap, r.y + 58}, kSmall, kMuted);
    }
    ui::text("Panels arrive in Phase 8.", {r.x + kGap, r.y + 80}, kSmall, kFaint);
}

void App::drawEventLog(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::hline(r.x, r.x + r.width, r.y, kRule);
    ui::caps("Event log", {r.x + kGap, r.y + kGap}, kSmall, kMuted);
    const DynamicArray<Event>& ev = manager_.events();
    float y = r.y + kGap + 24;
    for (std::size_t k = 0; k < ev.size() && k < 4; ++k) {
        const Event& e = ev[ev.size() - 1 - k];  // newest first
        ui::text(formatClock(e.time), {r.x + kGap, y}, kSmall, kMuted);
        ui::text(ui::fit(e.text, r.width - 200, kSmall), {r.x + 150, y}, kSmall, kText);
        y += 20;
    }
}
