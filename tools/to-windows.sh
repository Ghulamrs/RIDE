#!/usr/bin/env bash
#
# Copies this tree to the Windows box and builds it there.
#
# That box was rebuilt on 2026-08-25 and everything below is about the machine
# as it is now: reached as `ssh windows`, its ssh shell is cmd.exe, and the
# projects are siblings under C:\Users\GRA\source - RIDE (4.0's own
# directory, so the sealed 3.5 tree in RStudio is left alone), VM6747,
# ASM6x, MASM, Converter-C2S - which is the shape RStudio.sln assumes when
# it names ..\VM6747\Compiler-Ci\msvc\cc1.vcxproj and the rest.
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
DIR="$ROOT\\RIDE"
# 3.5: the four VM6747 repositories travel with the editor - they have no
# remote, by that line's rules - laid out on the box as they are here.
# Compiler-Si joined when shci was docked; the Compiler-S beside it on the
# box is the sealed original and is no longer what the solution builds.
VM_ROOT="$ROOT\\VM6747"
CC1I_DIR="$VM_ROOT\\Compiler-Ci"
CXX1_DIR="$VM_ROOT\\Compiler-Cppi"
SHCI_DIR="$VM_ROOT\\Compiler-Si"
EMU_DIR="$VM_ROOT\\Emulator"
ASM_DIR="$ROOT\\ASM6x"
MASM_DIR="$ROOT\\MASM"
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
( cd ../VM6747/Compiler-Ci && tar --no-mac-metadata --exclude '* 2.*' --exclude 'obj' --exclude '*.exe' --exclude 'out-*' \
    -czf "$TMP/cc1i-src.tgz" src lib msvc tests examples Makefile README.md ) || exit 2
( cd ../VM6747/Compiler-Cppi && tar --no-mac-metadata --exclude '* 2.*' --exclude 'obj' --exclude '*.exe' --exclude 'out-*' \
    -czf "$TMP/cxx1-src.tgz" src include lib msvc tests Makefile cxx1.vcxproj README.md ) || exit 2
( cd ../VM6747/Emulator && tar --no-mac-metadata --exclude '* 2.*' --exclude '*.exe' \
    -czf "$TMP/vm6747-src.tgz" src msvc tests Makefile vm6747.vcxproj README.md ) || exit 2
# asm6x: the C6000 assembler, beside this checkout as ../ASM6x, its own repository.
( cd ../ASM6x && tar --no-mac-metadata --exclude '* 2.*' --exclude '*.exe' --exclude 'build' \
    -czf "$TMP/asm6x-src.tgz" src tests Makefile asm6x.vcxproj README.md ) || exit 2
# masm: the x86-64 assembler, beside this checkout as ../MASM, its own repository.
( cd ../MASM && tar --no-mac-metadata --exclude '* 2.*' --exclude '*.exe' --exclude 'build' \
    -czf "$TMP/masm-src.tgz" src tests Makefile masm.vcxproj README.md ) || exit 2
# shci: what shc.vcxproj compiles - src and the runtime it builds beside the
# binary - and nothing built here; lib/ holds this machine's archives.
( cd ../VM6747/Compiler-Si && tar --no-mac-metadata --exclude '* 2.*' --exclude '*.exe' --exclude 'lib' --exclude 'out-*' \
    -czf "$TMP/shci-src.tgz" src runtime tests examples Makefile build.bat shc.vcxproj README.md ) || exit 2

say "copying to $BOX:$DIR and $VM_ROOT"
# One directory per call: in cmd, `if not exist X mkdir X & if ...` makes the
# second `if` part of the first one's body, so it runs only when X was missing.
for d in "$DIR" "$CC1I_DIR" "$CXX1_DIR" "$SHCI_DIR" "$EMU_DIR" "$ASM_DIR" "$MASM_DIR"; do
  ssh -n "$BOX" "if not exist \"$d\" mkdir \"$d\"" || exit 2
done
scp -q "$TMP/rstudio-src.tgz" "$BOX:$DIR\\rstudio-src.tgz" || exit 2
scp -q "$TMP/cc1i-src.tgz" "$BOX:$CC1I_DIR\\cc1i-src.tgz" || exit 2
scp -q "$TMP/cxx1-src.tgz" "$BOX:$CXX1_DIR\\cxx1-src.tgz" || exit 2
scp -q "$TMP/vm6747-src.tgz" "$BOX:$EMU_DIR\\vm6747-src.tgz" || exit 2
scp -q "$TMP/asm6x-src.tgz" "$BOX:$ASM_DIR\\asm6x-src.tgz" || exit 2
scp -q "$TMP/masm-src.tgz" "$BOX:$MASM_DIR\\masm-src.tgz" || exit 2
scp -q "$TMP/shci-src.tgz" "$BOX:$SHCI_DIR\\shci-src.tgz" || exit 2

# ---- the script that does the work there -----------------------------------
# One .cmd, generated here so that what runs is what this file says. The
# compilers are named by full path for the suites - the same four make names
# on Unix - and they are the ones the solution just built into bin (via
# /p:OutDir), beside the editor, which is where the editor would find them on
# its own.
BIN="$DIR\\bin"
{
  printf '@echo off\r\n'
  # tests\ is emptied before each archive lands, as to-linux.sh empties it: a
  # case retired or renamed here would otherwise stay there under its old
  # name and be found by whatever globs the directory - which is how the
  # Linux box reported two probes the Mac no longer has (RStudio 19c73e6).
  # Only tests\: the rest is laid over, since these directories also hold
  # hand-run experiments that are not ours.
  for pair in "$DIR rstudio" "$CC1I_DIR cc1i" "$CXX1_DIR cxx1" \
              "$EMU_DIR vm6747" "$ASM_DIR asm6x" "$MASM_DIR masm" "$SHCI_DIR shci"; do
    set -- $pair
    printf 'cd /d "%s" || exit /b 2\r\n' "$1"
    printf 'if exist tests rmdir /s /q tests\r\n'
    printf 'tar -xzf %s-src.tgz || exit /b 2\r\n' "$2"
    printf 'del /q %s-src.tgz\r\n' "$2"
  done
  printf 'cd /d "%s"\r\n' "$DIR"
  printf 'set CC1=%s\\cc1i.exe\r\n' "$BIN"
  printf 'set CXX1=%s\\cxx1i.exe\r\n' "$BIN"
  printf 'set VM6747=%s\\vm6747.exe\r\n' "$BIN"
  printf 'set SHC=%s\\shci.exe\r\n' "$BIN"
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
