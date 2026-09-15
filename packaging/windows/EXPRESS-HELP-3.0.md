# RStudio 3.0 — Express Help

Three languages, three targets, one editor. This is the quick reference; the
full pages are under `help\`.

--------------------------------------------------------------------------
## 1. Languages, versions and scope

| Language | Files        | Compiler | Standard / version | Debug info      |
|----------|--------------|----------|--------------------|-----------------|
| C        | `.c` `.h`    | `cc1`    | ISO C 90           | DWARF (2 of 3)  |
| C++      | `.cpp` `.hpp`| `cxx1`   | ISO C++ 11         | DWARF (2 of 3)  |
| Shalimar | `.shl`       | `shc`    | Shalimar 1.2       | none, by design |

The suffix picks the language; the **Language** menu overrides it for a file
whose name says otherwise.

**The three targets** (Target menu, or Ctrl-T to cycle):

| Target            | Runs here?                       |
|-------------------|----------------------------------|
| `x86_64-windows`  | yes, on this machine (host)      |
| `x86_64-linux`    | assembly only (`-S`) on Windows  |
| `arm64-darwin`    | assembly only (`-S`) on Windows  |

C goes to `cc1`, C++ to `cxx1`, Shalimar to `shc`; the host compiler (`cl`) is
reachable per group.

--------------------------------------------------------------------------
## 2. Making and updating a project

All on the **Project** menu:

- **New…** — pick a folder and a name; writes `<name>.pro` and opens it.
- **New File** (Ctrl-N) — names a new file, creates it, adds it to the project.
- **Add File** — adds the file in front of you to a group you name.
- **Remove File** — takes the open file out of the project (leaves it on disk).
- **Save** — writes the `.pro` back. (New/Add/Remove save it for you.)

Every add and remove is reflected immediately in the `.pro` file.

--------------------------------------------------------------------------
## 3. The project file (`.pro`) architecture

A `.pro` is one JSON object:

    {
      "name": "demo",
      "toolchain": "auto",           // auto | cc1 | cxx1 | shc | msvc
      "arch": "x86_64-windows",      // one of the three targets
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
  split — the C to `cc1`, the C++ to `cxx1` — and the objects are linked.
- Say no `build` and nothing is built (Ctrl-B still compiles the open file).

--------------------------------------------------------------------------
## 4. Compiling, and building/running a project

- **Ctrl-B** — compile the file in front of you.
- **F5** — compile and run that file.
- **F4** — build the project's program (from `build`).
- **Run project** (Build menu) — build it and run it.
- **Ctrl-T** / **Target** — choose the target; **Ctrl-D** — debug/release.
- The bottom panel has **Console**, **Debug** and **Assembly** (Ctrl-1/2/3).
  The Assembly tab shows the compiler's output for any of the three targets;
  the two non-host targets reach assembly only, and the panel says so.

--------------------------------------------------------------------------
## Where things are

    bin\      the editor (RStudio.exe), the console editor, and the compilers
              cc1 cxx1 shc, and the c2s converter
    bin\lib\  the Shalimar runtime (shmrt-x86_64-windows[-debug].lib)
    include\  the C++ standard headers (<vector>, <new>, <typeinfo>, …)
    lib\      the C standard headers (<stdio.h>, <string.h>, …)
    examples\ worked programs and a demo project (demo.pro)
    help\     the full help pages

--------------------------------------------------------------------------
Note: RStudio 3.5 adds a fourth target (TMS320C6747 / tms6747) with an emulator
and a real-silicon TI build path, and per-file language guards. This is 3.0.
