//
// Created by asimy on 05.11.2026.
//

#include "EquationSolver.h"
#include <algorithm>

std::vector<VariableAssignment> EquationSolver::findTrivial(const std::vector<SparseRow>& matrix) {
        std::vector<VariableAssignment> deductions;

        for (const auto& row : matrix) {
                int activeCount = static_cast<int>(row.activeVarIDs.size());
                if (activeCount == 0) {
                        continue;
                }
                // the following are instant reducible guaranteed golden answers
                // If an equation sums to 0, every variable inside it MUST equal 0 (Safe).
                if (row.targetValue == 0) {
                        for (int id : row.activeVarIDs) {
                                // the variable isn't a mine
                                deductions.push_back({id, false});
                        }
                        return deductions;
                }
                // math Principle: Max Target (b = n)
                // If the target equals the number of variables, they MUST all equal 1 (Mines).
                if (row.targetValue == activeCount) {
                        for (int id : row.activeVarIDs) {
                                deductions.push_back({id, true});
                        }
                        return deductions;
                }
        }
        //return the finalized system
        return deductions;
}

//Matrix reduction
std::vector<VariableAssignment> EquationSolver::reduceSubsets(const std::vector<SparseRow>& matrix) {
        //same stuff
        std::vector<VariableAssignment> deductions;
        const int numRows = static_cast<int>(matrix.size());

        for (int i = 0; i < numRows; ++i) {
                // reassigned for readability
                const auto& rowI = matrix[i];
                //reduced row
                const int activeInI = static_cast<int>(rowI.activeVarIDs.size());
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
                        // range pruning: cannot overlap if ranges do not intersect
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
                                                deductions.push_back({varId, false});
                                        }
                                        return deductions;
                                }
                                // every single unique cell contains a mine
                                if (reducedTarget == reducedActiveCount) {
                                        for (int varId : reducedActiveVars) {
                                                deductions.push_back({varId, true});
                                        }
                                        return deductions;
                                }
                        }
                }
        }
        // return the final mapping
        return deductions;
}

std::vector<VariableAssignment> EquationSolver::solve(const std::vector<SparseRow>& matrix) {
        if (matrix.empty()) {
                return {};
        }
        std::vector<VariableAssignment> deductions = findTrivial(matrix);
        if (!deductions.empty()) {
                return deductions;
        }
        return reduceSubsets(matrix);
}