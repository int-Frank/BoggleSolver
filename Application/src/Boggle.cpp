// Boggle application code: owns the AppData lifecycle and builds the UI each frame.

#include "ApplicationAPI.h"
#include "CharacterGridGenerator.h"
#include "IDictionary.h"
#include "IWordSearch.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
  void SelectWord(App::AppData * pData, Engine::WordData const & word)
  {
    pData->UI.SelectedWord = word.Word;
    pData->UI.SelectedPath = word.Locations.empty() ? std::vector<Engine::Coord>() : word.Locations[0];
    pData->UI.SelectionStartTime = std::chrono::steady_clock::now();
    pData->UI.PendingScrollToSelection = true;
  }

  // width/height are only used for BoardType::Custom - the other types have a fixed size.
  Engine::Grid2D<char> GenerateBoard(App::BoardType type, unsigned int width, unsigned int height, unsigned int * pSeed)
  {
    switch (type)
    {
      case App::BoardType::Classic: return Engine::GenerateClassicBoggleGrid(pSeed);
      case App::BoardType::Big:     return Engine::GenerateBigBoggle(pSeed);
      case App::BoardType::Super:   return Engine::GenerateSuperBoggle(pSeed);
      case App::BoardType::Custom:  return Engine::GenerateCustomBoggle(width, height, pSeed);
      default:                      return Engine::GenerateModernBoggleGrid(pSeed);
    }
  }

  char const * BoardTypeName(App::BoardType type)
  {
    switch (type)
    {
      case App::BoardType::Classic: return "Classic";
      case App::BoardType::Big:     return "Big";
      case App::BoardType::Super:   return "Super";
      case App::BoardType::Custom:  return "Custom";
      default:                      return "Modern";
    }
  }

  // Standard ImGui splitter idiom (see the ImGui wiki's "Widgets" page): draws and drives a
  // draggable divider between two regions whose combined size is assumed to stay constant.
  bool Splitter(bool splitVertically, float thickness, float * pSize1, float * pSize2, float minSize1, float minSize2)
  {
    ImGuiWindow * pWindow = ImGui::GetCurrentWindow();
    ImGuiID id = pWindow->GetID("##BoggleSplitter");

    ImRect bb;
    bb.Min = pWindow->DC.CursorPos + (splitVertically ? ImVec2(*pSize1, 0.0f) : ImVec2(0.0f, *pSize1));
    bb.Max = bb.Min + ImGui::CalcItemSize(splitVertically ? ImVec2(thickness, -1.0f) : ImVec2(-1.0f, thickness), 0.0f, 0.0f);

    return ImGui::SplitterBehavior(bb, id, splitVertically ? ImGuiAxis_X : ImGuiAxis_Y, pSize1, pSize2, minSize1, minSize2, 0.0f);
  }

  // Assumes it's called every frame from within the same window/ID stack as the
  // ImGui::OpenPopup("New Board") call, so the popup ID matches.
  //
  // Returns true on the frame a new board was started via OK - the caller must open the
  // "Working" popup itself (from its own context) rather than us doing it here: OpenPopup
  // hashes the popup ID against the *current* window, which while we're inside this
  // popup's own Begin/End block is "New Board", not the caller's window - opening
  // "Working" from here would raise it under the wrong ID and it would never appear.
  bool DrawNewBoardPopup(App::AppData * pData)
  {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal("New Board", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
      return false;

    static char const * BoardTypes[] = { "Classic (4x4)", "Modern (4x4)", "Big (5x5)", "Super (6x6)", "Custom" };
    static int boardTypeIndex = 1;
    static int customWidth = 4;
    static int customHeight = 4;
    static bool useSeed = false;
    static int seedValue = 0;

    ImGui::Combo("Board", &boardTypeIndex, BoardTypes, IM_ARRAYSIZE(BoardTypes));

    if (boardTypeIndex == 4)
    {
      ImGui::PushItemWidth(100.0f);
      ImGui::InputInt("Width", &customWidth);
      ImGui::SameLine();
      ImGui::InputInt("Height", &customHeight);
      ImGui::PopItemWidth();

      customWidth = customWidth < 1 ? 1 : customWidth;
      customHeight = customHeight < 1 ? 1 : customHeight;
    }

    ImGui::Checkbox("Use seed", &useSeed);
    if (useSeed)
    {
      ImGui::SameLine();
      ImGui::InputInt("Seed", &seedValue);
    }

    ImGui::Spacing();

    bool started = false;

    if (ImGui::Button("OK"))
    {
      unsigned int seedStorage = static_cast<unsigned int>(seedValue);
      unsigned int * pSeed = useSeed ? &seedStorage : nullptr;

      App::BoardType type = static_cast<App::BoardType>(boardTypeIndex);
      Engine::Grid2D<char> board = GenerateBoard(type, static_cast<unsigned int>(customWidth), static_cast<unsigned int>(customHeight), pSeed);

      pData->CurrentBoardType = type;
      App::NewGameBoard(board, pData);
      started = true;
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
    return started;
  }

  // Classic "rotating arc" spinner, hand-drawn since core ImGui has no built-in busy
  // indicator widget. Reserves a radius*2 square and advances using ImGui::GetTime().
  void DrawSpinner(float radius, float thickness, ImU32 color)
  {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));

    ImDrawList * pDrawList = ImGui::GetWindowDrawList();
    ImVec2 center(pos.x + radius, pos.y + radius);

    float time = static_cast<float>(ImGui::GetTime());
    constexpr int NumSegments = 24;
    constexpr float MinArc = IM_PI * 0.3f;
    constexpr float MaxArc = IM_PI * 1.6f;

    float rotation = time * 6.0f;
    float arcLength = MinArc + (std::sin(time * 2.5f) * 0.5f + 0.5f) * (MaxArc - MinArc);

    pDrawList->PathClear();
    for (int i = 0; i <= NumSegments; i++)
    {
      float angle = rotation + (static_cast<float>(i) / NumSegments) * arcLength;
      pDrawList->PathLineTo(ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius));
    }
    pDrawList->PathStroke(color, thickness);
  }

  // Opened (via ImGui::OpenPopup("Working")) by whatever kicks off NewGameBoard, and
  // self-closes once pData->pActiveSearch completes. Assumes the same calling convention
  // as DrawNewBoardPopup - called every frame from the same window/ID stack.
  void DrawWorkingPopup(App::AppData * pData)
  {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal("Working", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
      return;

    DrawSpinner(14.0f, 4.0f, IM_COL32(232, 90, 90, 255));
    ImGui::SameLine();
    ImGui::TextUnformatted("Solving board...");

    if (pData->pActiveSearch == nullptr)
      ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
  }

  void DrawLeftPanel(App::AppData * pData, float width)
  {
    ImGui::BeginChild("LeftPanel", ImVec2(width, 0.0f), false);

    if (ImGui::Button("New board"))
      ImGui::OpenPopup("New Board");
    if (DrawNewBoardPopup(pData))
      ImGui::OpenPopup("Working");
    DrawWorkingPopup(pData);

    ImGui::Separator();

    if (pData->CurrentBoardType == App::BoardType::Custom)
      ImGui::Text("Current board: Custom (%dx%d)", pData->BoggleLayout.Width(), pData->BoggleLayout.Height());
    else
      ImGui::Text("Current board: %s", BoardTypeName(pData->CurrentBoardType));

    if (ImGui::Button("Shake!"))
    {
      unsigned int boardWidth = static_cast<unsigned int>(pData->BoggleLayout.Width());
      unsigned int boardHeight = static_cast<unsigned int>(pData->BoggleLayout.Height());
      Engine::Grid2D<char> board = GenerateBoard(pData->CurrentBoardType, boardWidth, boardHeight, nullptr);
      App::NewGameBoard(board, pData);
      ImGui::OpenPopup("Working");
    }

    ImGui::Spacing();

    if (pData->Result.Time >= 0.05)
      ImGui::Text("Time to solve: %.1fs", pData->Result.Time);
    else
      ImGui::Text("Time to solve: %.2fms", pData->Result.Time * 1000.0);

    ImGui::Text("Found words: %d", static_cast<int>(pData->Result.Words.size()));

    static char const * SortModes[] = { "Alphabetical", "Smallest first", "Largest first" };
    static int sortMode = 0;
    ImGui::Combo("Sort by", &sortMode, SortModes, IM_ARRAYSIZE(SortModes));

    ImGui::Spacing();
    ImGui::BeginChild("WordList", ImVec2(0.0f, 0.0f), true);

    std::vector<Engine::WordData const *> sortedWords;
    sortedWords.reserve(pData->Result.Words.size());
    for (Engine::WordData const & word : pData->Result.Words)
      sortedWords.push_back(&word);

    auto byAlphabetical = [](Engine::WordData const * pA, Engine::WordData const * pB) { return pA->Word < pB->Word; };

    switch (sortMode)
    {
      case 1: // Smallest first
        std::stable_sort(sortedWords.begin(), sortedWords.end(), [](Engine::WordData const * pA, Engine::WordData const * pB)
          {
            if (pA->Word.size() != pB->Word.size())
              return pA->Word.size() < pB->Word.size();
            return pA->Word < pB->Word;
          });
        break;
      case 2: // Largest first
        std::stable_sort(sortedWords.begin(), sortedWords.end(), [](Engine::WordData const * pA, Engine::WordData const * pB)
          {
            if (pA->Word.size() != pB->Word.size())
              return pA->Word.size() > pB->Word.size();
            return pA->Word < pB->Word;
          });
        break;
      default: // Alphabetical
        std::stable_sort(sortedWords.begin(), sortedWords.end(), byAlphabetical);
        break;
    }

    for (Engine::WordData const * pWord : sortedWords)
    {
      bool isSelected = (pWord->Word == pData->UI.SelectedWord);
      if (ImGui::Selectable(pWord->Word.c_str(), isSelected))
        SelectWord(pData, *pWord);
    }

    ImGui::EndChild();

    ImGui::EndChild();
  }

  void DrawBoard(App::AppData * pData)
  {
    constexpr float TileSize = 64.0f;
    constexpr float TileGap = 10.0f;
    constexpr float TrayPadding = 20.0f;
    constexpr float TileRounding = 8.0f;
    constexpr float TrayRounding = 20.0f;

    const ImU32 trayBgColor = IM_COL32(74, 98, 122, 255);
    const ImU32 trayBorderColor = IM_COL32(45, 60, 78, 255);
    const ImU32 tileColor = IM_COL32(245, 222, 179, 255);
    const ImU32 tileBorderColor = IM_COL32(120, 120, 120, 255);
    const ImU32 textColor = IM_COL32(30, 30, 30, 255);
    const ImU32 pathColor = IM_COL32(232, 90, 90, 255);

    int cols = pData->BoggleLayout.Width();
    int rows = pData->BoggleLayout.Height();

    float gridWidth = cols * TileSize + (cols - 1) * TileGap;
    float gridHeight = rows * TileSize + (rows - 1) * TileGap;
    ImVec2 traySize(gridWidth + TrayPadding * 2.0f, gridHeight + TrayPadding * 2.0f);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 centerOffset(
      traySize.x < avail.x ? (avail.x - traySize.x) * 0.5f : 0.0f,
      traySize.y < avail.y ? (avail.y - traySize.y) * 0.5f : 0.0f);

    if (centerOffset.x > 0.0f || centerOffset.y > 0.0f)
      ImGui::SetCursorPos(ImGui::GetCursorPos() + centerOffset);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList * pDrawList = ImGui::GetWindowDrawList();

    // Scroll the selected word's starting tile into view, once, right after it's selected -
    // matters once the board is bigger than the panel and the selection can start offscreen.
    if (pData->UI.PendingScrollToSelection && !pData->UI.SelectedPath.empty())
    {
      Engine::Coord target = pData->UI.SelectedPath[0];
      ImVec2 targetScreenPos = origin + ImVec2(TrayPadding + target.X * (TileSize + TileGap) + TileSize * 0.5f,
                                                TrayPadding + target.Y * (TileSize + TileGap) + TileSize * 0.5f);

      ImGuiWindow * pRightPanelWindow = ImGui::GetCurrentWindow();
      ImGui::SetScrollFromPosX(pRightPanelWindow, targetScreenPos.x - pRightPanelWindow->Pos.x, 0.5f);
      ImGui::SetScrollFromPosY(pRightPanelWindow, targetScreenPos.y - pRightPanelWindow->Pos.y, 0.5f);

      pData->UI.PendingScrollToSelection = false;
    }

    pDrawList->AddRectFilled(origin, origin + traySize, trayBgColor, TrayRounding);
    pDrawList->AddRect(origin, origin + traySize, trayBorderColor, TrayRounding, 2.0f);

    ImFont * pFont = ImGui::GetFont();
    float letterFontSize = ImGui::GetFontSize() * 1.9f;

    // Tiles first, then the path line, then the letters on top - so the line passes
    // under the letters instead of over them.
    for (int y = 0; y < rows; y++)
    {
      for (int x = 0; x < cols; x++)
      {
        ImVec2 tileMin = origin + ImVec2(TrayPadding + x * (TileSize + TileGap), TrayPadding + y * (TileSize + TileGap));
        ImVec2 tileMax = tileMin + ImVec2(TileSize, TileSize);

        pDrawList->AddRectFilled(tileMin, tileMax, tileColor, TileRounding);
        pDrawList->AddRect(tileMin, tileMax, tileBorderColor, TileRounding, 1.5f);
      }
    }

    if (pData->UI.SelectedPath.size() >= 2)
    {
      constexpr float PixelsPerSecond = 700.0f;
      constexpr float LineThickness = 5.0f;

      auto TileCenter = [&](Engine::Coord coord) -> ImVec2
      {
        return origin + ImVec2(TrayPadding + coord.X * (TileSize + TileGap) + TileSize * 0.5f,
                                TrayPadding + coord.Y * (TileSize + TileGap) + TileSize * 0.5f);
      };

      double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - pData->UI.SelectionStartTime).count();
      float remainingLength = static_cast<float>(elapsedSeconds) * PixelsPerSecond;

      // Cap each segment with filled circles at both ends - gives it a rounded-cap look,
      // and smooths the joints where consecutive segments meet.
      auto DrawSegment = [&](ImVec2 a, ImVec2 b)
      {
        pDrawList->AddLine(a, b, pathColor, LineThickness);
        float radius = LineThickness * 0.5f;
        pDrawList->AddCircleFilled(a, radius, pathColor);
        pDrawList->AddCircleFilled(b, radius, pathColor);
      };

      ImVec2 previousPoint = TileCenter(pData->UI.SelectedPath[0]);
      for (size_t i = 1; i < pData->UI.SelectedPath.size() && remainingLength > 0.0f; i++)
      {
        ImVec2 nextPoint = TileCenter(pData->UI.SelectedPath[i]);
        ImVec2 segment = nextPoint - previousPoint;
        float segmentLength = std::sqrt(segment.x * segment.x + segment.y * segment.y);

        if (remainingLength >= segmentLength)
        {
          DrawSegment(previousPoint, nextPoint);
          remainingLength -= segmentLength;
        }
        else
        {
          float t = segmentLength > 0.0f ? remainingLength / segmentLength : 0.0f;
          DrawSegment(previousPoint, previousPoint + segment * t);
          remainingLength = 0.0f;
        }

        previousPoint = nextPoint;
      }
    }

    for (int y = 0; y < rows; y++)
    {
      for (int x = 0; x < cols; x++)
      {
        ImVec2 tileMin = origin + ImVec2(TrayPadding + x * (TileSize + TileGap), TrayPadding + y * (TileSize + TileGap));

        char c = pData->BoggleLayout.Get(Engine::Coord(x, y));
        std::string label = (c == 'q')
          ? std::string("Qu")
          : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(c))));

        ImVec2 textSize = pFont->CalcTextSizeA(letterFontSize, FLT_MAX, 0.0f, label.c_str());
        ImVec2 textPos = tileMin + (ImVec2(TileSize, TileSize) - textSize) * 0.5f;
        pDrawList->AddText(pFont, letterFontSize, textPos, textColor, label.c_str());
      }
    }

    ImGui::Dummy(traySize);
  }

  void DrawRightPanel(App::AppData * pData, float width)
  {
    ImGui::BeginChild("RightPanel", ImVec2(width, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    DrawBoard(pData);
    ImGui::EndChild();
  }

  // Drains the worker pool's post-work queue and, once the active search (if any) is
  // done, finalizes Result and clears pActiveSearch. Called once per frame from DoFrame.
  void UpdateActiveSearch(App::AppData * pData)
  {
    if (pData->pActiveSearch == nullptr)
      return;

    pData->pWorkerPool->DoPostWork();

    std::vector<Engine::WordData> const * pResult = pData->pActiveSearch->GetResult();
    if (pResult == nullptr)
      return;

    pData->Result.Time = std::chrono::duration<double>(std::chrono::steady_clock::now() - pData->SearchStartTime).count();
    pData->Result.Words = *pResult;

    delete pData->pActiveSearch;
    pData->pActiveSearch = nullptr;
  }
}

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
      GetDictionary(),
      UIData{},
      BoardType::Modern,
      nullptr,
      std::chrono::steady_clock::time_point{}
    };

    Engine::Grid2D<char> board = Engine::GenerateModernBoggleGrid(nullptr);
    NewGameBoard(board, *ppData);

    return true;
  }

  bool DestroyAppData(AppData ** ppData)
  {
    if (ppData == nullptr || *ppData == nullptr)
      return false;

    // pWorkerPool must go first: its destructor joins all worker threads and safely
    // drains any queued/in-flight seed tasks (see WorkerPool::~WorkerPool), which is
    // what makes it safe to then delete a still-active IWordSearch below - its own
    // contract otherwise forbids deleting it while any seed task may be in flight.
    delete (*ppData)->pWorkerPool;
    delete (*ppData)->pActiveSearch;
    delete (*ppData)->pDictionary;
    delete *ppData;
    *ppData = nullptr;

    return true;
  }

  void DoFrame(AppData * pData)
  {
    UpdateActiveSearch(pData);

    ImGuiViewport const * pViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(pViewport->WorkPos);
    ImGui::SetNextWindowSize(pViewport->WorkSize);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("BoggleSolver", nullptr, windowFlags);

    static float leftPanelWidth = 260.0f;
    float rightPanelWidth = ImGui::GetContentRegionAvail().x - leftPanelWidth - 8.0f;

    Splitter(true, 8.0f, &leftPanelWidth, &rightPanelWidth, 150.0f, 300.0f);

    DrawLeftPanel(pData, leftPanelWidth);
    ImGui::SameLine();
    DrawRightPanel(pData, rightPanelWidth);

    ImGui::End();
  }

  void NewGameBoard(Engine::Grid2D<char> board, AppData * pData)
  {
    // A search is already running - the UI shouldn't be able to trigger this (the
    // "Working" modal blocks other interaction), but guard anyway since it's not safe
    // to abandon an in-flight IWordSearch (see its class comment).
    if (pData->pActiveSearch != nullptr)
      return;

    pData->BoggleLayout = board;
    pData->UI = UIData{};
    pData->Result = BoggleResult{};

    pData->pActiveSearch = Engine::IWordSearch::Begin(&pData->BoggleLayout, pData->pWorkerPool, pData->pDictionary);
    pData->SearchStartTime = std::chrono::steady_clock::now();
  }
}
