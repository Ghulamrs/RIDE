# Windows installers (Inno Setup)

`setup.exe` installers for RIDE, built on the Windows box with Inno Setup 6
(`winget install JRSoftware.InnoSetup`). Two versions:

- **RIDE 3.5** — three languages, **four** targets (incl. `tms6747` with the
  `vm6747` emulator), the C↔Shalimar converter, and a **license-safe TI build
  path** (`bin\ti\ti-build.cmd`) that produces a real C674x `.out`/`.hex` using
  a TI CGT install found on the machine (no TI binaries are shipped).
- **RIDE 3.0** — three languages, **three** targets, no emulator (the frozen
  originals cc1/cxx1/shc).

## How to build (on the box)

1. Build the workspace so the binaries exist:
   - 3.5: `tools/to-windows.sh` (into `C:\Users\GRA\source\RStudio\bin`).
   - 3.0: a worktree at `8be81ca`, `ED1_WINDOWS_ROOT=...\source30 tools/to-windows.sh`
     (into `...\source30\RStudio\x64\Release`), with the four originals relayed.
     `8be81ca` predates the 15-part manual, so copy `help/manual/` from `main`
     into `...\source30\RStudio\help\manual\` before staging — otherwise the 3.0
     package silently drops the manual and its Start-menu "Manual" shortcut (and
     the Express Help's "full manual" note) point at files that are not there.
2. Stage the install tree:
   - `stage.cmd   <RStudio> <Compiler-Cppi> <stage35>`  (3.5, sources from `bin\`)
   - `stage30.cmd <RStudio> <Compiler-Cpp>  <stage30>`  (3.0, sources from `x64\Release`)
   then copy `EXPRESS-HELP-<ver>.md` to the stage root as `EXPRESS-HELP.md`, and
   (3.5 only) `ti-build.cmd`+`ti-link.cmd`+`TI-BUILD.txt` into `stage35\bin\ti\`.
3. `mkinstaller.cmd RStudio-3.5.iss` (and `RStudio-3.0.iss`) → `RIDE-<ver>-setup.exe`.

## One-shot build scripts

`build-installer.bat` (Windows) and `build-installer.sh` (Linux/macOS) do the
whole job in one command: compile every compiler project and the RIDE editor,
regenerate the HTML docs, stage the tree, and produce the installer.

    build-installer.bat 3.5        REM -> dist\RIDE-3.5-setup.exe (Inno Setup)
    ./build-installer.sh 3.5       #   -> dist/RIDE-3.5-<os>.tar.gz (no Inno on Unix)

Both take the version as `%1`/`$1` (default 3.5) and honour `CPP` (the C++ clone
with `include/` and `lib/`) and `OUT` (output dir) as overrides. The `.iss` take
`/DStage=` and `/DOutDir=` so the stage/output paths are not hard-coded. HTML is
regenerated with `docs2html.py` when Python is present; otherwise the committed
`help/manual.html`, `help/guide.html` and `EXPRESS-HELP-<ver>.html` are used.

The `.iss` files name box-local stage paths (`C:\Users\GRA\rstudio-pkg\...`);
adjust the `Stage` define for another machine.

## Layout the installer lays down

    <install>\bin\     RStudio.exe, RStudioConsole.exe, the compilers, (3.5) vm6747, asm6x, c2s
    <install>\bin\lib\ Shalimar runtime (.lib) and (3.5) shmrt-tms6747\*.s
    <install>\bin\ti\  (3.5) ti-build.cmd - real-silicon TI build path
    <install>\include\ C++ headers      <install>\lib\ C headers
    <install>\examples\  <install>\help\  <install>\docs\  EXPRESS-HELP.md
