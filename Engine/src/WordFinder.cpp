
#include <array>

#include "WordFinder.h"

namespace Engine
{
  struct Coord
  {
    int X;
    int Y;
  };

  struct WordFinderContext
  {
    Grid2D<char> const * pCharacterGrid;
    Grid2D<bool> Visited;
    int CurrentLength;
    std::vector<char> CharacterBlock;
    IDictionary::Context const * pDictionaryContext;
    IDictionary const * pDictionary;
    std::set<std::string> FoundWords;

    WordFinderContext(Grid2D<char> const & characterGrid, IDictionary const * pDictionary)
      : pCharacterGrid(&characterGrid)
      , Visited(characterGrid.Width(), characterGrid.Height(), false)
      , CurrentLength(0)
      , CharacterBlock(characterGrid.Width() * characterGrid.Height())
      , pDictionaryContext(nullptr)
      , pDictionary(pDictionary)
    {
    }
  };

  static void Process(Coord coord, WordFinderContext * pContext);
  static std::array<Coord, 8> GetSurroundingCoords(Coord coord);
  static std::string_view GetWord(WordFinderContext const * pContext);

  std::set<std::string> FindWords(Grid2D<char> const & characterGrid,
                                     int seedX,
                                     int seedY,
                                     IDictionary const * pDictionary)
  {
    if (seedX < 0 || seedX >= characterGrid.Width() || seedY < 0 || seedY >= characterGrid.Height())
    {
      return std::set<std::string>();
    }

    WordFinderContext context(characterGrid, pDictionary);

    Coord seed
    {
      seedX, seedY
    };

    Process(seed, &context);

    return context.FoundWords;
  }

  static void Process(Coord coord, WordFinderContext *pContext)
  {
    if (coord.X < 0 || coord.X >= pContext->pCharacterGrid->Width() ||
      coord.Y < 0 || coord.Y >= pContext->pCharacterGrid->Height() ||
        pContext->Visited.Get(coord.X, coord.Y))
    {
      return;
    }

    char currentCharacter = pContext->pCharacterGrid->Get(coord.X, coord.Y);
    pContext->CharacterBlock[pContext->CurrentLength] = currentCharacter;
    pContext->CurrentLength++;
    uint32_t wordCount = 1; // Just needs to be non-zero.

    if (pContext->CurrentLength == 1)
    {
      char c = pContext->CharacterBlock[0];
      wordCount = pContext->pDictionary->WordCount(c);
    }

    else if (pContext->CurrentLength == 2)
    {
      char c0 = pContext->CharacterBlock[0];
      char c1 = pContext->CharacterBlock[1];
      wordCount = pContext->pDictionary->WordCount(c0, c1);
    }

    else if (pContext->CurrentLength == 3)
    {
      char c0 = pContext->CharacterBlock[0];
      char c1 = pContext->CharacterBlock[1];
      char c2 = pContext->CharacterBlock[2];
      wordCount = pContext->pDictionary->WordCount(c0, c1, c2, &pContext->pDictionaryContext);
    }

    if (wordCount == 0)
    {
      pContext->CurrentLength--;
      return;
    }

    if (pContext->CurrentLength >= 3)
    {
      auto word = GetWord(pContext);
      if (pContext->pDictionary->IsWord(word, pContext->pDictionaryContext))
      {
        std::string str(word);
        pContext->FoundWords.insert(str);
      }
    }

    pContext->Visited.Set(coord.X, coord.Y, true);
    auto surroundingCoords = GetSurroundingCoords(coord);
    for (auto nextCoord : surroundingCoords)
    {
      Process(nextCoord, pContext);
    }
    pContext->CurrentLength--;
    pContext->Visited.Set(coord.X, coord.Y, false);
  }

  static std::array<Coord, 8> GetSurroundingCoords(Coord coord)
  {
    return
    {
      Coord{ coord.X - 1, coord.Y - 1 },
      Coord{ coord.X,     coord.Y - 1 },
      Coord{ coord.X + 1, coord.Y - 1 },
      Coord{ coord.X - 1, coord.Y },
      Coord{ coord.X + 1, coord.Y },
      Coord{ coord.X - 1, coord.Y + 1 },
      Coord{ coord.X,     coord.Y + 1 },
      Coord{ coord.X + 1, coord.Y + 1 }
    };
  }

  static std::string_view GetWord(WordFinderContext const *pContext)
  {
    return std::string_view(pContext->CharacterBlock.data(), pContext->CurrentLength);
  }
}