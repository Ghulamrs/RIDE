# Part III — Projects: the `.pro` file, and making and updating one

A compiler takes a command line, not a project. A *project* is the editor's way
of remembering what a program is made of and turning that into command lines.
This part covers the project file, what it can and cannot express, and the
operations that create and change it.

--------------------------------------------------------------------------------
## 11. The project file (`.pro`)

Beside a project's file there is **the installation's `settings.json`**, one
directory above `bin\`, beside `include\` and `lib\`:

    {
      "include": "include",
      "lib": "lib",
      "vcvars": ""
    }

It is read for every compile, project or none. `include` is where `cxx1i`'s
headers are (its C++ headers and the C ones they wrap, in one directory) and
`lib` where `cc1i`'s are, each relative to the file unless absolute; the
editor passes each compiler its own as `-I`, and the compilers also look
there themselves, so the command line works without the editor. `vcvars` is
empty until Visual Studio's tools could not be found by looking, and then
names the `vcvars64.bat` to use. *Tools ▸ Header directories...* and *Tools ▸
Locate vcvars64.bat...* write this file; the editor writes it with the two
directories on its first run where it finds none.

A project is one JSON object in a file named `<name>.pro`, living in the
project's directory. Here is a complete one:

    {
      "name": "demo",
      "toolchain": "auto",
      "arch": "x86_64-windows",
      "indent": 4,
      "tabs": false,
      "groups": {
        "Sources": ["greet.c", "main.c"],
        "Headers": ["greet.h"]
      },
      "include": ["include", "../common"],
      "libraries": ["lib/mathlib.lib"],
      "build": { "target": "demo", "groups": ["Sources"] }
    }

Field by field:

- **`name`** — the project's name. Shown in the title bar (3.5: `RIDE 3.5 -
  <name> - <file>`).
- **`toolchain`** — the project-wide compiler choice: `auto` (the file's suffix
  decides), or a named one: `cc1`, `cxx1`, `shc`, or `msvc` (the host's `cl`).
  `auto` is almost always right; a named one forces every auto group to that
  compiler.
- **`arch`** — the target the project builds for: one of `x86_64-windows`,
  `x86_64-linux`, `arm64-darwin`, `tms6747`. The editor's Target menu changes
  it; a project that names none gets this machine's host target.
- **`indent`** / **`tabs`** — how the editor lays this project's files out.
- **`groups`** — a set of named groups, each a list of files. The names are the
  headings in the left pane. A file's group is a display and organisation
  convenience *and* the unit the build selects. A group may carry its own
  `"toolchain"` to override the project's for that group only (an object form:
  `"Legacy": { "files": [...], "toolchain": "msvc" }`).
- **`include`** — header directories of the project's own, relative to the
  project's directory (an absolute path stays as written). Every compile of
  the project's sources searches them, in this order, before the shipped
  headers: `-I` to `cc1i`, `cxx1i` and the host's C++, `/I` to `cl`. Shalimar
  has no include and `shci` is given none. Edited with *Project ▸ Include
  paths...*, one line with `;` between the entries.
- **`libraries`** — libraries linked into the program after its objects, each
  relative to the project's directory: `.lib` files under Windows, `.a` under
  Unix. A project that names any is built as objects and linked by the host's
  linker, whatever its compilers (`cc1i` and `cxx1i` take sources only). They
  do not apply to `tms6747`, which is not linked. Edited with *Project ▸
  Libraries...*.
- **`open`** — the file that opens with the project, relative to its
  directory. Written by the editor from the file in front when the project is
  closed or the editor left. A project without it opens the file that defines
  `main` (`int main(` in C and C++, `fun <> = main()` in Shalimar), and
  failing that its first file.
- **`build`** — what the project builds: `target` names the program, and
  `groups` lists which groups' sources compile into it. Files in un-named groups,
  and headers, are passed over. A project made in the editor gets one that
  builds its `Sources` group into a program of the project's name.

**What the `.pro` provides:**

- A named program made from a chosen set of sources, so `Build project` (F4) and
  `Run project` know what to make without you naming files each time.
- Per-group compiler choice, so a C-and-C++ program is expressible: a group of
  both languages is split, the C to `cc1i` and the C++ to `cxx1i`, and the
  objects are linked (Part IV chapter 15).
- A remembered target and layout, so the project opens the same way wherever it
  is opened.
- A stable, human-readable, diffable file you can keep in version control — it
  is plain JSON, and every add/remove writes it back immediately.

**What the `.pro` lacks — deliberately:**

- **No compiler flags per file or per project** beyond the target, the
  compiler choice, the header directories and the libraries. There is no
  `cflags`/`cxxflags`/`defines` field. The editor builds each source with a
  fixed, correct recipe for its language and target; if you need custom flags,
  compile from the command line (Part IV).
- **No custom build steps, no rules, no dependencies you write.** It is not
  `make`. The build is: compile the named groups' sources, link them, done.
- **No multiple programs from one `.pro`.** One `build` entry, one `target`. Two
  programs is two projects. (This is why Shalimar-beside-C is refused — see
  below.)
- **No library targets, no install rules, no configurations beyond
  debug/release** (which is an editor toggle, not a `.pro` field). A project
  *uses* libraries; it does not make one.
- **No conditional inclusion** (files in / out by platform or setting). A file
  is in a group or it is not.

The philosophy: the `.pro` says *what the program is made of*, and the editor
knows *how* to build each language for each target. It keeps the file small and
correct rather than a general-purpose build system. When you outgrow it, the
command line (Part IV) is right there and takes over seamlessly.

--------------------------------------------------------------------------------
## 12. Making and updating a project

Everything is on the **Project** menu, and every operation that changes the file
set writes the `.pro` back immediately (you do not have to Save after each):

- **New…** — opens a folder picker and asks a name, then writes `<name>.pro`
  into the folder and opens it. An empty project has an empty `Sources` group.
- **New File** (Ctrl-N) — asks a name (optionally `subdir/name`), creates the
  file on disk (empty), adds it to the right group by suffix, and opens it.
- **Add File** — adds the file you are looking at to a group you name (defaults
  to the group its suffix suggests). The file must already be saved so it has a
  name.
- **Remove File** — takes the open file out of the project. It leaves the file on
  disk — "remove from project" is not "delete from disk".
- **Save** / **Save as…** — write the `.pro` (Save as… under a new name).
- **Open…** / **Close** — open an existing `.pro`, or close the current project.

Each of these is present and identical in both RIDE 3.0 and 3.5. A worked,
step-by-step lifecycle — new project, add an empty file, give it content, add a
second file, build, add an existing file, build and run, remove the file, change
the call, run again — is the fastest way to learn them; the `demo` project in
`examples/` is the end state of exactly that sequence.

**What you see as you work.** The left pane shows the groups and their files. The
title bar shows the project and file. The bottom-right status shows the language,
configuration, compiler (with a `*` when the file chose it) and target; in 3.5
the top-right of the menu bar shows the compiler in use as plain text, changing
as the file, the Language menu or the Tools menu change it.

--------------------------------------------------------------------------------
## 13. Groups, the build target, and mixed C/C++

**Groups** are both organisation and the build's unit of selection. A group can
hold one language or several. What happens at build time depends on the group's
`toolchain`:

- A group left at `auto` holding **one** language compiles with that language's
  compiler.
- A group left at `auto` holding **C and C++ together** is **split**: the C files
  become one part compiled by `cc1i`, the C++ files another part compiled by
  `cxx1i`, and the editor links the two sets of objects — because no compiler
  here takes an object as a compile input, and "a C and C++ project together" is
  the point. You see two lines in the console: `Sources (cc1)` and
  `Sources (cxx1)`.
- A group that **names** a `toolchain` (say `msvc`) is one part and that compiler
  takes all of it — which is the only way to make `cl` compile C as C++ on
  purpose.

**Shalimar in a group is the refusal that stayed.** Shalimar cannot share a
build with C or C++: in a group with C because no compiler takes both; in a group
of its own beside C because of what a Shalimar object *is* (every unit exports
the same three startup symbols, the runtime owns `main`, and there are no
cross-file declarations to check a link against). A project that wants Shalimar
beside C builds two programs.

**Debug information does not mix.** `cl` writes CodeView, `cc1i`/`cxx1i` write
DWARF on two targets and nothing on the third, and `shci` writes none anywhere.
So a debug build of a mixed-compiler target is limited by the least-capable
piece; the editor handles this rather than producing a debugger session that
half-works.

**The link driver** is chosen by whether any part is C++: a program with C++ in
it links through the C++ driver (so the C++ runtime is present); a pure-C program
links through the C driver. The editor picks this for you and prints
`linking with <driver>`.
