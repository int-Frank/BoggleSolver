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

    // Words containing a 'q' not immediatly followed by a 'u' are ignored.
    // 'qu' is registerd as a single 'q'.
    static IDictionary * Create(std::set<std::string> const & words);

    virtual ~IDictionary() = default;

    virtual WordSearchResult Search(char) const = 0;

    // Once the word finder has two letters, check if any of these words exist - 
    // We don't need to continue if for example no 'xf' words exist
    virtual WordSearchResult Search(char, char) const = 0;

    // Once the word finder has three letters, check if any of these words exist - 
    // We don't need to continue if for example no 'frg' words exist
    // The idea here is that we have a has map of all the three letter combinations which are, or begin a word.
    virtual WordSearchResult Search(char, char, char, Context const ** ppContext) const = 0;

    virtual WordSearchResult Search(std::string_view word, Context const * pContext) const = 0;
  };
}

#endif