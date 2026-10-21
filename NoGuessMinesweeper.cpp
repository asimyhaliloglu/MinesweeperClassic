//
// Created by asimy on 6/25/2026.
//

#include "NoGuessMinesweeper.h"
#include "Solver.h"
#include <thread>
#include <atomic>
#include <vector>

void NoGuessMinesweeper::generateBoard(int firstClickX, int firstClickY) {
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) {
                numThreads = 4;
        }

        const int maxTotalAttempts = 2000;
        const int attemptsPerThread = maxTotalAttempts / numThreads;


        //prevent data race
        std::atomic<bool> foundBoard(false);
        std::vector<bool> winningMineMap;
        //mutex to handle concurrency
        std::mutex winMutex;

        auto worker = [&](unsigned int seedOffset) {
                // Each thread gets its own independent board clone
                NoGuessMinesweeper localBoard;
                localBoard.loadBoard(this->getWidth(), this->getHeight(), this->totalMines);
                localBoard.useSafeOpening = this->useSafeOpening;

                // Unique seed per thread to ensure diverse random layouts
                localBoard.gen.seed(std::random_device{}() + seedOffset);

                for (int attempt = 0; attempt < attemptsPerThread; ++attempt) {
                        if (foundBoard.load(std::memory_order_relaxed)) {
                                return;
                        }

                        localBoard.initBoard(firstClickX, firstClickY);

                        if (Solver::isSolvable(localBoard, firstClickX, firstClickY)) {
                                bool expected = false;
                                if (foundBoard.compare_exchange_strong(expected, true)) {
                                        std::lock_guard<std::mutex> lock(winMutex);
                                        winningMineMap.resize(localBoard.getGrid().size());
                                        for (size_t i = 0; i < localBoard.getGrid().size(); ++i) {
                                                winningMineMap[i] = localBoard.getGrid()[i].isMine;
                                        }
                                }
                                return;
                        }
                }
        };

        std::vector<std::thread> threads;
        threads.reserve(numThreads);
        for (unsigned int i = 0; i < numThreads; ++i) {
                threads.emplace_back(worker, i * 1000 + 7);
        }

        for (auto& t : threads) {
                t.join();
        }

        // Apply result or run fallback
        resetBoardConstraints();
        resetBoardSolver();

        if (foundBoard.load()) {
                for (size_t i = 0; i < grid.size(); ++i) {
                        grid[i].isMine = winningMineMap[i];
                }
        }
        else {
                // Fallback: regular placement if time-out reached
                initBoard(firstClickX, firstClickY);
        }

        resetBoardSolver();
        countNeighboringMines();
}
