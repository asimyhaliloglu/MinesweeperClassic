//
// Created by asimy on 6/25/2026.
//

#ifndef MINESWEEPERCLASSIC_GAMECONTROLLER_H
#define MINESWEEPERCLASSIC_GAMECONTROLLER_H
#include <memory>

#include "ClassicMinesweeper.h"
#include "raylib.h"
#include "BaseMinesweeper.h"
#include "Config.h"

enum class GameState {
        BEGIN,
        RUNNING,
        WON,
        LOST
};

class GameController {
private:
        std::unique_ptr<BaseMinesweeper> engine;
        AppState appState = AppState::MAIN_MENU;
        GameMode selectedMode = GameMode::CLASSIC;
        GameState currentState = GameState::BEGIN;
        int tileSize = 32;

        double startTime = 0.0;
        int timeElapsed = 0;

        Camera2D camera = {};

        void render() const;

        void renderMainMenu() const;

        void renderGame() const;

        void handleInput();

        void handleMenuInput();

        void handleGameInput();

        void update();

        static Color getNumberColor(int number);

        void reset(DifficultyConfig config);

        void centerCamera();

        static void drawFlagIcon(int cellPixelX, int cellPixelY, int size) {
                float x = static_cast<float>(cellPixelX);
                float y = static_cast<float>(cellPixelY);
                float s = static_cast<float>(size);

                // 1. Pole base (dark gray / black platform)
                Rectangle baseRect = { x + s * 0.25f, y + s * 0.80f, s * 0.50f, s * 0.08f };
                DrawRectangleRec(baseRect, DARKGRAY);

                // 2. Pole mast (vertical line up the center-left)
                Vector2 poleBottom = { x + s * 0.38f, y + s * 0.80f };
                Vector2 poleTop    = { x + s * 0.38f, y + s * 0.20f };
                DrawLineEx(poleBottom, poleTop, 2.5f, BLACK);

                // 3. Pennant / Banner (Red Triangle pointing right)
                Vector2 p1 = poleTop;
                Vector2 p2 = { x + s * 0.38f, y + s * 0.52f };
                Vector2 p3 = { x + s * 0.82f, y + s * 0.36f };
                DrawTriangle(p1, p2, p3, RED);
        }

public:
        GameController() {
                camera.target = {0.0f, 0.0f};
                camera.offset = {0.0f, 0.0f};
                camera.rotation = 0.0f;
                camera.zoom = 1.0f;
        }
        void run();
};


#endif //MINESWEEPERCLASSIC_GAMECONTROLLER_H
