#ifndef COORD_H
#define COORD_H

namespace Engine
{
  struct Coord
  {
    int X;
    int Y;

    Coord()
    {
      X = 0;
      Y = 0;
    }

    Coord(int x, int y)
    {
      X = x;
      Y = y;
    }

    bool operator==(Coord const & other) const
    {
      return X == other.X && Y == other.Y;
    }
  };
}

#endif