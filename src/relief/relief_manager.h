// this file declares ReliefManager -> the main logic of the project.
// the CLI and GUI only call this class, they don't have any relief logic themselves.
//
// priority of a request:
//   base = urgency * 100 + people / 100 + 50 (if vulnerable)
//   key  = base - 2 * arrivalTime   (aging, so old requests don't wait forever)

#pragma once // this prevents multiple inclusions of the header file.

#include <cstddef>
#include <string>

#include "core/dynamic_array.h"
#include "core/graph.h"
#include "core/hash_table.h"
#include "core/min_heap.h"
#include "relief/types.h"

class ReliefManager {
public:
    static constexpr double kAgingPerMinute = 2.0; // k in the aging formula
    static constexpr int kDispatchMinutes = 10;    // clock moves 10 min after each decision

    ReliefManager() = default;
    // no copying allowed
    ReliefManager(const ReliefManager&) = delete;
    ReliefManager& operator=(const ReliefManager&) = delete;

    // ==================== setup ====================
    Graph& graph() { return graph_; }
    const Graph& graph() const { return graph_; }

    int addDepot(int nodeId);          // throws if the node is not a depot
    void addRequest(const Request& r); // checks the request then pushes it in the heap

    // ==================== actions ====================
    Decision processNext(); // serves the most urgent request
    int processAll();       // serves all requests in the heap
    bool hasPending() const { return !queue_.isEmpty(); }

    // world changes (each one retries the deferred requests)
    bool blockRoad(int u, int v);
    bool reopenRoad(int u, int v);
    bool setRoadCondition(int u, int v, RoadCondition c);
    void addStock(int depotNodeId, Supply s, int quantity);
    void advanceTime(int minutes);

    // ==================== getters (read only) ====================
    const DynamicArray<Request>& requests() const { return requests_; }
    const DynamicArray<Depot>& depots() const { return depots_; }
    const DynamicArray<Event>& events() const { return events_; }
    const Decision& lastDecision() const { return lastDecision_; }
    Stats stats() const;
    int now() const { return now_; }
    std::size_t deferredCount() const { return deferred_.size(); }
    DynamicArray<int> queueSnapshot() const; // heap array as it is stored (for the "queue" command)
    const DynamicArray<int>& deferredList() const { return deferred_; }

    const Request* findRequest(const std::string& id) const; // nullptr if not found
    const Depot* findDepot(int nodeId) const;

    // public so tests and GUI can show them
    static double baseScore(const Request& r);
    static double heapKey(const Request& r);

private:
    // what we store in the heap
    struct HeapItem {
        double key;
        int arrival;
        int index; // index in requests_
    };
    // comparator -> higher key first, if same key then who came first
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
    DynamicArray<int> deferred_;     // requests waiting for a retry (not in the heap)
    HashTable requestIndex_;         // "R-07" -> index in requests_
    MinHeap<HeapItem, HigherPriorityFirst> queue_; // priority queue of requests
    Decision lastDecision_;
    int now_ = 0;                    // clock in minutes
    // for stats
    double totalRouteMinutes_ = 0.0;
    long long totalWaitMinutes_ = 0;
    int processedCount_ = 0;
};
