@echo off
setlocal
set SRC=%~1
set CPP=%~2
set STAGE=%~3
rem The C compiler clone, for lib\ - cc1i's headers. Beside the C++ one unless named.
set CC=%~4
if "%CC%"=="" set "CC=%CPP%\..\Compiler-Ci"
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\bin" "%STAGE%\bin\lib" "%STAGE%\examples"
for %%f in (RStudio.exe RStudioConsole.exe cc1i.exe cxx1i.exe shci.exe vm6747.exe asm6x.exe c2s.exe) do (
  if exist "%SRC%\bin\%%f" copy /y "%SRC%\bin\%%f" "%STAGE%\bin\" >nul
)
if exist "%SRC%\bin\lib\*.lib" copy /y "%SRC%\bin\lib\*.lib" "%STAGE%\bin\lib\" >nul
if exist "%SRC%\bin\lib\shmrt-tms6747" xcopy /e /i /q "%SRC%\bin\lib\shmrt-tms6747" "%STAGE%\bin\lib\shmrt-tms6747" >nul
rem include\ is cxx1i's - its C++ headers and the C ones they wrap, in one
rem directory; lib\ is cc1i's. Each compiler looks one directory above its
rem bin\ for its own, and settings.json beside them says so for the editor.
if exist "%CPP%\include" xcopy /e /i /q "%CPP%\include" "%STAGE%\include" >nul
if exist "%CPP%\lib\*.h" copy /y "%CPP%\lib\*.h" "%STAGE%\include\" >nul
if exist "%CC%\lib" xcopy /e /i /q "%CC%\lib" "%STAGE%\lib" >nul
(
  echo {
  echo   "include": "include",
  echo   "lib": "lib",
  echo   "vcvars": "",
  echo   "assembler": "",
  echo   "ti": "",
  echo   "tilib": "",
  echo   "compiler": "auto",
  echo   "indent": 4,
  echo   "tabs": false,
  echo   "font": "",
  echo   "includes": [],
  echo   "libraries": []
  echo }
) > "%STAGE%\settings.json"
for %%e in (c h cpp shl pro) do (
  if exist "%SRC%\examples\*.%%e" copy /y "%SRC%\examples\*.%%e" "%STAGE%\examples\" >nul
)
if exist "%SRC%\help" xcopy /e /i /q "%SRC%\help" "%STAGE%\help" >nul
if exist "%SRC%\docs" xcopy /e /i /q "%SRC%\docs" "%STAGE%\docs" >nul
if exist "%SRC%\README.md" copy /y "%SRC%\README.md" "%STAGE%\" >nul
echo staged files:
dir /s /b "%STAGE%" | find /c /v ""
endlocal
