//
// Created by asimy on 6/25/2026.
//

#ifndef MINESWEEPERCLASSIC_GAMECONTROLLER_H
#define MINESWEEPERCLASSIC_GAMECONTROLLER_H
#include <array>
#include <memory>

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
        enum class TouchTool {
                DIG,
                FLAG
        };


        enum class Control {
                NONE,
                MODE,
                BEGINNER,
                INTERMEDIATE,
                EXPERT,
                EVIL,
                DIG,
                FLAG,
                FIT,
                RESTART,
                MENU,
                CELL
        };

        struct HitTarget {
                Control control = Control::NONE;
                int cellIndex = -1;

                bool operator==(const HitTarget&) const = default;
        };

        struct UILayout {
                Rectangle title = {};
                Rectangle mode = {};
                std::array<Rectangle, 4> difficulties = {};
                Rectangle header = {};
                Rectangle mineCounter = {};
                Rectangle restart = {};
                Rectangle timeCounter = {};
                Rectangle status = {};
                Rectangle board = {};
                std::array<Rectangle, 4> toolbar = {};
                float toolbarTop = 0.0f;
        };

        bool initialized = false;
        TouchTool touchTool = TouchTool::DIG;
        int primaryTouchId = -1;
        Vector2 touchStartPos = {};
        Vector2 touchLastPos = {};
        HitTarget touchStartTarget = {};
        bool touchPending = false;
        bool touchBlocked = false;
        Vector2 suppressedTouchReleasePosition = {};
        int touchMouseSuppressionFrames = 0;
        bool suppressMouseUntilRelease = false;
        std::array<int, 2> pinchTouchIds = {-1, -1};
        Vector2 pinchMidpoint = {};
        float pinchDistance = 0.0f;
        bool pinchActive = false;

        enum class SoundEffect {
                DIG,
                FLAG,
                CHORD,
                LOSS,
                WIN
        };

        std::array<Sound, 5> soundEffects{};
        std::array<std::array<int, 4>, 2> bestTimes{};
        bool audioDeviceReady = false;

        bool handleTouchGestures();
        void resetTouchState();
        void renderMobileControls() const;
        [[nodiscard]] UILayout getLayout() const;
        [[nodiscard]] HitTarget hitTest(Vector2 position) const;
        void activateTarget(HitTarget target);
        void actOnCell(int index, TouchTool tool);
        void initializeAudio();
        void unloadAudio();
        void playSoundEffect(SoundEffect effect) const;
        void loadBestTimes();
        void saveBestTimes() const;
        void recordWin();
        [[nodiscard]] int getSelectedDifficultyIndex() const;
        [[nodiscard]] int getSelectedModeIndex() const;
        [[nodiscard]] int getBestTimeForSelection() const;

        std::unique_ptr<BaseMinesweeper> engine;
        AppState appState = AppState::MAIN_MENU;
        GameMode selectedMode = GameMode::CLASSIC;
        GameState currentState = GameState::BEGIN;
        int tileSize = 32;

        double startTime = 0.0;
        int timeElapsed = 0;
        int explodedMineIndex = -1;

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

        static void drawMineIcon(int cellPixelX, int cellPixelY, int size);

public:
        GameController() {
                camera.target = {0.0f, 0.0f};
                camera.offset = {0.0f, 0.0f};
                camera.rotation = 0.0f;
                camera.zoom = 1.0f;
        }

        void init();
        void frame();
        void shutdown();
        void run();
};


#endif //MINESWEEPERCLASSIC_GAMECONTROLLER_H
