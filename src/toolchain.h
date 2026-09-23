#ifndef EDITOR_TOOLCHAIN_H
#define EDITOR_TOOLCHAIN_H

#include <string>
#include <vector>

#include "syntax.h"

namespace editor {

enum ToolchainKind {
    ToolAuto = 0,
    ToolCc1,
    ToolMsvc,
    ToolShc,
    ToolCxx,
    ToolCxx1,
    ToolCount
};

enum Configuration {
    ConfigDebug = 0,
    ConfigRelease,
    ConfigCount
};

const char* configName(Configuration config);

std::string configFlags(ToolchainKind kind, Configuration config,
                        const std::string& arch);

bool optimises(ToolchainKind kind);

bool emitsDebugInfo(ToolchainKind kind, const std::string& arch);

std::vector<std::string> debugNote(ToolchainKind kind, const std::string& arch);

const char* hostCxxName();

ToolchainKind hostCppToolchain();

struct Toolchain {
    ToolchainKind kind;
    std::string cc1;
    std::string cl;
    std::string shc;
    std::string cxx;
    std::string cxx1;

    // c90, cpp11 and shalimar since 3.5: the VM6747 line, the first two
    // carrying tms6747. The kinds keep their names, cc1, cxx1 and shc, being
    // the same compilers one target on.
    // Where the shipped headers are, from the settings: include/ is cxx1's
    // and lib/ is cc1's. Empty leaves each compiler to find its own.
    std::string include;
    std::string lib;

    // The project's own: header directories every compiler searches first,
    // absolute, in the order the project lists them, and libraries linked
    // after the objects.
    std::vector<std::string> includes;
    std::vector<std::string> libraries;

    Toolchain()
        : kind(ToolAuto), cc1("c90.exe"), cl("cl"), shc("shalimar.exe"),
          cxx(hostCxxName()), cxx1("cpp11.exe") {}
};

// The header directories a compiler is given, as its flags: the project's
// first, then the shipped ones this compiler reads - cc1 lib/, cxx1 include/.
// shc gets none, Shalimar having no include.
std::string includeFlags(const Toolchain& tool, ToolchainKind kind);
// The project's libraries, spelled for the link.
std::string libraryArguments(const Toolchain& tool);

ToolchainKind resolve(const Toolchain& tool, Language lang);

const char* toolchainName(ToolchainKind kind);
// The word a project file or settings.json spells a compiler with, and back.
ToolchainKind toolchainFrom(const std::string& word);
const char* toolchainWord(ToolchainKind kind);
const char* programOf(const Toolchain& tool, ToolchainKind kind);

// **The fourth target runs on an emulator.** tms6747 is the TI TMS320C6747;
// the compilers docked with this editor since 3.5 are c90 and cpp11 - the
// VM6747 line, which know it along with the three host targets - and vm6747,
// the VM6747 emulator, runs what they emit. There is nothing to assemble or
// link: the program is the .s file, or a directory of them for a project.
bool isEmulated(const std::string& arch);
std::string emulatorProgram();
// The C6000 assembler beside the editor (ASM6x's asm6x.exe, built with it),
// or empty when it is not there; $ASM6X names one elsewhere.
std::string c6xAssembler();
// The command that runs a built program: the program itself, or the emulator
// with it.
std::string launchCommand(const std::string& program, bool shalimar = false,
                          const std::vector<std::string>& args = std::vector<std::string>());
// The directory of runtime assembly a Shalimar program needs on the emulator.
std::string shalimarRuntimeDir();
// Where a project's program goes for the emulated target: <program>.vm, a
// directory of assembly, the .exe a Windows program name carries dropped.
std::string emulatedProgram(const std::string& program);

std::string toolchainShown(const Toolchain& tool, ToolchainKind kind);

bool usesArch(ToolchainKind kind);

bool canCompile(ToolchainKind kind, Language lang);
std::string refusal(ToolchainKind kind, Language lang);

const char* hostArch();
// One of the three machines' own targets, as against the emulated C6000.
bool isHostArch(const std::string& arch);

bool runsHere(ToolchainKind kind, const std::string& arch);

std::string whyNotRun(ToolchainKind kind, const std::string& arch);

struct Recipe {
    std::string command;
    std::string assemblyPath;
    std::vector<std::string> leftovers;
};

Recipe assemblyRecipe(const Toolchain& tool, ToolchainKind kind,
                      const std::string& source, Language lang,
                      const std::string& arch, Configuration config);

std::string shownCommand(const Toolchain& tool, ToolchainKind kind,
                         const std::string& source, Language lang,
                         const std::string& arch, Configuration config);

Recipe programRecipe(const Toolchain& tool, ToolchainKind kind,
                     const std::string& source, Language lang,
                     const std::string& arch, Configuration config);

std::string shownProgramCommand(const Toolchain& tool, ToolchainKind kind,
                                const std::string& source, Language lang,
                                const std::string& arch, Configuration config);

// " -masm=masm" for cpp11 on x86_64-windows when settings name an assembler.
std::string assemblerFlag(ToolchainKind kind, const std::string& arch);
Recipe targetRecipe(const Toolchain& tool, ToolchainKind kind,
                    const std::vector<std::string>& sources, Language lang,
                    const std::string& arch, Configuration config,
                    const std::string& program);

Recipe objectRecipe(const Toolchain& tool, ToolchainKind kind,
                    const std::vector<std::string>& sources, Language lang,
                    const std::string& arch, Configuration config,
                    const std::string& objectDir, std::vector<std::string>& objects);

Recipe linkRecipe(const Toolchain& tool, const std::vector<std::string>& objects,
                  bool withCpp, const std::string& arch, Configuration config,
                  const std::string& program);

std::string linkerName(bool withCpp);

bool prepareFor(ToolchainKind kind);
// Whether the vendor's tools for a target are on this machine, found the way
// a build finds them - never by PATH: Visual Studio through vswhere or the
// "vcvars" setting, whose vcvars64.bat is imported into this process; TI's
// lnk6x by full path under the "ti" directory. The question of going native
// is only put when the answer here is yes.
bool nativeToolsAvailable(const std::string& arch);

}

#endif
