// map_view.cpp - camera, projection and drawing the graph on the map.
#include "gui/map_view.h"

#include <cmath>
#include <string>

#include "gui/text.h"
#include "gui/theme.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr float kMaxZoom = 16.0f;  // camera zoom 16 = tile zoom 10

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

Vector2 add(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
Vector2 sub(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
Vector2 scale(Vector2 a, float s) { return {a.x * s, a.y * s}; }
float length(Vector2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }

// Dashed line from a to b. `phase` (pixels) lets dashes continue smoothly
// across the segments of a polyline and lets us animate them over time.
void dashedLine(Vector2 a, Vector2 b, float width, float dash, float gap, float& phase, Color c) {
    float len = length(sub(b, a));
    if (len < 0.01f) return;
    Vector2 dir = scale(sub(b, a), 1.0f / len);
    float period = dash + gap;
    float pos = -std::fmod(phase, period);
    while (pos < len) {
        float s = pos < 0 ? 0 : pos;
        float e = pos + dash > len ? len : pos + dash;
        if (e > s) DrawLineEx(add(a, scale(dir, s)), add(a, scale(dir, e)), width, c);
        pos += period;
    }
    phase += len;
}

bool segmentVisible(Vector2 a, Vector2 b, Rectangle r) {
    float minX = a.x < b.x ? a.x : b.x, maxX = a.x < b.x ? b.x : a.x;
    float minY = a.y < b.y ? a.y : b.y, maxY = a.y < b.y ? b.y : a.y;
    return maxX >= r.x && minX <= r.x + r.width && maxY >= r.y && minY <= r.y + r.height;
}

// Text with a 1 px black outline, so labels stay readable over tiles and roads.
void haloText(const std::string& s, Vector2 p, float size, Color c, Weight w) {
    const Vector2 offs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const Vector2& o : offs) ui::text(s, add(p, o), size, theme::kHalo, w);
    ui::text(s, p, size, c, w);
}

void hollowSquare(Vector2 c, float half, float thick, Color col) {
    DrawRectangleRec({c.x - half, c.y - half, half * 2, half * 2}, theme::kMapBackground);
    DrawRectangleLinesEx({c.x - half, c.y - half, half * 2, half * 2}, thick, col);
}

void hollowTriangle(Vector2 c, float r, float thick, Color col) {
    Vector2 top{c.x, c.y - r}, left{c.x - r * 0.9f, c.y + r * 0.7f}, right{c.x + r * 0.9f, c.y + r * 0.7f};
    DrawTriangle(top, left, right, theme::kMapBackground);  // counter-clockwise order
    DrawLineEx(top, left, thick, col);
    DrawLineEx(left, right, thick, col);
    DrawLineEx(right, top, thick, col);
}

bool overlapsAny(const DynamicArray<Rectangle>& used, Rectangle r) {
    for (std::size_t i = 0; i < used.size(); ++i) {
        if (CheckCollisionRecs(used[i], r)) return true;
    }
    return false;
}

std::string withoutDepotWord(const std::string& name) {
    const std::string suffix = " Depot";
    if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        return name.substr(0, name.size() - suffix.size());
    return name;
}

}  // namespace

// ---------------------------------------------------------------- projection

Vector2 MapView::latLonToWorld(double lat, double lon) {
    // Web Mercator at the reference zoom: world size = 256 x 2^6 pixels.
    const double size = 256.0 * static_cast<double>(1 << Basemap::kRefZoom);
    double phi = lat * kPi / 180.0;
    double x = (lon + 180.0) / 360.0 * size;
    double y = (1.0 - std::log(std::tan(phi) + 1.0 / std::cos(phi)) / kPi) / 2.0 * size;
    return {static_cast<float>(x), static_cast<float>(y)};
}

Vector2 MapView::nodeScreen(const Graph& g, int id) const {
    const Node& n = g.node(id);
    return worldToScreen(latLonToWorld(n.lat, n.lon));
}

// ---------------------------------------------------------------- camera

void MapView::init() { basemap_.init(); }
void MapView::unload() { basemap_.unload(); }

float MapView::fitZoom() const {
    Vector2 tl = latLonToWorld(kMaxLat, kMinLon), br = latLonToWorld(kMinLat, kMaxLon);
    float zx = rect_.width / (br.x - tl.x), zy = rect_.height / (br.y - tl.y);
    return (zx < zy ? zx : zy) * 0.96f;
}

void MapView::resetView() {
    Vector2 tl = latLonToWorld(kMaxLat, kMinLon), br = latLonToWorld(kMinLat, kMaxLon);
    cam_.zoom = fitZoom();
    cam_.rotation = 0;
    cam_.target = {(tl.x + br.x) / 2, (tl.y + br.y) / 2};
    cam_.offset = {rect_.x + rect_.width / 2, rect_.y + rect_.height / 2};
}

// Keeps the view centred on Pakistan: the world point in the middle of the
// map may not leave Pakistan's bounding box.
void MapView::clampCamera() {
    Vector2 centre{rect_.x + rect_.width / 2, rect_.y + rect_.height / 2};
    Vector2 w = GetScreenToWorld2D(centre, cam_);
    Vector2 tl = latLonToWorld(kMaxLat, kMinLon), br = latLonToWorld(kMinLat, kMaxLon);
    w.x = clampf(w.x, tl.x, br.x);
    w.y = clampf(w.y, tl.y, br.y);
    cam_.target = w;
    cam_.offset = centre;
}

void MapView::update(Rectangle mapRect, bool mouseOverUI) {
    bool resized = mapRect.width != rect_.width || mapRect.height != rect_.height || mapRect.x != rect_.x ||
                   mapRect.y != rect_.y;
    rect_ = mapRect;
    if (!initialised_) {
        resetView();
        initialised_ = true;
    } else if (resized) {
        clampCamera();
    }

    clicked_ = false;
    Vector2 mouse = GetMousePosition();
    bool inMap = CheckCollisionPointRec(mouse, rect_) && !mouseOverUI;
    float minZoom = fitZoom() * 0.75f;

    // Zoom towards the cursor: remember the world point under the mouse,
    // change the zoom, then put that same point back under the mouse.
    float wheel = GetMouseWheelMove();
    if (inMap && wheel != 0) {
        Vector2 before = GetScreenToWorld2D(mouse, cam_);
        cam_.zoom = clampf(cam_.zoom * std::pow(1.2f, wheel), minZoom, kMaxZoom);
        cam_.target = before;
        cam_.offset = mouse;
        clampCamera();
    }
    if (IsKeyPressed(KEY_HOME)) resetView();
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) cam_.zoom = clampf(cam_.zoom * 1.5f, minZoom, kMaxZoom);
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
        cam_.zoom = clampf(cam_.zoom / 1.5f, minZoom, kMaxZoom);

    // Drag to pan (left, right or middle button). A left press that never
    // moves more than 4 px is a CLICK instead (used for picking).
    if (inMap && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        pressedInMap_ = true;
        dragging_ = false;
        pressPos_ = mouse;
    }
    if (pressedInMap_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        if (!dragging_ && length(sub(mouse, pressPos_)) > 4.0f) {
            dragging_ = true;
            cam_.target = sub(cam_.target, scale(sub(mouse, pressPos_), 1.0f / cam_.zoom));
        } else if (dragging_) {
            cam_.target = sub(cam_.target, scale(GetMouseDelta(), 1.0f / cam_.zoom));
        }
    }
    if (pressedInMap_ && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        clicked_ = !dragging_;
        pressedInMap_ = false;
        dragging_ = false;
    }
    if (inMap && (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))) {
        cam_.target = sub(cam_.target, scale(GetMouseDelta(), 1.0f / cam_.zoom));
    }
    clampCamera();
}

// ---------------------------------------------------------------- drawing

void MapView::draw(const ReliefManager& m, const MapOverlay& overlay) {
    const Graph& g = m.graph();
    BeginScissorMode(static_cast<int>(rect_.x), static_cast<int>(rect_.y), static_cast<int>(rect_.width),
                     static_cast<int>(rect_.height));
    DrawRectangleRec(rect_, theme::kMapBackground);

    BeginMode2D(cam_);
    drawBasemap();
    EndMode2D();

    drawGraticule();
    drawRoads(g, overlay);
    drawRoutes(g, overlay);
    drawNodes(m, overlay);
    drawLabels(m, overlay);
    drawLegend();
    drawCorner();
    EndScissorMode();
}

void MapView::drawBasemap() { basemap_.draw(cam_, rect_); }

// Latitude/longitude lines every 2 degrees: the "chart" feel, and they keep
// the map readable even when the tiles are offline.
void MapView::drawGraticule() {
    for (int lon = 60; lon <= 78; lon += 2) {
        Vector2 a = worldToScreen(latLonToWorld(20.0, lon)), b = worldToScreen(latLonToWorld(40.0, lon));
        DrawLineEx(a, b, 1, theme::kGraticule);
        if (a.x > rect_.x + 40 && a.x < rect_.x + rect_.width - 60)
            ui::text(std::to_string(lon) + "°E", {a.x + 4, rect_.y + theme::kPad}, theme::kSmall, theme::kFaint);
    }
    for (int lat = 22; lat <= 38; lat += 2) {
        Vector2 a = worldToScreen(latLonToWorld(lat, 58.0)), b = worldToScreen(latLonToWorld(lat, 80.0));
        DrawLineEx(a, b, 1, theme::kGraticule);
        if (a.y > rect_.y + 30 && a.y < rect_.y + rect_.height - 30)
            ui::text(std::to_string(lat) + "°N", {rect_.x + theme::kPad, a.y - 16}, theme::kSmall, theme::kFaint);
    }
}

void MapView::drawRoads(const Graph& g, const MapOverlay& o) {
    float zoomBoost = clampf(std::sqrt(cam_.zoom / fitZoom()), 1.0f, 2.6f);  // wider roads when zoomed in
    float phase = 0;
    for (std::size_t u = 0; u < g.nodeCount(); ++u) {
        const DynamicArray<Edge>& out = g.neighbors(static_cast<int>(u));
        for (std::size_t k = 0; k < out.size(); ++k) {
            const Edge& e = out[k];
            if (e.to < static_cast<int>(u)) continue;  // each two-way road once
            Vector2 a = nodeScreen(g, static_cast<int>(u)), b = nodeScreen(g, e.to);
            if (!segmentVisible(a, b, rect_)) continue;

            float w = (e.type == RoadType::Motorway ? 2.0f : e.type == RoadType::Highway ? 1.6f : 1.2f) * zoomBoost;
            bool isHover = (o.hoverRoadU == static_cast<int>(u) && o.hoverRoadV == e.to) ||
                           (o.hoverRoadV == static_cast<int>(u) && o.hoverRoadU == e.to);
            bool isSel = (o.selectedRoadU == static_cast<int>(u) && o.selectedRoadV == e.to) ||
                         (o.selectedRoadV == static_cast<int>(u) && o.selectedRoadU == e.to);
            if (isSel) DrawLineEx(a, b, w + 8, Fade(theme::kAccent, 0.35f));
            if (isHover) DrawLineEx(a, b, w + 8, Fade(theme::kText, 0.22f));

            if (e.blocked) {
                phase = 0;
                dashedLine(a, b, w + 0.4f, 6, 5, phase, theme::kBlocked);
                Vector2 mid = scale(add(a, b), 0.5f);  // small X: "closed"
                DrawLineEx({mid.x - 4, mid.y - 4}, {mid.x + 4, mid.y + 4}, 2, theme::kBlocked);
                DrawLineEx({mid.x + 4, mid.y - 4}, {mid.x - 4, mid.y + 4}, 2, theme::kBlocked);
            } else if (e.condition == RoadCondition::Flooded) {
                DrawLineEx(a, b, w + 0.4f, theme::kFlooded);
            } else if (e.condition == RoadCondition::Damaged) {
                DrawLineEx(a, b, w, theme::kDamaged);
            } else {
                Color c = e.type == RoadType::Motorway ? theme::kMotorway : theme::kRoad;
                if (o.roadMode) c = theme::kMuted;  // Block tool: roads stand out
                DrawLineEx(a, b, w, c);
            }
        }
    }
}

// The last delivery route(s): a soft wide glow plus a bright dashed line
// whose dashes move from the depot towards the people (offset = time).
void MapView::drawRoutes(const Graph& g, const MapOverlay& o) {
    float t = static_cast<float>(GetTime());
    for (std::size_t r = 0; r < o.routes.size(); ++r) {
        const DynamicArray<int>& path = o.routes[r];
        for (std::size_t i = 1; i < path.size(); ++i) {
            DrawLineEx(nodeScreen(g, path[i - 1]), nodeScreen(g, path[i]), 9, Fade(theme::kRoute, 0.16f));
        }
        float phase = -t * 36.0f;  // negative: dashes travel along the path
        phase = std::fmod(phase, 16.0f) + 16.0f;
        for (std::size_t i = 1; i < path.size(); ++i) {
            dashedLine(nodeScreen(g, path[i - 1]), nodeScreen(g, path[i]), 3.0f, 10, 6, phase, theme::kRoute);
        }
    }
}

void MapView::drawNodes(const ReliefManager& m, const MapOverlay& o) {
    const Graph& g = m.graph();
    float zoomBoost = clampf(std::sqrt(cam_.zoom / fitZoom()), 1.0f, 2.0f);

    // Most urgent OPEN request per node (0 = none), and whether a node has
    // only fulfilled requests. O(requests) per frame: 28 items, trivial.
    DynamicArray<int> urgent;
    DynamicArray<int> state;  // 0 nothing, 1 all done, 2 open, 3 deferred
    for (std::size_t i = 0; i < g.nodeCount(); ++i) {
        urgent.pushBack(0);
        state.pushBack(0);
    }
    const DynamicArray<Request>& reqs = m.requests();
    for (std::size_t i = 0; i < reqs.size(); ++i) {
        const Request& r = reqs[i];
        std::size_t n = static_cast<std::size_t>(r.locationId);
        if (r.status == Status::Fulfilled) {
            if (state[n] == 0) state[n] = 1;
            continue;
        }
        if (r.urgency > urgent[n]) urgent[n] = r.urgency;
        if (r.status == Status::Deferred) state[n] = 3;
        else if (state[n] < 2) state[n] = 2;
    }

    for (std::size_t i = 0; i < g.nodeCount(); ++i) {
        const Node& n = g.node(static_cast<int>(i));
        Vector2 p = nodeScreen(g, static_cast<int>(i));
        if (!CheckCollisionPointRec(p, {rect_.x - 20, rect_.y - 20, rect_.width + 40, rect_.height + 40})) continue;

        // ring for open requests: colour = urgency (red-orange if deferred)
        if (state[i] >= 2) {
            Color rc = state[i] == 3 ? theme::kDeferred : theme::urgencyColor(urgent[i]);
            DrawRing(p, 8 * zoomBoost - 1.4f, 8 * zoomBoost, 0, 360, 32, rc);
        } else if (state[i] == 1) {
            DrawRing(p, 8 * zoomBoost - 1.2f, 8 * zoomBoost, 0, 360, 32, Fade(theme::kFulfilled, 0.7f));
        }

        if (n.type == NodeType::Depot) {
            hollowSquare(p, 4.5f * zoomBoost, 1.6f, theme::kDepot);
        } else if (n.type == NodeType::Shelter) {
            hollowTriangle(p, 5.0f * zoomBoost, 1.4f, theme::kShelter);
        } else {
            DrawCircleV(p, 2.6f * zoomBoost, theme::kCity);
        }

        if (static_cast<int>(i) == o.hoverNode) DrawRing(p, 11 * zoomBoost, 12.5f * zoomBoost, 0, 360, 32, theme::kText);
        if (static_cast<int>(i) == o.selectedNode)
            DrawRing(p, 11 * zoomBoost, 13 * zoomBoost, 0, 360, 32, theme::kAccent);
    }

    // one-shot pulse on the city that was just served
    if (o.pulseNode >= 0 && o.pulseAge < 1.2f) {
        Vector2 p = nodeScreen(g, o.pulseNode);
        float k = o.pulseAge / 1.2f;
        float r = 8 + 28 * k;
        DrawRing(p, r - 2, r, 0, 360, 48, Fade(theme::kFulfilled, 1.0f - k));
    }
}

void MapView::drawLabels(const ReliefManager& m, const MapOverlay& o) {
    const Graph& g = m.graph();
    bool detailed = cam_.zoom >= fitZoom() * theme::kLabelZoom;

    // Greedy label placement: a label is only drawn if its box hits nothing
    // already "used". Symbols, the legend and the corner texts count as used,
    // so labels never cover them.
    DynamicArray<Rectangle> used;
    used.pushBack(legendRect());
    used.pushBack({rect_.x + rect_.width - 180, rect_.y, 180, 70});                    // zoom + home hint
    used.pushBack({rect_.x + rect_.width - 220, rect_.y + rect_.height - 34, 220, 34});  // attribution
    for (std::size_t i = 0; i < g.nodeCount(); ++i) {
        Vector2 p = nodeScreen(g, static_cast<int>(i));
        used.pushBack({p.x - 6, p.y - 6, 12, 12});
    }

    // force = hovered/selected: always drawn, even if it overlaps something.
    auto place = [&](int id, Color c, Weight w, bool force = false) {
        const Node& n = g.node(id);
        std::string name = n.type == NodeType::Depot ? withoutDepotWord(n.name) : n.name;
        Vector2 p = nodeScreen(g, id);
        float tw = ui::measure(name, theme::kSmall, w);
        Rectangle box{p.x + 10, p.y - 8, tw, theme::kSmall + 2};
        if (!force && overlapsAny(used, box)) {  // try the left side
            box.x = p.x - 10 - tw;
            if (overlapsAny(used, box)) return;
        }
        if (force) DrawRectangleRec({box.x - 3, box.y - 1, box.width + 6, box.height + 2}, theme::kOverlay);
        used.pushBack(box);
        haloText(name, {box.x, box.y}, theme::kSmall, c, w);
    };

    // Hovered / selected first, so they always win the space.
    if (o.selectedNode >= 0) place(o.selectedNode, theme::kText, Weight::SemiBold, true);
    if (o.hoverNode >= 0 && o.hoverNode != o.selectedNode) place(o.hoverNode, theme::kText, Weight::SemiBold, true);

    for (std::size_t i = 0; i < g.nodeCount(); ++i) {  // depots always (they are the "big" places)
        int id = static_cast<int>(i);
        if (g.node(id).type == NodeType::Depot && id != o.hoverNode && id != o.selectedNode)
            place(id, theme::kText, Weight::SemiBold);
    }
    if (!detailed) return;
    for (std::size_t i = 0; i < g.nodeCount(); ++i) {  // then shelters, then cities
        int id = static_cast<int>(i);
        if (g.node(id).type == NodeType::Shelter && id != o.hoverNode && id != o.selectedNode)
            place(id, theme::kText, Weight::Regular);
    }
    for (std::size_t i = 0; i < g.nodeCount(); ++i) {
        int id = static_cast<int>(i);
        const Node& n = g.node(id);
        if (n.type != NodeType::City || id == o.hoverNode || id == o.selectedNode) continue;
        // "Sukkur" is already written next to Sukkur Depot: don't repeat it.
        // (findNode is an O(1) HashTable lookup.)
        if (g.findNode(n.name + " Depot") >= 0) continue;
        place(id, theme::kMuted, Weight::Regular);
    }
}

// Top-left: that corner of the map is Afghanistan, so it hides no node.
Rectangle MapView::legendRect() const {
    const float rowH = 18, colW = 112;
    return {rect_.x + theme::kGap + 40, rect_.y + theme::kGap + 24, theme::kPad * 2 + colW * 2,
            theme::kPad * 2 + 18 + rowH * 4};
}

void MapView::drawLegend() {
    const float rowH = 18, colW = 112;
    Rectangle box = legendRect();
    DrawRectangleRec(box, theme::kOverlay);
    DrawRectangleLinesEx(box, 1, theme::kRule);
    ui::caps("Legend", {box.x + theme::kPad + 4, box.y + theme::kPad}, theme::kSmall, theme::kMuted);

    float y0 = box.y + theme::kPad + 20;
    float phase = 0;
    for (int i = 0; i < 8; ++i) {
        float x = box.x + theme::kPad + 4 + static_cast<float>(i / 4) * colW;
        float y = y0 + static_cast<float>(i % 4) * rowH;
        Vector2 c{x + 10, y + 7};
        const char* label = "";
        switch (i) {
            case 0: DrawLineEx({x, c.y}, {x + 20, c.y}, 3, theme::kRoute); label = "Last route"; break;
            case 1: DrawLineEx({x, c.y}, {x + 20, c.y}, 2.4f, theme::kFlooded); label = "Flooded"; break;
            case 2: DrawLineEx({x, c.y}, {x + 20, c.y}, 2, theme::kDamaged); label = "Damaged"; break;
            case 3: phase = 0; dashedLine({x, c.y}, {x + 20, c.y}, 2, 5, 3, phase, theme::kBlocked); label = "Blocked"; break;
            case 4: DrawCircleV(c, 3, theme::kCity); label = "City"; break;
            case 5: hollowSquare(c, 4.5f, 1.6f, theme::kDepot); label = "Depot"; break;
            case 6: hollowTriangle(c, 5, 1.4f, theme::kShelter); label = "Shelter"; break;
            case 7: DrawRing(c, 5.6f, 7, 0, 360, 24, theme::urgencyColor(5)); label = "Open request"; break;
        }
        ui::text(label, {x + 28, y}, theme::kSmall, theme::kText);
    }
}

void MapView::drawCorner() {
    float right = rect_.x + rect_.width - theme::kGap;
    std::string z = "Z " + std::to_string(basemap_.tileZoomFor(cam_.zoom));
    ui::caps(z, {right - ui::measureCaps(z, theme::kSmall), rect_.y + theme::kGap}, theme::kSmall, theme::kMuted);
    std::string home = "Home: reset view";
    ui::textRight(home, right, rect_.y + theme::kGap + 18, theme::kSmall, theme::kFaint);
    if (basemap_.pendingDownloads() > 0)
        ui::textRight("Loading map tiles...", right, rect_.y + theme::kGap + 36, theme::kSmall, theme::kFaint);

    // Attribution is required by the OSM tile usage policy: always visible.
    std::string attr = "© OpenStreetMap contributors";
    float w = ui::measure(attr, theme::kSmall) + 12;
    Rectangle box{rect_.x + rect_.width - w - theme::kPad, rect_.y + rect_.height - 22 - theme::kPad, w, 22};
    DrawRectangleRec(box, theme::kOverlay);
    ui::text(attr, {box.x + 6, box.y + 4}, theme::kSmall, theme::kMuted);
}
