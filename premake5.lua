workspace "BoggleSolver"
  architecture "x64"
  configurations {"Debug", "Release"}
  startproject "Application"

  project "Engine"
    location "Engine"
    kind "StaticLib"
    targetdir ("%{wks.location}/build/%{prj.name}-%{cfg.buildcfg}")
    objdir ("%{wks.location}/build/intermediate/%{prj.name}-%{cfg.buildcfg}")
    systemversion "latest"
    language "C++"
    cppdialect "C++20"
    warnings "Extra"
    fatalwarnings "All"

    files
    {
      "Engine/src/**.h",
      "Engine/src/**.cpp",
    }

    filter "configurations:Debug"
      runtime "Debug"
      staticruntime "on"
      symbols "on"

    filter "configurations:Release"
      runtime "Release"
      staticruntime "on"
      optimize "on"

  project "Application"
    location "Application"
    kind "WindowedApp"
    entrypoint "mainCRTStartup"
    targetdir ("%{wks.location}/build/%{prj.name}-%{cfg.buildcfg}")
    objdir ("%{wks.location}/build/intermediate/%{prj.name}-%{cfg.buildcfg}")
    systemversion "latest"
    language "C++"
    cppdialect "C++20"
    warnings "Extra"
    fatalwarnings "All"

    files
    {
      "Application/src/**.h",
      "Application/src/**.cpp",
      "Application/3rdParty/ImGui/imgui.cpp",
      "Application/3rdParty/ImGui/imgui.h",
      "Application/3rdParty/ImGui/imgui_internal.h",
      "Application/3rdParty/ImGui/imgui_draw.cpp",
      "Application/3rdParty/ImGui/imgui_tables.cpp",
      "Application/3rdParty/ImGui/imgui_widgets.cpp",
      "Application/3rdParty/ImGui/backends/imgui_impl_sdl3.h",
      "Application/3rdParty/ImGui/backends/imgui_impl_sdl3.cpp",
      "Application/3rdParty/ImGui/backends/imgui_impl_vulkan.h",
      "Application/3rdParty/ImGui/backends/imgui_impl_vulkan.cpp",
    }

    includedirs
    {
      "Engine/src",
    }

    externalincludedirs
    {
      "Application/3rdParty/SDL3/include",
      "Application/3rdParty/ImGui",
      "Application/3rdParty/ImGui/backends",
      "$(VULKAN_SDK)/Include",
    }

    libdirs
    {
      "Application/3rdParty/SDL3/lib/x64",
      "$(VULKAN_SDK)/Lib",
    }

    links
    {
      "Engine",
      "SDL3",
      "vulkan-1",
    }

    postbuildcommands
    {
      '{COPY} "%{wks.location}/Application/3rdParty/SDL3/lib/x64/SDL3.dll" "%{cfg.targetdir}"',
    }

    -- ImGui/backends are third-party sources; don't hold them to this project's
    -- warnings-as-errors policy.
    filter "files:Application/3rdParty/**"
      warnings "Default"
      buildoptions { "/WX-" }

    filter "configurations:Debug"
      runtime "Debug"
      staticruntime "on"
      symbols "on"

    filter "configurations:Release"
      runtime "Release"
      staticruntime "on"
      optimize "on"
