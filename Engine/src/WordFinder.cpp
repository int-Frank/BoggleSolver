
#include <array>
#include <unordered_map>
#include <thread>

#include "WordFinder.h"
#include "Coord.h"
#include "IWorkerPool.h"

namespace Engine
{
  struct WordFinderContext
  {
    Grid2D<char> const * pCharacterGrid;
    Grid2D<bool> Visited;
    int CurrentLength;
    std::vector<char> CharacterBlock;
    std::vector<Coord> PathBlock;
    IDictionary::Context const * pDictionaryContext;
    IDictionary const * pDictionary;
    std::vector<WordData> CapturedWords;
    std::unordered_map<std::string, int> CapturedWordsMap;

    WordFinderContext(Grid2D<char> const & characterGrid, IDictionary const * pDictionary)
      : pCharacterGrid(&characterGrid)
      , Visited(characterGrid.Width(), characterGrid.Height(), false)
      , CurrentLength(0)
      , CharacterBlock(characterGrid.Width() * characterGrid.Height())
      , PathBlock(characterGrid.Width() * characterGrid.Height())
      , pDictionaryContext(nullptr)
      , pDictionary(pDictionary)
    {
    }
  };

  static void Process(Coord coord, WordFinderContext * pContext);
  static std::array<Coord, 8> GetSurroundingCoords(Coord coord);
  static std::string_view GetWord(WordFinderContext const * pContext);
  static void CaptureCurrentWord(WordFinderContext * pContext);

  std::vector<WordData> FindWords(Grid2D<char> const & characterGrid,
                                  int seedX,
                                  int seedY,
                                  IDictionary const * pDictionary)
  {
    if (seedX < 0 || seedX >= characterGrid.Width() || seedY < 0 || seedY >= characterGrid.Height())
    {
      return std::vector<WordData>();
    }

    WordFinderContext context(characterGrid, pDictionary);

    Coord seed
    {
      seedX, seedY
    };

    Process(seed, &context);

    return context.CapturedWords;
  }

  static void Process(Coord coord, WordFinderContext *pContext)
  {
    if (coord.X < 0 || coord.X >= pContext->pCharacterGrid->Width() ||
      coord.Y < 0 || coord.Y >= pContext->pCharacterGrid->Height() ||
        pContext->Visited.Get(coord))
    {
      return;
    }

    char currentCharacter = pContext->pCharacterGrid->Get(coord);
    pContext->CharacterBlock[pContext->CurrentLength] = currentCharacter;
    pContext->PathBlock[pContext->CurrentLength] = coord;
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
        CaptureCurrentWord(pContext);
      }
    }

    pContext->Visited.Set(coord, true);
    auto surroundingCoords = GetSurroundingCoords(coord);
    for (auto nextCoord : surroundingCoords)
    {
      Process(nextCoord, pContext);
    }
    pContext->CurrentLength--;
    pContext->Visited.Set(coord, false);
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

  static void CaptureCurrentWord(WordFinderContext * pContext)
  {
    std::string word(GetWord(pContext));
    std::vector<Coord> path(pContext->PathBlock.begin(), pContext->PathBlock.begin() + pContext->CurrentLength);

    auto it = pContext->CapturedWordsMap.find(word);
    if (it == pContext->CapturedWordsMap.end())
    {
      int index = (int)pContext->CapturedWords.size();
      pContext->CapturedWordsMap.emplace(word, index);

      WordData data;
      data.Word = word;
      data.Locations.push_back(std::move(path));
      pContext->CapturedWords.push_back(std::move(data));
      return;
    }

    pContext->CapturedWords[it->second].Locations.push_back(std::move(path));
  }

  struct MultiSeedContext
  {
    std::vector<WordData> * pResults;
    std::unordered_map<std::string, int> * pWordEntries;
  };

  struct MultiSeedTask
  {
    Grid2D<char> const * pGrid;
    Coord Seed;
    IDictionary const * pDictionary;
    MultiSeedContext const * pContext;
    std::vector<WordData> Result;
  };

  static void RunMultiSeedTask(void * pUserData) noexcept
  {
    MultiSeedTask * pTask = static_cast<MultiSeedTask *>(pUserData);
    pTask->Result = FindWords(*pTask->pGrid, pTask->Seed.X, pTask->Seed.Y, pTask->pDictionary);
  }

  // Runs on the main thread via IWorkerPool::DoPostWork, so pResults/pWordEntries need no locking.
  static void MergeMultiSeedTask(void * pUserData) noexcept
  {
    MultiSeedTask * pTask = static_cast<MultiSeedTask *>(pUserData);
    MultiSeedContext const * pContext = pTask->pContext;

    for (WordData & wordData : pTask->Result)
    {
      auto it = pContext->pWordEntries->find(wordData.Word);
      if (it == pContext->pWordEntries->end())
      {
        int index = (int)pContext->pResults->size();
        pContext->pWordEntries->emplace(wordData.Word, index);
        pContext->pResults->push_back(std::move(wordData));
        continue;
      }

      WordData & existing = (*pContext->pResults)[it->second];
      for (auto const & location : wordData.Locations)
        existing.Locations.push_back(location);
    }
  }

  static void FreeMultiSeedTask(void * pUserData) noexcept
  {
    delete static_cast<MultiSeedTask *>(pUserData);
  }

  static bool IsReversePath(std::vector<Coord> const & a, std::vector<Coord> const & b)
  {
    if (a.size() != b.size())
      return false;

    size_t count = a.size();
    for (size_t i = 0; i < count; i++)
    {
      Coord const & fromEnd = b[count - 1 - i];
      if (a[i].X != fromEnd.X || a[i].Y != fromEnd.Y)
        return false;
    }

    return true;
  }

  static std::vector<WordData> RemovePallindromes(std::vector<WordData> results)
  {
    // For each word, find any location sequences which are the same forward as backward and elimate one
    for (WordData & wordData : results)
    {
      std::vector<std::vector<Coord>> keptLocations;
      keptLocations.reserve(wordData.Locations.size());

      for (auto & path : wordData.Locations)
      {
        bool isDuplicateReverse = false;
        for (auto const & keptPath : keptLocations)
        {
          if (IsReversePath(path, keptPath))
          {
            isDuplicateReverse = true;
            break;
          }
        }

        if (!isDuplicateReverse)
          keptLocations.push_back(std::move(path));
      }

      wordData.Locations = std::move(keptLocations);
    }

    return results;
  }

  static std::vector<WordData> Clean(std::vector<WordData> results)
  {
    return RemovePallindromes(results);
  }

  std::vector<WordData> FindWords(Grid2D<char> const & grid,
    int threadCount,
    IDictionary const * pDictionary)
  {
    if (threadCount < 1)
    {
      threadCount = (int)std::thread::hardware_concurrency() - 1;
      if (threadCount < 1)
        threadCount = 1;
    }

    IWorkerPool * pWorkerPool = IWorkerPool::Create(threadCount);

    std::vector<WordData> results;
    std::unordered_map<std::string, int> wordEntries;

    if (pWorkerPool == nullptr)
      return results;

    MultiSeedContext context{ &results, &wordEntries };

    for (int y = 0; y < grid.Height(); y++)
    {
      for (int x = 0; x < grid.Width(); x++)
      {
        MultiSeedTask * pTask = new MultiSeedTask{ &grid, Coord{ x, y }, pDictionary, &context, {} };

        if (pWorkerPool->AddTask(RunMultiSeedTask, pTask, FreeMultiSeedTask, MergeMultiSeedTask) != ErrorCode::None)
          delete pTask;
      }
    }

    for (;;)
    {
      uint32_t processed = pWorkerPool->DoPostWork();
      if (processed == 0)
      {
        if (!pWorkerPool->HasActiveWorkers())
          break;
        std::this_thread::yield();
      }
    }

    delete pWorkerPool;

    return Clean(results);
  }
}