// app.cpp - window, layout, map interaction, tools, rail and top bar.
#include "gui/app.h"

#include <cstdio>
#include <exception>

#include "gui/picking.h"
#include "gui/theme.h"
#include "gui/widgets.h"

using namespace theme;

namespace {

constexpr float kNodePickPx = 10;  // how close (screen px) the mouse must be to a node
constexpr float kRoadPickPx = 6;   // ... or to a road

std::string roadTitle(const Edge& e) { return e.roadName.empty() || e.roadName == "-" ? "Local road" : e.roadName; }

}  // namespace

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
    tab_ = Tab::Requests;
    form_ = AddForm{};
    selNode_ = selU_ = selV_ = -1;
    pulseNode_ = -1;
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
    panelHoverNode_ = -1;

    // Panel first: if the mouse is on a panel (or on the info card over the
    // map), the map ignores it. Checked ONCE per frame, used everywhere.
    Vector2 mouse = GetMousePosition();
    bool cardVisible = (selNode_ >= 0 || selU_ >= 0) && tool_ != Tool::AddRequest;
    bool mouseOverUI = !CheckCollisionPointRec(mouse, layout_.map) ||
                       (cardVisible && CheckCollisionPointRec(mouse, infoCardRect()));
    map_.update(layout_.map, mouseOverUI);
    if (load_.ok) updateMap(mouseOverUI);

    // Keyboard shortcuts (everything also works with the mouse alone).
    if (load_.ok) {
        if (IsKeyPressed(KEY_S)) setTool(Tool::Select);
        if (IsKeyPressed(KEY_B)) setTool(Tool::BlockRoad);
        if (IsKeyPressed(KEY_R)) setTool(Tool::AddRequest);
        if (IsKeyPressed(KEY_N)) processNext();
        if (IsKeyPressed(KEY_A)) processAll();
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (tool_ != Tool::Select) setTool(Tool::Select);
            else selNode_ = selU_ = selV_ = -1;
        }
    }

    BeginDrawing();
    ClearBackground(kBackground);
    if (load_.ok) {
        MapOverlay overlay;
        buildOverlay(overlay);
        map_.draw(*manager_, overlay);
        drawInfoCard();
    } else {
        DrawRectangleRec(layout_.map, kMapBackground);
        ui::caps("Could not load the scenario", {layout_.map.x + kGap * 2, layout_.map.y + kGap * 2}, kBody,
                 kBlocked);
        ui::paragraph(load_.error, {layout_.map.x + kGap * 2, layout_.map.y + kGap * 2 + 30},
                      layout_.map.width - kGap * 4, kBody, kText);
    }
    drawTopBar(layout_.topBar);
    drawRail(layout_.rail);
    if (load_.ok) {
        drawRightPanel(layout_.rightPanel);
        drawEventLog(layout_.eventLog);
    }
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
    tab_ = Tab::Why;  // the Why tab explains what just happened
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
    tab_ = Tab::Requests;
}

void App::buildOverlay(MapOverlay& o) const {
    o.hoverNode = hoverNode_ >= 0 ? hoverNode_ : panelHoverNode_;
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

// ---------------------------------------------------------------- info card

Rectangle App::infoCardRect() const {
    const Rectangle& m = layout_.map;
    return {m.x + m.width - 300 - kGap, m.y + m.height - 170 - 22 - kGap * 2, 300, 170};
}

void App::drawInfoCard() {
    if ((selNode_ < 0 && selU_ < 0) || tool_ == Tool::AddRequest) return;
    const Graph& g = manager_->graph();
    Rectangle r = infoCardRect();
    DrawRectangleRec(r, kOverlay);
    DrawRectangleLinesEx(r, 1, kRule);
    DrawRectangleRec({r.x, r.y, 2, r.height}, kAccent);
    float x = r.x + kGap, y = r.y + kGap, w = r.width - kGap * 2;

    if (selNode_ >= 0) {
        const Node& n = g.node(selNode_);
        ui::text(ui::fit(n.name, w, kBody, Weight::SemiBold), {x, y}, kBody, kText, Weight::SemiBold);
        y += 24;
        char coords[64];
        std::snprintf(coords, sizeof(coords), "%s · %.2f°N %.2f°E", nodeTypeName(n.type), n.lat, n.lon);
        ui::caps(coords, {x, y}, kSmall, kMuted, Weight::Regular);
        y += 24;
        const DynamicArray<Edge>& out = g.neighbors(selNode_);
        int blocked = 0;
        for (std::size_t i = 0; i < out.size(); ++i) blocked += out[i].blocked ? 1 : 0;
        std::string roads = std::to_string(out.size()) + (out.size() == 1 ? " road" : " roads") + (blocked ? " · " + std::to_string(blocked) + " blocked" : "");
        ui::text(roads, {x, y}, kSmall, kText);
        y += 22;
        if (n.type == NodeType::Depot) {
            const Depot* dp = manager_->findDepot(selNode_);
            if (dp) {
                for (int s = 0; s < kSupplyCount; ++s) {
                    std::string line = std::string(supplyName(static_cast<Supply>(s))) + "  " + formatNumber(dp->stock[s]);
                    ui::text(line, {x + static_cast<float>(s % 2) * 140, y + static_cast<float>(s / 2) * 20}, kSmall,
                             dp->stock[s] > 0 ? kText : kBlocked);
                }
            }
        } else {
            const DynamicArray<Request>& rq = manager_->requests();
            int shown = 0;
            for (std::size_t i = 0; i < rq.size() && shown < 3; ++i) {
                if (rq[i].locationId != selNode_) continue;
                std::string line = rq[i].id + "  " + supplyName(rq[i].supply) + "  " + formatNumber(rq[i].remaining()) +
                                   " left · " + statusName(rq[i].status);
                ui::text(ui::fit(line, w, kSmall), {x, y}, kSmall, kText);
                y += 20;
                ++shown;
            }
            if (shown == 0) ui::text("No requests here.", {x, y}, kSmall, kMuted);
        }
        return;
    }

    const Edge* e = g.edgeBetween(selU_, selV_);
    if (!e) return;
    ui::text(roadTitle(*e), {x, y}, kBody, kText, Weight::SemiBold);
    if (e->blocked) ui::caps("Blocked", {r.x + r.width - kGap - ui::measureCaps("Blocked", kSmall), y + 3}, kSmall, kBlocked);
    y += 24;
    ui::text(ui::fit(g.node(selU_).name + " - " + g.node(selV_).name, w, kSmall), {x, y}, kSmall, kMuted);
    y += 24;
    ui::text(std::string(roadTypeName(e->type)) + " · " + conditionName(e->condition), {x, y}, kSmall, kText);
    y += 20;
    std::string times = "Base " + formatMinutes(e->baseMinutes) + " · now " + formatMinutes(e->weight());
    ui::text(times, {x, y}, kSmall, kText);
    y += 30;
    ui::text(tool_ == Tool::BlockRoad ? "Click again to toggle." : "Block tool (B): click to close / reopen.", {x, y},
             kSmall, kFaint);
}

// ---------------------------------------------------------------- top bar + rail

void App::drawTopBar(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::hline(r.x, r.x + r.width, r.y + r.height - 1, kRule);
    float y = r.y + (r.height - kBody) / 2;
    ui::caps("Flood Relief", {r.x + kGap, y}, kBody, kText);
    float x = r.x + kGap + ui::measureCaps("Flood Relief", kBody) + kGap;
    ui::caps("Pakistan · Monsoon 2022", {x, y + 2}, kSmall, kMuted, Weight::Regular);
    x += ui::measureCaps("Pakistan · Monsoon 2022", kSmall, Weight::Regular) + kGap * 2;

    // live stats "pills": a colour square, a number, a label, hairline between
    if (load_.ok) {
        Stats s = manager_->stats();
        struct Pill {
            const char* label;
            int n;
            Color c;
        } pills[4] = {{"Done", s.fulfilled, kFulfilled},
                      {"Partial", s.partial, kPartial},
                      {"Deferred", s.deferred, kDeferred},
                      {"Pending", s.pending, kPending}};
        for (const Pill& p : pills) {
            ui::vline(x, r.y + 12, r.y + r.height - 12, kRule);
            x += kGap;
            DrawRectangleRec({x, r.y + r.height / 2 - 3, 6, 6}, p.c);
            x += 14;
            std::string n = std::to_string(p.n);
            ui::text(n, {x, y - 1}, kBody, kText, Weight::SemiBold);
            x += ui::measure(n, kBody, Weight::SemiBold) + 6;
            ui::caps(p.label, {x, y + 2}, kSmall, kMuted, Weight::Regular);
            x += ui::measureCaps(p.label, kSmall, Weight::Regular) + kGap;
        }
    }

    // right side: Reset + clock
    std::string clock = formatClock(load_.ok ? manager_->now() : 0);
    float cw = ui::measure(clock, kTitle, Weight::SemiBold);
    ui::text(clock, {r.x + r.width - kGap - cw, y - 3}, kTitle, kText, Weight::SemiBold);
    Rectangle reset{r.x + r.width - kGap * 2 - cw - 76, r.y + 10, 76, 28};
    if (ui::button(reset, "Reset")) loadScenario();
    if (ui::hovered(reset)) {
        tooltip_ = "Reload the scenario from data/pakistan.txt";
        tooltipPos_ = {reset.x - 140, reset.y + 36};
    }
}

void App::drawRail(Rectangle r) {
    DrawRectangleRec(r, kPanel);
    ui::vline(r.x + r.width - 1, r.y, r.y + r.height, kRule);
    if (!load_.ok) return;
    struct Item {
        ui::Icon icon;
        const char* tip;
    } items[5] = {{ui::Icon::Select, "Select  (S)"},
                  {ui::Icon::Block, "Block / reopen a road  (B)"},
                  {ui::Icon::Add, "Add a request  (R)"},
                  {ui::Icon::Next, "Process next request  (N)"},
                  {ui::Icon::All, "Process all  (A)"}};
    float y = r.y + kGap;
    bool pending = manager_->hasPending();
    for (int i = 0; i < 5; ++i) {
        if (i == 3) {
            ui::hline(r.x + 16, r.x + r.width - 16, y + 2, kRule);
            y += kGap;
        }
        Rectangle b{r.x + 6, y, r.width - 12, 44};
        bool active = (i == 0 && tool_ == Tool::Select) || (i == 1 && tool_ == Tool::BlockRoad) ||
                      (i == 2 && tool_ == Tool::AddRequest);
        bool enabled = i < 3 || pending;
        if (ui::iconButton(b, items[i].icon, active, enabled)) {
            if (i == 0) setTool(Tool::Select);
            if (i == 1) setTool(Tool::BlockRoad);
            if (i == 2) setTool(Tool::AddRequest);
            if (i == 3) processNext();
            if (i == 4) processAll();
        }
        if (ui::hovered(b)) {
            tooltip_ = enabled ? items[i].tip : "Queue is empty";
            tooltipPos_ = {r.x + r.width + 6, b.y + 9};
        }
        y += 44 + kPad;
    }
}
