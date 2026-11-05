//
// Created by asimy on 04.11.2026.
//

#ifndef MINESWEEPERCLASSIC_HIGHSCORESTORAGE_H
#define MINESWEEPERCLASSIC_HIGHSCORESTORAGE_H

#include <array>
#include <string>

using BestTimes = std::array<std::array<int, 4>, 2>;

class HighScoreStorage {
        static bool deserializeBestTimes(const std::string& serialized, BestTimes& bestTimes);
public:
        static constexpr int noBestTime = -1;
        // Reads high scores from disk (desktop) or localStorage (web)
        [[nodiscard]] static BestTimes load();

        // Saves high scores to disk (desktop) or localStorage (web)
        static void save(const BestTimes& times);

};
#endif
