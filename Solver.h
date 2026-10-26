//
// Created by asimy on 16.10.2026.
//

#ifndef MINESWEEPERCLASSIC_SOLVER_H
#define MINESWEEPERCLASSIC_SOLVER_H
#include "BaseMinesweeper.h"
#include <vector>


// The final answer, holds if the cell is a mine or not
struct Deduction {
        Cell* cellPtr;
        bool isMine;
};

// Holds the boolean coefficients as well as the dynamic solution parameter.
struct SparseRow {
        std::vector<int> activeVarIDs;
        int targetValue;
};

// A linear equation system that updates with each expansion
struct LinearEquationSystem {
        std::vector<SparseRow> matrixA;
        std::vector<Cell*> variableToCellMap;
};

struct SolverHintAnalysis {
        LinearEquationSystem system;
        std::vector<Deduction> deductions;
};

class Solver {
public:
        static bool isSolvable(BaseMinesweeper& board, int firstClickX, int firstClickY);
        static SolverHintAnalysis analyzeForHint(BaseMinesweeper& board);

private:
        static LinearEquationSystem buildLinearSystem(BaseMinesweeper& board);

        static void assignFrontierIDs(BaseMinesweeper& board, LinearEquationSystem& system);

        static void buildEquation(const BaseMinesweeper& board, LinearEquationSystem& system);

        static std::vector<Deduction> executeAlgebraicPass(const LinearEquationSystem& system);

        static std::vector<Deduction> findTrivialDeductions(const LinearEquationSystem& system);

        static std::vector<Deduction> reduceOverlappingEquations(const LinearEquationSystem& system);
};


#endif //MINESWEEPERCLASSIC_SOLVER_H
