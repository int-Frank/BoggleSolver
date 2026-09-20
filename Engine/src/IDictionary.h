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

    static IDictionary * Create(std::set<std::string> const & words);

    virtual ~IDictionary() = default;

    virtual uint32_t WordCount(char) const = 0;

    // Once the word finder has two letters, check if any of these words exist - 
    // We don't need to continue if for example no 'xf' words exist
    virtual uint32_t WordCount(char, char) const = 0;

    // Once the word finder has three letters, check if any of these words exist - 
    // We don't need to continue if for example no 'frg' words exist
    // The idea here is that we have a has map of all the three letter combinations which are, or begin a word.
    // From here, the word finder will continue to add letters to try to 
    virtual uint32_t WordCount(char, char, char, Context const ** ppContext) const = 0;

    virtual bool IsWord(std::string_view word, Context const * pContext) const = 0;
  };
}

#endif