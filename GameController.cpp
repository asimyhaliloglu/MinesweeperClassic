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

        constexpr Color classicCanvas = {192, 192, 192, 255};
        constexpr Color classicSurface = {194, 194, 194, 255};
        constexpr Color classicHighlight = {255, 255, 255, 255};
        constexpr Color classicShadow = {112, 112, 112, 255};
        constexpr Color classicOutline = {56, 56, 56, 255};

        void drawPixelBlock(float x, float y, float pixelSize, int column, int row,
                            int width, int height, Color color) {
                DrawRectangleRec({x + column * pixelSize, y + row * pixelSize,
                                  width * pixelSize, height * pixelSize}, color);
        }

        void drawBeveledSurface(Rectangle bounds, Color face, float thickness, bool raised) {
                if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
                const float bevel = std::min(std::max(0.5f, thickness),
                                             std::min(bounds.width, bounds.height) * 0.24f);
                const Color topLeft = raised ? classicHighlight : classicShadow;
                const Color bottomRight = raised ? classicShadow : classicHighlight;
                DrawRectangleRec(bounds, face);
                DrawRectangleLinesEx(bounds, std::max(0.5f, bevel * 0.35f), classicOutline);
                DrawRectangleRec({bounds.x + bevel, bounds.y + bevel,
                                  std::max(0.0f, bounds.width - bevel * 2.0f), bevel}, topLeft);
                DrawRectangleRec({bounds.x + bevel, bounds.y + bounds.height - bevel * 2.0f,
                                  std::max(0.0f, bounds.width - bevel * 2.0f), bevel}, bottomRight);
                DrawRectangleRec({bounds.x + bevel, bounds.y + bevel, bevel,
                                  std::max(0.0f, bounds.height - bevel * 2.0f)}, topLeft);
                DrawRectangleRec({bounds.x + bounds.width - bevel * 2.0f, bounds.y + bevel, bevel,
                                  std::max(0.0f, bounds.height - bevel * 2.0f)}, bottomRight);
        }

        void drawBoardTile(Rectangle bounds, Color face, bool raised) {
                DrawRectangleRec(bounds, face);
                if (!raised) {
                        DrawRectangleLinesEx(bounds, 1.0f, Color{139, 148, 158, 255});
                        return;
                }

                constexpr float bevel = 2.0f;
                constexpr Color highlight = {243, 244, 246, 255};
                constexpr Color shadow = {139, 148, 158, 255};
                const float horizontalInterior = std::max(0.0f, bounds.width - bevel);
                const float verticalInterior = std::max(0.0f, bounds.height - bevel);
                DrawRectangleRec({bounds.x, bounds.y, horizontalInterior, bevel}, highlight);
                DrawRectangleRec({bounds.x, bounds.y + bevel, bevel, verticalInterior}, highlight);
                DrawRectangleRec({bounds.x + bevel, bounds.y + bounds.height - bevel,
                                  horizontalInterior, bevel}, shadow);
                DrawRectangleRec({bounds.x + bounds.width - bevel, bounds.y + bevel,
                                  bevel, verticalInterior}, shadow);
        }

        void drawSevenSegmentDigit(Rectangle bounds, int digit, Color activeColor) {
                constexpr int digitSegments[] = {
                        0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
                        0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111
                };
                const int activeSegments = digit < 0 ? 0b01000000 : digitSegments[digit];
                const float pixelSize = std::max(1.0f, std::round(bounds.height / 12.0f));
                const float thickness = std::max(pixelSize * 2.0f,
                        std::round(std::min(bounds.width, bounds.height) * 0.22f / pixelSize) * pixelSize);
                const float halfHeight = bounds.height * 0.5f;
                const std::array<Rectangle, 7> segments = {{
                        {bounds.x + thickness * 0.5f, bounds.y, bounds.width - thickness, thickness},
                        {bounds.x + bounds.width - thickness, bounds.y + thickness * 0.5f,
                         thickness, halfHeight - thickness * 0.5f},
                        {bounds.x + bounds.width - thickness, bounds.y + halfHeight,
                         thickness, halfHeight - thickness * 0.5f},
                        {bounds.x + thickness * 0.5f, bounds.y + bounds.height - thickness,
                         bounds.width - thickness, thickness},
                        {bounds.x, bounds.y + halfHeight, thickness, halfHeight - thickness * 0.5f},
                        {bounds.x, bounds.y + thickness * 0.5f, thickness, halfHeight - thickness * 0.5f},
                        {bounds.x + thickness * 0.5f, bounds.y + halfHeight - thickness * 0.5f,
                         bounds.width - thickness, thickness}
                }};
                constexpr Color inactiveColor = {38, 0, 0, 255};
                for (int index = 0; index < static_cast<int>(segments.size()); ++index) {
                        const Color color = (activeSegments & (1 << index)) != 0 ? activeColor : inactiveColor;
                        const Rectangle segment = segments[index];
                        const float left = std::round((segment.x - bounds.x) / pixelSize) * pixelSize + bounds.x;
                        const float top = std::round((segment.y - bounds.y) / pixelSize) * pixelSize + bounds.y;
                        const float right = std::round((segment.x + segment.width - bounds.x) / pixelSize)
                                * pixelSize + bounds.x;
                        const float bottom = std::round((segment.y + segment.height - bounds.y) / pixelSize)
                                * pixelSize + bounds.y;
                        DrawRectangleRec({left, top, std::max(pixelSize, right - left),
                                          std::max(pixelSize, bottom - top)}, color);
                }
        }

        void drawLedCounter(Rectangle bounds, int value) {
                drawBeveledSurface(bounds, BLACK, bounds.height * 0.09f, false);
                const int displayValue = std::clamp(value, -99, 999);
                const char* display = TextFormat("%03d", displayValue);
                const float digitHeight = bounds.height * 0.72f;
                const float digitWidth = std::min(digitHeight * 0.52f, bounds.width * 0.25f);
                const float gap = (bounds.width - digitWidth * 3.0f) / 4.0f;
                const float digitY = bounds.y + (bounds.height - digitHeight) * 0.5f;
                for (int index = 0; index < 3; ++index) {
                        const int digit = display[index] == '-' ? -1 : display[index] - '0';
                        drawSevenSegmentDigit({bounds.x + gap + index * (digitWidth + gap), digitY,
                                               digitWidth, digitHeight}, digit, Color{255, 0, 0, 255});
                }
        }

        void drawFaceButton(Rectangle bounds, GameState state, bool pressed, bool shocked) {
                drawBeveledSurface(bounds, classicSurface, bounds.width * 0.09f, !pressed);
                const float radius = std::min(bounds.width, bounds.height) * 0.31f;
                const float pixelSize = radius * 2.0f / 16.0f;
                const float faceX = bounds.x + (bounds.width - pixelSize * 16.0f) * 0.5f;
                const float faceY = bounds.y + (bounds.height - pixelSize * 16.0f) * 0.5f;
                constexpr int faceStarts[] = {6, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6};
                constexpr int faceWidths[] = {4, 8, 10, 12, 14, 16, 16, 16, 16, 16, 16, 14, 12, 10, 8, 4};
                for (int row = 0; row < 16; ++row) {
                        drawPixelBlock(faceX, faceY, pixelSize, faceStarts[row], row,
                                       faceWidths[row], 1, BLACK);
                        if (faceWidths[row] > 2) {
                                drawPixelBlock(faceX, faceY, pixelSize, faceStarts[row] + 1, row,
                                               faceWidths[row] - 2, 1, Color{255, 255, 0, 255});
                        }
                }

                if (state == GameState::WON) {
                        drawPixelBlock(faceX, faceY, pixelSize, 3, 5, 4, 2, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 9, 5, 4, 2, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 7, 5, 2, 1, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 4, 9, 1, 1, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 11, 9, 1, 1, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 5, 10, 2, 1, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 9, 10, 2, 1, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 7, 11, 2, 1, BLACK);
                }
                else {
                        if (state == GameState::LOST) {
                                drawPixelBlock(faceX, faceY, pixelSize, 4, 5, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 6, 7, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 5, 6, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 6, 5, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 4, 7, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 9, 5, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 11, 7, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 10, 6, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 11, 5, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 9, 7, 1, 1, BLACK);
                        }
                        else {
                                drawPixelBlock(faceX, faceY, pixelSize, 5, 5, 1, 2, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 10, 5, 1, 2, BLACK);
                        }

                        if (shocked) {
                                drawPixelBlock(faceX, faceY, pixelSize, 7, 9, 2, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 6, 10, 1, 2, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 9, 10, 1, 2, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 7, 12, 2, 1, BLACK);
                        }
                        else if (state == GameState::LOST) {
                                drawPixelBlock(faceX, faceY, pixelSize, 7, 9, 2, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 5, 10, 2, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 9, 10, 2, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 4, 11, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 11, 11, 1, 1, BLACK);
                        }
                        else {
                                drawPixelBlock(faceX, faceY, pixelSize, 4, 9, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 11, 9, 1, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 5, 10, 2, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 9, 10, 2, 1, BLACK);
                                drawPixelBlock(faceX, faceY, pixelSize, 7, 11, 2, 1, BLACK);
                        }
                }
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
                Color background = selected ? Color{211, 229, 237, 255} : classicSurface;
                if (!enabled) background = Color{160, 160, 160, 255};
                else if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
                        background = selected ? Color{224, 238, 243, 255} : Color{211, 211, 211, 255};
                }
                drawBeveledSurface(bounds, background, 3.0f * getUiScale(), true);
                if (selected) {
                        DrawRectangleLinesEx({bounds.x + 4.0f, bounds.y + 4.0f,
                                              bounds.width - 8.0f, bounds.height - 8.0f},
                                             1.0f * getUiScale(), Color{75, 111, 125, 255});
                }
                drawCenteredText(label, bounds, static_cast<int>(20.0f * getUiScale()),
                                 enabled ? Color{30, 35, 39, 255} : Color{96, 96, 96, 255});
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

        const float headerHeight = 104.0f * uiScale;
        layout.header = {0.0f, 0.0f, screenWidth, headerHeight};
        const float sideMargin = 8.0f * uiScale;
        const float faceSize = std::min(44.0f * uiScale,
                                        std::max(32.0f * uiScale, screenWidth * 0.14f));
        const float counterWidth = std::min(92.0f * uiScale,
                std::max(54.0f * uiScale, (screenWidth - faceSize - sideMargin * 4.0f) * 0.5f));
        const float counterY = 6.0f * uiScale;
        const float counterHeight = 36.0f * uiScale;
        layout.mineCounter = {sideMargin, counterY, counterWidth, counterHeight};
        layout.restart = {(screenWidth - faceSize) * 0.5f, 2.0f * uiScale, faceSize, faceSize};
        layout.timeCounter = {screenWidth - sideMargin - counterWidth, counterY, counterWidth, counterHeight};
        layout.status = {0.0f, 58.0f * uiScale, screenWidth,
                         std::max(0.0f, headerHeight - 58.0f * uiScale)};
        layout.toolbarTop = screenHeight;
        if (mobileControlsEnabled) {
                const float controlHeight = std::clamp(screenHeight * 0.075f, 40.0f * uiScale, 64.0f * uiScale);
                const float controlGap = 8.0f * uiScale;
                const float toolbarPadding = 12.0f * uiScale;
                const int columns = screenWidth < 440.0f * uiScale ? 2 : 4;
                const int rows = (static_cast<int>(layout.toolbar.size()) + columns - 1) / columns;
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
        if (CheckCollisionPointRec(position, layout.restart)) return {Control::RESTART};
        if (mobileControlsEnabled) {
                constexpr Control controls[] = {Control::DIG, Control::FLAG, Control::FIT, Control::MENU};
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
        ClearBackground(Color{168, 179, 185, 255});
        UILayout layout = getLayout();
        const float uiScale = getUiScale();
        const Rectangle lastDifficulty = layout.difficulties.back();
        const float panelY = std::max(0.0f, layout.title.y - 8.0f * uiScale);
        const Rectangle menuPanel = {layout.mode.x - 12.0f * uiScale, panelY,
                                     layout.mode.width + 24.0f * uiScale,
                                     lastDifficulty.y + lastDifficulty.height - panelY + 8.0f * uiScale};
        drawBeveledSurface(menuPanel, classicSurface, 4.0f * uiScale, true);
        drawCenteredText("MINESWEEPER", layout.title, static_cast<int>(layout.title.height * 0.8f),
                         Color{35, 43, 47, 255});
        drawButton(selectedMode == GameMode::CLASSIC ? "Mode: Classic" : "Mode: NoGuess", layout.mode, true);
        const char* labels[] = {"Beginner (9x9)", "Intermediate (16x16)", "Expert (30x16)", "Evil (30x20)"};
        for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                const Rectangle bounds = layout.difficulties[index];
                const bool selected = index == getSelectedDifficultyIndex();
                Color background = selected ? Color{211, 229, 237, 255} : classicSurface;
                if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
                        background = selected ? Color{224, 238, 243, 255} : Color{211, 211, 211, 255};
                }
                drawBeveledSurface(bounds, background, 3.0f * uiScale, true);
                if (selected) {
                        DrawRectangleLinesEx({bounds.x + 4.0f * uiScale, bounds.y + 4.0f * uiScale,
                                              bounds.width - 8.0f * uiScale, bounds.height - 8.0f * uiScale},
                                             1.0f * uiScale, Color{75, 111, 125, 255});
                }
                const float lineHeight = bounds.height * 0.5f;
                drawCenteredText(labels[index], {bounds.x, bounds.y, bounds.width, lineHeight},
                                 static_cast<int>(17.0f * uiScale), Color{30, 35, 39, 255});
                const int bestTime = bestTimes[getSelectedModeIndex()][index];
                const char* recordText = bestTime == noBestTime ? "Best: --"
                        : TextFormat("Best: %02d:%02d", bestTime / 60, bestTime % 60);
                drawCenteredText(recordText, {bounds.x, bounds.y + lineHeight, bounds.width, lineHeight},
                                 static_cast<int>(14.0f * uiScale), Color{78, 85, 89, 255});
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
        ClearBackground(classicCanvas);

        UILayout layout = getLayout();
        const float uiScale = getUiScale();
        const bool gameFinished = currentState == GameState::WON || currentState == GameState::LOST;
        Vector2 pointerPosition = GetMousePosition();
        const int touchCount = GetTouchPointCount();
        if (touchCount == 1) pointerPosition = GetTouchPosition(0);
        const HitTarget pointerTarget = hitTest(pointerPosition);
        const int hoveredCell = pointerTarget.control == Control::CELL ? pointerTarget.cellIndex : -1;
        const bool pressedRestart = pointerTarget.control == Control::RESTART
                && (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || touchCount == 1);
        const bool shockedFace = !gameFinished && hoveredCell >= 0
                && !engine->getCell(hoveredCell).isRevealed
                && (IsMouseButtonDown(MOUSE_LEFT_BUTTON)
                    || (touchCount == 1 && touchTool == TouchTool::DIG));

        drawBeveledSurface(layout.header, classicSurface, 2.0f * uiScale, false);
        drawLedCounter(layout.mineCounter, engine->total_mines() - engine->total_flags());
        drawFaceButton(layout.restart, currentState, pressedRestart, shockedFace);
        drawLedCounter(layout.timeCounter, std::clamp(timeElapsed, 0, 999));
        drawCenteredText("MINES", {layout.mineCounter.x, layout.mineCounter.y + layout.mineCounter.height,
                                   layout.mineCounter.width, 14.0f * uiScale},
                         static_cast<int>(10.0f * uiScale), Color{55, 61, 64, 255});
        drawCenteredText("TIME", {layout.timeCounter.x, layout.timeCounter.y + layout.timeCounter.height,
                                  layout.timeCounter.width, 14.0f * uiScale},
                         static_cast<int>(10.0f * uiScale), Color{55, 61, 64, 255});
        if (gameFinished) {
                const char* message = currentState == GameState::WON ? "YOU WIN!" : "GAME OVER";
                const float resultLineHeight = layout.status.height * 0.5f;
                drawCenteredText(message, {layout.status.x, layout.status.y,
                                           layout.status.width, resultLineHeight},
                                 static_cast<int>(20.0f * uiScale),
                                 currentState == GameState::WON ? GREEN : RED);
                if (currentState == GameState::WON) {
                        const int bestTime = getBestTimeForSelection();
                        const char* recordText = bestTime == noBestTime ? "BEST: --"
                                : TextFormat("BEST: %02d:%02d", bestTime / 60, bestTime % 60);
                        drawCenteredText(recordText, {layout.status.x, layout.status.y + resultLineHeight,
                                                       layout.status.width, resultLineHeight},
                                         static_cast<int>(15.0f * uiScale), Color{113, 83, 15, 255});
                }
                else {
                        drawCenteredText("FACE OR R: RESTART  |  ESC: MENU",
                                         {layout.status.x, layout.status.y + resultLineHeight,
                                          layout.status.width, resultLineHeight},
                                         static_cast<int>(12.0f * uiScale), Color{83, 48, 45, 255});
                }
        }

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
                const Rectangle rect = {static_cast<float>(x), static_cast<float>(y),
                                        static_cast<float>(tileSize - 1), static_cast<float>(tileSize - 1)};
                if (currentState == GameState::LOST && cell.isMine && cell.isFlagged) {
                        drawBoardTile(rect, Color{198, 198, 198, 255}, true);
                        drawFlagIcon(x, y, tileSize);
                }
                else if (!cell.isRevealed) {
                        Color tileColor = Color{198, 198, 198, 255};
                        if (std::find(chordPreviewCells.begin(), chordPreviewCells.end(), idx) != chordPreviewCells.end()) {
                                tileColor = Color{205, 214, 205, 255};
                        }
                        else if (idx == hoveredCell) {
                                tileColor = Color{210, 210, 210, 255};
                        }
                        drawBoardTile(rect, tileColor, true);
                        if (cell.isFlagged) {
                                drawFlagIcon(x, y, tileSize);
                                if (currentState == GameState::LOST && !cell.isMine) {
                                        const Color crossColor = Color{196, 31, 31, 255};
                                        const float crossWidth = std::max(1.5f, tileSize * 0.07f);
                                        DrawLineEx({x + tileSize * 0.16f, y + tileSize * 0.16f},
                                                   {x + tileSize * 0.84f, y + tileSize * 0.84f},
                                                   crossWidth, crossColor);
                                        DrawLineEx({x + tileSize * 0.84f, y + tileSize * 0.16f},
                                                   {x + tileSize * 0.16f, y + tileSize * 0.84f},
                                                   crossWidth, crossColor);
                                }
                        }
                }
                else {
                        if (cell.isMine) {
                                const Color mineCellColor = idx == explodedMineIndex
                                        ? Color{229, 57, 53, 255} : Color{229, 231, 235, 255};
                                drawBoardTile(rect, mineCellColor, false);
                                drawMineIcon(x, y, tileSize);
                        }
                        else {
                                const Color revealedColor = idx == hoveredCell && !chordPreviewCells.empty()
                                        ? Color{229, 241, 229, 255} : Color{229, 231, 235, 255};
                                drawBoardTile(rect, revealedColor, false);
                                if (cell.neighboringMines > 0) {
                                        const char* clueText = TextFormat("%d", cell.neighboringMines);
                                        const int fontSize = std::max(10, static_cast<int>(tileSize * 0.68f));
                                        drawCenteredText(clueText, rect, fontSize,
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
                explodedMineIndex = -1;
                const std::vector<Cell>& cells = engine->getGrid();
                for (int index = 0; index < static_cast<int>(cells.size()); ++index) {
                        if (cells[index].isMine && cells[index].isRevealed) {
                                explodedMineIndex = index;
                                break;
                        }
                }
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
        explodedMineIndex = -1;
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
                return GetColor(0x1976D2FF);
        case 2:
                return GetColor(0x388E3CFF);
        case 3:
                return GetColor(0xD32F2FFF);
        case 4:
                return GetColor(0x0D47A1FF);
        case 5:
                return GetColor(0x880E4FFF);
        case 6:
                return GetColor(0x00796BFF);
        case 7:
                return GetColor(0x212121FF);
        case 8:
                return GetColor(0x616161FF);
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
        const float x = static_cast<float>(cellPixelX);
        const float y = static_cast<float>(cellPixelY);
        const float pixelSize = static_cast<float>(size) / 16.0f;
        constexpr Color red = {255, 0, 0, 255};
        constexpr int outlineStarts[] = {5, 5, 5, 5, 5};
        constexpr int outlineWidths[] = {5, 7, 9, 7, 5};
        constexpr int pennantWidths[] = {3, 5, 7, 5, 3};

        for (int row = 0; row < 5; ++row) {
                drawPixelBlock(x, y, pixelSize, outlineStarts[row], row + 3,
                               outlineWidths[row], 1, BLACK);
                drawPixelBlock(x, y, pixelSize, outlineStarts[row] + 1, row + 3,
                               pennantWidths[row], 1, red);
        }
        drawPixelBlock(x, y, pixelSize, 5, 2, 1, 11, BLACK);
        drawPixelBlock(x, y, pixelSize, 3, 13, 5, 1, BLACK);
        drawPixelBlock(x, y, pixelSize, 2, 14, 7, 1, BLACK);
}

void GameController::drawMineIcon(int cellPixelX, int cellPixelY, int size) {
        const float x = static_cast<float>(cellPixelX);
        const float y = static_cast<float>(cellPixelY);
        const float pixelSize = static_cast<float>(size) / 16.0f;
        constexpr Color mine = BLACK;

        drawPixelBlock(x, y, pixelSize, 0, 7, 6, 2, mine);
        drawPixelBlock(x, y, pixelSize, 10, 7, 6, 2, mine);
        drawPixelBlock(x, y, pixelSize, 7, 0, 2, 6, mine);
        drawPixelBlock(x, y, pixelSize, 7, 10, 2, 6, mine);
        for (int step = 1; step <= 6; ++step) {
                drawPixelBlock(x, y, pixelSize, 7 - step, 7 - step, 1, 1, mine);
                drawPixelBlock(x, y, pixelSize, 8 + step, 8 + step, 1, 1, mine);
                drawPixelBlock(x, y, pixelSize, 8 + step, 7 - step, 1, 1, mine);
                drawPixelBlock(x, y, pixelSize, 7 - step, 8 + step, 1, 1, mine);
        }

        constexpr int bodyStarts[] = {6, 4, 3, 2, 2, 3, 4, 6};
        constexpr int bodyWidths[] = {4, 8, 10, 12, 12, 10, 8, 4};
        for (int row = 0; row < 8; ++row) {
                drawPixelBlock(x, y, pixelSize, bodyStarts[row], row + 4,
                               bodyWidths[row], 1, mine);
        }
        drawPixelBlock(x, y, pixelSize, 5, 5, 1, 1, WHITE);
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
        drawBeveledSurface({0.0f, layout.toolbarTop, layout.header.width,
                            static_cast<float>(GetScreenHeight()) - layout.toolbarTop}, classicSurface,
                           3.0f * uiScale, true);
        const char* labels[] = {"Dig", "Flag", "Fit", "Menu"};
        for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                const bool selected = (index == 0 && touchTool == TouchTool::DIG)
                        || (index == 1 && touchTool == TouchTool::FLAG);
                const bool enabled = index != 1 || currentState == GameState::RUNNING;
                const Rectangle bounds = layout.toolbar[index];
                drawButton(labels[index], bounds, selected, enabled);
        }
}
