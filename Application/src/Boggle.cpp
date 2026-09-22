// Boggle application code: owns the AppData lifecycle and builds the UI each frame.

#include "ApplicationAPI.h"
#include "CharacterGridGenerator.h"
#include "IDictionary.h"
#include "IWordSearch.h"

#include "imgui.h"

#include <chrono>
#include <fstream>
#include <set>
#include <thread>

namespace App
{
  Engine::IDictionary const * GetDictionary()
  {
    std::set<std::string> words;
    std::ifstream file("resources/dictionary.txt");
    std::string word;
    while (std::getline(file, word))
    {
      if (!word.empty() && word.back() == '\r')
        word.pop_back();
      if (!word.empty())
        words.insert(word);
    }
    return Engine::IDictionary::Create(words);
  }

  bool InitAppData(AppData ** ppData)
  {
    Engine::IWorkerPool * pWorkerPool = Engine::IWorkerPool::Create(static_cast<int>(std::thread::hardware_concurrency()));
    if (pWorkerPool == nullptr)
      return false;

    *ppData = new AppData
    { 
      Engine::Grid2D<char>(1, 1, ' '), 
      BoggleResult{}, 
      pWorkerPool,
      GetDictionary() 
    };

    Engine::Grid2D<char> board = Engine::GenerateModernBoggleGrid(nullptr);
    NewGameBoard(board, *ppData);

    return true;
  }

  bool DestroyAppData(AppData ** ppData)
  {
    if (ppData == nullptr || *ppData == nullptr)
      return false;

    delete (*ppData)->pWorkerPool;
    delete *ppData;
    *ppData = nullptr;

    return true;
  }

  void DoFrame(AppData * pData)
  {
    ImGui::Begin("Boggle Solver");
    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
      static float placeholderValue = 0.5f;
      ImGui::TextUnformatted("Placeholder controls - wire these up to the solver.");
      ImGui::SliderFloat("Example value", &placeholderValue, 0.0f, 1.0f);

      ImGuiIO & io = ImGui::GetIO();
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", static_cast<double>(1000.0f / io.Framerate), static_cast<double>(io.Framerate));
      ImGui::Text("Boggle grid: %d x %d", pData->BoggleLayout.Width(), pData->BoggleLayout.Height());
    }
    ImGui::End();
  }

  void NewGameBoard(Engine::Grid2D<char> board, AppData * pData)
  {
    pData->BoggleLayout = board;

    Engine::IWordSearch * pSearch = Engine::IWordSearch::Begin(&pData->BoggleLayout, pData->pWorkerPool, pData->pDictionary);

    auto startTime = std::chrono::steady_clock::now();

    std::vector<Engine::WordData> const * pResult = nullptr;
    while ((pResult = pSearch->GetResult()) == nullptr)
    {
      pData->pWorkerPool->DoPostWork();
      std::this_thread::yield();
    }

    pData->Result.Time = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
    pData->Result.Words = *pResult;

    delete pSearch;
  }
}
