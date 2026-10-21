//
// Created by asimy on 16.10.2026.
//

#include "Solver.h"
#include <algorithm>

bool Solver::isSolvable(BaseMinesweeper& board, int firstClickX, int firstClickY) {
        int firstClickIdx = board.getCellID(firstClickX, firstClickY);
        board.revealCell(firstClickIdx);

        while (true) {
                if (board.isGameWon()) {
                        return true;
                }
                LinearEquationSystem currentSystem = buildLinearSystem(board);

                std::vector<Deduction> choices = executeAlgebraicPass(currentSystem);
                if (choices.empty()) {
                        return false;
                }

                for (const auto& move : choices) {
                        int cellIdx = static_cast<int>(move.cellPtr - &board.getGrid()[0]);
                        if (move.isMine) {
                                move.cellPtr->isFlagged = true;
                        }
                        else {
                                board.revealCell(cellIdx);
                        }
                }
        }
}

// Assign linear equation IDs x_n acting as pointers,
void Solver::assignFrontierIDs(BaseMinesweeper& board, LinearEquationSystem& system) {
        // assign id while resetting them first
        for (auto& cell : board.getGrid()) {
                cell.id = -1;
                if (!cell.isRevealed && !cell.isFlagged) {
                        bool touchesClue = false;
                        //check if this cell touches an active clue (non-blank opened cells)
                        for (Cell* neighbor : cell.neighbors) {
                                if (neighbor->isRevealed && neighbor->neighboringMines > 0) {
                                        touchesClue = true;
                                        break;
                                }
                        }
                        // assign seq. IDs. i.e a contiguous front values are assigned and updated with each iteration
                        if (touchesClue) {
                                cell.id = static_cast<int>(system.variableToCellMap.size());
                                system.variableToCellMap.push_back(&cell);
                        }
                }
        }
}

// creates an actual reduced linear equation system
void Solver::buildEquation(const BaseMinesweeper& board, LinearEquationSystem& system) {
        // total active frontier variables across all open clues.
        int totalVariables = static_cast<int>(system.variableToCellMap.size());
        // early exit guard
        if (totalVariables == 0) {
                return;
        }
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
                                else if (neighbor->id != -1) {
                                        // evaluate the neighbor and push if is an active frontier member
                                        row.activeVarIDs.push_back(neighbor->id);
                                }
                        }
                        //check if the row is satisfied
                        if (!row.activeVarIDs.empty()) {
                                //sort for optimized subset behavior
                                std::sort(row.activeVarIDs.begin(), row.activeVarIDs.end());
                                // remaining unknown mines needed around this clue (clue count minus flags)
                                row.targetValue = clueCell.neighboringMines - currentFlagsCount;
                                // append to the equation system
                                system.matrixA.push_back(row);
                        }
                }
        }
}

// bounds them all together
LinearEquationSystem Solver::buildLinearSystem(BaseMinesweeper& board) {
        LinearEquationSystem system;
        assignFrontierIDs(board, system);
        buildEquation(board, system);
        return system;
}


//handle easy cases
std::vector<Deduction> Solver::executeAlgebraicPass(const LinearEquationSystem& system) {
        if (system.variableToCellMap.empty()) {
                return {};
        }
        //Try to find instant certainties without cross-referencing rows
        std::vector<Deduction> deductions = findTrivialDeductions(system);
        if (!deductions.empty()) {
                return deductions;
        }
        //If trivial logic fails, use row reduction to force a certainty
        return reduceOverlappingEquations(system);
}

//
std::vector<Deduction> Solver::findTrivialDeductions(const LinearEquationSystem& system) {
        //Placeholder that enables the algorithm to act upon certain cells
        std::vector<Deduction> deductions;

        for (const auto& row : system.matrixA) {
                int activeCount = static_cast<int>(row.activeVarIDs.size());
                if (activeCount == 0) {
                        continue;
                }
                // the following are instant reducible guaranteed golden answers
                // If an equation sums to 0, every variable inside it MUST equal 0 (Safe).
                if (row.targetValue == 0) {
                        for (int varId : row.activeVarIDs) {
                                // the variable isn't a mine
                                deductions.push_back({system.variableToCellMap[varId], false});
                        }
                        return deductions;
                }
                // math Principle: Max Target (b = n)
                // If the target equals the number of variables, they MUST all equal 1 (Mines).
                if (row.targetValue == activeCount) {
                        for (int varId : row.activeVarIDs) {
                                deductions.push_back({system.variableToCellMap[varId], true});
                        }
                        return deductions;
                }
        }
        //return the finalized system
        return deductions;
}


//Matrix reduction
std::vector<Deduction> Solver::reduceOverlappingEquations(const LinearEquationSystem& system) {
        //same stuff
        std::vector<Deduction> deductions;
        const auto& matrix = system.matrixA;
        int numRows = static_cast<int>(matrix.size());

        for (int i = 0; i < numRows; ++i) {
                // reassigned for readability
                const auto& rowI = matrix[i];
                // reduced row
                int activeInI = static_cast<int>(rowI.activeVarIDs.size());
                if (activeInI == 0) {
                        continue;
                }
                for (int j = 0; j < numRows; ++j) {
                        //skip overlapping values
                        if (i == j) continue;
                        // reassigned
                        const auto& rowJ = matrix[j];
                        int activeInJ = static_cast<int>(rowJ.activeVarIDs.size());

                        // I can only be a strict subset of J if I touches fewer cells than J
                        if (activeInI >= activeInJ) {
                                continue;
                        }
                        // check range, subsets on two opposite ends cannot overlap by any means
                        if (rowI.activeVarIDs.front() > rowJ.activeVarIDs.back() ||
                                rowI.activeVarIDs.back() < rowJ.activeVarIDs.front()) {
                                continue;
                        }
                        // strict subset check
                        if (std::includes(rowJ.activeVarIDs.begin(), rowJ.activeVarIDs.end(),
                                          rowI.activeVarIDs.begin(), rowI.activeVarIDs.end())) {
                                int reducedTarget = rowJ.targetValue - rowI.targetValue;
                                // number of unique cells that only touch J in this context
                                int reducedActiveCount = activeInJ - activeInI;
                                // Form the new theoretical equation by skipping variables that were in I
                                std::vector<int> reducedActiveVars;
                                //memory alloc for better heap behavior
                                reducedActiveVars.reserve(reducedActiveCount);
                                std::set_difference(rowJ.activeVarIDs.begin(), rowJ.activeVarIDs.end(),
                                                    rowI.activeVarIDs.begin(), rowI.activeVarIDs.end(),
                                                    std::back_inserter(reducedActiveVars));
                                // if the target drops to 0, then the unique cells belonging to J are safe and ready to open
                                if (reducedTarget == 0) {
                                        for (int varId : reducedActiveVars) {
                                                deductions.push_back({system.variableToCellMap[varId], false});
                                        }
                                        return deductions;
                                }
                                // every single unique cell contains a mine
                                if (reducedTarget == reducedActiveCount) {
                                        for (int varId : reducedActiveVars) {
                                                deductions.push_back({system.variableToCellMap[varId], true});
                                        }
                                        return deductions;
                                }
                        }
                }
        }

        // return the final mapping
        return deductions;
}
