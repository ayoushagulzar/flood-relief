// dijkstra.cpp - shortest travel times with lazy deletion (see dijkstra.h).
#include "core/dijkstra.h"

#include <stdexcept>

// O(V). Resets every array so the same object can be reused for a new source.
void Dijkstra::start(int source) {
    if (!graph_->isValid(source)) throw std::out_of_range("Dijkstra: invalid source");
    const std::size_t n = graph_->nodeCount();
    dist_.clear();
    prev_.clear();
    visited_.clear();
    heap_.clear();
    for (std::size_t i = 0; i < n; ++i) {
        dist_.pushBack(INF);
        prev_.pushBack(-1);
        visited_.pushBack(false);
    }
    visitedCount_ = 0;
    source_ = source;
    started_ = true;
    dist_[static_cast<std::size_t>(source)] = 0.0;
    heap_.push({0.0, source});
}

// O(degree(u) log V) for the node it settles. Returns that node, or -1 when
// the heap is empty (everything reachable is settled).
int Dijkstra::step() {
    while (!heap_.isEmpty()) {
        Item it = heap_.pop();
        const std::size_t u = static_cast<std::size_t>(it.node);
        // Lazy deletion: when a distance improves we push a NEW item instead of
        // updating the old one, so old (bigger) items are still in the heap.
        // If this item is out of date, or u is already settled, skip it.
        if (visited_[u] || it.dist > dist_[u]) continue;
        visited_[u] = true;
        ++visitedCount_;
        const DynamicArray<Edge>& edges = graph_->neighbors(it.node);
        for (std::size_t i = 0; i < edges.size(); ++i) {
            const Edge& e = edges[i];
            if (e.blocked) continue;  // blocked roads cannot be used at all
            const std::size_t v = static_cast<std::size_t>(e.to);
            double nd = it.dist + e.weight();
            // Relax the edge: going through u is faster than the best route
            // to v found so far -> remember it and where we came from.
            if (nd < dist_[v]) {
                dist_[v] = nd;
                prev_[v] = it.node;
                heap_.push({nd, e.to});
            }
        }
        return it.node;  // one settled node per step (the GUI inspector shows these)
    }
    return -1;
}

// O((V + E) log V) in total.
void Dijkstra::run() {
    while (step() != -1) {
    }
}

double Dijkstra::distanceTo(int v) const {
    if (!started_) throw std::logic_error("Dijkstra: call start() first");
    if (!graph_->isValid(v)) throw std::out_of_range("Dijkstra: invalid node");
    return dist_[static_cast<std::size_t>(v)];
}

bool Dijkstra::isVisited(int v) const {
    if (!started_ || !graph_->isValid(v)) return false;
    return visited_[static_cast<std::size_t>(v)];
}

// O(path length). prev_ points backwards (v -> ... -> source), so we collect
// the nodes and then reverse them into source -> ... -> v.
DynamicArray<int> Dijkstra::pathTo(int v) const {
    DynamicArray<int> path;
    if (distanceTo(v) == INF) return path;
    DynamicArray<int> reversed;
    for (int cur = v; cur != -1; cur = prev_[static_cast<std::size_t>(cur)]) reversed.pushBack(cur);
    for (std::size_t i = reversed.size(); i > 0; --i) path.pushBack(reversed[i - 1]);
    return path;
}
