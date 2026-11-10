// raylib GUI for Flood Relief.
// Rule: GUI code draws and handles input, then calls ReliefManager.
// No relief logic lives here.

#include "raylib.h"

int main() {
    const int screenWidth  = 1280;
    const int screenHeight = 720;

    InitWindow(screenWidth, screenHeight, "Flood Relief");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{14, 22, 32, 255});

        DrawLineEx({400, 300}, {700, 450}, 3.0f, GRAY);   // a road
        DrawCircle(400, 300, 12, RED);                     // a city
        DrawText("Sukkur", 418, 290, 20, RAYWHITE);
        DrawText("raylib is working", 20, 20, 20, SKYBLUE);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
