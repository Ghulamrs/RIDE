@echo off
setlocal
set SRC=%~1
set CPP=%~2
set STAGE=%~3
set REL=%SRC%\x64\Release
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\bin" "%STAGE%\bin\lib" "%STAGE%\examples"
for %%f in (RStudio.exe RStudioConsole.exe cc1.exe cxx1.exe shc.exe c2s.exe) do (
  if exist "%REL%\%%f" copy /y "%REL%\%%f" "%STAGE%\bin\" >nul
)
if exist "%REL%\lib\*.lib" copy /y "%REL%\lib\*.lib" "%STAGE%\bin\lib\" >nul
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
