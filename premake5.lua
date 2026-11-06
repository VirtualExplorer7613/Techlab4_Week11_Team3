workspace "Hitori"
	architecture "x64"
	startproject "HitoriEditor"

	configurations
	{
		"Debug",
		"Release",
		"Shipping",
		"ObjViewer",
		"Benchmark",
		"Benchmark_Debug",
	}


	multiprocessorcompile "On"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

IncludeDir = {}
IncludeDir["ImGui"] = "Source/ThirdParty/ImGui"
IncludeDir["stb"]   = "Source/ThirdParty/stb"
IncludeDir["json"]  = "Source/ThirdParty/json"
IncludeDir["DDSLoader"] = "Source/ThirdParty/DDSLoader"
IncludeDir["lua"]   = "Source/ThirdParty/lua/src"
IncludeDir["sol2"]  = "Source/ThirdParty/sol2/include"
IncludeDir["miniaudio"]  = "Source/ThirdParty/miniaudio"
IncludeDir["fbxsdk"]  = "Source/ThirdParty/FBXSDK/include"
IncludeDir["mikktspace"]  = "Source/ThirdParty/mikktspace"

FbxSdkLibDir = "%{wks.location}/Source/ThirdParty/FBXSDK/lib/x64"

-- premake의 filter는 project()를 만나면 초기화된다.
-- 두 프로젝트가 같은 런타임(/MDd vs /MD)으로 컴파일되지 않으면 링크가 실패하므로
-- 공통 설정을 함수로 묶어 각 프로젝트에서 호출한다.
function CommonSettings()
	language   "C++"
	cppdialect "C++20"
	staticruntime "off"
	characterset  "Unicode"

	targetdir ("Build/Bin/" .. outputdir)
	objdir    ("Build/Intermediate/" .. outputdir)
	-- 에셋·쉐이더를 상대 경로로 읽으므로 작업 디렉터리는 저장소 루트다.
	debugdir  "%{wks.location}"

	defines
	{
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"UNICODE",
		"_UNICODE",
		"SOL_ALL_SAFETIES_ON=1", 
	}

	filter "files:**.hlsl"
		excludefrombuild "On"

	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }
		defines { "ENGINE_PLATFORM_WINDOWS" }

	filter "configurations:Debug"
		defines { "ENGINE_DEBUG", "_DEBUG" }
		runtime  "Debug"
		symbols  "on"

	filter "configurations:Release"
		defines  { "ENGINE_RELEASE", "NDEBUG" }
		runtime  "Release"
		optimize "on"
		symbols  "on"

	-- 에디터 없이 OBJ 파일만 열어보는 Viewer 빌드
	filter "configurations:ObjViewer"
		defines  { "ENGINE_RELEASE", "NDEBUG", "OBJ_VIEWER" }
		runtime  "Release"
		optimize "on"
		symbols  "on"

	filter "configurations:Benchmark_Debug"
		defines  { "ENGINE_DEBUG", "_DEBUG", "BENCHMARK" }
		runtime  "Debug"
		symbols  "on"

	filter "configurations:Benchmark"
		defines  { "ENGINE_RELEASE", "NDEBUG", "BENCHMARK" }
		runtime  "Release"
		optimize "speed"
		-- /GL + /LTCG는 끈다. 켠 exe가 Defender ML 탐지(Trojan:Win32/Wacatac)에 오탐으로 걸렸다.
		vectorextensions "AVX2"
		-- /GS-(버퍼 보안 검사 끄기)는 성능 이득이 거의 없고 백신 휴리스틱 오탐을 부르기 쉬워 켜 둔다 (기본값 /GS)
		buildoptions { "/Gw" }
		symbols  "on"

	filter "configurations:Shipping"
		defines  { "ENGINE_RELEASE", "ENGINE_SHIPPING", "NDEBUG" }
		runtime  "Release"
		optimize "speed"
		symbols  "on"


	filter {}
end

-- FBX SDK는 DLL로 쓴다. libfbxsdk.lib은 임포트 라이브러리라 StaticLib(HitoriEngine)이 아니라 exe가 링크한다.
-- 런타임(/MDd, /MD)에 맞는 폴더를 고르고, dll은 exe 옆에 복사해 실행 시 로더가 찾게 한다.
function LinkFbxSdk()
	links { "libfbxsdk" }

	filter "configurations:Debug or Benchmark_Debug"
		libdirs { FbxSdkLibDir .. "/debug" }
		postbuildcommands { '{COPYFILE} "' .. FbxSdkLibDir .. '/debug/libfbxsdk.dll" "%{cfg.targetdir}"' }

	filter { "configurations:not Debug", "configurations:not Benchmark_Debug" }
		libdirs { FbxSdkLibDir .. "/release" }
		postbuildcommands { '{COPYFILE} "' .. FbxSdkLibDir .. '/release/libfbxsdk.dll" "%{cfg.targetdir}"' }

	filter {}
end


-- 외부 라이브러리. 별도 프로젝트로 두면 에디터의 소스 루트가 Source/Editor 하나로 좁혀져
-- Solution Explorer의 "모든 파일 표시"에서 폴더 구조가 그대로 보인다.
project "ImGui"
	location "Source/ThirdParty/ImGui"
	kind     "StaticLib"
	CommonSettings()

	files
	{
		"%{IncludeDir.ImGui}/*.h",
		"%{IncludeDir.ImGui}/*.cpp",
		"%{IncludeDir.ImGui}/backends/imgui_impl_win32.*",
		"%{IncludeDir.ImGui}/backends/imgui_impl_dx11.*",
	}

	includedirs
	{
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}


-- Lua 5.4. C로 컴파일한다. lua.c·luac.c는 main이 있는 단독 실행 파일용이라 뺀다.
project "Lua"
	location "Source/ThirdParty/lua"
	kind     "StaticLib"
	CommonSettings()
	language "C"   -- CommonSettings의 C++를 덮어쓴다

	files { "%{IncludeDir.lua}/*.h", "%{IncludeDir.lua}/*.c" }
	removefiles { "%{IncludeDir.lua}/lua.c", "%{IncludeDir.lua}/luac.c" }
	includedirs { "%{IncludeDir.lua}" }


-- 헤더 툴 (UE의 UnrealHeaderTool 역할). 엔진 코드에 의존하지 않고 std만 쓴다.
-- HitoriEngine / HitoriEditor가 빌드 전에 이 exe로 *.generated.h를 만든다.
HeaderTool = '"%{wks.location}/Build/Bin/' .. outputdir .. '/HitoriHeaderTool.exe"'
GeneratedDir = "%{wks.location}/Intermediate/Generated"

project "HitoriHeaderTool"
	location "Source/Programs/HitoriHeaderTool"
	kind     "ConsoleApp"
	CommonSettings()

	files
	{
		"Source/Programs/HitoriHeaderTool/**.h",
		"Source/Programs/HitoriHeaderTool/**.cpp",
	}

-- 런타임. 에디터를 모른다. include 경로에 Source/Editor가 없는 것이 그 방벽이다.
-- location이 소스 루트와 같아야 "모든 파일 표시"에서 폴더가 보인다.
project "HitoriEngine"
	location "Source/Runtime"
	kind     "StaticLib"
	CommonSettings()

	pchheader "EnginePCH.h"
	pchsource "Source/Runtime/EnginePCH.cpp"

	files
	{
		"Source/Runtime/**.h",
		"Source/Runtime/**.hpp",
		"Source/Runtime/**.cpp",
		"%{IncludeDir.DDSLoader}/*.h",
		"%{IncludeDir.DDSLoader}/*.cpp",
		"%{IncludeDir.mikktspace}/*.h",
		"%{IncludeDir.mikktspace}/*.c",
	}

	includedirs
	{
		"Source/Runtime",
		"Intermediate/Generated/Runtime",   -- 추가
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
		"%{IncludeDir.DDSLoader}",
		"%{IncludeDir.miniaudio}",
		"%{IncludeDir.lua}",
		"%{IncludeDir.sol2}",
		"%{IncludeDir.fbxsdk}",
		"%{IncludeDir.mikktspace}",
	}

	-- fbxsdk.h의 FBXSDK_DLL을 __declspec(dllimport)로 만든다. 없으면 정적 lib 기준 심볼을 찾아 링크가 실패한다.
	defines { "FBXSDK_SHARED" }

	links { "Lua" }

	dependson { "HitoriHeaderTool" }
	prebuildcommands { HeaderTool .. ' Runtime "' .. GeneratedDir .. '/Runtime" "%{wks.location}/Source/Runtime"' }

	-- sol2 템플릿이 섹션 수 제한을 넘긴다
	filter "files:Source/Runtime/Script/**.cpp"
		buildoptions { "/bigobj" }
	filter {}

	-- d3d11·dxgi 등 시스템 라이브러리는 여기서 링크하지 않는다. exe 프로젝트들이 링크한다.
	-- 정적 라이브러리가 임포트 라이브러리를 품고 있으면 exe의 /WHOLEARCHIVE가 그것까지 끌어와
	-- __NULL_IMPORT_DESCRIPTOR 중복 정의(LNK2005)가 난다.

	filter "files:Source/ThirdParty/DDSLoader/**.cpp"
		enablepch "Off"
	filter {}

	filter "files:Source/Runtime/Audio/MiniaudioImpl.cpp"
		enablepch "Off"
	filter {}

	-- MikkTSpace는 C 소스다. 엔진 PCH(C++)를 쓰지 않고 C로 컴파일한다.
	filter "files:Source/ThirdParty/mikktspace/**.c"
		enablepch "Off"
		compileas "C"
	filter {}

-- 에디터 애플리케이션. ObjViewer 구성에서는 같은 exe가 뷰어로 빌드된다.
project "HitoriEditor"
	location "Source/Editor"
	kind     "WindowedApp"
	CommonSettings()

	pchheader "EnginePCH.h"
	pchsource "Source/Editor/EditorPCH.cpp"

	links { "HitoriEngine", "ImGui", "Lua" }

	-- UClass는 헤더의 정적 객체가 등록하므로, exe가 직접 참조하지 않는 클래스의 .obj를 링커가 버리면
	-- 그 클래스가 등록되지 않아 씬 로드 때 액터가 사라진다. 엔진 .obj를 전부 링크해 막는다.
	linkoptions { "/WHOLEARCHIVE:HitoriEngine" }

	files
	{
		"Source/Editor/**.h",
		"Source/Editor/**.cpp",
		"Source/Programs/**.h",
		"Source/Programs/**.cpp",
		"Source/Editor/**.rc",   -- 창·exe 아이콘
	}

	removefiles { "Source/Programs/HitoriHeaderTool/**" }

	dependson { "HitoriHeaderTool" }
	prebuildcommands { HeaderTool .. ' Editor "' .. GeneratedDir .. '/Editor" "%{wks.location}/Source/Editor" "%{wks.location}/Source/Programs/ObjViewer" "%{wks.location}/Source/Programs/Benchmark"' }

	includedirs
	{
		"Source",             -- "Editor/OutputLog/ConsolePanel.h"처럼 Source 기준 include를 쓴다
		"Source/Runtime",
		"Source/Programs",
		"Intermediate/Generated/Runtime",   -- 추가: 엔진 헤더가 include하는 generated.h
		"Intermediate/Generated/Editor",    -- 추가
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
		"%{IncludeDir.lua}",
		"%{IncludeDir.sol2}",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}

	LinkFbxSdk()

	filter "configurations:ObjViewer"
		targetname "ObjViewer"

	filter "configurations:Benchmark"
		targetname "Benchmark"

	filter "configurations:Benchmark_Debug"
		targetname "Benchmark_Debug"

	filter {}

-- 패키징용 게임 실행 파일. 에디터 소스와 ImGui를 링크하지 않는다.
-- 에셋과 셰이더는 실행 중에 읽으므로 배포 시 Package.bat으로 콘텐츠를 함께 복사한다.
project "HitoriGame"
	location "Source/Game"
	kind     "WindowedApp"
	CommonSettings()
	targetname "HitoriGame"

	pchheader "EnginePCH.h"
	pchsource "Source/Game/GamePCH.cpp"

	links { "HitoriEngine", "Lua" }

	-- 에디터와 같은 이유. 게임 코드는 라이트 액터 등을 직접 참조하지 않아 특히 빠지기 쉽다.
	linkoptions { "/WHOLEARCHIVE:HitoriEngine" }

	files
	{
		"Source/Game/**.h",
		"Source/Game/**.cpp",
		"Source/Game/**.rc",   -- 아이콘을 넣을 때
	}

	includedirs
	{
		"Source",
		"Source/Runtime",
		"Intermediate/Generated/Runtime",
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
		"%{IncludeDir.lua}",
		"%{IncludeDir.sol2}",
	}

	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}

	LinkFbxSdk()


-- 셰이더를 VS에서 편집하기 위한 보기 전용 프로젝트 (언리얼의 Shaders와 같은 역할).
-- 에셋(Assets)은 에디터 Content Drawer에서 다루므로 솔루션에 넣지 않는다.
-- 새 셰이더 파일은 SlnGenerator.bat을 다시 돌려야 목록에 반영된다.
project "Shaders"
	location "Intermediate/ProjectFiles"
	kind     "Utility"

	files { "Resources/Shader/**.hlsl" }
	files { "Resources/Shader/**.hlsli" }
	vpaths { ["*"] = "Resources/Shader/**" }

	filter "files:**.hlsl"
		excludefrombuild "On"

	filter "files:**.hlsli"
		excludefrombuild "On"

	filter {}


