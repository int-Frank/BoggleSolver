#ifndef CHARACTERGRIDGENERATOR_H
#define CHARACTERGRIDGENERATOR_H

#include "Grid2D.h"

namespace Engine
{
  Grid2D<char> GenerateClassicBoggleGrid(unsigned int * pSeed);
  Grid2D<char> GenerateModernBoggleGrid(unsigned int * pSeed);
  Grid2D<char> GenerateBigBoggle(unsigned int * pSeed);
  Grid2D<char> GenerateSuperBoggle(unsigned int * pSeed);
  Grid2D<char> GenerateCustomBoggle(unsigned int width, unsigned int height, unsigned int * pSeed);
}

#endif