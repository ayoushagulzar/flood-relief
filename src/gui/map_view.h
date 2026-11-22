// map_view.h - the map: projection, camera, basemap and the graph on top.
//
// World vs screen:
//   world  = Web Mercator pixels at the reference zoom 6 (Pakistan is ~780 x 730)
//   screen = window pixels. Camera2D (offset, target, zoom) maps world -> screen.
// Tiles are drawn inside BeginMode2D(cam) (they live in world space).
// Roads, symbols and labels are drawn in SCREEN space after projecting each
// point with GetWorldToScreen2D: that keeps lines 1-3 px thick and text
// crisp at every zoom instead of growing with the camera.
// To find what the mouse is over: GetScreenToWorld2D(GetMousePosition(), cam).
#pragma once

#include "raylib.h"

#include "core/dynamic_array.h"
#include "gui/basemap.h"
#include "relief/relief_manager.h"

// Extra things to highlight on top of the graph. Phase 7 passes an empty one;
// Phase 8 fills it from the mouse (hover/selection) and the last Decision.
struct MapOverlay {
    int hoverNode = -1;
    int hoverRoadU = -1, hoverRoadV = -1;
    int selectedNode = -1;
    int selectedRoadU = -1, selectedRoadV = -1;
    DynamicArray<DynamicArray<int>> routes;  // node paths to draw as the route
    int pulseNode = -1;                      // node that just got served
    float pulseAge = 99.0f;                  // seconds since the pulse started
    bool roadMode = false;                   // Block tool: roads get easier to see
};

class MapView {
public:
    // Pakistan's bounding box (with a little margin).
    static constexpr double kMinLat = 23.4, kMaxLat = 37.3, kMinLon = 60.6, kMaxLon = 77.9;

    void init();    // after InitWindow
    void unload();  // before CloseWindow

    // Projection. Pure maths, O(1).
    static Vector2 latLonToWorld(double lat, double lon);
    Vector2 worldToScreen(Vector2 w) const { return GetWorldToScreen2D(w, cam_); }
    Vector2 screenToWorld(Vector2 s) const { return GetScreenToWorld2D(s, cam_); }
    Vector2 nodeScreen(const Graph& g, int id) const;

    // Input: wheel zoom towards the cursor, drag to pan, Home resets.
    // mouseOverUI = the mouse is on a panel, so the map must ignore it.
    void update(Rectangle mapRect, bool mouseOverUI);
    void resetView();

    // True for one frame when the left button was released WITHOUT dragging:
    // a real click on the map (Phase 8 uses this for picking).
    bool clicked() const { return clicked_; }
    bool isDragging() const { return dragging_; }
    float zoom() const { return cam_.zoom; }
    const Camera2D& camera() const { return cam_; }
    Rectangle rect() const { return rect_; }

    void draw(const ReliefManager& m, const MapOverlay& overlay);

private:
    void drawBasemap();  // tiles (or a plain grid if offline)
    void drawGraticule();
    void drawRoads(const Graph& g, const MapOverlay& o);
    void drawRoutes(const Graph& g, const MapOverlay& o);
    void drawNodes(const ReliefManager& m, const MapOverlay& o);
    void drawLabels(const ReliefManager& m, const MapOverlay& o);
    void drawLegend();
    Rectangle legendRect() const;
    void drawCorner();  // zoom, home hint, attribution
    void clampCamera();
    float fitZoom() const;

    Camera2D cam_{};
    Rectangle rect_{0, 0, 0, 0};
    Basemap basemap_;
    bool initialised_ = false;
    bool pressedInMap_ = false;
    bool dragging_ = false;
    bool clicked_ = false;
    bool userMoved_ = false;  // zoomed or panned since the last reset?
    Vector2 pressPos_{0, 0};
};
