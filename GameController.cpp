#include "GameController.h"
#include "ClassicMinesweeper.h"
#include "NoGuessMinesweeper.h"
#include "Solver.h"
#include "WindowUtils.h"

#include <algorithm>
#include <vector>

GameController::GameController() {
        camera.target = {0.0f, 0.0f};
        camera.offset = {0.0f, 0.0f};
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;
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
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
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

        audio.init();
        bestTimes = HighScoreStorage::load();
        initialized = true;
        reset(BEGINNER);
        appState = AppState::MAIN_MENU;
}

void GameController::frame() {
        if (!initialized) return;

        if (IsWindowResized()) {
                input.reset();
                centerCamera();
        }

        if (IsWindowFocused()) {
                const UserAction action = input.processInput(appState, currentState, touchTool,
                                                             camera, tileSize, engine.get());
                handleAction(action);
                update();
        }
        else {
                if (currentState == GameState::RUNNING) {
                        lastTimerUpdateTime = GetTime();
                }
                input.reset();
        }

        render();
}

void GameController::shutdown() {
        if (!initialized) return;
        input.reset();
        engine.reset();
        audio.shutdown();
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

void GameController::handleAction(const UserAction& action) {
        switch (action.type) {
        case UserAction::Type::TOGGLE_MODE:
                selectedMode = (selectedMode == GameMode::CLASSIC) ? GameMode::NO_GUESS : GameMode::CLASSIC;
                break;
        case UserAction::Type::SELECT_DIFFICULTY:
                reset(action.difficulty);
                break;
        case UserAction::Type::SELECT_TOOL:
                if (action.tool == TouchTool::FLAG && currentState != GameState::RUNNING) break;
                touchTool = action.tool;
                break;
        case UserAction::Type::CENTER_CAMERA:
                centerCamera();
                break;
        case UserAction::Type::REQUEST_HINT:
                requestHint();
                break;
        case UserAction::Type::RESTART_GAME:
                reset({engine->getWidth(), engine->getHeight(), engine->total_mines()});
                break;
        case UserAction::Type::OPEN_MENU:
                appState = AppState::MAIN_MENU;
                input.reset();
                break;
        case UserAction::Type::ACT_ON_CELL:
                actOnCell(action.cellIndex, action.tool);
                break;
        case UserAction::Type::NONE:
                break;
        }
}

void GameController::actOnCell(int index, TouchTool tool) {
        if (currentState == GameState::WON || currentState == GameState::LOST) return;

        const auto countRevealedCells = [this]() {
                return static_cast<int>(std::count_if(engine->getGrid().begin(), engine->getGrid().end(),
                                                      [](const Cell& cell) { return cell.isRevealed; }));
        };
        const int revealedBefore = countRevealedCells();

        if (index == hintedCellIndex) {
                hintedCellIndex = -1;
                hintType = HintType::NONE;
        }

        if (tool == TouchTool::FLAG) {
                if (currentState == GameState::RUNNING && !engine->getCell(index).isRevealed) {
                        const bool wasFlagged = engine->getCell(index).isFlagged;
                        engine->toggleFlag(index);
                        if (engine->getCell(index).isFlagged != wasFlagged) audio.play(SoundEffect::FLAG);
                }
                return;
        }

        if (currentState == GameState::BEGIN) {
                engine->generateBoard(index % engine->getWidth(), index / engine->getWidth());
                if (engine->revealCell(index)) {
                        currentState = GameState::RUNNING;
                        elapsedTimeSeconds = 0.0;
                        timeElapsed = 0;
                        lastTimerUpdateTime = GetTime();
                        audio.play(SoundEffect::DIG);
                }
                if (countRevealedCells() > revealedBefore) {
                        hintedCellIndex = -1;
                        hintType = HintType::NONE;
                }
                return;
        }

        if (engine->getCell(index).isRevealed) {
                const Cell& clue = engine->getCell(index);
                const bool chordReady = clue.neighboringMines > 0
                        && engine->countNeighboringFlags(index) == clue.neighboringMines;
                bool hasHiddenUnflagged = false;
                if (chordReady) {
                        for (const Cell* neighbor : clue.neighbors) {
                                if (!neighbor->isRevealed && !neighbor->isFlagged) {
                                        hasHiddenUnflagged = true;
                                        break;
                                }
                        }
                }
                engine->chordCell(index);
                if (hasHiddenUnflagged) audio.play(SoundEffect::CHORD);
        }
        else if (engine->revealCell(index)) {
                audio.play(SoundEffect::DIG);
        }

        if (countRevealedCells() > revealedBefore) {
                hintedCellIndex = -1;
                hintType = HintType::NONE;
        }
}

void GameController::requestHint() {
        if (!engine || appState != AppState::PLAYING || currentState != GameState::RUNNING
                || engine->isGameWon() || engine->isGameLost()) {
                return;
        }

        const HintResult hint = Solver::calculateHint(*engine);
        hintedCellIndex = hint.cellIndex;
        hintType = hint.type;
}

void GameController::update() {
        if (currentState != GameState::RUNNING || appState != AppState::PLAYING) {
                return;
        }

        const double currentTime = GetTime();
        elapsedTimeSeconds += std::max(0.0, currentTime - lastTimerUpdateTime);
        lastTimerUpdateTime = currentTime;
        timeElapsed = static_cast<int>(elapsedTimeSeconds);

        if (engine->isGameLost()) {
                currentState = GameState::LOST;
                explodedMineIndex = -1;
                const auto& cells = engine->getGrid();
                for (int i = 0; i < static_cast<int>(cells.size()); ++i) {
                        if (cells[i].isMine && cells[i].isRevealed) {
                                explodedMineIndex = i;
                                break;
                        }
                }
                engine->revealAllMines();
                audio.play(SoundEffect::LOSS);
        }
        else if (engine->isGameWon()) {
                currentState = GameState::WON;
                engine->flagAllMines();
                recordWin();
                audio.play(SoundEffect::WIN);
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

void GameController::renderMainMenu() const {
        renderer.renderMainMenu(selectedMode, getSelectedDifficultyIndex(), bestTimes);
}

void GameController::renderGame() const {
        if (!engine) return;

        Vector2 pointerPosition = GetMousePosition();
        const int touchCount = GetTouchPointCount();
        if (touchCount == 1) pointerPosition = GetTouchPosition(0);
        const auto pointerTarget = input.hitTest(pointerPosition, appState, camera, tileSize, engine.get());

        GameRenderState state;
        state.currentState = currentState;
        state.timeElapsed = timeElapsed;
        state.explodedMineIndex = explodedMineIndex;
        state.hintedCellIndex = hintedCellIndex;
        state.hintType = hintType;
        state.touchTool = touchTool;
        state.bestTime = getBestTimeForSelection();
        state.hoveredCell = (pointerTarget.control == InputManager::Control::CELL) ? pointerTarget.cellIndex : -1;
        state.isRestartPressed = (pointerTarget.control == InputManager::Control::RESTART)
                && (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || touchCount == 1);

        if (currentState == GameState::RUNNING && touchTool == TouchTool::DIG && state.hoveredCell >= 0) {
                const Cell& clue = engine->getCell(state.hoveredCell);
                if (clue.isRevealed && clue.neighboringMines > 0
                        && engine->countNeighboringFlags(state.hoveredCell) == clue.neighboringMines) {
                        for (const Cell* neighbor : clue.neighbors) {
                                if (!neighbor->isRevealed && !neighbor->isFlagged) {
                                        state.chordPreviewCells.push_back(
                                                static_cast<int>(neighbor - engine->getGrid().data()));
                                }
                        }
                }
        }

        renderer.renderGame(*engine, camera, tileSize, state);
}

void GameController::reset(DifficultyConfig config) {
        if (selectedMode == GameMode::CLASSIC) {
                engine = std::make_unique<ClassicMinesweeper>();
        }
        else {
                engine = std::make_unique<NoGuessMinesweeper>();
        }
        engine->loadBoard(config.width, config.height, config.mineCount);

        elapsedTimeSeconds = 0.0;
        lastTimerUpdateTime = GetTime();
        timeElapsed = 0;
        explodedMineIndex = -1;
        hintedCellIndex = -1;
        hintType = HintType::NONE;
        currentState = GameState::BEGIN;
        appState = AppState::PLAYING;
        touchTool = TouchTool::DIG;
        input.reset();
        centerCamera();
}

void GameController::centerCamera() {
        if (!engine) return;
        const float boardWidth = static_cast<float>(engine->getWidth() * tileSize);
        const float boardHeight = static_cast<float>(engine->getHeight() * tileSize);

        const UILayout layout = GameRenderer::getLayout();
        const auto [screenWidth, screenHeight] = getScreenDimensions();
        const auto [renderWidth, renderHeight] = getRenderDimensions();
        const float renderScaleX = renderWidth / screenWidth;
        const float renderScaleY = renderHeight / screenHeight;

        const float scaleX = std::max(1.0f, (layout.board.width - 40.0f) * renderScaleX) / boardWidth;
        const float scaleY = std::max(1.0f, (layout.board.height - 40.0f) * renderScaleY) / boardHeight;

        camera.zoom = std::clamp(std::min({scaleX, scaleY, 2.0f}), 0.2f, 4.0f);
        camera.target = {boardWidth * 0.5f, boardHeight * 0.5f};
        camera.offset = {
                (layout.board.x + layout.board.width * 0.5f) * renderScaleX,
                (layout.board.y + layout.board.height * 0.5f) * renderScaleY
        };
}

void GameController::recordWin() {
        int& bestTime = bestTimes[getSelectedModeIndex()][getSelectedDifficultyIndex()];
        if (bestTime == HighScoreStorage::noBestTime || timeElapsed < bestTime) {
                bestTime = timeElapsed;
                HighScoreStorage::save(bestTimes);
        }
}

int GameController::getSelectedDifficultyIndex() const {
        if (!engine) return 0;
        constexpr DifficultyConfig diffs[] = {BEGINNER, INTERMEDIATE, EXPERT, EVIL};
        for (int i = 0; i < 4; ++i) {
                if (engine->getWidth() == diffs[i].width && engine->getHeight() == diffs[i].height
                        && engine->total_mines() == diffs[i].mineCount) {
                        return i;
                }
        }
        return 0;
}

int GameController::getSelectedModeIndex() const {
        return selectedMode == GameMode::CLASSIC ? 0 : 1;
}

int GameController::getBestTimeForSelection() const {
        return bestTimes[getSelectedModeIndex()][getSelectedDifficultyIndex()];
}
