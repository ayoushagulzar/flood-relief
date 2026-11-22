// scenario_loader.h - reads a scenario text file into a ReliefManager.
//
// File format (comma-separated fields, spaces around commas are ignored):
//   # comment            blank lines are ignored too
//   [NODES]     name, type (city|shelter|depot), lat, lon
//   [ROADS]     from, to, minutes, type (motorway|highway|road), name [, condition]
//               condition (optional): clear | damaged | flooded | blocked
//               name "-" means an unnamed local road
//   [STOCK]     depot, supply (food|water|medicine|shelter), quantity
//   [REQUESTS]  id, location, supply, quantity, urgency 1-5, people, vulnerable (yes|no)
//
// Errors stop loading and say exactly where and what:
//   pakistan.txt:42: unknown place 'Sukur' in [ROADS]
#pragma once

#include <string>

#include "relief/relief_manager.h"

struct LoadResult {
    bool ok = false;
    std::string error;  // "file:line: message" when ok == false
    int nodes = 0, roads = 0, stockLines = 0, requests = 0;
};

class ScenarioLoader {
public:
    // Loads from a file on disk. The file name is used in error messages.
    static LoadResult loadFile(const std::string& path, ReliefManager& m);

    // Loads from text in memory (used by the tests). `name` appears in errors.
    static LoadResult loadText(const std::string& text, const std::string& name, ReliefManager& m);
};
