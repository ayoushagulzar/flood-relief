// tests for Dijkstra
#include "test_framework.h"
#include "core/dijkstra.h"
#include "core/graph.h"

namespace {

// small graph that i solved on paper first
// roads: A-B 4, A-C 2, B-C 1, B-D 5, C-D 8, C-E 11, D-E 2
// answers: A0 C2 B3 D8 E10 (with B-D blocked: D10 E12)
struct Paper {
    Graph g;
    int A, B, C, D, E;
    Paper() {
        A = g.addNode("A", NodeType::City, 0, 0);
        B = g.addNode("B", NodeType::City, 0, 0);
        C = g.addNode("C", NodeType::City, 0, 0);
        D = g.addNode("D", NodeType::City, 0, 0);
        E = g.addNode("E", NodeType::City, 0, 0);
        g.addRoad(A, B, 4, RoadType::Road, "AB");
        g.addRoad(A, C, 2, RoadType::Road, "AC");
        g.addRoad(B, C, 1, RoadType::Road, "BC");
        g.addRoad(B, D, 5, RoadType::Road, "BD");
        g.addRoad(C, D, 8, RoadType::Road, "CD");
        g.addRoad(C, E, 11, RoadType::Road, "CE");
        g.addRoad(D, E, 2, RoadType::Road, "DE");
    }
};

// checks if path p is exactly the list we want
bool samePath(const DynamicArray<int>& p, std::initializer_list<int> want) {
    if (p.size() != want.size()) return false;
    std::size_t i = 0;
    for (int x : want)
        if (p[i++] != x) return false;
    return true;
}

void paperExampleAllOpen() {
    Paper p;
    Dijkstra d(p.g);
    d.start(p.A);
    d.run();
    CHECK_EQ(d.distanceTo(p.A), 0.0);
    CHECK_EQ(d.distanceTo(p.C), 2.0);
    CHECK_EQ(d.distanceTo(p.B), 3.0);
    CHECK_EQ(d.distanceTo(p.D), 8.0);
    CHECK_EQ(d.distanceTo(p.E), 10.0);
    CHECK(samePath(d.pathTo(p.E), {p.A, p.C, p.B, p.D, p.E}));
    CHECK_EQ(d.visitedCount(), 5u);
}

// B-D blocked so it must go A C D E
void blockedRoadForcesDetour() {
    Paper p;
    p.g.blockRoad(p.B, p.D);
    Dijkstra d(p.g);
    d.start(p.A);
    d.run();
    CHECK_EQ(d.distanceTo(p.D), 10.0);
    CHECK_EQ(d.distanceTo(p.E), 12.0);
    CHECK(samePath(d.pathTo(p.E), {p.A, p.C, p.D, p.E}));
}

// flooded B-D = 5*2.5 = 12.5 so C-D (8) is better now
void floodingSwitchesRoute() {
    Paper p;
    p.g.setCondition(p.B, p.D, RoadCondition::Flooded);
    Dijkstra d(p.g);
    d.start(p.A);
    d.run();
    CHECK_EQ(d.distanceTo(p.D), 10.0);
    CHECK(samePath(d.pathTo(p.D), {p.A, p.C, p.D}));
}

// no open road -> INF and empty path
void unreachableIsInfinity() {
    Graph g;
    int a = g.addNode("A", NodeType::City, 0, 0);
    int b = g.addNode("B", NodeType::City, 0, 0);
    int c = g.addNode("C", NodeType::City, 0, 0);
    g.addRoad(a, b, 5, RoadType::Road, "AB");
    g.blockRoad(a, b);
    Dijkstra d(g);
    d.start(a);
    d.run();
    CHECK(d.distanceTo(b) == Dijkstra::INF);
    CHECK(d.distanceTo(c) == Dijkstra::INF);
    CHECK(!d.reachable(c));
    CHECK(d.pathTo(b).isEmpty());
    CHECK(d.pathTo(c).isEmpty());
}

void sourceEqualsTarget() {
    Paper p;
    Dijkstra d(p.g);
    d.start(p.D);
    d.run();
    CHECK_EQ(d.distanceTo(p.D), 0.0);
    CHECK(samePath(d.pathTo(p.D), {p.D}));
}

void blockingOneWayBlocksBoth() {
    Paper p;
    p.g.blockRoad(p.D, p.B);
    Dijkstra d(p.g);
    d.start(p.B);
    d.run();
    CHECK_EQ(d.distanceTo(p.D), 9.0);
}

// step by step should give same answer as run()
void stepAgreesWithRun() {
    Paper p;
    Dijkstra a(p.g), b(p.g);
    a.start(p.A);
    a.run();
    b.start(p.A);
    int steps = 0;
    int firstPopped = b.step();
    ++steps;
    while (b.step() != -1) ++steps;
    CHECK_EQ(firstPopped, p.A);
    CHECK_EQ(steps, 5);
    CHECK(b.isDone());
    bool same = true;
    for (int v = 0; v < 5; ++v)
        if (a.distanceTo(v) != b.distanceTo(v)) same = false;
    CHECK(same);
}

void restartResetsEverything() {
    Paper p;
    Dijkstra d(p.g);
    d.start(p.A);
    d.run();
    d.start(p.E);
    d.run();
    CHECK_EQ(d.distanceTo(p.E), 0.0);
    CHECK_EQ(d.distanceTo(p.D), 2.0);
    CHECK_EQ(d.visitedCount(), 5u);
}

// S-X looks short (1) but X-T is 100. answer must be 10 via Y
void shortFirstRoadIsATrap() {
    Graph g;
    int s = g.addNode("S", NodeType::City, 0, 0);
    int x = g.addNode("X", NodeType::City, 0, 0);
    int y = g.addNode("Y", NodeType::City, 0, 0);
    int t = g.addNode("T", NodeType::City, 0, 0);
    g.addRoad(s, x, 1, RoadType::Road, "SX");
    g.addRoad(x, t, 100, RoadType::Road, "XT");
    g.addRoad(s, y, 5, RoadType::Road, "SY");
    g.addRoad(y, t, 5, RoadType::Road, "YT");
    Dijkstra d(g);
    d.start(s);
    d.run();
    CHECK_EQ(d.distanceTo(t), 10.0);
    CHECK(samePath(d.pathTo(t), {s, y, t}));
}

}

// called from test_main.cpp
void runDijkstraTests() {
    test::section("Dijkstra");
    test::runCase("paperExampleAllOpen", paperExampleAllOpen);
    test::runCase("blockedRoadForcesDetour", blockedRoadForcesDetour);
    test::runCase("floodingSwitchesRoute", floodingSwitchesRoute);
    test::runCase("unreachableIsInfinity", unreachableIsInfinity);
    test::runCase("sourceEqualsTarget", sourceEqualsTarget);
    test::runCase("blockingOneWayBlocksBoth", blockingOneWayBlocksBoth);
    test::runCase("stepAgreesWithRun", stepAgreesWithRun);
    test::runCase("restartResetsEverything", restartResetsEverything);
    test::runCase("shortFirstRoadIsATrap", shortFirstRoadIsATrap);
}
