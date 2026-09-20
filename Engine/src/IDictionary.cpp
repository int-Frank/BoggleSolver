#include <algorithm>
#include <unordered_map>

#include "IDictionary.h"

namespace Engine
{
  class IDictionary::Context
  {
  public:

    // As entries are sorted, these are the indices of the first and last words beginning
    // with the three letters - we save this information for binary searching.
    uint32_t firstEntry;
    uint32_t lastEntry;

    uint32_t Count() const
    {
      return lastEntry - firstEntry;
    }
  };

  class Dictionary : public IDictionary
  {
    struct Word
    {
      uint32_t index;
      uint32_t length;
    };

    std::unordered_map<uint32_t, Context> m_oneLetterWords;
    std::unordered_map<uint32_t, Context> m_twoLetterWords;
    std::unordered_map<uint32_t, Context> m_threeLetterWords;

    std::vector<Word> m_wordEntryPoints;
    std::vector<char> m_wordCharacters;

  public:

    Dictionary(std::set<std::string> const & words)
    {
      size_t totalChars = 0;
      for (std::string const & word : words)
        totalChars += word.size();

      m_wordCharacters.reserve(totalChars);
      m_wordEntryPoints.reserve(words.size());

      // std::set already iterates in sorted (lexicographic) order, which is
      // exactly the ordering IsWord's binary search and the prefix maps
      // below rely on - no separate sort step needed.
      for (std::string const & word : words)
      {
        Word entry{};
        entry.index = (uint32_t)m_wordCharacters.size();
        entry.length = (uint32_t)word.size();

        m_wordCharacters.insert(m_wordCharacters.end(), word.begin(), word.end());
        m_wordEntryPoints.push_back(entry);
      }

      BuildPrefixMap(1, m_oneLetterWords);
      BuildPrefixMap(2, m_twoLetterWords);
      BuildPrefixMap(3, m_threeLetterWords);
    }

    bool IsWord(std::string_view word, Context const * pContext) const override
    {
      uint32_t first = 0;
      uint32_t last = (uint32_t)m_wordEntryPoints.size();

      if (pContext)
      {
        first = pContext->firstEntry;
        last = pContext->lastEntry;
      }

      auto begin = m_wordEntryPoints.cbegin() + first;
      auto end = m_wordEntryPoints.cbegin() + last;

      auto it = std::lower_bound(begin, end, word,
        [this](Word const & w, std::string_view target)
        {
          return WordAt(w) < target;
        });

      return it != end && WordAt(*it) == word;
    }

    uint32_t WordCount(char c) const override
    {
      uint32_t key = ToKey(c);

      auto it = m_oneLetterWords.find(key);
      if (it != m_oneLetterWords.cend())
        return it->second.Count();

      return 0;
    }

    uint32_t WordCount(char c0, char c1) const override
    {
      uint32_t key = ToKey(c0, c1);

      auto it = m_twoLetterWords.find(key);
      if (it != m_twoLetterWords.cend())
        return it->second.Count();

      return 0;
    }

    uint32_t WordCount(char c0, char c1, char c2, Context const ** ppContext) const override
    {
      uint32_t key = ToKey(c0, c1, c2);

      auto it = m_threeLetterWords.find(key);
      if (it != m_threeLetterWords.cend())
      {
        if (ppContext)
          *ppContext = &it->second;
        return it->second.Count();
      }

      if (ppContext)
        *ppContext = nullptr;
      return 0;
    }

  private:

    std::string_view WordAt(Word const & w) const
    {
      return std::string_view(m_wordCharacters.data() + w.index, w.length);
    }

    // Groups the (already sorted) m_wordEntryPoints into runs that share the
    // first prefixLength characters, recording each run's [first, last) index
    // range in map. Words shorter than prefixLength can't match any prefix of
    // that length and are skipped - they're still reachable via the shorter
    // prefix maps and the full IsWord binary search.
    void BuildPrefixMap(size_t prefixLength, std::unordered_map<uint32_t, Context> & map)
    {
      uint32_t count = (uint32_t)m_wordEntryPoints.size();
      uint32_t i = 0;

      while (i < count)
      {
        std::string_view word = WordAt(m_wordEntryPoints[i]);

        if (word.size() < prefixLength)
        {
          i++;
          continue;
        }

        std::string_view prefix = word.substr(0, prefixLength);

        uint32_t j = i + 1;
        while (j < count)
        {
          std::string_view next = WordAt(m_wordEntryPoints[j]);
          if (next.size() < prefixLength || next.substr(0, prefixLength) != prefix)
            break;
          j++;
        }

        Context context{};
        context.firstEntry = i;
        context.lastEntry = j;
        map.emplace(ToKey(prefix), context);

        i = j;
      }
    }

    static uint32_t ToKey(char c)
    {
      return (uint32_t)(unsigned char)c;
    }

    static uint32_t ToKey(char c0, char c1)
    {
      return (uint32_t)(unsigned char)c0 << 8
        | (uint32_t)(unsigned char)c1;
    }

    static uint32_t ToKey(char c0, char c1, char c2)
    {
      return (uint32_t)(unsigned char)c0 << 16
        | (uint32_t)(unsigned char)c1 << 8
        | (uint32_t)(unsigned char)c2;
    }

    static uint32_t ToKey(std::string_view prefix)
    {
      switch (prefix.size())
      {
        case 1: return ToKey(prefix[0]);
        case 2: return ToKey(prefix[0], prefix[1]);
        default: return ToKey(prefix[0], prefix[1], prefix[2]);
      }
    }
  };

  IDictionary * IDictionary::Create(std::set<std::string> const & words)
  {
    return new Dictionary(words);
  }
}