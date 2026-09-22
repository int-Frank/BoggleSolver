#ifndef APPLICATIONAPI_H
#define APPLICATIONAPI_H

#include "AppData.h"
#include "WordData.h"
#include "IWorkerPool.h"

namespace App
{
  bool InitApplication();
  void BeginFrame();
  bool IsDone();
  bool IsMinimised();
  void EndFrame();
  bool Shutdown();

  void DoFrame(AppData *pData);
  bool DestroyAppData(AppData ** ppData);
  bool InitAppData(AppData ** ppData);

  // Sets the grid, runs the word search to completion and records the timed result in pData.
  void NewGameBoard(Engine::Grid2D<char> board, AppData * pData);
}

#endif