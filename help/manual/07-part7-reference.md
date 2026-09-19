# Part VII — Reference: keys, the editor's command line, diagnostics, troubleshooting

--------------------------------------------------------------------------------
## 26. Keys and menus

**The keys** (the console editor; the Windows window differs on ten — see below):

| Key | Does | Key | Does |
|-----|------|-----|------|
| `F1` | the keys | `Ctrl-B` | compile this file |
| `F10` | the menu | `F5` | run this file |
| `F2`/`F3` | previous / next file | `F4` | build the project |
| `F9` | breakpoint | `F8` | debug — start / continue |
| `F7`/`F6` | step over / into | `Ctrl-D` | debug or release |
| `Ctrl-Up`/`Ctrl-Down` | up / down the stack | `Ctrl-T` | next target |
| `Ctrl-K` | next compiler | `Ctrl-L` | line numbers |
| `Ctrl-W` | next pane | `Ctrl-P` | project pane |
| `Ctrl-E` | bottom panel | `Tab` | lay this line out |
| `Ctrl-A` | re-indent selection | `Ctrl-F` | find |
| `Ctrl-G` | find next | `Ctrl-R` | replace |
| `Ctrl-Z`/`Ctrl-Y` | undo / redo | `Ctrl-S` | save |
| `Ctrl-C`/`Ctrl-X`/`Ctrl-V` | copy / cut / paste | `Ctrl-Q` | leave |

In the project pane, Enter opens. In the bottom panel, left/right change tab and
shift+up/down resize it; on Console, Enter jumps to the line the compiler named.

**The Windows window (`RStudio.exe`) differs deliberately on nine keys**, keeping
what a Windows application means by them: `Ctrl+PageDown`/`Ctrl+PageUp` move
between files (`F2`/`F3` are Rename and Find-next there), so **`F3` is Find next
and `Shift-F3` Find previous**; **`Ctrl-W` closes the file** (next pane in the
console); **`Ctrl-A` selects all** and **`Ctrl-L` re-indents** (in the console
`Ctrl-A` re-indents and `Ctrl-L` is line numbers); **`Ctrl-H` is Replace**;
**`Ctrl-N` is New File**; the View menu's `Ctrl-0`…`Ctrl-4` pick the pane and the
tab. Menus are reached with `Alt`+the underlined letter. In the console a
letter typed while the menu is open goes to the next column whose title starts
with it - `T` is Tools, `T` again Target - and F10 opens on File every time.

**The menus:** File, Edit, Project (New / Open / Save as / Close, then New File /
Add File / Remove File / Rename File / Delete File / Move to Group - the last
three act on the file in front, or on the one picked in the project pane, and
the project follows the file; there is no Save, since every change is written
as it is made), Build (Compile / Run / Build project / Run project / Debug /
Release), Debug, View (Project pane / Bottom panel / Console / Debug / Assembly
/ Line numbers / Plain frame), Language (By extension / C / C++ / Shalimar /
JSON / Plain text / Convert), Tools (By language / cc1 / cxx1 / shc / MSVC (cl)
/ C++ (host), then Header directories, the *shared* include paths and libraries
of the installation, the *project's* include paths and libraries in its `.pro`,
and where vcvars64, the assembler and TI's compiler are), Target (the
architectures), Help. The last three — Language, Tools, Target — are one chain: what the file
**is**, which **compiler** reads it, which **machine** it runs on.

--------------------------------------------------------------------------------
## 27. The editor's own command line

    RStudio.exe [file] [--project dir]
        [--toolchain auto|cc1|cxx1|msvc|shc|c++]
        [--config debug|release]
        [--cc1 path] [--cxx1 path] [--cl path] [--shc path]

- A bare `file` opens that file; `--project dir` opens the project in `dir`.
- With nothing, the editor opens nothing: the last three projects it was in
  are remembered and named at the end of the Project menu, never opened on
  their own.
- `--toolchain` and `--config` preset the Tools and Debug/Release choices.
- `--cc1`/`--cxx1`/`--cl`/`--shc` name the compilers explicitly; otherwise the
  editor finds them beside itself (`bin\`) before `PATH`. `$CC1`/`$CXX1`/`$CL`/
  `$SHC` do the same through the environment.

--------------------------------------------------------------------------------
## 28. Reading a diagnostic

A compiler diagnostic names the file, line and column, the severity, and the
message, and the editor turns the top one into the status line and (on a
double-click, or Enter on the Console) jumps the caret there. The compilers
**diagnose at the point of interception** — the error is reported where the rule
was broken, not deferred to a later phase — and they **refuse by name**: an
unsupported construct produces a specific message (e.g. cxx1i's "…is not
supported yet" or "…is C++14, and this compiler is C++11"), not a generic
parser stumble. If you see a message you do not expect from a one-line program,
that message is the truth of what the compiler did.

Two diagnostics worth recognising:

- **`cc1: <file>.cpp looks like C++ … compile it with cxx1`** — you handed the C
  compiler a C++ file. Use `cxx1i` (or let `auto` route it).
- **`<arch> only reaches -S here — switch to <host> to run it`** — a foreign
  target: the assembly is produced, but this host cannot assemble/link it.

--------------------------------------------------------------------------------
## 29. Troubleshooting — why a thing fails

- **`'ml64.exe' is not recognized`** — the assemble step ran without a Visual
  Studio environment. The compiler normally sources `vcvars64.bat` itself; if a
  hand-run build hits this, run from a Developer Command Prompt or let the
  compiler find VS. Never a code fault.
- **A build "does nothing" / runs an old program** — a stale binary. On Windows,
  confirm the editor and compilers are the ones you just built (they must sit
  together in `bin\`); an editor without its compilers beside it says so in its
  About box.
- **`cc1i`/`cxx1i` refuses a file by suffix** — the language guard. `.c` → cc1i,
  `.cpp` → cxx1i; use the matching compiler or the Language menu.
- **A C++ feature is refused** — check it against Part II chapter 8 and the C++
  compiler's `docs/EXCLUSIONS.md`; cxx1i is C++11 minus a documented list, and it
  refuses by name.
- **`-g` refused for `x86_64-windows`** — MASM carries no line table; use
  `-masm=gnu` for a steppable DWARF build, or build the C++ with `cl` for
  CodeView (Part V chapter 22).
- **Shalimar won't share a build with C** — by design; build two programs (Part
  III chapter 13).
- **`ti-build` says "TI CGT not found"** — install TI's free C6000 Code
  Generation Tools, or set `RSTUDIO_TI_CGT`; the `vm6747` emulator runs tms6747
  without it.
- **`ti-build` first run is slow / "building the runtime"** — it is building the
  EH runtime once into `%LOCALAPPDATA%\RStudio\tilib`; needs a POSIX `sh` (Git
  for Windows) on `PATH`. Subsequent runs are fast.
- **Total failure of a suite on Windows only** — read it as a line-ending
  question (MSVC writes CRLF, golden files are LF) before a compiler fault.

--------------------------------------------------------------------------------
## 30. Glossary

- **Target** — the machine the code is generated for (`x86_64-windows`,
  `x86_64-linux`, `arm64-darwin`, `tms6747`). Distinct from the host.
- **Host** — the machine you are compiling on.
- **i-line** — `cc1i`/`cxx1i`/`shci`, the compilers RIDE 3.5 drives; the same
  three compilers as 3.0's originals, one target on (tms6747).
- **`-S` / `-c`** — stop at assembly / stop at an object. No flag: build a
  program.
- **`.pro`** — the project file (JSON): name, toolchain, arch, groups, build.
- **Group** — a named list of files in a project; the build selects by group; a
  group may name its own compiler.
- **The runtime (`shmrt-*`)** — the Shalimar support library, in `bin\lib\`
  beside `shci`.
- **`vm6747`** — the C6000 instruction-set emulator; runs tms6747 assembly with
  no TI tools.
- **CGT** — TI's Code Generation Tools (`cl6x`/`asm6x`/`lnk6x`/`hex6x`), used by
  the optional `ti-build` path, found on the machine, never shipped by us.
- **MASM / ml64** — Microsoft's assembler; the `x86_64-windows` assembly dialect.
- **DWARF / CodeView** — the two debug-info formats; DWARF on the GNU targets,
  CodeView only via `cl`.

--------------------------------------------------------------------------------
*This manual describes RIDE 3.5. RIDE 3.0 is the same editor with three
languages and three targets — no tms6747, no emulator, no TI build path, and the
frozen original compilers. Where a chapter is target-specific, 3.0 has the first
three targets only.*
