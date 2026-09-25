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
}

#endif