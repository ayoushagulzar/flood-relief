// relief_manager.cpp - the brain (see relief_manager.h for the design).
#include "relief/relief_manager.h"

#include <stdexcept>

#include "core/dijkstra.h"

// ---------- helpers ----------

// urgency dominates (x100), then people affected (/100), +50 if vulnerable.
double ReliefManager::baseScore(const Request& r) {
    return r.urgency * 100.0 + r.people / 100.0 + (r.vulnerable ? 50.0 : 0.0);
}

// Aging: priority(now) = base + k*(now - arrival). k*now is the same for every
// request, so comparing base - k*arrival gives the same order -> a FIXED key.
double ReliefManager::heapKey(const Request& r) {
    return baseScore(r) - kAgingPerMinute * r.arrivalTime;
}

void ReliefManager::log(EventType type, const std::string& text) {
    Event e;
    e.time = now_;
    e.type = type;
    e.text = text;
    events_.pushBack(e);
}

void ReliefManager::pushToQueue(int index) {
    const Request& r = requests_[static_cast<std::size_t>(index)];
    queue_.push({heapKey(r), r.arrivalTime, index});
}

Depot* ReliefManager::depotAt(int nodeId) {
    for (std::size_t i = 0; i < depots_.size(); ++i)
        if (depots_[i].nodeId == nodeId) return &depots_[i];
    return nullptr;
}

const Depot* ReliefManager::findDepot(int nodeId) const {
    for (std::size_t i = 0; i < depots_.size(); ++i)
        if (depots_[i].nodeId == nodeId) return &depots_[i];
    return nullptr;
}

const Request* ReliefManager::findRequest(const std::string& id) const {
    int idx = -1;
    if (!requestIndex_.find(id, idx)) return nullptr;
    return &requests_[static_cast<std::size_t>(idx)];
}

std::string ReliefManager::roadNamesAlong(const DynamicArray<int>& path) const {
    std::string out;
    std::string last;
    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
        const Edge* e = graph_.edgeBetween(path[i], path[i + 1]);
        if (!e || e->roadName.empty() || e->roadName == last) continue;
        if (!out.empty()) out += ", ";
        out += e->roadName;
        last = e->roadName;
    }
    return out.empty() ? "local roads" : out;
}

std::string ReliefManager::roadLabel(int u, int v) const {
    const Edge* e = graph_.edgeBetween(u, v);
    std::string name = (e && !e->roadName.empty()) ? e->roadName + " " : "";
    return name + graph_.node(u).name + "-" + graph_.node(v).name;
}

// ---------- setup ----------

int ReliefManager::addDepot(int nodeId) {
    if (!graph_.isValid(nodeId)) throw std::invalid_argument("addDepot: invalid node");
    if (graph_.node(nodeId).type != NodeType::Depot)
        throw std::invalid_argument("addDepot: '" + graph_.node(nodeId).name + "' is not a depot");
    for (std::size_t i = 0; i < depots_.size(); ++i)
        if (depots_[i].nodeId == nodeId) return static_cast<int>(i);
    Depot d;
    d.nodeId = nodeId;
    depots_.pushBack(d);
    return static_cast<int>(depots_.size() - 1);
}

// O(log n): validate, store, index the id in the HashTable, push into the heap.
void ReliefManager::addRequest(const Request& in) {
    if (in.id.empty()) throw std::invalid_argument("request id is empty");
    if (requestIndex_.contains(in.id)) throw std::invalid_argument("duplicate request id '" + in.id + "'");
    if (!graph_.isValid(in.locationId)) throw std::invalid_argument("request '" + in.id + "': invalid location");
    if (in.quantity <= 0) throw std::invalid_argument("request '" + in.id + "': quantity must be > 0");
    if (in.urgency < 1 || in.urgency > 5) throw std::invalid_argument("request '" + in.id + "': urgency must be 1-5");
    if (in.people < 0) throw std::invalid_argument("request '" + in.id + "': people must be >= 0");

    Request r = in;
    r.delivered = 0;
    r.status = Status::Pending;
    r.arrivalTime = now_;
    r.firstProcessedTime = -1;
    int index = static_cast<int>(requests_.size());
    requests_.pushBack(r);
    requestIndex_.insert(r.id, index);
    pushToQueue(index);
    log(EventType::RequestAdded, r.id + " " + formatNumber(r.quantity) + " " + supplyName(r.supply) + " at " +
                                     graph_.node(r.locationId).name + " (urgency " + std::to_string(r.urgency) + ")");
}

// Builds d.explanation (one readable sentence) and logs the matching event.
void ReliefManager::explain(Decision& d, const Request& r) {
    const std::string who = r.id + " (urgency " + std::to_string(r.urgency) + ", " + formatNumber(r.people) +
                            " people" + (r.vulnerable ? ", vulnerable" : "") + ")";
    std::string legsText;
    for (std::size_t i = 0; i < d.legs.size(); ++i) {
        const DecisionLeg& leg = d.legs[i];
        if (i > 0) legsText += " and ";
        legsText += formatNumber(leg.quantity) + " from " + graph_.node(leg.depotNodeId).name + " via " + leg.via +
                    ", " + formatMinutes(leg.minutes);
    }
    // Real reasons are listed by name; "farther" depots are only counted
    // (the full list stays in d.skipped for the GUI's Why tab and the CLI's `why`).
    std::string skippedText;
    int fartherCount = 0;
    for (std::size_t i = 0; i < d.skipped.size(); ++i) {
        if (d.skipped[i].reason.rfind("farther", 0) == 0) {
            ++fartherCount;
            continue;
        }
        if (!skippedText.empty()) skippedText += "; ";
        skippedText += graph_.node(d.skipped[i].depotNodeId).name + ": " + d.skipped[i].reason;
    }

    const std::string sup = supplyName(r.supply);
    if (r.status == Status::Fulfilled) {
        d.explanation = who + " served " + formatNumber(d.deliveredNow) + " " + sup + ": " + legsText + ".";
        log(EventType::Served, r.id + " served " + formatNumber(d.deliveredNow) + " " + sup + " (" + legsText + ")");
    } else if (r.status == Status::PartiallyFulfilled) {
        d.explanation = who + " got " + formatNumber(r.delivered) + " of " + formatNumber(r.quantity) + " " + sup +
                        ": " + legsText + "; " + formatNumber(r.remaining()) + " still needed, deferred for retry.";
        log(EventType::Partial, r.id + " partly served, " + formatNumber(r.remaining()) + " " + sup + " still needed");
    } else {
        d.explanation = who + " deferred: no reachable depot has " + sup + ".";
        log(EventType::Deferred, r.id + " deferred: no reachable depot has " + sup);
    }
    if (!skippedText.empty()) d.explanation += " Skipped " + skippedText + ".";
    if (fartherCount > 0)
        d.explanation += " " + std::to_string(fartherCount) + " farther depot(s) not needed.";
}

// ---------- the brain ----------

// The main algorithm. Cost: one Dijkstra O((V + E) log V) + sorting the depots.
// Greedy = nearest depots first. Fast and simple, but not always the best plan
// overall (README "Limitations").
Decision ReliefManager::processNext() {
    Decision d;
    if (queue_.isEmpty()) {
        d.explanation = "Nothing to process: the queue is empty.";
        lastDecision_ = d;
        return d;
    }

    HeapItem top = queue_.pop();
    Request& r = requests_[static_cast<std::size_t>(top.index)];
    if (r.firstProcessedTime < 0) {
        r.firstProcessedTime = now_;
        totalWaitMinutes_ += now_ - r.arrivalTime;
        ++processedCount_;
    }
    const int s = supplyIndex(r.supply);

    d.valid = true;
    d.requestId = r.id;
    d.requestIndex = top.index;

    // 1. Shortest travel times from the request location to every node.
    //    Roads are two-way, so "depot -> location" = "location -> depot".
    Dijkstra dj(graph_);
    dj.start(r.locationId);
    dj.run();

    // 2. Sort depots into: no stock / unreachable / candidates (nearest first).
    DynamicArray<int> candidates;  // indices into depots_
    for (std::size_t i = 0; i < depots_.size(); ++i) {
        const Depot& dep = depots_[i];
        SkippedDepot sk;
        sk.depotNodeId = dep.nodeId;
        if (dep.stock[s] <= 0) {
            sk.reason = std::string("no ") + supplyName(r.supply);
            d.skipped.pushBack(sk);
        } else if (!dj.reachable(dep.nodeId)) {
            sk.reason = "unreachable (roads blocked)";
            d.skipped.pushBack(sk);
        } else {
            // insertion sort by distance: candidate lists are tiny
            std::size_t pos = candidates.size();
            double myDist = dj.distanceTo(dep.nodeId);
            while (pos > 0 &&
                   dj.distanceTo(depots_[static_cast<std::size_t>(candidates[pos - 1])].nodeId) > myDist)
                --pos;
            candidates.insertAt(pos, static_cast<int>(i));
        }
    }

    // 3. Greedy: take from the nearest depots until the request is filled.
    for (std::size_t c = 0; c < candidates.size(); ++c) {
        Depot& dep = depots_[static_cast<std::size_t>(candidates[c])];
        if (r.remaining() <= 0) {
            SkippedDepot sk;
            sk.depotNodeId = dep.nodeId;
            sk.reason = "farther (request already filled)";
            d.skipped.pushBack(sk);
            continue;
        }
        int take = dep.stock[s] < r.remaining() ? dep.stock[s] : r.remaining();
        dep.stock[s] -= take;
        r.delivered += take;
        d.deliveredNow += take;

        DecisionLeg leg;
        leg.depotNodeId = dep.nodeId;
        leg.quantity = take;
        leg.minutes = dj.distanceTo(dep.nodeId);
        DynamicArray<int> back = dj.pathTo(dep.nodeId);  // location ... depot
        for (std::size_t i = back.size(); i > 0; --i) leg.path.pushBack(back[i - 1]);
        leg.via = roadNamesAlong(leg.path);
        totalRouteMinutes_ += leg.minutes;
        d.legs.pushBack(leg);
    }

    // 4. Status, deferred list, events, explanation.
    if (r.remaining() == 0) r.status = Status::Fulfilled;
    else if (r.delivered > 0) r.status = Status::PartiallyFulfilled;
    else r.status = Status::Deferred;
    d.status = r.status;
    // Not finished -> wait in deferred_ (NOT the heap) until the world changes.
    if (r.status != Status::Fulfilled) deferred_.pushBack(top.index);

    explain(d, r);

    now_ += kDispatchMinutes;  // each decision takes the control room 10 minutes
    lastDecision_ = d;
    return d;
}

int ReliefManager::processAll() {
    int n = 0;
    while (!queue_.isEmpty()) {
        processNext();
        ++n;
    }
    return n;
}

// Called after block/reopen/condition/stock changes: something changed, so
// every deferred request gets another chance.
void ReliefManager::retryDeferred(const std::string& reason) {
    if (deferred_.isEmpty()) return;
    std::size_t n = deferred_.size();
    for (std::size_t i = 0; i < n; ++i) pushToQueue(deferred_[i]);
    deferred_.clear();
    log(EventType::Retry, std::to_string(n) + " deferred request(s) queued again after " + reason);
}

// ---------- world changes ----------

bool ReliefManager::blockRoad(int u, int v) {
    if (!graph_.blockRoad(u, v)) return false;
    log(EventType::RoadBlocked, roadLabel(u, v) + " BLOCKED");
    retryDeferred("road block");
    return true;
}

bool ReliefManager::reopenRoad(int u, int v) {
    if (!graph_.reopenRoad(u, v)) return false;
    log(EventType::RoadReopened, roadLabel(u, v) + " reopened");
    retryDeferred("road reopened");
    return true;
}

bool ReliefManager::setRoadCondition(int u, int v, RoadCondition c) {
    if (!graph_.setCondition(u, v, c)) return false;
    log(EventType::ConditionChanged, roadLabel(u, v) + " is now " + conditionName(c));
    retryDeferred("road condition change");
    return true;
}

void ReliefManager::addStock(int depotNodeId, Supply s, int quantity) {
    if (quantity <= 0) throw std::invalid_argument("addStock: quantity must be > 0");
    int idx = addDepot(depotNodeId);  // throws if the node is not a depot
    depots_[static_cast<std::size_t>(idx)].stock[supplyIndex(s)] += quantity;
    log(EventType::StockAdded, "+" + formatNumber(quantity) + " " + supplyName(s) + " at " +
                                   graph_.node(depotNodeId).name);
    retryDeferred("new stock");
}

void ReliefManager::advanceTime(int minutes) {
    if (minutes < 0) throw std::invalid_argument("advanceTime: negative minutes");
    now_ += minutes;
}

Stats ReliefManager::stats() const {
    Stats st;
    st.total = static_cast<int>(requests_.size());
    for (std::size_t i = 0; i < requests_.size(); ++i) {
        const Request& r = requests_[i];
        switch (r.status) {
            case Status::Pending: ++st.pending; break;
            case Status::Fulfilled: ++st.fulfilled; break;
            case Status::PartiallyFulfilled: ++st.partial; break;
            case Status::Deferred: ++st.deferred; break;
        }
        st.unmet[supplyIndex(r.supply)] += r.remaining();
    }
    st.totalRouteMinutes = totalRouteMinutes_;
    st.averageWaitMinutes = processedCount_ ? static_cast<double>(totalWaitMinutes_) / processedCount_ : 0.0;
    return st;
}
