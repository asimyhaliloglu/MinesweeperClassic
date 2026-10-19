//
// Created by asimy on 6/25/2026.
//

#include "NoGuessMinesweeper.h"
#include "Solver.h"

void NoGuessMinesweeper::generateBoard(int firstClickX, int firstClickY) {
        const int maxAttempts = 1000;
        int attempts = 0;

        while (attempts < maxAttempts) {
                attempts++;
                initBoard(firstClickX, firstClickY);

                if (Solver::isSolvable(*this, firstClickX, firstClickY)) {
                        resetBoardSolver();
                        return;
                }
        }

        // Fallback if 1000 attempts exceeded
        resetBoardSolver();
        countNeighboringMines();
}