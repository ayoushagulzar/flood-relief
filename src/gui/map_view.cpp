// map_view.cpp - camera, projection and drawing the graph on the map.
#include "gui/map_view.h"

#include <cmath>
#include <string>

#include "gui/outline.h"
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

// Text with a 1 px white outline, so labels stay readable over tiles and roads.
void haloText(const std::string& s, Vector2 p, float size, Color c, Weight w) {
    const Vector2 offs[4] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    for (const Vector2& o : offs) ui::text(s, add(p, o), size, theme::kHalo, w);
    ui::text(s, p, size, c, w);
}

// Depot: a filled dark square with a white edge.
void depotSquare(Vector2 c, float half, Color col) {
    DrawRectangleRec({c.x - half - 1.5f, c.y - half - 1.5f, half * 2 + 3, half * 2 + 3}, theme::kLand);
    DrawRectangleRec({c.x - half, c.y - half, half * 2, half * 2}, col);
}

void hollowTriangle(Vector2 c, float r, float thick, Color col) {
    Vector2 top{c.x, c.y - r}, left{c.x - r * 0.9f, c.y + r * 0.7f}, right{c.x + r * 0.9f, c.y + r * 0.7f};
    DrawTriangle(top, left, right, theme::kLand);  // counter-clockwise order
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

// Search: fly to a place and zoom in a little (never zoom OUT).
void MapView::focusOn(Vector2 world) {
    float z = fitZoom() * 2.5f;
    if (cam_.zoom < z) cam_.zoom = z;
    cam_.target = world;
    cam_.offset = {rect_.x + rect_.width / 2, rect_.y + rect_.height / 2};
    userMoved_ = true;
    clampCamera();
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

void MapView::update(Rectangle mapRect, bool mouseOverUI, bool keyboard) {
    bool resized = mapRect.width != rect_.width || mapRect.height != rect_.height || mapRect.x != rect_.x ||
                   mapRect.y != rect_.y;
    rect_ = mapRect;
    if (!initialised_) {
        resetView();
        initialised_ = true;
    } else if (resized) {
        // Window maximised/resized: if the user hasn't zoomed or panned yet,
        // fit Pakistan to the new size; otherwise keep their view.
        if (userMoved_) clampCamera();
        else resetView();
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
        userMoved_ = true;
        cam_.target = before;
        cam_.offset = mouse;
        clampCamera();
    }
    if (keyboard && IsKeyPressed(KEY_HOME)) {
        resetView();
        userMoved_ = false;
    }
    if (keyboard && (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))) {
        cam_.zoom = clampf(cam_.zoom * 1.5f, minZoom, kMaxZoom);
        userMoved_ = true;
    }
    if (keyboard && (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))) {
        cam_.zoom = clampf(cam_.zoom / 1.5f, minZoom, kMaxZoom);
        userMoved_ = true;
    }

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
            userMoved_ = true;
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
        userMoved_ = true;
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

    drawOutline();  // offline background; tiles (if any) cover it
    BeginMode2D(cam_);
    drawBasemap();
    EndMode2D();

    drawRoads(g, overlay);
    drawRoutes(g, overlay);
    drawNodes(m, overlay);
    drawLabels(m, overlay);
    drawLegend();
    drawCorner();
    EndScissorMode();
}

void MapView::drawBasemap() { basemap_.draw(cam_, rect_); }

// Pakistan + sea from outline.h: the map still looks like a map with no
// internet. Triangles are projected every frame (about 70 of them: cheap).
// raylib skips triangles given clockwise, so we check each one's winding.
void MapView::drawOutline() {
    auto fill = [&](const LatLon* pts, const int* tris, std::size_t triCount, Color c) {
        for (std::size_t t = 0; t < triCount; ++t) {
            Vector2 a = worldToScreen(latLonToWorld(pts[tris[t * 3]].lat, pts[tris[t * 3]].lon));
            Vector2 b = worldToScreen(latLonToWorld(pts[tris[t * 3 + 1]].lat, pts[tris[t * 3 + 1]].lon));
            Vector2 d = worldToScreen(latLonToWorld(pts[tris[t * 3 + 2]].lat, pts[tris[t * 3 + 2]].lon));
            float cross = (b.x - a.x) * (d.y - a.y) - (b.y - a.y) * (d.x - a.x);
            if (cross < 0) DrawTriangle(a, b, d, c);  // counter-clockwise on screen
            else DrawTriangle(a, d, b, c);
        }
    };
    fill(kSea, kSeaTriangles, sizeof(kSeaTriangles) / sizeof(int) / 3, theme::kSea);
    fill(kBorder, kBorderTriangles, sizeof(kBorderTriangles) / sizeof(int) / 3, theme::kLand);
    const std::size_t n = sizeof(kBorder) / sizeof(LatLon);
    for (std::size_t i = 0; i < n; ++i) {
        const LatLon& p = kBorder[i];
        const LatLon& q = kBorder[(i + 1) % n];
        DrawLineEx(worldToScreen(latLonToWorld(p.lat, p.lon)), worldToScreen(latLonToWorld(q.lat, q.lon)), 1.2f,
                   theme::kBorderLine);
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
            if (isSel) DrawLineEx(a, b, w + 8, Fade(theme::kAccent, 0.30f));
            if (isHover) DrawLineEx(a, b, w + 8, Fade(theme::kText, 0.15f));

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

// The last delivery route(s): a white casing plus a dark dashed line
// whose dashes move from the depot towards the people (offset = time).
void MapView::drawRoutes(const Graph& g, const MapOverlay& o) {
    float t = static_cast<float>(GetTime());
    for (std::size_t r = 0; r < o.routes.size(); ++r) {
        const DynamicArray<int>& path = o.routes[r];
        for (std::size_t i = 1; i < path.size(); ++i) {
            DrawLineEx(nodeScreen(g, path[i - 1]), nodeScreen(g, path[i]), 7, theme::kRouteCasing);
        }
        float phase = -t * 36.0f;  // negative: dashes travel along the path
        phase = std::fmod(phase, 16.0f) + 16.0f;
        for (std::size_t i = 1; i < path.size(); ++i) {
            dashedLine(nodeScreen(g, path[i - 1]), nodeScreen(g, path[i]), 3.2f, 10, 5, phase, theme::kRoute);
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
            DrawCircleV(p, 8 * zoomBoost, Fade(rc, 0.12f));
            DrawRing(p, 8 * zoomBoost - 1.5f, 8 * zoomBoost, 0, 360, 32, rc);
        } else if (state[i] == 1) {
            DrawRing(p, 8 * zoomBoost - 1.2f, 8 * zoomBoost, 0, 360, 32, Fade(theme::kFulfilled, 0.7f));
        }

        if (n.type == NodeType::Depot) {
            depotSquare(p, 4.0f * zoomBoost, theme::kDepot);
        } else if (n.type == NodeType::Shelter) {
            hollowTriangle(p, 5.0f * zoomBoost, 1.4f, theme::kShelter);
        } else {
            DrawCircleV(p, 2.6f * zoomBoost, theme::kCity);
        }

        if (static_cast<int>(i) == o.hoverNode) DrawRing(p, 11 * zoomBoost, 12.5f * zoomBoost, 0, 360, 32, theme::kMuted);
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
    used.pushBack({rect_.x + rect_.width - 240, rect_.y, 240, 70});                    // zoom + home hint
    used.pushBack({rect_.x + rect_.width - 260, rect_.y + rect_.height - 40, 260, 40});  // attribution
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
        if (force) ui::roundRect({box.x - 5, box.y - 2, box.width + 10, box.height + 4}, 5, theme::kOverlay);
        used.pushBack(box);
        haloText(name, {box.x, box.y}, theme::kSmall, c, w);
    };

    // Hovered / selected first, so they always win the space.
    if (o.selectedNode >= 0) place(o.selectedNode, theme::kAccent, Weight::SemiBold, true);
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
            place(id, theme::kLabel, Weight::Regular);
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
    const float rowH = 22, colW = 146;
    return {rect_.x + theme::kGap, rect_.y + theme::kGap, theme::kPad * 2 + colW * 2, theme::kPad * 2 + 24 + rowH * 4};
}

void MapView::drawLegend() {
    const float rowH = 22, colW = 146;
    Rectangle box = legendRect();
    ui::roundRect(box, 8, theme::kOverlay);
    ui::roundBorder(box, 8, theme::kRule);
    ui::text("Legend", {box.x + theme::kPad + 4, box.y + theme::kPad - 1}, theme::kSmall, theme::kText, Weight::SemiBold);

    float y0 = box.y + theme::kPad + 25;
    float phase = 0;
    for (int i = 0; i < 8; ++i) {
        float x = box.x + theme::kPad + 4 + static_cast<float>(i / 4) * colW;
        float y = y0 + static_cast<float>(i % 4) * rowH;
        Vector2 c{x + 10, y + 9};
        const char* label = "";
        switch (i) {
            case 0: DrawLineEx({x, c.y}, {x + 20, c.y}, 3, theme::kRoute); label = "Last route"; break;
            case 1: DrawLineEx({x, c.y}, {x + 20, c.y}, 2.4f, theme::kFlooded); label = "Flooded"; break;
            case 2: DrawLineEx({x, c.y}, {x + 20, c.y}, 2, theme::kDamaged); label = "Damaged"; break;
            case 3: phase = 0; dashedLine({x, c.y}, {x + 20, c.y}, 2, 5, 3, phase, theme::kBlocked); label = "Blocked"; break;
            case 4: DrawCircleV(c, 3, theme::kCity); label = "City"; break;
            case 5: depotSquare(c, 4.0f, theme::kDepot); label = "Depot"; break;
            case 6: hollowTriangle(c, 5, 1.4f, theme::kShelter); label = "Shelter"; break;
            case 7: DrawRing(c, 5.6f, 7, 0, 360, 24, theme::urgencyColor(5)); label = "Urgent request"; break;
        }
        ui::text(label, {x + 28, y}, theme::kSmall, theme::kMuted);
    }
}

void MapView::drawCorner() {
    float right = rect_.x + rect_.width - theme::kGap;
    std::string z = "Zoom " + std::to_string(basemap_.tileZoomFor(cam_.zoom)) + "  ·  Home resets";
    ui::textRight(z, right, rect_.y + theme::kGap, theme::kSmall, theme::kMuted);
    if (basemap_.pendingDownloads() > 0)
        ui::textRight("Loading map tiles...", right, rect_.y + theme::kGap + 21, theme::kSmall, theme::kFaint);

    // Attribution is required by the OSM tile usage policy: always visible.
    std::string attr = "© OpenStreetMap contributors";
    float w = ui::measure(attr, theme::kSmall) + 12;
    Rectangle box{rect_.x + rect_.width - w - theme::kPad, rect_.y + rect_.height - 26 - theme::kPad, w, 26};
    ui::roundRect(box, 6, theme::kOverlay);
    ui::text(attr, {box.x + 6, box.y + 4}, theme::kSmall, theme::kMuted);
}
