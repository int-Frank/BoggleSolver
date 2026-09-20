#include <unordered_map>

#include "IDictionary.h"

namespace Engine
{
  class IDictionary::Context
  {
  public:


    // As entries are sorted, these are the indices of the first and last words beginning
    // with the three letters - we save this information for binary searching.
    size_t firstEntry;
    size_t lastEntry;

    size_t Count() const
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

    bool IsWord(std::string_view word, Context const * entry)
    {
      
    }

    size_t WordCount(char c) const override
    {
      uint32_t key = ToKey(c);

      auto it = m_oneLetterWords.find(key);
      if (it != m_oneLetterWords.cend())
        return it->second.Count();

      return 0;
    }

    size_t WordCount(char c0, char c1) const override
    {
      uint32_t key = ToKey(c0, c1);

      auto it = m_twoLetterWords.find(key);
      if (it != m_twoLetterWords.cend())
        return it->second.Count();

      return 0;
    }

    size_t WordCount(char c0, char c1, char c2, Context const ** pEntry) const override
    {
      uint32_t key = ToKey(c0, c1, c2);

      auto it = m_threeLetterWords.find(key);
      if (it != m_threeLetterWords.cend())
      {
        if (pEntry)
          *pEntry = &it->second;
        return it->second.Count();
      }

      if (pEntry)
        *pEntry = nullptr;
      return 0;
    }

  private:

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
  };


  IDictionary * IDictionary::Create(std::set<std::string> const & words)
  {
    // TODO
  }
}