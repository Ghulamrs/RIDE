#!/usr/bin/env bash
#
# Copies this tree to the Windows box and builds it there.
#
# That box was rebuilt on 2026-08-25 and everything below is about the machine
# as it is now: reached as `ssh windows`, its ssh shell is cmd.exe, and the
# projects are siblings under C:\Users\GRA\source - RStudio, Compiler-C,
# Compiler-Cpp, Compiler-S, Converter-C2S - which is the shape RStudio.sln
# assumes when it names ..\Compiler-C\msvc\cc1.vcxproj and the rest.
#
# Three rules that each cost an hour before they were written down:
#
#   * A tarball, extracted by Windows' own tar. scp of a directory tree at a
#     time left files behind and nobody noticed; one archive is one thing to
#     check. macOS puts ._ AppleDouble files in the archive unless told not to.
#   * A .cmd file, scp'd over and run by its full path, with no `cmd /c` in
#     front of it. The ssh shell is already cmd, and a command line with quotes
#     in it loses one on the way; a script file has no such problem.
#   * The tree there has no git. What this copies is what is built; a stale
#     file on that side is a stale build with a green suite in front of it.
#
# cxx1 travels with the editor since 3.0. RStudio.sln builds it from
# ..\Compiler-Cpp\cxx1.vcxproj, which is written by tools/make-projects.py at
# the root of the C++ checkout here; the sources, headers, msvc\compat and
# that project go over together, laid over the tree there - never wiping it,
# since that directory also holds hand-run experiments that are not ours.
#
#   ./tools/to-windows.sh              build the solution and run both suites
#   ./tools/to-windows.sh build        the console editor only, no suites
#   ./tools/to-windows.sh gui          also msbuild the window on its own
set -uo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

BOX="${ED1_WINDOWS_BOX:-windows}"
ROOT="${ED1_WINDOWS_ROOT:-C:\\Users\\GRA\\source}"
DIR="$ROOT\\RStudio"
CXX1_DIR="$ROOT\\Compiler-Cpp"
WHAT="${1:-check}"
TMP="${TMPDIR:-/tmp}"

say() { printf '%s\n' "$*"; }

# ---- the editor -----------------------------------------------------------
# Built things are left out by name as well as by suffix: a Mach-O RStudio.exe
# or tests/test that travels over is "newer" than its source there and reads
# as a broken machine. help/ goes because tests/test.cpp checks Help > Contents
# against it, and that check would otherwise run on one machine in three.
tar --no-mac-metadata \
    --exclude 'obj' --exclude '*.o' --exclude '*.d' --exclude '*.exe' \
    --exclude 'tests/test' --exclude 'tests/session' --exclude '* 2.*' \
    --exclude 'lib' --exclude 'x64' --exclude 'DerivedData' \
    -czf "$TMP/rstudio-src.tgz" \
    src tests winforms examples help tools docs packaging \
    Makefile workspace.mk build.bat clean.cmd README.md RStudio.json \
    RStudio.sln RStudioConsole.vcxproj 2>/dev/null || exit 2

# ---- cxx1 ------------------------------------------------------------------
# The parts its Visual Studio project compiles and includes, and nothing of
# its own build tree.
( cd ../C++ && tar --no-mac-metadata --exclude '* 2.*' --exclude 'obj' \
    -czf "$TMP/cxx1-src.tgz" src include lib msvc Makefile cxx1.vcxproj README.md ) || exit 2

say "copying to $BOX:$DIR and $CXX1_DIR"
ssh -n "$BOX" "if not exist \"$DIR\" mkdir \"$DIR\" & if not exist \"$CXX1_DIR\" mkdir \"$CXX1_DIR\"" || exit 2
scp -q "$TMP/rstudio-src.tgz" "$BOX:$DIR\\rstudio-src.tgz" || exit 2
scp -q "$TMP/cxx1-src.tgz" "$BOX:$CXX1_DIR\\cxx1-src.tgz" || exit 2

# ---- the script that does the work there -----------------------------------
# One .cmd, generated here so that what runs is what this file says. The
# compilers are named by full path for the suites - the same four make names
# on Unix - and they are the ones the solution just built into x64\Release,
# beside the editor, which is where the editor would find them on its own.
BIN="$DIR\\x64\\Release"
{
  printf '@echo off\r\n'
  printf 'cd /d "%s" || exit /b 2\r\n' "$DIR"
  printf 'tar -xzf rstudio-src.tgz || exit /b 2\r\n'
  printf 'del /q rstudio-src.tgz\r\n'
  printf 'cd /d "%s" || exit /b 2\r\n' "$CXX1_DIR"
  printf 'tar -xzf cxx1-src.tgz || exit /b 2\r\n'
  printf 'del /q cxx1-src.tgz\r\n'
  printf 'cd /d "%s"\r\n' "$DIR"
  printf 'set CC1=%s\\cc1.exe\r\n' "$BIN"
  printf 'set CXX1=%s\\cxx1.exe\r\n' "$BIN"
  printf 'set SHC=%s\\shc.exe\r\n' "$BIN"
  printf 'set C2S=%s\\c2s.exe\r\n' "$BIN"
  case "$WHAT" in
    build)
      printf 'call build.bat\r\n' ;;
    gui)
      printf 'call build.bat gui\r\n' ;;
    *)
      # The solution first - every program the editor drives, into
      # x64\Release - and then the two suites against exactly those.
      printf 'call build.bat solution\r\n'
      printf 'if errorlevel 1 exit /b 1\r\n'
      printf 'call build.bat check\r\n' ;;
  esac
  printf 'exit /b %%errorlevel%%\r\n'
} > "$TMP/rstudio-run.cmd"
scp -q "$TMP/rstudio-run.cmd" "$BOX:$DIR\\rstudio-run.cmd" || exit 2

say "running $DIR\\rstudio-run.cmd ($WHAT)"
ssh -n "$BOX" "$DIR\\rstudio-run.cmd"
