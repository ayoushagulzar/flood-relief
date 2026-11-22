// picking.h - what is the mouse over on the map?
//
// Both tests work in WORLD coordinates, so they stay correct at any zoom:
//   mouseWorld = GetScreenToWorld2D(GetMousePosition(), cam)
// A tolerance of "10 screen pixels" becomes 10 / cam.zoom world units.
//
// City:  hovered if distance(mouseWorld, cityPos) < radius / zoom
// Road:  project P onto segment AB:
//          t = clamp(dot(P - A, B - A) / |B - A|^2, 0, 1)
//          closest = A + t (B - A)
//        hovered if distance(P, closest) < tolerance / zoom
#pragma once

#include "raylib.h"

#include "core/graph.h"
#include "gui/map_view.h"

namespace picking {

// Distance from p to the segment a-b (any units). O(1).
float distanceToSegment(Vector2 p, Vector2 a, Vector2 b);

// Nearest node within radiusPx screen pixels, or -1. O(V).
// allowDepots = false skips depots (the Add tool can't target a depot).
int pickNode(const MapView& map, const Graph& g, Vector2 mouseScreen, float radiusPx, bool allowDepots = true);

// Nearest road within tolerancePx screen pixels. O(V + E).
// Returns false if none; otherwise u, v are its two ends.
bool pickRoad(const MapView& map, const Graph& g, Vector2 mouseScreen, float tolerancePx, int& u, int& v);

}  // namespace picking
