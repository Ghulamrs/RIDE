@echo off
rem Builds WinConsole with MSVC, which is how it is built on the machine it is
rem meant for. There is no make on that box, and none is needed: a couple of
rem dozen translation units and one link.
rem
rem RStudioConsole.exe is this project's console editor on Windows.
rem On Windows RStudio.exe is the window - which is what somebody there runs -
rem and on a Mac or Linux, where there is no window, RStudio.exe is this one. - the same source as
rem ed1 on Linux and macOS, over the Windows half of the terminal, and named for
rem the machine it runs on so that the three variants can be told apart where
rem they are installed. See "The three variants" in the README.
rem
rem   build            builds RStudioConsole.exe
rem   build test       builds it, then builds and runs the unit tests
rem   build session    builds it, then drives the editor itself with keystrokes
rem   build check      both
rem
rem Run it from a Developer Command Prompt, or run it from anywhere and let it
rem find vcvars64 itself.
rem
rem The search is pinned to Visual Studio 2022 - the [17.0,18.0) below. A bare
rem "vswhere -latest" reaches past it to a newer Visual Studio if one is
rem installed, which is not the toolset this is built with.
rem
rem _CRT_SECURE_NO_WARNINGS is defined for the same reason cc1's own project
rem defines it: getenv and strerror are standard C++17, and MSVC's objection to
rem them is house policy rather than a defect to go and fix.
setlocal

rem Before anything is built, because this one only looks at files. It needs no
rem compiler and no Visual Studio environment, and a check that rebuilds the
rem editor before answering is a check nobody runs.
if "%1"=="confirm" goto :confirm

if not "%VSCMD_ARG_TGT_ARCH%"=="x64" call :findvcvars
if errorlevel 1 goto :fail

if not exist obj mkdir obj

rem **Into x64\Release, which is where the compilers are.** This built the
rem console editor into the repository root until 2026-08-27, and an editor
rem there has nothing beside it: the solution puts cc1.exe, shc.exe, c2s.exe
rem and shc's lib\ in x64\Release, and RStudio finds what it drives with
rem path::besideProgram before it looks at PATH. So a `build.bat` editor could
rem not compile anything without $CC1 being named, and said so in its own About
rem box - three times over, "not beside this program". The two ways of building
rem the editor here now write to the same directory, which is what make does on
rem Unix with BINDIR.
rem
rem The stale root copy goes with it. Two RStudioConsole.exe in one tree is a
rem tree where nobody can say which one they ran, and this one would be the
rem older every time from now on.
if "%BINDIR%"=="" set BINDIR=x64\Release
if not exist "%BINDIR%" mkdir "%BINDIR%"
if exist RStudioConsole.exe del RStudioConsole.exe

cl /nologo /std:c++14 /W4 /WX /EHsc /permissive- /O2 /D_CRT_SECURE_NO_WARNINGS ^
   /Fe:%BINDIR%\RStudioConsole.exe /Fo:obj\ ^
   src\main.cpp src\editor.cpp src\buffer.cpp src\compile.cpp src\convert.cpp ^
   src\indent.cpp src\menu.cpp src\tree.cpp src\syntax.cpp src\toolchain.cpp ^
   src\json.cpp src\project.cpp src\find.cpp src\utf8.cpp src\workspace.cpp src\symbols.cpp src\demangle_win.cpp ^
   src\path.cpp src\process.cpp src\debugger.cpp src\settings.cpp src\about.cpp src\help.cpp ^
   src\shalimar\channel.cpp src\shalimar\session.cpp ^
   src\terminal_common.cpp ^
   src\terminal_win.cpp
if errorlevel 1 goto :fail

if "%1"=="solution" goto :solution
if "%1"=="gui" goto :gui
if "%1"=="product" goto :product
if "%1"=="test" goto :unit
if "%1"=="check" goto :unit
if "%1"=="session" goto :session
goto :done

:gui
rem The window, which is C++/CLI and which the console build above never
rem compiles. Run from here for the same reason the solution is: msbuild
rem reaches PATH only after vcvars64.bat, which the top of this file has
rem already found, so nothing else has to know where Visual Studio is.
msbuild winforms\RStudioGui.vcxproj /p:Configuration=Release /p:Platform=x64 /v:minimal
if errorlevel 1 goto :fail
echo built RStudio.exe (the window)
exit /b 0

:solution
rem **All four programs into one directory.** cc1, shc, this editor's console
rem half and the window, with both editors depending on both compilers so a
rem change to one and the change to the editor that goes with it are a single
rem build. This is what workspace.mk is on Unix.
rem
rem They land in x64\Release\ together, which is the whole point: the editor
rem finds the compilers it drives beside itself before it looks at PATH, so a
rem build that scatters them is a build you cannot run from. Two of the four
rem always landed there; cc1 did not, because cc1.vcxproj set OutDir to its own
rem project directory, and the window did not, because it was not in the
rem solution at all. Both fixed at the source rather than by copying afterwards
rem - see the comments in cc1.vcxproj and shc.vcxproj.
rem
rem Run from here rather than by calling msbuild directly, because msbuild is
rem on PATH only after vcvars64.bat - which the top of this file has already
rem found. One place knows where Visual Studio is.
rem
rem RStudio.sln reaches ..\Compiler-C, ..\Compiler-Cpp, ..\Compiler-S and
rem ..\Converter-C2S, so all five have to be checked out beside each other on
rem this machine - and the C++ compiler's checkout under the repository's
rem name, Compiler-Cpp, which is what the solution says.
msbuild RStudio.sln /p:Configuration=Release /p:Platform=x64 /v:minimal /m
if errorlevel 1 goto :fail
echo built the solution
goto :confirm

:confirm
rem What the editor drives, and the check that it is actually there.
rem
rem The editor links against none of it. It *runs* these, and finds them beside
rem itself, so "built" and "usable" are two different states and nothing
rem checked the second until now. A shc.exe standing without its runtime
rem compiles, writes correct assembly and then dies at the link - which no
rem build and no suite could see, because -S needs no runtime. It waited for
rem somebody to press Run on a Shalimar file, and then read as a broken
rem compiler rather than an incomplete directory.
rem
rem c2s.exe is named too. The Makefile's confirm has always listed it and this
rem did not, so the Windows check was one binary weaker than the Unix one - and
rem the converter is exactly the kind of thing that goes missing, being the one
rem of the four that nothing links and only the Language menu runs.
rem
rem Both runtime archives are named. Debug is the editor's default
rem configuration and links the other one, so checking a single archive would
rem confirm exactly the half that was not about to be used.
if "%BINDIR%"=="" set BINDIR=x64\Release
set MISSING=0
for %%f in (cc1.exe cxx1.exe shc.exe c2s.exe lib\shmrt-x86_64-windows.lib lib\shmrt-x86_64-windows-debug.lib) do (
   if exist "%BINDIR%\%%f" (echo   ok       %%f) else (echo   MISSING  %%f& set MISSING=1)
)
if "%MISSING%"=="1" (
   echo.
   echo RStudio is in %BINDIR% without what it drives. Build the solution with
   echo "build.bat solution", or name them with %%CC1%%, %%CXX1%% and %%SHC%%.
   goto :fail
)
echo.
echo RStudio and everything it drives are in %BINDIR%
exit /b 0

:product
rem The product, as against the build: one directory holding what you would
rem actually run, away from the project space it was compiled in. Both Windows
rem variants land here side by side - the console one and the window - and
rem is copied when msbuild has made it and passed over when it has not.
if "%BINDIR%"=="" set BINDIR=x64\Release
set PRODUCT=%USERPROFILE%\cc1-studio
rem Emptied first, the same as the Makefile's rule and for the same reason: a
rem binary that was renamed leaves its old self here, and a directory holding
rem two names is one where nobody can say which was run.
rem
rem The two directories this fills, and not %PRODUCT% itself - see the Makefile,
rem where PRODUCT is the caller's to set and a wholesale delete is a foot-gun.
rem Here it is fixed, but the two rules are kept the same shape on purpose.
if exist "%PRODUCT%\bin" rmdir /s /q "%PRODUCT%\bin"
if exist "%PRODUCT%\examples" rmdir /s /q "%PRODUCT%\examples"
if not exist "%PRODUCT%\bin" mkdir "%PRODUCT%\bin"
if not exist "%PRODUCT%\examples" mkdir "%PRODUCT%\examples"
copy /y "%BINDIR%\RStudioConsole.exe" "%PRODUCT%\bin\" >nul
if exist winforms\x64\Release\RStudio.exe copy /y winforms\x64\Release\RStudio.exe "%PRODUCT%\bin\" >nul

rem And what the editor drives, from wherever the solution built it. This used
rem to ship the editor alone, so the cc1.exe sitting in that bin\ was whatever
rem somebody had copied there by hand - on this machine a build from four days
rem earlier that nothing refreshed. An editor without its compilers is not a
rem product; it is half of one that fails at the first Ctrl-B.
rem
rem shc's runtime goes too, and into bin\lib\ rather than anywhere tidier,
rem because that is where shc looks: beside its own binary.
if "%BINDIR%"=="" set BINDIR=x64\Release
if exist "%BINDIR%\cc1.exe" copy /y "%BINDIR%\cc1.exe" "%PRODUCT%\bin\" >nul
if exist "%BINDIR%\cxx1.exe" copy /y "%BINDIR%\cxx1.exe" "%PRODUCT%\bin\" >nul
rem cxx1's headers go with it: it looks for include\ and lib\ beside its
rem binary and then one directory up, and falls back to the paths compiled
rem into it, which name the checkout. One up, because bin\lib\ is shc's.
if "%CXX1_DIR%"=="" set CXX1_DIR=..\Compiler-Cpp
if exist "%PRODUCT%\include" rmdir /s /q "%PRODUCT%\include"
if exist "%PRODUCT%\lib" rmdir /s /q "%PRODUCT%\lib"
if exist "%CXX1_DIR%\include" xcopy /e /i /q "%CXX1_DIR%\include" "%PRODUCT%\include" >nul
if exist "%CXX1_DIR%\lib" xcopy /e /i /q "%CXX1_DIR%\lib" "%PRODUCT%\lib" >nul
if exist "%BINDIR%\shc.exe" copy /y "%BINDIR%\shc.exe" "%PRODUCT%\bin\" >nul
if exist "%BINDIR%\RStudio.exe" copy /y "%BINDIR%\RStudio.exe" "%PRODUCT%\bin\" >nul
if exist "%BINDIR%\lib\*.lib" (
   if not exist "%PRODUCT%\bin\lib" mkdir "%PRODUCT%\bin\lib"
   copy /y "%BINDIR%\lib\*.lib" "%PRODUCT%\bin\lib\" >nul
)
copy /y README.md "%PRODUCT%\" >nul
rem All three languages, and the headers. This copied only *.c and *.cpp until
rem 2026-08-24, which shipped table.cpp and vector3.cpp without the headers they
rem include - neither compiles on arrival - and left out gcd.shl, primes.shl and
rem rotmat.shl altogether, which is every Shalimar program here, in the product
rem whose third language is Shalimar. example.pro goes too, so that there is a
rem project to open rather than only loose files.
copy /y examples\*.c "%PRODUCT%\examples\" >nul
copy /y examples\*.h "%PRODUCT%\examples\" >nul
copy /y examples\*.cpp "%PRODUCT%\examples\" >nul
copy /y examples\*.shl "%PRODUCT%\examples\" >nul
copy /y examples\*.pro "%PRODUCT%\examples\" >nul
echo RStudio is in %PRODUCT%
goto :done


:unit

cl /nologo /std:c++14 /W4 /WX /EHsc /permissive- /D_CRT_SECURE_NO_WARNINGS ^
   /I src /I winforms /Fe:test.exe /Fo:obj\ ^
   tests\test.cpp src\compile.cpp src\convert.cpp src\indent.cpp src\syntax.cpp src\toolchain.cpp ^
   src\json.cpp src\project.cpp src\find.cpp src\buffer.cpp src\utf8.cpp src\workspace.cpp src\symbols.cpp ^
   src\demangle_win.cpp src\path.cpp src\process.cpp src\debugger.cpp ^
   src\settings.cpp src\about.cpp src\help.cpp ^
   src\shalimar\channel.cpp src\shalimar\session.cpp ^
   winforms\bridge.cpp
if errorlevel 1 goto :fail
test.exe
if errorlevel 1 goto :fail
if not "%1"=="check" goto :done

:session
rem Its own object directory. src\shalimar\session.cpp and tests\session.cpp
rem both become session.obj under one /Fo, and the two builds would take it in
rem turns to overwrite each other's - which works, right up until the day
rem something links both.
if not exist obj\harness mkdir obj\harness
cl /nologo /std:c++14 /W4 /WX /EHsc /permissive- /D_CRT_SECURE_NO_WARNINGS ^
   /I src /Fe:session.exe /Fo:obj\harness\ tests\session.cpp src\path.cpp
if errorlevel 1 goto :fail
session.exe %BINDIR%\RStudioConsole.exe %CC1%
if errorlevel 1 goto :fail

:done
echo built %BINDIR%\RStudioConsole.exe
exit /b 0

:findvcvars
rem vswhere's answer goes through a file rather than a for/f. A for/f with a
rem quoted program AND quoted arguments loses a quote pair to cmd's own parsing,
rem and the version range here has both.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" echo could not find vswhere.exe & exit /b 1
"%VSWHERE%" -latest -products * -version "[17.0,18.0)" -property installationPath > "%TEMP%\ed1-vspath.txt"
set VSPATH=
set /p VSPATH=<"%TEMP%\ed1-vspath.txt"
del "%TEMP%\ed1-vspath.txt"
if "%VSPATH%"=="" echo could not find Visual Studio 2022 & exit /b 1
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" echo no vcvars64 under %VSPATH% & exit /b 1
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
