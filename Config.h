#ifndef MINESWEEPERCLASSIC_CONFIG_H
#define MINESWEEPERCLASSIC_CONFIG_H

enum class AppState {
        MAIN_MENU,
        PLAYING,
        OPTIONS
};

enum class GameMode {
        CLASSIC,
        NO_GUESS
};

inline bool soundEnabled = true;

struct DifficultyConfig {
        int width;
        int height;
        int mineCount;

        [[nodiscard]] int getWidth() const {
                return width;
        }

        [[nodiscard]] int getHeight() const {
                return height;
        }

        [[nodiscard]] int mine_count() const {
                return mineCount;
        }
};

constexpr DifficultyConfig BEGINNER = {9, 9, 10};
constexpr DifficultyConfig INTERMEDIATE = {16, 16, 40};
constexpr DifficultyConfig EXPERT = {30, 16, 99};
constexpr DifficultyConfig EVIL = {30, 20, 130};

class Config {
};


#endif //MINESWEEPERCLASSIC_CONFIG_H
