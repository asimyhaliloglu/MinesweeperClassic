//
// Created by asimy on 02.11.2026.
//

#ifndef MINESWEEPERCLASSIC_GAMERENDERER_H
#define MINESWEEPERCLASSIC_GAMERENDERER_H
#include <array>
#include <vector>

#include "BaseMinesweeper.h"
#include "Config.h"
#include "raylib.h"




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
        std::array<Rectangle, 5> toolbar = {};
        float toolbarTop = 0.0f;
};

struct GameRenderState {
        GameState currentState = GameState::BEGIN;
        int timeElapsed = 0;
        int explodedMineIndex = -1; //not exploded
        int hintedCellIndex = -1; //no hint can be given
        HintType hintType = HintType::NONE; //default on classic
        TouchTool touchTool = TouchTool::DIG;
        int bestTime = -1; //not recorded
        int hoveredCell = -1;
        bool isRestartPressed = false;
        std::vector<int> chordPreviewCells{};
};


class GameRenderer {
        static constexpr Color classicCanvas = {192, 192, 192, 255};

        static constexpr Color classicSurface = {194, 194, 194, 255};

        static constexpr Color classicHighlight = {255, 255, 255, 255};

        static constexpr Color classicShadow = {112, 112, 112, 255};

        static constexpr Color classicOutline = {56, 56, 56, 255};

        void drawPixelBlock(float x, float y, float pixelSize, int column, int row, int width, int height,
                            Color color) const;

        void drawBoardTile(Rectangle bounds, Color face, bool raised) const;

        void drawLedCounter(Rectangle bounds, int value) const;

        void drawFaceButton(Rectangle bounds, GameState state, bool pressed, bool shocked) const;

        void drawCenteredText(const char* text, Rectangle bounds, int fontSize, Color color) const;


        void drawBeveledSurface(Rectangle bounds, Color face, float thickness, bool raised) const;

        void drawSevenSegmentDigit(Rectangle bounds, int digit, Color activeColor) const;

        void drawButton(const char* label, Rectangle bounds, bool selected, bool enabled) const;

        void drawFlagIcon(int cellPixelX, int cellPixelY, int size) const;
        void drawMineIcon(int cellPixelX, int cellPixelY, int size) const;
        void drawMobileControls(const UILayout& layout, TouchTool touchTool, GameState currentState) const;
        [[nodiscard]] Color getNumberColor(int color) const;


public:
        GameRenderer() = default;

        ~GameRenderer() = default;

        static UILayout getLayout();

        static float getUIScale();

        static Vector2 toRenderCoordinates(Vector2 screenPosition);

        void renderMainMenu(GameMode selectedMode, int selectedDifficultyIndex,
                            const std::array<std::array<int, 4>, 2>& bestTimes) const;
        void renderGame(const BaseMinesweeper& engine, const Camera2D& camera, int tileSize,
                        const GameRenderState& state) const;
};


#endif
