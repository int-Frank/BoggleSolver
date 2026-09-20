#ifndef IWORDFINDER_H
#define IWORDFINDER_H

#include <vector>
#include <string>

#include "Grid2D.h"
#include "IDictionary.h"

namespace Engine
{
  std::set<std::string> FindWords(Grid2D<char> const & grid,
                                     int seedX,
                                     int seedY,
                                     IDictionary const * pDictionary);
}

#endif