#ifndef APPDATA_H
#define APPDATA_H

#include "Grid2D.h"
#include "WordData.h"
#include "IWorkerPool.h"
#include "IDictionary.h"
#include "IWordSearch.h"

#include <chrono>
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
    std::vector<Engine::Coord> SelectedPath;
    std::chrono::steady_clock::time_point SelectionStartTime;

    // Consumed (and cleared) by the board view the first time it draws after a
    // selection is made, so it scrolls the selection into view exactly once and
    // doesn't fight any scrolling the user does afterward.
    bool PendingScrollToSelection;
  };

  struct AppData
  {
    Engine::Grid2D<char> BoggleLayout;
    BoggleResult Result;
    Engine::IWorkerPool * pWorkerPool;
    Engine::IDictionary const * pDictionary;
    UIData UI;
    BoardType CurrentBoardType;

    // Non-null while NewGameBoard's word search is running in the background - see
    // NewGameBoard's comment in ApplicationAPI.h for how this is driven.
    Engine::IWordSearch * pActiveSearch;
    std::chrono::steady_clock::time_point SearchStartTime;
  };
}

#endif