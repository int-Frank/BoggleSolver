//@group Misc/impl

#include "CharacterGrid.h"

namespace Engine
{
  CharacterGrid::CharacterGrid(int a_width, int a_height)
  {
    if (a_width < 1)
      a_width = 1;
    if (a_height < 1)
      a_height = 1;

    m_width = a_width;
    m_height = a_height;
    m_characters.assign((size_t)m_width * (size_t)m_height, 'a');
  }

  CharacterGrid::~CharacterGrid()
  {
  }

  int CharacterGrid::Width() const
  {
    return m_width;
  }

  int CharacterGrid::Height() const
  {
    return m_height;
  }

  char CharacterGrid::Get(int a_x, int a_y) const
  {
    if (a_x < 0 || a_x >= m_width || a_y < 0 || a_y >= m_height)
      return 0;

    return m_characters[(size_t)a_y * m_width + a_x];
  }

  void CharacterGrid::Set(int a_x, int a_y, char a_c)
  {
    if (a_x < 0 || a_x >= m_width || a_y < 0 || a_y >= m_height)
      return;

    m_characters[(size_t)a_y * m_width + a_x] = a_c;
  }
}
