#include "compile.h"

#include "path.h"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#define POPEN  _popen
#define PCLOSE _pclose
#else
#include <sys/wait.h>
#include <unistd.h>
#define POPEN  popen
#define PCLOSE pclose
#endif

namespace editor {

const char* const kArches[kArchCount] = {"x86_64-windows", "x86_64-linux", "arm64-darwin", "tms6747"};

namespace {

bool parseGnu(const std::string& line, Diagnostic& d) {
    const std::string marker = ": error: ";
    size_t at = line.find(marker);
    if (at == std::string::npos) return false;

    std::string where = line.substr(0, at);
    size_t colAt = where.rfind(':');
    if (colAt == std::string::npos || colAt == 0) return false;
    size_t lineAt = where.rfind(':', colAt - 1);
    if (lineAt == std::string::npos) return false;

    size_t lineNo = static_cast<size_t>(std::atol(where.c_str() + lineAt + 1));
    size_t colNo = static_cast<size_t>(std::atol(where.c_str() + colAt + 1));
    if (lineNo == 0 || colNo == 0) return false;

    d.file = where.substr(0, lineAt);
    d.line = lineNo;
    d.col = colNo;
    d.message = line.substr(at + marker.size());
    d.present = true;
    return true;
}

bool parseMsvc(const std::string& line, Diagnostic& d) {
    size_t at = line.find("): ");
    if (at == std::string::npos) return false;

    std::string rest = line.substr(at + 3);
    if (rest.compare(0, 6, "error ") != 0 && rest.compare(0, 12, "fatal error ") != 0)
        return false;

    size_t open = line.rfind('(', at);
    if (open == std::string::npos) return false;

    std::string inside = line.substr(open + 1, at - open - 1);
    size_t comma = inside.find(',');
    size_t lineNo = static_cast<size_t>(std::atol(inside.c_str()));
    if (lineNo == 0) return false;

    size_t colNo = 1;
    if (comma != std::string::npos)
        colNo = static_cast<size_t>(std::atol(inside.c_str() + comma + 1));
    if (colNo == 0) colNo = 1;

    d.file = line.substr(0, open);
    d.line = lineNo;
    d.col = colNo;

    d.message = rest.compare(0, 12, "fatal error ") == 0 ? rest.substr(12) : rest.substr(6);
    d.present = true;
    return true;
}

}

bool parseCc1Preprocessor(const std::string& first, const std::string& caretLine,
                          Diagnostic& d) {
    if (first.empty()) return false;

    size_t caret = caretLine.find('^');
    if (caret == std::string::npos) return false;
    for (size_t i = 0; i < caret; ++i)
        if (caretLine[i] != ' ' && caretLine[i] != '\t') return false;

    size_t after = first.rfind(": ");
    if (after == std::string::npos || after == 0) return false;
    std::string where = first.substr(0, after);
    size_t lineAt = where.rfind(':');
    if (lineAt == std::string::npos || lineAt == 0) return false;

    for (size_t i = lineAt + 1; i < where.size(); ++i)
        if (where[i] < '0' || where[i] > '9') return false;
    if (lineAt + 1 >= where.size()) return false;

    size_t lineNo = static_cast<size_t>(std::atol(where.c_str() + lineAt + 1));
    if (lineNo == 0) return false;

    size_t prefix = after + 2;
    size_t col = caret >= prefix ? caret - prefix + 1 : 1;

    std::string message = caretLine.substr(caret + 1);
    size_t begin = message.find_first_not_of(" \t");
    message = (begin == std::string::npos) ? std::string() : message.substr(begin);
    if (message.empty()) return false;

    d.file = where.substr(0, lineAt);
    d.line = lineNo;
    d.col = col;
    d.message = message;
    d.present = true;
    return true;
}

bool parseShalimar(const std::string& line, const std::string& source, Diagnostic& d) {
    const std::string marker = "Error: line ";
    if (line.compare(0, marker.size(), marker) != 0) return false;

    size_t at = line.find(':', marker.size());
    if (at == std::string::npos) return false;

    size_t lineNo = static_cast<size_t>(std::atol(line.c_str() + marker.size()));
    if (lineNo == 0) return false;

    size_t message = at + 1;
    while (message < line.size() && line[message] == ' ') ++message;

    d.file = source;
    d.line = lineNo;
    d.col = 1;
    d.message = line.substr(message);
    d.present = true;
    return true;
}

Diagnostic parseDiagnostic(const std::string& text, const std::string& source) {
    Diagnostic d;

    std::string previous;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        std::string line = text.substr(at, end == std::string::npos ? std::string::npos
                                                                    : end - at);
        if (!line.empty() && line[line.size() - 1] == '\r') line.resize(line.size() - 1);

        if (parseGnu(line, d) || parseMsvc(line, d)) return d;
        if (parseShalimar(line, source, d)) return d;
        if (parseCc1Preprocessor(previous, line, d)) return d;

        previous = line;
        if (end == std::string::npos) break;
        at = end + 1;
    }

    return d;
}

namespace {

std::string temporaryDirectory(const char* what) {
    char id[32];
#ifdef _WIN32
    std::snprintf(id, sizeof id, "%lu", static_cast<unsigned long>(GetCurrentProcessId()));
#else
    std::snprintf(id, sizeof id, "%ld", static_cast<long>(getpid()));
#endif
    return path::join(path::tempDir(), std::string(what) + "-" + id);
}

}

int runCaptured(const std::string& command, std::string& output,
                LineSink sink, void* context) {
    // Nothing run from here has any business reading the editor's own input.
    // A compiler that inherits it consumes the keystrokes the editor has not
    // read yet, and on Windows that ended the session: the editor's next read
    // saw end of file and it quit, so every key pressed after a build was
    // silently the last one. The run step always said this; the build step
    // did not, and only the build step is a child that might.
#ifdef _WIN32
    const char* noInput = " < NUL";
#else
    const char* noInput = " < /dev/null";
#endif
    std::string cmd = command + noInput + " 2>&1";

#ifdef _WIN32

    cmd = "\"" + cmd + "\"";
#endif

    FILE* pipe = POPEN(cmd.c_str(), "r");
    if (!pipe) return -1;

    char chunk[512];
    std::string pending;
    while (std::fgets(chunk, sizeof chunk, pipe)) {
        output += chunk;
        if (!sink) continue;
        for (const char* p = chunk; *p; ++p) {
            if (*p == '\n') {
                sink(context, pending);
                pending.clear();
            } else if (*p != '\r') {
                pending += *p;
            }
        }
    }
    if (sink && !pending.empty()) sink(context, pending);

    int status = PCLOSE(pipe);
#ifndef _WIN32

    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
#endif
    return status;
}

namespace {

bool looksLikeMissingProgram(const std::string& output) {
    bool shellSaidSo =
        output.find("command not found") != std::string::npos ||
        output.find("not recognized as an internal or external command") != std::string::npos ||
        output.find(": No such file or directory") != std::string::npos;
    if (!shellSaidSo) return false;

    const char* ranAfterAll[] = {"Undefined symbols", "symbol(s) not found", "ld: ",
                                 "LNK", "error:", "warning:"};
    for (size_t i = 0; i < sizeof ranAfterAll / sizeof *ranAfterAll; ++i)
        if (output.find(ranAfterAll[i]) != std::string::npos) return false;
    return true;
}

}

Build build(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
            Language lang, const std::string& arch, Configuration config,
            LineSink sink, void* context) {
    Build result;

    if (!prepareFor(kind)) {
        result.output = "no Visual Studio 2022 found - cl cannot be run\n";
        if (sink) sink(context, "no Visual Studio 2022 found - cl cannot be run");
        return result;
    }

    Recipe recipe = assemblyRecipe(tool, kind, sourcePath, lang, arch, config);
    int status = runCaptured(recipe.command, result.output, sink, context);
    if (status < 0) {
        result.output = std::string("could not run ") + programOf(tool, kind);
        return result;
    }

    result.ok = (status == 0);
    result.diag = parseDiagnostic(result.output, sourcePath);

    if (!result.ok && !result.diag.present && looksLikeMissingProgram(result.output)) {
        std::string hint = std::string(programOf(tool, kind)) +
                           " could not be run - name it with --cc1, --cxx1 or --cl, or put it on PATH";
        result.output += hint + "\n";
        if (sink) sink(context, hint);
    }

    if (result.ok) {

        FILE* assembly = std::fopen(recipe.assemblyPath.c_str(), "rb");
        if (assembly) {
            std::string line;
            for (;;) {
                int c = std::fgetc(assembly);
                if (c == EOF) {
                    if (!line.empty()) result.asmLines.push_back(line);
                    break;
                }
                if (c == '\n') {
                    if (!line.empty() && line[line.size() - 1] == '\r')
                        line.resize(line.size() - 1);
                    result.asmLines.push_back(line);
                    line.clear();
                    continue;
                }
                line += static_cast<char>(c);
            }
            std::fclose(assembly);
        }
    }

    std::remove(recipe.assemblyPath.c_str());
    for (size_t i = 0; i < recipe.leftovers.size(); ++i)
        std::remove(recipe.leftovers[i].c_str());

    return result;
}

Built buildProgram(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
                   Language lang, const std::string& arch, Configuration config,
                   LineSink sink, void* context) {
    Built result;

    if (!prepareFor(kind)) {
        result.output = "no Visual Studio 2022 found - cl cannot be run\n";
        if (sink) sink(context, "no Visual Studio 2022 found - cl cannot be run");
        return result;
    }

    Recipe recipe = programRecipe(tool, kind, sourcePath, lang, arch, config);
    result.program = recipe.assemblyPath;
    result.leftovers = recipe.leftovers;

    int made = runCaptured(recipe.command, result.output, sink, context);
    if (made < 0) {
        result.output = std::string("could not run ") + programOf(tool, kind);
        return result;
    }

    result.diag = parseDiagnostic(result.output, sourcePath);
    result.ok = (made == 0);

    if (!result.ok && !result.diag.present && looksLikeMissingProgram(result.output)) {
        std::string hint = std::string(programOf(tool, kind)) +
                           " could not be run - name it with --cc1, --cxx1 or --cl, or put it on PATH";
        result.output += hint + "\n";
        if (sink) sink(context, hint);
    }
    return result;
}

Built buildTarget(const Toolchain& tool, ToolchainKind kind,
                  const std::vector<std::string>& sources, Language lang,
                  const std::string& arch, Configuration config,
                  const std::string& program, LineSink sink, void* context) {
    Built result;

    if (sources.empty()) {
        result.output = "nothing to build\n";
        return result;
    }

    if (!prepareFor(kind)) {
        result.output = "no Visual Studio 2022 found - cl cannot be run\n";
        if (sink) sink(context, "no Visual Studio 2022 found - cl cannot be run");
        return result;
    }

    Recipe recipe = targetRecipe(tool, kind, sources, lang, arch, config, program);
    result.program = recipe.assemblyPath;
    result.leftovers = recipe.leftovers;

    int made = runCaptured(recipe.command, result.output, sink, context);
    if (made < 0) {
        result.output = std::string("could not run ") + programOf(tool, kind);
        return result;
    }

    result.diag = parseDiagnostic(result.output, sources.empty() ? std::string() : sources[0]);
    result.ok = (made == 0);

    if (!result.ok && !result.diag.present && looksLikeMissingProgram(result.output)) {
        std::string hint = std::string(programOf(tool, kind)) +
                           " could not be run - name it with --cc1, --cxx1 or --cl, or put it on PATH";
        result.output += hint + "\n";
        if (sink) sink(context, hint);
    }

    for (size_t i = 0; i < result.leftovers.size(); ++i)
        std::remove(result.leftovers[i].c_str());
    result.leftovers.clear();
    return result;
}

Built buildParts(const Toolchain& tool, const std::vector<Part>& parts,
                 const std::string& arch, Configuration config,
                 const std::string& program, LineSink sink, void* context) {
    Built result;

    if (parts.empty()) {
        result.output = "nothing to build\n";
        return result;
    }

    if (parts.size() == 1) {
        return buildTarget(tool, toolchainOf(tool, parts[0]), parts[0].sources,
                           parts[0].lang, arch, config, program, sink, context);
    }

    bool withCpp = false;
    for (size_t i = 0; i < parts.size(); ++i) {
        ToolchainKind kind = toolchainOf(tool, parts[i]);
        if (!prepareFor(kind)) {
            result.output = "no Visual Studio 2022 found - cl cannot be run\n";
            if (sink) sink(context, "no Visual Studio 2022 found - cl cannot be run");
            return result;
        }
        if (parts[i].lang == LangCpp) withCpp = true;
    }

    // For the emulated target the parts' assembly is the program: each part
    // writes its .s files straight into <program>.vm, and there is no link.
    const bool emulated = isEmulated(arch);
    std::string objects = emulated ? emulatedProgram(program) : temporaryDirectory("rstudio-parts");
    if (emulated) path::removeTree(objects);
    path::makeDirectories(objects);

    std::vector<std::string> made;
    for (size_t i = 0; i < parts.size(); ++i) {
        ToolchainKind kind = toolchainOf(tool, parts[i]);

        if (sink)
            sink(context, "$ " + parts[i].group + " (" + toolchainShown(tool, kind) + ")");

        std::vector<std::string> theirs;
        Recipe recipe = objectRecipe(tool, kind, parts[i].sources, parts[i].lang,
                                     arch, config, objects, theirs);

        int rc = runCaptured(recipe.command, result.output, sink, context);
        if (rc != 0) {

            result.diag = parseDiagnostic(result.output, parts[i].sources[0]);
            if (rc < 0 || (!result.diag.present && looksLikeMissingProgram(result.output))) {
                std::string hint = std::string(programOf(tool, kind)) +
                                   " could not be run - name it with --cc1, --cxx1 or --cl, or put "
                                   "it on PATH";
                result.output += hint + "\n";
                if (sink) sink(context, hint);
            }
            path::removeTree(objects);
            return result;
        }
        for (size_t o = 0; o < theirs.size(); ++o) made.push_back(theirs[o]);
    }

    if (emulated) {
        result.program = objects;
        result.ok = true;
        return result;
    }

    if (sink) sink(context, "$ linking with " + linkerName(withCpp));

    Recipe link = linkRecipe(tool, made, withCpp, arch, config, program);
    int linked = runCaptured(link.command, result.output, sink, context);

#ifdef __APPLE__
    // **The DWARF is in the objects, and the objects are about to go.** On a
    // Mac the linker leaves debug information where the compiler wrote it
    // and puts a map to those files in the program; lldb follows the map. The
    // driver runs dsymutil to gather it into a .dSYM only when it compiled
    // the sources itself - a link of objects, which is all this is, gets no
    // bundle however many -g it is given. So the bundle is asked for here,
    // before the objects are removed, or a breakpoint in a project of two
    // compilers stops nowhere and says nothing about why. Found when the
    // first cc1-and-cxx1 project was put under the debugger; a one-compiler
    // project never saw it because its compiler links the sources itself.
    if (linked == 0 && config == ConfigDebug)
        runCaptured("dsymutil \"" + program + "\"", result.output, sink, context);
#endif

    path::removeTree(objects);

    if (linked != 0) {

        if (linked < 0) {
            std::string hint = linkerName(withCpp) +
                               " could not be run - it is the host's linker, and on Windows "
                               "it reaches PATH only after vcvars64.bat";
            result.output += hint + "\n";
            if (sink) sink(context, hint);
        }
        return result;
    }

    result.program = program;
    result.ok = true;
    return result;
}

Ran runBuilt(const std::string& program, LineSink sink, void* context) {
    Ran result;
    if (program.empty()) return result;

    result.built = true;
    result.ran = true;
    result.status = runCaptured(launchCommand(program), result.output, sink, context);
    return result;
}

void removeProgram(const Built& built) {
    // A program for the emulated target is a .s file or a .vm directory.
    if (!built.program.empty() && path::isDirectory(built.program))
        path::removeTree(built.program);
    else if (!built.program.empty()) std::remove(built.program.c_str());
    for (size_t i = 0; i < built.leftovers.size(); ++i)
        std::remove(built.leftovers[i].c_str());
#ifdef __APPLE__
    // The .dSYM a debug build leaves beside the program - the compiler's
    // driver makes one for a single file, buildParts makes one for a project
    // - is a directory, and std::remove does not take those. Left behind, a
    // temporary directory filled with rstudio-run-<pid>.dSYM bundles, one per
    // F8, which is how this was noticed.
    if (!built.program.empty()) path::removeTree(built.program + ".dSYM");
#endif
}

Ran runProgram(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
               Language lang, const std::string& arch, Configuration config,
               LineSink sink, void* context) {
    Ran result;

    Built made = buildProgram(tool, kind, sourcePath, lang, arch, config, sink, context);
    result.output = made.output;
    result.diag = made.diag;
    result.built = made.ok;

    if (result.built) {

#ifdef _WIN32
        const char* noInput = " < NUL";
#else
        const char* noInput = " < /dev/null";
#endif
        result.ran = true;
        result.status = runCaptured(launchCommand(made.program) + noInput,
                                    result.output, sink, context);
    }

    removeProgram(made);
    return result;
}

}
