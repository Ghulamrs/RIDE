# 6. The project

A project is one file — **`prime.pro`**, named after the program it builds —
and there does not have to be one. Ordinary JSON inside; the suffix says what
the file is *for* rather than what it is made of, the way `.vcxproj` and
`.xcodeproj` do — and the editor colours it as JSON, keys apart from values, so
opening one tells you what it says.

**`docs/sample.pro` is the template**: every key there is, filled in, to read
and copy. It is not a project and nothing opens it — a `.pro` is only looked
for in the directory you actually open, never one below, which is what keeps a
template a template. `examples/example.pro` is the opposite: a real, minimal
one that leaves four things to their defaults.

| key | left out means |
| --- | --- |
| `name` | the directory's own name |
| `arch` | this machine |
| `toolchain` | `auto` — the language chooses: C to cc1, C++ to cxx1, Shalimar to shc |
| a group's `toolchain` | the project's, and then the language |
| `build` | no project program; Ctrl-B still builds the file in front of you |

**Debug or release is not in here.** Which of the two you are building is what
*you* are doing today, not a property of the program — and a project file
travels, so one arriving with a configuration in it would put everyone who
opened it into release. It lives in `~/.rstudio/config.json` with the rest of
what this machine had, `--config` overrides it for one run, and a `"config"`
key left in a project file is read by nothing.

**A directory may hold several.** `prime.pro` and `sums.pro` side by side is
the case the naming is for: opening the directory takes the first by name and
says so, and `Project ▸ Open` lists them to choose from. Whichever you
opened is the one reopened next time.

**`Project ▸ Save as...`** writes one out under a name of its own.
That is the only thing that converts a project — see below.

## What was here before

It was one `RStudio.json` per directory, and `ed1.json` before that. Both are
still read, and a project opened under either is **saved back to it** — nothing
is converted behind your back, and a directory never quietly ends up with two
project files. Convert one when you mean to, with Project ▸ Save as.

With no project at all, the pane on the left shows the files you have open, and
nothing at all when none are.

`Project ▸ Close` is how you get there from a project. It closes the
view and not the project: the project file is left exactly as it was, nothing is
taken out of it, and every file you have open stays open.

**It was called `ed1.json` until 2026-08-23**, when the last thing still
carrying the editor's old name was renamed with the rest. **Projects written
under the old name still open** — the editor reads either, and a project loaded
as `ed1.json` is saved back as `ed1.json`, so nothing ends up holding both.
Rename it yourself when you want the new one; nothing here will do it behind
your back.

**A file's name decides which group it is offered to.** A `.h` or `.hpp` goes
to Headers, a `.shl` to Shalimar, and the rest of what this editor compiles to
Sources - the same rule whether the project was written from a directory, the
file was added with **Add this file**, or it was made with **New file**. The
three used to agree only by accident, and a header added by hand landed among
the sources. Type a different group name over the one offered and that wins.

**There is no project file extension.** A project is a directory with an
a `.pro` in it, and that is the whole of what being one consists of — there
is nothing to look for called `.proj`. `Project ▸ Open...` lists the
directories under the one you are in and opens the one you pick; a directory
that holds a project file is opened, and one that does not is stepped into, so
you can walk down to where the project actually is.

```json
{
  "name": "mixed",
  "toolchain": "auto",
  "config": "debug",
  "arch": "arm64-darwin",
  "indent": 4,
  "tabs": false,

  "groups": {
    "Sources": ["src/main.c", "src/util.c"],
    "Legacy":  { "files": ["src/old.c"], "toolchain": "c++" },
    "Engine":  ["src/engine.cpp"]
  },

  "build": { "target": "mixed", "groups": ["Sources", "Legacy", "Engine"] }
}
```

Seven keys, flat except the groups, and every one has a default — so `{}` is a
valid project file. Comments with `//` are allowed, because a file people edit
by hand is a file people leave notes in.

## Groups

A group is the project's own arrangement and has nothing to do with
directories: moving a file between groups changes two lists and nothing on
disk. The Project menu makes files, renames them, moves them between groups and
deletes them — the last asks you to type `yes`, because it is the only thing
here that cannot be undone.

**A group is a list of files, or an object that also names a compiler.** The
plain list is not deprecated: a group with nothing to say is written back as a
list, so adding a file to a project written before any of this leaves the file
looking the way its author left it.

## `"build"` — what the project makes

```json
"build": { "target": "mixed", "groups": ["Sources", "Legacy", "Engine"] }
```

- **`target`** is the program's name, without `.exe`. It lands beside
  the project file, so it is still there when the editor is not.
- **`groups`** is which groups go into it — deliberately not all of them, so a
  project's own tests, examples, headers and notes stay out of its program.

It also **sets the order**: groups are compiled in the order this list names
them, and that is the order the objects reach the linker. For Shalimar it
additionally **picks the program**, since every `.shl` has a `main()` and
nothing inside a file can say it is the one being built.

Saying nothing is not an error. It means the project builds nothing, and
`Ctrl-B` still compiles the file in front of you.

## A compiler per group

**C and C++ have the same decision in them, and Shalimar has none.** C goes
to **cc1** and C++ to **cxx1** — the compilers this editor was written for,
and the defaults — and each can go instead to the machine's own compiler,
`cl` on Windows and `clang++` or `g++` elsewhere, when a group says so.
Shalimar goes to `shc`, the only thing that reads it.

So a group naming a compiler is a group of C or C++ saying it wants the
host's. That is why `Legacy` above is the only group with a `"toolchain"` in
it, and why a group of C++ that is happy with cxx1 needs none. (Until 3.0
C++ had no cxx1 to go to and went to the host's on its own; a project written
then still builds, with its C++ now going to cxx1 - name `"c++"` on the group
to have it go where it went.)

The words are `cc1`, `cxx1`, `cl` (or `msvc`), `shc`, `c++`, and `auto`.
`"c++"` means *this machine's* C++ compiler rather than g++ specifically —
which one that is is a fact about a machine, and a project file does not get
to have an opinion about it. For the same reason the *paths* to the compilers
are not in here either; they come from `--cc1`, `--cxx1`, `--cl`, `--cxx`,
`$CC1`, `$CXX1`, `$CXX`, or PATH.

**A group under `auto` holding two languages is split**, one part per language,
rather than refused. A group that names a compiler is one part and that
compiler takes all of it — which is the only way to make `cl` compile C as C++
on purpose.

## Two limits worth knowing

**Shalimar cannot share a target with C or C++.** In one group, because no
compiler takes both. In a group of its own beside them, because of what a
Shalimar object is — see [the Shalimar page](shalimar.md).

**One flat list cannot say "these files on Linux, those on Windows".** This
project's own sources are the example: `terminal.cpp` and `terminal_win.cpp`
are never built together. A project that differs by platform wants a group per
platform and a `build` entry naming the one you are on.

## Where a file may sit

The root, or one directory under it, and no deeper. As many directories as you
like may sit side by side — `src`, `tests`, `examples`, `docs` — but none of
them holds another. It is a rule the project keeps rather than a habit anyone
is asked to remember, because a structure nobody has to explore is one anyone
can read at a glance.
