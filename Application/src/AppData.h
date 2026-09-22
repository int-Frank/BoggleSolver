#ifndef APPDATA_H
#define APPDATA_H

#include "Grid2D.h"
#include "WordData.h"
#include "IWorkerPool.h"
#include "IDictionary.h"

#include <chrono>
#include <string>
#include <vector>

namespace App
{
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
  };

  struct AppData
  {
    Engine::Grid2D<char> BoggleLayout;
    BoggleResult Result;
    Engine::IWorkerPool * pWorkerPool;
    Engine::IDictionary const * pDictionary;
    UIData UI;
  };
}

#endif