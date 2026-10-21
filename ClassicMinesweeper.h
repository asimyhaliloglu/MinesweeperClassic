//
// Created by asimy on 6/24/2026.
//

#ifndef MINESWEEPER_MINESWEEPER_H
#define MINESWEEPER_MINESWEEPER_H

#include "BaseMinesweeper.h"

class ClassicMinesweeper : public BaseMinesweeper {
public:
        void generateBoard(int firstClickX, int firstClickY) override;
};

#endif //MINESWEEPER_MINESWEEPER_H
