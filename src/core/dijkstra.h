// this file declares Dijkstra -> shortest travel time from one place to all other places.
// it uses lazy deletion: when a distance gets better we just push a new item in the heap,
// and old items are skipped when they come out. total O((V + E) log V)

#pragma once // this prevents multiple inclusions of the header file.

#include <cstddef>
#include <limits> // for infinity

#include "core/dynamic_array.h"
#include "core/graph.h"
#include "core/min_heap.h"

class Dijkstra {
public:
    // INF means not reachable
    static constexpr double INF = std::numeric_limits<double>::infinity();

    explicit Dijkstra(const Graph& g) : graph_(&g) {}

    // can run step by step: start() then step() until it returns -1
    void start(int source); // reset everything, dist[source] = 0
    int step();             // settles one node and returns it, -1 when finished
    bool isDone() const { return !started_ || heap_.isEmpty(); }
    void run();             // calls step() until finished

    double distanceTo(int v) const;            // INF if unreachable
    DynamicArray<int> pathTo(int v) const;     // source ... v (empty if unreachable)
    bool reachable(int v) const { return distanceTo(v) != INF; }
    std::size_t visitedCount() const { return visitedCount_; }
    bool isVisited(int v) const;
    int source() const { return source_; }

private:
    // one item in the heap
    struct Item {
        double dist;
        int node;
    };
    // smaller distance comes out first
    struct CloserFirst {
        bool operator()(const Item& a, const Item& b) const { return a.dist < b.dist; }
    };

    const Graph* graph_;
    DynamicArray<double> dist_;        // best distance found so far
    DynamicArray<int> prev_;           // previous node on the path (to make the path later)
    DynamicArray<bool> visited_;       // true = distance is final
    MinHeap<Item, CloserFirst> heap_;
    std::size_t visitedCount_ = 0;
    int source_ = -1;
    bool started_ = false;
};
