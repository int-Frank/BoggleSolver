
#include <array>
#include <atomic>
#include <unordered_map>

#include "WordFinder.h"
#include "Coord.h"
#include "IWorkerPool.h"

namespace Engine
{
  class FindWordsTask : public IFindWordsTask
  {
  public:
    FindWordsTask(Grid2D<char> const * pGrid, IWorkerPool * pWorkerPool, IDictionary const * pDictionary);

    bool IsDone() const override;
    std::vector<WordData> TakeResult() override;

  private:
    struct SeedContext
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

      SeedContext(Grid2D<char> const * pCharacterGrid, IDictionary const * pDictionary)
        : pCharacterGrid(pCharacterGrid)
        , Visited(pCharacterGrid->Width(), pCharacterGrid->Height(), false)
        , CurrentLength(0)
        , CharacterBlock(pCharacterGrid->Width() * pCharacterGrid->Height())
        , PathBlock(pCharacterGrid->Width() * pCharacterGrid->Height())
        , pDictionaryContext(nullptr)
        , pDictionary(pDictionary)
      {
      }
    };

    struct SeedTask
    {
      Grid2D<char> const * pGrid;
      Coord Seed;
      IDictionary const * pDictionary;
      FindWordsTask * pOwner;
      std::vector<WordData> Result;
    };

    static std::vector<WordData> FindWordsForSeed(Grid2D<char> const * pGrid,
                                                    int seedX,
                                                    int seedY,
                                                    IDictionary const * pDictionary);
    static void ProcessSeed(Coord coord, SeedContext * pContext);
    static std::array<Coord, 8> GetSurroundingCoords(Coord coord);
    static std::string_view GetWord(SeedContext const * pContext);
    static void CaptureCurrentWord(SeedContext * pContext);

    static void RunSeedTask(void * pUserData) noexcept;
    static void MergeSeedTask(void * pUserData) noexcept;
    static void FreeSeedTask(void * pUserData) noexcept;

    static bool IsReversePath(std::vector<Coord> const & a, std::vector<Coord> const & b);
    static std::vector<WordData> RemovePalindromes(std::vector<WordData> results);

    void MergeResult(std::vector<WordData> & seedResult);
    void OnSeedComplete();

    std::atomic<int> m_pending{ 0 };
    std::atomic<bool> m_done{ false };
    std::vector<WordData> m_results;
    std::unordered_map<std::string, int> m_wordEntries;
  };

  IFindWordsTask * IFindWordsTask::Begin(Grid2D<char> const * pGrid,
                                          IWorkerPool * pWorkerPool,
                                          IDictionary const * pDictionary)
  {
    return new FindWordsTask(pGrid, pWorkerPool, pDictionary);
  }

  FindWordsTask::FindWordsTask(Grid2D<char> const * pGrid, IWorkerPool * pWorkerPool, IDictionary const * pDictionary)
  {
    int totalSeeds = pGrid->Width() * pGrid->Height();
    m_pending.store(totalSeeds, std::memory_order_relaxed);

    if (pWorkerPool == nullptr || totalSeeds == 0)
    {
      m_pending.store(0, std::memory_order_relaxed);
      m_done.store(true, std::memory_order_release);
      return;
    }

    for (int y = 0; y < pGrid->Height(); y++)
    {
      for (int x = 0; x < pGrid->Width(); x++)
      {
        SeedTask * pTask = new SeedTask{ pGrid, Coord{ x, y }, pDictionary, this, {} };

        if (pWorkerPool->AddTask(RunSeedTask, pTask, FreeSeedTask, MergeSeedTask) != ErrorCode::None)
        {
          delete pTask;
          // This seed will never run/merge, so account for it here or
          // m_pending would never reach zero and the task would hang forever.
          OnSeedComplete();
        }
      }
    }
  }

  bool FindWordsTask::IsDone() const
  {
    return m_done.load(std::memory_order_acquire);
  }

  std::vector<WordData> FindWordsTask::TakeResult()
  {
    return std::move(m_results);
  }

  void FindWordsTask::OnSeedComplete()
  {
    if (m_pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
      m_results = RemovePalindromes(std::move(m_results));
      m_done.store(true, std::memory_order_release);
    }
  }

  void FindWordsTask::MergeResult(std::vector<WordData> & seedResult)
  {
    for (WordData & wordData : seedResult)
    {
      auto it = m_wordEntries.find(wordData.Word);
      if (it == m_wordEntries.end())
      {
        int index = (int)m_results.size();
        m_wordEntries.emplace(wordData.Word, index);
        m_results.push_back(std::move(wordData));
        continue;
      }

      WordData & existing = m_results[it->second];
      for (auto const & location : wordData.Locations)
        existing.Locations.push_back(location);
    }
  }

  void FindWordsTask::RunSeedTask(void * pUserData) noexcept
  {
    SeedTask * pTask = static_cast<SeedTask *>(pUserData);
    pTask->Result = FindWordsForSeed(pTask->pGrid, pTask->Seed.X, pTask->Seed.Y, pTask->pDictionary);
  }

  // Runs on whichever single thread drives the shared IWorkerPool's DoPostWork(),
  // so m_results/m_wordEntries need no locking - as long as only that one thread
  // ever calls DoPostWork() on the pool, merges for this task never overlap.
  void FindWordsTask::MergeSeedTask(void * pUserData) noexcept
  {
    SeedTask * pTask = static_cast<SeedTask *>(pUserData);
    FindWordsTask * pOwner = pTask->pOwner;

    pOwner->MergeResult(pTask->Result);
    pOwner->OnSeedComplete();
  }

  void FindWordsTask::FreeSeedTask(void * pUserData) noexcept
  {
    delete static_cast<SeedTask *>(pUserData);
  }

  std::vector<WordData> FindWordsTask::FindWordsForSeed(Grid2D<char> const * pCharacterGrid,
                                                          int seedX,
                                                          int seedY,
                                                          IDictionary const * pDictionary)
  {
    if (seedX < 0 || seedX >= pCharacterGrid->Width() || seedY < 0 || seedY >= pCharacterGrid->Height())
    {
      return std::vector<WordData>();
    }

    SeedContext context(pCharacterGrid, pDictionary);

    Coord seed
    {
      seedX, seedY
    };

    ProcessSeed(seed, &context);

    return context.CapturedWords;
  }

  void FindWordsTask::ProcessSeed(Coord coord, SeedContext * pContext)
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
      ProcessSeed(nextCoord, pContext);
    }
    pContext->CurrentLength--;
    pContext->Visited.Set(coord, false);
  }

  std::array<Coord, 8> FindWordsTask::GetSurroundingCoords(Coord coord)
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

  std::string_view FindWordsTask::GetWord(SeedContext const * pContext)
  {
    return std::string_view(pContext->CharacterBlock.data(), pContext->CurrentLength);
  }

  void FindWordsTask::CaptureCurrentWord(SeedContext * pContext)
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

  bool FindWordsTask::IsReversePath(std::vector<Coord> const & a, std::vector<Coord> const & b)
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

  std::vector<WordData> FindWordsTask::RemovePalindromes(std::vector<WordData> results)
  {
    // For each word, find any location sequences which are the same forward as backward and eliminate one
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
}
