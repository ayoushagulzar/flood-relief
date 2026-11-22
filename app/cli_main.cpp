// Command-line interface for Flood Relief.
// Rule: this file only reads input, calls ReliefManager, and prints results.
// No relief logic lives here (Graph/Dijkstra are used read-only for `route`
// and `bench`).
//
// Usage:  relief_cli [scenario.txt]
// Type `help` for commands. Arguments with spaces are separated by commas:
//   block Jamshoro, Sehwan
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "core/dijkstra.h"
#include "relief/relief_manager.h"
#include "relief/scenario_loader.h"

namespace {

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

DynamicArray<std::string> splitArgs(const std::string& s) {
    DynamicArray<std::string> out;
    if (trim(s).empty()) return out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) out.pushBack(trim(part));
    return out;
}

bool fileExists(const std::string& p) { return static_cast<bool>(std::ifstream(p)); }

// The exe usually runs from build/bin, so also look a few folders up.
std::string findScenario(const std::string& name) {
    const char* prefixes[] = {"", "../", "../../", "../../../"};
    for (const char* p : prefixes) {
        std::string candidate = std::string(p) + name;
        if (fileExists(candidate)) return candidate;
    }
    return name;
}

class Cli {
public:
    void run(int argc, char** argv) {
        std::cout << "FLOOD RELIEF - command line (type 'help')\n";
        std::string first = argc > 1 ? argv[1] : findScenario("data/pakistan.txt");
        load(first);
        std::string line;
        while (true) {
            std::cout << "\n> " << std::flush;
            if (!std::getline(std::cin, line)) break;
            line = trim(line);
            if (line.empty()) continue;
            std::string cmd = line, rest;
            std::size_t sp = line.find(' ');
            if (sp != std::string::npos) {
                cmd = line.substr(0, sp);
                rest = line.substr(sp + 1);
            }
            if (cmd == "quit" || cmd == "exit") break;
            try {
                dispatch(cmd, rest);
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\n";
            }
        }
    }

private:
    std::unique_ptr<ReliefManager> m_ = std::make_unique<ReliefManager>();
    std::string loadedName_;

    void dispatch(const std::string& cmd, const std::string& rest) {
        DynamicArray<std::string> a = splitArgs(rest);
        if (cmd == "help") help();
        else if (cmd == "load") load(trim(rest));
        else if (cmd == "list") listRequests();
        else if (cmd == "depots") listDepots();
        else if (cmd == "add") addRequest(a);
        else if (cmd == "next") next();
        else if (cmd == "all") all();
        else if (cmd == "why") why();
        else if (cmd == "block") roadAction(a, 'b');
        else if (cmd == "reopen") roadAction(a, 'r');
        else if (cmd == "condition") roadAction(a, 'c');
        else if (cmd == "stock") addStock(a);
        else if (cmd == "route") route(a);
        else if (cmd == "stats") stats();
        else if (cmd == "log") showLog(a);
        else if (cmd == "time") advance(a);
        else if (cmd == "bench") bench();
        else std::cout << "Unknown command '" << cmd << "'. Type 'help'.\n";
    }

    void help() {
        std::cout << "Commands (arguments separated by commas):\n"
                     "  load <file>                         load a scenario (replaces the current one)\n"
                     "  list                                requests by status and priority\n"
                     "  depots                              stock per depot\n"
                     "  add <id>, <place>, <supply>, <qty>, <urgency 1-5>, <people>, <yes|no>\n"
                     "  next                                process the most urgent request\n"
                     "  all                                 process every queued request\n"
                     "  why                                 the last decision in full\n"
                     "  block <place>, <place>              block a road (both directions)\n"
                     "  reopen <place>, <place>             reopen a road\n"
                     "  condition <place>, <place>, <clear|damaged|flooded>\n"
                     "  stock <depot>, <supply>, <qty>      add stock\n"
                     "  route <from>, <to>                  fastest open route\n"
                     "  time <minutes>                      advance the clock\n"
                     "  stats                               totals, unmet demand, waiting time\n"
                     "  log [n]                             last n events (default 10)\n"
                     "  bench                               Dijkstra benchmark (2,000 nodes)\n"
                     "  quit\n";
    }

    void load(const std::string& path) {
        if (path.empty()) {
            std::cout << "Usage: load <file>\n";
            return;
        }
        auto fresh = std::make_unique<ReliefManager>();
        LoadResult r = ScenarioLoader::loadFile(path, *fresh);
        if (!r.ok) {
            std::cout << "Load failed: " << r.error << "\n";
            return;
        }
        m_ = std::move(fresh);
        loadedName_ = path;
        std::cout << "Loaded " << path << ": " << r.nodes << " places, " << r.roads << " roads, "
                  << m_->depots().size() << " depots, " << r.requests << " requests.\n";
    }

    int place(const std::string& name) {
        int id = m_->graph().findNode(name);
        if (id < 0) throw std::invalid_argument("unknown place '" + name + "'");
        return id;
    }

    const std::string& name(int id) { return m_->graph().node(id).name; }

    void listRequests() {
        const DynamicArray<Request>& rs = m_->requests();
        if (rs.isEmpty()) {
            std::cout << "No requests.\n";
            return;
        }
        std::cout << std::left << std::setw(7) << "ID" << std::setw(17) << "Location" << std::setw(10) << "Supply"
                  << std::setw(13) << "Delivered" << std::setw(4) << "U" << std::setw(8) << "People"
                  << std::setw(8) << "Score" << "Status\n";
        for (std::size_t i = 0; i < rs.size(); ++i) {
            const Request& r = rs[i];
            std::cout << std::left << std::setw(7) << r.id << std::setw(17) << name(r.locationId) << std::setw(10)
                      << supplyName(r.supply) << std::setw(13)
                      << (std::to_string(r.delivered) + "/" + std::to_string(r.quantity)) << std::setw(4)
                      << r.urgency << std::setw(8) << r.people << std::setw(8) << std::fixed
                      << std::setprecision(0) << ReliefManager::heapKey(r) << statusName(r.status)
                      << (r.vulnerable ? "  (vulnerable)" : "") << "\n";
        }
        std::cout << "Queued: " << (m_->hasPending() ? "yes" : "no") << ", waiting for retry: "
                  << m_->deferredCount() << "\n";
    }

    void listDepots() {
        const DynamicArray<Depot>& ds = m_->depots();
        std::cout << std::left << std::setw(18) << "Depot" << std::setw(9) << "food" << std::setw(9) << "water"
                  << std::setw(10) << "medicine" << "shelter\n";
        for (std::size_t i = 0; i < ds.size(); ++i) {
            std::cout << std::left << std::setw(18) << name(ds[i].nodeId);
            for (int s = 0; s < kSupplyCount; ++s) std::cout << std::setw(s == 2 ? 10 : 9) << ds[i].stock[s];
            std::cout << "\n";
        }
    }

    void addRequest(const DynamicArray<std::string>& a) {
        if (a.size() != 7) {
            std::cout << "Usage: add <id>, <place>, <supply>, <qty>, <urgency 1-5>, <people>, <yes|no>\n";
            return;
        }
        Request r;
        r.id = a[0];
        r.locationId = place(a[1]);
        if (!parseSupply(a[2], r.supply)) throw std::invalid_argument("unknown supply '" + a[2] + "'");
        r.quantity = std::stoi(a[3]);
        r.urgency = std::stoi(a[4]);
        r.people = std::stoi(a[5]);
        r.vulnerable = (a[6] == "yes" || a[6] == "y");
        m_->addRequest(r);
        std::cout << "Added " << r.id << ".\n";
    }

    void printDecision(const Decision& d) {
        if (!d.valid) {
            std::cout << d.explanation << "\n";
            return;
        }
        std::cout << "[" << statusName(d.status) << "] " << d.explanation << "\n";
    }

    void next() { printDecision(m_->processNext()); }

    void all() {
        int n = 0;
        while (m_->hasPending()) {
            printDecision(m_->processNext());
            ++n;
        }
        std::cout << "Processed " << n << " request(s).\n";
    }

    void why() {
        const Decision& d = m_->lastDecision();
        if (!d.valid) {
            std::cout << "No decision yet. Use 'next'.\n";
            return;
        }
        const Request* r = m_->findRequest(d.requestId);
        std::cout << "Request " << d.requestId;
        if (r)
            std::cout << " at " << name(r->locationId) << ": priority key " << std::fixed << std::setprecision(0)
                      << ReliefManager::heapKey(*r) << " (base " << ReliefManager::baseScore(*r)
                      << " - 2 x arrival " << r->arrivalTime << ")";
        std::cout << "\nStatus: " << statusName(d.status) << ", delivered now: " << d.deliveredNow << "\n";
        for (std::size_t i = 0; i < d.legs.size(); ++i) {
            const DecisionLeg& l = d.legs[i];
            std::cout << "  leg " << i + 1 << ": " << l.quantity << " from " << name(l.depotNodeId) << ", "
                      << formatMinutes(l.minutes) << " via " << l.via << "\n         ";
            for (std::size_t k = 0; k < l.path.size(); ++k) std::cout << (k ? " > " : "") << name(l.path[k]);
            std::cout << "\n";
        }
        for (std::size_t i = 0; i < d.skipped.size(); ++i)
            std::cout << "  skipped " << name(d.skipped[i].depotNodeId) << ": " << d.skipped[i].reason << "\n";
    }

    void roadAction(const DynamicArray<std::string>& a, char kind) {
        if (a.size() < 2 || (kind == 'c' && a.size() != 3)) {
            std::cout << "Usage: block|reopen <place>, <place>   or   condition <place>, <place>, <clear|damaged|flooded>\n";
            return;
        }
        int u = place(a[0]), v = place(a[1]);
        bool ok = false;
        if (kind == 'b') ok = m_->blockRoad(u, v);
        else if (kind == 'r') ok = m_->reopenRoad(u, v);
        else {
            RoadCondition c;
            if (a[2] == "clear") c = RoadCondition::Clear;
            else if (a[2] == "damaged") c = RoadCondition::Damaged;
            else if (a[2] == "flooded") c = RoadCondition::Flooded;
            else throw std::invalid_argument("condition must be clear, damaged or flooded");
            ok = m_->setRoadCondition(u, v, c);
        }
        if (!ok) std::cout << "There is no direct road between " << a[0] << " and " << a[1] << ".\n";
        else std::cout << m_->events().back().text << "\n";
        if (ok && m_->hasPending() && kind != 'b') std::cout << "(deferred requests are queued again: try 'next')\n";
    }

    void addStock(const DynamicArray<std::string>& a) {
        if (a.size() != 3) {
            std::cout << "Usage: stock <depot>, <supply>, <qty>\n";
            return;
        }
        Supply s;
        if (!parseSupply(a[1], s)) throw std::invalid_argument("unknown supply '" + a[1] + "'");
        m_->addStock(place(a[0]), s, std::stoi(a[2]));
        std::cout << m_->events().back().text << "\n";
    }

    void route(const DynamicArray<std::string>& a) {
        if (a.size() != 2) {
            std::cout << "Usage: route <from>, <to>\n";
            return;
        }
        int u = place(a[0]), v = place(a[1]);
        Dijkstra d(m_->graph());
        d.start(u);
        d.run();
        if (!d.reachable(v)) {
            std::cout << "No open route from " << a[0] << " to " << a[1] << ".\n";
            return;
        }
        DynamicArray<int> p = d.pathTo(v);
        std::cout << formatMinutes(d.distanceTo(v)) << " (" << d.visitedCount() << " places explored)\n";
        for (std::size_t i = 0; i + 1 < p.size(); ++i) {
            const Edge* e = m_->graph().edgeBetween(p[i], p[i + 1]);
            std::cout << "  " << name(p[i]) << " > " << name(p[i + 1]) << "  " << e->roadName << ", "
                      << formatMinutes(e->weight())
                      << (e->condition != RoadCondition::Clear ? std::string(" (") + conditionName(e->condition) + ")"
                                                               : std::string())
                      << "\n";
        }
    }

    void stats() {
        Stats s = m_->stats();
        std::cout << "Requests: " << s.total << "  fulfilled " << s.fulfilled << ", partial " << s.partial
                  << ", deferred " << s.deferred << ", pending " << s.pending << "\n";
        std::cout << "Unmet demand:";
        for (int i = 0; i < kSupplyCount; ++i)
            std::cout << " " << supplyName(static_cast<Supply>(i)) << " " << s.unmet[i];
        std::cout << "\nTotal route time: " << formatMinutes(s.totalRouteMinutes)
                  << ", average wait before processing: " << formatMinutes(s.averageWaitMinutes) << "\n";
        std::cout << "Clock: " << formatClock(m_->now()) << "\n";
    }

    void showLog(const DynamicArray<std::string>& a) {
        const DynamicArray<Event>& ev = m_->events();
        std::size_t n = a.isEmpty() ? 10 : static_cast<std::size_t>(std::stoi(a[0]));
        std::size_t start = ev.size() > n ? ev.size() - n : 0;
        for (std::size_t i = ev.size(); i > start; --i) {  // newest first
            const Event& e = ev[i - 1];
            std::cout << formatClock(e.time) << "  " << std::left << std::setw(10) << eventTypeName(e.type)
                      << e.text << "\n";
        }
    }

    void advance(const DynamicArray<std::string>& a) {
        if (a.size() != 1) {
            std::cout << "Usage: time <minutes>\n";
            return;
        }
        m_->advanceTime(std::stoi(a[0]));
        std::cout << "Clock: " << formatClock(m_->now()) << "\n";
    }

    // Random connected graph: a spanning tree (node i links to a random earlier
    // node) plus extra random roads, ~6,000 roads in total.
    void bench() {
        const int kNodes = 2000, kRoads = 6000, kRuns = 100;
        Graph g;
        unsigned int seed = 12345;
        auto rnd = [&seed](int n) {
            seed = seed * 1103515245u + 12345u;
            return static_cast<int>((seed >> 8) % static_cast<unsigned int>(n));
        };
        for (int i = 0; i < kNodes; ++i) g.addNode("n" + std::to_string(i), NodeType::City, 0, 0);
        for (int i = 1; i < kNodes; ++i) g.addRoad(i, rnd(i), 1 + rnd(120), RoadType::Road, "r");
        while (g.roadCount() < static_cast<std::size_t>(kRoads)) {
            int u = rnd(kNodes), v = rnd(kNodes);
            if (u != v && !g.hasRoad(u, v)) g.addRoad(u, v, 1 + rnd(120), RoadType::Road, "r");
        }
        Dijkstra d(g);
        auto t0 = std::chrono::steady_clock::now();
        std::size_t visited = 0;
        for (int r = 0; r < kRuns; ++r) {
            d.start(rnd(kNodes));
            d.run();
            visited += d.visitedCount();
        }
        auto t1 = std::chrono::steady_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / kRuns;
        std::cout << "Dijkstra on " << kNodes << " nodes / " << g.roadCount() << " roads: " << std::fixed
                  << std::setprecision(3) << ms << " ms per run (average of " << kRuns << ", "
                  << visited / kRuns << " nodes settled per run)\n";
    }
};

}  // namespace

int main(int argc, char** argv) {
    Cli cli;
    cli.run(argc, argv);
    return 0;
}
