// app.cpp - window, layout, map, tools, sidebar, top bar, search and KPIs.
#include "gui/app.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <exception>

#include "gui/picking.h"
#include "gui/theme.h"
#include "gui/widgets.h"

using namespace theme;

namespace {

constexpr float kNodePickPx = 10;  // how close (screen px) the mouse must be to a node
constexpr float kRoadPickPx = 6;   // ... or to a road
constexpr float kMapRadius = 14;   // map card corners
constexpr int kMaxMatches = 6;     // search drop-down rows
constexpr float kMatchRowH = 36;

std::string roadTitle(const Edge& e) { return e.roadName.empty() || e.roadName == "-" ? "Local road" : e.roadName; }

std::string lower(const std::string& s) {
    std::string out = s;
    for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return out;
}

// The map is drawn in a plain rectangle. To give it rounded corners we paint
// the 4 corner "outsides" in the page colour: for each corner, a fan of thin
// triangles between the corner point and the arc.
void maskCorners(Rectangle r, float rad, Color bg) {
    const int steps = 8;
    struct Corner {
        Vector2 point, centre;
        float startDeg;
    } cs[4] = {{{r.x, r.y}, {r.x + rad, r.y + rad}, 180},
               {{r.x + r.width, r.y}, {r.x + r.width - rad, r.y + rad}, 270},
               {{r.x + r.width, r.y + r.height}, {r.x + r.width - rad, r.y + r.height - rad}, 0},
               {{r.x, r.y + r.height}, {r.x + rad, r.y + r.height - rad}, 90}};
    for (const Corner& c : cs) {
        for (int i = 0; i < steps; ++i) {
            float a0 = (c.startDeg + 90.0f * static_cast<float>(i) / steps) * DEG2RAD;
            float a1 = (c.startDeg + 90.0f * static_cast<float>(i + 1) / steps) * DEG2RAD;
            Vector2 p0{c.centre.x + std::cos(a0) * rad, c.centre.y + std::sin(a0) * rad};
            Vector2 p1{c.centre.x + std::cos(a1) * rad, c.centre.y + std::sin(a1) * rad};
            // both windings: raylib only draws counter-clockwise triangles
            DrawTriangle(c.point, p1, p0, bg);
            DrawTriangle(c.point, p0, p1, bg);
        }
    }
}

}  // namespace

Layout App::computeLayout(float w, float h) {
    Layout l;
    l.sidebar = {0, 0, kSidebar, h};
    l.topBar = {kSidebar, 0, w - kSidebar, kTopBar};
    l.rightPanel = {w - kRightPanel, kTopBar, kRightPanel, h - kTopBar};
    float x0 = kSidebar + 18, x1 = w - kRightPanel - 18;
    l.header = {x0, kTopBar + 12, x1 - x0, 44};
    l.kpis = {x0, h - 16 - kKpiH, x1 - x0, kKpiH};
    float mapY = l.header.y + l.header.height + 8;
    float mapBottom = l.kpis.y - 40;  // room for the "Today's highlights" title
    l.map = {x0, mapY, x1 - x0, mapBottom - mapY};
    return l;
}

int App::run() {
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(kWindowW, kWindowH, "Flood Relief");
    SetWindowMinSize(1280, 720);
    SetTargetFPS(60);
    MaximizeWindow();      // a dashboard: use the whole screen
    SetExitKey(KEY_NULL);  // Esc cancels a tool instead of closing the window
    // data/, assets/ and cache/ live next to the .exe (CMake copies them there)
    ChangeDirectory(GetApplicationDirectory());

    fonts_.load();
    ui::setFonts(&fonts_);
    map_.init();
    loadScenario();

    while (!WindowShouldClose()) frame();

    map_.unload();
    fonts_.unload();
    CloseWindow();
    return 0;
}

// (Re)loads the scenario into a brand-new manager. Used at start and by Reset.
void App::loadScenario() {
    manager_ = std::make_unique<ReliefManager>();
    load_ = ScenarioLoader::loadFile(scenarioPath_, *manager_);
    tool_ = Tool::Select;
    page_ = Page::Overview;
    form_ = AddForm{};
    selNode_ = selU_ = selV_ = -1;
    pulseNode_ = -1;
    searchFocus_ = false;
    searchText_.clear();
    listScroll_ = depotScroll_ = whyScroll_ = logScroll_ = 0;
    for (int s = 0; s < kSupplyCount; ++s) initialMax_[s] = 1;
    const DynamicArray<Depot>& ds = manager_->depots();
    for (std::size_t i = 0; i < ds.size(); ++i) {
        for (int s = 0; s < kSupplyCount; ++s) {
            if (ds[i].stock[s] > initialMax_[s]) initialMax_[s] = ds[i].stock[s];
        }
    }
}

void App::frame() {
    layout_ = computeLayout(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
    tooltip_.clear();
    // Panels are drawn AFTER the map, so the map uses last frame's panel hover.
    lastPanelHover_ = panelHoverNode_;
    panelHoverNode_ = -1;

    // Search first: while it has focus, the keyboard belongs to it.
    if (load_.ok) updateSearch();

    // Panels first: if the mouse is on a panel (or on the info card or the
    // search list over the map), the map ignores it. Checked ONCE per frame.
    Vector2 mouse = GetMousePosition();
    bool cardVisible = (selNode_ >= 0 || selU_ >= 0) && tool_ != Tool::AddRequest;
    Rectangle sr = searchRect();
    Rectangle dropDown{sr.x, sr.y, sr.width, sr.height + 8 + kMaxMatches * kMatchRowH};
    bool mouseOverUI = !CheckCollisionPointRec(mouse, layout_.map) ||
                       (cardVisible && CheckCollisionPointRec(mouse, infoCardRect())) ||
                       (searchFocus_ && CheckCollisionPointRec(mouse, dropDown));
    map_.update(layout_.map, mouseOverUI, !searchFocus_);
    if (load_.ok) updateMap(mouseOverUI);

    // Keyboard shortcuts (everything also works with the mouse alone).
    if (load_.ok && !searchFocus_) {
        if (IsKeyPressed(KEY_S)) setTool(Tool::Select);
        if (IsKeyPressed(KEY_B)) setTool(tool_ == Tool::BlockRoad ? Tool::Select : Tool::BlockRoad);
        if (IsKeyPressed(KEY_R)) setTool(tool_ == Tool::AddRequest ? Tool::Select : Tool::AddRequest);
        if (IsKeyPressed(KEY_N)) processNext();
        if (IsKeyPressed(KEY_A)) processAll();
        if (IsKeyPressed(KEY_SLASH)) searchFocus_ = true;
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (tool_ != Tool::Select) setTool(Tool::Select);
            else selNode_ = selU_ = selV_ = -1;
        }
    }

    BeginDrawing();
    ClearBackground(kBackground);
    drawMapCard();
    drawHeader(layout_.header);
    if (load_.ok) drawKpis(layout_.kpis);
    drawTopBar(layout_.topBar);
    drawSidebar(layout_.sidebar);
    if (load_.ok) drawRightPanel(layout_.rightPanel);
    if (load_.ok) drawSearch();                                   // drop-down over everything
    if (!tooltip_.empty()) ui::tooltip(tooltip_, tooltipPos_);  // last = on top of everything
    EndDrawing();
}

// ---------------------------------------------------------------- map tools

void App::setTool(Tool t) {
    tool_ = t;
    if (t == Tool::AddRequest) {
        form_.error.clear();
        // If a city/shelter is already selected, start the form there.
        if (selNode_ >= 0 && manager_->graph().node(selNode_).type != NodeType::Depot) form_.locationId = selNode_;
    }
    if (t != Tool::Select) selU_ = selV_ = -1;
    if (t == Tool::BlockRoad) selNode_ = -1;  // the road card replaces the place card
}

// Hover picking every frame; click handling depends on the current tool.
void App::updateMap(bool mouseOverUI) {
    hoverNode_ = hoverU_ = hoverV_ = -1;
    if (mouseOverUI || map_.isDragging()) return;
    const Graph& g = manager_->graph();
    Vector2 mouse = GetMousePosition();

    if (tool_ != Tool::BlockRoad) {
        hoverNode_ = picking::pickNode(map_, g, mouse, kNodePickPx, tool_ != Tool::AddRequest);
    }
    if (hoverNode_ < 0 && tool_ != Tool::AddRequest) {
        int u = -1, v = -1;
        if (picking::pickRoad(map_, g, mouse, kRoadPickPx, u, v)) {
            hoverU_ = u;
            hoverV_ = v;
        }
    }
    if (map_.clicked()) handleMapClick();
}

void App::handleMapClick() {
    searchFocus_ = false;
    switch (tool_) {
        case Tool::Select:
            selNode_ = hoverNode_;
            selU_ = hoverU_;
            selV_ = hoverV_;
            break;
        case Tool::BlockRoad: {
            if (hoverU_ < 0) break;
            const Edge* e = manager_->graph().edgeBetween(hoverU_, hoverV_);
            if (!e) break;
            // The manager logs the event and retries deferred requests itself.
            if (e->blocked) manager_->reopenRoad(hoverU_, hoverV_);
            else manager_->blockRoad(hoverU_, hoverV_);
            selU_ = hoverU_;
            selV_ = hoverV_;
            break;
        }
        case Tool::AddRequest:
            if (hoverNode_ >= 0) {
                form_.locationId = hoverNode_;
                form_.error.clear();
            }
            break;
    }
}

void App::processNext() {
    if (!manager_->hasPending()) return;
    Decision d = manager_->processNext();
    if (!d.valid) return;
    pulseNode_ = manager_->requests()[static_cast<std::size_t>(d.requestIndex)].locationId;
    pulseStart_ = GetTime();
    if (page_ != Page::Why) page_ = Page::Overview;  // Overview shows the decision tiles
    whyScroll_ = 0;
}

void App::processAll() {
    if (!manager_->hasPending()) return;
    manager_->processAll();
    const Decision& d = manager_->lastDecision();
    if (d.valid) {
        pulseNode_ = manager_->requests()[static_cast<std::size_t>(d.requestIndex)].locationId;
        pulseStart_ = GetTime();
    }
}

void App::buildOverlay(MapOverlay& o) const {
    o.hoverNode = hoverNode_ >= 0 ? hoverNode_ : lastPanelHover_;
    o.hoverRoadU = hoverU_;
    o.hoverRoadV = hoverV_;
    o.selectedNode = tool_ == Tool::AddRequest ? form_.locationId : selNode_;
    o.selectedRoadU = selU_;
    o.selectedRoadV = selV_;
    o.roadMode = tool_ == Tool::BlockRoad;
    const Decision& d = manager_->lastDecision();
    if (d.valid) {
        for (std::size_t i = 0; i < d.legs.size(); ++i) o.routes.pushBack(d.legs[i].path);
    }
    o.pulseNode = pulseNode_;
    o.pulseAge = static_cast<float>(GetTime() - pulseStart_);
}

// ---------------------------------------------------------------- map card

void App::drawMapCard() {
    const Rectangle& m = layout_.map;
    ui::roundRect({m.x, m.y + 2, m.width, m.height}, kMapRadius, kShadow);
    if (load_.ok) {
        MapOverlay overlay;
        buildOverlay(overlay);
        map_.draw(*manager_, overlay);
        drawInfoCard();
    } else {
        DrawRectangleRec(m, kMapBackground);
        ui::text("Could not load the scenario", {m.x + kGap * 2, m.y + kGap * 2}, kBody, kAlert, Weight::SemiBold);
        ui::paragraph(load_.error, {m.x + kGap * 2, m.y + kGap * 2 + 30}, m.width - kGap * 4, kBody, kText);
    }
    maskCorners(m, kMapRadius, kBackground);
    ui::roundBorder(m, kMapRadius, kRule);
}

// The card grows with its content: a road, a depot (stock), or a place
// with 0-3 requests.
Rectangle App::infoCardRect() const {
    const Rectangle& m = layout_.map;
    float h = 170;
    if (selNode_ >= 0) {
        int rows = 2;
        if (manager_->graph().node(selNode_).type != NodeType::Depot) {
            rows = 0;
            const DynamicArray<Request>& rq = manager_->requests();
            for (std::size_t i = 0; i < rq.size() && rows < 3; ++i) rows += rq[i].locationId == selNode_ ? 1 : 0;
            if (rows == 0) rows = 1;
        }
        h = 104 + static_cast<float>(rows) * 23 + 12;
    }
    return {m.x + m.width - 330 - 12, m.y + m.height - h - 44, 330, h};
}

void App::drawInfoCard() {
    if ((selNode_ < 0 && selU_ < 0) || tool_ == Tool::AddRequest) return;
    const Graph& g = manager_->graph();
    Rectangle r = infoCardRect();
    ui::card(r, 12);
    float x = r.x + kGap, y = r.y + 14, w = r.width - kGap * 2;

    if (selNode_ >= 0) {
        const Node& n = g.node(selNode_);
        ui::text(ui::fit(n.name, w, kBody, Weight::SemiBold), {x, y}, kBody, kText, Weight::SemiBold);
        y += 26;
        char coords[64];
        std::snprintf(coords, sizeof(coords), "%s · %.2f°N %.2f°E", nodeTypeName(n.type), n.lat, n.lon);
        ui::text(coords, {x, y}, kSmall, kMuted);
        y += 23;
        const DynamicArray<Edge>& out = g.neighbors(selNode_);
        int blocked = 0;
        for (std::size_t i = 0; i < out.size(); ++i) blocked += out[i].blocked ? 1 : 0;
        std::string roads = std::to_string(out.size()) + (out.size() == 1 ? " road" : " roads") +
                            (blocked ? " · " + std::to_string(blocked) + " closed" : "");
        ui::text(roads, {x, y}, kSmall, blocked ? kAlert : kText);
        y += 23;
        ui::hline(x, x + w, y + 2, kRule);
        y += 10;
        if (n.type == NodeType::Depot) {
            const Depot* dp = manager_->findDepot(selNode_);
            if (dp) {
                for (int s = 0; s < kSupplyCount; ++s) {
                    float cx = x + static_cast<float>(s % 2) * 150, cy = y + static_cast<float>(s / 2) * 23;
                    ui::text(supplyName(static_cast<Supply>(s)), {cx, cy}, kSmall, kMuted);
                    ui::text(formatNumber(dp->stock[s]), {cx + 76, cy}, kSmall, dp->stock[s] > 0 ? kText : kAlert,
                             Weight::SemiBold);
                }
            }
        } else {
            const DynamicArray<Request>& rq = manager_->requests();
            int shown = 0;
            for (std::size_t i = 0; i < rq.size() && shown < 3; ++i) {
                if (rq[i].locationId != selNode_) continue;
                ui::text(rq[i].id, {x, y}, kSmall, kText, Weight::SemiBold);
                std::string line = std::string(supplyName(rq[i].supply)) + " · " + formatNumber(rq[i].remaining()) +
                                   " left · " + statusName(rq[i].status);
                ui::text(ui::fit(line, w - 52, kSmall), {x + 52, y}, kSmall, kMuted);
                y += 23;
                ++shown;
            }
            if (shown == 0) ui::text("No requests here.", {x, y}, kSmall, kMuted);
        }
        return;
    }

    const Edge* e = g.edgeBetween(selU_, selV_);
    if (!e) return;
    ui::text(roadTitle(*e), {x, y}, kBody, kText, Weight::SemiBold);
    if (e->blocked) {
        float tw = ui::measure("Closed", kSmall, Weight::SemiBold) + 16;
        Rectangle pill{r.x + r.width - kGap - tw, y, tw, 24};
        ui::roundRect(pill, 12, Fade(kAlert, 0.15f));
        ui::text("Closed", {pill.x + 8, pill.y + 4}, kSmall, kAlert, Weight::SemiBold);
    }
    y += 28;
    ui::text(ui::fit(g.node(selU_).name + " – " + g.node(selV_).name, w, kSmall), {x, y}, kSmall, kMuted);
    y += 25;
    ui::text(std::string(roadTypeName(e->type)) + " · " + conditionName(e->condition), {x, y}, kSmall, kText);
    y += 23;
    ui::text("Normal " + formatMinutes(e->baseMinutes) + " · now " + formatMinutes(e->weight()), {x, y}, kSmall, kText);
    y += 30;
    ui::text(tool_ == Tool::BlockRoad ? "Click again to reopen / close." : "Close road (B): click to close / reopen.",
             {x, y}, kSmall, kFaint);
}

// ---------------------------------------------------------------- header + KPIs

void App::drawHeader(Rectangle r) {
    ui::text("Current scenario", {r.x, r.y}, kSmall, kMuted);
    std::string title = "Pakistan";
    if (load_.ok) {
        const Graph& g = manager_->graph();
        title += " · " + std::to_string(g.nodeCount()) + " places · " + std::to_string(g.roadCount()) + " roads";
    }
    ui::text(title, {r.x, r.y + 19}, kBody, kText, Weight::SemiBold);

    // Right side: what a click on the map does right now.
    std::string hint = "Drag to move · wheel to zoom · click a place or road";
    Color hc = kMuted;
    if (tool_ == Tool::BlockRoad) {
        hint = "Close road: click a road to close or reopen it  (Esc to stop)";
        hc = kAccent;
    } else if (tool_ == Tool::AddRequest) {
        hint = "Add request: click a city or camp, then fill in the form";
        hc = kAccent;
    }
    ui::textRight(hint, r.x + r.width, r.y + 21, kSmall, hc, tool_ == Tool::Select ? Weight::Regular : Weight::SemiBold);
}

void App::drawKpis(Rectangle r) {
    ui::text("Today's highlights", {r.x, r.y - 31}, kBody, kText, Weight::SemiBold);
    Stats s = manager_->stats();
    const DynamicArray<Request>& rq = manager_->requests();
    int delivered = 0, needed = 0;
    for (std::size_t i = 0; i < rq.size(); ++i) {
        delivered += rq[i].delivered;
        needed += rq[i].remaining();
    }
    // Deferred: list the ids (they are waiting for a road or stock).
    std::string deferredIds;
    const DynamicArray<int>& df = manager_->deferredList();
    for (std::size_t i = 0; i < df.size() && i < 3; ++i) {
        const Request& q = rq[static_cast<std::size_t>(df[i])];
        if (i) deferredIds += ", ";
        deferredIds += q.id;
        if (df.size() == 1) deferredIds += " · " + manager_->graph().node(q.locationId).name;
    }
    if (df.size() > 3) deferredIds += " …";
    if (df.isEmpty()) deferredIds = "none waiting";

    struct Kpi {
        const char* title;
        std::string value, sub;
        Color c;
    } k[4] = {{"Served", std::to_string(s.fulfilled), formatNumber(delivered) + " units delivered", kAccent},
              {"Waiting", std::to_string(manager_->queueSnapshot().size()), "in the priority heap", kText},
              {"Deferred", std::to_string(df.size()), deferredIds, df.isEmpty() ? kText : kAlert},
              {"Still needed", formatNumber(needed), "units across Pakistan", kText}};
    float gap = 12, w = (r.width - gap * 3) / 4;
    for (int i = 0; i < 4; ++i) {
        Rectangle c{r.x + static_cast<float>(i) * (w + gap), r.y, w, r.height};
        ui::card(c, 12);
        ui::text(k[i].title, {c.x + 16, c.y + 12}, kSmall, kMuted);
        ui::text(k[i].value, {c.x + 16, c.y + 33}, kTitle, k[i].c, Weight::SemiBold);
        ui::text(ui::fit(k[i].sub, w - 32, kSmall), {c.x + 16, c.y + 70}, kSmall, kMuted);
    }
}

// ---------------------------------------------------------------- sidebar + top bar

void App::drawSidebar(Rectangle r) {
    DrawRectangleRec(r, kBrand);
    Rectangle logo{r.x + 22, r.y + 20, 34, 34};
    ui::roundRect(logo, 9, kAccent);
    ui::icon(ui::Icon::Drop, {logo.x + 17, logo.y + 17}, 18, kBackground);
    ui::text("Flood Relief", {logo.x + 46, logo.y + 7}, kBody, kBackground, Weight::SemiBold);

    struct Item {
        ui::Icon icon;
        const char* label;
        Page page;
    } items[5] = {{ui::Icon::Map, "Overview", Page::Overview},
                  {ui::Icon::List, "Queue", Page::Queue},
                  {ui::Icon::Box, "Depots", Page::Depots},
                  {ui::Icon::Help, "Why", Page::Why},
                  {ui::Icon::Log, "Events", Page::Events}};
    float y = r.y + 84;
    for (const Item& it : items) {
        Rectangle row{r.x, y, r.width, 48};
        if (ui::navItem(row, it.icon, it.label, page_ == it.page) && load_.ok) {
            page_ = it.page;
            if (tool_ == Tool::AddRequest) setTool(Tool::Select);
        }
        y += 48;
    }

    // Keyboard keys, so they are easy to find during a demo.
    float ky = r.y + r.height - 262;
    ui::caps("Keys", {r.x + 22, ky}, kSmall, Color{160, 120, 128, 255});
    const char* keys[5][2] = {{"N", "next request"}, {"A", "serve all"}, {"B", "close road"}, {"R", "add request"},
                              {"/", "search"}};
    for (int i = 0; i < 5; ++i) {
        float yy = ky + 26 + static_cast<float>(i) * 25;
        Rectangle cap{r.x + 22, yy - 1, 22, 21};
        ui::roundRect(cap, 4, kBrand2);
        float kw = ui::measure(keys[i][0], kSmall, Weight::SemiBold);
        ui::text(keys[i][0], {cap.x + (cap.width - kw) / 2, yy}, kSmall, kBackground, Weight::SemiBold);
        ui::text(keys[i][1], {r.x + 54, yy}, kSmall, kSideText);
    }

    // Scenario, clock and Reset at the bottom.
    float by = r.y + r.height - 92;
    ui::text("Pakistan monsoon 2022", {r.x + 22, by}, kSmall, kSideText);
    ui::text(formatClock(load_.ok ? manager_->now() : 0), {r.x + 22, by + 21}, kSmall, kBackground, Weight::SemiBold);
    Rectangle reset{r.x + 22, by + 50, r.width - 44, 32};
    bool hot = ui::hovered(reset);
    ui::roundRect(reset, 7, hot ? kBrand2 : kBrand);
    ui::roundBorder(reset, 7, Color{110, 60, 70, 255});
    float tw = ui::measure("Reset scenario", kSmall);
    ui::text("Reset scenario", {reset.x + (reset.width - tw) / 2, reset.y + 7}, kSmall, kSideText);
    if (ui::clickedIn(reset)) loadScenario();
    if (hot) {
        tooltip_ = "Reload data/pakistan.txt";
        tooltipPos_ = {reset.x + reset.width + 8, reset.y};
    }
}

void App::drawTopBar(Rectangle r) {
    DrawRectangleRec(r, kBackground);
    ui::hline(r.x, r.x + r.width, r.y + r.height - 1, kRule);
    if (!load_.ok) return;

    Rectangle s = searchRect();
    ui::roundRect(s, 9, kField);
    if (searchFocus_) ui::roundBorder(s, 9, kAccent, 1.5f);
    if (searchText_.empty()) {
        ui::text(searchFocus_ ? "Type a place name..." : "Search a place...", {s.x + 14, s.y + 11}, kSmall, kFaint);
    } else {
        ui::text(ui::fit(searchText_, s.width - 48, kSmall), {s.x + 14, s.y + 11}, kSmall, kText);
    }
    if (searchFocus_ && static_cast<int>(GetTime() * 2) % 2 == 0) {  // blinking cursor
        float cx = s.x + 14 + (searchText_.empty() ? 0 : ui::measure(searchText_, kSmall)) + 1;
        DrawRectangleRec({cx, s.y + 11, 1.2f, 19}, kText);
    }
    ui::icon(ui::Icon::Search, {s.x + s.width - 20, s.y + s.height / 2}, 16, kMuted);
    if (ui::clickedIn(s)) searchFocus_ = true;

    // Actions. Close road / Add request are tools: they stay "on" until
    // clicked again or Esc. Next / Serve all act immediately.
    bool pending = manager_->hasPending();
    float x = s.x + s.width + 18, y = r.y + 13, h = 40;
    struct Btn {
        ui::Icon icon;
        const char* label;
        float w;
        const char* tip;
    } btns[4] = {{ui::Icon::Next, "Next request", 152, "Serve the top of the priority heap  (N)"},
                 {ui::Icon::All, "Serve all", 120, "Serve every waiting request  (A)"},
                 {ui::Icon::Road, "Close road", 130, "Click roads to close / reopen them  (B)"},
                 {ui::Icon::Add, "Add request", 142, "Click a place to add a new request  (R)"}};
    for (int i = 0; i < 4; ++i) {
        Rectangle b{x, y, btns[i].w, h};
        bool active = (i == 2 && tool_ == Tool::BlockRoad) || (i == 3 && tool_ == Tool::AddRequest);
        bool enabled = i >= 2 || pending;
        if (ui::iconButton(b, btns[i].icon, btns[i].label, i == 0, enabled, active)) {
            if (i == 0) processNext();
            if (i == 1) processAll();
            if (i == 2) setTool(active ? Tool::Select : Tool::BlockRoad);
            if (i == 3) setTool(active ? Tool::Select : Tool::AddRequest);
        }
        if (ui::hovered(b)) {
            tooltip_ = enabled ? btns[i].tip : "The queue is empty";
            tooltipPos_ = {b.x, b.y + h + 8};
        }
        x += btns[i].w + 10;
    }

    // Right: the simulation clock.
    std::string clock = formatClock(manager_->now());
    float cw = ui::measure(clock, kBody, Weight::SemiBold);
    float cx = r.x + r.width - kRightPanel + kRightPanel - 22 - cw;
    if (cx > x + 30) {
        ui::icon(ui::Icon::Clock, {cx - 16, r.y + r.height / 2}, 16, kMuted);
        ui::text(clock, {cx, r.y + (r.height - kBody) / 2 - 1}, kBody, kText, Weight::SemiBold);
    }
}

// ---------------------------------------------------------------- search
//
// Type part of a name: the drop-down lists matching places (linear scan,
// 77 nodes, trivial). An EXACT name is found with Graph::findNode, which is
// our HashTable lookup: O(1) on average.

Rectangle App::searchRect() const { return {layout_.topBar.x + 22, layout_.topBar.y + 13, 280, 40}; }

void App::findMatches(DynamicArray<int>& out) const {
    if (searchText_.empty()) return;
    const Graph& g = manager_->graph();
    int exact = g.findNode(searchText_);  // HashTable: O(1)
    if (exact >= 0) out.pushBack(exact);
    std::string needle = lower(searchText_);
    for (std::size_t i = 0; i < g.nodeCount() && out.size() < kMaxMatches; ++i) {
        int id = static_cast<int>(i);
        if (id == exact) continue;
        if (lower(g.node(id).name).find(needle) != std::string::npos) out.pushBack(id);
    }
}

void App::selectAndShow(int nodeId) {
    if (tool_ == Tool::AddRequest) form_.locationId = nodeId;
    else setTool(Tool::Select);
    selNode_ = nodeId;
    selU_ = selV_ = -1;
    const Node& n = manager_->graph().node(nodeId);
    map_.focusOn(MapView::latLonToWorld(n.lat, n.lon));
    searchFocus_ = false;
    searchText_.clear();
}

void App::updateSearch() {
    if (!searchFocus_) return;
    // GetCharPressed: one call per typed character (handles shift, layouts).
    for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed()) {
        if (ch >= 32 && ch < 127 && searchText_.size() < 40) searchText_ += static_cast<char>(ch);
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !searchText_.empty()) searchText_.pop_back();
    if (IsKeyPressed(KEY_ESCAPE)) {
        searchFocus_ = false;
        searchText_.clear();
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        DynamicArray<int> m;
        findMatches(m);
        if (!m.isEmpty()) selectAndShow(m[0]);
    }
    // A click anywhere else closes the search.
    Rectangle s = searchRect();
    Rectangle dropDown{s.x, s.y, s.width, s.height + 8 + kMaxMatches * kMatchRowH};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !ui::hovered(dropDown)) searchFocus_ = false;
}

void App::drawSearch() {
    if (!searchFocus_ || searchText_.empty()) return;
    DynamicArray<int> m;
    findMatches(m);
    Rectangle s = searchRect();
    float rowH = kMatchRowH;
    Rectangle box{s.x, s.y + s.height + 6, s.width, (m.isEmpty() ? 1 : static_cast<float>(m.size())) * rowH + 8};
    ui::card(box, 10);
    if (m.isEmpty()) {
        ui::text("No place called \"" + ui::fit(searchText_, 140, kSmall) + "\"", {box.x + 12, box.y + 12}, kSmall,
                 kMuted);
        return;
    }
    const Graph& g = manager_->graph();
    for (std::size_t i = 0; i < m.size(); ++i) {
        Rectangle row{box.x + 4, box.y + 4 + static_cast<float>(i) * rowH, box.width - 8, rowH};
        bool hot = ui::hovered(row);
        if (hot || (i == 0 && !ui::hovered(box))) ui::roundRect(row, 7, kHover);
        const Node& n = g.node(m[i]);
        ui::text(ui::fit(n.name, row.width - 100, kSmall), {row.x + 10, row.y + 9}, kSmall, kText,
                 i == 0 ? Weight::SemiBold : Weight::Regular);
        ui::textRight(nodeTypeName(n.type), row.x + row.width - 10, row.y + 9, kSmall, kFaint);
        if (hot) panelHoverNode_ = m[i];
        if (ui::clickedIn(row)) {
            selectAndShow(m[i]);
            return;
        }
    }
}
