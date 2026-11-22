// app.h - the GUI application: window, layout, game loop, tools and panels.
//
// Immediate mode: every frame we read input, update, and redraw EVERYTHING
// from the manager's current state. No widget objects to keep in sync:
// ReliefManager is the only source of truth.
//
// Code is split over two files:
//   app.cpp    - window, layout, map interaction, tools, rail, top bar
//   panels.cpp - right panel (Requests | Depots | Why | Add form) and event log
#pragma once

#include <memory>
#include <string>

#include "raylib.h"

#include "gui/map_view.h"
#include "gui/text.h"
#include "relief/relief_manager.h"
#include "relief/scenario_loader.h"

struct Layout {
    Rectangle topBar, rail, map, rightPanel, eventLog;
};

// The current tool is a tiny state machine. It lives HERE (in App), the
// map only reports "clicked at this point"; App decides what that means.
enum class Tool { Select, BlockRoad, AddRequest };
enum class Tab { Requests, Depots, Why };

// What the Add-request form is editing (only used while Tool::AddRequest).
struct AddForm {
    int locationId = -1;
    int supply = 1;  // index into Supply (0 food, 1 water, 2 medicine, 3 shelter)
    int quantity = 300;
    int urgency = 3;
    int people = 2000;
    bool vulnerable = false;
    std::string error;
};

class App {
public:
    int run();  // opens the window, runs until closed, returns the exit code

private:
    static Layout computeLayout(float w, float h);
    void loadScenario();
    void frame();

    // ---- app.cpp ----
    void updateMap(bool mouseOverUI);
    void handleMapClick();
    void processNext();
    void processAll();
    void setTool(Tool t);
    void buildOverlay(MapOverlay& o) const;
    void drawTopBar(Rectangle r);
    void drawRail(Rectangle r);
    void drawInfoCard();
    Rectangle infoCardRect() const;

    // ---- panels.cpp ----
    void drawRightPanel(Rectangle r);
    void drawRequestsTab(Rectangle r);
    void drawDepotsTab(Rectangle r);
    void drawWhyTab(Rectangle r);
    void drawAddForm(Rectangle r);
    void drawEventLog(Rectangle r);
    std::string nextRequestId() const;

    std::unique_ptr<ReliefManager> manager_;  // unique_ptr: "Reset" builds a fresh one
    LoadResult load_;
    std::string scenarioPath_ = "data/pakistan.txt";
    Fonts fonts_;
    MapView map_;
    Layout layout_{};

    Tool tool_ = Tool::Select;
    Tab tab_ = Tab::Requests;
    AddForm form_;

    int hoverNode_ = -1, hoverU_ = -1, hoverV_ = -1;
    int selNode_ = -1, selU_ = -1, selV_ = -1;
    int panelHoverNode_ = -1;  // request row under the mouse -> highlight on map

    double pulseStart_ = -100.0;  // GetTime() when the last request was served
    int pulseNode_ = -1;
    float listScroll_ = 0, depotScroll_ = 0, whyScroll_ = 0, logScroll_ = 0;
    int initialMax_[kSupplyCount] = {1, 1, 1, 1};  // largest stock at load time (bar scale)
    std::string tooltip_;
    Vector2 tooltipPos_{0, 0};
};
