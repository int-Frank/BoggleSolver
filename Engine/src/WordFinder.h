#ifndef IWORDFINDER_H
#define IWORDFINDER_H

#include <string>

#include "Grid2D.h"
#include "IDictionary.h"
#include "WordData.h"

namespace Engine
{
  class IWorkerPool;

  class IFindWordsTask
  {
  public:

    static IFindWordsTask * Begin(Grid2D<char> const * pGrid,
                                  IWorkerPool * pWorkerPool,
                                  IDictionary const * pDictionary);

    // Do not delete (or let go out of scope) an IFindWordsTask until IsDone()
    // returns true.
    virtual ~IFindWordsTask() = default;

    virtual bool IsDone() const = 0;

    // Valid only once IsDone() is true.
    virtual std::vector<WordData> TakeResult() = 0;
  };
}

#endif