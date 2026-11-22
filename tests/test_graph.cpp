// Tests for Graph (adjacency list of two-way roads).
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
    CHECK(ba->blocked);   // the other direction too!
    CHECK(g.reopenRoad(b, a));  // reopen from the other side
    CHECK(!ab->blocked);
    CHECK(!ba->blocked);
}

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

void invalidIdsThrow() {
    Graph g;
    g.addNode("A", NodeType::City, 0, 0);
    CHECK_THROWS_AS(g.node(5), std::out_of_range);
    CHECK_THROWS_AS(g.node(-1), std::out_of_range);
    CHECK_THROWS_AS(g.neighbors(1), std::out_of_range);
    CHECK_THROWS_AS(g.addRoad(0, 9, 10, RoadType::Road, "x"), std::out_of_range);
    CHECK_THROWS_AS(g.addRoad(0, 0, 10, RoadType::Road, "x"), std::invalid_argument);
}

void findNodeByName() {
    Graph g;
    g.addNode("Karachi", NodeType::City, 24.86, 67.00);
    int s = g.addNode("Sukkur", NodeType::City, 27.70, 68.86);
    CHECK_EQ(g.findNode("Sukkur"), s);
    CHECK_EQ(g.findNode("Sukur"), -1);
    CHECK_THROWS_AS(g.addNode("Sukkur", NodeType::City, 0, 0), std::invalid_argument);
}

// TODO (you): one test of your own. Idea: a node with 3 roads has 3 neighbours,
// and blocking one of them does NOT remove it from neighbors() (it is only marked).

}  // namespace

void runGraphTests() {
    test::section("Graph");
    test::runCase("addNodesGivesIds", addNodesGivesIds);
    test::runCase("roadIsTwoWay", roadIsTwoWay);
    test::runCase("blockAndReopenBothDirections", blockAndReopenBothDirections);
    test::runCase("conditionChangesWeight", conditionChangesWeight);
    test::runCase("missingRoadReturnsFalse", missingRoadReturnsFalse);
    test::runCase("invalidIdsThrow", invalidIdsThrow);
    test::runCase("findNodeByName", findNodeByName);
}
