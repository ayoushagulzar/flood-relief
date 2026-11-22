// raylib GUI for Flood Relief.
// Rule: GUI code draws and handles input, then calls ReliefManager.
// No relief logic lives here. Everything is in src/gui/app.cpp.

#include "gui/app.h"

int main() {
    App app;
    return app.run();
}
