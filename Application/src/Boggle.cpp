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
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
  // Minimum time the "Working" popup must have been visible before ProcessPendingBoardStart
  // is allowed to block on the search - one frame isn't reliably enough (present latency,
  // vsync, etc. can mean it never actually hits the screen before we'd block again), so we
  // give it a real wall-clock budget across however many frames that takes instead.
  constexpr std::chrono::milliseconds MinWorkingPopupVisibleTime(100);

  void SelectWord(App::AppData * pData, Engine::WordData const & word)
  {
    pData->UI.SelectedWord = word.Word;
    pData->UI.SelectedPaths = word.Locations;
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
      case App::BoardType::Mammoth: return Engine::GenerateCustomBoggle(100, 100, pSeed);
      case App::BoardType::Custom:  return Engine::GenerateCustomBoggle(width, height, pSeed);
      default:                      return Engine::GenerateModernBoggleGrid(pSeed);
    }
  }

  void NewGameBoard(Engine::Grid2D<char> board, App::AppData * pData)
  {
    pData->BoggleLayout = board;
    pData->UI = App::UIData{};

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

  // Switches to board, either immediately (small boards) or via the "Working" popup dance
  // (see ProcessPendingBoardStart). Returns true if the caller still needs to open the
  // "Working" popup itself - see DrawNewBoardPopup's comment for why it must do that from
  // its own context rather than us doing it here.
  bool StartNewBoard(App::AppData * pData, Engine::Grid2D<char> board)
  {
    int totalDice = board.Width() * board.Height();
    if (totalDice < App::AppData::DirectStartDiceThreshold)
    {
      NewGameBoard(std::move(board), pData);
      return false;
    }

    pData->PendingBoard = App::PendingBoardStart{ std::move(board), std::chrono::steady_clock::now() };
    return true;
  }

  char const * BoardTypeName(App::BoardType type)
  {
    switch (type)
    {
      case App::BoardType::Classic: return "Classic";
      case App::BoardType::Big:     return "Big";
      case App::BoardType::Super:   return "Super";
      case App::BoardType::Mammoth: return "Mammoth";
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

    static char const * BoardTypes[] = { "Classic (4x4)", "Modern (4x4)", "Big (5x5)", "Super (6x6)", "Mammoth (100x100)", "Custom" };
    static int boardTypeIndex = 1;
    static int customWidth = 4;
    static int customHeight = 4;
    static bool useSeed = false;
    static int seedValue = 0;

    ImGui::Combo("Board", &boardTypeIndex, BoardTypes, IM_ARRAYSIZE(BoardTypes));

    if (boardTypeIndex == 5)
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
      started = StartNewBoard(pData, std::move(board));
      ImGui::CloseCurrentPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
    return started;
  }

  // Opened (via ImGui::OpenPopup("Working")) by whatever stashes a board into
  // pData->PendingBoard. Stays open until that board has been visible for long enough that
  // ProcessPendingBoardStart has consumed it and blocked on the search, at which point it
  // closes itself immediately. Assumes the same calling convention as DrawNewBoardPopup -
  // called every frame from the same window/ID stack.
  void DrawWorkingPopup(App::AppData * pData)
  {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal("Working", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
      return;

    ImGui::TextUnformatted("Solving board...");

    if (!pData->PendingBoard.has_value())
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
      if (StartNewBoard(pData, std::move(board)))
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
    // When a word has multiple locations, focus on the first one.
    if (pData->UI.PendingScrollToSelection && !pData->UI.SelectedPaths.empty() && !pData->UI.SelectedPaths[0].empty())
    {
      Engine::Coord target = pData->UI.SelectedPaths[0][0];
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

    if (!pData->UI.SelectedPaths.empty())
    {
      constexpr float PixelsPerSecond = 700.0f;
      constexpr float LineThickness = 5.0f;

      auto TileCenter = [&](Engine::Coord coord) -> ImVec2
      {
        return origin + ImVec2(TrayPadding + coord.X * (TileSize + TileGap) + TileSize * 0.5f,
                                TrayPadding + coord.Y * (TileSize + TileGap) + TileSize * 0.5f);
      };

      double elapsedSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - pData->UI.SelectionStartTime).count();

      // Cap each segment with filled circles at both ends - gives it a rounded-cap look,
      // and smooths the joints where consecutive segments meet.
      auto DrawSegment = [&](ImVec2 a, ImVec2 b)
      {
        pDrawList->AddLine(a, b, pathColor, LineThickness);
        float radius = LineThickness * 0.5f;
        pDrawList->AddCircleFilled(a, radius, pathColor);
        pDrawList->AddCircleFilled(b, radius, pathColor);
      };

      // All of the word's locations animate in together, at the same pace.
      for (auto const & path : pData->UI.SelectedPaths)
      {
        if (path.size() < 2)
          continue;

        float remainingLength = static_cast<float>(elapsedSeconds) * PixelsPerSecond;

        ImVec2 previousPoint = TileCenter(path[0]);
        for (size_t i = 1; i < path.size() && remainingLength > 0.0f; i++)
        {
          ImVec2 nextPoint = TileCenter(path[i]);
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

    // Covers the tray's footprint (same role Dummy played before) and doubles as a
    // click-and-drag panning surface - handy for boards too big to fit the panel.
    // Disabled when the whole board already fits, since there's nothing to pan to.
    ImGui::InvisibleButton("BoardDragArea", traySize);

    bool canScroll = traySize.x > avail.x || traySize.y > avail.y;

    if (canScroll && ImGui::IsItemActive())
    {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

      ImVec2 dragDelta = ImGui::GetIO().MouseDelta;
      ImGui::SetScrollX(ImGui::GetScrollX() - dragDelta.x);
      ImGui::SetScrollY(ImGui::GetScrollY() - dragDelta.y);
    }
    else if (canScroll && ImGui::IsItemHovered())
    {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
  }

  void DrawRightPanel(App::AppData * pData, float width)
  {
    ImGui::BeginChild("RightPanel", ImVec2(width, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    DrawBoard(pData);
    ImGui::EndChild();
  }

  // Consumes pData->PendingBoard (if set and due) by running the old blocking NewGameBoard
  // on it. Called once per frame, first thing in DoFrame - so that when this runs, the
  // "Working" popup opened when PendingBoard was stashed has already been rendered and
  // presented for at least MinWorkingPopupVisibleTime, and the user isn't staring at a
  // frozen window with no feedback.
  void ProcessPendingBoardStart(App::AppData * pData)
  {
    if (!pData->PendingBoard.has_value())
      return;

    if (std::chrono::steady_clock::now() - pData->PendingBoard->QueuedTime < MinWorkingPopupVisibleTime)
      return;

    NewGameBoard(std::move(pData->PendingBoard->Board), pData);
    pData->PendingBoard.reset();
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
      BoardType::Modern
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
    delete (*ppData)->pDictionary;
    delete *ppData;
    *ppData = nullptr;

    return true;
  }

  void DoFrame(AppData * pData)
  {
    ProcessPendingBoardStart(pData);

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
}
