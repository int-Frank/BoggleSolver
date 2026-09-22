
#include <algorithm>
#include <array>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "IWordSearch.h"
#include "Coord.h"
#include "IWorkerPool.h"
#include "IDictionary.h"

namespace Engine
{
  class WordSearch : public IWordSearch
  {
  public:
    WordSearch(Grid2D<char> const * pGrid, IWorkerPool * pWorkerPool, IDictionary const * pDictionary);

    std::vector<WordData> const * GetResult() const override;

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
        , CharacterBlock(PathBufferSize(pCharacterGrid, pDictionary))
        , PathBlock(PathBufferSize(pCharacterGrid, pDictionary))
        , pDictionaryContext(nullptr)
        , pDictionary(pDictionary)
      {
      }

      // A path can never usefully grow past the longest word the dictionary holds - one
      // more than that to cover the speculative write ProcessSeed makes for the extending
      // character it's about to find the dictionary has nothing matching (see ProcessSeed).
      static size_t PathBufferSize(Grid2D<char> const * pCharacterGrid, IDictionary const * pDictionary)
      {
        size_t boardCells = (size_t)pCharacterGrid->Width() * (size_t)pCharacterGrid->Height();
        size_t maxPathLength = (size_t)pDictionary->MaxStoredWordLength() + 1;
        return std::min(boardCells, maxPathLength);
      }
    };

    struct SeedTask
    {
      Grid2D<char> const * pGrid;
      Coord Seed;
      IDictionary const * pDictionary;
      WordSearch * pOwner;
      std::vector<WordData> Result;
    };

    static std::vector<WordData> FindWordsForSeed(Grid2D<char> const * pGrid,
                                                    int seedX,
                                                    int seedY,
                                                    IDictionary const * pDictionary);

    // Returns newly found words from the current sequence
    static void ProcessSeed(Coord coord, SeedContext * pContext);
    static std::array<Coord, 8> GetSurroundingCoords(Coord coord);
    static std::string_view GetWord(SeedContext const * pContext);

    // Dictionary entries store 'q' as a stand-in for 'qu' (see Dictionary::Dictionary) -
    // expand it back before the word is captured.
    static std::string ExpandQu(std::string_view word);

    // Returns true if the word has previously been captured
    static void CaptureCurrentWord(SeedContext * pContext);

    static void RunSeedTask(void * pUserData) noexcept;
    static void MergeSeedTask(void * pUserData) noexcept;
    static void FreeSeedTask(void * pUserData) noexcept;

    struct PathHash
    {
      size_t operator()(std::vector<Coord> const & path) const;
    };

    struct PathEqual
    {
      bool operator()(std::vector<Coord> const & a, std::vector<Coord> const & b) const;
    };

    static std::vector<Coord> const & CanonicalPath(std::vector<Coord> const & path, std::vector<Coord> & reversedScratch);
    static std::vector<WordData> RemovePalindromes(std::vector<WordData> results);

    void MergeResult(std::vector<WordData> & seedResult);
    void OnSeedComplete();

    std::atomic<int> m_pending{ 0 };
    std::atomic<bool> m_done{ false };
    std::vector<WordData> m_results;
    std::unordered_map<std::string, int> m_wordEntries;
  };

  IWordSearch * IWordSearch::Begin(Grid2D<char> const * pGrid,
                                          IWorkerPool * pWorkerPool,
                                          IDictionary const * pDictionary)
  {
    return new WordSearch(pGrid, pWorkerPool, pDictionary);
  }

  WordSearch::WordSearch(Grid2D<char> const * pGrid, IWorkerPool * pWorkerPool, IDictionary const * pDictionary)
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
          // m_pending would never reach zero and the search would hang forever.
          OnSeedComplete();
        }
      }
    }
  }

  std::vector<WordData> const * WordSearch::GetResult() const
  {
    if (!m_done.load(std::memory_order_acquire))
      return nullptr;

    return &m_results;
  }

  void WordSearch::OnSeedComplete()
  {
    if (m_pending.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
      m_results = RemovePalindromes(std::move(m_results));
      m_done.store(true, std::memory_order_release);
    }
  }

  void WordSearch::MergeResult(std::vector<WordData> & seedResult)
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

  void WordSearch::RunSeedTask(void * pUserData) noexcept
  {
    SeedTask * pTask = static_cast<SeedTask *>(pUserData);
    pTask->Result = FindWordsForSeed(pTask->pGrid, pTask->Seed.X, pTask->Seed.Y, pTask->pDictionary);
  }

  // Runs on whichever single thread drives the shared IWorkerPool's DoPostWork(),
  // so m_results/m_wordEntries need no locking - as long as only that one thread
  // ever calls DoPostWork() on the pool, merges for this search never overlap.
  void WordSearch::MergeSeedTask(void * pUserData) noexcept
  {
    SeedTask * pTask = static_cast<SeedTask *>(pUserData);
    WordSearch * pOwner = pTask->pOwner;

    pOwner->MergeResult(pTask->Result);
    pOwner->OnSeedComplete();
  }

  void WordSearch::FreeSeedTask(void * pUserData) noexcept
  {
    delete static_cast<SeedTask *>(pUserData);
  }

  std::vector<WordData> WordSearch::FindWordsForSeed(Grid2D<char> const * pCharacterGrid,
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

  void WordSearch::ProcessSeed(Coord coord, SeedContext * pContext)
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
    bool anyWordsBeginWith = false;

    if (pContext->CurrentLength == 1)
    {
      char c = pContext->CharacterBlock[0];
      anyWordsBeginWith = pContext->pDictionary->Search(c).AnyWordsBeginWith;
    }

    else if (pContext->CurrentLength == 2)
    {
      char c0 = pContext->CharacterBlock[0];
      char c1 = pContext->CharacterBlock[1];
      anyWordsBeginWith = pContext->pDictionary->Search(c0, c1).AnyWordsBeginWith;
    }

    else if (pContext->CurrentLength == 3)
    {
      char c0 = pContext->CharacterBlock[0];
      char c1 = pContext->CharacterBlock[1];
      char c2 = pContext->CharacterBlock[2];
      auto result = pContext->pDictionary->Search(c0, c1, c2, &pContext->pDictionaryContext);

      if (result.IsWord)
      {
        CaptureCurrentWord(pContext);
      }

      anyWordsBeginWith = result.AnyWordsBeginWith;
    }

    else //(pContext->CurrentLength > 3)
    {
      auto word = GetWord(pContext);
      auto result = pContext->pDictionary->Search(word, pContext->pDictionaryContext);

      if (result.IsWord)
      {
        CaptureCurrentWord(pContext);
      }

      anyWordsBeginWith = result.AnyWordsBeginWith;
    }

    if (!anyWordsBeginWith)
    {
      pContext->CurrentLength--;
      return;
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

  std::array<Coord, 8> WordSearch::GetSurroundingCoords(Coord coord)
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

  std::string_view WordSearch::GetWord(SeedContext const * pContext)
  {
    return std::string_view(pContext->CharacterBlock.data(), pContext->CurrentLength);
  }

  std::string WordSearch::ExpandQu(std::string_view word)
  {
    std::string result;
    result.reserve(word.size());

    for (char c : word)
    {
      result.push_back(c);
      if (c == 'q')
        result.push_back('u');
    }

    return result;
  }

  void WordSearch::CaptureCurrentWord(SeedContext * pContext)
  {
    std::string word = ExpandQu(GetWord(pContext));
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

  size_t WordSearch::PathHash::operator()(std::vector<Coord> const & path) const
  {
    size_t seed = path.size();
    for (Coord const & c : path)
      seed ^= (std::hash<int>{}(c.X) ^ (std::hash<int>{}(c.Y) << 1)) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
  }

  bool WordSearch::PathEqual::operator()(std::vector<Coord> const & a, std::vector<Coord> const & b) const
  {
    return a == b;
  }

  // Returns whichever of path/reverse(path) sorts first, so that a path and its
  // reverse always map to the same canonical form. reversedScratch is caller-owned
  // storage used only when the reversed form is the one returned.
  std::vector<Coord> const & WordSearch::CanonicalPath(std::vector<Coord> const & path, std::vector<Coord> & reversedScratch)
  {
    size_t count = path.size();
    for (size_t i = 0; i < count / 2; i++)
    {
      Coord const & fromStart = path[i];
      Coord const & fromEnd = path[count - 1 - i];

      if (fromStart.X != fromEnd.X || fromStart.Y != fromEnd.Y)
      {
        bool startIsSmaller = (fromStart.X != fromEnd.X) ? (fromStart.X < fromEnd.X) : (fromStart.Y < fromEnd.Y);
        if (startIsSmaller)
          return path;

        reversedScratch.assign(path.rbegin(), path.rend());
        return reversedScratch;
      }
    }

    // Path is a true palindrome - either direction is already canonical.
    return path;
  }

  std::vector<WordData> WordSearch::RemovePalindromes(std::vector<WordData> results)
  {
    // For each word, find any location sequences which are the same forward as backward and eliminate one
    std::unordered_set<std::vector<Coord>, PathHash, PathEqual> seenCanonical;
    std::vector<Coord> reversedScratch;

    for (WordData & wordData : results)
    {
      seenCanonical.clear();

      std::vector<std::vector<Coord>> keptLocations;
      keptLocations.reserve(wordData.Locations.size());

      for (auto & path : wordData.Locations)
      {
        std::vector<Coord> const & canonical = CanonicalPath(path, reversedScratch);
        if (seenCanonical.insert(canonical).second)
          keptLocations.push_back(std::move(path));
      }

      wordData.Locations = std::move(keptLocations);
    }

    return results;
  }
}
