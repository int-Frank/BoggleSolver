#include "ApplicationAPI.h"

#include <cstdlib>

int main(int, char**)
{
  if (!App::InitApplication())
    return EXIT_FAILURE;

  App::AppData * pData = nullptr;
  if (!App::InitAppData(&pData))
  {
    App::Shutdown();
    return EXIT_FAILURE;
  }

  for (;;)
  {
    App::BeginFrame();

    if (App::IsDone())
      break;

    if (App::IsMinimised())
      continue;

    App::DoFrame(pData);
    App::EndFrame();
  }

  App::DestroyAppData(&pData);
  App::Shutdown();

  return EXIT_SUCCESS;
}
