//
// Created by asimy on 6/25/2026.
//

#ifndef MINESWEEPERCLASSIC_NOGUESSMINESWEEPER_H
#define MINESWEEPERCLASSIC_NOGUESSMINESWEEPER_H
#include "BaseMinesweeper.h"


class NoGuessMinesweeper : public BaseMinesweeper {
public:
        void generateBoard(int firstClickX, int firstClickY) override;
        friend class Solver;
};


#endif //MINESWEEPERCLASSIC_NOGUESSMINESWEEPER_H
