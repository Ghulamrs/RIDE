# RIDE 4.0 — The Complete Manual

**Part I — What the compilers are, what they eat, and what they make**

--------------------------------------------------------------------------------
## Table of the whole manual

- Part I — The compilation model (this file): input, output, the pipeline, the
  native tools, and where every artefact lands.
- Part II — The three languages: C (cc1i), C++ (cxx1i) and Shalimar (shci) —
  what each supports and, just as carefully, what each does not.
- Part III — Projects: the `.pro` file, what it provides and what it lacks,
  making and updating a project, groups, the build target, mixed C/C++.
- Part IV — Choosing and driving a compiler: `auto` vs the Language and Tools
  menus, the full command-line flag reference, compiling from a shell.
- Part V — The four targets: the three host targets and tms6747, the assembler
  and linker each uses, why `ml64.exe` works and CodeView does not.
- Part VI — The fourth target in depth: the C6000, the `vm6747` emulator, and
  the license-safe TI build path to real `.out`/`.hex`.
- Part VII — Reference: keys, diagnostics, troubleshooting, the glossary.

--------------------------------------------------------------------------------
## 1. What RIDE is

RIDE is one editor over three separate command-line compilers and a small
set of tools. Nothing about the editor is a compiler; nothing about a compiler
is the editor. The editor's whole job is to find the right compiler for a file,
run it with the right arguments, show you what it said, and — when the target
can run here — run the program and show you that too.

The pieces that ship, and what each one is:

| Program            | What it is                                             |
|--------------------|--------------------------------------------------------|
| `RStudio.exe`      | the windowed editor (on Windows). On Unix the same name is the console editor. |
| `RStudioConsole.exe` | the console (terminal) editor on Windows.            |
| `cc1i.exe`         | the C compiler. ISO C 90. Four targets.                |
| `cxx1i.exe`        | the C++ compiler. ISO C++ 11. Four targets.            |
| `shci.exe`         | the Shalimar compiler. Three host targets + tms6747.   |
| `vm6747.exe`       | the TMS320C6747 (C6000) instruction-set emulator.      |
| `c2s.exe`          | the C89 ↔ Shalimar converter.                          |

The three compilers are the point. The editor is a convenience around them, and
everything the editor does you can do yourself from a shell — Part IV shows how.
That separation is deliberate: a compiler you can only run through an editor is
a compiler you cannot script, cannot put in a build, and cannot test in
isolation. Each of these runs on its own, reads files, writes files, and says
what it did on its standard streams.

The "i" in `cc1i`/`cxx1i`/`shci` is the *i-line* — the same three compilers, one
target on (the fourth, tms6747), built beside the originals under distinct
names. RIDE drives the i-line since 3.5; RIDE 3.0 drove the originals
(`cc1`/`cxx1`/`shc`), which are frozen and three-target. Everything in this
manual is about 4.0 and the i-line unless it says otherwise. What 4.0 adds
over 3.5 is the project's own assembler for x86-64, `masm`, docked beside the
editor and used in place of Microsoft's `ml64` (section 5); the C6000 one,
`asm6x`, arrived in 3.5 (Part VI).

--------------------------------------------------------------------------------
## 2. What a compiler takes as input

A compiler's input is three things: **source text**, **options**, and the
**headers** its includes pull in. Nothing else. It does not read a project file,
an environment of settings, or a registry; the editor turns your project into a
command line, and the command line is the whole of what the compiler sees.

**Source text.** One or more source files named on the command line. The suffix
declares the language, and each compiler refuses a language that is not its own:

- `cc1i` takes `.c` (and `.h` if you hand it one). A `.cpp`, `.cc`, `.cxx`,
  `.hpp`, `.hh` or `.hxx` is turned away by name, with a message that points you
  at `cxx1i`, rather than being read as C and failing on the first `class` or
  `::` with a confusing "expected a type".
- `cxx1i` takes `.cpp` and its family. A `.c` is turned away by name, because a
  C source read as C++ can be quietly miscompiled where the two languages
  disagree; the message points at `cc1i`.
- `shci` takes `.shl` (and `.shm`, the suffix the phone app writes). It reads
  nothing else.

A compiler can also take an object or a library as an input — but that is an
input to the *link* step only, not something to compile. `cxx1i` sorts `.o`,
`.obj`, `.a` and `.lib` out of its inputs and hands them straight to the linker.

**Options.** Flags that say what to produce (`-S`, `-c`, plain), where to put it
(`-o`), what target to generate for (`-arch`, or `--target` for shci), what to
define (`-D`, `-U`), where to look for headers (`-I`), and how much to say
(`-nologo`, `-time`). Part IV is the complete list.

**Headers.** What `#include` reaches. The system headers are found beside the
compiler (see chapter 4 of Part I and the header-routing chapter of Part II);
your own headers are found by `-I` directories and by the directory of the file
that included them. The preprocessor resolves every include before the parser
sees a token — the compiler proper never opens a header itself; it sees one flat
stream of text with `#line` records marking where each piece came from.

What a compiler does **not** take: it does not take a directory and "figure out"
what to build (that is the editor's or your build's job); it does not take a
list of libraries to search by convention (you name them); it does not read
`CFLAGS` or any ambient variable except the few documented include-path
overrides. If it is not on the command line or in an included header, the
compiler does not know about it.

--------------------------------------------------------------------------------
## 3. What a compiler produces

A compiler produces exactly one of three things, chosen by a flag:

1. **Assembly** (`-S`). It stops after code generation and writes assembly text
   — one `.s` per input (`.asm` for the MASM spelling on `x86_64-windows`). This
   is the compiler's real output; everything past it is the native assembler's
   and linker's work. The Assembly tab in the editor shows exactly this.

2. **An object** (`-c`). It goes one step further: it hands the assembly to the
   native assembler and writes one object file per input (`.o` on Unix, `.obj`
   on Windows). No linking. This is what a project build makes for each source
   before the link step gathers them.

3. **A program** (neither flag). It compiles, assembles and links every input
   into one runnable program, named by `-o` (or `a.out`, `a.exe` on Windows).
   Several inputs link together into one program.

The thing to hold onto: the compiler's *own* product is the assembly. The object
and the program are the assembly plus the native tools. That is why a target you
cannot assemble here still reaches `-S` — the compiler has done all of its own
work; only the host's assembler is missing. And it is why the Assembly tab works
for every target while "run" works only for the ones this machine can build.

Debug information, when asked for (`-g`, or the editor's Debug configuration),
is written *into* the assembly and objects as DWARF, for the targets that can
carry it. See Part V for which targets, and why one cannot.

--------------------------------------------------------------------------------
## 4. The pipeline inside a compiler

All three compilers are one pass per stage, with no separate intermediate
representation:

    Driver → Preprocessor → Lexer → Parser (+ type work) → Backend

- **Driver** reads the command line, decides the language, target, and output
  kind, and orchestrates the stages and the native tools.
- **Preprocessor** turns text into text: it expands macros, obeys `#include`,
  `#if`, `#define` and the rest, and leaves `#line` records so later stages know
  the original file, line and column. (Shalimar has no preprocessor; see Part
  II.)
- **Lexer** turns the preprocessed text into tokens.
- **Parser** builds the syntax tree and does all the type work in one place, so
  a diagnostic and the code that is generated can never disagree about what a
  construct meant.
- **Backend** walks the tree and writes assembly for the chosen target.

One source object owns the preprocessed text and turns any byte offset back into
a file/line/column, so diagnostics and the line table are always consistent —
they go through the same map.

The backends share exactly one thing: a single statement-walk (the "Walker")
holding the control-flow visitors, the jump stack and the label discipline in
one place. Each target supplies a handful of one-line primitives
(compare-and-branch, jump, label, case-compare). Everything else — expressions,
and calls most of all, because that is where ABIs genuinely differ — is
per-target. A difference between two targets is therefore a visible decision in
one small place, not a smear across the code generator. This is the mechanism
that lets `x86_64-linux` and `x86_64-windows` share one instruction stream and
differ only in how it is spelled (GNU vs MASM).

--------------------------------------------------------------------------------
## 5. What help the compiler gets from native tools

The compiler generates assembly. It does **not** assemble or link — it calls the
host's tools for that. Three jobs go to native tools:

1. **Assembling** the `.s`/`.asm` into an object. On `x86_64-windows` that is
   the MASM dialect: since 4.0 the project's own `masm.exe` beside the editor
   assembles it (the installation's `settings.json` names it, `"assembler":
   "bin/masm.exe"`; Tools > Assembler for x86_64-windows... changes or clears
   it), and without one it is Microsoft's `ml64.exe`. `masm` takes ml64's own
   command line and is held to ml64 byte for byte on the compilers' whole
   corpus. With `-masm=gnu` it is the GNU assembler spelling. On
   `x86_64-linux` it is the GNU assembler; on `arm64-darwin` it is the
   assembler `clang` drives.

2. **Linking** objects into a program. On Windows that is Microsoft's `link.exe`.
   The project's own linker, `link.exe` beside the editor (LINK, held to
   Microsoft's byte for byte on its probe bed), is built and shipped with 4.0
   and takes its place the way `masm` took ml64's once Tools > Linker for
   x86_64-windows... names it; it is not named by default, because a program
   the compilers write links against Microsoft's C runtime, and that linker
   does not yet search `LIB`, fold the runtime's COMDAT sections or supply the
   default entry point;
   on Linux the host driver (`cc`/`gcc`) or, for C++, `clang++`/`g++`; on macOS
   `clang++`. A link of C++ objects goes through the C++ driver so the C++
   runtime is pulled in.

3. **Finding those tools.** On Windows `ml64` and `link` are on `PATH` only
   inside a Developer Command Prompt. The compiler finds Visual Studio itself
   (via `vswhere`, pinned to VS 2022) and runs the assemble and link steps
   inside a shell that has sourced `vcvars64.bat`, so a build started from an
   editor opened off the Desktop works the same as one started from a developer
   prompt. This is why a bare build once failed with `'ml64.exe' is not
   recognized` — the environment, not the compiler, was missing.

For the fourth target, tms6747, there is a different answer entirely: the
`vm6747` emulator assembles and runs the `.s` itself, so no native assembler is
involved at all (Part VI). And, optionally, TI's own tools can assemble and link
our `.s` into a real chip binary (Part VI, the TI build path).

The consequence worth internalising: **a target's "runs here" is a fact about
this machine's tools, not about the compiler.** The compiler can generate
`arm64-darwin` on Windows all day; it just cannot assemble it there, so that
target reaches `-S` and stops.

--------------------------------------------------------------------------------
## 6. Which output goes where

There are four kinds of place output lands, and knowing them removes most "where
did my file go" confusion:

**Beside the source.** `-S` and `-c` without `-o` write beside each input: a
`.s`/`.asm` or `.o`/`.obj` next to the `.c`/`.cpp`. With `-o` you name the single
output of a single input. A project's program is written beside its `.pro` file,
so it is there when the editor is not.

**The install's `bin\`.** The editor and every compiler live in `bin\`. The
editor finds the compilers it drives *beside itself* (`path::besideProgram`)
before it looks at `PATH`, which is why they must sit together — see Part VII's
layout. A build you started from the installed editor runs the installed
compilers, not whatever else is on the machine.

**`bin\lib\`.** The Shalimar runtime archives live here, beside `shci` —
`shmrt-x86_64-windows.lib` (release) and `-debug.lib` (debug) — because `shci`
looks for `lib\` next to its own binary when it links. The C6000 Shalimar
runtime is here too, as `shmrt-tms6747\*.s` (a directory of assembly, not an
archive; Part VI says why).

**`include\` and `lib\`, one level up from `bin\`.** `cxx1i`'s C++ headers and C
headers. `cxx1i` looks for them beside its binary and then one directory up, so
from `bin\cxx1i.exe` it finds `..\include` and `..\lib`. This is the reason the
Shalimar `bin\lib\` (runtime) and the top-level `lib\` (C headers) never
collide: they are at different levels, and `cxx1i` accepts a `lib\` as its
header directory only if it actually contains `stddef.h`.

**Temporary directories.** When the editor builds, intermediate `.s` and objects
go to a temporary directory it makes and removes; only the final program is
kept. When you build by hand, they go where your `-o`/defaults say.

The single rule behind all of this: **the editor finds what it drives beside
itself, and each compiler finds its own support (headers, runtime) beside or
just above itself.** Put the pieces where the installer puts them and nothing is
undiscoverable; scatter them and you will be naming paths by hand.
