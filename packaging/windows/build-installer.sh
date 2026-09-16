#!/usr/bin/env bash
# =============================================================================
#  build-installer.sh - build RIDE end to end on Linux/macOS and package it.
#
#  Steps: compile every compiler + the RIDE editor (make -f workspace.mk),
#  (re)generate the HTML docs, stage the install tree, and package it as a
#  .tar.gz.  Inno Setup is Windows-only, so on these platforms the deliverable
#  is a relocatable tarball (unpack and run bin/RStudio); use build-installer.bat
#  on Windows for a real setup.exe.
#
#  Usage:   ./build-installer.sh [3.5]
#  Env overrides (optional):
#     CPP    the C++ compiler clone carrying include/ (C++) and lib/ (C) headers
#            (default: first of <repo>/../Compiler-Cppi, <repo>/../C++)
#     OUT    output directory for the stage tree and the tarball
#            (default: <repo>/dist)
#     PY     python interpreter (default: python3)
# =============================================================================
set -euo pipefail

VER="${1:-3.5}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
PY="${PY:-python3}"
OUT="${OUT:-$ROOT/dist}"
STAGE="$OUT/RIDE-$VER"

# Locate the C++ clone that carries the shipped headers.
if [ -z "${CPP:-}" ]; then
  for c in "$ROOT/../VM6747/Compiler-Cppi" "$ROOT/../Compiler-Cppi" "$ROOT/../C++" "$ROOT/../Compiler-Cpp"; do
    [ -d "$c/include" ] && CPP="$c" && break
  done
fi
: "${CPP:?set CPP to the C++ clone that has include/ and lib/}"
CC="${CC:-$CPP/../Compiler-Ci}"

echo "==========================================================================="
echo " RIDE $VER build (Linux/macOS)"
echo "   repo    : $ROOT"
echo "   headers : $CPP (include), $CC (lib)"
echo "   output  : $OUT"
echo "==========================================================================="

echo "[1/6] Building the compilers and the RIDE editor (make -f workspace.mk) ..."
( cd "$ROOT" && make -f workspace.mk )

echo "[2/6] Generating the HTML docs ..."
if command -v "$PY" >/dev/null 2>&1; then
  "$PY" "$HERE/docs2html.py" "$ROOT/help/manual.html" \
      "RIDE $VER — The Complete Manual" "C, C++ and Shalimar · four targets · one editor" \
      "$ROOT"/help/manual/*.md
  "$PY" "$HERE/docs2html.py" "$ROOT/help/guide.html" "RIDE $VER — User Guide" "Using the editor" \
      "$ROOT"/help/01-what-it-is.md "$ROOT"/help/02-getting-started.md "$ROOT"/help/03-the-screen.md \
      "$ROOT"/help/04-editing.md "$ROOT"/help/05-finding.md "$ROOT"/help/06-the-project.md \
      "$ROOT"/help/07-building.md "$ROOT"/help/08-debugging.md "$ROOT"/help/09-the-panel.md \
      "$ROOT"/help/10-keys.md "$ROOT"/help/c.md "$ROOT"/help/cpp.md "$ROOT"/help/shalimar.md \
      "$ROOT"/help/mixing-c-and-shalimar.md "$ROOT"/help/appendix-a-shalimar-language.md
  "$PY" "$HERE/docs2html.py" "$HERE/EXPRESS-HELP-$VER.html" "RIDE $VER — Express Help" \
      "Quick reference" "$HERE/EXPRESS-HELP-$VER.md"
else
  echo "   $PY not found - using the committed HTML docs."
fi

echo "[3/6] Staging the install tree ..."
rm -rf "$STAGE"
mkdir -p "$STAGE/bin/lib" "$STAGE/examples"
# The editor and the compilers, whatever the platform named them (this tree
# builds them with a .exe suffix on every OS); everything in bin/ except the
# build scratch and the runtime subdir.
for f in "$ROOT"/bin/*; do
  b="$(basename "$f")"
  case "$b" in obj|lib) continue ;; esac
  [ -f "$f" ] && cp -f "$f" "$STAGE/bin/"
done
cp -f "$ROOT"/bin/lib/*.a "$STAGE/bin/lib/" 2>/dev/null || true
[ -d "$ROOT/bin/lib/shmrt-tms6747" ] && cp -rf "$ROOT/bin/lib/shmrt-tms6747" "$STAGE/bin/lib/"
# include/ is cxx1i's - its C++ headers and the C ones they wrap, in one
# directory; lib/ is cc1i's. Each compiler looks one directory above its bin/
# for its own, and settings.json beside them says so for the editor.
[ -d "$CPP/include" ] && cp -rf "$CPP/include" "$STAGE/include"
[ -d "$CPP/lib" ] && cp -f "$CPP"/lib/*.h "$STAGE/include/"
[ -d "$CC/lib" ] && cp -rf "$CC/lib" "$STAGE/lib"
printf '{\n  "include": "include",\n  "lib": "lib",\n  "vcvars": "",\n  "compiler": "auto",\n  "indent": 4,\n  "tabs": false,\n  "font": "",\n  "includes": [],\n  "libraries": []\n}\n' > "$STAGE/settings.json"
[ -d "$ROOT/help" ] && cp -rf "$ROOT/help" "$STAGE/help"
[ -d "$ROOT/docs" ] && cp -rf "$ROOT/docs" "$STAGE/docs"
for e in c h cpp shl pro; do cp -f "$ROOT"/examples/*."$e" "$STAGE/examples/" 2>/dev/null || true; done
[ -f "$ROOT/README.md" ] && cp -f "$ROOT/README.md" "$STAGE/"

echo "[4/6] Bundling Express Help ..."
cp -f "$HERE/EXPRESS-HELP-$VER.md" "$STAGE/EXPRESS-HELP.md"
[ -f "$HERE/EXPRESS-HELP-$VER.html" ] && cp -f "$HERE/EXPRESS-HELP-$VER.html" "$STAGE/EXPRESS-HELP.html"

echo "[5/6] Packaging the tarball ..."
TARBALL="$OUT/RIDE-$VER-$(uname -s | tr 'A-Z' 'a-z').tar.gz"
( cd "$OUT" && tar -czf "$TARBALL" "RIDE-$VER" )

echo "[6/6] Done."
echo "   stage   : $STAGE"
echo "   tarball : $TARBALL"
