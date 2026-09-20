workspace "DgEngineSamples"
  architecture "x64"

  project "Engine"
    location "Engine"
    kind "StaticLib"
    targetdir ("%{wks.location}/build/%{prj.name}-%{cfg.buildcfg}")
    objdir ("%{wks.location}/build/intermediate/%{prj.name}-%{cfg.buildcfg}")
    systemversion "latest"
    language "C++"
    cppdialect "C++20"
    flags {"FatalWarnings"}
  
    files 
    {
      "Engine/src/**.h",
      "Engine/src/**.cpp",
    }

    filter "configurations:Debug"
	  runtime "Debug"
	  symbols "on"

	filter "configurations:Release"
	  runtime "Release"
	  optimize "on"
    
  project "Application"
    location "Application"
    kind "StaticLib"
    targetdir ("%{wks.location}/build/%{prj.name}-%{cfg.buildcfg}")
    objdir ("%{wks.location}/build/intermediate/%{prj.name}-%{cfg.buildcfg}")
    systemversion "latest"
    language "C++"
    cppdialect "C++20"
    flags {"FatalWarnings"}
    
    files 
    {
      "Application/src/**.h",
      "Application/src/**.cpp",
    }

    filter "configurations:Debug"
	  runtime "Debug"
	  symbols "on"

	filter "configurations:Release"
	  runtime "Release"
	  optimize "on"