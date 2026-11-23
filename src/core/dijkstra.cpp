// this file has the code of Dijkstra's algorithm
#include "core/dijkstra.h"

#include <stdexcept>

// resets all arrays so the same object can be used again
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

int Dijkstra::step() {
    while (!heap_.isEmpty()) {
        Item it = heap_.pop(); // closest node
        const std::size_t u = static_cast<std::size_t>(it.node);
        // lazy deletion -> skip old items (already visited or distance is outdated)
        if (visited_[u] || it.dist > dist_[u]) continue;
        visited_[u] = true;
        ++visitedCount_;
        const DynamicArray<Edge>& edges = graph_->neighbors(it.node);
        for (std::size_t i = 0; i < edges.size(); ++i) {
            const Edge& e = edges[i];
            if (e.blocked) continue; // can't use blocked roads
            const std::size_t v = static_cast<std::size_t>(e.to);
            double nd = it.dist + e.weight();
            // relax -> if going through u is faster, update v
            if (nd < dist_[v]) {
                dist_[v] = nd;
                prev_[v] = it.node;
                heap_.push({nd, e.to});
            }
        }
        return it.node;
    }
    return -1;
}

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

// prev_ goes backwards (v -> source) so we collect it and then reverse it
DynamicArray<int> Dijkstra::pathTo(int v) const {
    DynamicArray<int> path;
    if (distanceTo(v) == INF) return path;
    DynamicArray<int> reversed;
    for (int cur = v; cur != -1; cur = prev_[static_cast<std::size_t>(cur)]) reversed.pushBack(cur);
    for (std::size_t i = reversed.size(); i > 0; --i) path.pushBack(reversed[i - 1]);
    return path;
}
