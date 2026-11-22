// app.h - the GUI application: window, layout, game loop.
//
// Immediate mode: every frame we read input, update, and redraw EVERYTHING
// from the manager's current state. No widget objects to keep in sync:
// ReliefManager is the only source of truth.
#pragma once

#include <string>

#include "raylib.h"

#include "gui/map_view.h"
#include "gui/text.h"
#include "relief/relief_manager.h"
#include "relief/scenario_loader.h"

struct Layout {
    Rectangle topBar, rail, map, rightPanel, eventLog;
};

class App {
public:
    int run();  // opens the window, runs until closed, returns the exit code

private:
    static Layout computeLayout(float w, float h);
    void frame();
    void drawTopBar(Rectangle r);
    void drawRail(Rectangle r);
    void drawRightPanel(Rectangle r);
    void drawEventLog(Rectangle r);

    ReliefManager manager_;
    LoadResult load_;
    std::string scenarioPath_ = "data/pakistan.txt";
    Fonts fonts_;
    MapView map_;
    Layout layout_{};
};
