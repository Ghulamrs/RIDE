# Part V — The four targets, their native tools, and why ml64 works

A *target* is the machine the code is generated for. It is a separate thing from
the *host* — the machine you are compiling on. The compiler can generate any
target's assembly on any host; whether that assembly can be turned into a program
depends on whether the host has the assembler and linker for it. This part is the
four targets, what each needs, and the Windows tool story in detail.

--------------------------------------------------------------------------------
## 19. Host versus target, and "runs here"

The compiler's own output is assembly (Part I). Turning assembly into a program
needs a native assembler and linker *for that target*, and those live on the
host. So:

- Generating a target's assembly (`-S`) works on **any** host.
- Building and running a target's program works only where the host has that
  target's tools.

On each host, one target is native and the others reach `-S` and stop:

| Host             | Native target     | The others reach       |
|------------------|-------------------|------------------------|
| Windows          | `x86_64-windows`  | `-S` only              |
| Linux            | `x86_64-linux`    | `-S` only              |
| macOS (arm64)    | `arm64-darwin`    | `-S` only              |
| any (+ emulator) | `tms6747`         | runs on `vm6747`       |

`tms6747` is the exception: it runs on the `vm6747` emulator, which is
host-independent, so the fourth target runs on all three hosts. When the editor
says a target "only reaches -S here — switch to <host> to run it", that is this
fact, not a failure.

--------------------------------------------------------------------------------
## 20. The three host targets

**`x86_64-linux`** — 64-bit x86, System V ABI, GNU assembler and linker. Debug
info is DWARF. This and `x86_64-windows` share **one instruction stream**: the
same code generator produces both, and the only difference is how the
instructions are spelled — GNU syntax here, MASM there. That is why `-masm=gnu`
on `x86_64-windows` produces the same program: it is the same instructions in the
other dialect.

**`arm64-darwin`** — 64-bit ARM, the Apple ABI, the assembler and linker `clang`
drives, DWARF (gathered into a `.dSYM` on a real link). This is the native target
on an Apple-silicon Mac.

**`x86_64-windows`** — 64-bit x86, the Microsoft ABI, **MASM** assembly for
`ml64.exe`, linked with `link.exe`. Debug info: **none in the MASM spelling** —
see the next chapter. This is the native target on the Windows box, and the one
the installers target.

The type model is shared where the standard allows and differs where the ABI
demands: `long` is 64-bit on the Unix targets and 32-bit on Windows (LLP64),
which the code generator knows per target.

--------------------------------------------------------------------------------
## 21. `tms6747` — the fourth target

The TI **TMS320C6747** (a C674x, C6000-family VLIW DSP). The compiler generates
C6000 assembly; there is no C6000 assembler in our toolchain and none on the
Mac/Linux hosts, so the **`vm6747` emulator** takes the assembly and runs it. A
real chip binary is available through TI's own tools (Part VI). The C6000 has its
own ABI (arguments and small-struct returns in A/B register pairs, DP-relative
near data, TI's exception-table format), which the backend implements to match
TI's `cl6x` — verified word-for-word. wchar_t is 16-bit and unsigned on the
C6000; `long` is 32-bit; `long double` is 8 bytes. Part VI is this target in
full.

--------------------------------------------------------------------------------
## 22. Why `ml64.exe` works, and why CodeView does not

This is the most-asked Windows question, so here it is in full.

**What `ml64.exe` is.** Microsoft's 64-bit macro assembler (MASM). On
`x86_64-windows` the compiler emits MASM-syntax assembly and `ml64` turns it into
a COFF object; `link.exe` links it. `ml64` works because the compiler writes
exactly the dialect it expects — the same instruction stream as the Linux target,
spelled Microsoft's way (`MasmSpelling` versus `GnuSpelling`; the two are the
whole of the difference). Where a construct has no MASM room — for instance an
identifier that MASM treats as a reserved word — the backend spells around it
(e.g. `OPTION NOKEYWORD` for a name like `fabs`, `OPTION PROC:PRIVATE` so a
`static` function is not exported as an external symbol). These are decisions
recorded at the emission site, not accidents.

**Why it must be *found* first.** `ml64` and `link` are on `PATH` only inside a
Visual Studio Developer Command Prompt. An editor started from Explorer is not
one. So the compiler locates Visual Studio itself (`vswhere`, pinned to the
2022 toolset) and runs the assemble and link steps inside a shell that has
sourced `vcvars64.bat`. That sets both `PATH` (for `ml64`/`link`) and `LIB` (so
the linker finds `libcmt.lib`). Before this, a build started from a
double-clicked editor died with `'ml64.exe' is not recognized`, which read like a
broken compiler and was a missing environment. The lesson: on Windows, "the
compiler works from a developer prompt but not from the editor" is always an
environment question, never a code-generation one.

**Why CodeView (native Windows debug info) does *not* work here.** A native
Windows debugger wants **CodeView**, not DWARF. Two things stop the MASM path
from carrying it:

1. **MASM carries no line table of its own**, and `ml64` builds none from the
   assembly. So `-g` for `x86_64-windows` in the MASM spelling has nowhere to put
   a line table — the compiler refuses `-g` there and says why, rather than
   emit debug info a debugger cannot use.
2. **`ml64` cannot relocate CodeView.** Emitting CodeView records into the
   assembly and having `ml64` fix them up was tried and does not work — `ml64`
   will not relocate those sections. So CodeView was decided against for good on
   this path.

The way to get a line table for x86-64 *is* available: **`-masm=gnu`** switches
to the GNU spelling, which carries DWARF, and that path can be stepped. So on
`x86_64-windows` you choose: MASM (native `ml64`/`link`, no line table, cannot
step C) or GNU (DWARF, steppable) — and the editor's Debug configuration knows
which targets can be stepped and says so for the ones that cannot.

**C++ on Windows *can* be debugged — through `cl`.** When a group uses the host
compiler (`cl`), `cl` writes CodeView into a `.pdb` and `cdb` reads it, so C++
built with `cl` steps on the box (cdb is installed there under the Windows Kits,
found by the debugger even though it is not on `PATH`). What cannot be stepped is
**C built by `cc1i` for `x86_64-windows`**, because that is the MASM-no-line-table
path above. The editor's status and Debug menu say which case you are in rather
than failing quietly.

**Summary of the Windows debug matrix:**

| Built by | Target          | Debug info | Steppable on the box |
|----------|-----------------|------------|----------------------|
| `cc1i`   | x86_64-windows (MASM) | none  | no (MASM, no line table) |
| `cc1i`   | x86_64-windows (`-masm=gnu`) | DWARF | with a DWARF debugger |
| `cxx1i`  | x86_64-windows (MASM) | none  | no                   |
| `cl`     | x86_64-windows  | CodeView   | yes (cdb)            |
| `cc1i`/`cxx1i` | x86_64-linux / arm64-darwin | DWARF | yes (gdb/lldb) |
| any      | tms6747         | none       | no (runs on the emulator; not a debugger) |
