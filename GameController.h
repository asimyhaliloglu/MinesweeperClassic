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

        static void drawFlagIcon(int cellPixelX, int cellPixelY, int size);

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
