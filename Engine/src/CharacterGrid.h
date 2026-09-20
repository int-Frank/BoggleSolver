#ifndef CHARACTERGRID_H
#define CHARACTERGRID_H

#include <vector>

namespace Engine
{
  class CharacterGrid
  {
    int m_width;
    int m_height;
    std::vector<char> m_characters;

  public:

    CharacterGrid(int width, int height); // Invalid dimensions are set to 1. Characters are initialised to 'a'

    ~CharacterGrid();

    int Width() const;
    int Height() const;
    char Get(int x, int y) const; // returns 0 if invalid coord
    void Set(int x, int y, char c); // does nothing if invalid coord
  };
}

#endif