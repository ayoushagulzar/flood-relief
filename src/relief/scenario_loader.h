// this file declares ScenarioLoader -> reads the scenario text file (data/pakistan.txt).
// file has 4 sections: [NODES] [ROADS] [STOCK] [REQUESTS], fields are separated by commas
// and '#' starts a comment.
// on an error it stops and gives "file:line: message"

#pragma once // this prevents multiple inclusions of the header file.

#include <string>

#include "relief/relief_manager.h"

// result of loading
struct LoadResult {
    bool ok = false;
    std::string error; // "file:line: message" if ok is false
    int nodes = 0, roads = 0, stockLines = 0, requests = 0;
};

class ScenarioLoader {
public:
    // static so we can call it without making an object
    static LoadResult loadFile(const std::string& path, ReliefManager& m);

    // same but from a string (used in tests)
    static LoadResult loadText(const std::string& text, const std::string& name, ReliefManager& m);
};
