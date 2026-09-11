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

    Toolchain()
        : kind(ToolAuto), cc1("cc1.exe"), cl("cl"), shc("shc.exe"),
          cxx(hostCxxName()), cxx1("cxx1.exe") {}
};

ToolchainKind resolve(const Toolchain& tool, Language lang);

const char* toolchainName(ToolchainKind kind);
const char* programOf(const Toolchain& tool, ToolchainKind kind);

std::string toolchainShown(const Toolchain& tool, ToolchainKind kind);

bool usesArch(ToolchainKind kind);

bool canCompile(ToolchainKind kind, Language lang);
std::string refusal(ToolchainKind kind, Language lang);

const char* hostArch();

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

}

#endif
