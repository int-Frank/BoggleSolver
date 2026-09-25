#ifndef WORDDATA_H
#define WORDDATA_H

#include <vector>
#include <string>

#include "Coord.h"

namespace Engine
{
  struct WordData
  {
    std::string Word;
    std::vector<std::vector<Coord>> Locations;
  };
}

#endif