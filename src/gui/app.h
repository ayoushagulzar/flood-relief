// app.h - the GUI application: window, layout, game loop, tools and panels.
//
// Immediate mode: every frame we read input, update, and redraw EVERYTHING
// from the manager's current state. No widget objects to keep in sync:
// ReliefManager is the only source of truth.
//
// Screen:
//   +---------+---------------------------------------+-------------+
//   |         | search | Next | Serve all | tools      |             |
//   | sidebar +---------------------------------------+ right panel |
//   | (pages) | scenario title             tool hint  | (the page   |
//   |         | [            map card             ]   |  chosen in  |
//   |         | Today's highlights: 4 KPI cards       |  sidebar)   |
//   +---------+---------------------------------------+-------------+
//
// Code is split over two files:
//   app.cpp    - window, layout, map, tools, sidebar, top bar, search, KPIs
//   panels.cpp - right panel pages (Overview, Queue, Depots, Why, Events)
//                and the Add-request form
#pragma once

#include <memory>
#include <string>

#include "raylib.h"

#include "gui/map_view.h"
#include "gui/text.h"
#include "relief/relief_manager.h"
#include "relief/scenario_loader.h"

struct Layout {
    Rectangle sidebar, topBar, header, map, kpis, rightPanel;
};

// The current tool is a tiny state machine. It lives HERE (in App), the
// map only reports "clicked at this point"; App decides what that means.
enum class Tool { Select, BlockRoad, AddRequest };
// What the right panel shows (picked in the sidebar).
enum class Page { Overview, Queue, Depots, Why, Events };

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
    void drawSidebar(Rectangle r);
    void drawTopBar(Rectangle r);
    void drawHeader(Rectangle r);
    void drawMapCard();
    void drawKpis(Rectangle r);
    void drawInfoCard();
    Rectangle infoCardRect() const;
    // search box
    Rectangle searchRect() const;
    void updateSearch();
    void drawSearch();
    void findMatches(DynamicArray<int>& out) const;
    void selectAndShow(int nodeId);

    // ---- panels.cpp ----
    void drawRightPanel(Rectangle r);
    void drawOverviewPage(Rectangle r);
    void drawQueuePage(Rectangle r);
    void drawDepotsPage(Rectangle r);
    void drawWhyPage(Rectangle r);
    void drawEventsPage(Rectangle r);
    void drawAddForm(Rectangle r);
    std::string nextRequestId() const;

    std::unique_ptr<ReliefManager> manager_;  // unique_ptr: "Reset" builds a fresh one
    LoadResult load_;
    std::string scenarioPath_ = "data/pakistan.txt";
    Fonts fonts_;
    MapView map_;
    Layout layout_{};

    Tool tool_ = Tool::Select;
    Page page_ = Page::Overview;
    AddForm form_;

    int hoverNode_ = -1, hoverU_ = -1, hoverV_ = -1;
    int selNode_ = -1, selU_ = -1, selV_ = -1;
    int panelHoverNode_ = -1;  // row under the mouse in a panel -> highlight on map
    int lastPanelHover_ = -1;  // ... as it was last frame (the map is drawn first)

    bool searchFocus_ = false;
    std::string searchText_;

    double pulseStart_ = -100.0;  // GetTime() when the last request was served
    int pulseNode_ = -1;
    float listScroll_ = 0, depotScroll_ = 0, whyScroll_ = 0, logScroll_ = 0;
    int initialMax_[kSupplyCount] = {1, 1, 1, 1};  // largest stock at load time (bar scale)
    std::string tooltip_;
    Vector2 tooltipPos_{0, 0};
};
