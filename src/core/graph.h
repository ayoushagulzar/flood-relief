// graph.h - the road network as an adjacency list.
//
// Nodes = places (cities, shelters, depots). Edges = roads.
// A two-way road is stored as TWO directed edges (u->v and v->u), so
// block/reopen/setCondition must always update both.
//
// Why an adjacency list and not a matrix? ~40 places and ~60 roads:
// a matrix would be 40 x 40 = 1,600 cells with only 120 used. A list
// uses O(V + E) memory and visiting u's neighbours costs O(degree(u)).
//
// Edge weight = baseMinutes x condition factor (clear 1.0, damaged 1.5,
// flooded 2.5). Blocked roads stay in the list but Dijkstra skips them.
//
// Implementation in graph.cpp.
#pragma once
#include <cstddef>
#include <string>

#include "core/dynamic_array.h"
#include "core/hash_table.h"

enum class NodeType { City, Shelter, Depot };
enum class RoadType { Motorway, Highway, Road };
enum class RoadCondition { Clear, Damaged, Flooded };

double conditionFactor(RoadCondition c);
const char* nodeTypeName(NodeType t);
const char* roadTypeName(RoadType t);
const char* conditionName(RoadCondition c);

struct Node {
    int id = -1;
    std::string name;
    NodeType type = NodeType::City;
    double lat = 0.0;
    double lon = 0.0;
};

struct Edge {
    int to = -1;
    double baseMinutes = 0.0;
    RoadCondition condition = RoadCondition::Clear;
    RoadType type = RoadType::Road;
    std::string roadName;
    bool blocked = false;

    double weight() const { return baseMinutes * conditionFactor(condition); }
};

class Graph {
public:
    // O(1) amortized. Returns the new node id (0, 1, 2 ... in order).
    // Also stores name -> id in nameIndex_. Duplicate name -> std::invalid_argument.
    int addNode(const std::string& name, NodeType type, double lat, double lon);
    // O(1) amortized. Adds BOTH directions. Throws on invalid ids,
    // u == v (std::invalid_argument) or negative minutes.
    void addRoad(int u, int v, double minutes, RoadType type, const std::string& name);

    // O(degree). Return false if there is no road u-v. Update both directions.
    bool blockRoad(int u, int v);
    bool reopenRoad(int u, int v);
    bool setCondition(int u, int v, RoadCondition c);

    // O(degree(u)). The edge u->v, or nullptr if there is none.
    const Edge* edgeBetween(int u, int v) const;
    bool hasRoad(int u, int v) const { return edgeBetween(u, v) != nullptr; }

    // O(1). All outgoing edges of u (including blocked ones). Throws on bad id.
    const DynamicArray<Edge>& neighbors(int u) const;
    // O(1). Throws std::out_of_range on bad id.
    const Node& node(int id) const;
    std::size_t nodeCount() const { return nodes_.size(); }
    // O(1). Two-way roads, each counted once.
    std::size_t roadCount() const { return roads_; }
    // O(1) average. Node id for a name, or -1 if unknown (uses your HashTable).
    int findNode(const std::string& name) const;

    bool isValid(int id) const { return id >= 0 && static_cast<std::size_t>(id) < nodes_.size(); }

private:
    // Throws std::out_of_range if id is not a valid node.
    void checkId(int id) const;
    // Non-const helper: pointer to edge u->v inside adj_, or nullptr.
    Edge* findEdge(int u, int v);

    DynamicArray<Node> nodes_;
    DynamicArray<DynamicArray<Edge>> adj_;  // adj_[u] = roads leaving u
    HashTable nameIndex_;  // node name -> id
    std::size_t roads_ = 0;
};
