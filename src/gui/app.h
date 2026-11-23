// this file declares App -> the whole GUI window.
// immediate mode: every frame we read the mouse/keys and draw everything again
// from the ReliefManager, so the screen always shows the latest data.
// app.cpp = window, map, sidebar, top bar, search. panels.cpp = right panel pages
#pragma once // this prevents multiple inclusions of the header file.

#include <memory>
#include <string>

#include "raylib.h"

#include "gui/map_view.h"
#include "gui/text.h"
#include "relief/relief_manager.h"
#include "relief/scenario_loader.h"

// rectangles of each part of the screen
struct Layout {
    Rectangle sidebar, topBar, header, map, kpis, rightPanel;
};

// what a click on the map does
enum class Tool { Select, BlockRoad, AddRequest };
// which page the right panel shows
enum class Page { Overview, Queue, Depots, Why, Events };

// values of the add request form
struct AddForm {
    int locationId = -1;
    int supply = 1; // 0 food, 1 water, 2 medicine, 3 shelter
    int quantity = 300;
    int urgency = 3;
    int people = 2000;
    bool vulnerable = false;
    std::string error;
};

class App {
public:
    int run(); // opens the window and runs until it is closed

private:
    static Layout computeLayout(float w, float h); // window size -> rectangles
    void loadScenario(); // also used by the Reset button
    void frame(); // one frame: input, update, draw

    // ---- in app.cpp ----
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
    Rectangle searchRect() const;
    void updateSearch();
    void drawSearch();
    void findMatches(DynamicArray<int>& out) const;
    void selectAndShow(int nodeId);

    // ---- in panels.cpp ----
    void drawRightPanel(Rectangle r);
    void drawOverviewPage(Rectangle r);
    void drawQueuePage(Rectangle r);
    void drawDepotsPage(Rectangle r);
    void drawWhyPage(Rectangle r);
    void drawEventsPage(Rectangle r);
    void drawAddForm(Rectangle r);
    std::string nextRequestId() const;

    std::unique_ptr<ReliefManager> manager_; // unique_ptr so Reset can make a new one
    LoadResult load_;
    std::string scenarioPath_ = "data/pakistan.txt";
    Fonts fonts_;
    MapView map_;
    Layout layout_{};

    Tool tool_ = Tool::Select; // current tool
    Page page_ = Page::Overview; // current page
    AddForm form_;

    int hoverNode_ = -1, hoverU_ = -1, hoverV_ = -1; // under the mouse (-1 = nothing)
    int selNode_ = -1, selU_ = -1, selV_ = -1; // clicked / selected
    int panelHoverNode_ = -1; // row under the mouse in the panel (highlighted on the map)
    int lastPanelHover_ = -1;

    bool searchFocus_ = false; // true while typing in search
    std::string searchText_;

    double pulseStart_ = -100.0; // time when the last place was served (for the pulse)
    int pulseNode_ = -1;
    float listScroll_ = 0, depotScroll_ = 0, whyScroll_ = 0, logScroll_ = 0; // scroll of each page
    int initialMax_[kSupplyCount] = {1, 1, 1, 1}; // biggest stock at start (for the bar length)
    std::string tooltip_;
    Vector2 tooltipPos_{0, 0};
};
