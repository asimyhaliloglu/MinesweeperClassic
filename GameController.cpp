//
// Created by asimy on 6/25/2026.
//

#include "GameController.h"

#include "BaseMinesweeper.h"

#include "NoGuessMinesweeper.h"

#include "ClassicMinesweeper.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>

#ifndef MINESWEEPER_MOBILE_CONTROLS
#if defined(__ANDROID__) || defined(PLATFORM_IOS)
#define MINESWEEPER_MOBILE_CONTROLS 1
#else
#define MINESWEEPER_MOBILE_CONTROLS 0
#endif
#endif

namespace {
        constexpr float tapMovementTolerance = 8.0f;
        constexpr float minimumZoom = 0.2f;
        constexpr float maximumZoom = 4.0f;

        void drawCenteredText(const char* text, Rectangle bounds, int fontSize, Color color) {
                while (fontSize > 10 && (MeasureText(text, fontSize) > bounds.width - 12.0f
                                       || fontSize > bounds.height - 8.0f)) {
                        --fontSize;
                }
                DrawText(text, static_cast<int>(bounds.x + (bounds.width - MeasureText(text, fontSize)) * 0.5f),
                         static_cast<int>(bounds.y + (bounds.height - fontSize) * 0.5f), fontSize, color);
        }

        void drawButton(const char* label, Rectangle bounds, bool selected = false, bool enabled = true) {
                Color background = selected ? BLUE : GRAY;
                if (!enabled) background = GetColor(0x383838FF);
                else if (CheckCollisionPointRec(GetMousePosition(), bounds)) background = selected ? DARKBLUE : LIGHTGRAY;
                DrawRectangleRec(bounds, background);
                drawCenteredText(label, bounds, 20, enabled ? RAYWHITE : GRAY);
        }
}


void GameController::init() {
        if (initialized) return;
        selectedMode = GameMode::CLASSIC;
        engine = std::make_unique<ClassicMinesweeper>();
        engine->loadBoard(BEGINNER.width, BEGINNER.height, BEGINNER.mineCount);
#if defined(__ANDROID__) || defined(PLATFORM_IOS)
        SetConfigFlags(FLAG_VSYNC_HINT);
        InitWindow(0, 0, "Minesweeper Classic");
#else
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
        InitWindow(1280, 720, "Minesweeper Classic");
#endif
        if (!IsWindowReady()) {
                engine.reset();
                return;
        }
        SetExitKey(KEY_NULL);
#if !defined(PLATFORM_IOS)
        SetTargetFPS(120);
#endif

        camera.target = {0.0f, 0.0f};
        camera.offset = {0.0f, 0.0f};
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;

        appState = AppState::MAIN_MENU;
        currentState = GameState::BEGIN;
        timeElapsed = 0;
        touchTool = TouchTool::DIG;
        initialized = true;
        resetTouchState();
}

void GameController::frame() {
        if (!initialized) return;
        if (IsWindowResized()) {
                resetTouchState();
                centerCamera();
        }
        if (IsWindowFocused()) {
                handleInput();
                update();
        }
        else {
                resetTouchState();
        }
        render();
}

void GameController::shutdown() {
        if (!initialized) return;
        resetTouchState();
        engine.reset();
        CloseWindow();
        initialized = false;
}

void GameController::run() {
        init();
        while (initialized && !WindowShouldClose()) {
                frame();
        }
        shutdown();
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

GameController::UILayout GameController::getLayout() const {
        UILayout layout;
        float screenWidth = static_cast<float>(std::max(1, GetScreenWidth()));
        float screenHeight = static_cast<float>(std::max(1, GetScreenHeight()));
        float titleHeight = std::min(48.0f, std::max(20.0f, (screenWidth - 24.0f) / 7.0f));
        float buttonHeight = std::clamp((screenHeight - titleHeight - 72.0f) / 5.0f, 24.0f, 52.0f);
        float buttonWidth = std::max(1.0f, std::min(360.0f, screenWidth - 24.0f));
        float menuHeight = titleHeight + 16.0f + buttonHeight * 5.0f + 32.0f;
        float menuTop = std::max(12.0f, (screenHeight - menuHeight) * 0.5f);
        layout.title = {0.0f, menuTop, screenWidth, titleHeight + 8.0f};
        layout.mode = {(screenWidth - buttonWidth) * 0.5f, menuTop + titleHeight + 16.0f,
                       buttonWidth, buttonHeight};
        for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                layout.difficulties[index] = {layout.mode.x, layout.mode.y + (index + 1) * (buttonHeight + 8.0f),
                                              buttonWidth, buttonHeight};
        }

        float headerHeight = currentState == GameState::WON || currentState == GameState::LOST ? 96.0f : 60.0f;
        layout.header = {0.0f, 0.0f, screenWidth, headerHeight};
        layout.toolbarTop = screenHeight;
        if (MINESWEEPER_MOBILE_CONTROLS) {
                int columns = screenWidth < 360.0f ? 3 : 5;
                int rows = columns == 3 ? 2 : 1;
                float controlWidth = std::max(1.0f, (screenWidth - 24.0f - (columns - 1) * 8.0f) / columns);
                layout.toolbarTop = std::max(headerHeight, screenHeight - (rows * 48.0f + (rows - 1) * 8.0f + 24.0f));
                for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                        int row = index / columns;
                        int column = index % columns;
                        int rowControls = std::min(columns, static_cast<int>(layout.toolbar.size()) - row * columns);
                        float rowWidth = rowControls * controlWidth + (rowControls - 1) * 8.0f;
                        layout.toolbar[index] = {(screenWidth - rowWidth) * 0.5f + column * (controlWidth + 8.0f),
                                                 layout.toolbarTop + 12.0f + row * 56.0f, controlWidth, 48.0f};
                }
        }
        layout.board = {0.0f, headerHeight, screenWidth, std::max(0.0f, layout.toolbarTop - headerHeight)};
        return layout;
}

GameController::HitTarget GameController::hitTest(Vector2 position) const {
        UILayout layout = getLayout();
        if (appState == AppState::MAIN_MENU) {
                if (CheckCollisionPointRec(position, layout.mode)) return {Control::MODE};
                constexpr Control difficulties[] = {Control::BEGINNER, Control::INTERMEDIATE, Control::EXPERT, Control::EVIL};
                for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                        if (CheckCollisionPointRec(position, layout.difficulties[index])) return {difficulties[index]};
                }
                return {};
        }
        if (appState != AppState::PLAYING) return {};
        if (MINESWEEPER_MOBILE_CONTROLS) {
                constexpr Control controls[] = {Control::DIG, Control::FLAG, Control::FIT, Control::RESTART, Control::MENU};
                for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                        if (CheckCollisionPointRec(position, layout.toolbar[index])) return {controls[index]};
                }
        }
        if (!CheckCollisionPointRec(position, layout.board)) return {};
        Vector2 worldPosition = GetScreenToWorld2D(position, camera);
        int gridX = static_cast<int>(std::floor(worldPosition.x / tileSize));
        int gridY = static_cast<int>(std::floor(worldPosition.y / tileSize));
        if (gridX < 0 || gridX >= engine->getWidth() || gridY < 0 || gridY >= engine->getHeight()) return {};
        return {Control::CELL, engine->getCellID(gridX, gridY)};
}

void GameController::activateTarget(HitTarget target) {
        switch (target.control) {
        case Control::MODE:
                selectedMode = selectedMode == GameMode::CLASSIC ? GameMode::NO_GUESS : GameMode::CLASSIC;
                break;
        case Control::BEGINNER:
                reset(BEGINNER);
                break;
        case Control::INTERMEDIATE:
                reset(INTERMEDIATE);
                break;
        case Control::EXPERT:
                reset(EXPERT);
                break;
        case Control::EVIL:
                reset(EVIL);
                break;
        case Control::DIG:
                touchTool = TouchTool::DIG;
                break;
        case Control::FLAG:
                if (currentState == GameState::RUNNING) touchTool = TouchTool::FLAG;
                break;
        case Control::FIT:
                centerCamera();
                break;
        case Control::RESTART:
                reset({engine->getWidth(), engine->getHeight(), engine->total_mines()});
                break;
        case Control::MENU:
                appState = AppState::MAIN_MENU;
                resetTouchState();
                break;
        case Control::CELL:
                actOnCell(target.cellIndex, touchTool);
                break;
        case Control::NONE:
                break;
        }
}

void GameController::actOnCell(int index, TouchTool tool) {
        if (currentState == GameState::WON || currentState == GameState::LOST) return;
        if (tool == TouchTool::FLAG) {
                if (currentState == GameState::RUNNING) engine->toggleFlag(index);
                return;
        }
        if (currentState == GameState::BEGIN) {
                engine->generateBoard(index % engine->getWidth(), index / engine->getWidth());
                currentState = GameState::RUNNING;
                startTime = GetTime();
        }
        if (engine->getGrid()[index].isRevealed) engine->chordCell(index);
        else engine->revealCell(index);
}

void GameController::renderMainMenu() const {
        BeginDrawing();
        ClearBackground(DARKGRAY);
        UILayout layout = getLayout();
        drawCenteredText("MINESWEEPER", layout.title, static_cast<int>(layout.title.height - 8.0f), RAYWHITE);
        drawButton(selectedMode == GameMode::CLASSIC ? "Mode: Classic" : "Mode: NoGuess", layout.mode, true);
        const char* labels[] = {"Beginner (9x9)", "Intermediate (16x16)", "Expert (30x16)", "Evil (30x20)"};
        for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                drawButton(labels[index], layout.difficulties[index]);
        }
        EndDrawing();
}

void GameController::handleMenuInput() {
        if (IsKeyPressed(KEY_M)) {
                selectedMode = selectedMode == GameMode::CLASSIC ? GameMode::NO_GUESS : GameMode::CLASSIC;
        }
#if !defined(__ANDROID__) && !defined(PLATFORM_IOS)
        if (IsKeyPressed(KEY_F11)) {
                ToggleFullscreen();
        }
#endif
        if (handleTouchGestures()) return;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                activateTarget(hitTest(GetMousePosition()));
        }
}


void GameController::renderGame() const {
        BeginDrawing();
        ClearBackground(DARKGRAY);

        UILayout layout = getLayout();
        DrawRectangleRec(layout.header, GetColor(0x333333FF));
        drawCenteredText(TextFormat("MINES: %03d", engine->total_mines() - engine->total_flags()),
                         {0.0f, 0.0f, layout.header.width * 0.5f, 60.0f}, 20, RED);
        drawCenteredText(TextFormat("TIME: %03d", timeElapsed),
                         {layout.header.width * 0.5f, 0.0f, layout.header.width * 0.5f, 60.0f}, 20, RED);
        if (currentState == GameState::WON || currentState == GameState::LOST) {
                const char* message = currentState == GameState::WON ? "YOU WIN!" : "GAME OVER";
                if (!MINESWEEPER_MOBILE_CONTROLS) message = TextFormat("%s  (R: restart / Esc: menu)", message);
                drawCenteredText(message, {0.0f, 60.0f, layout.header.width, 32.0f}, 20,
                                 currentState == GameState::WON ? GREEN : RED);
        }

        BeginScissorMode(static_cast<int>(layout.board.x), static_cast<int>(layout.board.y),
                         static_cast<int>(layout.board.width), static_cast<int>(layout.board.height));
        BeginMode2D(camera);
        for (int idx = 0; idx < static_cast<int>(engine->getGrid().size()); ++idx) {
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
        EndScissorMode();
        renderMobileControls();
        EndDrawing();
}

void GameController::handleGameInput() {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK)) {
                appState = AppState::MAIN_MENU;
                resetTouchState();
                return;
        }
#if !defined(__ANDROID__) && !defined(PLATFORM_IOS)
        if (IsKeyPressed(KEY_F11)) {
                ToggleFullscreen();
        }
#endif
        if ((currentState == GameState::LOST || currentState == GameState::WON) && IsKeyPressed(KEY_R)) {
                reset({engine->getWidth(), engine->getHeight(), engine->total_mines()});
                return;
        }
        if (handleTouchGestures()) return;
        HitTarget target = hitTest(GetMousePosition());
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && target.control != Control::CELL) {
                activateTarget(target);
                return;
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(), getLayout().board)) {
                Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
                camera.zoom = std::clamp(camera.zoom + wheel * 0.1f, minimumZoom, maximumZoom);
                Vector2 zoomedWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
                camera.target.x += mouseWorldPos.x - zoomedWorldPos.x;
                camera.target.y += mouseWorldPos.y - zoomedWorldPos.y;
                target = hitTest(GetMousePosition());
        }
        if (target.control != Control::CELL) return;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                activateTarget(target);
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
                actOnCell(target.cellIndex, TouchTool::FLAG);
        }
}


void GameController::update() {
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
        touchTool = TouchTool::DIG;
        resetTouchState();

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

        UILayout layout = getLayout();
        float scaleX = std::max(1.0f, layout.board.width - 40.0f) / boardPixelWidth;
        float scaleY = std::max(1.0f, layout.board.height - 40.0f) / boardPixelHeight;

        camera.zoom = std::clamp(std::min({scaleX, scaleY, 2.0f}), minimumZoom, maximumZoom);

        camera.target = {boardPixelWidth / 2.0f, boardPixelHeight / 2.0f};
        camera.offset = {layout.board.x + layout.board.width * 0.5f, layout.board.y + layout.board.height * 0.5f};
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


void GameController::resetTouchState() {
        primaryTouchId = -1;
        touchPending = false;
        touchBlocked = GetTouchPointCount() > 0;
        suppressMouseUntilRelease = true;
        pinchTouchIds = {-1, -1};
        pinchDistance = 0.0f;
        pinchActive = false;
}

bool GameController::handleTouchGestures() {
        int touchCount = GetTouchPointCount();
        if (touchCount == 0) {
                if (touchPending && !touchBlocked) {
                        touchLastPos = GetTouchPosition(0);
                        float deltaX = touchLastPos.x - touchStartPos.x;
                        float deltaY = touchLastPos.y - touchStartPos.y;
                        if (deltaX * deltaX + deltaY * deltaY > tapMovementTolerance * tapMovementTolerance) touchPending = false;
                }
                bool consumed = primaryTouchId != -1 || touchBlocked || suppressMouseUntilRelease;
                bool activate = touchPending && !touchBlocked && hitTest(touchLastPos) == touchStartTarget;
                HitTarget target = touchStartTarget;
                primaryTouchId = -1;
                touchPending = false;
                touchBlocked = false;
                pinchActive = false;
                pinchTouchIds = {-1, -1};
                pinchDistance = 0.0f;
                if (activate) activateTarget(target);
                if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                        suppressMouseUntilRelease = false;
                }
                return consumed;
        }

        suppressMouseUntilRelease = true;
        if (touchCount >= 2) {
                touchPending = false;
                touchBlocked = true;
                if (appState != AppState::PLAYING) return true;

                int firstIndex = -1;
                int secondIndex = -1;
                for (int index = 0; index < touchCount; ++index) {
                        int touchId = GetTouchPointId(index);
                        if (touchId == pinchTouchIds[0]) firstIndex = index;
                        if (touchId == pinchTouchIds[1]) secondIndex = index;
                }
                bool rebase = !pinchActive || firstIndex == -1 || secondIndex == -1;
                if (rebase) {
                        firstIndex = 0;
                        secondIndex = 1;
                }
                Vector2 firstPosition = GetTouchPosition(firstIndex);
                Vector2 secondPosition = GetTouchPosition(secondIndex);
                Rectangle viewport = getLayout().board;
                if (!pinchActive && (!CheckCollisionPointRec(firstPosition, viewport)
                                     || !CheckCollisionPointRec(secondPosition, viewport))) return true;

                Vector2 midpoint = {(firstPosition.x + secondPosition.x) * 0.5f,
                                    (firstPosition.y + secondPosition.y) * 0.5f};
                float distance = std::hypot(firstPosition.x - secondPosition.x, firstPosition.y - secondPosition.y);
                if (!rebase) {
                        Vector2 worldAnchor = GetScreenToWorld2D(pinchMidpoint, camera);
                        if (pinchDistance > 1.0f && distance > 1.0f) {
                                camera.zoom = std::clamp(camera.zoom * distance / pinchDistance, minimumZoom, maximumZoom);
                        }
                        Vector2 movedWorldAnchor = GetScreenToWorld2D(midpoint, camera);
                        camera.target.x += worldAnchor.x - movedWorldAnchor.x;
                        camera.target.y += worldAnchor.y - movedWorldAnchor.y;
                }
                pinchActive = true;
                pinchTouchIds = {GetTouchPointId(firstIndex), GetTouchPointId(secondIndex)};
                pinchMidpoint = midpoint;
                pinchDistance = distance;
                return true;
        }

        pinchActive = false;
        if (touchBlocked) return true;
        int touchId = GetTouchPointId(0);
        Vector2 position = GetTouchPosition(0);
        if (primaryTouchId == -1) {
                primaryTouchId = touchId;
                touchStartPos = position;
                touchLastPos = position;
                touchStartTarget = hitTest(position);
                touchPending = touchStartTarget.control != Control::NONE;
        }
        else if (primaryTouchId != touchId) {
                touchBlocked = true;
                touchPending = false;
        }
        else {
                float deltaX = position.x - touchStartPos.x;
                float deltaY = position.y - touchStartPos.y;
                if (deltaX * deltaX + deltaY * deltaY > tapMovementTolerance * tapMovementTolerance) touchPending = false;
                touchLastPos = position;
        }
        return true;
}

void GameController::renderMobileControls() const {
        if (!MINESWEEPER_MOBILE_CONTROLS) return;
        UILayout layout = getLayout();
        DrawRectangleRec({0.0f, layout.toolbarTop, layout.header.width,
                          static_cast<float>(GetScreenHeight()) - layout.toolbarTop}, GetColor(0x333333FF));
        const char* labels[] = {"Dig", "Flag", "Fit", "Restart", "Menu"};
        for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                bool selected = (index == 0 && touchTool == TouchTool::DIG) || (index == 1 && touchTool == TouchTool::FLAG);
                bool enabled = index != 1 || currentState == GameState::RUNNING;
                drawButton(labels[index], layout.toolbar[index], selected, enabled);
        }
}
