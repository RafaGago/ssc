--This project builds "base_library" and "task_queue" in place

local projname             = "ssc"
local repo                 = path.getabsolute ("../..")

local base_library         = repo .. "/dependencies/base_library"
local base_library_include = base_library .. "/include"
local base_library_src     = base_library .. "/src"

local libcoro              = repo .. "/dependencies/libcoro"
local libcoro_include      = libcoro
local libcoro_src          = libcoro

local cmocka               = base_library .. "/dependencies/cmocka"
local cmocka_lib           = cmocka .. "/lib"
local cmocka_include       = cmocka .. "/include"

local repo_include         = repo .. "/include"
local repo_src             = repo .. "/src"
local repo_test_src        = repo .. "/test/src"
local repo_example_src     = repo .. "/example/src"
local repo_build           = repo .. "/build"

local stage                = repo_build .. "/stage/" .. os.get()
local build                = repo_build .. "/" .. os.get()
local version              = ".0.0.0"

local function get_bl_stage(cfg)
  return base_library .. "/build/stage/" .. os.get() .. "/" .. cfg
end

solution "build"
  configurations { "release", "debug" }
  location (build)

filter {"system:linux"}
  defines {"BL_USE_CLOCK_MONOTONIC_RAW"}

filter {"system:not windows"}
  prebuildcommands { "cd ../../dependencies && ./recompile_base_library.sh" }
  postbuildcommands {
    "cd %{cfg.buildtarget.directory} && "..
    "ln -sf %{cfg.buildtarget.name} " ..
    "%{cfg.buildtarget.name:gsub ('.%d+.%d+.%d+', '')}"
    }

filter {"configurations:*"}
  flags {"MultiProcessorCompile"}
  includedirs {
    repo_include,
    repo_src,
    base_library_include,
    libcoro_include
    }

filter {"configurations:release"}
  defines {"NDEBUG"}
  optimize "On"
  links {
    get_bl_stage ("release") .. "/lib/base_library_task_queue",
    get_bl_stage ("release") .. "/lib/base_library_nonblock",
    get_bl_stage ("release") .. "/lib/base_library",
    }

filter {"configurations:debug"}
  flags {"Symbols"}
  defines {"DEBUG"}
  optimize "Off"

filter {"configurations:debug", "action:vs*"}
  links {
    get_bl_stage ("debug") .. "/lib/base_library_task_queue",
    get_bl_stage ("debug") .. "/lib/base_library_nonblock",
    get_bl_stage ("debug") .. "/lib/base_library",
    }

filter {"configurations:debug", "action:gmake", "kind:ConsoleApp"}
  --Debug libraries have nonstandard names
  removelinks {
    projname .. "_static"
    }
  linkoptions {
    "-L".. get_bl_stage ("debug") .. "/lib",
    "-L".. stage .. "/debug/lib",
    "-l:lib" .. projname .. "_static.a.d",
    "-l:libbase_library_task_queue.a.d",
    "-l:libbase_library_nonblock.a.d",
    "-l:libbase_library.a.d",
  }

filter {"configurations:debug", "action:gmake", "kind:not ConsoleApp"}
  --Debug libraries have nonstandard names
  removelinks {
    projname .. "_static"
    }
  linkoptions {
    "-L".. get_bl_stage ("debug") .. "/lib",
    "-L".. stage .. "/debug/lib",
    "-l:libbase_library_task_queue.a.d",
    "-l:libbase_library_nonblock.a.d",
    "-l:libbase_library.a.d",
  }

filter {"kind:SharedLib", "configurations:debug", "system:not windows"}
  targetextension (".so" .. ".d" .. version)

filter {"kind:SharedLib", "configurations:release", "system:not windows"}
    targetextension (".so" .. version)

filter {"kind:StaticLib", "configurations:debug", "system:not windows"}
  targetextension (".a" .. ".d" .. version )

filter {"kind:StaticLib", "configurations:debug"}
  targetdir (stage .. "/debug/lib")

filter {"kind:StaticLib", "configurations:release", "system:not windows"}
  targetextension (".a" .. version )

filter {"kind:StaticLib", "configurations:release"}
  targetdir (stage .. "/release/lib")

filter {"kind:ConsoleApp", "configurations:debug", "system:not windows"}
  targetextension ("_d" .. version)

filter {"kind:ConsoleApp", "configurations:debug"}
  targetdir (stage .. "/debug/bin")

filter {"kind:ConsoleApp", "configurations:release", "system:not windows"}
  targetextension (version )

filter {"kind:ConsoleApp", "configurations:release"}
  targetdir (stage .. "/release/bin")

filter {"action:gmake"}
  buildoptions {"-std=gnu11"}
  buildoptions {"-Wfatal-errors"}
  buildoptions {"-fno-stack-protector"}

filter {"action:gmake", "kind:*Lib"}
  buildoptions {"-fvisibility=hidden"}

filter {"action:gmake", "kind:ConsoleApp"}
  links {"pthread", "rt"}

filter {"action:vs*"}
  buildoptions {"/TP"}

project (projname)
  kind "SharedLib"
  defines { "SSC_SIM_SHAREDLIB", "SSC_SIM_SHAREDLIB_COMPILATION"}
  language "C"
  files {
    repo_src .. "/**.c",
    libcoro .. "/coro.c"
    }

project (projname .. "_static")
  kind "StaticLib"
  defines {"SSC_PRIVATE_SYMS", "SSC_SIM_PRIVATE_SYMS"}
  language "C"
  files {
    repo_src .. "/**.c",
    libcoro .. "/coro.c"
    }

project (projname .. "_test")
  kind "ConsoleApp"
  language "C"
  files {
    repo_test_src .. "/**.c",
    }
  includedirs {
    repo_test_src,
    cmocka_include
    }
  links {
    cmocka_lib .. "/cmocka",
    projname .. "_static"
    }

project (projname .. "_example")
  kind "ConsoleApp"
  language "C"
  files {
    repo_example_src .. "/**.c",
    }
  links {
    projname .. "_static"
    }