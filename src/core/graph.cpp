// graph.cpp - road network as an adjacency list (see graph.h for the design).
#include "core/graph.h"

#include <stdexcept>

double conditionFactor(RoadCondition c) {
    switch (c) {
        case RoadCondition::Clear: return 1.0;
        case RoadCondition::Damaged: return 1.5;
        case RoadCondition::Flooded: return 2.5;
    }
    return 1.0;
}

const char* nodeTypeName(NodeType t) {
    switch (t) {
        case NodeType::City: return "city";
        case NodeType::Shelter: return "shelter";
        case NodeType::Depot: return "depot";
    }
    return "?";
}

const char* roadTypeName(RoadType t) {
    switch (t) {
        case RoadType::Motorway: return "motorway";
        case RoadType::Highway: return "highway";
        case RoadType::Road: return "road";
    }
    return "?";
}

const char* conditionName(RoadCondition c) {
    switch (c) {
        case RoadCondition::Clear: return "clear";
        case RoadCondition::Damaged: return "damaged";
        case RoadCondition::Flooded: return "flooded";
    }
    return "?";
}

void Graph::checkId(int id) const {
    if (!isValid(id)) throw std::out_of_range("Graph: invalid node id " + std::to_string(id));
}

// O(1) amortized. ids are given in order 0, 1, 2 ... so a node id is also its
// index in nodes_ and adj_. The name -> id pair goes into the HashTable.
int Graph::addNode(const std::string& name, NodeType type, double lat, double lon) {
    if (nameIndex_.contains(name)) throw std::invalid_argument("Graph: duplicate place name '" + name + "'");
    Node n;
    n.id = static_cast<int>(nodes_.size());
    n.name = name;
    n.type = type;
    n.lat = lat;
    n.lon = lon;
    nodes_.pushBack(n);
    adj_.pushBack(DynamicArray<Edge>());
    nameIndex_.insert(name, n.id);
    return n.id;
}

// O(1) amortized. ONE road = TWO directed edges (u->v and v->u), so roads_ is
// counted once but each adjacency list gets one edge.
void Graph::addRoad(int u, int v, double minutes, RoadType type, const std::string& name) {
    checkId(u);
    checkId(v);
    if (u == v) throw std::invalid_argument("Graph: a road cannot start and end at the same node");
    if (minutes < 0) throw std::invalid_argument("Graph: negative travel time");
    Edge e;
    e.baseMinutes = minutes;
    e.type = type;
    e.roadName = name;
    e.to = v;
    adj_[static_cast<std::size_t>(u)].pushBack(e);
    e.to = u;
    adj_[static_cast<std::size_t>(v)].pushBack(e);
    ++roads_;
}

// O(degree(u)). Pointer INTO adj_[u] (the list is taken by reference, so we
// return the real edge, not a copy). nullptr if u has no road to v.
Edge* Graph::findEdge(int u, int v) {
    DynamicArray<Edge>& list = adj_[static_cast<std::size_t>(u)];
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].to == v) return &list[i];
    return nullptr;
}

const Edge* Graph::edgeBetween(int u, int v) const {
    checkId(u);
    checkId(v);
    const DynamicArray<Edge>& list = adj_[static_cast<std::size_t>(u)];
    for (std::size_t i = 0; i < list.size(); ++i)
        if (list[i].to == v) return &list[i];
    return nullptr;
}

// O(degree). Both directions must change, otherwise Dijkstra could still use
// the road from the other side.
bool Graph::blockRoad(int u, int v) {
    checkId(u);
    checkId(v);
    Edge* a = findEdge(u, v);
    Edge* b = findEdge(v, u);
    if (!a || !b) return false;
    a->blocked = b->blocked = true;
    return true;
}

bool Graph::reopenRoad(int u, int v) {
    checkId(u);
    checkId(v);
    Edge* a = findEdge(u, v);
    Edge* b = findEdge(v, u);
    if (!a || !b) return false;
    a->blocked = b->blocked = false;
    return true;
}

bool Graph::setCondition(int u, int v, RoadCondition c) {
    checkId(u);
    checkId(v);
    Edge* a = findEdge(u, v);
    Edge* b = findEdge(v, u);
    if (!a || !b) return false;
    a->condition = b->condition = c;
    return true;
}

const DynamicArray<Edge>& Graph::neighbors(int u) const {
    checkId(u);
    return adj_[static_cast<std::size_t>(u)];
}

const Node& Graph::node(int id) const {
    checkId(id);
    return nodes_[static_cast<std::size_t>(id)];
}

// O(1) average: one HashTable lookup.
int Graph::findNode(const std::string& name) const {
    int id = -1;
    return nameIndex_.find(name, id) ? id : -1;
}
