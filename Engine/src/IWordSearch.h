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

    // Do not delete (or let go out of scope) an IWordSearch while any seed
    // task may still be in flight, or while anything holds a pointer
    // returned by GetResult().
    virtual ~IWordSearch() = default;

    // Returns nullptr while the search is still running. Once done, returns
    // a pointer to the results, owned by this IWordSearch and valid for as
    // long as it is - copy out via *GetResult() if you need it to outlive
    // the search.
    virtual std::vector<WordData> const * GetResult() const = 0;
  };
}

#endif