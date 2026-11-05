#include "InputManager.h"
#include <algorithm>
#include <cmath>

#ifndef MINESWEEPER_MOBILE_CONTROLS
#if defined(__ANDROID__) || defined(PLATFORM_IOS) || defined(__EMSCRIPTEN__)
#define MINESWEEPER_MOBILE_CONTROLS 1
#else
#define MINESWEEPER_MOBILE_CONTROLS 0
#endif
#endif

namespace {
        constexpr bool mobileControlsEnabled = MINESWEEPER_MOBILE_CONTROLS != 0;
}

bool InputManager::isWithinTouchTolerance(Vector2 first, Vector2 second) {
        const float deltaX = first.x - second.x;
        const float deltaY = first.y - second.y;
        return (deltaX * deltaX + deltaY * deltaY) < (tapMovementTolerance * tapMovementTolerance);
}

void InputManager::reset() {
        primaryTouchId = -1;
        touchPending = false;
        touchBlocked = GetTouchPointCount() > 0;
        if (touchBlocked) touchLastPos = GetTouchPosition(0);
        touchMouseSuppressionFrames = 0;
        suppressMouseUntilRelease = false;
        pinchTouchIds = {-1, -1};
        pinchDistance = 0.0f;
        pinchActive = false;
}

InputManager::HitTarget InputManager::hitTest(Vector2 position, AppState appState,
                                              const Camera2D& camera, int tileSize,
                                              const BaseMinesweeper* engine) const {
        const UILayout layout = GameRenderer::getLayout();

        if (appState == AppState::MAIN_MENU) {
                if (CheckCollisionPointRec(position, layout.mode)) return {Control::MODE};
                constexpr Control difficulties[] = {
                        Control::BEGINNER, Control::INTERMEDIATE, Control::EXPERT, Control::EVIL
                };
                for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                        if (CheckCollisionPointRec(position, layout.difficulties[index])) {
                                return {difficulties[index]};
                        }
                }
                return {};
        }

        if (appState != AppState::PLAYING) return {};

        if (CheckCollisionPointRec(position, layout.restart)) return {Control::RESTART};

        if (mobileControlsEnabled) {
                constexpr Control controls[] = {
                        Control::DIG, Control::FLAG, Control::FIT, Control::HINT, Control::MENU
                };
                for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                        if (CheckCollisionPointRec(position, layout.toolbar[index])) {
                                return {controls[index]};
                        }
                }
        }

        if (!engine || !CheckCollisionPointRec(position, layout.board)) return {};

        Vector2 worldPos = GetScreenToWorld2D(GameRenderer::toRenderCoordinates(position), camera);
        int gridX = static_cast<int>(std::floor(worldPos.x / tileSize));
        int gridY = static_cast<int>(std::floor(worldPos.y / tileSize));

        if (gridX < 0 || gridX >= engine->getWidth() || gridY < 0 || gridY >= engine->getHeight()) {
                return {};
        }
        return {Control::CELL, engine->getCellID(gridX, gridY)};
}

bool InputManager::handleTouchGestures(AppState appState, Camera2D& camera,
                                       int tileSize, const BaseMinesweeper* engine,
                                       HitTarget& outActivatedTarget) {
        const int touchCount = GetTouchPointCount();

        if (touchCount == 0) {
                const bool hadTouch = (primaryTouchId != -1) || touchBlocked || pinchActive;
                const bool activate = touchPending && !touchBlocked
                        && isWithinTouchTolerance(touchStartPos, touchLastPos)
                        && hitTest(touchLastPos, appState, camera, tileSize, engine) == touchStartTarget;

                HitTarget target = touchStartTarget;
                const Vector2 releasePos = touchLastPos;

                primaryTouchId = -1;
                touchPending = false;
                touchBlocked = false;
                pinchActive = false;
                pinchTouchIds = {-1, -1};
                pinchDistance = 0.0f;

                if (activate) {
                        outActivatedTarget = target;
                }

                if (hadTouch) {
                        suppressedTouchReleasePosition = releasePos;
                        touchMouseSuppressionFrames = 2;
                        suppressMouseUntilRelease = false;
                        return true;
                }

                if (touchMouseSuppressionFrames > 0) {
                        const bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
                        const bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                        if (isWithinTouchTolerance(GetMousePosition(), suppressedTouchReleasePosition)
                                && (mousePressed || mouseDown)) {
                                suppressMouseUntilRelease = mouseDown;
                                return true;
                        }
                        if (suppressMouseUntilRelease) {
                                if (!mouseDown) suppressMouseUntilRelease = false;
                                return true;
                        }
                        --touchMouseSuppressionFrames;
                }
                return false;
        }

        // 2-Finger Pinch / Pan
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

                const bool rebase = !pinchActive || firstIndex == -1 || secondIndex == -1;
                if (rebase) {
                        firstIndex = 0;
                        secondIndex = 1;
                }

                Vector2 firstPos = GetTouchPosition(firstIndex);
                Vector2 secondPos = GetTouchPosition(secondIndex);
                Rectangle viewport = GameRenderer::getLayout().board;

                if (!pinchActive && (!CheckCollisionPointRec(firstPos, viewport)
                        || !CheckCollisionPointRec(secondPos, viewport))) {
                        return true;
                }

                Vector2 midpoint = {(firstPos.x + secondPos.x) * 0.5f, (firstPos.y + secondPos.y) * 0.5f};
                float distance = std::hypot(firstPos.x - secondPos.x, firstPos.y - secondPos.y);

                if (!rebase) {
                        Vector2 worldAnchor = GetScreenToWorld2D(GameRenderer::toRenderCoordinates(pinchMidpoint),
                                                                 camera);
                        if (pinchDistance > 1.0f && distance > 1.0f) {
                                camera.zoom = std::clamp(camera.zoom * distance / pinchDistance, minimumZoom,
                                                         maximumZoom);
                        }
                        Vector2 movedWorldAnchor = GetScreenToWorld2D(GameRenderer::toRenderCoordinates(midpoint),
                                                                      camera);
                        camera.target.x += worldAnchor.x - movedWorldAnchor.x;
                        camera.target.y += worldAnchor.y - movedWorldAnchor.y;
                }

                pinchActive = true;
                pinchTouchIds = {GetTouchPointId(firstIndex), GetTouchPointId(secondIndex)};
                pinchMidpoint = midpoint;
                pinchDistance = distance;
                return true;
        }

        // Single touch tracking
        pinchActive = false;
        if (touchBlocked) return true;

        int touchId = GetTouchPointId(0);
        Vector2 position = GetTouchPosition(0);

        if (primaryTouchId == -1) {
                primaryTouchId = touchId;
                touchStartPos = position;
                touchLastPos = position;
                touchStartTarget = hitTest(position, appState, camera, tileSize, engine);
                touchPending = touchStartTarget.control != Control::NONE;
        }
        else if (primaryTouchId != touchId) {
                touchBlocked = true;
                touchPending = false;
        }
        else {
                if (!isWithinTouchTolerance(touchStartPos, position)) {
                        touchPending = false;
                }
                touchLastPos = position;
        }
        return true;
}

UserAction InputManager::processInput(AppState appState, GameState gameState,
                                      TouchTool activeTool, Camera2D& camera,
                                      int tileSize, const BaseMinesweeper* engine) {
#if !defined(__ANDROID__) && !defined(PLATFORM_IOS)
        if (IsKeyPressed(KEY_F11)) {
                ToggleFullscreen();
        }
#endif

        if (appState == AppState::PLAYING) {
                if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK)) {
                        reset();
                        return {UserAction::Type::OPEN_MENU};
                }
                if ((gameState == GameState::LOST || gameState == GameState::WON) && IsKeyPressed(KEY_R)) {
                        return {UserAction::Type::RESTART_GAME};
                }
                if (IsKeyPressed(KEY_H)) {
                        return {UserAction::Type::REQUEST_HINT};
                }
        }
        else if (appState == AppState::MAIN_MENU) {
                if (IsKeyPressed(KEY_M)) {
                        return {UserAction::Type::TOGGLE_MODE};
                }
        }

        HitTarget activatedTarget{};
        if (handleTouchGestures(appState, camera, tileSize, engine, activatedTarget)) {
                if (activatedTarget.control != Control::NONE) {
                        switch (activatedTarget.control) {
                        case Control::MODE: return {UserAction::Type::TOGGLE_MODE};
                        case Control::BEGINNER: return {UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, BEGINNER};
                        case Control::INTERMEDIATE: return {
                                        UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, INTERMEDIATE
                                };
                        case Control::EXPERT: return {UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, EXPERT};
                        case Control::EVIL: return {UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, EVIL};
                        case Control::DIG: return {UserAction::Type::SELECT_TOOL, -1, TouchTool::DIG};
                        case Control::FLAG: return {UserAction::Type::SELECT_TOOL, -1, TouchTool::FLAG};
                        case Control::FIT: return {UserAction::Type::CENTER_CAMERA};
                        case Control::HINT: return {UserAction::Type::REQUEST_HINT};
                        case Control::RESTART: return {UserAction::Type::RESTART_GAME};
                        case Control::MENU: return {UserAction::Type::OPEN_MENU};
                        case Control::CELL: return {
                                        UserAction::Type::ACT_ON_CELL, activatedTarget.cellIndex, activeTool
                                };
                        default: break;
                        }
                }
                return {};
        }

        // Desktop Mouse Wheel Zoom
        if (appState == AppState::PLAYING) {
                float wheel = GetMouseWheelMove();
                if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(), GameRenderer::getLayout().board)) {
                        Vector2 mousePos = GameRenderer::toRenderCoordinates(GetMousePosition());
                        Vector2 mouseWorld = GetScreenToWorld2D(mousePos, camera);
                        camera.zoom = std::clamp(camera.zoom + wheel * 0.1f, minimumZoom, maximumZoom);
                        Vector2 zoomedWorld = GetScreenToWorld2D(mousePos, camera);
                        camera.target.x += mouseWorld.x - zoomedWorld.x;
                        camera.target.y += mouseWorld.y - zoomedWorld.y;
                }
        }

        // Desktop Clicks
        HitTarget mouseTarget = hitTest(GetMousePosition(), appState, camera, tileSize, engine);

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                switch (mouseTarget.control) {
                case Control::MODE: return {UserAction::Type::TOGGLE_MODE};
                case Control::BEGINNER: return {UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, BEGINNER};
                case Control::INTERMEDIATE: return {UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, INTERMEDIATE};
                case Control::EXPERT: return {UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, EXPERT};
                case Control::EVIL: return {UserAction::Type::SELECT_DIFFICULTY, -1, activeTool, EVIL};
                case Control::DIG: return {UserAction::Type::SELECT_TOOL, -1, TouchTool::DIG};
                case Control::FLAG: return {UserAction::Type::SELECT_TOOL, -1, TouchTool::FLAG};
                case Control::FIT: return {UserAction::Type::CENTER_CAMERA};
                case Control::HINT: return {UserAction::Type::REQUEST_HINT};
                case Control::RESTART: return {UserAction::Type::RESTART_GAME};
                case Control::MENU: return {UserAction::Type::OPEN_MENU};
                case Control::CELL: return {UserAction::Type::ACT_ON_CELL, mouseTarget.cellIndex, activeTool};
                default: break;
                }
        }
        else if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && mouseTarget.control == Control::CELL) {
                return {UserAction::Type::ACT_ON_CELL, mouseTarget.cellIndex, TouchTool::FLAG};
        }

        return {};
}
