//
// Created by asimy on 6/25/2026.
//

#ifndef MINESWEEPERCLASSIC_NOGUESSMINESWEEPER_H
#define MINESWEEPERCLASSIC_NOGUESSMINESWEEPER_H
#include "BaseMinesweeper.h"
#include "Solver.h"


class NoGuessMinesweeper : public BaseMinesweeper {
private:


        void assignFrontierIDs(LinearEquationSystem &);

        void buildEquation(LinearEquationSystem &) const;

        LinearEquationSystem buildLinearSystem();

        static std::vector<Deduction> findTrivialDeductions(const LinearEquationSystem &system);

        static std::vector<Deduction> reduceOverlappingEquations(const LinearEquationSystem &system);

        static std::vector<Deduction> executeAlgebraicPass(const LinearEquationSystem &system);

public:
        void generateBoard(int firstClickX, int firstClickY) override;
        friend class Solver;
};


#endif //MINESWEEPERCLASSIC_NOGUESSMINESWEEPER_H
