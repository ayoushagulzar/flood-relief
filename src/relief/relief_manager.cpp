// this file has the code of ReliefManager (priority queue + dijkstra + greedy depots)
#include "relief/relief_manager.h"

#include <stdexcept>

#include "core/dijkstra.h"

// ======================== helpers ===================================

// e.g. urgency 5, 8000 people, vulnerable -> 500 + 80 + 50 = 630
double ReliefManager::baseScore(const Request& r) {
    return r.urgency * 100.0 + r.people / 100.0 + (r.vulnerable ? 50.0 : 0.0);
}

// aging: priority = base + 2 * (now - arrival).
// 2 * now is same for every request so we can remove it -> key = base - 2 * arrival
// this key never changes so the heap doesn't need re-sorting when time moves
double ReliefManager::heapKey(const Request& r) {
    return baseScore(r) - kAgingPerMinute * r.arrivalTime;
}

// saves an event for the event log
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

// linear search, we only have 9 depots
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

// uses the hash table -> O(1) average
const Request* ReliefManager::findRequest(const std::string& id) const {
    int idx = -1;
    if (!requestIndex_.find(id, idx)) return nullptr;
    return &requests_[static_cast<std::size_t>(idx)];
}

// path -> road names like "N-5, Dadu-Moro Bridge" (same name twice is written once)
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

// e.g. "N-55 Jamshoro-Sehwan"
std::string ReliefManager::roadLabel(int u, int v) const {
    const Edge* e = graph_.edgeBetween(u, v);
    std::string name = (e && !e->roadName.empty()) ? e->roadName + " " : "";
    return name + graph_.node(u).name + "-" + graph_.node(v).name;
}

// ======================== setup =====================================

int ReliefManager::addDepot(int nodeId) {
    if (!graph_.isValid(nodeId)) throw std::invalid_argument("addDepot: invalid node");
    if (graph_.node(nodeId).type != NodeType::Depot)
        throw std::invalid_argument("addDepot: '" + graph_.node(nodeId).name + "' is not a depot");
    // if it already exists just return it
    for (std::size_t i = 0; i < depots_.size(); ++i)
        if (depots_[i].nodeId == nodeId) return static_cast<int>(i);
    Depot d;
    d.nodeId = nodeId;
    depots_.pushBack(d);
    return static_cast<int>(depots_.size() - 1);
}

void ReliefManager::addRequest(const Request& in) {
    // checking the input first
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
    requestIndex_.insert(r.id, index); // save id -> index
    pushToQueue(index);
    log(EventType::RequestAdded, r.id + " " + formatNumber(r.quantity) + " " + supplyName(r.supply) + " at " +
                                     graph_.node(r.locationId).name + " (urgency " + std::to_string(r.urgency) + ")");
}

// makes the readable sentence for the decision (used by "why" and the GUI)
void ReliefManager::explain(Decision& d, const Request& r) {
    const std::string who = r.id + " (urgency " + std::to_string(r.urgency) + ", " + formatNumber(r.people) +
                            " people" + (r.vulnerable ? ", vulnerable" : "") + ")";
    // one part for each depot that sent something
    std::string legsText;
    for (std::size_t i = 0; i < d.legs.size(); ++i) {
        const DecisionLeg& leg = d.legs[i];
        if (i > 0) legsText += " and ";
        legsText += formatNumber(leg.quantity) + " from " + graph_.node(leg.depotNodeId).name + " via " + leg.via +
                    ", " + formatMinutes(leg.minutes);
    }
    // skipped depots: real reasons are written, "farther" ones are only counted
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
        // on a retry maybe nothing new came, so we say that instead of an empty list
        std::string now = d.legs.isEmpty() ? "nothing new this time" : legsText;
        d.explanation = who + " got " + formatNumber(r.delivered) + " of " + formatNumber(r.quantity) + " " + sup +
                        " so far (" + now + "); " + formatNumber(r.remaining()) + " still needed, deferred for retry.";
        log(EventType::Partial, r.id + " partly served, " + formatNumber(r.remaining()) + " " + sup + " still needed");
    } else {
        d.explanation = who + " deferred: no reachable depot has " + sup + ".";
        log(EventType::Deferred, r.id + " deferred: no reachable depot has " + sup);
    }
    if (!skippedText.empty()) d.explanation += " Skipped " + skippedText + ".";
    if (fartherCount > 0)
        d.explanation += " " + std::to_string(fartherCount) + " farther depot(s) not needed.";
}

// ======================== main algorithm ============================

// 1. pop most urgent request  2. dijkstra from its location
// 3. sort depots by distance  4. take from nearest depots first (greedy)
Decision ReliefManager::processNext() {
    Decision d;
    if (queue_.isEmpty()) {
        d.explanation = "Nothing to process: the queue is empty.";
        lastDecision_ = d;
        return d;
    }

    HeapItem top = queue_.pop();
    Request& r = requests_[static_cast<std::size_t>(top.index)];
    // first time this request is processed -> count its waiting time
    if (r.firstProcessedTime < 0) {
        r.firstProcessedTime = now_;
        totalWaitMinutes_ += now_ - r.arrivalTime;
        ++processedCount_;
    }
    const int s = supplyIndex(r.supply);

    d.valid = true;
    d.requestId = r.id;
    d.requestIndex = top.index;

    // step 1: one dijkstra from the request location gives distance to ALL depots
    // (roads are two way so location -> depot is same as depot -> location)
    Dijkstra dj(graph_);
    dj.start(r.locationId);
    dj.run();

    // step 2: skip depots with no stock or no open road, sort the rest by distance
    DynamicArray<int> candidates;
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
            // insertion sort (only 9 depots so its fine)
            std::size_t pos = candidates.size();
            double myDist = dj.distanceTo(dep.nodeId);
            while (pos > 0 &&
                   dj.distanceTo(depots_[static_cast<std::size_t>(candidates[pos - 1])].nodeId) > myDist)
                --pos;
            candidates.insertAt(pos, static_cast<int>(i));
        }
    }

    // step 3: greedy -> take from nearest depot first until request is full
    for (std::size_t c = 0; c < candidates.size(); ++c) {
        Depot& dep = depots_[static_cast<std::size_t>(candidates[c])];
        if (r.remaining() <= 0) {
            SkippedDepot sk;
            sk.depotNodeId = dep.nodeId;
            sk.reason = "farther (request already filled)";
            d.skipped.pushBack(sk);
            continue;
        }
        int take = dep.stock[s] < r.remaining() ? dep.stock[s] : r.remaining(); // min(stock, needed)
        dep.stock[s] -= take;
        r.delivered += take;
        d.deliveredNow += take;

        DecisionLeg leg;
        leg.depotNodeId = dep.nodeId;
        leg.quantity = take;
        leg.minutes = dj.distanceTo(dep.nodeId);
        DynamicArray<int> back = dj.pathTo(dep.nodeId);
        for (std::size_t i = back.size(); i > 0; --i) leg.path.pushBack(back[i - 1]); // reverse -> depot to location
        leg.via = roadNamesAlong(leg.path);
        totalRouteMinutes_ += leg.minutes;
        d.legs.pushBack(leg);
    }

    // step 4: set status. if not fully served it goes to the deferred list (not back in the heap)
    if (r.remaining() == 0) r.status = Status::Fulfilled;
    else if (r.delivered > 0) r.status = Status::PartiallyFulfilled;
    else r.status = Status::Deferred;
    d.status = r.status;
    if (r.status != Status::Fulfilled) deferred_.pushBack(top.index);

    explain(d, r);

    now_ += kDispatchMinutes;
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

// something changed in the world, so all deferred requests go back in the heap
void ReliefManager::retryDeferred(const std::string& reason) {
    if (deferred_.isEmpty()) return;
    std::size_t n = deferred_.size();
    std::string ids;
    for (std::size_t i = 0; i < n; ++i) {
        pushToQueue(deferred_[i]);
        ids += (i ? ", " : "") + requests_[static_cast<std::size_t>(deferred_[i])].id;
    }
    deferred_.clear();
    log(EventType::Retry, std::to_string(n) + " deferred request(s) queued again after " + reason + ": " + ids);
}

DynamicArray<int> ReliefManager::queueSnapshot() const {
    DynamicArray<int> out;
    for (std::size_t i = 0; i < queue_.size(); ++i) out.pushBack(queue_.at(i).index);
    return out;
}

// ======================== world changes =============================
// change the graph -> log it -> retry deferred requests

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
    int idx = addDepot(depotNodeId);
    depots_[static_cast<std::size_t>(idx)].stock[supplyIndex(s)] += quantity;
    log(EventType::StockAdded, "+" + formatNumber(quantity) + " " + supplyName(s) + " at " +
                                   graph_.node(depotNodeId).name);
    retryDeferred("new stock");
}

// heap doesn't need any change here bcuz the key is fixed
void ReliefManager::advanceTime(int minutes) {
    if (minutes < 0) throw std::invalid_argument("advanceTime: negative minutes");
    now_ += minutes;
}

// counts requests by status and adds up what is still needed
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
