#ifndef GRID2D_H
#define GRID2D_H

#include <vector>
#include <stdexcept>

#include "Coord.h"

namespace Engine
{
  template<typename T>
  class Grid2D
  {
    int m_width;
    int m_height;
    std::vector<T> m_elements;

  public:

    // Throws std::invalid_argument if width or height is less than 1
    Grid2D(int width, int height, T defaultValue)
    {
      if (width < 1 || height < 1)
        throw std::invalid_argument("Grid2D dimensions must be at least 1");

      m_width = width;
      m_height = height;
      m_elements.assign((size_t)m_width * (size_t)m_height, defaultValue);
    }

    int Width() const
    {
      return m_width;
    }

    int Height() const
    {
      return m_height;
    }

    // Throws std::out_of_range if invalid coordinates
    T Get(Coord coord) const
    {
      ValidateCoord(coord);
      return m_elements[(size_t)coord.Y * m_width + coord.X];
    }

    // Throws std::out_of_range if invalid coordinates
    void Set(Coord coord, T value)
    {
      ValidateCoord(coord);
      m_elements[(size_t)coord.Y * m_width + coord.X] = value;
    }

  private:

    void ValidateCoord(Coord coord) const
    {
      if (coord.X < 0 || coord.X >= m_width || coord.Y < 0 || coord.Y >= m_height)
        throw std::out_of_range("Grid2D coordinates out of range");
    }
  };
}

#endif