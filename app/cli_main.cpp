// this file is the command line program (relief_cli).
// it only reads commands, calls ReliefManager and prints the result.
// usage: relief_cli [scenario.txt] [--no-color]
// e.g. "block Jamshoro, Sehwan" (commas between place names)

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

// windows only headers (for colours and UTF-8 in the console)
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "core/dijkstra.h"
#include "relief/relief_manager.h"
#include "relief/scenario_loader.h"

// anonymous namespace -> only used in this file
namespace {

// helpers for colours, padding and boxes
namespace term {

bool gColor = true; // false = print without colours

// colours used in the terminal
enum class C { Yellow, Pink, Green, Frame, White, Grey, Red, Orange, Blue };

// ANSI escape code for each colour (24-bit colour)
std::string code(C c) {
    if (!gColor) return "";
    switch (c) {
        case C::Yellow: return "\x1b[38;2;245;214;60m";
        case C::Pink: return "\x1b[38;2;232;122;232m";
        case C::Green: return "\x1b[38;2;40;200;140m";
        case C::Frame: return "\x1b[38;2;30;140;100m";
        case C::White: return "\x1b[38;2;235;235;235m";
        case C::Grey: return "\x1b[38;2;140;140;140m";
        case C::Red: return "\x1b[38;2;255;90;80m";
        case C::Orange: return "\x1b[38;2;255;170;60m";
        case C::Blue: return "\x1b[38;2;90;160;255m";
    }
    return "";
}
std::string bold() { return gColor ? "\x1b[1m" : ""; }
std::string reset() { return gColor ? "\x1b[0m" : ""; }

// paint(C::Green, "served") -> "served" in green
std::string paint(C c, const std::string& s, bool strong = false) { return (strong ? bold() : "") + code(c) + s + reset(); }

// counts characters, not bytes ("→" is 3 bytes but 1 character)
std::size_t width(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char ch : s)
        if ((ch & 0xC0) != 0x80) ++n;
    return n;
}
// pad = spaces on the right, padLeft = spaces on the left, fit = cuts with "..."
std::string pad(const std::string& s, std::size_t w) { return width(s) >= w ? s : s + std::string(w - width(s), ' '); }
std::string padLeft(const std::string& s, std::size_t w) { return width(s) >= w ? s : std::string(w - width(s), ' ') + s; }
std::string fit(const std::string& s, std::size_t w) { return s.size() <= w ? s : s.substr(0, w - 3) + "..."; }

// returns text in CAPITALS
std::string upper_copy(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

// repeat("=", 3) -> "==="
std::string repeat(const std::string& piece, std::size_t n) {
    std::string out;
    for (std::size_t i = 0; i < n; ++i) out += piece;
    return out;
}

// breaks long text into lines of max w characters
DynamicArray<std::string> wrap(const std::string& text, std::size_t w) {
    DynamicArray<std::string> lines;
    std::istringstream words(text);
    std::string word, line;
    while (words >> word) {
        if (!line.empty() && width(line) + 1 + width(word) > w) {
            lines.pushBack(line);
            line.clear();
        }
        line += (line.empty() ? "" : " ") + word;
    }
    if (!line.empty()) lines.pushBack(line);
    return lines;
}

// one line in a box. plain = same text without colour codes (used to measure it)
struct Line {
    std::string shown;
    std::string plain;
};
Line line(const std::string& shown, const std::string& plain) { return Line{shown, plain}; }
Line blank() { return Line{"", ""}; }

// draws a box around the lines (used for the green decision box)
void box(const DynamicArray<Line>& lines, std::size_t w = 76) {
    std::string h = gColor ? "\xE2\x94\x80" : "-", v = gColor ? "\xE2\x94\x82" : "|";
    std::string tl = gColor ? "\xE2\x94\x8C" : "+", tr = gColor ? "\xE2\x94\x90" : "+";
    std::string bl = gColor ? "\xE2\x94\x94" : "+", br = gColor ? "\xE2\x94\x98" : "+";
    std::cout << "  " << paint(C::Frame, tl + repeat(h, w - 2) + tr) << "\n";
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::size_t len = width(lines[i].plain);
        std::size_t space = len < w - 2 ? w - 2 - len : 0;
        std::size_t left = space / 2, right = space - left;
        std::cout << "  " << paint(C::Frame, v) << std::string(left, ' ') << lines[i].shown << std::string(right, ' ')
                  << paint(C::Frame, v) << "\n";
    }
    std::cout << "  " << paint(C::Frame, bl + repeat(h, w - 2) + br) << "\n";
}

// decides if we use colours. on windows it also turns on colours + UTF-8
void setup(bool noColorFlag) {
    bool tty;
#ifdef _WIN32
    tty = _isatty(_fileno(stdout)) != 0;
    SetConsoleOutputCP(CP_UTF8);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
        SetConsoleMode(out, mode | 0x0004);
#else
    tty = isatty(1) != 0;
#endif
    const char* t = std::getenv("TERM");
    bool termOk = t && std::string(t) != "dumb";
    gColor = !noColorFlag && std::getenv("NO_COLOR") == nullptr && (tty || termOk);
}

}

// so we can write C::Green instead of term::C::Green
using term::C;
using term::paint;

// symbols written as UTF-8 bytes
const char* kDot = "\xC2\xB7";
const char* kArrow = "\xE2\x86\x92";
const char* kServed = "\xE2\x97\x8F";
const char* kPartial = "\xE2\x97\x8B";
const char* kCross = "\xC3\x97";

// removes spaces at start and end
std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// "Jamshoro, Sehwan" -> ["Jamshoro", "Sehwan"]
DynamicArray<std::string> splitArgs(const std::string& s) {
    DynamicArray<std::string> out;
    if (trim(s).empty()) return out;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) out.pushBack(trim(part));
    return out;
}

bool fileExists(const std::string& p) { return static_cast<bool>(std::ifstream(p)); }

// finds data/pakistan.txt, the exe is in build/bin so it also checks folders above
std::string findScenario(const std::string& name) {
    const char* prefixes[] = {"", "../", "../../", "../../../"};
    for (const char* p : prefixes) {
        std::string candidate = std::string(p) + name;
        if (fileExists(candidate)) return candidate;
    }
    return name;
}

// "Sukkur Depot" -> "Sukkur"
std::string withoutDepotWord(const std::string& name) {
    const std::string suffix = " Depot";
    if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        return name.substr(0, name.size() - suffix.size());
    return name;
}

// symbol + word + colour for each status
struct Look {
    const char* mark;
    const char* word;
    C color;
};
Look lookOf(Status s) {
    switch (s) {
        case Status::Fulfilled: return {kServed, "SERVED", C::Green};
        case Status::PartiallyFulfilled: return {kPartial, "PARTIAL", C::Orange};
        case Status::Deferred: return {kCross, "WAITING", C::Red};
        case Status::Pending: return {" ", "QUEUED", C::Grey};
    }
    return {" ", "?", C::Grey};
}

// 5 red, 4 orange, 3 yellow, rest grey
C urgencyColor(int u) { return u >= 5 ? C::Red : (u == 4 ? C::Orange : (u == 3 ? C::Yellow : C::Grey)); }

// colour for each event type in the log
C eventColor(EventType t) {
    switch (t) {
        case EventType::Served:
        case EventType::RoadReopened: return C::Green;
        case EventType::Partial: return C::Orange;
        case EventType::Deferred:
        case EventType::RoadBlocked: return C::Red;
        case EventType::Retry: return C::Yellow;
        case EventType::ConditionChanged: return C::Blue;
        default: return C::Grey;
    }
}

// the CLI program itself
class Cli {
public:
    // reads arguments, loads the file, then keeps reading commands until quit
    void run(int argc, char** argv) {
        bool noColor = false;
        std::string file;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--no-color") noColor = true;
            else if (file.empty()) file = a;
        }
        term::setup(noColor);
        banner();
        load(file.empty() ? findScenario("data/pakistan.txt") : file, true);
        help();

        std::string input;
        while (true) {
            std::cout << "\n" << paint(C::Yellow, "flood", true) << " > " << std::flush;
            if (!std::getline(std::cin, input)) break;
            input = trim(input);
            if (input.empty()) continue;
            std::string cmd = input, rest; // first word = command, rest = arguments
            std::size_t sp = input.find(' ');
            if (sp != std::string::npos) {
                cmd = input.substr(0, sp);
                rest = input.substr(sp + 1);
            }
            if (cmd == "quit" || cmd == "exit") break;
            std::cout << "\n";
            try {
                dispatch(cmd, rest);
            } catch (const std::exception& e) { // e.g. unknown place -> show error and continue
                error(e.what());
            }
        }
        std::cout << "\n";
    }

private:
    std::unique_ptr<ReliefManager> m_ = std::make_unique<ReliefManager>(); // unique_ptr -> deletes itself, and we can swap it on "load"
    std::string loadedName_;

    // title line at the start
    static void banner() {
        std::string rule = term::gColor ? term::repeat("\xE2\x95\x90", 76) : std::string(76, '=');
        std::string dot = std::string("  ") + kDot + "  ";
        std::cout << "\n  " << paint(C::Yellow, "FLOOD RELIEF", true)
                  << paint(C::Grey, dot + "relief_cli 1.0" + dot + "C++17" + dot + "type 'help' for commands") << "\n"
                  << "  " << paint(C::Frame, rule) << "\n\n"
                  << "  " << paint(C::Pink, "Usage: relief_cli") << "\n";
    }

    // small print helpers
    static void error(const std::string& msg) {
        std::cout << "  " << paint(C::Red, std::string(kCross) + " Error:", true) << " " << msg << "\n";
    }

    static void note(const std::string& msg) { std::cout << "  " << paint(C::Grey, msg) << "\n"; }

    static void heading(const std::string& title) { std::cout << "  " << paint(C::White, title, true) << "\n"; }

    // command -> function
    void dispatch(const std::string& cmd, const std::string& rest) {
        DynamicArray<std::string> a = splitArgs(rest);
        if (cmd == "help") help();
        else if (cmd == "load") load(trim(rest), false);
        else if (cmd == "list") listRequests();
        else if (cmd == "queue") showQueue();
        else if (cmd == "depots") listDepots();
        else if (cmd == "add") addRequest(a);
        else if (cmd == "next") next();
        else if (cmd == "all") all();
        else if (cmd == "why") why();
        else if (cmd == "block") roadAction(a, rest, 'b');
        else if (cmd == "reopen") roadAction(a, rest, 'r');
        else if (cmd == "condition") roadAction(a, rest, 'c');
        else if (cmd == "stock") addStock(a, rest);
        else if (cmd == "route") route(a, rest);
        else if (cmd == "stats") stats();
        else if (cmd == "log") showLog(a);
        else if (cmd == "time") advance(a);
        else if (cmd == "bench") bench();
        else error("unknown command '" + cmd + "'. Type 'help'.");
    }

    // prints all the commands
    static void help() {
        struct Row {
            const char* cmd;
            const char* what;
            const char* tag;
        };
        const Row rows[] = {
            {"next", "Serve the most urgent request", "[process]"},
            {"all", "Serve every queued request", "[process]"},
            {"why", "Explain the last decision", "[process]"},
            {"block <a>, <b>", "Close a road (both directions)", "[world]"},
            {"reopen <a>, <b>", "Reopen a road", "[world]"},
            {"condition <a>, <b>, <c>", "clear | damaged | flooded", "[world]"},
            {"stock <depot>, <supply>, <n>", "Add stock to a depot", "[world]"},
            {"add <id>, <place>, ...", "Add a request ('add' = help)", "[world]"},
            {"time <minutes>", "Move the clock forward", "[world]"},
            {"list", "Requests by priority + heap key", "[view]"},
            {"queue", "The heap itself + deferred list", "[view]"},
            {"depots", "Stock per depot", "[view]"},
            {"route <a>, <b>", "Fastest open route", "[view]"},
            {"stats", "Totals and unmet demand", "[view]"},
            {"log [n]", "Last n events", "[view]"},
            {"bench", "Dijkstra speed test", "[view]"},
            {"load <file>", "Load another scenario", ""},
            {"quit", "Exit", ""},
        };
        std::cout << "\n";
        heading("Commands:");
        for (const Row& r : rows)
            std::cout << "    " << paint(C::White, term::pad(r.cmd, 30)) << term::pad(r.what, 34)
                      << paint(C::Grey, term::padLeft(r.tag, 10)) << "\n";
        std::cout << "\n  " << paint(C::Grey, "Places: ") << paint(C::Pink, "block Jamshoro, Sehwan") << paint(C::Grey, "  or  ")
                  << paint(C::Pink, "block jamshoro sehwan") << "\n";
    }

    // loads into a new manager first, only replaces the old one if loading worked
    void load(const std::string& path, bool startup) {
        if (path.empty()) {
            note("Usage: load <file>");
            return;
        }
        auto fresh = std::make_unique<ReliefManager>();
        LoadResult r = ScenarioLoader::loadFile(path, *fresh);
        if (!r.ok) {
            if (startup) std::cout << "\n";
            error("could not load " + r.error);
            return;
        }
        m_ = std::move(fresh);
        loadedName_ = path;

        std::string title = "Disaster relief allocation and route planning for Pakistan";
        std::string info = "Loaded " + path + "   " + std::to_string(r.nodes) + " places " + kDot + " " +
                           std::to_string(r.roads) + " roads " + kDot + " " + std::to_string(m_->depots().size()) +
                           " depots " + kDot + " " + std::to_string(r.requests) + " requests";
        DynamicArray<term::Line> lines;
        lines.pushBack(term::blank());
        lines.pushBack(term::line(paint(C::Green, title), title));
        lines.pushBack(term::line(paint(C::Grey, info), info));
        lines.pushBack(term::blank());
        if (!startup) std::cout << "\n";
        term::box(lines);
    }

    // name -> id. first exact match (hash table), then not case sensitive ("jamshoro" works)
    int findPlace(const std::string& name) const {
        int id = m_->graph().findNode(name);
        if (id >= 0) return id;
        std::string want = term::upper_copy(name);
        for (std::size_t i = 0; i < m_->graph().nodeCount(); ++i)
            if (term::upper_copy(m_->graph().node(static_cast<int>(i)).name) == want) return static_cast<int>(i);
        return -1;
    }

    // same but throws if not found
    int place(const std::string& name) {
        int id = findPlace(name);
        if (id < 0) throw std::invalid_argument("unknown place '" + name + "' (check spelling; 'list' and 'depots' show names)");
        return id;
    }

    // without comma: "Dadu Camp Dadu" -> tries every space until both sides are real places
    bool splitTwoPlaces(const std::string& text, std::string& a, std::string& b) const {
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] != ' ') continue;
            std::string left = trim(text.substr(0, i)), right = trim(text.substr(i + 1));
            if (!left.empty() && !right.empty() && findPlace(left) >= 0 && findPlace(right) >= 0) {
                a = left;
                b = right;
                return true;
            }
        }
        return false;
    }

    // works with "A, B" and "A B"
    bool twoPlaces(const DynamicArray<std::string>& args, const std::string& rest, DynamicArray<std::string>& out) const {
        if (args.size() >= 2) {
            out = args;
            return true;
        }
        std::string a, b;
        if (!splitTwoPlaces(trim(rest), a, b)) return false;
        out.clear();
        out.pushBack(a);
        out.pushBack(b);
        return true;
    }

    static std::string lastWord(std::string& text) { // removes and returns the last word
        text = trim(text);
        std::size_t sp = text.find_last_of(' ');
        std::string w = sp == std::string::npos ? text : text.substr(sp + 1);
        text = sp == std::string::npos ? "" : trim(text.substr(0, sp));
        return w;
    }

    const std::string& name(int id) { return m_->graph().node(id).name; }

    // first line of a decision e.g. "● SERVED  R-01  Dadu Camp ..."
    void decisionHeader(const Decision& d) {
        Look lk = lookOf(d.status);
        std::cout << "  " << paint(lk.color, std::string(lk.mark) + " " + lk.word, true) << "  "
                  << paint(C::White, d.requestId, true);
        const Request* r = m_->findRequest(d.requestId);
        if (r) {
            std::string dot = std::string(" ") + paint(C::Grey, kDot) + " ";
            std::cout << "  " << name(r->locationId) << dot << supplyName(r->supply) << dot
                      << paint(urgencyColor(r->urgency), "urgency " + std::to_string(r->urgency)) << dot
                      << formatNumber(r->people) << " people" << (r->vulnerable ? dot + "vulnerable" : "");
        }
        std::cout << "\n";
    }

    // green box with amount, depot, time and path for each depot
    void decisionBox(const Decision& d) {
        DynamicArray<term::Line> lines;
        lines.pushBack(term::blank());
        const Request* r = m_->findRequest(d.requestId);
        std::string sup = r ? supplyName(r->supply) : "";
        for (std::size_t i = 0; i < d.legs.size(); ++i) {
            const DecisionLeg& l = d.legs[i];
            if (i > 0) lines.pushBack(term::blank());
            std::string amount = formatNumber(l.quantity) + " " + sup;
            std::string depot = name(l.depotNodeId);
            lines.pushBack(term::line(paint(C::White, amount, true) + "  from  " + paint(C::Yellow, depot, true),
                                      amount + "  from  " + depot));
            std::string how = formatMinutes(l.minutes) + "  " + kDot + "  via " + (l.via.empty() ? "local roads" : l.via);
            lines.pushBack(term::line(paint(C::Grey, how), how));
            std::string path;
            for (std::size_t k = 0; k < l.path.size(); ++k)
                path += (k ? std::string(" ") + kArrow + " " : "") + name(l.path[k]);
            DynamicArray<std::string> parts = term::wrap(path, 70);
            for (std::size_t k = 0; k < parts.size(); ++k) lines.pushBack(term::line(paint(C::Grey, parts[k]), parts[k]));
        }
        if (d.legs.isEmpty()) {
            std::string why = "Nothing could be sent this time";
            lines.pushBack(term::line(paint(C::Red, why, true), why));
            for (std::size_t i = 0; i < d.skipped.size() && i < 4; ++i) {
                std::string s = withoutDepotWord(name(d.skipped[i].depotNodeId)) + ": " + d.skipped[i].reason;
                lines.pushBack(term::line(paint(C::Grey, s), s));
            }
        }
        lines.pushBack(term::blank());
        term::box(lines);
    }

    // "next" -> serves one request
    void next() {
        Decision d = m_->processNext();
        if (!d.valid) {
            note(d.explanation);
            return;
        }
        decisionHeader(d);
        std::cout << "\n";
        decisionBox(d);
        int farther = 0;
        for (std::size_t i = 0; i < d.skipped.size(); ++i)
            if (d.skipped[i].reason.rfind("farther", 0) == 0) ++farther;
        const Request* r = m_->findRequest(d.requestId);
        if (r && r->remaining() > 0)
            std::cout << "  " << paint(C::Orange, formatNumber(r->remaining()) + " still needed:") << " "
                      << paint(C::Grey, "it is tried again when a road or stock changes.") << "\n";
        std::cout << "  " << paint(C::Grey, (farther ? std::to_string(farther) + " farther depots not needed.   " : std::string()) + "Type ")
                  << paint(C::Pink, "why") << paint(C::Grey, " for the full explanation.") << "\n";
    }

    // "all" -> serves every request in the heap, one line each
    void all() {
        int n = 0, served = 0, partial = 0, waiting = 0;
        while (m_->hasPending()) {
            Decision d = m_->processNext();
            const Request* r = m_->findRequest(d.requestId);
            Look lk = lookOf(d.status);
            std::string from = d.legs.isEmpty() ? "no reachable stock"
                                                : withoutDepotWord(name(d.legs[0].depotNodeId)) +
                                                      (d.legs.size() > 1 ? " +" + std::to_string(d.legs.size() - 1) : "");
            std::cout << "    " << paint(lk.color, lk.mark, true) << "  " << paint(C::White, term::pad(d.requestId, 6))
                      << term::pad(r ? term::fit(name(r->locationId), 19) : "", 20)
                      << paint(C::Grey, term::pad(r ? supplyName(r->supply) : "", 10))
                      << paint(C::White, term::padLeft(formatNumber(d.deliveredNow), 6)) << "   "
                      << paint(d.legs.isEmpty() ? C::Red : C::Grey, from) << "\n";
            ++n;
            if (d.status == Status::Fulfilled) ++served;
            else if (d.status == Status::PartiallyFulfilled) ++partial;
            else if (d.status == Status::Deferred) ++waiting;
        }
        if (n == 0) {
            note("Nothing to process: the queue is empty.");
            return;
        }
        std::cout << "\n  " << paint(C::Pink, "Processed " + std::to_string(n) + ":") << "  "
                  << paint(C::Green, std::to_string(served) + " served") << "   "
                  << paint(C::Orange, std::to_string(partial) + " partial") << "   "
                  << paint(C::Red, std::to_string(waiting) + " waiting for a road or stock") << "\n";
    }

    // "why" -> full explanation of last decision + the priority calculation
    void why() {
        const Decision& d = m_->lastDecision();
        if (!d.valid) {
            note("No decision yet. Type 'next' first.");
            return;
        }
        decisionHeader(d);
        std::cout << "\n";
        DynamicArray<std::string> text = term::wrap(d.explanation, 74);
        for (std::size_t i = 0; i < text.size(); ++i) std::cout << "  " << text[i] << "\n";

        if (!d.legs.isEmpty()) {
            std::cout << "\n";
            heading("Route:");
            for (std::size_t i = 0; i < d.legs.size(); ++i) {
                const DecisionLeg& l = d.legs[i];
                std::cout << "    " << paint(C::Yellow, withoutDepotWord(name(l.depotNodeId)), true) << "  "
                          << formatNumber(l.quantity) << "  " << paint(C::Grey, formatMinutes(l.minutes)) << "\n";
                std::string path;
                for (std::size_t k = 0; k < l.path.size(); ++k)
                    path += (k ? std::string(" ") + kArrow + " " : "") + name(l.path[k]);
                DynamicArray<std::string> parts = term::wrap(path, 70);
                for (std::size_t k = 0; k < parts.size(); ++k) std::cout << "      " << paint(C::Grey, parts[k]) << "\n";
            }
        }
        if (!d.skipped.isEmpty()) {
            std::cout << "\n";
            heading("Skipped depots:");
            for (std::size_t i = 0; i < d.skipped.size(); ++i) {
                bool farther = d.skipped[i].reason.rfind("farther", 0) == 0;
                std::cout << "    " << term::pad(withoutDepotWord(name(d.skipped[i].depotNodeId)), 14)
                          << paint(farther ? C::Grey : C::Orange, d.skipped[i].reason) << "\n";
            }
        }
        const Request* r = m_->findRequest(d.requestId);
        if (r) {
            std::cout << "\n";
            heading("Why it went first:");
            auto row = [](const std::string& what, double value, bool total) {
                std::ostringstream v;
                v << std::fixed << std::setprecision(0) << (value == 0 ? 0.0 : value);
                std::cout << "    " << paint(total ? C::White : C::Grey, term::pad(what, 40), total)
                          << paint(total ? C::Pink : C::White, term::padLeft(v.str(), 6), total) << "\n";
            };
            row("urgency " + std::to_string(r->urgency) + " x 100", r->urgency * 100.0, false);
            row("people " + formatNumber(r->people) + " / 100", r->people / 100.0, false);
            row(r->vulnerable ? "vulnerable: +50" : "not vulnerable: +0", r->vulnerable ? 50.0 : 0.0, false);
            row("= base score", ReliefManager::baseScore(*r), true);
            row("arrived at minute " + std::to_string(r->arrivalTime) + ": minus 2 x " + std::to_string(r->arrivalTime),
                -ReliefManager::kAgingPerMinute * r->arrivalTime, false);
            row("= priority key (what the heap sorts)", ReliefManager::heapKey(*r), true);
            std::cout << "\n";
            note("Aging: a request that arrived earlier loses less, so the longer it");
            note("waits, the higher it ranks against newer ones (no starvation).");
        }
    }

    // block / reopen / condition. kind: 'b' = block, 'r' = reopen, 'c' = condition
    void roadAction(const DynamicArray<std::string>& args, const std::string& rest, char kind) {
        const char* usage = kind == 'b' ? "block <place>, <place>      e.g. block Jamshoro, Sehwan"
                          : kind == 'r' ? "reopen <place>, <place>     e.g. reopen Abbottabad, Gilgit"
                                        : "condition <place>, <place>, <clear|damaged|flooded>   e.g. condition Sehwan, Dadu, clear";
        DynamicArray<std::string> a;
        bool ok2;
        if (kind == 'c' && args.size() < 3) { // no commas: last word is the condition
            std::string text = rest;
            std::string cond = lastWord(text);
            DynamicArray<std::string> none;
            ok2 = twoPlaces(none, text, a);
            if (ok2) a.pushBack(cond);
        } else {
            ok2 = twoPlaces(args, rest, a);
        }
        if (!ok2 || (kind == 'c' && a.size() != 3)) {
            error("expected two places.");
            note("Format: " + std::string(usage));
            return;
        }
        int u = place(a[0]), v = place(a[1]);
        bool ok = false;
        RoadCondition c = RoadCondition::Clear;
        if (kind == 'b') ok = m_->blockRoad(u, v);
        else if (kind == 'r') ok = m_->reopenRoad(u, v);
        else {
            if (a[2] == "clear") c = RoadCondition::Clear;
            else if (a[2] == "damaged") c = RoadCondition::Damaged;
            else if (a[2] == "flooded") c = RoadCondition::Flooded;
            else throw std::invalid_argument("condition must be clear, damaged or flooded");
            ok = m_->setRoadCondition(u, v, c);
        }
        if (!ok) {
            error("there is no direct road between " + name(u) + " and " + name(v) + ".");
            note("Tip: 'route " + name(u) + ", " + name(v) + "' shows the roads that connect them.");
            return;
        }
        const Edge* e = m_->graph().edgeBetween(u, v);
        std::string road = (e && !e->roadName.empty() ? e->roadName + "  " : std::string()) + name(u) + " - " + name(v);
        if (kind == 'b')
            std::cout << "  " << paint(C::Red, std::string(kCross) + " CLOSED", true) << "    " << road << "   "
                      << paint(C::Grey, "(both directions)") << "\n";
        else if (kind == 'r')
            std::cout << "  " << paint(C::Green, std::string(kServed) + " REOPENED", true) << "  " << road << "\n";
        else
            std::cout << "  " << paint(c == RoadCondition::Flooded ? C::Blue : (c == RoadCondition::Damaged ? C::Orange : C::Green),
                                       std::string("~ ") + term::upper_copy(conditionName(c)), true)
                      << "  " << road << paint(C::Grey, "   now " + formatMinutes(e->weight())) << "\n";

        const DynamicArray<Event>& ev = m_->events();
        if (!ev.isEmpty() && ev.back().type == EventType::Retry)
            std::cout << "  " << paint(C::Yellow, "> " + ev.back().text) << "\n  " << paint(C::Grey, "Type ")
                      << paint(C::Pink, "next") << paint(C::Grey, " to serve them, or ") << paint(C::Pink, "queue")
                      << paint(C::Grey, " to see the heap.") << "\n";
    }

    // "stock <depot>, <supply>, <n>"
    void addStock(const DynamicArray<std::string>& args, const std::string& rest) {
        DynamicArray<std::string> a = args;
        if (a.size() == 1) {
            std::string text = rest;
            std::string qty = lastWord(text), sup = lastWord(text);
            a.clear();
            a.pushBack(text);
            a.pushBack(sup);
            a.pushBack(qty);
        }
        if (a.size() != 3 || a[0].empty()) {
            error("expected a depot, a supply and a quantity.");
            note("Format: stock <depot>, <supply>, <quantity>   e.g. stock Sukkur Depot, water, 500");
            return;
        }
        Supply s;
        if (!parseSupply(a[1], s)) throw std::invalid_argument("unknown supply '" + a[1] + "'");
        m_->addStock(place(a[0]), s, std::stoi(a[2]));
        std::cout << "  " << paint(C::Green, std::string(kServed) + " STOCK", true) << "  " << m_->events().back().text << "\n";
    }

    // "add" -> new request (needs 7 values)
    void addRequest(const DynamicArray<std::string>& a) {
        if (a.size() != 7) {
            note("Usage: add <id>, <place>, <supply>, <qty>, <urgency 1-5>, <people>, <yes|no>");
            note("Example: add R-29, Moro, water, 500, 4, 3000, yes");
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
        std::cout << "  " << paint(C::Green, std::string(kServed) + " ADDED", true) << "  " << paint(C::White, r.id, true) << "  "
                  << name(r.locationId) << "  " << formatNumber(r.quantity) << " " << supplyName(r.supply) << ", "
                  << paint(urgencyColor(r.urgency), "urgency " + std::to_string(r.urgency)) << "\n";
    }

    // "time <minutes>" -> moves the clock
    void advance(const DynamicArray<std::string>& a) {
        if (a.size() != 1) {
            note("Usage: time <minutes>");
            return;
        }
        m_->advanceTime(std::stoi(a[0]));
        std::cout << "  " << paint(C::Pink, "Clock:") << " " << formatClock(m_->now()) << "\n";
    }

    // "list" -> all requests, open ones by priority, served ones at the end
    void listRequests() {
        const DynamicArray<Request>& rs = m_->requests();
        if (rs.isEmpty()) {
            note("No requests.");
            return;
        }
        DynamicArray<int> order; // insertion sort on indexes
        auto before = [&](int x, int y) {
            bool dx = rs[x].status == Status::Fulfilled, dy = rs[y].status == Status::Fulfilled;
            if (dx != dy) return !dx;
            return ReliefManager::heapKey(rs[x]) > ReliefManager::heapKey(rs[y]);
        };
        for (std::size_t i = 0; i < rs.size(); ++i) {
            int v = static_cast<int>(i);
            std::size_t j = order.size();
            order.pushBack(v);
            while (j > 0 && before(v, order[j - 1])) {
                order[j] = order[j - 1];
                --j;
            }
            order[j] = v;
        }
        std::cout << "    " << paint(C::White, term::pad("", 3) + term::pad("ID", 6) + term::pad("Location", 20) +
                                               term::pad("Supply", 10) + term::padLeft("Left", 6) + term::padLeft("Urg", 5) +
                                               term::padLeft("Key", 6) + "   Status", true)
                  << "\n";
        for (std::size_t k = 0; k < order.size(); ++k) {
            const Request& r = rs[static_cast<std::size_t>(order[k])];
            Look lk = lookOf(r.status);
            bool done = r.status == Status::Fulfilled;
            std::ostringstream key;
            key << std::fixed << std::setprecision(0) << ReliefManager::heapKey(r);
            C main = done ? C::Grey : C::White;
            std::cout << "    " << paint(lk.color, term::pad(lk.mark, 3), true) << paint(main, term::pad(r.id, 6))
                      << paint(main, term::pad(term::fit(name(r.locationId), 19), 20))
                      << paint(C::Grey, term::pad(supplyName(r.supply), 10))
                      << paint(main, term::padLeft(formatNumber(r.remaining()), 6))
                      << paint(done ? C::Grey : urgencyColor(r.urgency), term::padLeft(std::to_string(r.urgency), 5))
                      << paint(C::Grey, term::padLeft(key.str(), 6)) << "   " << paint(lk.color, lk.word)
                      << (r.vulnerable ? paint(C::Grey, " *") : "") << "\n";
        }
        std::cout << "\n  " << paint(C::Grey, "Left = units still needed   Key = heap key (higher goes first)   * vulnerable") << "\n";
    }

    // "queue" -> prints the heap array level by level and checks parent >= children
    void showQueue() {
        const DynamicArray<Request>& rs = m_->requests();
        DynamicArray<int> heap = m_->queueSnapshot();
        auto key = [&](std::size_t slot) { return ReliefManager::heapKey(rs[static_cast<std::size_t>(heap[slot])]); };
        auto fmt = [](double k) {
            std::ostringstream o;
            o << std::fixed << std::setprecision(0) << k;
            return o.str();
        };

        heading("Priority queue: binary heap in an array, highest key on top");
        if (heap.isEmpty()) {
            note("  (empty)");
        } else {
            std::size_t start = 0, levelSize = 1;
            for (int level = 0; start < heap.size(); ++level) {
                std::cout << "    " << paint(C::Grey, term::pad("level " + std::to_string(level), 9));
                std::size_t col = 0;
                for (std::size_t i = start; i < start + levelSize && i < heap.size(); ++i, ++col) {
                    if (col > 0 && col % 4 == 0) std::cout << "\n             ";
                    std::string slot = "[" + std::to_string(i) + "]";
                    std::cout << paint(C::Grey, term::padLeft(slot, 4)) << " " << paint(i == 0 ? C::Yellow : C::White, rs[static_cast<std::size_t>(heap[i])].id, i == 0)
                              << " " << paint(C::Pink, term::pad(fmt(key(i)), 5)) << " ";
                }
                std::cout << "\n";
                start += levelSize;
                levelSize *= 2;
            }
            std::size_t pairs = 0, bad = 0;
            for (std::size_t i = 1; i < heap.size(); ++i, ++pairs)
                if (key((i - 1) / 2) < key(i)) ++bad;
            std::cout << "\n    " << paint(C::White, "Next to serve: ") << paint(C::Yellow, rs[static_cast<std::size_t>(heap[0])].id, true)
                      << paint(C::Grey, " (slot [0], key " + fmt(key(0)) + ")") << "\n";
            if (bad == 0)
                std::cout << "    " << paint(C::Green, std::string(kServed) + " Heap order OK") << paint(C::Grey, ": every parent >= its children (" + std::to_string(pairs) + " pairs checked)") << "\n";
            else
                std::cout << "    " << paint(C::Red, std::string(kCross) + " Heap order broken in " + std::to_string(bad) + " of " + std::to_string(pairs) + " pairs") << "\n";
        }

        const DynamicArray<int>& waiting = m_->deferredList();
        std::cout << "\n";
        heading("Deferred list: waiting for a road or stock (not in the heap)");
        if (waiting.isEmpty()) note("  (empty)");
        for (std::size_t i = 0; i < waiting.size(); ++i) {
            const Request& r = rs[static_cast<std::size_t>(waiting[i])];
            std::cout << "    " << paint(lookOf(r.status).color, term::pad(r.id, 6), true) << term::pad(name(r.locationId), 20)
                      << paint(C::Grey, term::pad(supplyName(r.supply), 10) + formatNumber(r.remaining()) + " still needed") << "\n";
        }
        std::cout << "\n";
        note("Retried automatically after block, reopen, condition or stock changes.");
    }

    // "depots" -> stock of every depot
    void listDepots() {
        const DynamicArray<Depot>& ds = m_->depots();
        std::cout << "    " << paint(C::White, term::pad("Depot", 14), true);
        for (int s = 0; s < kSupplyCount; ++s)
            std::cout << paint(C::White, term::padLeft(supplyName(static_cast<Supply>(s)), 12), true);
        std::cout << "\n";
        for (std::size_t i = 0; i < ds.size(); ++i) {
            std::cout << "    " << paint(C::Yellow, term::pad(withoutDepotWord(name(ds[i].nodeId)), 14));
            for (int s = 0; s < kSupplyCount; ++s) {
                int v = ds[i].stock[s];
                std::cout << paint(v > 0 ? C::White : C::Red, term::padLeft(formatNumber(v), 12));
            }
            std::cout << "\n";
        }
    }

    // "route A, B" -> runs dijkstra and prints the path road by road
    void route(const DynamicArray<std::string>& args, const std::string& rest) {
        DynamicArray<std::string> a;
        if (!twoPlaces(args, rest, a) || a.size() != 2) {
            error("expected two places.");
            note("Format: route <from>, <to>   e.g. route Karachi, Gilgit");
            return;
        }
        int u = place(a[0]), v = place(a[1]);
        Dijkstra d(m_->graph());
        d.start(u);
        d.run();
        if (!d.reachable(v)) {
            error("no open route from " + a[0] + " to " + a[1] + ".");
            return;
        }
        DynamicArray<int> p = d.pathTo(v);
        std::cout << "  " << paint(C::Yellow, name(u) + " " + kArrow + " " + name(v), true) << "   "
                  << paint(C::White, formatMinutes(d.distanceTo(v)), true) << "   "
                  << paint(C::Grey, std::to_string(d.visitedCount()) + " places explored") << "\n\n";
        for (std::size_t i = 0; i + 1 < p.size(); ++i) {
            const Edge* e = m_->graph().edgeBetween(p[i], p[i + 1]);
            std::string leg = term::fit(name(p[i]) + " " + kArrow + " " + name(p[i + 1]), 34);
            std::cout << "    " << term::pad(leg, 36) << paint(C::Grey, term::pad(e->roadName.empty() ? "local road" : e->roadName, 18))
                      << term::padLeft(formatMinutes(e->weight()), 11);
            if (e->condition == RoadCondition::Flooded) std::cout << paint(C::Blue, "  flooded");
            if (e->condition == RoadCondition::Damaged) std::cout << paint(C::Orange, "  damaged");
            std::cout << "\n";
        }
    }

    // "stats" -> totals and unmet demand
    void stats() {
        Stats s = m_->stats();
        std::cout << "  " << paint(C::Pink, "Requests:") << "  " << s.total << "   " << paint(C::Green, std::to_string(s.fulfilled) + " served")
                  << "   " << paint(C::Orange, std::to_string(s.partial) + " partial") << "   "
                  << paint(C::Red, std::to_string(s.deferred) + " waiting") << "   " << paint(C::Grey, std::to_string(s.pending) + " queued")
                  << "\n\n";
        heading("Still needed:");
        for (int i = 0; i < kSupplyCount; ++i)
            std::cout << "    " << term::pad(supplyName(static_cast<Supply>(i)), 12)
                      << paint(s.unmet[i] > 0 ? C::Red : C::Green, term::padLeft(formatNumber(s.unmet[i]), 8)) << "\n";
        std::cout << "\n    " << term::pad("Total route time", 34) << formatMinutes(s.totalRouteMinutes) << "\n"
                  << "    " << term::pad("Average wait before processing", 34) << formatMinutes(s.averageWaitMinutes) << "\n"
                  << "    " << term::pad("Clock", 34) << paint(C::White, formatClock(m_->now()), true) << "\n";
    }

    // "log [n]" -> last n events, newest first
    void showLog(const DynamicArray<std::string>& a) {
        const DynamicArray<Event>& ev = m_->events();
        std::size_t n = a.isEmpty() ? 10 : static_cast<std::size_t>(std::stoi(a[0]));
        std::size_t start = ev.size() > n ? ev.size() - n : 0;
        for (std::size_t i = ev.size(); i > start; --i) {
            const Event& e = ev[i - 1];
            std::string clock = formatClock(e.time);
            DynamicArray<std::string> lines = term::wrap(e.text, 50);
            std::cout << "    " << paint(C::Grey, term::pad(clock.size() > 6 ? clock.substr(clock.size() - 5) : clock, 7))
                      << paint(eventColor(e.type), term::pad(eventTypeName(e.type), 11), true);
            for (std::size_t k = 0; k < lines.size(); ++k) std::cout << (k ? std::string(22, ' ') : "") << lines[k] << "\n";
        }
    }

    // "bench" -> times dijkstra on a random graph (2000 nodes, 6000 roads).
    // first a tree so the graph is connected, then random extra roads
    void bench() {
        const int kNodes = 2000, kRoads = 6000, kRuns = 100;
        Graph g;
        unsigned int seed = 12345; // fixed seed -> same graph every time
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
        std::ostringstream t;
        t << std::fixed << std::setprecision(3) << ms << " ms";
        std::cout << "  " << paint(C::Pink, "Dijkstra benchmark") << "\n"
                  << "    " << term::pad("Graph", 18) << kNodes << " nodes, " << g.roadCount() << " roads (random, connected)\n"
                  << "    " << term::pad("Runs", 18) << kRuns << ", " << visited / kRuns << " nodes settled per run\n"
                  << "    " << term::pad("Time per run", 18) << paint(C::Yellow, t.str(), true) << "\n";
    }
};

}

// program starts here
int main(int argc, char** argv) {
    Cli cli;
    cli.run(argc, argv);
    return 0;
}
