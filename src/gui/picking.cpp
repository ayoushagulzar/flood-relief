// this file has the code of picking (what is under the mouse)
#include "gui/picking.h"

#include <cmath>

namespace picking {

// project p on the line a-b, t is clamped between 0 and 1 so the point stays on the segment
float distanceToSegment(Vector2 p, Vector2 a, Vector2 b) {
    float abx = b.x - a.x, aby = b.y - a.y;
    float apx = p.x - a.x, apy = p.y - a.y;
    float len2 = abx * abx + aby * aby;
    float t = len2 > 0 ? (apx * abx + apy * aby) / len2 : 0;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float cx = a.x + t * abx, cy = a.y + t * aby;
    return std::sqrt((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy));
}

// check all places, keep the closest one inside the radius
int pickNode(const MapView& map, const Graph& g, Vector2 mouseScreen, float radiusPx, bool allowDepots) {
    Vector2 m = map.screenToWorld(mouseScreen);
    float limit = radiusPx / map.zoom();
    int best = -1;
    float bestD = limit;
    for (std::size_t i = 0; i < g.nodeCount(); ++i) {
        const Node& n = g.node(static_cast<int>(i));
        if (!allowDepots && n.type == NodeType::Depot) continue;
        Vector2 w = MapView::latLonToWorld(n.lat, n.lon);
        float d = std::sqrt((m.x - w.x) * (m.x - w.x) + (m.y - w.y) * (m.y - w.y));
        if (d < bestD) {
            bestD = d;
            best = static_cast<int>(i);
        }
    }
    return best;
}

// check all roads, keep the closest one
bool pickRoad(const MapView& map, const Graph& g, Vector2 mouseScreen, float tolerancePx, int& u, int& v) {
    Vector2 m = map.screenToWorld(mouseScreen);
    float bestD = tolerancePx / map.zoom();
    bool found = false;
    for (std::size_t a = 0; a < g.nodeCount(); ++a) {
        const Node& na = g.node(static_cast<int>(a));
        Vector2 wa = MapView::latLonToWorld(na.lat, na.lon);
        const DynamicArray<Edge>& out = g.neighbors(static_cast<int>(a));
        for (std::size_t k = 0; k < out.size(); ++k) {
            if (out[k].to < static_cast<int>(a)) continue;
            const Node& nb = g.node(out[k].to);
            float d = distanceToSegment(m, wa, MapView::latLonToWorld(nb.lat, nb.lon));
            if (d < bestD) {
                bestD = d;
                u = static_cast<int>(a);
                v = out[k].to;
                found = true;
            }
        }
    }
    return found;
}

}
