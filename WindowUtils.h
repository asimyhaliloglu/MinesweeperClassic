//
// Created by asimy on 04.11.2026.
//

#ifndef MINESWEEPERCLASSIC_WINDOWUTILS_H
#define MINESWEEPERCLASSIC_WINDOWUTILS_H
#include <algorithm>

#include "raylib.h"


struct Dimensions {
        float width = 0.0f;
        float height = 0.0f;
};

[[nodiscard]] inline Dimensions getScreenDimensions() noexcept {
        return {
                static_cast<float>(std::max(1, GetScreenWidth())),
                static_cast<float>(std::max(1, GetScreenHeight()))
        };
}

[[nodiscard]] inline Dimensions getRenderDimensions() noexcept {
        return {
                static_cast<float>(std::max(1, GetRenderWidth())),
                static_cast<float>(std::max(1, GetRenderHeight()))
        };
}


#endif //MINESWEEPERCLASSIC_WINDOWUTILS_H
