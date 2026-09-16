# RIDE 3.5 — Express Help

Three languages, four targets, one editor. This is the quick reference; the
full pages are under `help\`.

--------------------------------------------------------------------------
## 1. Languages, versions and scope

| Language | Files        | Compiler  | Standard / version      | Debug info      |
|----------|--------------|-----------|-------------------------|-----------------|
| C        | `.c` `.h`    | `cc1i`    | ISO C 90                | DWARF (2 of 4)  |
| C++      | `.cpp` `.hpp`| `cxx1i`   | ISO C++ 11              | DWARF (2 of 4)  |
| Shalimar | `.shl`       | `shci`    | Shalimar 1.2            | none, by design |

The suffix picks the language; **Language** menu overrides it for a file whose
name says otherwise. Each compiler announces itself at the start of a compile
(e.g. `©2026 G. R. Akhtar - ISO C++ 11`).

**The four targets** (Target menu, or Ctrl-T to cycle):

| Target            | Runs here?                        |
|-------------------|-----------------------------------|
| `x86_64-windows`  | yes, on this machine (host)       |
| `x86_64-linux`    | assembly only (`-S`) on Windows   |
| `arm64-darwin`    | assembly only (`-S`) on Windows   |
| `tms6747`         | yes, on the **vm6747 emulator**   |

Scope of a compiler: `cc1i` compiles C only (a `.cpp` is refused with a message,
not a cryptic error); `cxx1i` compiles C++ only (a `.c` is refused); `shci`
compiles Shalimar only. The host compiler (`cl`) is still reachable per group.

--------------------------------------------------------------------------
## 2. Making and updating a project

All on the **Project** menu:

- **New…** — pick a folder and a name; writes `<name>.pro` and opens it.
- **New File** (Ctrl-N) — names a new file, creates it, adds it to the project.
- **Add File** — adds the file in front of you to a group you name.
- **Remove File** — takes the open file out of the project (leaves it on disk).
- **Save** — writes the `.pro` back. (New/Add/Remove save it for you.)

The title bar shows where you are: `RStudio 3.5 - <project> - <file>`. The
top-right of the menu bar shows the compiler in use (a `*` means the file chose
it). Every add and remove is reflected immediately in the `.pro` file.

--------------------------------------------------------------------------
## 3. The project file (`.pro`) architecture

A `.pro` is one JSON object:

    {
      "name": "demo",
      "toolchain": "auto",           // auto | cc1 | cxx1 | shc | msvc
      "arch": "x86_64-windows",      // one of the four targets
      "indent": 4,
      "groups": {
        "Sources": ["greet.c", "main.c"],
        "Headers": ["greet.h"]
      },
      "build": { "target": "demo", "groups": ["Sources"] }
    }

- **groups** name the files, in headings shown in the left pane. A group may
  set its own `"toolchain"`.
- **build** names the program (`target`) and which groups compile into it;
  headers and un-named groups are passed over. A group holding C *and* C++ is
  split — the C to `cc1i`, the C++ to `cxx1i` — and the objects are linked.
- Say no `build` and nothing is built (Ctrl-B still compiles the open file).

--------------------------------------------------------------------------
## 4. Compiling, and building/running a project

- **Ctrl-B** — compile the file in front of you.
- **F5** — compile and run that file.
- **F4** — build the project's program (from `build`).
- **Run project** (Build menu) — build it and run it.
- **Ctrl-T** / **Target** — choose the target; **Ctrl-D** — debug/release.
- The bottom panel has **Console**, **Debug** and **Assembly** (Ctrl-1/2/3).
  The Assembly tab shows the compiler's output for any target.

**tms6747:** F5 / Run project builds with `-S` and runs on the **vm6747**
emulator (`counted to three`, `[program returned 3]`). Foreign targets
(`x86_64-linux`, `arm64-darwin`) reach assembly only and say so.

**Real C674x silicon (.out / .hex):** the emulator is enough to run and test.
To produce a genuine linked `.out` and Intel `.hex`, use:

    bin\ti\ti-build.cmd  <program.c|.cpp|.shl>

It uses a Texas Instruments Code Generation Tools install already on the
machine (none of TI's tools ship here); see `bin\ti\TI-BUILD.txt`.

--------------------------------------------------------------------------
## Where things are

    bin\      the editor (RStudio.exe), the console editor, and the compilers
              cc1i cxx1i shci, the vm6747 emulator, the c2s converter
    bin\lib\  the Shalimar runtime (shmrt-x86_64-windows[-debug].lib) and the
              C6000 runtime (shmrt-tms6747\*.s)
    bin\ti\   ti-build.cmd — the real-silicon TI build path
    include\  cxx1i's headers: C++ (<vector>, <new>, <typeinfo>, …) and the
              C ones they wrap, in one directory
    lib\      cc1i's headers: the C standard headers (<stdio.h>, <string.h>, …)
    settings.json  the installation's settings: where include\ and lib\ are,
              and a vcvars64.bat when Visual Studio had to be named by hand
              (Tools > Header directories..., Tools > Locate vcvars64.bat...)
    examples\ worked programs and a demo project (demo.pro)
    help\     the full help pages

A project's own .pro may add `"include": [dirs]` and `"libraries": [files]`,
relative to the project (Tools > Project include paths..., Tools > Project
libraries...). The last three projects opened are at the end of the Project
menu; none is opened on its own at start.

--------------------------------------------------------------------------
## The full manual

This is the quick reference. The complete manual — the compilation model, every
language feature and non-feature, the project-file architecture, the full
command-line reference, the targets and the native tools, and troubleshooting —
is in `help\manual\` (Part I begins in `01-part1-model.md`), with the full
Shalimar language spec in `help\appendix-a-shalimar-language.md`.
