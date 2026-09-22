#ifndef IDICTIONARY_H
#define IDICTIONARY_H

#include <cstdint>
#include <string>
#include <string_view>
#include <set>

namespace Engine
{
  class IDictionary
  {

  public:

    class Context;

    struct WordSearchResult
    {
      bool IsWord;
      uint32_t WordsBeginWith;
    };

    // Words less than 3 letters are ignored.
    // Words containing a 'q' not immediatly followed by a 'u' are ignored.
    // 'qu' is registerd as a single 'q'.
    static IDictionary * Create(std::set<std::string> const & words);

    virtual ~IDictionary() = default;

    virtual WordSearchResult Search(char) const = 0;

    virtual WordSearchResult Search(char, char) const = 0;

    virtual WordSearchResult Search(char, char, char, Context const ** ppContext) const = 0;

    virtual WordSearchResult Search(std::string_view word, Context const * pContext) const = 0;

    // Length, in stored characters (i.e. after 'qu' has been collapsed to a single 'q'),
    // of the longest word held by this dictionary.
    virtual uint32_t MaxStoredWordLength() const = 0;
  };
}

#endif