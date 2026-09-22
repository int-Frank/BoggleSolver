#ifndef APPLICATIONAPI_H
#define APPLICATIONAPI_H

#include "AppData.h"
#include "WordData.h"
#include "IWorkerPool.h"

namespace App
{
  // Add to Window.cpp - or a name befitting this code - it's kind of all the base framework code
  bool InitApplication();
  void BeginFrame();
  bool IsDone(); // Do we want to exit?
  bool IsMinimised();
  void EndFrame();
  bool Shutdown();

  // Add to Boggle.cpp - or a name befitting this code. It's essentially the application code
  void DoFrame(AppData *pData);
  bool DestroyAppData(AppData ** ppData);
  bool InitAppData(AppData ** ppData);

  // Sets the grid, runs the word search to completion and records the timed result in pData.
  void NewGameBoard(Engine::Grid2D<char> board, AppData * pData);
}

#endif