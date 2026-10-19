#include "BaseMinesweeper.h"

#include <cmath>

void BaseMinesweeper::toggleFlag(int index) {
        if (index < 0 || index >= static_cast<int>(grid.size()) || grid[index].isRevealed) {
                return;
        }
        grid[index].isFlagged = !grid[index].isFlagged;
        if (grid[index].isFlagged) {
                totalFlags++;
        } else {
                totalFlags--;
        }
}

void BaseMinesweeper::loadBoard(int w, int h, int m) {
        width = w;
        height = h;
        totalMines = m;
        grid.clear();
        grid.resize(width * height);

        // Pure flat memory calculation
        for (auto &cell: grid) {
                int index = static_cast<int>(&cell - &grid[0]);
                int cx = index % width;
                int cy = index / width;

                for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dy == 0) {
                                        continue;
                                }
                                int nx = cx + dx;
                                int ny = cy + dy;

                                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                                        cell.neighbors.push_back(&grid[nx + ny * width]);
                                }
                        }
                }
        }
}

void BaseMinesweeper::initBoard(int cx, int cy) {
        std::uniform_int_distribution<> distX(0, width - 1);
        std::uniform_int_distribution<> distY(0, height - 1);

        resetBoardConstraints();
        resetBoardSolver();

        int placedMines = 0;
        while (placedMines < totalMines) {
                int rx = distX(gen);
                int ry = distY(gen);

                bool isInsideSafetyZone = false;

                if (useSafeOpening) {
                        // safety buffer for not instant failure
                        isInsideSafetyZone = std::abs(rx - cx) <= 1 && std::abs(ry - cy) <= 1;
                } else {
                        // true old school
                        isInsideSafetyZone = rx == cx && ry == cy;
                }
                // Reject if it falls in the safety zone OR if a mine is already there
                int targetIndex = getCellID(rx, ry);
                if (isInsideSafetyZone || getCell(targetIndex).isMine) {
                        continue;
                }
                getCell(targetIndex).isMine = true;
                placedMines++;
        }
        countNeighboringMines();
}

void BaseMinesweeper::resetBoardSolver() {
        for (auto &cell: grid) {
                cell.isRevealed = false;
                cell.isFlagged = false;
                cell.id = -1;
        }
        mineExploded = false;
        totalRevealed = 0;
        totalFlags = 0;
}

void BaseMinesweeper::resetBoardConstraints() {
        for (auto &cell : grid) {
                cell.isMine = false;
                cell.neighboringMines = 0;
        }
}

bool BaseMinesweeper::revealCell(int index) {
        if (index < 0 || index >= static_cast<int>(grid.size())) {
                return false;
        }
        Cell *startCell = &getCell(index);
        if (startCell->isRevealed || startCell->isFlagged) {
                return false;
        }
        std::vector<Cell *> toReveal;
        toReveal.push_back(startCell);
        while (!toReveal.empty()) {
                Cell *current = toReveal.back();
                toReveal.pop_back();
                if (current->isRevealed || current->isFlagged) {
                        continue;
                }
                current->isRevealed = true;
                if (current->isMine) {
                        mineExploded = true;
                } else {
                        ++totalRevealed;
                }
                if (current->neighboringMines == 0 && !current->isMine) {
                        for (Cell *neighbor: current->neighbors) {
                                if (!neighbor->isRevealed && !neighbor->isFlagged) {
                                        toReveal.push_back(neighbor);
                                }
                        }
                }
        }
        return true;
}

void BaseMinesweeper::countNeighboringMines() {
        for (auto &cell: grid) {
                if (cell.isMine) {
                        continue;
                }
                int mineCount = 0;
                for (Cell *neighbor: cell.neighbors) {
                        if (neighbor->isMine) {
                                ++mineCount;
                        }
                }
                cell.neighboringMines = mineCount;
        }
}

void BaseMinesweeper::chordCell(int index) {
        Cell &cell = getCell(index);
        if (!cell.isRevealed || cell.neighboringMines == 0) {
                return;
        }
        // If the user placed enough flags, safely clear all remaining hidden tiles
        if (countNeighboringFlags(index) == cell.neighboringMines) {
                for (Cell *neighbor: cell.neighbors) {
                        if (!neighbor->isFlagged && !neighbor->isRevealed) {
                                revealCell(neighbor - &grid[0]);
                        }
                }
        }
}

int BaseMinesweeper::countNeighboringFlags(int index) const {
        const auto &cell = getCell(index);
        int count = 0;
        for (Cell *neighbor: cell.neighbors) {
                if (neighbor->isFlagged) {
                        ++count;
                }
        }
        return count;
}

int BaseMinesweeper::countUnrevealedNeighbors(int index) const {
        const auto &cell = getCell(index);
        int count = 0;
        for (Cell *neighbor: cell.neighbors) {
                if (!neighbor->isRevealed) {
                        ++count;
                }
        }
        return count;
}


void BaseMinesweeper::incrementFlag() {
        totalFlags++;
}

void BaseMinesweeper::decrementFlag() {
        totalFlags--;
}
