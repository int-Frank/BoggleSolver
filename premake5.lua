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
    kind "ConsoleApp"
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
    }

    includedirs
    {
      "Engine/src",
    }

    links
    {
      "Engine",
    }

    filter "configurations:Debug"
      runtime "Debug"
      staticruntime "on"
      symbols "on"

    filter "configurations:Release"
      runtime "Release"
      staticruntime "on"
      optimize "on"
