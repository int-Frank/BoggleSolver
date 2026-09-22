#ifndef APPDATA_H
#define APPDATA_H

#include "Grid2D.h"
#include "WordData.h"
#include "IWorkerPool.h"
#include "IDictionary.h"

namespace App
{
  struct BoggleResult
  {
    std::vector<Engine::WordData> Words;
    double Time;
  };

  struct AppData
  {
    Engine::Grid2D<char> BoggleLayout;
    BoggleResult Result;
    Engine::IWorkerPool * pWorkerPool;
    Engine::IDictionary const * pDictionary;
  };
}

#endif