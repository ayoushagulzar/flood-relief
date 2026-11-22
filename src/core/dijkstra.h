// dijkstra.h - shortest travel time from one source to every node.
//
// Step-able on purpose: start() / step() / isDone() instead of one big loop.
// run() just calls step() until it returns -1. This costs nothing now and
// lets the v2 GUI show the algorithm one node at a time.
//
// Lazy deletion: instead of "decrease key" (hard in a binary heap) we push
// a NEW (dist, node) item every time a distance improves. Old items stay in
// the heap; when one is popped and it's out of date, we just skip it.
//
// Total cost: O((V + E) log V). Only works because no weight is negative.
// Implementation in dijkstra.cpp.
#pragma once
#include <cstddef>
#include <limits>

#include "core/dynamic_array.h"
#include "core/graph.h"
#include "core/min_heap.h"

class Dijkstra {
public:
    static constexpr double INF = std::numeric_limits<double>::infinity();

    explicit Dijkstra(const Graph& g) : graph_(&g) {}

    // O(V). Reset dist (INF), prev (-1), visited (false) for every node, clear
    // the heap, set dist[source] = 0 and push {0, source}. Throws on bad source.
    void start(int source);
    // O(degree log V). Pop until a non-stale item, relax its edges, return its id.
    // Returns -1 when the heap is empty (finished).
    int step();
    bool isDone() const { return !started_ || heap_.isEmpty(); }
    // O((V + E) log V). Calls step() until it returns -1.
    void run();

    // O(1). INF if unreachable. Throws if start() was never called.
    double distanceTo(int v) const;
    // O(path length). source ... v, or EMPTY if v is unreachable.
    DynamicArray<int> pathTo(int v) const;
    bool reachable(int v) const { return distanceTo(v) != INF; }
    std::size_t visitedCount() const { return visitedCount_; }
    bool isVisited(int v) const;
    int source() const { return source_; }

private:
    struct Item {
        double dist;
        int node;
    };
    struct CloserFirst {
        bool operator()(const Item& a, const Item& b) const { return a.dist < b.dist; }
    };

    const Graph* graph_;
    DynamicArray<double> dist_;
    DynamicArray<int> prev_;
    DynamicArray<bool> visited_;
    MinHeap<Item, CloserFirst> heap_;
    std::size_t visitedCount_ = 0;
    int source_ = -1;
    bool started_ = false;
};
