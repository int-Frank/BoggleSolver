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

  // Sets the grid and starts the word search running in the background - does not block.
  // Progress is polled once per frame from DoFrame; pData->pActiveSearch is non-null until
  // it completes, at which point Result is populated with the timed outcome. Calling this
  // again while a search is already running is a no-op (the UI shouldn't allow it anyway).
  void NewGameBoard(Engine::Grid2D<char> board, AppData * pData);
}

#endif