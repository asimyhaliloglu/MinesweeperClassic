#ifndef MINESWEEPERCLASSIC_INPUTMANAGER_H
#define MINESWEEPERCLASSIC_INPUTMANAGER_H

#include <array>
#include "raylib.h"
#include "Config.h"
#include "BaseMinesweeper.h"
#include "GameRenderer.h"

// Semantic action produced by user input
struct UserAction {
        enum class Type {
                NONE,
                ACT_ON_CELL,
                TOGGLE_MODE,
                SELECT_DIFFICULTY,
                SELECT_TOOL,
                CENTER_CAMERA,
                REQUEST_HINT,
                RESTART_GAME,
                OPEN_MENU
        };

        Type type = Type::NONE;
        int cellIndex = -1;
        TouchTool tool = TouchTool::DIG;
        DifficultyConfig difficulty = BEGINNER;
};

class InputManager {
public:
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
                HINT,
                RESTART,
                MENU,
                CELL
        };

        struct HitTarget {
                Control control = Control::NONE;
                int cellIndex = -1;

                bool operator==(const HitTarget&) const = default;
        };

        InputManager() = default;

        // Resets touch and gesture state upon window resize, focus loss, or navigation
        void reset();

        // Polls hardware input and translates into high-level intent
        [[nodiscard]] UserAction processInput(AppState appState, GameState gameState,
                                              TouchTool activeTool, Camera2D& camera,
                                              int tileSize, const BaseMinesweeper* engine);

        // Tests screen coordinate against UI layout elements and the board grid
        [[nodiscard]] HitTarget hitTest(Vector2 position, AppState appState,
                                        const Camera2D& camera, int tileSize,
                                        const BaseMinesweeper* engine) const;

private:
        static constexpr float tapMovementTolerance = 8.0f;
        static constexpr float minimumZoom = 0.2f;
        static constexpr float maximumZoom = 4.0f;

        // Single touch tracking
        int primaryTouchId = -1;
        Vector2 touchStartPos = {};
        Vector2 touchLastPos = {};
        HitTarget touchStartTarget = {};
        bool touchPending = false;
        bool touchBlocked = false;

        // Multi-touch pinch/pan tracking
        std::array<int, 2> pinchTouchIds = {-1, -1};
        Vector2 pinchMidpoint = {};
        float pinchDistance = 0.0f;
        bool pinchActive = false;

        // Synthetic touch-mouse event suppression
        Vector2 suppressedTouchReleasePosition = {};
        int touchMouseSuppressionFrames = 0;
        bool suppressMouseUntilRelease = false;

        [[nodiscard]] bool handleTouchGestures(AppState appState, Camera2D& camera,
                                               int tileSize, const BaseMinesweeper* engine,
                                               HitTarget& outActivatedTarget);

        static bool isWithinTouchTolerance(Vector2 first, Vector2 second);
};

#endif // MINESWEEPERCLASSIC_INPUTMANAGER_H
