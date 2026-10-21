//
// Created by asimy on 6/25/2026.
//

#include "GameController.h"

#include "BaseMinesweeper.h"

#include "NoGuessMinesweeper.h"

#include "ClassicMinesweeper.h"

#include "raylib.h"

#include <algorithm>


void GameController::run() {
        selectedMode = GameMode::CLASSIC;
        engine = std::make_unique<ClassicMinesweeper>();
        engine->loadBoard(BEGINNER.width, BEGINNER.height, BEGINNER.mineCount);
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(1280, 720, "Minesweeper Classic");
        SetTargetFPS(120);

        camera.target = {0.0f, 0.0f};
        camera.offset = {0.0f, 0.0f};
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;

        appState = AppState::MAIN_MENU;

        while (!WindowShouldClose()) {
                handleInput(); // Correctly routes logic based on menu vs playing
                update(); // Updates logic streams
                render(); // Correctly routes clear screens and button overlays
        }
        CloseWindow();
}


void GameController::handleInput() {
        switch (appState) {
        case AppState::MAIN_MENU:
                handleMenuInput();
                break;
        case AppState::PLAYING:
                handleGameInput();
                break;
        case AppState::OPTIONS:
                break;
        }
}


void GameController::render() const {
        switch (appState) {
        case AppState::MAIN_MENU:
                renderMainMenu();
                break;
        case AppState::PLAYING:
                renderGame();
                break;
        case AppState::OPTIONS:
                break;
        }
}

//Main Menu UI and Logic
void GameController::renderMainMenu() const {
        BeginDrawing();
        ClearBackground(DARKGRAY);

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        int titleWidth = MeasureText("MINESWEEPER", 60);
        DrawText("MINESWEEPER", (screenWidth - titleWidth) / 2, screenHeight / 4 - 60, 60, RAYWHITE);

        const char* modeText = selectedMode == GameMode::CLASSIC
                                       ? "Mode: Classic (Press M to toggle)"
                                       : "Mode: NoGuess (Press M to toggle)";
        int modeWidth = MeasureText(modeText, 20);
        DrawText(modeText, (screenWidth - modeWidth) / 2, screenHeight / 4 + 20, 20, LIGHTGRAY);

        int buttonWidth = 300;
        int buttonHeight = 50;
        int startX = (screenWidth - buttonWidth) / 2; // Perfectly center buttons horizontally
        int startY = screenHeight / 2 - 50;
        const char* labels[] = {"Beginner (9x9)", "Intermediate (16x16)", "Expert (30x16)", "Evil (30x20)"};

        for (int i = 0; i < 4; ++i) {
                Rectangle buttonReact = {
                        (float)startX, (float)startY + i * 70, (float)buttonWidth, (float)buttonHeight
                };
                if (CheckCollisionPointRec(GetMousePosition(), buttonReact)) {
                        DrawRectangleRec(buttonReact, LIGHTGRAY);
                        DrawText(labels[i], startX + 20, startY + 15 + i * 70, 20, BLACK);
                }
                else {
                        DrawRectangleRec(buttonReact, GRAY);
                        DrawText(labels[i], startX + 20, startY + 15 + i * 70, 20, RAYWHITE);
                }
        }
        EndDrawing();
}

void GameController::handleMenuInput() {
        if (IsKeyPressed(KEY_M)) {
                selectedMode = selectedMode == GameMode::CLASSIC ? GameMode::NO_GUESS : GameMode::CLASSIC;
        }
        if (IsKeyPressed(KEY_F11)) {
                ToggleFullscreen();
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                int startX = (GetScreenWidth() - 300) / 2;
                int startY = GetScreenHeight() / 2 - 50;

                if (CheckCollisionPointRec(mouse, {(float)startX, (float)startY, 300, 50})) {
                        reset(BEGINNER);
                }
                else if (CheckCollisionPointRec(mouse, {(float)startX, (float)startY + 70, 300, 50})) {
                        reset(INTERMEDIATE);
                }
                else if (CheckCollisionPointRec(mouse, {(float)startX, (float)startY + 140, 300, 50})) {
                        reset(EXPERT);
                }
                else if (CheckCollisionPointRec(mouse, {(float)startX, (float)startY + 210, 300, 50})) {
                        reset(EVIL);
                }
        }
}


void GameController::renderGame() const {
        BeginDrawing();
        ClearBackground(DARKGRAY);

        DrawRectangle(0, 0, GetScreenWidth(), 60, GetColor(0x333333FF));
        DrawText(TextFormat("MINES: %03d", engine->total_mines() - engine->total_flags()), 20, 20, 20, RED);
        DrawText(TextFormat("TIME: %03d", timeElapsed), GetScreenWidth() - 120, 20, 20, RED);


        if (currentState == GameState::WON) {
                DrawText("YOU WIN! (Press R to restart / ESC for Menu)", GetScreenWidth() / 2 - 200, 20, 20, GREEN);
        }
        if (currentState == GameState::LOST) {
                DrawText("GAME OVER (Press R to restart / ESC for Menu)", GetScreenWidth() / 2 - 220, 20, 20, RED);
        }


        BeginMode2D(camera);
        for (int idx = 0; idx < engine->getGrid().size(); ++idx) {
                const Cell& cell = engine->getGrid()[idx];
                int x = (idx % engine->getWidth()) * tileSize;
                int y = (idx / engine->getWidth()) * tileSize;

                Rectangle rect = {(float)x, (float)y, (float)tileSize - 3, (float)tileSize - 3};

                if (!cell.isRevealed) {
                        // Draw the tile background base
                        DrawRectangleRec(rect, LIGHTGRAY);
                        // Draw the flag on top if marked
                        if (cell.isFlagged) {
                                drawFlagIcon(x, y, tileSize);
                        }
                }
                else {
                        // If the game is lost and this cell exploded, give it a red background;
                        // otherwise draw a normal open tile background
                        if (cell.isMine) {
                                // Red tile highlight for the mine that blew up, RAYWHITE for the others
                                DrawRectangleRec(rect, currentState == GameState::LOST ? RED : RAYWHITE);
                                drawMineIcon(x, y, tileSize);
                        }
                        else {
                                DrawRectangleRec(rect, RAYWHITE);
                                if (cell.neighboringMines > 0) {
                                        DrawText(TextFormat("%d", cell.neighboringMines), x + 10, y + 6, 20,
                                                 getNumberColor(cell.neighboringMines));
                                }
                        }
                }
        }
        EndMode2D();
        EndDrawing();
}

void GameController::handleGameInput() {
        if (IsKeyPressed(KEY_ESCAPE)) {
                appState = AppState::MAIN_MENU;
                return;
        }
        if (IsKeyPressed(KEY_F11)) {
                ToggleFullscreen();
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
                Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
                camera.target = mouseWorldPos;
                camera.offset = GetMousePosition();
                camera.zoom += (wheel * 0.1f);
                if (camera.zoom < 0.2f) camera.zoom = 0.2f;
        }

        if (currentState == GameState::LOST || currentState == GameState::WON

        ) {
                if (IsKeyPressed(KEY_R)) {
                        reset({engine->getWidth(), engine->getHeight(), engine->total_mines()});
                }
                return;
        }

        Vector2 worldPos = GetScreenToWorld2D(GetMousePosition(), camera);
        int gridX = worldPos.x / tileSize;
        int gridY = worldPos.y / tileSize;

        if (gridX < 0 || gridX >= engine->getWidth() || gridY < 0 || gridY >= engine->getHeight()) {
                return;
        }
        int index = engine->getCellID(gridX, gridY);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (currentState == GameState::BEGIN) {
                        engine->generateBoard(gridX, gridY);
                        currentState = GameState::RUNNING;
                        startTime = GetTime(); //Fixed the early clock initialization
                }
                if (engine->getGrid()[index].isRevealed) {
                        engine->chordCell(index);
                }
                else {
                        engine->revealCell(index);
                }
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && currentState == GameState::RUNNING) {
                engine->toggleFlag(index);
        }
}


void GameController::update() {
        if (IsWindowResized()) {
                centerCamera();
        }
        if (currentState != GameState::RUNNING) {
                return;
        }
        timeElapsed = static_cast<int>(GetTime() - startTime);

        if (engine->isGameLost()) {
                currentState = GameState::LOST;
                engine->revealAllMines();
        }
        else if (engine->isGameWon()) {
                currentState = GameState::WON;
                engine->flagAllMines();
        }
}

void GameController::reset(DifficultyConfig config) {
        // Create a fresh empty engine wrapper via default 0-arg constructor
        if (selectedMode == GameMode::CLASSIC) {
                engine = std::make_unique<ClassicMinesweeper>();
        }
        else {
                engine = std::make_unique<NoGuessMinesweeper>();
        }
        engine->loadBoard(config.width, config.height, config.mineCount);

        // Reset tracking metrics
        timeElapsed = 0;
        currentState = GameState::BEGIN;
        appState = AppState::PLAYING;

        // Reset camera matrices
        centerCamera();
}

Color GameController::getNumberColor(int number) {
        switch (number) {
        case 1:
                return GetColor(0x0000FFFF);
        case 2:
                return GetColor(0x008000FF);
        case 3:
                return GetColor(0xFF0000FF);
        case 4:
                return GetColor(0x00008BFF);
        case 5:
                return GetColor(0x800000FF);
        case 6:
                return GetColor(0x008080FF);
        case 7:
                return GetColor(0x000000FF);
        case 8:
                return GetColor(0x808080FF);
        default:
                return WHITE;
        }
}

void GameController::centerCamera() {
        if (!engine) return;
        float boardPixelWidth = (float)(engine->getWidth() * tileSize);
        float boardPixelHeight = (float)(engine->getHeight() * tileSize);

        float scaleX = (float)(GetScreenWidth() - 40) / boardPixelWidth;
        float scaleY = (float)(GetScreenHeight() - 100) / boardPixelHeight;

        camera.zoom = std::min({scaleX, scaleY, 2.0f});

        camera.target = {boardPixelWidth / 2.0f, boardPixelHeight / 2.0f};
        camera.offset = {(float)GetScreenWidth() / 2.0f, (float)(GetScreenHeight() + 60) / 2.0f};
}

void GameController::drawFlagIcon(int cellPixelX, int cellPixelY, int size) {
        float x = static_cast<float>(cellPixelX);
        float y = static_cast<float>(cellPixelY);
        float s = static_cast<float>(size);

        // 1. Pole base (dark gray / black platform)
        Rectangle baseRect = {x + s * 0.25f, y + s * 0.80f, s * 0.50f, s * 0.08f};
        DrawRectangleRec(baseRect, DARKGRAY);

        // 2. Pole mast (vertical line up the center-left)
        Vector2 poleBottom = {x + s * 0.38f, y + s * 0.80f};
        Vector2 poleTop = {x + s * 0.38f, y + s * 0.20f};
        DrawLineEx(poleBottom, poleTop, 2.5f, BLACK);

        // 3. Pennant / Banner (Red Triangle pointing right)
        Vector2 p1 = poleTop;
        Vector2 p2 = {x + s * 0.38f, y + s * 0.52f};
        Vector2 p3 = {x + s * 0.82f, y + s * 0.36f};
        DrawTriangle(p1, p2, p3, RED);
}

void GameController::drawMineIcon(int cellPixelX, int cellPixelY, int size) {
        float x = static_cast<float>(cellPixelX);
        float y = static_cast<float>(cellPixelY);
        float s = static_cast<float>(size);

        Vector2 center = {x + s * 0.5f, y + s * 0.5f};
        float bodyRadius = s * 0.28f;
        float spikeLength = s * 0.40f;
        float spikeThick = 2.0f;

        // 1. Eight radiating spikes (horizontal, vertical, diagonals)
        // Horizontal & Vertical
        DrawLineEx({center.x - spikeLength, center.y}, {center.x + spikeLength, center.y}, spikeThick, BLACK);
        DrawLineEx({center.x, center.y - spikeLength}, {center.x, center.y + spikeLength}, spikeThick, BLACK);

        // Diagonals (offset by ~0.7071 * spikeLength)
        float diagOffset = spikeLength * 0.7071f;
        DrawLineEx({center.x - diagOffset, center.y - diagOffset}, {center.x + diagOffset, center.y + diagOffset},
                   spikeThick, BLACK);
        DrawLineEx({center.x - diagOffset, center.y + diagOffset}, {center.x + diagOffset, center.y - diagOffset},
                   spikeThick, BLACK);

        // 2. Central mine body (solid black/dark circle)
        DrawCircleV(center, bodyRadius, BLACK);

        // 3. Retro specular highlight (small white dot offset toward the top-left)
        Vector2 highlightCenter = {center.x - bodyRadius * 0.35f, center.y - bodyRadius * 0.35f};
        DrawCircleV(highlightCenter, bodyRadius * 0.28f, RAYWHITE);
}
