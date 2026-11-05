//
// Created by asimy on 16.10.2026.
//

#ifndef MINESWEEPERCLASSIC_SOLVER_H
#define MINESWEEPERCLASSIC_SOLVER_H
#include "BaseMinesweeper.h"
#include "Config.h"
#include <vector>

#include "EquationSolver.h"


struct HintResult {
        int cellIndex = -1;
        HintType type = HintType::NONE;
};

// A linear equation system that updates with each expansion
struct LinearEquationSystem {
        std::vector<SparseRow> matrix;
        std::vector<int> variableToCellMap;
};

class Solver {
public:
        static bool isSolvable(BaseMinesweeper& board, int firstClickX, int firstClickY);
        [[nodiscard]] static HintResult calculateHint(const BaseMinesweeper& board);

private:
        // Builds the frontier system WITHOUT touching cell.id on the board
        static LinearEquationSystem extractFrontier(const BaseMinesweeper& board);
};


#endif //MINESWEEPERCLASSIC_SOLVER_H
