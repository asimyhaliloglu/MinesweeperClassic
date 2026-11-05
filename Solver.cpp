//
// Created by asimy on 16.10.2026.
//

#include "Solver.h"
#include <algorithm>
#include <utility>

#include "EquationSolver.h"


bool Solver::isSolvable(BaseMinesweeper& board, int firstClickX, int firstClickY) {
        int firstClickIdx = board.getCellID(firstClickX, firstClickY);
        board.revealCell(firstClickIdx);

        while (true) {
                if (board.isGameWon()) {
                        return true;
                }
                LinearEquationSystem currentSystem = extractFrontier(board);

                std::vector<VariableAssignment> choices = EquationSolver::solve(currentSystem.matrix);
                if (choices.empty()) {
                        return false;
                }

                for (const auto& move : choices) {
                        int cellIdx = currentSystem.variableToCellMap[move.varId];
                        if (move.isMine) {
                                board.getGrid()[cellIdx].isFlagged = true;
                        }
                        else {
                                board.revealCell(cellIdx);
                        }
                }
        }
}

LinearEquationSystem Solver::extractFrontier(const BaseMinesweeper& board) {
        LinearEquationSystem frontier;
        const auto& grid = board.getGrid();
        const int totalCells = static_cast<int>(grid.size());

        // Local lookup grid index -> variable ID (-1 if not on frontier)
        std::vector<int> cellToVar(totalCells, -1);

        //Identify all unrevealed cells that touch an active clue
        //replaces old assignFrontierIDs function with simple index calculation
        for (int idx = 0; idx < totalCells; ++idx) {
                const Cell& cell = grid[idx];
                if (!cell.isRevealed && !cell.isFlagged) {
                        bool touchesClue = false;
                        for (const Cell* neighbor : cell.neighbors) {
                                if (neighbor->isRevealed && neighbor->neighboringMines > 0) {
                                        touchesClue = true;
                                        break;
                                }
                        }
                        if (touchesClue) {
                                cellToVar[idx] = static_cast<int>(frontier.variableToCellMap.size());
                                frontier.variableToCellMap.push_back(idx);
                        }
                }
        }
        if (frontier.variableToCellMap.empty()) {
                return frontier;
        }

        //replaces old buildEquation body
        // iterates over the cells and ignores the null assigned frontier values to eliminate redundancy
        for (const auto& clueCell : board.getGrid()) {
                if (clueCell.isRevealed && clueCell.neighboringMines > 0) {
                        //initialize a row
                        SparseRow row;
                        // total number of 1's
                        int currentFlagsCount = 0;

                        for (Cell* neighbor : clueCell.neighbors) {
                                // count the already placed flags to reduce
                                if (neighbor->isFlagged) {
                                        ++currentFlagsCount;
                                }
                                else {
                                        int neighborIdx = static_cast<int>(neighbor - grid.data());
                                        int varId = cellToVar[neighborIdx];
                                        if (varId != -1) {
                                                row.activeVarIDs.push_back(varId);
                                        }
                                }
                        }
                        //check if the row is satisfied
                        if (!row.activeVarIDs.empty()) {
                                //sort for optimized subset behavior
                                std::sort(row.activeVarIDs.begin(), row.activeVarIDs.end());
                                // remaining unknown mines needed around this clue (clue count minus flags)
                                row.targetValue = clueCell.neighboringMines - currentFlagsCount;
                                // append to the equation system
                                frontier.matrix.push_back(row);
                        }
                }
        }
        return frontier;
}

HintResult Solver::calculateHint(const BaseMinesweeper& board) {
        LinearEquationSystem frontier = extractFrontier(board);
        std::vector<VariableAssignment> moves = EquationSolver::solve(frontier.matrix);
        // Guaranteed logical moves
        int safeIndex = -1;
        int mineIndex = -1;

        for (const auto& move : moves) {
                int cellIdx = frontier.variableToCellMap[move.varId];
                if (move.isMine) {
                        if (mineIndex < 0 || cellIdx < mineIndex) mineIndex = cellIdx;
                }
                else {
                        if (safeIndex < 0 || cellIdx < safeIndex) safeIndex = cellIdx;
                }
        }

        if (safeIndex >= 0) return {safeIndex, HintType::SAFE};
        if (mineIndex >= 0) return {mineIndex, HintType::MINE};

        // No certain move found
        return {-1, HintType::NONE};
}
