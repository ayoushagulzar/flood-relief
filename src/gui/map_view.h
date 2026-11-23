// this file declares MapView -> the map: camera, zoom, pan and drawing the graph.
// world = map pixels at zoom 6, screen = window pixels. Camera2D converts between them
#pragma once // this prevents multiple inclusions of the header file.

#include "raylib.h"

#include "core/dynamic_array.h"
#include "gui/basemap.h"
#include "relief/relief_manager.h"

// things to highlight on the map this frame (App fills it)
struct MapOverlay {
    int hoverNode = -1;
    int hoverRoadU = -1, hoverRoadV = -1;
    int selectedNode = -1;
    int selectedRoadU = -1, selectedRoadV = -1;
    DynamicArray<DynamicArray<int>> routes; // paths of the last delivery
    int pulseNode = -1; // place that was just served
    float pulseAge = 99.0f;
    bool roadMode = false; // true when Close road tool is on
};

class MapView {
public:
    static constexpr double kMinLat = 23.4, kMaxLat = 37.3, kMinLon = 60.6, kMaxLon = 77.9; // Pakistan box

    void init();
    void unload();

    static Vector2 latLonToWorld(double lat, double lon); // lat/lon -> map pixels (Web Mercator)
    Vector2 worldToScreen(Vector2 w) const { return GetWorldToScreen2D(w, cam_); }
    Vector2 screenToWorld(Vector2 s) const { return GetScreenToWorld2D(s, cam_); }
    Vector2 nodeScreen(const Graph& g, int id) const;

    void update(Rectangle mapRect, bool mouseOverUI, bool keyboard = true); // zoom with wheel, drag to move, Home resets
    void resetView(); // show all of Pakistan
    void focusOn(Vector2 world); // zoom to a place (search)

    bool clicked() const { return clicked_; } // true if clicked without dragging
    bool isDragging() const { return dragging_; }
    float zoom() const { return cam_.zoom; }
    const Camera2D& camera() const { return cam_; }
    Rectangle rect() const { return rect_; }

    void draw(const ReliefManager& m, const MapOverlay& overlay); // draws everything

private:
    void drawOutline(); // offline Pakistan shape
    void drawBasemap(); // OSM tiles
    void drawRoads(const Graph& g, const MapOverlay& o);
    void drawRoutes(const Graph& g, const MapOverlay& o);
    void drawNodes(const ReliefManager& m, const MapOverlay& o);
    void drawLabels(const ReliefManager& m, const MapOverlay& o);
    void drawLegend();
    Rectangle legendRect() const;
    void drawCorner(); // zoom text + OSM credit
    void clampCamera(); // don't let the map go too far away
    float fitZoom() const; // zoom where Pakistan fits

    Camera2D cam_{}; // raylib camera (offset, target, zoom)
    Rectangle rect_{0, 0, 0, 0};
    Basemap basemap_;
    bool initialised_ = false;
    bool pressedInMap_ = false;
    bool dragging_ = false;
    bool clicked_ = false;
    bool userMoved_ = false; // zoomed or moved since reset?
    Vector2 pressPos_{0, 0}; // to tell click and drag apart
};
