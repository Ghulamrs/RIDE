@echo off
setlocal enabledelayedexpansion
rem ===========================================================================
rem  build-installer.bat - build RIDE end to end and produce its Windows setup.
rem
rem  Steps: compile every compiler project + the RIDE editor, (re)generate the
rem  HTML docs, stage the install tree, and compile the Inno Setup installer.
rem
rem  Usage:   build-installer.bat [4.0|3.5|3.0]
rem  Env overrides (all optional):
rem     CPP    the C++ compiler clone that carries include\ and lib\ headers
rem            (default: <repo>\..\VM6747\Compiler-Cppi, else <repo>\..\Compiler-Cpp)
rem     CC     the C compiler clone whose lib\ holds cc1i's headers
rem            (default: <CPP>\..\Compiler-Ci)
rem     OUT    output directory for the stage tree and the setup.exe
rem            (default: <repo>\dist)
rem     ISCC   full path to Inno Setup's ISCC.exe (auto-detected otherwise)
rem
rem  Run it from anywhere; paths are resolved from the script's own location.
rem  Requires: Visual Studio 2022 build tools (build.bat finds them) and Inno
rem  Setup 6. Python is optional - if absent, the committed HTML docs are used.
rem ===========================================================================

set "VER=%~1"
if "%VER%"=="" set "VER=4.0"
set "NV=%VER:.=%"
set "HERE=%~dp0"
for %%I in ("%HERE%..\..") do set "ROOT=%%~fI"
if "%CPP%"=="" if exist "%ROOT%\..\VM6747\Compiler-Cppi\include" set "CPP=%ROOT%\..\VM6747\Compiler-Cppi"
if "%CPP%"=="" set "CPP=%ROOT%\..\Compiler-Cpp"
if "%CC%"=="" set "CC=%CPP%\..\Compiler-Ci"
if "%OUT%"=="" set "OUT=%ROOT%\dist"
set "STAGE=%OUT%\stage%NV%"

echo ===========================================================================
echo  RIDE %VER% installer build
echo    repo    : %ROOT%
echo    headers : %CPP% (include), %CC% (lib)
echo    output  : %OUT%
echo ===========================================================================

echo [1/6] Building the compilers and the RIDE editor (build.bat solution) ...
pushd "%ROOT%"
call build.bat solution
if errorlevel 1 (echo   BUILD FAILED & popd & exit /b 1)
popd

echo [2/6] Generating the HTML docs ...
call :genhtml

echo [3/6] Staging the install tree ...
if not exist "%OUT%" mkdir "%OUT%"
call "%HERE%stage.cmd" "%ROOT%" "%CPP%" "%STAGE%" "%CC%"
if errorlevel 1 (echo   STAGE FAILED & exit /b 1)

echo [4/6] Bundling Express Help and (3.5, 4.0) the TI build path ...
copy /y "%HERE%EXPRESS-HELP-%VER%.md"   "%STAGE%\EXPRESS-HELP.md"   >nul
if exist "%HERE%EXPRESS-HELP-%VER%.html" copy /y "%HERE%EXPRESS-HELP-%VER%.html" "%STAGE%\EXPRESS-HELP.html" >nul
if not "%VER%"=="3.0" (
  if not exist "%STAGE%\bin\ti" mkdir "%STAGE%\bin\ti"
  copy /y "%HERE%ti-build.cmd" "%STAGE%\bin\ti\" >nul
  copy /y "%HERE%ti-link.cmd"  "%STAGE%\bin\ti\" >nul
  copy /y "%HERE%TI-BUILD.txt" "%STAGE%\bin\ti\" >nul
)

echo [5/6] Compiling the installer (Inno Setup) ...
if "%ISCC%"=="" set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (echo   ISCC.exe not found - set ISCC=... & exit /b 1)
"%ISCC%" /DStage="%STAGE%" /DOutDir="%OUT%" "%HERE%RStudio-%VER%.iss"
if errorlevel 1 (echo   INNO FAILED & exit /b 1)

echo [6/6] Done.  %OUT%\RIDE-%VER%-setup.exe
exit /b 0

rem ---- HTML docs ------------------------------------------------------------
:genhtml
set "PY="
where python >nul 2>&1 && set "PY=python"
if "%PY%"=="" where py >nul 2>&1 && set "PY=py"
if "%PY%"=="" (echo    Python not found - using the committed HTML docs. & goto :eof)
set "MAN="
for %%f in ("%ROOT%\help\manual\*.md") do set "MAN=!MAN! "%%f""
%PY% "%HERE%docs2html.py" "%ROOT%\help\manual.html" "RIDE %VER% - The Complete Manual" "C, C++ and Shalimar - four targets - one editor" !MAN!
%PY% "%HERE%docs2html.py" "%ROOT%\help\guide.html" "RIDE %VER% - User Guide" "Using the editor" ^
  "%ROOT%\help\01-what-it-is.md" "%ROOT%\help\02-getting-started.md" "%ROOT%\help\03-the-screen.md" ^
  "%ROOT%\help\04-editing.md" "%ROOT%\help\05-finding.md" "%ROOT%\help\06-the-project.md" ^
  "%ROOT%\help\07-building.md" "%ROOT%\help\08-debugging.md" "%ROOT%\help\09-the-panel.md" ^
  "%ROOT%\help\10-keys.md" "%ROOT%\help\c.md" "%ROOT%\help\cpp.md" "%ROOT%\help\shalimar.md" ^
  "%ROOT%\help\mixing-c-and-shalimar.md" "%ROOT%\help\appendix-a-shalimar-language.md"
%PY% "%HERE%docs2html.py" "%HERE%EXPRESS-HELP-%VER%.html" "RIDE %VER% - Express Help" "Quick reference" "%HERE%EXPRESS-HELP-%VER%.md"
goto :eof
