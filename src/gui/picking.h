// this file declares picking -> finding which city or road is under the mouse.
// city: distance to the dot < radius
// road: distance from the mouse to the line segment < tolerance
#pragma once // this prevents multiple inclusions of the header file.

#include "raylib.h"

#include "core/graph.h"
#include "gui/map_view.h"

namespace picking {

// shortest distance from point p to line a-b
float distanceToSegment(Vector2 p, Vector2 a, Vector2 b);

// nearest place or -1
int pickNode(const MapView& map, const Graph& g, Vector2 mouseScreen, float radiusPx, bool allowDepots = true);

// nearest road, u and v are its ends
bool pickRoad(const MapView& map, const Graph& g, Vector2 mouseScreen, float tolerancePx, int& u, int& v);

}
