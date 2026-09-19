#include "toolchain.h"

#include "path.h"
#include "settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#define POPEN  _popen
#define PCLOSE _pclose
const char kSep = '\\';
#else
#include <unistd.h>
#define POPEN  popen
#define PCLOSE pclose
const char kSep = '/';
#endif

namespace editor {

namespace {

std::string tempDir() {
#ifdef _WIN32
    const char* t = std::getenv("TEMP");
    if (!t) t = std::getenv("TMP");
    return t ? t : ".";
#else
    const char* t = std::getenv("TMPDIR");
    return t ? t : "/tmp";
#endif
}

std::string quote(const std::string& s) { return "\"" + s + "\""; }

std::string languageFlag(ToolchainKind kind, Language lang) {
    if (kind != ToolCxx) return std::string();
    return (lang == LangCpp) ? " -x c++" : " -x c";
}


std::string quoteDirectory(const std::string& s) {
    std::string path = s;
    if (!path.empty() && path[path.size() - 1] == kSep) path += kSep;
    return quote(path);
}

#ifdef _WIN32

// Nothing asked from here reads the editor's input, and one of these was: cmd
// running vcvars64.bat to hand back its environment inherits stdin, and read
// it to the end while the parent drained the answer. stdin is the editor's
// keystrokes, so after the first build there were none left - every key
// pressed after it was silently the last one, and the editor quit on what it
// took for end of input.
//
// The redirect is on a parenthesised block, not on the command: in a '&&'
// chain cmd binds '< NUL' to the one command it follows, so the obvious
// spelling left the 'call' still reading.
std::string forCmd(const std::string& s) { return "\"( " + s + " ) < NUL\""; }

std::string firstLineOf(const std::string& command) {
    FILE* pipe = POPEN(command.c_str(), "r");
    if (!pipe) return std::string();

    char buffer[1024];
    std::string line;
    if (std::fgets(buffer, sizeof buffer, pipe)) line = buffer;
    PCLOSE(pipe);

    while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
        line.resize(line.size() - 1);
    return line;
}

// The batch file that puts cl, ml64 and link on PATH and LIB where they can
// be found: named in the settings when a person had to, else the newest
// Visual Studio vswhere knows of - any version, any edition, Build Tools
// included - and failing vswhere, the places the installer puts them.
std::string findVcvars() {
    std::string named = settings::vcvars();
    if (!named.empty()) return named;

    const char* programFiles = std::getenv("ProgramFiles(x86)");
    if (programFiles) {
        std::string vswhere = std::string(programFiles) +
                              "\\Microsoft Visual Studio\\Installer\\vswhere.exe";
        if (path::exists(vswhere)) {
            std::string where = firstLineOf(forCmd(
                quote(vswhere) + " -latest -prerelease -products *"
                                 " -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
                                 " -property installationPath"));
            std::string bat = where + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
            if (!where.empty() && path::exists(bat)) return bat;
        }
    }

    static const char* const roots[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\18\\",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\"
    };
    static const char* const editions[] = {
        "Community", "Professional", "Enterprise", "BuildTools", "Preview"
    };
    for (size_t r = 0; r < sizeof roots / sizeof roots[0]; ++r)
        for (size_t e = 0; e < sizeof editions / sizeof editions[0]; ++e) {
            std::string bat = std::string(roots[r]) + editions[e] +
                              "\\VC\\Auxiliary\\Build\\vcvars64.bat";
            if (path::exists(bat)) return bat;
        }
    return std::string();
}

bool importMsvcEnvironment() {
    static int done = 0;
    if (done == 1) return true;
    // A failure is tried again once a person has named the batch file.
    if (done == -1 && settings::vcvars().empty()) return false;

    if (std::getenv("VSCMD_ARG_TGT_ARCH")) {
        done = 1;
        return true;
    }

    std::string bat = findVcvars();
    if (bat.empty()) {
        done = -1;
        return false;
    }

    std::string command = forCmd("call " + quote(bat) + " >nul && set");
    FILE* pipe = POPEN(command.c_str(), "r");
    if (!pipe) {
        done = -1;
        return false;
    }

    char line[4096];
    int taken = 0;
    while (std::fgets(line, sizeof line, pipe)) {
        char* eq = std::strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';

        char* value = eq + 1;
        size_t n = std::strlen(value);
        while (n > 0 && (value[n - 1] == '\n' || value[n - 1] == '\r')) value[--n] = '\0';

        if (_putenv_s(line, value) == 0) ++taken;
    }
    PCLOSE(pipe);

    done = (taken > 0) ? 1 : -1;
    return done == 1;
}
#endif

}

const char* hostCxxName() {
#if defined(_WIN32)
    return "cl";
#elif defined(__APPLE__)
    return "clang++";
#else
    return "g++";
#endif
}

ToolchainKind hostCppToolchain() {
#ifdef _WIN32
    return ToolMsvc;
#else
    return ToolCxx;
#endif
}

// C++ goes to cxx1 the way C goes to cc1: the compiler this editor is for,
// with the machine's own reachable by name. Until 3.0 C++ had no decision in
// it and went straight to the host's; now it has the same one C has.
ToolchainKind resolve(const Toolchain& tool, Language lang) {
    if (tool.kind != ToolAuto) return tool.kind;
    if (lang == LangShalimar) return ToolShc;

    return (lang == LangCpp) ? ToolCxx1 : ToolCc1;
}

ToolchainKind toolchainFrom(const std::string& word) {
    if (word == "cc1") return ToolCc1;
    if (word == "msvc" || word == "cl") return ToolMsvc;
    if (word == "shc") return ToolShc;
    if (word == "cxx1") return ToolCxx1;

    if (word == "c++" || word == "cxx" || word == "g++" || word == "clang++")
        return hostCppToolchain();
    return ToolAuto;
}

const char* toolchainWord(ToolchainKind kind) {
    if (kind == ToolCc1) return "cc1";
    if (kind == ToolMsvc) return "msvc";
    if (kind == ToolShc) return "shc";
    if (kind == ToolCxx1) return "cxx1";

    if (kind == ToolCxx) return "c++";
    return "auto";
}

const char* toolchainName(ToolchainKind kind) {
    switch (kind) {
        case ToolMsvc: return "cl";
        case ToolCc1:  return "cc1";
        case ToolShc:  return "shc";
        case ToolCxx:  return "c++";
        case ToolCxx1: return "cxx1";
        default:       return "auto";
    }
}

std::string toolchainShown(const Toolchain& tool, ToolchainKind kind) {
    if (kind != ToolCxx) return toolchainName(kind);

    return path::filename(tool.cxx);
}

const char* programOf(const Toolchain& tool, ToolchainKind kind) {
    if (kind == ToolMsvc) return tool.cl.c_str();
    if (kind == ToolShc) return tool.shc.c_str();
    if (kind == ToolCxx) return tool.cxx.c_str();
    if (kind == ToolCxx1) return tool.cxx1.c_str();
    return tool.cc1.c_str();
}

bool usesArch(ToolchainKind kind) {
    return kind == ToolCc1 || kind == ToolShc || kind == ToolCxx1;
}

std::string includeFlags(const Toolchain& tool, ToolchainKind kind) {
    if (kind == ToolShc) return std::string();
    const char* flag = (kind == ToolMsvc) ? " /I" : " -I";

    std::string flags;
    for (size_t i = 0; i < tool.includes.size(); ++i) flags += flag + quote(tool.includes[i]);
    if (kind == ToolCc1 && !tool.lib.empty()) flags += flag + quote(tool.lib);
    if (kind == ToolCxx1 && !tool.include.empty()) flags += flag + quote(tool.include);
    return flags;
}

std::string libraryArguments(const Toolchain& tool) {
    std::string named;
    for (size_t i = 0; i < tool.libraries.size(); ++i) named += " " + quote(tool.libraries[i]);
    return named;
}

// The target as each compiler spells it: `-arch x` to cc1 and cxx1, `--target=x` to shc.
std::string archFlag(ToolchainKind kind, const std::string& arch) {
    return kind == ToolShc ? " --target=" + arch : " -arch " + arch;
}

bool isEmulated(const std::string& arch) { return arch == "tms6747"; }

std::string emulatorProgram() {
    const char* fromEnv = std::getenv("VM6747");
    if (fromEnv && *fromEnv) return fromEnv;
    std::string beside = path::besideProgram("vm6747.exe");
    if (beside.empty()) beside = path::besideProgram("vm6747");
    return beside.empty() ? std::string("vm6747") : beside;
}

std::string c6xAssembler() {
    const char* fromEnv = std::getenv("ASM6X");
    if (fromEnv && *fromEnv) return fromEnv;
    return path::besideProgram("asm6x.exe");
}

std::string emulatedProgram(const std::string& program) {
    std::string name = program;
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".exe") == 0) name.resize(name.size() - 4);
    return name + ".vm";
}

// The Shalimar runtime for the emulator: a directory of the .s cxx1i wrote
// from it, beside the editor in lib/, which vm6747 assembles with the program.
std::string shalimarRuntimeDir() {
    const char* fromEnv = std::getenv("SHMRT6747");
    if (fromEnv && *fromEnv) return fromEnv;
    // A directory, which besideProgram does not answer for: it names files.
    std::string where = path::programDirectory();
    if (where.empty()) return std::string();
    std::string dir = path::join(path::join(where, "lib"), "shmrt-tms6747");
    return path::isDirectory(dir) ? dir : std::string();
}

std::string launchCommand(const std::string& program, bool shalimar) {
    std::string leaf = path::filename(program);
    bool assembly = leaf.size() > 2 && leaf.compare(leaf.size() - 2, 2, ".s") == 0;
    bool directory = leaf.size() > 3 && leaf.compare(leaf.size() - 3, 3, ".vm") == 0;
    if (assembly || directory) {
        std::string command = quote(emulatorProgram()) + " " + quote(program);
        if (shalimar) {
            std::string runtime = shalimarRuntimeDir();
            if (!runtime.empty()) command += " " + quote(runtime);
        }
        return command;
    }
    return quote(program);
}

const char* configName(Configuration config) {
    return config == ConfigRelease ? "release" : "debug";
}

bool optimises(ToolchainKind kind) { return kind == ToolMsvc || kind == ToolCxx; }

bool emitsDebugInfo(ToolchainKind kind, const std::string& arch) {

    if (kind == ToolMsvc) return true;

    if (kind == ToolCxx) return true;

    // cc1 and cxx1 alike: DWARF on the two GNU targets, MASM on the third,
    // and nothing for the C6000 - the emulator runs it, no debugger reads it.
    if (kind != ToolCc1 && kind != ToolCxx1) return false;
    return arch == "x86_64-linux" || arch == "arm64-darwin";
}

std::string configFlags(ToolchainKind kind, Configuration config,
                        const std::string& arch) {

    // No --debug on the emulator: it is not a debugger, the runtime beside
    // it is the release one, and the debug runtime's names would be undefined.
    if (kind == ToolShc)
        return config == ConfigDebug && !isEmulated(arch) ? std::string(" --debug") : std::string();

    if (kind == ToolCxx)
        return config == ConfigRelease ? " -O2 -DNDEBUG=1" : " -g -D_DEBUG=1";

    if (kind == ToolMsvc)
        return config == ConfigRelease ? " /O2 /DNDEBUG" : " /Od /Zi /D_DEBUG";

    if (config == ConfigRelease) return " -DNDEBUG=1";
    return emitsDebugInfo(kind, arch) ? " -g -D_DEBUG=1" : " -D_DEBUG=1";
}

std::vector<std::string> debugNote(ToolchainKind kind, const std::string& arch) {
    std::vector<std::string> said;
    if (kind == ToolCxx) {
        said.push_back("This is the machine's own C++ compiler, so a debug build has real");
        said.push_back("DWARF in it and lldb or gdb reads it - there was never a question");
        said.push_back("about that one. What is below is the assembly the build produced,");
        said.push_back("read back out of itself; the editor stops at -S and assembles");
        said.push_back("nothing, so nothing has been linked or run.");
    } else if (emitsDebugInfo(kind, arch)) {
        said.push_back(std::string(toolchainName(kind)) + " writes DWARF for " + arch +
                       " - line tables, types, objects and");
        said.push_back("lexical blocks - so a debugger has something to read here. This");
        said.push_back("editor is not that debugger: it builds to assembly and stops, so");
        said.push_back("nothing has been assembled, linked or run. What the build did leave");
        said.push_back("behind is the assembly, and this is what is in it.");
    } else if (kind == ToolShc) {
        said.push_back("shc writes no debug information for any target, and that is a");
        said.push_back("decision rather than a gap: a Shalimar program carries its own");
        said.push_back("position instead - shm_line before every statement, in every");
        said.push_back("build - which is what names the line of a runtime error and what");
        said.push_back("F8 stops on. So there is a debugger here and no debug format. What");
        said.push_back("it cannot do is read a variable. This is the assembly the build");
        said.push_back("produced, read back out of itself.");
    } else if ((kind == ToolCc1 || kind == ToolCxx1) && isEmulated(arch)) {
        said.push_back(std::string(toolchainName(kind)) + "i writes no debug information for " +
                       arch + ": what it emits runs on");
        said.push_back("vm6747, the VM6747 emulator, which is not a debugger - it can trace");
        said.push_back("every instruction (-t) but stops nowhere. This is what the build");
        said.push_back("produced, read back out of its own assembly.");
    } else if (kind == ToolCc1 || kind == ToolCxx1) {
        said.push_back(std::string(toolchainName(kind)) + " writes no debug information for " +
                       arch + ": it generates MASM");
        said.push_back("there, and MASM carries no line table. So there is nothing to step");
        said.push_back("through. This is what the build produced, read back out of its own");
        said.push_back("assembly.");
    } else {
        said.push_back("cl is not asked for /Zi here, so this build carries no debug");
        said.push_back("information either. This is what it produced, read back out of its");
        said.push_back("own assembly.");
    }
    return said;
}

bool canCompile(ToolchainKind kind, Language lang) {
    if (lang == LangShalimar) return kind == ToolShc;
    if (kind == ToolShc) return false;
    if (lang == LangCpp) return kind == ToolMsvc || kind == ToolCxx || kind == ToolCxx1;
    if (lang == LangC) return kind != ToolCxx1;
    return false;
}

std::string refusal(ToolchainKind kind, Language lang) {
    if (lang == LangShalimar && kind != ToolShc)
        return std::string(toolchainName(kind)) +
               " does not compile Shalimar - Ctrl-K for automatic, and it picks shc";
    if (kind == ToolShc && lang != LangShalimar)
        return std::string("shc compiles Shalimar, not ") + languageName(lang) +
               " - Ctrl-K for automatic";
    if (lang == LangCpp && kind == ToolCc1)
        return "cc1 compiles C, not C++ - Ctrl-K for automatic, and it picks cxx1";
    if (lang == LangC && kind == ToolCxx1)
        return "cxx1 compiles C++, not C - Ctrl-K for automatic, and it picks cc1";
    if (lang == LangPlain)
        return "nothing to compile: no extension names a language - .c, .cpp or .shl picks the compiler";
    if (lang != LangC && lang != LangCpp)
        return std::string("nothing to compile: this is ") + languageName(lang) +
               ", not C or C++";
    return "cannot compile this file";
}

const char* hostArch() {
#if defined(_WIN32)
    return "x86_64-windows";
#elif defined(__APPLE__)
    return "arm64-darwin";
#else
    return "x86_64-linux";
#endif
}

bool isHostArch(const std::string& arch) {
    return arch == "x86_64-linux" || arch == "arm64-darwin" || arch == "x86_64-windows";
}

bool runsHere(ToolchainKind kind, const std::string& arch) {

    if (kind == ToolMsvc || kind == ToolCxx) return true;
    // The C6000 runs anywhere the emulator is, for the three docked compilers.
    if (isEmulated(arch)) return kind == ToolCc1 || kind == ToolCxx1 || kind == ToolShc;
    return arch == hostArch();
}

std::string whyNotRun(ToolchainKind kind, const std::string& arch) {
    if (runsHere(kind, arch)) return std::string();
    if (isEmulated(arch))
        return std::string(toolchainName(kind)) + " has no " + arch + " target - it is cc1's, cxx1's and shc's";
    return arch + " only reaches -S here - switch to " + hostArch() + " to run it";
}

namespace {

std::string mine(const char* what) {
    char id[32];
#ifdef _WIN32
    std::snprintf(id, sizeof id, "%lu", static_cast<unsigned long>(GetCurrentProcessId()));
#else
    std::snprintf(id, sizeof id, "%ld", static_cast<long>(getpid()));
#endif
    return tempDir() + kSep + what + "-" + id;
}

std::string programPath() {
    std::string path = mine("rstudio-run");
#ifdef _WIN32
    path += ".exe";
#endif
    return path;
}

std::string objectFor(const std::string& dir, const std::string& source,
                      const char* suffix) {
    std::string leaf = path::filename(source);
    size_t dot = leaf.find_last_of('.');
    if (dot != std::string::npos) leaf.resize(dot);
    return dir.empty() ? leaf + suffix : path::join(dir, leaf + suffix);
}
}

Recipe targetRecipe(const Toolchain& tool, ToolchainKind kind,
                    const std::vector<std::string>& sources, Language lang,
                    const std::string& arch, Configuration config,
                    const std::string& program) {
    Recipe recipe;
    recipe.assemblyPath = program;

    std::string named;
    for (size_t i = 0; i < sources.size(); ++i) named += " " + quote(sources[i]);

    if (kind == ToolMsvc) {

        std::string objects = mine("rstudio-objs");
        path::makeDirectories(objects);
        std::string forLanguage = (lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC";
        std::string pdb = path::join(objects, "rstudio-target.pdb");

        recipe.command = quote(programOf(tool, kind)) + " /nologo /diagnostics:column" +
                         forLanguage + configFlags(kind, config, arch) + includeFlags(tool, kind) +
                         (config == ConfigDebug ? " /Fd" + quote(pdb) : std::string()) +
                         " /Fe" + quote(program) +
                         " /Fo" + quoteDirectory(objects + kSep) + named + libraryArguments(tool) +
                         (config == ConfigDebug ? " /link /DEBUG" : std::string());

        for (size_t i = 0; i < sources.size(); ++i) {
            std::string leaf = path::filename(sources[i]);
            size_t dot = leaf.find_last_of('.');
            if (dot != std::string::npos) leaf.resize(dot);
            recipe.leftovers.push_back(path::join(objects, leaf + ".obj"));
        }
        if (config == ConfigDebug) recipe.leftovers.push_back(pdb);
        recipe.leftovers.push_back(objects);
        return recipe;
    }

    if (kind == ToolShc && !isEmulated(arch)) {

        recipe.command = quote(programOf(tool, kind)) + named + " -o " + quote(program) +
                         configFlags(kind, config, arch);
        return recipe;
    }

    if (isEmulated(arch)) {
        // Nothing links: the program is a directory of one .s per source,
        // which vm6747 assembles together. Each source is its own command,
        // since -S with several inputs writes beside them.
        std::string dir = emulatedProgram(program);
        path::removeTree(dir);
        path::makeDirectories(dir);
        recipe.assemblyPath = dir;
        if (kind == ToolShc) {
            // A Shalimar program is one compilation: the file with main() and
            // the files beside it that it calls into, which shc compiles as one
            // - a library file on its own has no main() and is refused. So all
            // the sources go in one command, and one .s comes out.
            // Named after the program without the .exe Windows gives it, as
            // the .vm directory is: prog.s in prog.vm, on every host.
            std::string stem = path::filename(emulatedProgram(program));
            stem.resize(stem.size() - 3);
            recipe.command = quote(programOf(tool, kind)) + named + " -S" + archFlag(kind, arch) + " -o " +
                             quote(path::join(dir, stem + ".s")) + configFlags(kind, config, arch);
            return recipe;
        }
        for (size_t i = 0; i < sources.size(); ++i) {
            if (i > 0) recipe.command += " && ";
            recipe.command += quote(programOf(tool, kind)) + " -S" + archFlag(kind, arch) +
                              " " + quote(sources[i]) + " -o " +
                              quote(path::join(dir, objectFor(std::string(), sources[i], ".s"))) +
                              configFlags(kind, config, arch) + includeFlags(tool, kind);
        }
        return recipe;
    }

    recipe.command = quote(programOf(tool, kind)) + languageFlag(kind, lang) +
                     named + " -o " + quote(program) + configFlags(kind, config, arch) +
                     assemblerFlag(kind, arch) + includeFlags(tool, kind);
    return recipe;
}

namespace {

const char* hostDriver() {
    const char* named = std::getenv("CC1_CC");
    return (named && *named) ? named : "cc";
}

const char* hostLinker() {
    const char* named = std::getenv("CC1_LD");
    return (named && *named) ? named : "link.exe";
}

const char* hostCppDriver() {
    const char* named = std::getenv("CXX");
    return (named && *named) ? named : hostCxxName();
}

}

const char* linkerNameFor(bool windows, bool withCpp) {
    if (windows) return hostLinker();
    if (withCpp) return hostCppDriver();
    return hostDriver();
}

std::string linkerName(bool withCpp) {
#ifdef _WIN32
    return linkerNameFor(true, withCpp);
#else
    return linkerNameFor(false, withCpp);
#endif
}

Recipe objectRecipe(const Toolchain& tool, ToolchainKind kind,
                    const std::vector<std::string>& sources, Language lang,
                    const std::string& arch, Configuration config,
                    const std::string& objectDir, std::vector<std::string>& objects) {
    Recipe recipe;
    objects.clear();

    std::string named;
    for (size_t i = 0; i < sources.size(); ++i) named += " " + quote(sources[i]);

    if (kind == ToolMsvc) {

        std::string forLanguage = (lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC";
        std::string crt = (config == ConfigDebug) ? " /MTd" : " /MT";
        std::string pdb = path::join(objectDir, "rstudio-target.pdb");

        recipe.command = quote(programOf(tool, kind)) + " /nologo /diagnostics:column /c" +
                         forLanguage + crt + configFlags(kind, config, arch) + includeFlags(tool, kind) +
                         (config == ConfigDebug ? " /Fd" + quote(pdb) : std::string()) +
                         " /Fo" + quoteDirectory(objectDir + kSep) + named;

        for (size_t i = 0; i < sources.size(); ++i)
            objects.push_back(objectFor(objectDir, sources[i], ".obj"));
        recipe.leftovers = objects;
        if (config == ConfigDebug) recipe.leftovers.push_back(pdb);
        return recipe;
    }

    if (isEmulated(arch)) {
        for (size_t i = 0; i < sources.size(); ++i) {
            std::string out = objectFor(objectDir, sources[i], ".s");
            if (i > 0) recipe.command += " && ";
            recipe.command += quote(programOf(tool, kind)) + " -S" + archFlag(kind, arch) +
                              " " + quote(sources[i]) + " -o " + quote(out) +
                              configFlags(kind, config, arch) + includeFlags(tool, kind);
            objects.push_back(out);
        }
        recipe.leftovers = objects;
        return recipe;
    }

    recipe.command = "cd " + quote(objectDir) + " && " +
                     quote(programOf(tool, kind)) + " -c" +
                     languageFlag(kind, lang) + named +
                     configFlags(kind, config, arch) + assemblerFlag(kind, arch) +
                     includeFlags(tool, kind);

    for (size_t i = 0; i < sources.size(); ++i)
        objects.push_back(objectFor(objectDir, sources[i], ".o"));
    recipe.leftovers = objects;
    return recipe;
}

Recipe linkRecipe(const Toolchain& tool, const std::vector<std::string>& objects,
                  bool withCpp, const std::string& arch, Configuration config,
                  const std::string& program) {
    (void)arch;
    Recipe recipe;
    recipe.assemblyPath = program;

    std::string named;
    for (size_t i = 0; i < objects.size(); ++i) named += " " + quote(objects[i]);
    named += libraryArguments(tool);

#ifdef _WIN32
    (void)withCpp;

    const char* crt = (config == ConfigDebug)
                          ? " libcmtd.lib libucrtd.lib libvcruntimed.lib"
                          : " libcmt.lib libucrt.lib libvcruntime.lib";
    recipe.command = quote(linkerNameFor(true, withCpp)) +
                     " /nologo /subsystem:console" +
                     (config == ConfigDebug ? std::string(" /DEBUG") : std::string()) +
                     " /out:" + quote(program) + named + crt +
                     " kernel32.lib legacy_stdio_definitions.lib";
#else

    recipe.command = quote(linkerNameFor(false, withCpp)) +
                     (config == ConfigDebug ? std::string(" -g") : std::string()) +
                     named + " -o " + quote(program) + " -lm";
#endif
    return recipe;
}

Recipe programRecipe(const Toolchain& tool, ToolchainKind kind,
                     const std::string& source, Language lang,
                     const std::string& arch, Configuration config) {
    Recipe recipe;
    std::string program = programOf(tool, kind);
    recipe.assemblyPath = programPath();

    if (isEmulated(arch) && usesArch(kind)) {
        // The program to run is the assembly: vm6747 runs it as it is - a
        // Shalimar one beside the runtime, which the launch adds.
        recipe.assemblyPath = mine("rstudio-run") + ".s";
        recipe.command = quote(programOf(tool, kind)) + " -S" + archFlag(kind, arch) + " " +
                         quote(source) + " -o " + quote(recipe.assemblyPath) +
                         configFlags(kind, config, arch) + includeFlags(tool, kind);
        return recipe;
    }

    if (kind == ToolMsvc) {

        std::string obj = mine("rstudio-run") + ".obj";
        std::string forLanguage = (lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC";

        std::string pdb = mine("rstudio-run") + ".pdb";
        recipe.command = quote(program) + " /nologo /diagnostics:column" + forLanguage +
                         configFlags(kind, config, arch) + includeFlags(tool, kind) +
                         (config == ConfigDebug ? " /Fd" + quote(pdb) : std::string()) +
                         " /Fe" + quote(recipe.assemblyPath) +
                         " /Fo" + quote(obj) + " " + quote(source) + libraryArguments(tool) +
                         (config == ConfigDebug ? " /link /DEBUG" : std::string());
        recipe.leftovers.push_back(obj);
        if (config == ConfigDebug) {
            recipe.leftovers.push_back(pdb);
            recipe.leftovers.push_back(mine("rstudio-run") + ".ilk");
        }
        return recipe;
    }

    // cc1 and cxx1 take sources only, so a library rides on F4, where the
    // objects are linked by the host; the machine's own C++ takes them here.
    recipe.command = quote(program) + " " + quote(source) + " -o " +
                     quote(recipe.assemblyPath) + configFlags(kind, config, arch) +
                     includeFlags(tool, kind) +
                     (kind == ToolCxx ? libraryArguments(tool) : std::string());
    return recipe;
}

std::string shownProgramCommand(const Toolchain& tool, ToolchainKind kind,
                                const std::string& source, Language lang,
                                const std::string& arch, Configuration config) {
    std::string program = programOf(tool, kind);
    if (isEmulated(arch) && usesArch(kind))
        return program + " -S" + archFlag(kind, arch) + " " + source + " -o rstudio-run.s" +
               configFlags(kind, config, arch) + includeFlags(tool, kind) +
               " && vm6747 rstudio-run.s" +
               (kind == ToolShc ? " lib/shmrt-tms6747" : "");
    if (kind == ToolMsvc)
        return program + " /diagnostics:column" +
               ((lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC") +
               configFlags(kind, config, arch) + includeFlags(tool, kind) +
               " /Ferstudio-run " + source + libraryArguments(tool);
    return program + " " + source + " -o rstudio-run" + configFlags(kind, config, arch) +
           includeFlags(tool, kind) + (kind == ToolCxx ? libraryArguments(tool) : std::string());
}

Recipe assemblyRecipe(const Toolchain& tool, ToolchainKind kind,
                      const std::string& source, Language lang,
                      const std::string& arch, Configuration config) {
    Recipe recipe;
    std::string stem = mine("rstudio-build");
    std::string program = programOf(tool, kind);

    if (kind == ToolMsvc) {
        recipe.assemblyPath = stem + ".asm";
        std::string obj = stem + ".obj";

        std::string forLanguage = (lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC";

        recipe.command = quote(program) + " /nologo /c /diagnostics:column /FAs" +
                         forLanguage + configFlags(kind, config, arch) + includeFlags(tool, kind) +
                         " /Fa" + quote(recipe.assemblyPath) +
                         " /Fo" + quote(obj) + " " + quote(source);
        recipe.leftovers.push_back(obj);
        return recipe;
    }

    if (kind == ToolShc) {
        recipe.assemblyPath = stem + (arch == "x86_64-windows" ? ".asm" : ".s");
        recipe.command = quote(program) + " -S " + quote(source) + " -o " +
                         quote(recipe.assemblyPath) + " --target=" + arch;
        return recipe;
    }

    recipe.assemblyPath = stem + ".s";
    recipe.command = quote(programOf(tool, kind)) + " -S" + languageFlag(kind, lang) + " " +
                     quote(source) + " -o " + quote(recipe.assemblyPath) +
                     (usesArch(kind) ? " -arch " + arch : std::string()) +
                     configFlags(kind, config, arch) + includeFlags(tool, kind);
    return recipe;
}

std::string shownCommand(const Toolchain& tool, ToolchainKind kind,
                         const std::string& source, Language lang,
                         const std::string& arch, Configuration config) {
    std::string program = programOf(tool, kind);
    if (kind == ToolMsvc)
        return program + " /c /diagnostics:column /FAs" +
               ((lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC") +
               configFlags(kind, config, arch) + includeFlags(tool, kind) + " " + source;
    if (kind == ToolShc)
        return program + " -S " + source + " --target=" + arch;
    return program + " -S " + source + " -arch " + arch +
           configFlags(kind, config, arch) + includeFlags(tool, kind);
}

bool prepareFor(ToolchainKind kind) {
#ifdef _WIN32
    if (kind == ToolMsvc) return importMsvcEnvironment();

    importMsvcEnvironment();
    // The project's assembler, where one is named: all three compilers read
    // the variable, and cxx1i also needs -masm=masm - see assemblerFlag.
    std::string as = settings::assembler();
    _putenv_s("CC1_AS", as.c_str());
    _putenv_s("CXX1_AS", as.c_str());
    _putenv_s("SHC_AS", as.c_str());
    return true;
#else
    (void)kind;
    return true;
#endif
}

std::string assemblerFlag(ToolchainKind kind, const std::string& arch) {
    if (kind == ToolCxx1 && arch == "x86_64-windows" && !settings::assembler().empty())
        return " -masm=masm";
    return std::string();
}

}
