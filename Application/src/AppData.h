#ifndef APPDATA_H
#define APPDATA_H

#include "Grid2D.h"
#include "WordData.h"
#include "IWorkerPool.h"
#include "IDictionary.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace App
{
  enum class BoardType
  {
    Classic,
    Modern,
    Big,
    Super,
    Mammoth,
    Custom
  };

  struct BoggleResult
  {
    std::vector<Engine::WordData> Words;
    double Time;
  };

  // View-only state for the board UI (selected word, path-highlight animation) - not
  // game data, but reset alongside it in NewGameBoard so it never outlives the board
  // it was computed against.
  struct UIData
  {
    std::string SelectedWord;
    std::vector<std::vector<Engine::Coord>> SelectedPaths;
    std::chrono::steady_clock::time_point SelectionStartTime;

    // Consumed (and cleared) by the board view the first time it draws after a
    // selection is made, so it scrolls the selection into view exactly once and
    // doesn't fight any scrolling the user does afterward.
    bool PendingScrollToSelection;
  };

  // Holds the board a "New board"/"Shake!" click wants to switch to until the "Working"
  // popup it opened has been visible for long enough - see ProcessPendingBoardStart in
  // Boggle.cpp.
  struct PendingBoardStart
  {
    Engine::Grid2D<char> Board;
    std::chrono::steady_clock::time_point QueuedTime;
  };

  struct AppData
  {
    // Boards with fewer dice than this solve fast enough that the "Working" popup dance
    // (see ProcessPendingBoardStart in Boggle.cpp) is unnecessary overhead - NewGameBoard
    // is just called directly instead.
    static constexpr int DirectStartDiceThreshold = 10000;

    Engine::Grid2D<char> BoggleLayout;
    BoggleResult Result;
    Engine::IWorkerPool * pWorkerPool;
    Engine::IDictionary const * pDictionary;
    UIData UI;
    BoardType CurrentBoardType;
    std::optional<PendingBoardStart> PendingBoard;
  };
}

#endif