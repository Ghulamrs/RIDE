#include "toolchain.h"

#include "path.h"

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

// cxx1 says who it is on stderr before every compile, and cc1 and shc say
// nothing - so the console showed a banner above the first error for one
// language in three. Its -nologo is cl's /nologo, and is passed the same way:
// in the command that runs, and not in the one the console shows.
std::string quietFlag(ToolchainKind kind) {
    return kind == ToolCxx1 ? std::string(" -nologo") : std::string();
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

std::string findVcvars() {
    const char* programFiles = std::getenv("ProgramFiles(x86)");
    if (!programFiles) return std::string();

    std::string vswhere = std::string(programFiles) +
                          "\\Microsoft Visual Studio\\Installer\\vswhere.exe";

    std::string where = firstLineOf(forCmd(
        quote(vswhere) + " -latest -products * -version \"[17.0,18.0)\""
                         " -property installationPath"));
    if (where.empty()) return std::string();

    return where + "\\VC\\Auxiliary\\Build\\vcvars64.bat";
}

bool importMsvcEnvironment() {
    static int done = 0;
    if (done != 0) return done == 1;

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

const char* configName(Configuration config) {
    return config == ConfigRelease ? "release" : "debug";
}

bool optimises(ToolchainKind kind) { return kind == ToolMsvc || kind == ToolCxx; }

bool emitsDebugInfo(ToolchainKind kind, const std::string& arch) {

    if (kind == ToolMsvc) return true;

    if (kind == ToolCxx) return true;

    // cc1 and cxx1 alike: DWARF on the two GNU targets, MASM on the third.
    if (kind != ToolCc1 && kind != ToolCxx1) return false;
    return arch == "x86_64-linux" || arch == "arm64-darwin";
}

std::string configFlags(ToolchainKind kind, Configuration config,
                        const std::string& arch) {

    if (kind == ToolShc)
        return config == ConfigDebug ? std::string(" --debug") : std::string();

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

bool runsHere(ToolchainKind kind, const std::string& arch) {

    if (kind == ToolMsvc || kind == ToolCxx) return true;
    return arch == hostArch();
}

std::string whyNotRun(ToolchainKind kind, const std::string& arch) {
    if (runsHere(kind, arch)) return std::string();
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
                         forLanguage + configFlags(kind, config, arch) +
                         (config == ConfigDebug ? " /Fd" + quote(pdb) : std::string()) +
                         " /Fe" + quote(program) +
                         " /Fo" + quoteDirectory(objects + kSep) + named +
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

    if (kind == ToolShc) {

        recipe.command = quote(programOf(tool, kind)) + named + " -o " + quote(program) +
                         configFlags(kind, config, arch);
        return recipe;
    }

    recipe.command = quote(programOf(tool, kind)) + quietFlag(kind) + languageFlag(kind, lang) +
                     named + " -o " + quote(program) + configFlags(kind, config, arch);
    return recipe;
}

namespace {

std::string objectFor(const std::string& dir, const std::string& source,
                      const char* suffix) {
    std::string leaf = path::filename(source);
    size_t dot = leaf.find_last_of('.');
    if (dot != std::string::npos) leaf.resize(dot);
    return path::join(dir, leaf + suffix);
}

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
                         forLanguage + crt + configFlags(kind, config, arch) +
                         (config == ConfigDebug ? " /Fd" + quote(pdb) : std::string()) +
                         " /Fo" + quoteDirectory(objectDir + kSep) + named;

        for (size_t i = 0; i < sources.size(); ++i)
            objects.push_back(objectFor(objectDir, sources[i], ".obj"));
        recipe.leftovers = objects;
        if (config == ConfigDebug) recipe.leftovers.push_back(pdb);
        return recipe;
    }

    recipe.command = "cd " + quote(objectDir) + " && " +
                     quote(programOf(tool, kind)) + quietFlag(kind) + " -c" +
                     languageFlag(kind, lang) + named +
                     configFlags(kind, config, arch);

    for (size_t i = 0; i < sources.size(); ++i)
        objects.push_back(objectFor(objectDir, sources[i], ".o"));
    recipe.leftovers = objects;
    return recipe;
}

Recipe linkRecipe(const Toolchain& tool, const std::vector<std::string>& objects,
                  bool withCpp, const std::string& arch, Configuration config,
                  const std::string& program) {
    (void)tool;
    (void)arch;
    Recipe recipe;
    recipe.assemblyPath = program;

    std::string named;
    for (size_t i = 0; i < objects.size(); ++i) named += " " + quote(objects[i]);

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

    if (kind == ToolMsvc) {

        std::string obj = mine("rstudio-run") + ".obj";
        std::string forLanguage = (lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC";

        std::string pdb = mine("rstudio-run") + ".pdb";
        recipe.command = quote(program) + " /nologo /diagnostics:column" + forLanguage +
                         configFlags(kind, config, arch) +
                         (config == ConfigDebug ? " /Fd" + quote(pdb) : std::string()) +
                         " /Fe" + quote(recipe.assemblyPath) +
                         " /Fo" + quote(obj) + " " + quote(source) +
                         (config == ConfigDebug ? " /link /DEBUG" : std::string());
        recipe.leftovers.push_back(obj);
        if (config == ConfigDebug) {
            recipe.leftovers.push_back(pdb);
            recipe.leftovers.push_back(mine("rstudio-run") + ".ilk");
        }
        return recipe;
    }

    recipe.command = quote(program) + quietFlag(kind) + " " + quote(source) + " -o " +
                     quote(recipe.assemblyPath) + configFlags(kind, config, arch);
    return recipe;
}

std::string shownProgramCommand(const Toolchain& tool, ToolchainKind kind,
                                const std::string& source, Language lang,
                                const std::string& arch, Configuration config) {
    std::string program = programOf(tool, kind);
    if (kind == ToolMsvc)
        return program + " /diagnostics:column" +
               ((lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC") +
               configFlags(kind, config, arch) + " /Ferstudio-run " + source;
    return program + " " + source + " -o rstudio-run" + configFlags(kind, config, arch);
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
                         forLanguage + configFlags(kind, config, arch) +
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
    recipe.command = quote(program) + quietFlag(kind) + " -S" + languageFlag(kind, lang) + " " +
                     quote(source) + " -o " + quote(recipe.assemblyPath) +
                     (usesArch(kind) ? " -arch " + arch : std::string()) +
                     configFlags(kind, config, arch);
    return recipe;
}

std::string shownCommand(const Toolchain& tool, ToolchainKind kind,
                         const std::string& source, Language lang,
                         const std::string& arch, Configuration config) {
    std::string program = programOf(tool, kind);
    if (kind == ToolMsvc)
        return program + " /c /diagnostics:column /FAs" +
               ((lang == LangCpp) ? " /TP /EHsc /std:c++14" : " /TC") +
               configFlags(kind, config, arch) + " " + source;
    if (kind == ToolShc)
        return program + " -S " + source + " --target=" + arch;
    return program + " -S " + source + " -arch " + arch +
           configFlags(kind, config, arch);
}

bool prepareFor(ToolchainKind kind) {
#ifdef _WIN32
    if (kind == ToolMsvc) return importMsvcEnvironment();

    importMsvcEnvironment();
    return true;
#else
    (void)kind;
    return true;
#endif
}

}
