#ifndef IWORDSEARCH_H
#define IWORDSEARCH_H

#include <string>

#include "Grid2D.h"
#include "WordData.h"

namespace Engine
{
  class IWorkerPool;
  class IDictionary;

  class IWordSearch
  {
  public:

    static IWordSearch * Begin(Grid2D<char> const * pGrid,
                               IWorkerPool * pWorkerPool,
                               IDictionary const * pDictionary);

    // Do not delete (or let go out of scope) an IWordSearch until IsDone()
    // returns true.
    virtual ~IWordSearch() = default;

    virtual bool IsDone() const = 0;

    // Valid only once IsDone() is true.
    virtual std::vector<WordData> TakeResult() = 0;
  };
}

#endif