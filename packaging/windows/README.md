# Windows installers (Inno Setup)

`setup.exe` installers for RStudio, built on the Windows box with Inno Setup 6
(`winget install JRSoftware.InnoSetup`). Two versions:

- **RStudio 3.5** — three languages, **four** targets (incl. `tms6747` with the
  `vm6747` emulator), the C↔Shalimar converter, and a **license-safe TI build
  path** (`bin\ti\ti-build.cmd`) that produces a real C674x `.out`/`.hex` using
  a TI CGT install found on the machine (no TI binaries are shipped).
- **RStudio 3.0** — three languages, **three** targets, no emulator (the frozen
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
3. `mkinstaller.cmd RStudio-3.5.iss` (and `RStudio-3.0.iss`) → `RStudio-<ver>-setup.exe`.

The `.iss` files name box-local stage paths (`C:\Users\GRA\rstudio-pkg\...`);
adjust the `Stage` define for another machine.

## Layout the installer lays down

    <install>\bin\     RStudio.exe, RStudioConsole.exe, the compilers, (3.5) vm6747, c2s
    <install>\bin\lib\ Shalimar runtime (.lib) and (3.5) shmrt-tms6747\*.s
    <install>\bin\ti\  (3.5) ti-build.cmd - real-silicon TI build path
    <install>\include\ C++ headers      <install>\lib\ C headers
    <install>\examples\  <install>\help\  <install>\docs\  EXPRESS-HELP.md
