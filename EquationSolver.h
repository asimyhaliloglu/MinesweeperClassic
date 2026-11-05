//
// Created by asimy on 05.11.2026.
//

#ifndef MINESWEEPERCLASSIC_EQUATIONSOLVER_H
#define MINESWEEPERCLASSIC_EQUATIONSOLVER_H
#include <vector>

// Holds the boolean coefficients as well as the dynamic solution parameter.
struct SparseRow {
        std::vector<int> activeVarIDs;
        int targetValue;
};
// Deductions with integer instead of pointer arithmetic
struct VariableAssignment {
        int varId;
        bool isMine;
};

class EquationSolver {
public:
        // Solves for all direct certainties or the first subset reduction
        static std::vector<VariableAssignment> solve(const std::vector<SparseRow>& matrix);
private:
        static std::vector<VariableAssignment> findTrivial(const std::vector<SparseRow>& matrix);
        static std::vector<VariableAssignment> reduceSubsets(const std::vector<SparseRow>& matrix);
};


#endif //MINESWEEPERCLASSIC_EQUATIONSOLVER_H
