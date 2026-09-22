#include <algorithm>
#include <unordered_map>

#include "IDictionary.h"

namespace Engine
{
  class IDictionary::Context
  {
  public:

    bool IsWord;

    // As entries are sorted, these are the indices of the first and last words beginning
    // with the three letters - we save this information for binary searching.
    uint32_t FirstEntry;
    uint32_t LastEntry;
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
    uint32_t m_maxWordLength = 0;

  public:

    Dictionary(std::set<std::string> const & words)
    {
      // Words are stored with 'u' dropped after every 'q', since a Boggle die never
      // shows a bare 'q' - it always shows "qu". Words that can't be spelled with such
      // a die (a 'q' not followed by 'u') are skipped entirely.
      //
      // totalChars/words.size() are upper bounds (skipped words and dropped 'u's only
      // make the real totals smaller), which is fine - reserve() just needs a bound.
      size_t totalChars = 0;
      for (std::string const & word : words)
        totalChars += word.size();

      m_wordCharacters.reserve(totalChars);
      m_wordEntryPoints.reserve(words.size());

      for (std::string const & word : words)
      {
        if (!IsValid(word))
          continue;

        Word entry{};
        entry.index = (uint32_t)m_wordCharacters.size();

        for (size_t i = 0; i < word.size(); i++)
        {
          m_wordCharacters.push_back(word[i]);
          if (word[i] == 'q')
            i++; // Skip the 'u' that IsSpellableWithQuDie guaranteed follows.
        }

        entry.length = (uint32_t)(m_wordCharacters.size() - entry.index);
        m_wordEntryPoints.push_back(entry);
        m_maxWordLength = std::max(m_maxWordLength, entry.length);
      }

      BuildPrefixMap(1, m_oneLetterWords);
      BuildPrefixMap(2, m_twoLetterWords);
      BuildPrefixMap(3, m_threeLetterWords);
    }

    WordSearchResult Search(std::string_view word, Context const * pContext) const override
    {
      uint32_t first = 0;
      uint32_t last = (uint32_t)m_wordEntryPoints.size();

      if (pContext)
      {
        first = pContext->FirstEntry;
        last = pContext->LastEntry;
      }

      auto begin = m_wordEntryPoints.cbegin() + first;
      auto end = m_wordEntryPoints.cbegin() + last;

      auto lowerBound = std::lower_bound(begin, end, word,
        [this](Word const & w, std::string_view target)
        {
          return WordAt(w) < target;
        });

      if (lowerBound == end)
        return WordSearchResult{ false, false };

      // A string always sorts before any of its own extensions, so if any entry in
      // [lowerBound, end) begins with word, lowerBound - the smallest such entry - is it.
      std::string_view candidate = WordAt(*lowerBound);

      WordSearchResult result;
      result.AnyWordsBeginWith = candidate.size() >= word.size() && candidate.substr(0, word.size()) == word;
      result.IsWord = result.AnyWordsBeginWith && candidate.size() == word.size();

      return result;
    }

    WordSearchResult Search(char c) const override
    {
      uint32_t key = ToKey(c);

      // Every entry in a prefix map covers at least one word, so being found here
      // always means AnyWordsBeginWith is true.
      auto it = m_oneLetterWords.find(key);
      if (it != m_oneLetterWords.cend())
        return WordSearchResult{ it->second.IsWord, true };

      return WordSearchResult{ false, false };
    }

    WordSearchResult Search(char c0, char c1) const override
    {
      uint32_t key = ToKey(c0, c1);

      auto it = m_twoLetterWords.find(key);
      if (it != m_twoLetterWords.cend())
        return WordSearchResult{ it->second.IsWord, true };

      return WordSearchResult{ false, false };
    }

    WordSearchResult Search(char c0, char c1, char c2, Context const ** ppContext) const override
    {
      uint32_t key = ToKey(c0, c1, c2);

      auto it = m_threeLetterWords.find(key);
      if (it != m_threeLetterWords.cend())
      {
        if (ppContext)
          *ppContext = &it->second;
        return WordSearchResult{ it->second.IsWord, true };
      }

      if (ppContext)
        *ppContext = nullptr;
      return WordSearchResult{ false, false };
    }

    uint32_t MaxStoredWordLength() const override
    {
      return m_maxWordLength;
    }

  private:

    // Hard-coded rules for validating words
    static bool IsValid(std::string_view word)
    {
      // Standard Boggle rule: words must be at least 3 letters long. This has to be
      // checked against the word's original length, before 'qu' is collapsed to a
      // single stored 'q' below - otherwise a 3-letter word like "que" (2 stored chars)
      // would be wrongly culled.
      if (word.size() < 3)
        return false;

      // A Boggle Qu die always contributes "qu", never a bare 'q' - so a word can only
      // be spelled if every 'q' in it is immediately followed by a 'u'.
      for (size_t i = 0; i < word.size(); i++)
      {
        if (word[i] == 'q' && (i + 1 >= word.size() || word[i + 1] != 'u'))
          return false;
      }

      return true;
    }

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
        context.IsWord = word.size() == prefixLength;
        context.FirstEntry = i;
        context.LastEntry = j;
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