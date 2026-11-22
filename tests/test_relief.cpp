// Integration tests for ReliefManager with a small hard-coded scenario.
#include <string>

#include "test_framework.h"
#include "relief/relief_manager.h"

namespace {

// Village --N-55 (10)-- Town --N-5 (20)-- Depot A        (A is 30 min away)
// Village --M-9 (90)----------------------- Depot B      (B is 90 min away)
struct World {
    ReliefManager m;
    int village, town, depotA, depotB;
    World() {
        Graph& g = m.graph();
        village = g.addNode("Village", NodeType::Shelter, 0, 0);
        town = g.addNode("Town", NodeType::City, 0, 0);
        depotA = g.addNode("Depot A", NodeType::Depot, 0, 0);
        depotB = g.addNode("Depot B", NodeType::Depot, 0, 0);
        g.addRoad(village, town, 10, RoadType::Highway, "N-55");
        g.addRoad(town, depotA, 20, RoadType::Highway, "N-5");
        g.addRoad(village, depotB, 90, RoadType::Motorway, "M-9");
        m.addDepot(depotA);
        m.addDepot(depotB);
    }
    void stock(int depot, Supply s, int q) { m.addStock(depot, s, q); }
    void request(const std::string& id, Supply s, int qty, int urgency, int people = 100, bool vulnerable = false) {
        Request r;
        r.id = id;
        r.locationId = village;
        r.supply = s;
        r.quantity = qty;
        r.urgency = urgency;
        r.people = people;
        r.vulnerable = vulnerable;
        m.addRequest(r);
    }
};

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

void baseScoreFormula() {
    Request r;
    r.urgency = 4;
    r.people = 1200;
    r.vulnerable = true;
    CHECK_EQ(ReliefManager::baseScore(r), 400.0 + 12.0 + 50.0);
    r.arrivalTime = 30;
    CHECK_EQ(ReliefManager::heapKey(r), 462.0 - 2.0 * 30);
}

void mostUrgentGoesFirst() {
    World w;
    w.stock(w.depotA, Supply::Water, 1000);
    w.request("low", Supply::Water, 10, 1);
    w.request("high", Supply::Water, 10, 5);
    w.request("mid", Supply::Water, 10, 3);
    CHECK(w.m.processNext().requestId == "high");
    CHECK(w.m.processNext().requestId == "mid");
    CHECK(w.m.processNext().requestId == "low");
    CHECK(!w.m.processNext().valid);  // queue empty
}

void agingLetsOldRequestsWin() {
    World w;
    w.stock(w.depotA, Supply::Water, 1000);
    w.request("old", Supply::Water, 10, 2);  // base 201, arrives at 0  -> key 201
    w.m.advanceTime(60);
    w.request("new", Supply::Water, 10, 3);  // base 301, arrives at 60 -> key 181
    CHECK(w.m.processNext().requestId == "old");
}

void nearestDepotFirstThenNext() {
    World w;
    w.stock(w.depotA, Supply::Water, 100);
    w.stock(w.depotB, Supply::Water, 500);
    w.request("R1", Supply::Water, 150, 5, 1200, true);
    Decision d = w.m.processNext();
    CHECK(d.status == Status::Fulfilled);
    CHECK_EQ(d.legs.size(), 2u);
    CHECK_EQ(d.legs.at(0).depotNodeId, w.depotA);  // nearest first
    CHECK_EQ(d.legs.at(0).quantity, 100);
    CHECK_EQ(d.legs.at(0).minutes, 30.0);
    CHECK(d.legs.at(0).via == "N-5, N-55");
    CHECK_EQ(d.legs.at(0).path.size(), 3u);         // Depot A -> Town -> Village
    CHECK_EQ(d.legs.at(0).path.at(0), w.depotA);
    CHECK_EQ(d.legs.at(1).depotNodeId, w.depotB);
    CHECK_EQ(d.legs.at(1).quantity, 50);
    CHECK(contains(d.explanation, "R1"));
    CHECK(contains(d.explanation, "1,200 people"));
    CHECK(contains(d.explanation, "Depot A"));
    CHECK_EQ(w.m.depots().at(0).stock[supplyIndex(Supply::Water)], 0);
    CHECK_EQ(w.m.depots().at(1).stock[supplyIndex(Supply::Water)], 450);
}

void skippedDepotReasons() {
    World w;
    w.stock(w.depotA, Supply::Water, 100);
    w.stock(w.depotB, Supply::Water, 100);
    w.stock(w.depotB, Supply::Food, 100);
    w.request("W", Supply::Water, 10, 5);   // A is enough -> B "farther"
    w.request("F", Supply::Food, 10, 4);    // A has no food
    Decision d1 = w.m.processNext();
    CHECK_EQ(d1.skipped.size(), 1u);
    CHECK_EQ(d1.skipped.at(0).depotNodeId, w.depotB);
    CHECK(contains(d1.skipped.at(0).reason, "farther"));
    Decision d2 = w.m.processNext();
    CHECK_EQ(d2.skipped.size(), 1u);
    CHECK_EQ(d2.skipped.at(0).depotNodeId, w.depotA);
    CHECK(d2.skipped.at(0).reason == "no food");
    CHECK(contains(d2.explanation, "Skipped Depot A: no food"));
    CHECK(contains(d1.explanation, "1 farther depot(s) not needed"));
}

void partialAndDeferred() {
    World w;
    w.stock(w.depotB, Supply::Water, 450);
    w.request("P", Supply::Water, 600, 3);
    w.request("D", Supply::Medicine, 10, 1);
    Decision p = w.m.processNext();
    CHECK(p.status == Status::PartiallyFulfilled);
    CHECK_EQ(p.deliveredNow, 450);
    const Request* pr = w.m.findRequest("P");
    CHECK(pr != nullptr && pr->remaining() == 150);
    Decision d = w.m.processNext();
    CHECK(d.status == Status::Deferred);
    CHECK(d.legs.isEmpty());
    CHECK_EQ(w.m.deferredCount(), 2u);
    Stats st = w.m.stats();
    CHECK_EQ(st.partial, 1);
    CHECK_EQ(st.deferred, 1);
    CHECK_EQ(st.unmet[supplyIndex(Supply::Water)], 150);
}

// A partial request is retried, but no depot has more stock: the sentence
// must say "nothing new this time" instead of an empty list (": ;").
void partialRetryWithNothingNew() {
    World w;
    w.stock(w.depotB, Supply::Water, 450);
    w.request("P", Supply::Water, 600, 3);
    w.m.processNext();                         // 450 of 600 -> partial
    w.m.reopenRoad(w.village, w.town);         // any world change retries deferred requests
    Decision again = w.m.processNext();
    CHECK(again.status == Status::PartiallyFulfilled);
    CHECK(again.legs.isEmpty());
    CHECK(contains(again.explanation, "got 450 of 600 water so far (nothing new this time)"));
    CHECK(!contains(again.explanation, ": ;"));
    // the retry event names the request that moved back into the queue
    bool named = false;
    const DynamicArray<Event>& ev = w.m.events();
    for (std::size_t i = 0; i < ev.size(); ++i)
        if (ev[i].type == EventType::Retry && contains(ev[i].text, "queued again after") && contains(ev[i].text, ": P"))
            named = true;
    CHECK(named);
}

// queueSnapshot() is the heap array: index 0 is the request processNext() takes.
void queueSnapshotTopIsNext() {
    World w;
    w.stock(w.depotA, Supply::Water, 1000);
    w.request("LOW", Supply::Water, 10, 1);
    w.request("HIGH", Supply::Water, 10, 5);
    w.request("MID", Supply::Water, 10, 3);
    DynamicArray<int> q = w.m.queueSnapshot();
    CHECK_EQ(q.size(), 3u);
    CHECK(w.m.requests()[static_cast<std::size_t>(q[0])].id == "HIGH");
    for (std::size_t i = 1; i < q.size(); ++i) {  // heap order: parent key >= child key
        double parent = ReliefManager::heapKey(w.m.requests()[static_cast<std::size_t>(q[(i - 1) / 2])]);
        CHECK(parent >= ReliefManager::heapKey(w.m.requests()[static_cast<std::size_t>(q[i])]));
    }
    CHECK(w.m.processNext().requestId == "HIGH");
    CHECK_EQ(w.m.queueSnapshot().size(), 2u);
}

void blockChangesRouteReopenRetries() {
    World w;
    w.stock(w.depotA, Supply::Water, 100);
    w.stock(w.depotB, Supply::Water, 100);
    w.m.blockRoad(w.village, w.town);  // cuts Depot A off
    w.request("R1", Supply::Water, 50, 5);
    Decision d1 = w.m.processNext();
    CHECK_EQ(d1.legs.size(), 1u);
    CHECK_EQ(d1.legs.at(0).depotNodeId, w.depotB);  // rerouted to the far depot
    CHECK(d1.legs.at(0).via == "M-9");
    bool aUnreachable = false;
    for (std::size_t i = 0; i < d1.skipped.size(); ++i)
        if (d1.skipped.at(i).depotNodeId == w.depotA && contains(d1.skipped.at(i).reason, "unreachable"))
            aUnreachable = true;
    CHECK(aUnreachable);

    w.request("R2", Supply::Water, 100, 4);  // B has 50 left, A unreachable
    Decision d2 = w.m.processNext();
    CHECK(d2.status == Status::PartiallyFulfilled);
    CHECK_EQ(w.m.deferredCount(), 1u);

    CHECK(w.m.reopenRoad(w.village, w.town));  // retry queues R2 again
    CHECK_EQ(w.m.deferredCount(), 0u);
    CHECK(w.m.hasPending());
    Decision d3 = w.m.processNext();
    CHECK(d3.requestId == "R2");
    CHECK(d3.status == Status::Fulfilled);
    CHECK_EQ(d3.legs.at(0).depotNodeId, w.depotA);
}

void addStockRetries() {
    World w;
    w.request("M", Supply::Medicine, 20, 5);
    CHECK(w.m.processNext().status == Status::Deferred);
    w.stock(w.depotB, Supply::Medicine, 20);
    CHECK(w.m.processNext().status == Status::Fulfilled);
    CHECK(w.m.stats().fulfilled == 1);
}

void clockAdvancesPerDecision() {
    World w;
    w.stock(w.depotA, Supply::Food, 100);
    w.request("C1", Supply::Food, 10, 3);
    w.request("C2", Supply::Food, 10, 3);
    w.m.processNext();
    w.m.processNext();
    CHECK_EQ(w.m.now(), 2 * ReliefManager::kDispatchMinutes);
    CHECK_EQ(w.m.stats().averageWaitMinutes, 5.0);  // waited 0 and 10 minutes
}

void processAllEmptiesQueue() {
    World w;
    w.stock(w.depotA, Supply::Food, 1000);
    for (int i = 0; i < 10; ++i) w.request("Q" + std::to_string(i), Supply::Food, 10, 1 + i % 5);
    CHECK_EQ(w.m.processAll(), 10);
    CHECK(!w.m.hasPending());
    CHECK_EQ(w.m.stats().fulfilled, 10);
}

void badRequestsThrow() {
    World w;
    w.request("X", Supply::Food, 10, 3);
    CHECK_THROWS_AS(w.request("X", Supply::Food, 10, 3), std::invalid_argument);  // duplicate
    CHECK_THROWS_AS(w.request("Y", Supply::Food, 0, 3), std::invalid_argument);   // quantity 0
    CHECK_THROWS_AS(w.request("Z", Supply::Food, 10, 6), std::invalid_argument);  // urgency 6
    Request bad;
    bad.id = "L";
    bad.locationId = 99;
    bad.quantity = 1;
    bad.urgency = 1;
    CHECK_THROWS_AS(w.m.addRequest(bad), std::invalid_argument);                 // bad location
    CHECK_THROWS_AS(w.m.addDepot(w.town), std::invalid_argument);                // not a depot
}

void eventsAreLogged() {
    World w;
    std::size_t before = w.m.events().size();
    w.stock(w.depotA, Supply::Water, 10);
    w.m.blockRoad(w.village, w.town);
    CHECK_EQ(w.m.events().size(), before + 2);
    if (w.m.events().isEmpty()) return;
    CHECK(w.m.events().back().type == EventType::RoadBlocked);
    CHECK(contains(w.m.events().back().text, "N-55"));
}

// TODO (you): one test of your own. Idea: setRoadCondition(Flooded) on N-55
// makes Depot A 10*2.5 + 20 = 45 min away, which is still nearer than B (90).
// Check leg.minutes == 45.

}  // namespace

void runReliefTests() {
    test::section("ReliefManager");
    test::runCase("baseScoreFormula", baseScoreFormula);
    test::runCase("mostUrgentGoesFirst", mostUrgentGoesFirst);
    test::runCase("agingLetsOldRequestsWin", agingLetsOldRequestsWin);
    test::runCase("nearestDepotFirstThenNext", nearestDepotFirstThenNext);
    test::runCase("skippedDepotReasons", skippedDepotReasons);
    test::runCase("partialAndDeferred", partialAndDeferred);
    test::runCase("partialRetryWithNothingNew", partialRetryWithNothingNew);
    test::runCase("queueSnapshotTopIsNext", queueSnapshotTopIsNext);
    test::runCase("blockChangesRouteReopenRetries", blockChangesRouteReopenRetries);
    test::runCase("addStockRetries", addStockRetries);
    test::runCase("clockAdvancesPerDecision", clockAdvancesPerDecision);
    test::runCase("processAllEmptiesQueue", processAllEmptiesQueue);
    test::runCase("badRequestsThrow", badRequestsThrow);
    test::runCase("eventsAreLogged", eventsAreLogged);
}
