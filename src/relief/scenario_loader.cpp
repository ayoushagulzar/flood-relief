// scenario_loader.cpp - text file -> ReliefManager.
// How errors work: any helper can call fail("message"). That throws a LoadError,
// which loadText() catches ONCE and turns into "file:line: message", so the
// parse functions never deal with line numbers themselves.
#include "relief/scenario_loader.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string lower(const std::string& s) {
    std::string out;
    for (char c : s) out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// Splits "a, b ,c" into trimmed fields.
DynamicArray<std::string> splitFields(const std::string& line) {
    DynamicArray<std::string> out;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) out.pushBack(trim(field));
    return out;
}

// Thrown inside the loader; caught once and turned into "file:line: message".
struct LoadError {
    std::string message;
};

[[noreturn]] void fail(const std::string& msg) { throw LoadError{msg}; }

double toDouble(const std::string& s, const char* what) {
    try {
        std::size_t used = 0;
        double v = std::stod(s, &used);
        if (used != s.size()) throw std::invalid_argument("junk");
        return v;
    } catch (const std::exception&) {
        fail(std::string("bad number '") + s + "' for " + what);
    }
}

int toInt(const std::string& s, const char* what) {
    try {
        std::size_t used = 0;
        int v = std::stoi(s, &used);
        if (used != s.size()) throw std::invalid_argument("junk");
        return v;
    } catch (const std::exception&) {
        fail(std::string("bad whole number '") + s + "' for " + what);
    }
}

int place(const ReliefManager& m, const std::string& name, const char* section) {
    int id = m.graph().findNode(name);
    if (id < 0) fail("unknown place '" + name + "' in " + section);
    return id;
}

void needFields(const DynamicArray<std::string>& f, std::size_t min, std::size_t max, const char* section,
                const char* format) {
    if (f.size() < min || f.size() > max)
        fail(std::string("expected ") + format + " in " + section + ", got " + std::to_string(f.size()) +
             " field(s)");
    for (std::size_t i = 0; i < f.size(); ++i)
        if (f[i].empty()) fail(std::string("empty field ") + std::to_string(i + 1) + " in " + section);
}

enum class Section { None, Nodes, Roads, Stock, Requests };

void parseNode(const DynamicArray<std::string>& f, ReliefManager& m) {
    needFields(f, 4, 4, "[NODES]", "name, type, lat, lon");
    std::string t = lower(f[1]);
    NodeType type;
    if (t == "city") type = NodeType::City;
    else if (t == "shelter") type = NodeType::Shelter;
    else if (t == "depot") type = NodeType::Depot;
    else fail("unknown node type '" + f[1] + "' (use city, shelter or depot)");
    if (m.graph().findNode(f[0]) >= 0) fail("duplicate place '" + f[0] + "'");
    double lat = toDouble(f[2], "lat");
    double lon = toDouble(f[3], "lon");
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) fail("coordinates out of range for '" + f[0] + "'");
    int id = m.graph().addNode(f[0], type, lat, lon);
    if (type == NodeType::Depot) m.addDepot(id);
}

void parseRoad(const DynamicArray<std::string>& f, ReliefManager& m) {
    needFields(f, 5, 6, "[ROADS]", "from, to, minutes, type, name [, condition]");
    int u = place(m, f[0], "[ROADS]");
    int v = place(m, f[1], "[ROADS]");
    if (u == v) fail("road from '" + f[0] + "' to itself");
    double minutes = toDouble(f[2], "minutes");
    if (minutes <= 0) fail("minutes must be > 0");
    std::string t = lower(f[3]);
    RoadType type;
    if (t == "motorway") type = RoadType::Motorway;
    else if (t == "highway") type = RoadType::Highway;
    else if (t == "road") type = RoadType::Road;
    else fail("unknown road type '" + f[3] + "' (use motorway, highway or road)");
    if (m.graph().hasRoad(u, v)) fail("duplicate road '" + f[0] + "' - '" + f[1] + "'");
    m.graph().addRoad(u, v, minutes, type, f[4] == "-" ? "" : f[4]);  // "-" = unnamed
    if (f.size() == 6) {
        std::string c = lower(f[5]);
        if (c == "clear") {
        } else if (c == "damaged") m.graph().setCondition(u, v, RoadCondition::Damaged);
        else if (c == "flooded") m.graph().setCondition(u, v, RoadCondition::Flooded);
        else if (c == "blocked") m.graph().blockRoad(u, v);
        else fail("unknown condition '" + f[5] + "' (use clear, damaged, flooded or blocked)");
    }
}

void parseStock(const DynamicArray<std::string>& f, ReliefManager& m) {
    needFields(f, 3, 3, "[STOCK]", "depot, supply, quantity");
    int id = place(m, f[0], "[STOCK]");
    if (m.graph().node(id).type != NodeType::Depot) fail("'" + f[0] + "' is not a depot");
    Supply s;
    if (!parseSupply(f[1], s)) fail("unknown supply '" + f[1] + "' (use food, water, medicine or shelter)");
    int q = toInt(f[2], "quantity");
    if (q <= 0) fail("quantity must be > 0");
    m.addStock(id, s, q);
}

void parseRequest(const DynamicArray<std::string>& f, ReliefManager& m) {
    needFields(f, 7, 7, "[REQUESTS]", "id, location, supply, quantity, urgency, people, vulnerable");
    Request r;
    r.id = f[0];
    if (m.findRequest(r.id)) fail("duplicate request id '" + r.id + "'");
    r.locationId = place(m, f[1], "[REQUESTS]");
    if (!parseSupply(f[2], r.supply)) fail("unknown supply '" + f[2] + "' (use food, water, medicine or shelter)");
    r.quantity = toInt(f[3], "quantity");
    if (r.quantity <= 0) fail("quantity must be > 0");
    r.urgency = toInt(f[4], "urgency");
    if (r.urgency < 1 || r.urgency > 5) fail("urgency must be 1-5, got " + f[4]);
    r.people = toInt(f[5], "people");
    if (r.people < 0) fail("people must be >= 0");
    std::string v = lower(f[6]);
    if (v == "yes" || v == "true" || v == "1") r.vulnerable = true;
    else if (v == "no" || v == "false" || v == "0") r.vulnerable = false;
    else fail("vulnerable must be yes or no, got '" + f[6] + "'");
    m.addRequest(r);
}

}  // namespace

LoadResult ScenarioLoader::loadText(const std::string& text, const std::string& name, ReliefManager& m) {
    LoadResult res;
    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    Section section = Section::None;
    try {
        while (std::getline(in, raw)) {
            ++lineNo;
            std::size_t hash = raw.find('#');
            std::string line = trim(hash == std::string::npos ? raw : raw.substr(0, hash));
            if (line.empty()) continue;
            if (line.front() == '[') {
                std::string s = lower(line);
                if (s == "[nodes]") section = Section::Nodes;
                else if (s == "[roads]") section = Section::Roads;
                else if (s == "[stock]") section = Section::Stock;
                else if (s == "[requests]") section = Section::Requests;
                else fail("unknown section '" + line + "'");
                continue;
            }
            DynamicArray<std::string> f = splitFields(line);
            switch (section) {
                case Section::None: fail("data before any [SECTION] header");
                case Section::Nodes: parseNode(f, m); ++res.nodes; break;
                case Section::Roads: parseRoad(f, m); ++res.roads; break;
                case Section::Stock: parseStock(f, m); ++res.stockLines; break;
                case Section::Requests: parseRequest(f, m); ++res.requests; break;
            }
        }
    } catch (const LoadError& e) {
        res.error = name + ":" + std::to_string(lineNo) + ": " + e.message;
        return res;
    } catch (const std::exception& e) {  // something the manager/graph refused
        res.error = name + ":" + std::to_string(lineNo) + ": " + e.what();
        return res;
    }
    if (res.nodes == 0) {
        res.error = name + ": no [NODES] found (empty or wrong file?)";
        return res;
    }
    res.ok = true;
    return res;
}

LoadResult ScenarioLoader::loadFile(const std::string& path, ReliefManager& m) {
    std::ifstream file(path);
    if (!file) {
        LoadResult res;
        res.error = path + ": cannot open file";
        return res;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string name = path;
    std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    return loadText(buffer.str(), name, m);
}
