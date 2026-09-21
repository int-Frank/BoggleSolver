
#include <array>
#include <random>
#include <algorithm>

#include "CharacterGridGenerator.h"

namespace Engine
{
  struct Dice
  {
    std::array<char, 6> Faces;
  };

  static const std::array<Dice, 16> ClassicDice
  {
    // Example
    Dice{{'a', 'a', 'c', 'i', 'o', 't'}},
    Dice{{'a', 'a', 'e', 'e', 'g', 'n'}},
    Dice{{'a', 'b', 'i', 'l', 't', 'y'}},
    Dice{{'a', 'b', 'b', 'j', 'o', 'o'}},
    Dice{{'a', 'b', 'j', 'm', 'o', 'q'}},
    Dice{{'a', 'c', 'h', 'o', 'p', 's'}},
    Dice{{'a', 'c', 'd', 'e', 'm', 'p'}},
    Dice{{'a', 'f', 'f', 'k', 'p', 's'}},
    Dice{{'a', 'c', 'e', 'l', 'r', 's'}},
    Dice{{'a', 'o', 'o', 't', 't', 'w'}},
    Dice{{'a', 'd', 'e', 'n', 'v', 'z'}},
    Dice{{'c', 'i', 'm', 'o', 't', 'u'}},
    Dice{{'a', 'h', 'm', 'o', 'r', 's'}},
    Dice{{'d', 'e', 'i', 'l', 'r', 'x'}},
    Dice{{'b', 'i', 'f', 'o', 'r', 'x'}},
    Dice{{'d', 'e', 'l', 'r', 'v', 'y'}}
  };

  static const std::array<Dice, 16> ModernDice
  {
    Dice{{'a', 'a', 'e', 'e', 'e', 'e'}},
    Dice{{'a', 'd', 'e', 'n', 'n', 'n'}},
    Dice{{'a', 'e', 'e', 'g', 'm', 'u'}},
    Dice{{'a', 'f', 'i', 'r', 's', 'y'}},
    Dice{{'b', 'j', 'k', 'q', 'x', 'z'}},
    Dice{{'c', 'e', 'i', 'l', 'p', 't'}},
    Dice{{'c', 'e', 'i', 'p', 's', 't'}},
    Dice{{'d', 'h', 'h', 'l', 'o', 'r'}},
    Dice{{'d', 'h', 'l', 'n', 'o', 'r'}},
    Dice{{'d', 'h', 'l', 'n', 'o', 'r'}},
    Dice{{'e', 'i', 'i', 'i', 't', 't'}},
    Dice{{'e', 'm', 'o', 't', 't', 't'}},
    Dice{{'e', 'n', 's', 's', 's', 'u'}},
    Dice{{'f', 'i', 'p', 'r', 's', 'y'}},
    Dice{{'g', 'o', 'r', 'r', 'v', 'w'}},
    Dice{{'n', 'o', 'o', 't', 'u', 'w'}}
  };

  static const std::array<Dice, 25> BigDice
  {
    Dice{{'a', 'a', 'a', 'f', 'r', 's'}},
    Dice{{'a', 'a', 'e', 'e', 'e', 'e'}},
    Dice{{'a', 'a', 'f', 'i', 'r', 's'}},
    Dice{{'a', 'd', 'e', 'n', 'n', 'n'}},
    Dice{{'a', 'e', 'e', 'e', 'e', 'm'}},
    Dice{{'a', 'e', 'e', 'g', 'm', 'u'}},
    Dice{{'a', 'e', 'g', 'm', 'n', 'n'}},
    Dice{{'a', 'f', 'i', 'r', 's', 'y'}},
    Dice{{'b', 'j', 'k', 'q', 'x', 'z'}},
    Dice{{'c', 'c', 'n', 's', 't', 'w'}},
    Dice{{'c', 'e', 'i', 'i', 'l', 't'}},
    Dice{{'c', 'e', 'i', 'l', 'p', 't'}},
    Dice{{'c', 'e', 'i', 'p', 's', 't'}},
    Dice{{'d', 'h', 'h', 'n', 'o', 't'}},
    Dice{{'d', 'h', 'h', 'l', 'o', 'r'}},
    Dice{{'d', 'h', 'l', 'n', 'o', 'r'}},
    Dice{{'d', 'd', 'l', 'n', 'o', 'r'}},
    Dice{{'e', 'i', 'i', 'i', 't', 't'}},
    Dice{{'e', 'm', 'o', 't', 't', 't'}},
    Dice{{'e', 'n', 's', 's', 's', 'u'}},
    Dice{{'f', 'i', 'p', 'r', 's', 'y'}},
    Dice{{'g', 'o', 'r', 'r', 'v', 'w'}},
    Dice{{'h', 'i', 'p', 'r', 'r', 'y'}},
    Dice{{'n', 'o', 'o', 't', 'u', 'w'}},
    Dice{{'o', 'o', 'o', 't', 't', 'u'}}
  };

  static const std::array<Dice, 36> SuperDice
  {
    Dice{{'a', 'a', 'a', 'f', 'r', 's'}},
    Dice{{'a', 'a', 'e', 'e', 'e', 'e'}},
    Dice{{'a', 'a', 'e', 'e', 'o', 'o'}},
    Dice{{'a', 'a', 'f', 'i', 'r', 's'}},
    Dice{{'a', 'b', 'd', 'e', 'i', 'o'}},
    Dice{{'a', 'd', 'e', 'n', 'n', 'n'}},
    Dice{{'a', 'e', 'e', 'e', 'e', 'm'}},
    Dice{{'a', 'e', 'e', 'g', 'm', 'u'}},
    Dice{{'a', 'e', 'g', 'm', 'n', 'n'}},
    Dice{{'a', 'e', 'i', 'l', 'm', 'n'}},
    Dice{{'a', 'e', 'i', 'n', 'o', 'u'}},
    Dice{{'a', 'f', 'i', 'r', 's', 'y'}},
    Dice{{'b', 'b', 'j', 'k', 'x', 'z'}},
    Dice{{'c', 'c', 'e', 'n', 's', 't'}},
    Dice{{'c', 'd', 'd', 'l', 'n', 'n'}},
    Dice{{'c', 'e', 'i', 'i', 't', 't'}},
    Dice{{'c', 'e', 'i', 'p', 's', 't'}},
    Dice{{'c', 'f', 'g', 'n', 'u', 'y'}},
    Dice{{'d', 'd', 'h', 'n', 'o', 't'}},
    Dice{{'d', 'h', 'h', 'l', 'o', 'r'}},
    Dice{{'d', 'h', 'h', 'n', 'o', 'w'}},
    Dice{{'d', 'h', 'l', 'n', 'o', 'r'}},
    Dice{{'e', 'h', 'i', 'l', 'r', 's'}},
    Dice{{'e', 'i', 'i', 'l', 's', 't'}},
    Dice{{'e', 'i', 'l', 'p', 's', 't'}},
    Dice{{'e', 'm', 't', 't', 't', 'o'}},
    Dice{{'e', 'n', 's', 's', 's', 'u'}},
    Dice{{'g', 'o', 'r', 'r', 'v', 'w'}},
    Dice{{'h', 'i', 'r', 's', 't', 'v'}},
    Dice{{'h', 'o', 'p', 'r', 's', 't'}},
    Dice{{'i', 'p', 'r', 's', 'y', 'y'}},
    Dice{{'j', 'k', 'q', 'w', 'x', 'z'}},
    Dice{{'n', 'o', 'o', 't', 'u', 'w'}},
    Dice{{'o', 'o', 'o', 't', 't', 'u'}},
    Dice{{'a', 'e', 'i', 'l', 'r', 't'}},
    Dice{{'d', 'e', 'i', 'l', 'n', 's'}}
  };

  static char RollDice(Dice dice, std::mt19937 & rng)
  {
    std::uniform_int_distribution<size_t> faceDist(0, dice.Faces.size() - 1);
    return dice.Faces[faceDist(rng)];
  }

  template<size_t N>
  static Grid2D<char> GenerateGridFromDice(const std::array<Dice, N> & dice, int width, int height, unsigned int * pSeed)
  {
    std::mt19937 rng = pSeed == nullptr ? std::mt19937(std::random_device{}()) : std::mt19937(*pSeed);

    std::array<Dice, N> shuffled = dice;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    Grid2D<char> grid(width, height, 'a');

    for (size_t i = 0; i < N; i++)
    {
      int x = (int)(i % width);
      int y = (int)(i / width);

      char c = RollDice(shuffled[i], rng);

      grid.Set(Coord(x, y), c);
    }

    return grid;
  }

  Grid2D<char> GenerateClassicBoggleGrid(unsigned int * pSeed)
  {
    return GenerateGridFromDice(ClassicDice, 4, 4, pSeed);
  }

  Grid2D<char> GenerateModernBoggleGrid(unsigned int * pSeed)
  {
    return GenerateGridFromDice(ModernDice, 4, 4, pSeed);
  }

  Grid2D<char> GenerateBigBoggle(unsigned int * pSeed)
  {
    return GenerateGridFromDice(BigDice, 5, 5, pSeed);
  }

  Grid2D<char> GenerateSuperBoggle(unsigned int * pSeed)
  {
    return GenerateGridFromDice(SuperDice, 6, 6, pSeed);
  }

  static std::array<int, 26> BuildLetterWeights()
  {
    std::array<int, 26> weights{};

    auto tally = [&weights](const auto & diceSet)
      {
        for (const auto & die : diceSet)
          for (char c : die.Faces)
            weights[c - 'a']++;
      };

    tally(ClassicDice);
    tally(ModernDice);
    tally(BigDice);
    tally(SuperDice);

    return weights;
  }

  static char RandomWeightedLetter(std::mt19937 & rng)
  {
    static const std::array<int, 26> weights = BuildLetterWeights();
    static std::discrete_distribution<int> dist(weights.begin(), weights.end());
    return static_cast<char>('a' + dist(rng));
  }

  Grid2D<char> GenerateCustomBoggle(unsigned int width, unsigned int height, unsigned int * pSeed)
  {
    width = width == 0 ? 1 : width;
    height = height == 0 ? 1 : height;

    std::mt19937 rng = pSeed == nullptr ? std::mt19937(std::random_device{}()) : std::mt19937(*pSeed);

    Grid2D<char> grid(width, height, 'a');

    for (unsigned int x = 0; x < width; x++)
    {
      for (unsigned int y = 0; y < height; y++)
      {
        char c = RandomWeightedLetter(rng);
        grid.Set(Coord(x, y), c);
      }
    }

    return grid;
  }
}