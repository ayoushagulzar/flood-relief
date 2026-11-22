// relief_manager.h - the brain. The CLI and the GUI only ever call this class.
//
// Data structures in use:
//   MinHeap<HeapItem, HigherPriorityFirst>  -> which request goes next
//   HashTable                               -> request id ("R-07") -> index
//   Graph + Dijkstra                        -> routes and travel times
//   DynamicArray                            -> requests, depots, events, deferred list
//
// Priority with aging (prevents starvation):
//   base = urgency * 100 + people / 100 + (vulnerable ? 50 : 0)
//   priority(now) = base + k * (now - arrival)
//   k * now is the same for every request, so the heap can use the FIXED key
//   key = base - k * arrival   (higher key first, ties -> earlier arrival).
//
// No cout in here: the manager reports through return values and events().
#pragma once

#include <cstddef>
#include <string>

#include "core/dynamic_array.h"
#include "core/graph.h"
#include "core/hash_table.h"
#include "core/min_heap.h"
#include "relief/types.h"

class ReliefManager {
public:
    static constexpr double kAgingPerMinute = 2.0;
    static constexpr int kDispatchMinutes = 10;  // clock advance per processNext()

    ReliefManager() = default;
    ReliefManager(const ReliefManager&) = delete;
    ReliefManager& operator=(const ReliefManager&) = delete;

    // ---------- setup (used by the scenario loader) ----------
    Graph& graph() { return graph_; }
    const Graph& graph() const { return graph_; }

    // Registers a depot at a node of type Depot (or returns the existing one).
    // Throws std::invalid_argument if the node is not a depot.
    int addDepot(int nodeId);

    // Adds a request. Throws std::invalid_argument on a duplicate id, invalid
    // location, quantity <= 0 or urgency outside 1..5. arrivalTime is set to now().
    void addRequest(const Request& r);

    // ---------- actions ----------
    Decision processNext();   // most urgent request -> greedy nearest depots; clock += 10 min
    int processAll();         // processNext() until the queue is empty; returns how many
    bool hasPending() const { return !queue_.isEmpty(); }

    bool blockRoad(int u, int v);
    bool reopenRoad(int u, int v);
    bool setRoadCondition(int u, int v, RoadCondition c);
    void addStock(int depotNodeId, Supply s, int quantity);
    void advanceTime(int minutes);

    // ---------- read-only views ----------
    const DynamicArray<Request>& requests() const { return requests_; }
    const DynamicArray<Depot>& depots() const { return depots_; }
    const DynamicArray<Event>& events() const { return events_; }
    const Decision& lastDecision() const { return lastDecision_; }
    Stats stats() const;
    int now() const { return now_; }
    std::size_t deferredCount() const { return deferred_.size(); }
    // The request heap exactly as stored (array order, index 0 = next to serve),
    // as indices into requests(). O(n). For the CLI `queue` view.
    DynamicArray<int> queueSnapshot() const;
    // Indices of requests waiting for a retry (not in the heap).
    const DynamicArray<int>& deferredList() const { return deferred_; }

    const Request* findRequest(const std::string& id) const;
    const Depot* findDepot(int nodeId) const;

    // base score and heap key (public so tests and the GUI can show them)
    static double baseScore(const Request& r);
    static double heapKey(const Request& r);

private:
    struct HeapItem {
        double key;
        int arrival;
        int index;  // into requests_
    };
    struct HigherPriorityFirst {
        bool operator()(const HeapItem& a, const HeapItem& b) const {
            if (a.key != b.key) return a.key > b.key;
            if (a.arrival != b.arrival) return a.arrival < b.arrival;
            return a.index < b.index;
        }
    };

    void log(EventType type, const std::string& text);
    void pushToQueue(int index);
    void retryDeferred(const std::string& reason);
    void explain(Decision& d, const Request& r);
    Depot* depotAt(int nodeId);
    std::string roadNamesAlong(const DynamicArray<int>& path) const;
    std::string roadLabel(int u, int v) const;

    Graph graph_;
    DynamicArray<Request> requests_;
    DynamicArray<Depot> depots_;
    DynamicArray<Event> events_;
    DynamicArray<int> deferred_;  // indices of requests waiting for a retry
    HashTable requestIndex_;      // "R-07" -> index into requests_
    MinHeap<HeapItem, HigherPriorityFirst> queue_;
    Decision lastDecision_;
    int now_ = 0;
    // for stats
    double totalRouteMinutes_ = 0.0;
    long long totalWaitMinutes_ = 0;
    int processedCount_ = 0;
};
