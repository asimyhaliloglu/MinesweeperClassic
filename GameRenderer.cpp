#include "GameRenderer.h"
#include <algorithm>
#include <cmath>
#include "WindowUtils.h"

#if defined(__EMSCRIPTEN__)
constexpr bool mobileControlsEnabled = true;
#elif defined(__ANDROID__) || defined(PLATFORM_IOS)
constexpr bool mobileControlsEnabled = true;
#else
constexpr bool mobileControlsEnabled = MINESWEEPER_MOBILE_CONTROLS != 0;
#endif

float GameRenderer::getUIScale() {
        const auto [screenWidth, screenHeight] = getScreenDimensions();
        const Vector2 dpiScale = GetWindowScaleDPI();
        const float dpiX = std::max(0.1f, dpiScale.x);
        const float dpiY = std::max(0.1f, dpiScale.y);

        const float logicalScale = std::min(screenWidth / dpiX / 1280.0f,
                                            screenHeight / dpiY / 720.0f);

        const float pixelScale = std::min(dpiX, dpiY);

        return std::clamp(logicalScale * pixelScale, 0.7f, 2.25f);
}

Vector2 GameRenderer::toRenderCoordinates(Vector2 screenPosition) {
        const auto [screenWidth, screenHeight] = getScreenDimensions();
        const auto [renderWidth, renderHeight] = getRenderDimensions();
        return {
                screenPosition.x * (renderWidth / screenWidth),
                screenPosition.y * (renderHeight / screenHeight)
        };
}

UILayout GameRenderer::getLayout() {
        UILayout layout;
        const auto [screenWidth, screenHeight] = getScreenDimensions();
        const float uiScale = getUIScale();
        const float inset = std::clamp(screenWidth * 0.025f, 10.0f * uiScale, 32.0f * uiScale);
        const float spacing = std::clamp(screenHeight * 0.012f, 6.0f * uiScale, 18.0f * uiScale);
        const float titleHeight = std::clamp(std::min(screenWidth * 0.09f, 48.0f * uiScale),
                                             24.0f * uiScale, 64.0f * uiScale);
        const float buttonHeight = std::clamp((screenHeight - titleHeight - 12.0f * spacing) / 5.0f,
                                              30.0f * uiScale, 58.0f * uiScale);
        const float buttonWidth = std::max(1.0f, std::min(460.0f * uiScale, screenWidth - inset * 2.0f));
        const float menuHeight = titleHeight + 6.0f * spacing + buttonHeight * 5.0f;
        const float menuTop = std::max(inset, (screenHeight - menuHeight) * 0.5f);

        layout.title = {0.0f, menuTop, screenWidth, titleHeight + 8.0f};
        layout.mode = {
                (screenWidth - buttonWidth) * 0.5f, menuTop + titleHeight + spacing,
                buttonWidth, buttonHeight
        };

        for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                layout.difficulties[index] = {
                        layout.mode.x, layout.mode.y + (index + 1) * (buttonHeight + spacing),
                        buttonWidth, buttonHeight
                };
        }

        const float headerHeight = 104.0f * uiScale;
        layout.header = {0.0f, 0.0f, screenWidth, headerHeight};
        const float sideMargin = 8.0f * uiScale;
        const float faceSize = std::min(44.0f * uiScale,
                                        std::max(32.0f * uiScale, screenWidth * 0.14f));
        const float counterWidth = std::min(92.0f * uiScale,
                                            std::max(54.0f * uiScale,
                                                     (screenWidth - faceSize - sideMargin * 4.0f) * 0.5f));
        const float counterY = 6.0f * uiScale;
        const float counterHeight = 36.0f * uiScale;
        layout.mineCounter = {sideMargin, counterY, counterWidth, counterHeight};
        layout.restart = {(screenWidth - faceSize) * 0.5f, 2.0f * uiScale, faceSize, faceSize};
        layout.timeCounter = {screenWidth - sideMargin - counterWidth, counterY, counterWidth, counterHeight};
        layout.status = {
                0.0f, 58.0f * uiScale, screenWidth,
                std::max(0.0f, headerHeight - 58.0f * uiScale)
        };
        layout.toolbarTop = screenHeight;

        if (mobileControlsEnabled) {
                const float controlHeight = std::clamp(screenHeight * 0.075f, 40.0f * uiScale, 64.0f * uiScale);
                const float controlGap = 8.0f * uiScale;
                const float toolbarPadding = 12.0f * uiScale;
                const int columns = screenWidth < 440.0f * uiScale ? 2 : 5;
                const int rows = (static_cast<int>(layout.toolbar.size()) + columns - 1) / columns;
                const float controlWidth = std::max(1.0f, (screenWidth - toolbarPadding * 2.0f
                                                            - (columns - 1) * controlGap) / columns);
                layout.toolbarTop = std::max(headerHeight, screenHeight - (rows * controlHeight
                                                     + (rows - 1) * controlGap + toolbarPadding * 2.0f));
                for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                        const int row = index / columns;
                        const int column = index % columns;
                        layout.toolbar[index] = {
                                (screenWidth - (columns * controlWidth + (columns - 1) * controlGap)) * 0.5f
                                + column * (controlWidth + controlGap),
                                layout.toolbarTop + toolbarPadding + row * (controlHeight + controlGap),
                                controlWidth, controlHeight
                        };
                }
        }
        layout.board = {0.0f, headerHeight, screenWidth, std::max(0.0f, layout.toolbarTop - headerHeight)};
        return layout;
}

void GameRenderer::drawPixelBlock(float x, float y, float pixelSize, int column, int row,
                                  int width, int height, Color color) const {
        DrawRectangleRec({
                                 x + column * pixelSize, y + row * pixelSize,
                                 width * pixelSize, height * pixelSize
                         }, color);
}

void GameRenderer::drawBeveledSurface(Rectangle bounds, Color face, float thickness, bool raised) const {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
        const float bevel = std::min(std::max(0.5f, thickness),
                                     std::min(bounds.width, bounds.height) * 0.24f);
        const Color topLeft = raised ? classicHighlight : classicShadow;
        const Color bottomRight = raised ? classicShadow : classicHighlight;
        DrawRectangleRec(bounds, face);
        DrawRectangleLinesEx(bounds, std::max(0.5f, bevel * 0.35f), classicOutline);
        DrawRectangleRec({
                                 bounds.x + bevel, bounds.y + bevel,
                                 std::max(0.0f, bounds.width - bevel * 2.0f), bevel
                         }, topLeft);
        DrawRectangleRec({
                                 bounds.x + bevel, bounds.y + bounds.height - bevel * 2.0f,
                                 std::max(0.0f, bounds.width - bevel * 2.0f), bevel
                         }, bottomRight);
        DrawRectangleRec({
                                 bounds.x + bevel, bounds.y + bevel, bevel,
                                 std::max(0.0f, bounds.height - bevel * 2.0f)
                         }, topLeft);
        DrawRectangleRec({
                                 bounds.x + bounds.width - bevel * 2.0f, bounds.y + bevel, bevel,
                                 std::max(0.0f, bounds.height - bevel * 2.0f)
                         }, bottomRight);
}

void GameRenderer::drawBoardTile(Rectangle bounds, Color face, bool raised) const {
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
        DrawRectangleRec({
                                 bounds.x + bevel, bounds.y + bounds.height - bevel,
                                 horizontalInterior, bevel
                         }, shadow);
        DrawRectangleRec({
                                 bounds.x + bounds.width - bevel, bounds.y + bevel,
                                 bevel, verticalInterior
                         }, shadow);
}

void GameRenderer::drawSevenSegmentDigit(Rectangle bounds, int digit, Color activeColor) const {
        constexpr int digitSegments[] = {
                0b00111111, 0b00000110, 0b01011011, 0b01001111, 0b01100110,
                0b01101101, 0b01111101, 0b00000111, 0b01111111, 0b01101111
        };
        const int activeSegments = digit < 0 ? 0b01000000 : digitSegments[digit];
        const float pixelSize = std::max(1.0f, std::round(bounds.height / 12.0f));
        const float thickness = std::max(pixelSize * 2.0f,
                                         std::round(std::min(bounds.width, bounds.height) * 0.22f / pixelSize) *
                                         pixelSize);
        const float halfHeight = bounds.height * 0.5f;
        const std::array<Rectangle, 7> segments = {
                {
                        {bounds.x + thickness * 0.5f, bounds.y, bounds.width - thickness, thickness},
                        {
                                bounds.x + bounds.width - thickness, bounds.y + thickness * 0.5f,
                                thickness, halfHeight - thickness * 0.5f
                        },
                        {
                                bounds.x + bounds.width - thickness, bounds.y + halfHeight,
                                thickness, halfHeight - thickness * 0.5f
                        },
                        {
                                bounds.x + thickness * 0.5f, bounds.y + bounds.height - thickness,
                                bounds.width - thickness, thickness
                        },
                        {bounds.x, bounds.y + halfHeight, thickness, halfHeight - thickness * 0.5f},
                        {bounds.x, bounds.y + thickness * 0.5f, thickness, halfHeight - thickness * 0.5f},
                        {
                                bounds.x + thickness * 0.5f, bounds.y + halfHeight - thickness * 0.5f,
                                bounds.width - thickness, thickness
                        }
                }
        };
        constexpr Color inactiveColor = {43, 0, 0, 255};
        for (int index = 0; index < static_cast<int>(segments.size()); ++index) {
                const Color color = (activeSegments & (1 << index)) != 0 ? activeColor : inactiveColor;
                const Rectangle segment = segments[index];
                const float left = std::round((segment.x - bounds.x) / pixelSize) * pixelSize + bounds.x;
                const float top = std::round((segment.y - bounds.y) / pixelSize) * pixelSize + bounds.y;
                const float right = std::round((segment.x + segment.width - bounds.x) / pixelSize)
                        * pixelSize + bounds.x;
                const float bottom = std::round((segment.y + segment.height - bounds.y) / pixelSize)
                        * pixelSize + bounds.y;
                DrawRectangleRec({
                                         left, top, std::max(pixelSize, right - left),
                                         std::max(pixelSize, bottom - top)
                                 }, color);
        }
}

void GameRenderer::drawLedCounter(Rectangle bounds, int value) const {
        drawBeveledSurface(bounds, Color{10, 10, 10, 255}, bounds.height * 0.09f, false);
        const int displayValue = std::clamp(value, -99, 999);
        const char* display = TextFormat("%03d", displayValue);
        const float digitHeight = bounds.height * 0.72f;
        const float digitWidth = std::min(digitHeight * 0.52f, bounds.width * 0.25f);
        const float gap = (bounds.width - digitWidth * 3.0f) / 4.0f;
        const float digitY = bounds.y + (bounds.height - digitHeight) * 0.5f;
        for (int index = 0; index < 3; ++index) {
                const int digit = display[index] == '-' ? -1 : display[index] - '0';
                drawSevenSegmentDigit({
                                              bounds.x + gap + index * (digitWidth + gap), digitY,
                                              digitWidth, digitHeight
                                      }, digit, Color{255, 30, 39, 255});
        }
}

void GameRenderer::drawFaceButton(Rectangle bounds, GameState state, bool pressed, bool shocked) const {
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
        else if (state == GameState::LOST) {
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

                drawPixelBlock(faceX, faceY, pixelSize, 7, 9, 2, 1, BLACK);
                drawPixelBlock(faceX, faceY, pixelSize, 5, 10, 2, 1, BLACK);
                drawPixelBlock(faceX, faceY, pixelSize, 9, 10, 2, 1, BLACK);
                drawPixelBlock(faceX, faceY, pixelSize, 4, 11, 1, 1, BLACK);
                drawPixelBlock(faceX, faceY, pixelSize, 11, 11, 1, 1, BLACK);
        }
        else {
                drawPixelBlock(faceX, faceY, pixelSize, 5, 5, 1, 2, BLACK);
                drawPixelBlock(faceX, faceY, pixelSize, 10, 5, 1, 2, BLACK);

                if (shocked) {
                        drawPixelBlock(faceX, faceY, pixelSize, 7, 9, 2, 1, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 6, 10, 1, 2, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 9, 10, 1, 2, BLACK);
                        drawPixelBlock(faceX, faceY, pixelSize, 7, 12, 2, 1, BLACK);
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

void GameRenderer::drawCenteredText(const char* text, Rectangle bounds, int fontSize, Color color) const {
        const float padding = std::clamp(std::min(bounds.width, bounds.height) * 0.12f, 4.0f, 24.0f);
        while (fontSize > 10 && (MeasureText(text, fontSize) > bounds.width - padding * 2.0f
                || fontSize > bounds.height - padding)) {
                --fontSize;
        }
        DrawText(text, static_cast<int>(bounds.x + (bounds.width - MeasureText(text, fontSize)) * 0.5f),
                 static_cast<int>(bounds.y + (bounds.height - fontSize) * 0.5f), fontSize, color);
}

void GameRenderer::drawButton(const char* label, Rectangle bounds, bool selected, bool enabled) const {
        Color background = selected ? Color{211, 229, 237, 255} : classicSurface;
        if (!enabled) background = Color{160, 160, 160, 255};
        else if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
                background = selected ? Color{224, 238, 243, 255} : Color{211, 211, 211, 255};
        }
        drawBeveledSurface(bounds, background, 3.0f * getUIScale(), true);
        if (selected) {
                DrawRectangleLinesEx({
                                             bounds.x + 4.0f, bounds.y + 4.0f,
                                             bounds.width - 8.0f, bounds.height - 8.0f
                                     },
                                     1.0f * getUIScale(), Color{75, 111, 125, 255});
        }
        drawCenteredText(label, bounds, static_cast<int>(20.0f * getUIScale()),
                         enabled ? Color{30, 35, 39, 255} : Color{96, 96, 96, 255});
}

void GameRenderer::drawFlagIcon(int cellPixelX, int cellPixelY, int size) const {
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

void GameRenderer::drawMineIcon(int cellPixelX, int cellPixelY, int size) const {
        const float x = static_cast<float>(cellPixelX);
        const float y = static_cast<float>(cellPixelY);
        const float cellSize = static_cast<float>(size);
        const Vector2 center = {x + cellSize * 0.5f, y + cellSize * 0.5f};
        const float bodyRadius = cellSize * 0.28f;
        const float spikeLength = cellSize * 0.38f;
        constexpr Color mineColor = {17, 24, 39, 255};
        constexpr std::array<Vector2, 8> directions = {
                {
                        {1.0f, 0.0f}, {0.7071f, 0.7071f}, {0.0f, 1.0f}, {-0.7071f, 0.7071f},
                        {-1.0f, 0.0f}, {-0.7071f, -0.7071f}, {0.0f, -1.0f}, {0.7071f, -0.7071f}
                }
        };

        for (const Vector2 direction : directions) {
                const Vector2 start = {
                        center.x + direction.x * bodyRadius * 0.92f,
                        center.y + direction.y * bodyRadius * 0.92f
                };
                const Vector2 tip = {
                        center.x + direction.x * spikeLength,
                        center.y + direction.y * spikeLength
                };
                const Vector2 perpendicular = {-direction.y, direction.x};
                DrawLineEx(start, tip, 2.0f, mineColor);
                DrawLineEx({tip.x - perpendicular.x * 1.5f, tip.y - perpendicular.y * 1.5f},
                           {tip.x + perpendicular.x * 1.5f, tip.y + perpendicular.y * 1.5f},
                           1.5f, mineColor);
        }
        DrawCircleV(center, bodyRadius, mineColor);
        DrawCircleLinesV(center, bodyRadius, BLACK);
        DrawCircleV({center.x - bodyRadius * 0.35f, center.y - bodyRadius * 0.35f},
                    bodyRadius * 0.25f, RAYWHITE);
}

Color GameRenderer::getNumberColor(int number) const {
        switch (number) {
        case 1: return GetColor(0x1976D2FF);
        case 2: return GetColor(0x388E3CFF);
        case 3: return GetColor(0xD32F2FFF);
        case 4: return GetColor(0x0D47A1FF);
        case 5: return GetColor(0x880E4FFF);
        case 6: return GetColor(0x00796BFF);
        case 7: return GetColor(0x212121FF);
        case 8: return GetColor(0x616161FF);
        default: return WHITE;
        }
}

void GameRenderer::drawMobileControls(const UILayout& layout, TouchTool touchTool, GameState currentState) const {
        if (!mobileControlsEnabled) return;
        const float uiScale = getUIScale();
        drawBeveledSurface({
                                   0.0f, layout.toolbarTop, layout.header.width,
                                   static_cast<float>(GetScreenHeight()) - layout.toolbarTop
                           }, classicSurface,
                           3.0f * uiScale, true);
        const char* labels[] = {"Dig", "Flag", "Fit", "Hint", "Menu"};
        for (int index = 0; index < static_cast<int>(layout.toolbar.size()); ++index) {
                const bool selected = (index == 0 && touchTool == TouchTool::DIG)
                        || (index == 1 && touchTool == TouchTool::FLAG);
                const bool enabled = (index != 1 && index != 3) || currentState == GameState::RUNNING;
                drawButton(labels[index], layout.toolbar[index], selected, enabled);
        }
}

void GameRenderer::renderMainMenu(GameMode selectedMode, int selectedDifficultyIndex,
                                  const std::array<std::array<int, 4>, 2>& bestTimes) const {
        BeginDrawing();
        ClearBackground(Color{168, 179, 185, 255});
        const UILayout layout = getLayout();
        const float uiScale = getUIScale();
        const Rectangle lastDifficulty = layout.difficulties.back();
        const float panelY = std::max(0.0f, layout.title.y - 8.0f * uiScale);
        const Rectangle menuPanel = {
                layout.mode.x - 12.0f * uiScale, panelY,
                layout.mode.width + 24.0f * uiScale,
                lastDifficulty.y + lastDifficulty.height - panelY + 8.0f * uiScale
        };
        drawBeveledSurface(menuPanel, classicSurface, 4.0f * uiScale, true);
        drawCenteredText("MINESWEEPER", layout.title, static_cast<int>(layout.title.height * 0.8f),
                         Color{35, 43, 47, 255});
        drawButton(selectedMode == GameMode::CLASSIC ? "Mode: Classic" : "Mode: NoGuess", layout.mode, true, true);
        const char* labels[] = {"Beginner (9x9)", "Intermediate (16x16)", "Expert (30x16)", "Evil (30x20)"};
        const int modeIndex = selectedMode == GameMode::CLASSIC ? 0 : 1;

        for (int index = 0; index < static_cast<int>(layout.difficulties.size()); ++index) {
                const Rectangle bounds = layout.difficulties[index];
                const bool selected = index == selectedDifficultyIndex;
                Color background = selected ? Color{211, 229, 237, 255} : classicSurface;
                if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
                        background = selected ? Color{224, 238, 243, 255} : Color{211, 211, 211, 255};
                }
                drawBeveledSurface(bounds, background, 3.0f * uiScale, true);
                if (selected) {
                        DrawRectangleLinesEx({
                                                     bounds.x + 4.0f * uiScale, bounds.y + 4.0f * uiScale,
                                                     bounds.width - 8.0f * uiScale, bounds.height - 8.0f * uiScale
                                             },
                                             1.0f * uiScale, Color{75, 111, 125, 255});
                }
                const float lineHeight = bounds.height * 0.5f;
                drawCenteredText(labels[index], {bounds.x, bounds.y, bounds.width, lineHeight},
                                 static_cast<int>(17.0f * uiScale), Color{30, 35, 39, 255});
                const int bestTime = bestTimes[modeIndex][index];
                const char* recordText = bestTime == -1
                                                 ? "Best: --"
                                                 : TextFormat("Best: %02d:%02d", bestTime / 60, bestTime % 60);
                drawCenteredText(recordText, {bounds.x, bounds.y + lineHeight, bounds.width, lineHeight},
                                 static_cast<int>(14.0f * uiScale), Color{78, 85, 89, 255});
        }
        EndDrawing();
}

void GameRenderer::renderGame(const BaseMinesweeper& engine, const Camera2D& camera,
                              int tileSize, const GameRenderState& state) const {
        BeginDrawing();
        ClearBackground(classicCanvas);

        const UILayout layout = getLayout();
        const float uiScale = getUIScale();
        const bool gameFinished = state.currentState == GameState::WON || state.currentState == GameState::LOST;

        const bool shockedFace = !gameFinished && state.hoveredCell >= 0
                && !engine.getCell(state.hoveredCell).isRevealed
                && (IsMouseButtonDown(MOUSE_LEFT_BUTTON)
                        || (GetTouchPointCount() == 1 && state.touchTool == TouchTool::DIG));

        drawBeveledSurface(layout.header, classicSurface, 2.0f * uiScale, false);
        drawLedCounter(layout.mineCounter, engine.total_mines() - engine.total_flags());
        drawFaceButton(layout.restart, state.currentState, state.isRestartPressed, shockedFace);
        drawLedCounter(layout.timeCounter, std::clamp(state.timeElapsed, 0, 999));

        drawCenteredText("MINES", {
                                 layout.mineCounter.x, layout.mineCounter.y + layout.mineCounter.height,
                                 layout.mineCounter.width, 14.0f * uiScale
                         },
                         static_cast<int>(10.0f * uiScale), Color{55, 61, 64, 255});
        drawCenteredText("TIME", {
                                 layout.timeCounter.x, layout.timeCounter.y + layout.timeCounter.height,
                                 layout.timeCounter.width, 14.0f * uiScale
                         },
                         static_cast<int>(10.0f * uiScale), Color{55, 61, 64, 255});

        if (gameFinished) {
                const char* message = state.currentState == GameState::WON ? "YOU WIN!" : "GAME OVER";
                const float resultLineHeight = layout.status.height * 0.5f;
                drawCenteredText(message, {
                                         layout.status.x, layout.status.y,
                                         layout.status.width, resultLineHeight
                                 },
                                 static_cast<int>(20.0f * uiScale),
                                 state.currentState == GameState::WON ? GREEN : RED);
                if (state.currentState == GameState::WON) {
                        const char* recordText = state.bestTime == -1
                                                         ? "BEST: --"
                                                         : TextFormat("BEST: %02d:%02d", state.bestTime / 60,
                                                                      state.bestTime % 60);
                        drawCenteredText(recordText, {
                                                 layout.status.x, layout.status.y + resultLineHeight,
                                                 layout.status.width, resultLineHeight
                                         },
                                         static_cast<int>(15.0f * uiScale), Color{113, 83, 15, 255});
                }
                else {
                        drawCenteredText("FACE OR R: RESTART  |  ESC: MENU",
                                         {
                                                 layout.status.x, layout.status.y + resultLineHeight,
                                                 layout.status.width, resultLineHeight
                                         },
                                         static_cast<int>(12.0f * uiScale), Color{83, 48, 45, 255});
                }
        }

        BeginScissorMode(static_cast<int>(layout.board.x), static_cast<int>(layout.board.y),
                         static_cast<int>(layout.board.width), static_cast<int>(layout.board.height));
        BeginMode2D(camera);
        for (int idx = 0; idx < static_cast<int>(engine.getGrid().size()); ++idx) {
                const Cell& cell = engine.getGrid()[idx];
                const int x = (idx % engine.getWidth()) * tileSize;
                const int y = (idx / engine.getWidth()) * tileSize;
                const Rectangle rect = {
                        static_cast<float>(x), static_cast<float>(y),
                        static_cast<float>(tileSize - 1), static_cast<float>(tileSize - 1)
                };

                if (state.currentState == GameState::LOST && cell.isMine && cell.isFlagged) {
                        drawBoardTile(rect, Color{198, 198, 198, 255}, true);
                        drawFlagIcon(x, y, tileSize);
                }
                else if (!cell.isRevealed) {
                        Color tileColor = Color{198, 198, 198, 255};
                        if (std::find(state.chordPreviewCells.begin(), state.chordPreviewCells.end(), idx) != state.
                                chordPreviewCells.end()) {
                                tileColor = Color{205, 214, 205, 255};
                        }
                        else if (idx == state.hoveredCell) {
                                tileColor = Color{210, 210, 210, 255};
                        }
                        drawBoardTile(rect, tileColor, true);
                        if (cell.isFlagged) {
                                drawFlagIcon(x, y, tileSize);
                                if (state.currentState == GameState::LOST && !cell.isMine) {
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
                                if (idx == state.explodedMineIndex) {
                                        drawBeveledSurface(rect, Color{220, 38, 38, 255}, 2.0f, false);
                                }
                                else {
                                        drawBoardTile(rect, Color{229, 231, 235, 255}, false);
                                }
                                drawMineIcon(x, y, tileSize);
                        }
                        else {
                                const Color revealedColor = idx == state.hoveredCell && !state.chordPreviewCells.empty()
                                                                    ? Color{229, 241, 229, 255}
                                                                    : Color{229, 231, 235, 255};
                                drawBoardTile(rect, revealedColor, false);
                                if (cell.neighboringMines > 0) {
                                        const char* clueText = TextFormat("%d", cell.neighboringMines);
                                        const int fontSize = std::max(10, static_cast<int>(tileSize * 0.68f));
                                        drawCenteredText(clueText, rect, fontSize,
                                                         getNumberColor(cell.neighboringMines));
                                }
                        }
                }

                if (idx == state.hintedCellIndex && state.hintType != HintType::NONE) {
                        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(GetTime() * 5.0));
                        Color hintColor = state.hintType == HintType::MINE
                                                  ? Color{245, 158, 11, 255}
                                                  : state.hintType == HintType::GUESS
                                                  ? Color{6, 182, 212, 255}
                                                  : Color{16, 185, 129, 255};
                        hintColor.a = static_cast<unsigned char>(110.0f + pulse * 135.0f);
                        DrawRectangleLinesEx(rect, std::max(2.0f, tileSize * 0.075f), hintColor);
                }
        }
        EndMode2D();
        EndScissorMode();

        drawMobileControls(layout, state.touchTool, state.currentState);
        EndDrawing();
}
