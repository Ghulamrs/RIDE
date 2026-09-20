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
for %%f in (RStudio.exe RStudioConsole.exe cc1i.exe cxx1i.exe shci.exe vm6747.exe asm6x.exe masm.exe link.exe lnk6x.exe c2s.exe) do (
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
rem The assembler for x86_64-windows is the project's own, masm.exe beside
rem the editor, named relative to this file so the installation can be put
rem anywhere; the editor makes it absolute against the file. The project's
rem own linker for x86_64-windows ships beside it as link.exe, but is not
rem named here: a program the compilers write links against Microsoft's C
rem runtime, and that linker does not yet search LIB, take the CRT's COMDAT
rem sections or supply the default entry point - so naming it would break
rem every Windows build. Tools > Linker for x86_64-windows... names it for a
rem link it can do. The C6000 linker ships beside it as lnk6x.exe on the same
rem terms: a program's link pulls members out of TI's runtime archive and
rem builds a cinit table, neither of which it does yet, so "tilinker" is not
rem named either; Tools > Linker for tms6747... names it for a link it can do.
set "ASM="
if exist "%STAGE%\bin\masm.exe" set "ASM=bin/masm.exe"
(
  echo {
  echo   "include": "include",
  echo   "lib": "lib",
  echo   "vcvars": "",
  echo   "assembler": "%ASM%",
  echo   "linker": "",
  echo   "ti": "",
  echo   "tilib": "",
  echo   "tilinker": "",
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
