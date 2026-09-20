#ifndef IWORDFINDER_H
#define IWORDFINDER_H

#include <string>

#include "Grid2D.h"
#include "IDictionary.h"
#include "WordData.h"

namespace Engine
{
  std::vector<WordData> FindWords(Grid2D<char> const & grid,
                                  int seedX,
                                  int seedY,
                                  IDictionary const * pDictionary);

  
  std::vector<WordData> FindWords(Grid2D<char> const & grid,
                                  int threadCount,
                                  IDictionary const * pDictionary);
}

#endif