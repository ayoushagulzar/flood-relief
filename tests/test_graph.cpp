// tests for Graph
#include "test_framework.h"
#include "core/graph.h"

namespace {

void addNodesGivesIds() {
    Graph g;
    int a = g.addNode("Karachi", NodeType::City, 24.86, 67.00);
    int b = g.addNode("Sukkur Depot", NodeType::Depot, 27.70, 68.86);
    CHECK_EQ(a, 0);
    CHECK_EQ(b, 1);
    CHECK_EQ(g.nodeCount(), 2u);
    CHECK(g.node(b).name == "Sukkur Depot");
    CHECK(g.node(b).type == NodeType::Depot);
}

// one road should be in both lists
void roadIsTwoWay() {
    Graph g;
    int a = g.addNode("A", NodeType::City, 0, 0);
    int b = g.addNode("B", NodeType::City, 0, 0);
    g.addRoad(a, b, 30, RoadType::Highway, "N-5");
    CHECK_EQ(g.roadCount(), 1u);
    CHECK_EQ(g.neighbors(a).size(), 1u);
    CHECK_EQ(g.neighbors(b).size(), 1u);
    CHECK_EQ(g.neighbors(a).at(0).to, b);
    CHECK_EQ(g.neighbors(b).at(0).to, a);
    CHECK(g.neighbors(b).at(0).roadName == "N-5");
}

// blocking one side blocks both sides
void blockAndReopenBothDirections() {
    Graph g;
    int a = g.addNode("A", NodeType::City, 0, 0);
    int b = g.addNode("B", NodeType::City, 0, 0);
    g.addRoad(a, b, 30, RoadType::Highway, "N-55");
    CHECK(g.blockRoad(a, b));
    const Edge* ab = g.edgeBetween(a, b);
    const Edge* ba = g.edgeBetween(b, a);
    if (!ab || !ba) {
        CHECK(ab != nullptr && ba != nullptr);
        return;
    }
    CHECK(ab->blocked);
    CHECK(ba->blocked);
    CHECK(g.reopenRoad(b, a));
    CHECK(!ab->blocked);
    CHECK(!ba->blocked);
}

// 100 clear -> 250 flooded -> 150 damaged
void conditionChangesWeight() {
    Graph g;
    int a = g.addNode("A", NodeType::City, 0, 0);
    int b = g.addNode("B", NodeType::City, 0, 0);
    g.addRoad(a, b, 100, RoadType::Road, "R-1");
    const Edge* ab = g.edgeBetween(a, b);
    const Edge* ba = g.edgeBetween(b, a);
    if (!ab || !ba) {
        CHECK(ab != nullptr && ba != nullptr);
        return;
    }
    CHECK_EQ(ab->weight(), 100.0);
    CHECK(g.setCondition(a, b, RoadCondition::Flooded));
    CHECK_EQ(ba->weight(), 250.0);
    CHECK(g.setCondition(a, b, RoadCondition::Damaged));
    CHECK_EQ(ab->weight(), 150.0);
}

void missingRoadReturnsFalse() {
    Graph g;
    int a = g.addNode("A", NodeType::City, 0, 0);
    int b = g.addNode("B", NodeType::City, 0, 0);
    CHECK(!g.blockRoad(a, b));
    CHECK(g.edgeBetween(a, b) == nullptr);
    CHECK(!g.hasRoad(a, b));
}

// bad ids should throw
void invalidIdsThrow() {
    Graph g;
    g.addNode("A", NodeType::City, 0, 0);
    CHECK_THROWS_AS(g.node(5), std::out_of_range);
    CHECK_THROWS_AS(g.node(-1), std::out_of_range);
    CHECK_THROWS_AS(g.neighbors(1), std::out_of_range);
    CHECK_THROWS_AS(g.addRoad(0, 9, 10, RoadType::Road, "x"), std::out_of_range);
    CHECK_THROWS_AS(g.addRoad(0, 0, 10, RoadType::Road, "x"), std::invalid_argument);
}

// name search uses the hash table, typo gives -1
void findNodeByName() {
    Graph g;
    g.addNode("Karachi", NodeType::City, 24.86, 67.00);
    int s = g.addNode("Sukkur", NodeType::City, 27.70, 68.86);
    CHECK_EQ(g.findNode("Sukkur"), s);
    CHECK_EQ(g.findNode("Sukur"), -1);
    CHECK_THROWS_AS(g.addNode("Sukkur", NodeType::City, 0, 0), std::invalid_argument);
}

// blocked road is only marked, not removed from the list
void blockedRoadStaysInList() {
    Graph g;
    int hub = g.addNode("Hub", NodeType::City, 0, 0);
    int a = g.addNode("A", NodeType::City, 0, 0);
    int b = g.addNode("B", NodeType::City, 0, 0);
    int c = g.addNode("C", NodeType::City, 0, 0);
    g.addRoad(hub, a, 10, RoadType::Road, "r1");
    g.addRoad(hub, b, 20, RoadType::Road, "r2");
    g.addRoad(hub, c, 30, RoadType::Road, "r3");
    CHECK_EQ(g.neighbors(hub).size(), 3u);
    CHECK(g.blockRoad(hub, b));
    CHECK_EQ(g.neighbors(hub).size(), 3u);
    const Edge* e = g.edgeBetween(hub, b);
    CHECK(e != nullptr && e->blocked);
    CHECK_EQ(g.roadCount(), 3u);
}

}

// called from test_main.cpp
void runGraphTests() {
    test::section("Graph");
    test::runCase("addNodesGivesIds", addNodesGivesIds);
    test::runCase("roadIsTwoWay", roadIsTwoWay);
    test::runCase("blockAndReopenBothDirections", blockAndReopenBothDirections);
    test::runCase("conditionChangesWeight", conditionChangesWeight);
    test::runCase("missingRoadReturnsFalse", missingRoadReturnsFalse);
    test::runCase("invalidIdsThrow", invalidIdsThrow);
    test::runCase("findNodeByName", findNodeByName);
    test::runCase("blockedRoadStaysInList", blockedRoadStaysInList);
}
