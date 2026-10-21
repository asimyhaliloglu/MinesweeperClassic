//
// Created by asimy on 6/25/2026.
//

#ifndef MINESWEEPERCLASSIC_BASEMINESWEEPER_H

#define MINESWEEPERCLASSIC_BASEMINESWEEPER_H

#include <vector>

#include <random>

struct Cell {
        bool isMine = false;
        bool isRevealed = false;
        bool isFlagged = false;
        int neighboringMines = 0;

        std::vector<Cell*> neighbors;

        int id = -1;
};

class BaseMinesweeper {
private:
        int width = 0;
        int height = 0;
        int totalFlags = 0;

protected:
        std::mt19937 gen;

        bool mineExploded = false;
        int totalMines = 0;
        int totalRevealed = 0;
        std::vector<Cell> grid;
        bool useSafeOpening = true;

        bool hasEnded = false;

        void resetBoardSolver();

        void resetBoardConstraints();

        [[nodiscard]] bool isValid(int x, int y) const {
                return x >= 0 && x < width && y >= 0 && y < height;
        }

        void countFlags(int x, int y);

        void initBoard(int cx, int cy);

public:
        BaseMinesweeper() : gen(std::random_device{}()) {
        }

        [[nodiscard]] int total_flags() const {
                return totalFlags;
        }

        virtual ~BaseMinesweeper() = default;

        //return mutable, modifiable objects
        [[nodiscard]] Cell& getCell(int index) {
                return grid[index];
        }

        // return immutable, unmodifiable objects, read-only
        [[nodiscard]] const Cell& getCell(int index) const {
                return grid[index];
        }

        [[nodiscard]] int getCellID(int x, int y) const {
                return x + (y * width);
        }

        [[nodiscard]] int total_mines() const {
                return totalMines;
        }

        [[nodiscard]] bool has_ended() const {
                return hasEnded;
        }

        void toggleFlag(int index);


        void incrementFlag();

        void decrementFlag();

        bool revealCell(int index);

        void countNeighboringMines();

        void loadBoard(int w, int h, int m);

        void chordCell(int index);

        [[nodiscard]] const std::vector<Cell>& getGrid() const {
                return grid;
        }

        [[nodiscard]] std::vector<Cell>& getGrid() {
                return grid;
        }

        [[nodiscard]] bool isGameLost() const { return mineExploded; }

        [[nodiscard]] bool isGameWon() const {
                return !mineExploded && totalRevealed == (static_cast<int>(grid.size()) - totalMines);
        }

        void revealAllMines() {
                for (auto& cell : grid) {
                        if (cell.isMine) {
                                cell.isRevealed = true;
                        }
                }
        }

        virtual void generateBoard(int firstClickX, int firstClickY) = 0;

        [[nodiscard]] int countNeighboringFlags(int index) const;

        [[nodiscard]] int countNeighboringMines(int x, int y) const;

        [[nodiscard]] int countUnrevealedNeighbors(int index) const;

        void flagAllMines() {
                for (auto& cell : grid) {
                        if (cell.isMine && !cell.isFlagged) {
                                cell.isFlagged = true;
                                totalFlags++;
                        }
                }
        }

        [[nodiscard]] int getWidth() const {
                return width;
        }

        [[nodiscard]] int getHeight() const {
                return height;
        }
};
#endif
