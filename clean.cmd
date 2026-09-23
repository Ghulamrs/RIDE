@echo off
rem Remove everything the four projects build on Windows, and nothing they are
rem made of. The Unix half of this is clean.sh; this is not a translation of it
rem because the two machines do not leave the same things behind.
rem
rem   clean.cmd        clean
rem   clean.cmd -n     say what would go, remove nothing
rem
rem Run it from the RIDE directory. The four projects are expected as
rem siblings - RIDE\, Compiler-C\, Compiler-S\, Converter-C2S\ - which is
rem what RIDE.sln's ..\ paths require anyway.
rem
rem **Every path here is named, never globbed.** clean.sh can ask git whether a
rem path is source; there is no git on this machine, so the safety has to come
rem from the list itself. `Compiler-C\lib` is sixteen tracked header files and
rem `Compiler-S\lib` is built runtime archives - the same name one directory
rem apart, meaning opposite things - so nothing here removes a directory called
rem lib, and the MSVC build does not produce one at that level in any case.
rem
rem What MSVC leaves that the Makefiles do not: a per-project intermediate
rem directory named after the project (RIDEConsole\, cc1\, shc\, c2s\), the
rem x64\ output tree, and .obj/.pdb/.ilk beside whatever was built by hand.
setlocal EnableDelayedExpansion

set DRY=0
if "%1"=="-n" set DRY=1

set ROOT=%~dp0..
for %%I in ("%ROOT%") do set ROOT=%%~fI

set COUNT=0

echo cleaning under %ROOT%
if "%DRY%"=="1" echo (dry run - nothing will be removed)
echo.

rem ---- the shared object root the Makefiles default to ------------------------
call :dropdir "%ROOT%\build"

rem ---- RIDE ---------------------------------------------------------------
echo RIDE:
call :dropdir "%ROOT%\RIDE\x64"
call :dropdir "%ROOT%\RIDE\obj"
call :dropdir "%ROOT%\RIDE\RIDEConsole"
call :dropdir "%ROOT%\RIDE\winforms\x64"
call :dropdir "%ROOT%\RIDE\winforms\RIDEGui"
call :dropfile "%ROOT%\RIDE\RIDEConsole.exe"
call :dropfile "%ROOT%\RIDE\RIDE.exe"
call :dropfile "%ROOT%\RIDE\test.exe"
call :dropfile "%ROOT%\RIDE\session.exe"
call :dropglob "%ROOT%\RIDE" *.obj
call :dropglob "%ROOT%\RIDE" *.ilk
call :dropglob "%ROOT%\RIDE" *.pdb
echo.

rem ---- Compiler-C ------------------------------------------------------------
echo Compiler-C:
call :dropdir "%ROOT%\Compiler-C\msvc\x64"
call :dropdir "%ROOT%\Compiler-C\msvc\cc1"
call :dropdir "%ROOT%\Compiler-C\obj"
call :dropfile "%ROOT%\Compiler-C\cc1.exe"
call :dropouts "%ROOT%\Compiler-C"
echo.

rem ---- Compiler-S ------------------------------------------------------------
echo Compiler-S:
call :dropdir "%ROOT%\Compiler-S\x64"
call :dropdir "%ROOT%\Compiler-S\shc"
call :dropdir "%ROOT%\Compiler-S\obj"
call :dropfile "%ROOT%\Compiler-S\shc.exe"
call :dropouts "%ROOT%\Compiler-S"
echo.

rem ---- Converter-C2S ---------------------------------------------------------
echo Converter-C2S:
call :dropdir "%ROOT%\Converter-C2S\x64"
call :dropdir "%ROOT%\Converter-C2S\c2s"
call :dropdir "%ROOT%\Converter-C2S\obj"
call :dropfile "%ROOT%\Converter-C2S\c2s.exe"
call :dropouts "%ROOT%\Converter-C2S"
echo.

if "%DRY%"=="1" (echo %COUNT% would be removed.) else (echo %COUNT% removed.)
exit /b 0

rem ---------------------------------------------------------------------------
:dropdir
if not exist %1\ exit /b 0
set /a COUNT+=1
if "%DRY%"=="1" (echo   would go %~1) else (rd /s /q %1 & echo   removed  %~1)
exit /b 0

:dropfile
if not exist %1 exit /b 0
set /a COUNT+=1
if "%DRY%"=="1" (echo   would go %~1) else (del /q %1 & echo   removed  %~1)
exit /b 0

rem A glob, but only over the one directory named and only for the extension
rem given - never recursive, because src\ lives below these and holds .cpp that
rem a careless /s would sit next to.
:dropglob
for %%F in ("%~1\%~2") do (
  set /a COUNT+=1
  if "%DRY%"=="1" (echo   would go %%~fF) else (del /q "%%~fF" & echo   removed  %%~fF)
)
exit /b 0

rem The suites' scratch. tests\out and tests\out-<target>, which the shell
rem suites make and which arrive here inside a relayed tarball.
:dropouts
for /d %%D in ("%~1\tests\out" "%~1\tests\out-*") do (
  set /a COUNT+=1
  if "%DRY%"=="1" (echo   would go %%~fD) else (rd /s /q "%%~fD" & echo   removed  %%~fD)
)
exit /b 0
