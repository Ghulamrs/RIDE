@echo off
setlocal
set SRC=%~1
set CPP=%~2
set STAGE=%~3
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\bin" "%STAGE%\bin\lib" "%STAGE%\examples"
for %%f in (RStudio.exe RStudioConsole.exe cc1i.exe cxx1i.exe shci.exe vm6747.exe c2s.exe) do (
  if exist "%SRC%\bin\%%f" copy /y "%SRC%\bin\%%f" "%STAGE%\bin\" >nul
)
if exist "%SRC%\bin\lib\*.lib" copy /y "%SRC%\bin\lib\*.lib" "%STAGE%\bin\lib\" >nul
if exist "%SRC%\bin\lib\shmrt-tms6747" xcopy /e /i /q "%SRC%\bin\lib\shmrt-tms6747" "%STAGE%\bin\lib\shmrt-tms6747" >nul
if exist "%CPP%\include" xcopy /e /i /q "%CPP%\include" "%STAGE%\include" >nul
if exist "%CPP%\lib" xcopy /e /i /q "%CPP%\lib" "%STAGE%\lib" >nul
for %%e in (c h cpp shl pro) do (
  if exist "%SRC%\examples\*.%%e" copy /y "%SRC%\examples\*.%%e" "%STAGE%\examples\" >nul
)
if exist "%SRC%\help" xcopy /e /i /q "%SRC%\help" "%STAGE%\help" >nul
if exist "%SRC%\docs" xcopy /e /i /q "%SRC%\docs" "%STAGE%\docs" >nul
if exist "%SRC%\README.md" copy /y "%SRC%\README.md" "%STAGE%\" >nul
echo staged files:
dir /s /b "%STAGE%" | find /c /v ""
endlocal
