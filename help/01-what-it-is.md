# 1. What it is

RStudio is an editor for three languages of our own: **C** through
[cc1](../../Compiler-C), **C++** through [cxx1](../../C++), and **Shalimar**
through [shc](../../Compiler-S) — with the machine's own C and C++ compiler,
`cl` on Windows and `c++` elsewhere, reachable by name for the first two. It
edits, builds, runs and debugs, and it does all four without leaving the
keyboard.

It is not a general-purpose editor that happens to know some compilers. It was
written for cc1 and grew the other two, and that shows in what it does well: a
diagnostic puts the caret on the line, a build shows you the assembly it
produced, and a breakpoint works the same whichever of the three you are in.

## Three variants, one core

| | runs on | what it is |
| --- | --- | --- |
| `RStudio.exe` | macOS, Linux | the terminal editor |
| `RStudio.exe` | Windows | the same editor, in a window |
| `RStudioConsole.exe` | Windows | and over the Windows console |

**Every rule lives in `src/` and all three call it.** Laying a line out,
colouring it, reading the project file, choosing a compiler, driving a debugger —
one implementation each. Only `editor.cpp` with `terminal*.cpp`, and
`winforms/`, are specific to a front end.

That is not tidiness for its own sake. Two editors that behave nearly the same
are worse than one editor with two windows: the "nearly" is where the bugs
live. When something is asked for in one, the answer is to lift it into the
core and rewire the other.

The names `RStudio` and `RStudioGui` belong to the binaries. **RStudio** is what the
pair is called, and it is what `Help ▸ About` prints.

> Called *CC1 Studio Workbench* until 2026-08-22, when Shalimar became the
> third language and the old name stopped describing it.

It is also not **CC1 Studio**, which is a different thing for the same
compiler: that one is an extension that teaches VS Code about cc1, and this is
an editor of our own.

## Releases

**1.0 — the editor for C and C++.** cc1 and `cl`, the three targets, the
project file and its groups, the panel with its three tabs, and real debugging:
breakpoints, stepping, variables and the call stack. Complete in itself, and
what the name *CC1 Studio Workbench* described.

**1.1 — Shalimar.** A third language. Shalimar is not C with fewer rules, so
it did not arrive as a suffix in a table: it brought its own indent dialect,
because `n : n + 1` is an assignment here and a label there; its own way of
finding the rest of a program, because it has no `include`; and its own
debugger, because a Shalimar program stops itself and there is nothing to
install.

The same release moved the compiler from being a property of the *project* to
being a property of a **group**, so a target can hold C and C++ together —
which is what made C the only language with a decision in it. It also gave C++
a compiler off Windows, which it had never had: `clang++` on a Mac and `g++` on
the Linux box, where `auto` used to route C++ to a `cl` that was not installed.

And it is why the product is called RStudio. Three languages is where *CC1
Studio Workbench* stopped being a description.

**1.2 — Shalimar borrows, and can call C.**

Shalimar stopped having a library of its own. `sin` and its nineteen
neighbours were available to every program whether it wanted them or not, and
now a file asks — `uses sin, cos` — which costs no program a name it did not
spend, and let the borrowable set grow from twenty to twenty-seven without the
runtime archive growing by a byte, because a borrowed function is a direct call
into the platform's own libm.

The same form reaches further. `uses <real> = mean(a[]: real)` declares a
function **this compiler will never see**, provided by a library the link is
given with `--with=` — so a Shalimar program can call C compiled by `cc1`, by
`cl`, or by the host's compiler. Arrays cross too, as an opaque handle, which
meant writing the array ABI down at last rather than leaving it as whatever two
functions happened to agree on.

The one thing it costs: a program that declares a foreign function has no
interpreter, because the phone app has no link step. Both readers parse the
declaration and the app refuses it by name.

[Calling C from Shalimar](mixing-c-and-shalimar.md) is the page; the worked
examples are in the two compilers' own repositories.

**3.0 — cxx1.** The release this manual is for. A fourth compiler, and the
second language of our own that has one: C++ goes to **cxx1**, the C++11
compiler that grew out of cc1, the way C goes to cc1 — by default, on every
machine alike, with the same three targets and DWARF on the same two of them.
The host's C++ compiler is not gone; it is what a group asks for by name now,
which is exactly the position the host's C compiler has always been in. So C
stopped being the only language with a decision in it, and the two decisions
are the same one.

What it changed around the edges: a project of C and C++ builds with nothing
named and neither half going to the host; the Debug tab reads cxx1's DWARF
through the same lldb or gdb it reads cc1's; `Help ▸ About` lists four
compilers, one per line, because cxx1's `--version` is two lines long; and
the workspace on each machine builds five programs rather than four.
[C++](cpp.md) is the page.

## What it will not do

Said here so that the rest of the manual does not have to keep apologising.

- **It does not have a plugin system**, a package manager, or a settings UI.
  Configuration is one JSON file per project and one per machine.
- **It does not guess.** Where two things could be meant, it asks or refuses,
  and the refusal says which file to move or which line to change.
- **It stops at the assembly for a cross target.** Building for a machine you
  are not on produces assembly and nothing else, because the assembler and
  linker it hands off to are this machine's.
- **It has no optimiser of its own.** What optimisation you get is whatever
  the compiler you chose does.
