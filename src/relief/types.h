// this file defines data types used throughout the project.

#pragma once // this prevents multiple inclusions of the header file.

#include "core/dynamic_array.h" //custom header file
#include <string>

// ======================== enum classes ==============================
// using enum classes to represent fixed options with better type safety
// (cuz implicit type conversion is not allowed)
enum class Supply {
    Food = 0,
    Water = 1,
    Medicine = 2,
    Shelter = 3
};
//constexpr = value is known at compile time
constexpr int kSupplyCount = 4;

enum class Status {
    Pending,
    Fulfilled,
    PartiallyFulfilled,
    Deferred //req is not served and is waiting for a retry.
};

//for event history
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

//==================== Functions Prototypes ===========================
//converting enum to readable text
const char* supplyName(Supply s);
const char* statusName(Status s);
const char* eventTypeName(EventType t);
//converting text to enum
bool parseSupply(const std::string& text, Supply& out); // "water" -> Supply::Water

//inline allows the same function to be defined in a header included by multiple files.
inline int supplyIndex(Supply s) {
    return static_cast<int>(s); //explicitly converts the enum value into int
}

struct Request {
    std::string id;                // "R-07"
    int locationId = -1;           // graph node where supplies are needed
    Supply supply = Supply::Food;  // requested supply type
    int quantity = 0;              // total requested units
    int delivered = 0;             // units delivered so far
    int urgency = 1;               // 1 (low) .. 5 (critical)
    int people = 0;                // no. of affected people
    bool vulnerable = false;       // e.g. children, elderly, pregnant women, injured
    int arrivalTime = 0;           // when request enter the system
    Status status = Status::Pending;//current req: status
    int firstProcessedTime = -1;   // when processNext() first looked at it (-1 = not yet)

    //calculates how many units are still needed
    int remaining() const{
        return quantity - delivered;
    }
};

//depot -> storage location containing relief supplies
struct Depot {
    int nodeId = -1;  //connects the depot to a vertex in graph
    int stock[kSupplyCount] = {0, 0, 0, 0}; //store available quantity for each supply
};

//event-> record something that happened in the system
struct Event {
    int time = 0;
    EventType type = EventType::Info;
    std::string text;
};

// One delivery from one depot.
struct DecisionLeg {
    int depotNodeId = -1;   //which depot provides supplies
    int quantity = 0;       //delivered units
    double minutes = 0.0;   //travel time
    //using dynamic array bcuz routes can have diff numbers of vertices.
    DynamicArray<int> path; // depot ... request location
    std::string via;        // road names along the path, e.g. "N-5, N-55"
};

struct SkippedDepot {
    int depotNodeId = -1;
    std::string reason; // "no water", "unreachable", "farther (request already filled)"
};

// stores the outcome of processing one request
struct Decision {
    bool valid = false;    // false if there was nothing to process
    std::string requestId;
    int requestIndex = -1;
    Status status = Status::Pending;
    int deliveredNow = 0;
    DynamicArray<DecisionLeg> legs; //individual deliveries from depots
    DynamicArray<SkippedDepot> skipped;
    std::string explanation; // one readable sentence for decision
};

//stores summary statistics about the entire relief operation
struct Stats {
    int total = 0;
    int pending = 0;
    int fulfilled = 0;
    int partial = 0;
    int deferred = 0;
    int unmet[kSupplyCount] = {0, 0, 0, 0}; // remaining units per supply
    double totalRouteMinutes = 0.0;
    double averageWaitMinutes = 0.0; // over processed requests
};

// Formatting helpers (used in explanations, CLI and GUI).
std::string formatMinutes(double minutes); // 160 -> "2 h 40 min"
std::string formatNumber(int n);           // 1200 -> "1,200"
std::string formatClock(int minutes);      // 860 -> "Day 1 22:20" (starts at 08:00)
