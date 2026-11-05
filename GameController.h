#ifndef MINESWEEPERCLASSIC_GAMECONTROLLER_H
#define MINESWEEPERCLASSIC_GAMECONTROLLER_H

#include <memory>
#include "raylib.h"
#include "BaseMinesweeper.h"
#include "Config.h"
#include "AudioManager.h"
#include "GameRenderer.h"
#include "HighScoreStorage.h"
#include "InputManager.h"

class GameController {
public:
        GameController();
        void init();
        void frame();
        void shutdown();
        void run();

private:
        // Subsystems
        GameRenderer renderer;
        AudioManager audio;
        InputManager input;

        // State & Engine
        std::unique_ptr<BaseMinesweeper> engine;
        AppState appState = AppState::MAIN_MENU;
        GameMode selectedMode = GameMode::CLASSIC;
        GameState currentState = GameState::BEGIN;
        TouchTool touchTool = TouchTool::DIG;
        Camera2D camera = {};

        // Session Metrics
        bool initialized = false;
        int tileSize = 32;
        double elapsedTimeSeconds = 0.0;
        double lastTimerUpdateTime = 0.0;
        int timeElapsed = 0;
        int explodedMineIndex = -1;
        int hintedCellIndex = -1;
        HintType hintType = HintType::NONE;
        BestTimes bestTimes{};

        void update();
        void render() const;
        void renderMainMenu() const;
        void renderGame() const;

        void handleAction(const UserAction& action);
        void actOnCell(int index, TouchTool tool);
        void requestHint();
        void recordWin();
        void reset(DifficultyConfig config);
        void centerCamera();

        [[nodiscard]] int getSelectedDifficultyIndex() const;
        [[nodiscard]] int getSelectedModeIndex() const;
        [[nodiscard]] int getBestTimeForSelection() const;
};

#endif // MINESWEEPERCLASSIC_GAMECONTROLLER_H
