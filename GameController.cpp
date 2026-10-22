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
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#ifndef MINESWEEPER_MOBILE_CONTROLS
#if defined(__ANDROID__) || defined(PLATFORM_IOS)
#define MINESWEEPER_MOBILE_CONTROLS 1
#else
#define MINESWEEPER_MOBILE_CONTROLS 0
#endif
#endif

#if defined(__EMSCRIPTEN__)
EM_JS(char*, loadBrowserHighScores, (), {
        try {
                const value = localStorage.getItem("minesweeper.highscores.v1");
                if (value === null) return 0;
                const size = lengthBytesUTF8(value) + 1;
                const buffer = _malloc(size);
                if (buffer === 0) return 0;
                stringToUTF8(value, buffer, size);
                return buffer;
        } catch (error) {
                return 0;
        }
});

EM_JS(int, saveBrowserHighScores, (const char *text), {
        try {
                localStorage.setItem("minesweeper.highscores.v1", UTF8ToString(text));
                return 1;
        } catch (error) {
                return 0;
        }
});
#endif

namespace {
        constexpr float tapMovementTolerance = 8.0f;
        constexpr float minimumZoom = 0.2f;
        constexpr float maximumZoom = 4.0f;
        constexpr int noBestTime = -1;
        constexpr char highScoresFileName[] = "highscores.dat";
        constexpr char highScoresSignature[] = "MINESWEEPER_HIGHSCORES";
        constexpr int highScoresFormatVersion = 1;

#if defined(__EMSCRIPTEN__)
        constexpr bool mobileControlsEnabled = true;
#else
        constexpr bool mobileControlsEnabled = MINESWEEPER_MOBILE_CONTROLS != 0;
#endif

        using BestTimes = std::array<std::array<int, 4>, 2>;

        float getUiScale() {
                const float screenWidth = static_cast<float>(std::max(1, GetScreenWidth()));
                const float screenHeight = static_cast<float>(std::max(1, GetScreenHeight()));
                const Vector2 dpiScale = GetWindowScaleDPI();
                const float dpiX = std::max(0.1f, dpiScale.x);
                const float dpiY = std::max(0.1f, dpiScale.y);
                const float logicalScale = std::min(screenWidth / dpiX / 1280.0f,
                                                    screenHeight / dpiY / 720.0f);
                const float pixelScale = std::min(dpiX, dpiY);
                return std::clamp(logicalScale * pixelScale, 0.7f, 2.25f);
        }

        Vector2 toRenderCoordinates(Vector2 screenPosition) {
                const float screenWidth = static_cast<float>(std::max(1, GetScreenWidth()));
                const float screenHeight = static_cast<float>(std::max(1, GetScreenHeight()));
                const float scaleX = static_cast<float>(std::max(1, GetRenderWidth())) / screenWidth;
                const float scaleY = static_cast<float>(std::max(1, GetRenderHeight())) / screenHeight;
                return {screenPosition.x * scaleX, screenPosition.y * scaleY};
        }

        bool deserializeBestTimes(const std::string& serialized, BestTimes& bestTimes) {
                std::istringstream input(serialized);
                std::string signature;
                int version = 0;
                if (!(input >> signature >> version) || signature != highScoresSignature
                    || version != highScoresFormatVersion) {
                        return false;
                }

                BestTimes parsed{};
                for (auto& modeTimes : parsed) {
                        for (int& time : modeTimes) {
                                if (!(input >> time) || time < noBestTime) return false;
                        }
                }
                std::string trailingData;
                if (input >> trailingData) return false;
                bestTimes = parsed;
                return true;
        }

        template<typename SampleFunction>
        Sound makeProceduralSound(float durationSeconds, float volume, SampleFunction sampleAt) {
                constexpr unsigned int sampleRate = 22050;
                const auto sampleCount = static_cast<std::size_t>(durationSeconds * sampleRate);
                std::vector<std::int16_t> samples(sampleCount);
                for (std::size_t index = 0; index < sampleCount; ++index) {
                        const float time = static_cast<float>(index) / sampleRate;
                        const float sample = std::clamp(sampleAt(time), -1.0f, 1.0f);
                        samples[index] = static_cast<std::int16_t>(sample * 32767.0f);
                }

                Wave wave{};
                wave.frameCount = static_cast<unsigned int>(samples.size());
                wave.sampleRate = sampleRate;
                wave.sampleSize = 16;
                wave.channels = 1;
                wave.data = samples.data();
                Sound sound = LoadSoundFromWave(wave);
                if (IsSoundValid(sound)) SetSoundVolume(sound, volume);
                return sound;
        }

        void drawCenteredText(const char* text, Rectangle bounds, int fontSize, Color color) {
                const float padding = std::clamp(std::min(bounds.width, bounds.height) * 0.12f, 4.0f, 24.0f);
                while (fontSize > 10 && (MeasureText(text, fontSize) > bounds.width - padding * 2.0f
                                       || fontSize > bounds.height - padding)) {
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
                drawCenteredText(label, bounds, static_cast<int>(20.0f * getUiScale()), enabled ? RAYWHITE : GRAY);
        }

        bool isWithinTouchTolerance(Vector2 first, Vector2 second) {
                const float deltaX = first.x - second.x;
                const float deltaY = first.y - second.y;
                return deltaX * deltaX + deltaY * deltaY < tapMovementTolerance * tapMovementTolerance;
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

        camera.target = {0.0f, 0.0f};
        camera.offset = {0.0f, 0.0f};
        camera.rotation = 0.0f;
        camera.zoom = 1.0f;

        appState = AppState::MAIN_MENU;
        currentState = GameState::BEGIN;
        timeElapsed = 0;
        touchTool = TouchTool::DIG;
        initializeAudio();
        loadBestTimes();
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
        unloadAudio();
        CloseWindow();
        initialized = false;
}

void GameController::initializeAudio() {
        InitAudioDevice();
        audioDeviceReady = IsAudioDeviceReady();
        if (!audioDeviceReady) {
                TraceLog(LOG_WARNING, "Audio device is unavailable; sound effects are disabled");
                return;
        }

        constexpr float twoPi = 6.28318530718f;
        soundEffects[static_cast<std::size_t>(SoundEffect::DIG)] = makeProceduralSound(
                0.065f, 0.28f, [](float time) {
                        return 0.72f * std::sin(2.0f * 3.14159265359f * 1480.0f * time) * std::exp(-time * 90.0f);
                });
        soundEffects[static_cast<std::size_t>(SoundEffect::FLAG)] = makeProceduralSound(
                0.045f, 0.2f, [](float time) {
                        return 0.5f * std::sin(2.0f * 3.14159265359f * 880.0f * time) * std::exp(-time * 140.0f);
                });
        soundEffects[static_cast<std::size_t>(SoundEffect::CHORD)] = makeProceduralSound(
                0.15f, 0.23f, [twoPi](float time) {
                        if (time < 0.045f) return 0.52f * std::sin(twoPi * 720.0f * time) * std::exp(-time * 90.0f);
                        if (time >= 0.085f) {
                                const float secondClickTime = time - 0.085f;
                                return 0.52f * std::sin(twoPi * 980.0f * secondClickTime)
                                        * std::exp(-secondClickTime * 90.0f);
                        }
                        return 0.0f;
                });

        unsigned int noiseSeed = 0x9E3779B9U;
        soundEffects[static_cast<std::size_t>(SoundEffect::LOSS)] = makeProceduralSound(
                0.38f, 0.36f, [&noiseSeed](float time) {
                        noiseSeed = noiseSeed * 1664525U + 1013904223U;
                        const float noise = (static_cast<float>((noiseSeed >> 16U) & 0x7FFFU) / 16383.5f) - 1.0f;
                        const float envelope = std::exp(-time * 8.5f);
                        const float rumble = std::sin(2.0f * 3.14159265359f * (58.0f - 18.0f * time) * time);
                        return (0.62f * rumble + 0.38f * noise) * envelope;
                });
        soundEffects[static_cast<std::size_t>(SoundEffect::WIN)] = makeProceduralSound(
                0.72f, 0.25f, [twoPi](float time) {
                        constexpr float noteDuration = 0.17f;
                        constexpr float notes[] = {659.25f, 783.99f, 987.77f, 1318.51f};
                        const int note = std::min(3, static_cast<int>(time / noteDuration));
                        const float noteTime = time - note * noteDuration;
                        const float envelope = std::exp(-noteTime * 4.0f);
                        return 0.56f * std::sin(twoPi * notes[note] * noteTime) * envelope;
                });
}

void GameController::unloadAudio() {
        if (!audioDeviceReady) return;
        for (Sound sound : soundEffects) {
                if (IsSoundValid(sound)) UnloadSound(sound);
        }
        CloseAudioDevice();
        audioDeviceReady = false;
        soundEffects.fill(Sound{});
}

void GameController::playSoundEffect(SoundEffect effect) const {
        if (!soundEnabled || !audioDeviceReady) return;
        const Sound sound = soundEffects[static_cast<std::size_t>(effect)];
        if (IsSoundValid(sound)) PlaySound(sound);
}

void GameController::loadBestTimes() {
        for (auto& modeTimes : bestTimes) modeTimes.fill(noBestTime);

        std::string serialized;
#if defined(__EMSCRIPTEN__)
        if (char* browserData = loadBrowserHighScores()) {
                serialized = browserData;
                std::free(browserData);
        }
#else
        if (FileExists(highScoresFileName)) {
                if (char* fileData = LoadFileText(highScoresFileName)) {
                        serialized = fileData;
                        UnloadFileText(fileData);
                }
        }
#endif
        BestTimes loaded{};
        if (!serialized.empty() && deserializeBestTimes(serialized, loaded)) bestTimes = loaded;
}

void GameController::saveBestTimes() const {
        std::ostringstream output;
        output << highScoresSignature << ' ' << highScoresFormatVersion << '\n';
        for (const auto& modeTimes : bestTimes) {
                for (int time : modeTimes) output << time << ' ';
                output << '\n';
        }
        std::string serialized = output.str();
#if defined(__EMSCRIPTEN__)
        if (saveBrowserHighScores(serialized.c_str()) == 0) {
                TraceLog(LOG_WARNING, "Could not save high scores to browser storage");
        }
#else
        if (!SaveFileText(highScoresFileName, serialized.data())) {
                TraceLog(LOG_WARNING, "Could not save high scores to %s", highScoresFileName);
        }
#endif
}

void GameController::recordWin() {
        const int modeIndex = getSelectedModeIndex();
        const int difficultyIndex = getSelectedDifficultyIndex();
        int& bestTime = bestTimes[modeIndex][difficultyIndex];
        if (bestTime == noBestTime || timeElapsed < bestTime) {
                bestTime = timeElapsed;
                saveBestTimes();
        }
}

int GameController::getSelectedDifficultyIndex() const {
        if (!engine) return 0;
        constexpr DifficultyConfig difficulties[] = {BEGINNER, INTERMEDIATE, EXPERT, EVIL};
        for (int index = 0; index < 4; ++index) {
                if (engine->getWidth() == difficulties[index].width && engine->getHeight() == difficulties[index].height
                    && engine->total_mines() == difficulties[index].mineCount) {
                        return index;
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
        const float uiScale = getUiScale();
        const float inset = std::clamp(screenWidth * 0.025f, 10.0f * uiScale, 32.0f * uiScale);
        const float spacing = std::clamp(screenHeight * 0.012f, 6.0f * uiScale, 18.0f * uiScale);
        float titleHeight = std::clamp(std::min(screenWidth * 0.09f, 48.0f * uiScale),
                                       24.0f * uiScale, 64.0f * uiScale);
        float buttonHeight = std::clamp((screenHeight - titleHeight - 12.0f * spacing) / 5.0f,
                                        30.0f * uiScale, 58.0f * uiScale);
        float buttonWidth = std::max(1.0f, std::min(460.0f * uiScale, screenWidth - inset * 2.0f));
        float menuHeight = titleHeight + 6.0f * spacing + buttonHeight * 5.0f;
        float menuTop = std::max(inset, (screenHeight - menuHeight) * 0.5f);
        layout.title = {0.0f, menuTop, screenWidth, titleHeight + 8.0f};
        layout.mode = {(screenWidth - buttonWidth) * 0.5f, menuTop + titleHeight + spacing,
                       buttonWidth, buttonHeight};
        for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                layout.difficulties[index] = {layout.mode.x, layout.mode.y + (index + 1) * (buttonHeight + spacing),
                                              buttonWidth, buttonHeight};
        }

        const bool gameFinished = currentState == GameState::WON || currentState == GameState::LOST;
        float headerHeight = (gameFinished ? 108.0f : 64.0f) * uiScale;
        layout.header = {0.0f, 0.0f, screenWidth, headerHeight};
        layout.toolbarTop = screenHeight;
        if (mobileControlsEnabled) {
                const float controlHeight = std::clamp(screenHeight * 0.075f, 40.0f * uiScale, 64.0f * uiScale);
                const float controlGap = 8.0f * uiScale;
                const float toolbarPadding = 12.0f * uiScale;
                int columns = screenWidth < 440.0f * uiScale ? 3 : 5;
                int rows = columns == 3 ? 2 : 1;
                float controlWidth = std::max(1.0f, (screenWidth - toolbarPadding * 2.0f
                                                       - (columns - 1) * controlGap) / columns);
                layout.toolbarTop = std::max(headerHeight, screenHeight - (rows * controlHeight
                        + (rows - 1) * controlGap + toolbarPadding * 2.0f));
                for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                        int row = index / columns;
                        int column = index % columns;
                        int rowControls = std::min(columns, static_cast<int>(layout.toolbar.size()) - row * columns);
                        float rowWidth = rowControls * controlWidth + (rowControls - 1) * controlGap;
                        layout.toolbar[index] = {(screenWidth - rowWidth) * 0.5f + column * (controlWidth + controlGap),
                                                 layout.toolbarTop + toolbarPadding + row * (controlHeight + controlGap),
                                                 controlWidth, controlHeight};
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
        if (mobileControlsEnabled) {
                constexpr Control controls[] = {Control::DIG, Control::FLAG, Control::FIT, Control::RESTART, Control::MENU};
                for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                        if (CheckCollisionPointRec(position, layout.toolbar[index])) return {controls[index]};
                }
        }
        if (!CheckCollisionPointRec(position, layout.board)) return {};
        Vector2 worldPosition = GetScreenToWorld2D(toRenderCoordinates(position), camera);
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
                if (currentState == GameState::RUNNING && !engine->getCell(index).isRevealed) {
                        const bool wasFlagged = engine->getCell(index).isFlagged;
                        engine->toggleFlag(index);
                        if (engine->getCell(index).isFlagged != wasFlagged) playSoundEffect(SoundEffect::FLAG);
                }
                return;
        }
        if (currentState == GameState::BEGIN) {
                engine->generateBoard(index % engine->getWidth(), index / engine->getWidth());
                currentState = GameState::RUNNING;
                startTime = GetTime();
        }
        if (engine->getCell(index).isRevealed) {
                const Cell& clue = engine->getCell(index);
                const bool chordReady = clue.neighboringMines > 0
                        && engine->countNeighboringFlags(index) == clue.neighboringMines;
                bool hasHiddenUnflaggedNeighbor = false;
                if (chordReady) {
                        for (const Cell* neighbor : clue.neighbors) {
                                if (!neighbor->isRevealed && !neighbor->isFlagged) {
                                        hasHiddenUnflaggedNeighbor = true;
                                        break;
                                }
                        }
                }
                engine->chordCell(index);
                if (hasHiddenUnflaggedNeighbor) playSoundEffect(SoundEffect::CHORD);
        }
        else if (engine->revealCell(index)) {
                playSoundEffect(SoundEffect::DIG);
        }
}

void GameController::renderMainMenu() const {
        BeginDrawing();
        ClearBackground(DARKGRAY);
        UILayout layout = getLayout();
        const float uiScale = getUiScale();
        drawCenteredText("MINESWEEPER", layout.title, static_cast<int>(layout.title.height * 0.8f), RAYWHITE);
        drawButton(selectedMode == GameMode::CLASSIC ? "Mode: Classic" : "Mode: NoGuess", layout.mode, true);
        const char* labels[] = {"Beginner (9x9)", "Intermediate (16x16)", "Expert (30x16)", "Evil (30x20)"};
        for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                const Rectangle bounds = layout.difficulties[index];
                const bool selected = index == getSelectedDifficultyIndex();
                Color background = selected ? BLUE : GRAY;
                if (CheckCollisionPointRec(GetMousePosition(), bounds)) background = selected ? DARKBLUE : LIGHTGRAY;
                DrawRectangleRec(bounds, background);
                const float lineHeight = bounds.height * 0.5f;
                drawCenteredText(labels[index], {bounds.x, bounds.y, bounds.width, lineHeight},
                                 static_cast<int>(17.0f * uiScale), RAYWHITE);
                const int bestTime = bestTimes[getSelectedModeIndex()][index];
                const char* recordText = bestTime == noBestTime ? "Best: --"
                        : TextFormat("Best: %02d:%02d", bestTime / 60, bestTime % 60);
                drawCenteredText(recordText, {bounds.x, bounds.y + lineHeight, bounds.width, lineHeight},
                                 static_cast<int>(14.0f * uiScale), GetColor(0xE5E5E5FF));
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
        const float uiScale = getUiScale();
        const bool gameFinished = currentState == GameState::WON || currentState == GameState::LOST;
        const float metricHeight = gameFinished ? layout.header.height / 3.0f : layout.header.height;
        DrawRectangleRec(layout.header, GetColor(0x333333FF));
        drawCenteredText(TextFormat("MINES: %03d", engine->total_mines() - engine->total_flags()),
                         {0.0f, 0.0f, layout.header.width * 0.5f, metricHeight},
                         static_cast<int>(20.0f * uiScale), RED);
        drawCenteredText(TextFormat("TIME: %03d", timeElapsed),
                         {layout.header.width * 0.5f, 0.0f, layout.header.width * 0.5f, metricHeight},
                         static_cast<int>(20.0f * uiScale), RED);
        if (gameFinished) {
                const char* message = currentState == GameState::WON ? "YOU WIN!" : "GAME OVER";
                if (!mobileControlsEnabled) message = TextFormat("%s  (R: restart / Esc: menu)", message);
                drawCenteredText(message, {0.0f, metricHeight, layout.header.width, metricHeight},
                                 static_cast<int>(20.0f * uiScale),
                                 currentState == GameState::WON ? GREEN : RED);
                if (currentState == GameState::WON) {
                        const int bestTime = getBestTimeForSelection();
                        const char* recordText = bestTime == noBestTime ? "BEST: --"
                                : TextFormat("BEST: %02d:%02d", bestTime / 60, bestTime % 60);
                        drawCenteredText(recordText, {0.0f, metricHeight * 2.0f, layout.header.width, metricHeight},
                                         static_cast<int>(17.0f * uiScale), GOLD);
                }
        }

        Vector2 pointerPosition = GetMousePosition();
        const int touchCount = GetTouchPointCount();
        if (touchCount == 1) pointerPosition = GetTouchPosition(0);
        const HitTarget pointerTarget = hitTest(pointerPosition);
        const int hoveredCell = pointerTarget.control == Control::CELL ? pointerTarget.cellIndex : -1;
        std::vector<int> chordPreviewCells;
        if (!gameFinished && currentState == GameState::RUNNING && touchTool == TouchTool::DIG && hoveredCell >= 0) {
                const Cell& clue = engine->getCell(hoveredCell);
                if (clue.isRevealed && clue.neighboringMines > 0
                    && engine->countNeighboringFlags(hoveredCell) == clue.neighboringMines) {
                        for (const Cell* neighbor : clue.neighbors) {
                                if (!neighbor->isRevealed && !neighbor->isFlagged) {
                                        chordPreviewCells.push_back(static_cast<int>(neighbor - engine->getGrid().data()));
                                }
                        }
                }
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
                        Color tileColor = LIGHTGRAY;
                        if (std::find(chordPreviewCells.begin(), chordPreviewCells.end(), idx) != chordPreviewCells.end()) {
                                tileColor = Color{188, 229, 198, 255};
                        }
                        else if (idx == hoveredCell) {
                                tileColor = Color{238, 238, 238, 255};
                        }
                        DrawRectangleRec(rect, tileColor);
                        if (cell.isFlagged) {
                                drawFlagIcon(x, y, tileSize);
                        }
                }
                else {
                        if (cell.isMine) {
                                DrawRectangleRec(rect, currentState == GameState::LOST ? RED : RAYWHITE);
                                drawMineIcon(x, y, tileSize);
                        }
                        else {
                                DrawRectangleRec(rect, idx == hoveredCell && !chordPreviewCells.empty()
                                        ? Color{230, 245, 232, 255} : RAYWHITE);
                                if (cell.neighboringMines > 0) {
                                        const char* clueText = TextFormat("%d", cell.neighboringMines);
                                        const int fontSize = std::max(10, static_cast<int>(tileSize * 0.62f));
                                        const int textWidth = MeasureText(clueText, fontSize);
                                        DrawText(clueText, x + (tileSize - textWidth) / 2,
                                                 y + (tileSize - fontSize) / 2, fontSize,
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
                Vector2 mousePosition = toRenderCoordinates(GetMousePosition());
                Vector2 mouseWorldPos = GetScreenToWorld2D(mousePosition, camera);
                camera.zoom = std::clamp(camera.zoom + wheel * 0.1f, minimumZoom, maximumZoom);
                Vector2 zoomedWorldPos = GetScreenToWorld2D(mousePosition, camera);
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
                playSoundEffect(SoundEffect::LOSS);
        }
        else if (engine->isGameWon()) {
                currentState = GameState::WON;
                engine->flagAllMines();
                recordWin();
                playSoundEffect(SoundEffect::WIN);
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
        const float renderScaleX = static_cast<float>(std::max(1, GetRenderWidth()))
                / static_cast<float>(std::max(1, GetScreenWidth()));
        const float renderScaleY = static_cast<float>(std::max(1, GetRenderHeight()))
                / static_cast<float>(std::max(1, GetScreenHeight()));
        float scaleX = std::max(1.0f, (layout.board.width - 40.0f) * renderScaleX) / boardPixelWidth;
        float scaleY = std::max(1.0f, (layout.board.height - 40.0f) * renderScaleY) / boardPixelHeight;

        camera.zoom = std::clamp(std::min({scaleX, scaleY, 2.0f}), minimumZoom, maximumZoom);

        camera.target = {boardPixelWidth / 2.0f, boardPixelHeight / 2.0f};
        camera.offset = {(layout.board.x + layout.board.width * 0.5f) * renderScaleX,
                         (layout.board.y + layout.board.height * 0.5f) * renderScaleY};
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
        if (touchBlocked) touchLastPos = GetTouchPosition(0);
        touchMouseSuppressionFrames = 0;
        suppressMouseUntilRelease = false;
        pinchTouchIds = {-1, -1};
        pinchDistance = 0.0f;
        pinchActive = false;
}

bool GameController::handleTouchGestures() {
        int touchCount = GetTouchPointCount();
        if (touchCount == 0) {
                const bool hadTouch = primaryTouchId != -1 || touchBlocked || pinchActive;
                const bool activate = touchPending && !touchBlocked
                        && isWithinTouchTolerance(touchStartPos, touchLastPos)
                        && hitTest(touchLastPos) == touchStartTarget;
                HitTarget target = touchStartTarget;
                const Vector2 releasePosition = touchLastPos;
                primaryTouchId = -1;
                touchPending = false;
                touchBlocked = false;
                pinchActive = false;
                pinchTouchIds = {-1, -1};
                pinchDistance = 0.0f;
                if (activate) activateTarget(target);
                if (hadTouch) {
                        suppressedTouchReleasePosition = releasePosition;
                        touchMouseSuppressionFrames = 2;
                        suppressMouseUntilRelease = false;
                }
                if (hadTouch) return true;

                if (touchMouseSuppressionFrames > 0) {
                        const bool mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
                        const bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
                        const bool matchingPosition = isWithinTouchTolerance(GetMousePosition(),
                                                                              suppressedTouchReleasePosition);
                        if (matchingPosition && (mousePressed || mouseDown)) {
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
                        Vector2 worldAnchor = GetScreenToWorld2D(toRenderCoordinates(pinchMidpoint), camera);
                        if (pinchDistance > 1.0f && distance > 1.0f) {
                                camera.zoom = std::clamp(camera.zoom * distance / pinchDistance, minimumZoom, maximumZoom);
                        }
                        Vector2 movedWorldAnchor = GetScreenToWorld2D(toRenderCoordinates(midpoint), camera);
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
                if (!isWithinTouchTolerance(touchStartPos, position)) touchPending = false;
                touchLastPos = position;
        }
        return true;
}

void GameController::renderMobileControls() const {
        if (!mobileControlsEnabled) return;
        UILayout layout = getLayout();
        const float uiScale = getUiScale();
        DrawRectangleRec({0.0f, layout.toolbarTop, layout.header.width,
                          static_cast<float>(GetScreenHeight()) - layout.toolbarTop}, GetColor(0x333333FF));
        const char* labels[] = {"Dig", "Flag", "Fit", "Restart", "Menu"};
        for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                bool selected = (index == 0 && touchTool == TouchTool::DIG) || (index == 1 && touchTool == TouchTool::FLAG);
                bool enabled = index != 1 || currentState == GameState::RUNNING;
                const Rectangle bounds = layout.toolbar[index];
                Color background = selected ? BLUE : GRAY;
                if (!enabled) background = GetColor(0x383838FF);
                else if (CheckCollisionPointRec(GetMousePosition(), bounds)) background = selected ? DARKBLUE : LIGHTGRAY;
                DrawRectangleRec(bounds, background);
                drawCenteredText(labels[index], bounds, static_cast<int>(18.0f * uiScale), enabled ? RAYWHITE : GRAY);
        }
}
