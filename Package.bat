@echo off
rem 실행되는 명령어 자체를 콘솔에 출력하지 않음 (@는 이 줄 자체도 숨김)

setlocal
rem 이후 set으로 만든 변수가 이 스크립트 안에서만 유효하도록 범위를 묶음
rem (스크립트가 끝나면 호출한 쪽 cmd 환경에 변수가 남지 않음)

rem Usage: Package.bat [Release^|Shipping]   (default: Release)
rem ^| 는 | 문자를 파이프로 해석하지 않도록 이스케이프한 것

set "CONFIG=%~1"
rem 첫 번째 인자를 CONFIG에 저장. %~1은 인자를 감싼 따옴표를 제거한 값
rem set "VAR=값" 형태로 쓰면 줄 끝 공백이 값에 섞이는 실수를 막을 수 있음

if "%CONFIG%"=="" set "CONFIG=Release"
rem 인자 없이 실행했으면 기본값 Release 사용

set "ROOT=%~dp0"
rem %~dp0 = 이 배치 파일이 있는 폴더 경로 (드라이브+경로, 끝에 \ 포함)
rem → 어느 폴더에서 실행하든 배치 파일 위치를 기준으로 동작함

set "BIN=%ROOT%Build\Bin\%CONFIG%-windows-x86_64"
rem 빌드 결과물 폴더 (예: Build\Bin\Release-windows-x86_64)

set "OUT=%ROOT%Build\Package\Hitori"
rem 패키징 결과를 모을 폴더

rem Build messages in English so they are not garbled when read through a pipe (editor Output Log).
set "VSLANG=1033"

rem ---- 1. 빌드 결과물 확인 ----

rem 2) Build HitoriGame and its dependencies (HitoriEngine, Lua).
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%M in (`"%VSWHERE%" -latest -prerelease -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%M"
if not defined MSBUILD (
    echo [Package] MSBuild not found. Install Visual Studio with the C++ workload.
    exit /b 1
)
"%MSBUILD%" "%ROOT%Hitori.slnx" -t:HitoriGame -p:Configuration=%CONFIG% -p:Platform=x64 -m -nologo -v:minimal
if errorlevel 1 (
    echo [Package] Build failed.
    exit /b 1
)

rem 3) Collect the exe and the content folders it reads at runtime.
if not exist "%BIN%\HitoriGame.exe" (
    echo [Package] %BIN%\HitoriGame.exe not found after build.
    exit /b 1
    rem /b: cmd 창 전체가 아니라 이 스크립트만 종료. 1은 실패 코드
)

rem ---- 2. 출력 폴더 초기화 ----
if exist "%OUT%" rmdir /s /q "%OUT%"
rem 이전 패키지가 있으면 통째로 삭제 (/s: 하위 전부, /q: 확인 묻지 않음)
rem → 지난번에 남은 쓰레기 파일이 섞이지 않도록 매번 깨끗하게 시작

mkdir "%OUT%"

copy /y "%BIN%\HitoriGame.exe" "%OUT%\" >nul
rem 실행 파일 복사. /y: 덮어쓰기 확인 생략, >nul: 출력 숨김

copy /y "%BIN%\libfbxsdk.dll" "%OUT%\" >nul
rem FBX SDK를 DLL로 링크하므로 exe 옆에 있어야 실행된다

rem ---- 3. 게임 데이터 폴더 복사 ----
rem Shaders are compiled at runtime, so Resources\Shader must ship as source.
rem 셰이더를 런타임에 컴파일하므로 .hlsl 원본이 Resources\Shader에 같이 들어가야 함

for %%D in (Assets Resources Scenes Scripts Config) do (
    rem 배치 파일 안의 for 변수는 %%D처럼 % 두 개 (cmd 직접 입력 시엔 %D)

    robocopy "%ROOT%%%D" "%OUT%\%%D" /e /njh /njs /nfl /ndl >nul
    rem /e  : 빈 폴더 포함 하위 폴더 전부 복사
    rem /njh: 작업 헤더 숨김, /njs: 요약 숨김
    rem /nfl: 파일 목록 숨김, /ndl: 폴더 목록 숨김

    if errorlevel 8 (
        rem robocopy 종료 코드는 0~7이 성공(복사함/할 것 없음 등), 8 이상이 실패
        rem "if errorlevel 8"은 "== 8"이 아니라 ">= 8"이라는 뜻
        rem (소스 폴더가 아예 없으면 16이 나와서 여기로 들어옴)
        echo [Package] Failed to copy %%D
        exit /b 1
    )
)

echo [Package] Done: %OUT%
endlocal
rem setlocal로 만든 변수 범위 종료