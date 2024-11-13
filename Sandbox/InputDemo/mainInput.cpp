//
// Created by mathe on 12/11/2024.
//

#include <cstddef>
#include <iostream>
#include <memory>
#include <raylib.h>
#include "core/inputs/input.h"
#include "core/inputs/mouse.h"
#include "core/window/window.h"

int main()
{
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    hive::Input::init({GetWindowHandle(),hive::WindowNativeData::RAYLIB});

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

    int lastXClic = 0;
    int lastYClic = 0;

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {

        bool Apressed = hive::Input::getKey(hive::KeyCode::KEY_A);

        BeginDrawing();

        ClearBackground(RAYWHITE);
        if (Apressed)
            DrawText("A is pressed", 190, 200, 20, LIME);
        else
            DrawText("A is not pressed", 190, 200, 20, LIGHTGRAY);

        Vector2 mousePosition = GetMousePosition();
        DrawText(TextFormat("Mouse position x:%i y:%i", (int)mousePosition.x, (int)mousePosition.y), 190, 240, 20, LIGHTGRAY);

        if(hive::Input::getMouseButtonPressed((int)hive::ButtonValue::BUTTON_1))
        {
            lastXClic = (int)hive::Input::getMouseX();
            lastYClic = (int)hive::Input::getMouseY();
        }

        DrawText(TextFormat("Last clic position x:%i y:%i", lastXClic, lastYClic), 190, 220, 20, LIGHTGRAY);

        if(hive::Input::getMouseButtonDown((int)hive::ButtonValue::BUTTON_1))
        {
            DrawCircle(hive::Input::getMouseX(), hive::Input::getMouseY(), 10, RED);
        }



        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();

    return 0;
}