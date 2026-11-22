// Tests for ScenarioLoader: good files load, broken files give exact errors.
#include <string>

#include "test_framework.h"
#include "relief/scenario_loader.h"

namespace {

const char* kGood = R"(
# a tiny scenario
[NODES]
Sukkur,       city,    27.7052, 68.8574
Dadu Camp,    shelter, 26.7319, 67.7750
Sukkur Depot, depot,   27.7100, 68.8500

[ROADS]
Sukkur Depot, Sukkur,    10,  road,    -
Sukkur,       Dadu Camp, 180, highway, N-55, flooded

[STOCK]
Sukkur Depot, water, 500

[REQUESTS]
R-01, Dadu Camp, water, 300, 5, 1200, yes
)";

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

LoadResult loadBroken(const std::string& text) {
    ReliefManager m;
    return ScenarioLoader::loadText(text, "test.txt", m);
}

void goodFileLoads() {
    ReliefManager m;
    LoadResult r = ScenarioLoader::loadText(kGood, "good.txt", m);
    CHECK(r.ok);
    CHECK(r.error.empty());
    if (!r.ok) return;  // nothing else can work if loading failed
    CHECK_EQ(r.nodes, 3);
    CHECK_EQ(r.roads, 2);
    CHECK_EQ(r.requests, 1);
    CHECK_EQ(m.depots().size(), 1u);
    CHECK_EQ(m.depots().at(0).stock[supplyIndex(Supply::Water)], 500);
    int sukkur = m.graph().findNode("Sukkur");
    int camp = m.graph().findNode("Dadu Camp");
    int depot = m.graph().findNode("Sukkur Depot");
    const Edge* flooded = m.graph().edgeBetween(sukkur, camp);
    const Edge* local = m.graph().edgeBetween(depot, sukkur);
    CHECK(flooded != nullptr && flooded->condition == RoadCondition::Flooded);
    CHECK(local != nullptr && local->roadName.empty());  // "-" = unnamed
    const Request* req = m.findRequest("R-01");
    CHECK(req != nullptr);
    CHECK(req != nullptr && req->vulnerable);
    Decision d = m.processNext();
    CHECK(d.status == Status::Fulfilled);
    CHECK_EQ(d.legs.at(0).minutes, 10.0 + 180.0 * 2.5);
}

void unknownPlaceHasLineNumber() {
    LoadResult r = loadBroken("[NODES]\nSukkur, city, 27.7, 68.8\n[ROADS]\nSukkur, Sukur, 10, road, x\n");
    CHECK(!r.ok);
    CHECK(contains(r.error, "test.txt:4:"));
    CHECK(contains(r.error, "unknown place 'Sukur'"));
    CHECK(contains(r.error, "[ROADS]"));
}

void badNumber() {
    LoadResult r = loadBroken("[NODES]\nSukkur, city, 27.7abc, 68.8\n");
    CHECK(!r.ok);
    CHECK(contains(r.error, "test.txt:2:"));
    CHECK(contains(r.error, "bad number '27.7abc'"));
}

void duplicateIdsAndPlaces() {
    LoadResult r1 = loadBroken("[NODES]\nA, city, 1, 1\nA, city, 2, 2\n");
    CHECK(!r1.ok);
    CHECK(contains(r1.error, "test.txt:3: duplicate place 'A'"));
    LoadResult r2 = loadBroken(
        "[NODES]\nA, city, 1, 1\n[REQUESTS]\nR1, A, food, 5, 3, 10, no\nR1, A, food, 5, 3, 10, no\n");
    CHECK(!r2.ok);
    CHECK(contains(r2.error, "test.txt:5: duplicate request id 'R1'"));
}

void missingField() {
    LoadResult r = loadBroken("[NODES]\nA, city, 1\n");
    CHECK(!r.ok);
    CHECK(contains(r.error, "test.txt:2:"));
    CHECK(contains(r.error, "expected name, type, lat, lon"));
}

void unknownSection() {
    LoadResult r = loadBroken("[NODES]\nA, city, 1, 1\n[TRUCKS]\n");
    CHECK(!r.ok);
    CHECK(contains(r.error, "test.txt:3: unknown section '[TRUCKS]'"));
}

void badValues() {
    CHECK(contains(loadBroken("[NODES]\nA, village, 1, 1\n").error, "unknown node type 'village'"));
    CHECK(contains(loadBroken("[NODES]\nA, city, 1, 1\n[REQUESTS]\nR, A, gold, 1, 1, 1, no\n").error,
                   "unknown supply 'gold'"));
    CHECK(contains(loadBroken("[NODES]\nA, city, 1, 1\n[REQUESTS]\nR, A, food, 1, 9, 1, no\n").error,
                   "urgency must be 1-5"));
    CHECK(contains(loadBroken("[NODES]\nA, city, 1, 1\n[STOCK]\nA, food, 10\n").error, "'A' is not a depot"));
}

void emptyFile() {
    LoadResult r = loadBroken("# nothing here\n\n");
    CHECK(!r.ok);
    CHECK(contains(r.error, "no [NODES]"));
}

void missingFile() {
    ReliefManager m;
    LoadResult r = ScenarioLoader::loadFile("does/not/exist.txt", m);
    CHECK(!r.ok);
    CHECK(contains(r.error, "cannot open"));
}

// TODO (you): one test of your own. Idea: data before any section header,
// e.g. "Sukkur, city, 1, 1" on line 1 -> "test.txt:1: data before any [SECTION]".

}  // namespace

void runLoaderTests() {
    test::section("ScenarioLoader");
    test::runCase("goodFileLoads", goodFileLoads);
    test::runCase("unknownPlaceHasLineNumber", unknownPlaceHasLineNumber);
    test::runCase("badNumber", badNumber);
    test::runCase("duplicateIdsAndPlaces", duplicateIdsAndPlaces);
    test::runCase("missingField", missingField);
    test::runCase("unknownSection", unknownSection);
    test::runCase("badValues", badValues);
    test::runCase("emptyFile", emptyFile);
    test::runCase("missingFile", missingFile);
}
