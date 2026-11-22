// types.h - plain data types the relief manager works with.
// (Finished plumbing: no algorithms here.)
#pragma once

#include <string>

#include "core/dynamic_array.h"

enum class Supply { Food = 0, Water = 1, Medicine = 2, Shelter = 3 };
constexpr int kSupplyCount = 4;

enum class Status { Pending, Fulfilled, PartiallyFulfilled, Deferred };

enum class EventType {
    Info,
    RequestAdded,
    Served,
    Partial,
    Deferred,
    RoadBlocked,
    RoadReopened,
    ConditionChanged,
    StockAdded,
    Retry
};

const char* supplyName(Supply s);
const char* statusName(Status s);
const char* eventTypeName(EventType t);
bool parseSupply(const std::string& text, Supply& out);  // "water" -> Supply::Water

inline int supplyIndex(Supply s) { return static_cast<int>(s); }

struct Request {
    std::string id;          // "R-07"
    int locationId = -1;     // node id in the graph
    Supply supply = Supply::Food;
    int quantity = 0;        // units asked for
    int delivered = 0;       // units delivered so far
    int urgency = 1;         // 1 (low) .. 5 (critical)
    int people = 0;          // people affected
    bool vulnerable = false; // children, elderly, pregnant women, injured
    int arrivalTime = 0;     // minutes since the scenario started
    Status status = Status::Pending;
    int firstProcessedTime = -1;  // when processNext() first looked at it (-1 = not yet)

    int remaining() const { return quantity - delivered; }
};

struct Depot {
    int nodeId = -1;
    int stock[kSupplyCount] = {0, 0, 0, 0};
};

struct Event {
    int time = 0;
    EventType type = EventType::Info;
    std::string text;
};

// One delivery from one depot.
struct DecisionLeg {
    int depotNodeId = -1;
    int quantity = 0;
    double minutes = 0.0;
    DynamicArray<int> path;  // depot ... request location
    std::string via;         // road names along the path, e.g. "N-5, N-55"
};

struct SkippedDepot {
    int depotNodeId = -1;
    std::string reason;  // "no water", "unreachable", "farther (request already filled)"
};

// What processNext() decided and WHY. This is the v1 differentiator.
struct Decision {
    bool valid = false;  // false if there was nothing to process
    std::string requestId;
    int requestIndex = -1;
    Status status = Status::Pending;
    int deliveredNow = 0;
    DynamicArray<DecisionLeg> legs;
    DynamicArray<SkippedDepot> skipped;
    std::string explanation;  // one readable sentence
};

struct Stats {
    int total = 0;
    int pending = 0;
    int fulfilled = 0;
    int partial = 0;
    int deferred = 0;
    int unmet[kSupplyCount] = {0, 0, 0, 0};  // remaining units per supply
    double totalRouteMinutes = 0.0;
    double averageWaitMinutes = 0.0;         // over processed requests
};

// Formatting helpers (used in explanations, CLI and GUI).
std::string formatMinutes(double minutes);  // 160 -> "2 h 40 min"
std::string formatNumber(int n);            // 1200 -> "1,200"
std::string formatClock(int minutes);       // 860 -> "Day 1 14:20"
