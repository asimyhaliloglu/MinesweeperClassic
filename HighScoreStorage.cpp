#include "HighScoreStorage.h"
#include "raylib.h"

#include <cstdlib>
#include <sstream>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>

EM_JS(char*, loadBrowserHighScores, (),  {
        try {
                const value= localStorage.getItem("minesweeper.highscores.v1");
                if (value == = null) return 0;
                const size= lengthBytesUTF8(value) + 1;
                const buffer= _malloc(size);
                if (buffer == = 0) return 0;
                stringToUTF8(value, buffer, size);
                return buffer;
        }
        catch (error) {
                return 0;
        }
});

EM_JS(int, saveBrowserHighScores, (const char* text),  {
        try {
                localStorage.setItem("minesweeper.highscores.v1", UTF8ToString(text));
                return 1;
        }
        catch (error) {
                return 0;
        }
});
#endif
using namespace std;
static constexpr char highScoresFileName[] = "highscores.dat";
static constexpr char highScoresSignature[] = "MINESWEEPER_HIGHSCORES";
static constexpr int highScoresFormatVersion = 1;

bool HighScoreStorage::deserializeBestTimes(const string& serialized, BestTimes& bestTimes) {
        istringstream input(serialized);
        string signature;
        int version = 0;

        if (!(input >> signature >> version) || signature != highScoresSignature || version !=
                highScoresFormatVersion) {
                return false;
        }

        BestTimes parsed{};
        for (auto& modeTimes : parsed) {
                for (int& time : modeTimes) {
                        if (!(input >> time) || time < HighScoreStorage::noBestTime) {
                                return false;
                        }
                }
        }

        string trailingData;
        if (input >> trailingData) {
                return false;
        }

        bestTimes = parsed;
        return true;
}

BestTimes HighScoreStorage::load() {
        BestTimes times{};
        for (auto& modeTimes : times) {
                modeTimes.fill(noBestTime);
        }

        string serialized;

#if defined(__EMSCRIPTEN__)
        if (char* browserData = loadBrowserHighScores()) {
                serialized = browserData;
                free(browserData);
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
        if (!serialized.empty() && deserializeBestTimes(serialized, loaded)) {
                times = loaded;
        }

        return times;
}

void HighScoreStorage::save(const BestTimes& times) {
        ostringstream output;
        output << highScoresSignature << ' ' << highScoresFormatVersion << '\n';
        for (const auto& modeTimes : times) {
                for (int time : modeTimes) {
                        output << time << ' ';
                }
                output << '\n';
        }

        const string serialized = output.str();

#if defined(__EMSCRIPTEN__)
        if (saveBrowserHighScores(serialized.c_str()) == 0) {
                TraceLog(LOG_WARNING, "Could not save high scores to browser storage");
        }
#else
        if (!SaveFileText(highScoresFileName, const_cast<char*>(serialized.data()))) {
                TraceLog(LOG_WARNING, "Could not save high scores to %s", highScoresFileName);
        }
#endif
}
