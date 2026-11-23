// this file declares the Graph (road network) using an adjacency list.
// nodes = places (city, shelter, depot), edges = roads.
// adjacency list instead of matrix bcuz we have only 97 roads for 77 places,
// a 77 x 77 matrix would be mostly empty.

#pragma once // this prevents multiple inclusions of the header file.

#include <cstddef>
#include <string>

#include "core/dynamic_array.h"
#include "core/hash_table.h"

// ======================== enum classes ==============================
enum class NodeType { City, Shelter, Depot };
enum class RoadType { Motorway, Highway, Road };
enum class RoadCondition { Clear, Damaged, Flooded };

//==================== Functions Prototypes ===========================
double conditionFactor(RoadCondition c); // clear 1.0, damaged 1.5, flooded 2.5
//converting enum to readable text
const char* nodeTypeName(NodeType t);
const char* roadTypeName(RoadType t);
const char* conditionName(RoadCondition c);

// one place on the map
struct Node {
    int id = -1;                    // also its index in nodes_
    std::string name;               // e.g. "Dadu Camp"
    NodeType type = NodeType::City;
    double lat = 0.0;
    double lon = 0.0;
};

// one direction of a road. a two way road is saved as 2 edges (u->v and v->u)
struct Edge {
    int to = -1;                                    // where the road goes
    double baseMinutes = 0.0;                       // travel time when road is clear
    RoadCondition condition = RoadCondition::Clear;
    RoadType type = RoadType::Road;
    std::string roadName;                           // e.g. "N-5" ("" if no name)
    bool blocked = false;                           // dijkstra skips blocked roads

    // actual travel time = base time x condition factor
    double weight() const { return baseMinutes * conditionFactor(condition); }
};

class Graph {
public:
    // adds a place and returns its id (0, 1, 2 ...). duplicate name -> throws
    int addNode(const std::string& name, NodeType type, double lat, double lon);
    // adds a road in BOTH directions
    void addRoad(int u, int v, double minutes, RoadType type, const std::string& name);

    // these change both directions. return false if there is no road u-v
    bool blockRoad(int u, int v);
    bool reopenRoad(int u, int v);
    bool setCondition(int u, int v, RoadCondition c);

    // returns the edge u->v or nullptr if there is no road
    const Edge* edgeBetween(int u, int v) const;
    bool hasRoad(int u, int v) const { return edgeBetween(u, v) != nullptr; }

    // all roads going out of u (blocked ones too)
    const DynamicArray<Edge>& neighbors(int u) const;
    const Node& node(int id) const;
    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t roadCount() const { return roads_; }
    // name -> id using the HashTable, -1 if not found. O(1) average
    int findNode(const std::string& name) const;

    bool isValid(int id) const { return id >= 0 && static_cast<std::size_t>(id) < nodes_.size(); }

private:
    void checkId(int id) const;    // throws if id is invalid
    Edge* findEdge(int u, int v);  // same as edgeBetween but we can change the edge

    DynamicArray<Node> nodes_;              // all places
    DynamicArray<DynamicArray<Edge>> adj_;  // adj_[u] = list of roads from u
    HashTable nameIndex_;                   // place name -> id
    std::size_t roads_ = 0;                 // no. of roads (each two way road counted once)
};
