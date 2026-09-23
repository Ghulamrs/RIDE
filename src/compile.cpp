#include "compile.h"

#include "path.h"
#include "product.h"
#include "settings.h"

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

// A build whose program cannot be written where it would go - the project's
// own directory, as a rule - is refused before any compiler runs, and the
// refusal names the directory and what to do about it. Found the hard way:
// RIDE 4.0's installer starts the editor in its own examples\ under Program
// Files, where a normal user may read and not write, and the build then died
// in the linker with "LNK1104: cannot open file ...demo.exe" under a hint
// about vcvars64.bat, neither of which is the matter. A directory is
// writable if a file can be made in it; the read-only attribute _access
// answers with is not what UAC withholds.
std::string unwritable(const std::string& program) {
    if (program.empty()) return std::string();
    std::string dir = path::parent(program);
    if (dir.empty()) dir = ".";
    std::string probe = path::join(dir, std::string(".") + product::kLower + "-writes-here");
    if (std::FILE* f = std::fopen(probe.c_str(), "wb")) {
        std::fclose(f);
        std::remove(probe.c_str());
        return std::string();
    }
    return dir + " cannot be written to, and the program would go there - "
           "copy the project to a folder of your own, such as Documents, and build it there";
}

bool refusedUnwritable(const std::string& program, Built& result, LineSink sink, void* context) {
    std::string why = unwritable(program);
    if (why.empty()) return false;
    result.output = why + "\n";
    if (sink) sink(context, why);
    return true;
}

// The front end's way of asking, or none.
AskNative askNative = 0;
void* askNativeContext = 0;

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

void setAskNative(AskNative ask, void* context) {
    askNative = ask;
    askNativeContext = context;
}

// Whether a failed build is one the native tools might make: it failed, the
// compilers found no fault in the source (a fault of the user's is not the
// tools'), one of the project's own tools was in play for this target, and
// the settings say to ask. What the question names is what was in play.
bool nativeFallbackWanted(bool ok, bool sourceFault, const std::string& arch,
                          std::string& question) {
    question.clear();
    if (ok || sourceFault || settings::nativeForced() || !settings::askNative()) return false;
    std::string ours, theirs;
    if (isEmulated(arch)) {
        if (settings::tilinker().empty()) return false;
        ours = "lnk6x"; theirs = "TI's lnk6x";
    } else if (arch == "x86_64-windows") {
        bool as = !settings::assembler().empty(), ld = !settings::linker().empty();
        if (!as && !ld) return false;
        ours = as && ld ? "masm and link" : as ? "masm" : "link";
        theirs = as && ld ? "Visual Studio's ml64 and link.exe" : as ? "Visual Studio's ml64" : "Microsoft's link.exe";
    } else {
        return false;
    }
    if (!nativeToolsAvailable(arch)) {
        question = "The project's own " + ours + " did not build it, and " + theirs +
                   (isEmulated(arch) ? " is not on this machine - Tools names TI's C6000 compiler directory"
                                     : " are not on this machine - no Visual Studio was found");
        return false;
    }
    question = "The project's own " + ours + " did not build it. Use " + theirs +
               " for this build instead?";
    return true;
}

namespace {

// The build again through the native tools, when the front end says yes to
// the question; the answer to the first build stands otherwise. The
// recipes read the settings as they go, so forcing native for the retry is
// the whole switch: assembler(), linker() and tilinker() answer nothing.
template <class Again>
Built withNativeFallback(Built first, const std::string& arch, LineSink sink, void* context,
                         Again again) {
    std::string question;
    if (!nativeFallbackWanted(first.ok, first.diag.present, arch, question)) {
        if (!question.empty()) {
            first.output += question + "\n";
            if (sink) sink(context, question);
        }
        return first;
    }
    if (!askNative) {
        // --build and --run have no one to ask: the build stands as failed,
        // and the line says what an interactive front end would have asked.
        std::string unasked = question + " (nothing here can ask - the build stands)";
        first.output += unasked + "\n";
        if (sink) sink(context, unasked);
        return first;
    }
    if (!askNative(askNativeContext, question)) return first;
    std::string said = "building again with the native tools, as asked";
    first.output += said + "\n";
    if (sink) sink(context, said);
    settings::forceNative(true);
    Built second = again();
    settings::forceNative(false);
    second.output = first.output + second.output;
    return second;
}

}

Build build(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
            Language lang, const std::string& arch, Configuration config,
            LineSink sink, void* context) {
    Build result;

    if (!prepareFor(kind)) {
        result.output = "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools\n";
        if (sink) sink(context, "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools");
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

namespace {

Built buildProgramOnce(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
                       Language lang, const std::string& arch, Configuration config,
                       LineSink sink, void* context) {
    Built result;

    if (!prepareFor(kind)) {
        result.output = "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools\n";
        if (sink) sink(context, "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools");
        return result;
    }

    Recipe recipe = programRecipe(tool, kind, sourcePath, lang, arch, config);
    result.program = recipe.assemblyPath;
    result.leftovers = recipe.leftovers;
    result.shalimar = kind == ToolShc;
    if (refusedUnwritable(result.program, result, sink, context)) return result;

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

struct ProgramAgain {
    const Toolchain* tool; ToolchainKind kind; const std::string* source; Language lang;
    const std::string* arch; Configuration config; LineSink sink; void* context;
    Built operator()() const {
        return buildProgramOnce(*tool, kind, *source, lang, *arch, config, sink, context);
    }
};

}

Built buildProgram(const Toolchain& tool, ToolchainKind kind, const std::string& sourcePath,
                   Language lang, const std::string& arch, Configuration config,
                   LineSink sink, void* context) {
    ProgramAgain again = {&tool, kind, &sourcePath, lang, &arch, config, sink, context};
    return withNativeFallback(again(), arch, sink, context, again);
}

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool copyText(const std::string& from, const std::string& to) {
    std::FILE* in = std::fopen(from.c_str(), "rb");
    if (!in) return false;
    std::FILE* out = std::fopen(to.c_str(), "wb");
    if (!out) { std::fclose(in); return false; }
    char buffer[4096];
    size_t got;
    bool ok = true;
    while ((got = std::fread(buffer, 1, sizeof buffer, in)) > 0)
        if (std::fwrite(buffer, 1, got, out) != got) { ok = false; break; }
    std::fclose(in);
    if (std::fclose(out) != 0) ok = false;
    return ok;
}

// The linker command file lnk6x needs: one flat memory for the C6747 with
// every section the compilers and TI's runtime write placed in it - the
// same file VM6747/Emulator/tests/ti.sh links the corpus with.
const std::string kTiLinkCmd =
    std::string("/* one flat memory for the C6747 and every section in it - written by ") + product::kName + " */\n" +
    "--rom_model\n--stack_size=0x4000\n--heap_size=0x100000\n"
    "MEMORY\n{\n    RAM : origin = 0xC0000000, length = 0x04000000\n}\n"
    "SECTIONS\n{\n"
    "    .text        > RAM\n    .const       > RAM\n    .data        > RAM\n    .bss         > RAM\n"
    "    .far         > RAM\n    .fardata     > RAM\n    .neardata    > RAM\n    .rodata      > RAM\n"
    "    .cinit       > RAM\n    .init_array  > RAM\n    .switch      > RAM\n    .cio         > RAM\n"
    "    .stack       > RAM\n    .sysmem      > RAM\n    .vm6747.eh   > RAM\n}\n";

std::vector<std::string> assemblyIn(const std::string& dir) {
    std::vector<std::string> found;
    std::vector<path::Entry> all = path::entries(dir);
    for (size_t i = 0; i < all.size(); ++i) {
        const std::string& n = all[i].name;
        if (!all[i].directory && n.size() > 2 && n.compare(n.size() - 2, 2, ".s") == 0 && n.find(' ') == std::string::npos)
            found.push_back(path::join(dir, n));
    }
    return found;
}

// **A tms6747 build made a real TI program too.** What the emulator runs is
// the assembly in <program>.vm; with asm6x beside the editor each .s of it
// becomes a TI object, and with TI's compiler directory named under Tools
// lnk6x links them - the Shalimar runtime's objects beside a Shalimar
// program's - against TI's runtime into <program>.out, the file CCS would
// load onto a C6747. Neither step is needed to run on the emulator, which
// reads the assembly, and neither runs here: the .out is for the board. An
// assembler that refuses, or a link that fails, fails the build - what the
// emulator runs must be a TI program, or the emulator is proving nothing.
void makeTiProgram(Built& result, const std::string& program, LineSink sink, void* context) {
    if (!result.ok) return;
    std::string as = c6xAssembler();
    if (as.empty()) return;
    std::string dir = result.program;
    std::vector<std::string> sources = assemblyIn(dir);
    if (sources.empty()) return;
    std::string say;
    if (result.shalimar) {
        // the runtime's assembly, copied in and assembled here rather than
        // beside the editor, which is the installation's to keep
        std::string runtime = shalimarRuntimeDir();
        std::string into = path::join(dir, "shmrt");
        path::makeDirectories(into);
        std::vector<std::string> theirs = assemblyIn(runtime);
        for (size_t i = 0; i < theirs.size(); ++i) {
            std::string to = path::join(into, path::filename(theirs[i]));
            if (!copyText(theirs[i], to)) { say = "cannot copy the Shalimar runtime's " + path::filename(theirs[i]); break; }
            sources.push_back(to);
        }
        if (!say.empty()) { result.ok = false; result.output += say + "\n"; if (sink) sink(context, say); return; }
    }
    std::string command = q(as);
    for (size_t i = 0; i < sources.size(); ++i) command += " " + q(sources[i]);
    if (sink) sink(context, "$ asm6x " + std::to_string(sources.size()) + " sources");
    if (runCaptured(command, result.output, sink, context) != 0) {
        result.ok = false;
        std::string hint = "asm6x refused the assembly - the emulator would run it, but it is not a TI program";
        result.output += hint + "\n";
        if (sink) sink(context, hint);
        return;
    }
    std::vector<std::string> objects;
    for (size_t i = 0; i < sources.size(); ++i)
        objects.push_back(sources[i].substr(0, sources[i].size() - 2) + ".obj");

    std::string ti = settings::ti();
    if (ti.empty()) {
        if (sink) sink(context, "[" + std::to_string(objects.size()) + " TI objects made; a .out needs TI's linker, named under Tools]");
        return;
    }
    // The project's own C6000 linker where one is named, TI's otherwise;
    // the runtime and the command file are TI's either way - see
    // settings::tilinker and tiLinker, which also says when a linker that was
    // named has gone, so that TI's standing in for it is never silent.
    LinkerChoice choice = tiLinker(settings::tilinker(), settings::namedTilinker(), ti);
    if (choice.path.empty()) {
        result.ok = false;
        std::string hint = choice.say.empty()
            ? "no lnk6x under " + ti + " - Tools names TI's C6000 compiler directory, the one with bin\\lnk6x"
            : choice.say;
        result.output += hint + "\n";
        if (sink) sink(context, hint);
        return;
    }
    if (!choice.say.empty()) {
        result.output += choice.say + "\n";
        if (sink) sink(context, choice.say);
    }
    std::string lnk = choice.path;
    std::string cmdfile = path::join(dir, "ti-link.cmd");
    if (std::FILE* f = std::fopen(cmdfile.c_str(), "wb")) { std::fputs(kTiLinkCmd.c_str(), f); std::fclose(f); }
    // the exception-handling build of TI's runtime where there is one (CCS
    // ships the other; the C++ programs need this one), else the shipped one
    std::string lib = path::join(ti, "lib"), extra = settings::tilib();
    std::string rts = "rts6740_elf.lib";
    if (path::exists(path::join(lib, "rts6740_elf_eh.lib")) || (!extra.empty() && path::exists(path::join(extra, "rts6740_elf_eh.lib"))))
        rts = "rts6740_elf_eh.lib";
    std::string out = program;
    if (out.size() > 4 && out.compare(out.size() - 4, 4, ".exe") == 0) out.resize(out.size() - 4);
    out += ".out";
    std::string link = q(lnk) + " -mv6740 --abi=eabi -i " + q(lib) + (extra.empty() ? std::string() : " -i " + q(extra)) +
                       " " + q(cmdfile);
    for (size_t i = 0; i < objects.size(); ++i) link += " " + q(objects[i]);
    link += " -l " + rts + " -o " + q(out);
    if (sink) sink(context, "$ lnk6x " + std::to_string(objects.size()) + " objects, " + rts + " -o " + path::filename(out));
    if (runCaptured(link, result.output, sink, context) != 0) {
        result.ok = false;
        std::string hint = "lnk6x did not link it - see its messages above";
        result.output += hint + "\n";
        if (sink) sink(context, hint);
        return;
    }
    if (sink) sink(context, "[linked " + out + "]");
}

}

// Nothing named: TI's own, .exe or not. Named and there: that one, and the
// console says which, because ours and TI's carry the same name and only the
// path tells them apart. Named and gone: TI's, and the console says so.
LinkerChoice tiLinker(const std::string& chosen, const std::string& named,
                      const std::string& tiDir) {
    LinkerChoice choice;
    // --tilinker skips the check settings::tilinker() makes, so a path typed
    // wrong arrives here whole. It is a choice stated for this run: say that
    // it is not there, rather than falling through to TI's and failing with
    // TI's directory named, which is not what went wrong.
    if (!chosen.empty() && !path::exists(chosen)) {
        choice.say = "the linker named for tms6747 is not there: " + chosen;
        return choice;
    }
    if (!chosen.empty()) {
        choice.path = chosen;
        choice.say = "[linking with " + chosen + "]";
        return choice;
    }
    std::string theirs = path::join(path::join(tiDir, "bin"), "lnk6x.exe");
    if (!path::exists(theirs)) theirs = path::join(path::join(tiDir, "bin"), "lnk6x");
    if (!path::exists(theirs)) return choice;
    choice.path = theirs;
    if (!named.empty())
        choice.say = "[" + named + " is not there - linking with TI's " + theirs + "]";
    return choice;
}

namespace {

Built buildTargetOnce(const Toolchain& tool, ToolchainKind kind,
                      const std::vector<std::string>& sources, Language lang,
                      const std::string& arch, Configuration config,
                      const std::string& program, LineSink sink, void* context) {
    Built result;

    if (sources.empty()) {
        result.output = "nothing to build\n";
        return result;
    }
    if (refusedUnwritable(program, result, sink, context)) return result;

    if (!prepareFor(kind)) {
        result.output = "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools\n";
        if (sink) sink(context, "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools");
        return result;
    }

    Recipe recipe = targetRecipe(tool, kind, sources, lang, arch, config, program);
    result.program = recipe.assemblyPath;
    result.shalimar = kind == ToolShc;
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
    if (isEmulated(arch)) makeTiProgram(result, program, sink, context);
    return result;
}

struct TargetAgain {
    const Toolchain* tool; ToolchainKind kind; const std::vector<std::string>* sources; Language lang;
    const std::string* arch; Configuration config; const std::string* program; LineSink sink; void* context;
    Built operator()() const {
        return buildTargetOnce(*tool, kind, *sources, lang, *arch, config, *program, sink, context);
    }
};

}

Built buildTarget(const Toolchain& tool, ToolchainKind kind,
                  const std::vector<std::string>& sources, Language lang,
                  const std::string& arch, Configuration config,
                  const std::string& program, LineSink sink, void* context) {
    TargetAgain again = {&tool, kind, &sources, lang, &arch, config, &program, sink, context};
    return withNativeFallback(again(), arch, sink, context, again);
}

namespace {

Built buildPartsOnce(const Toolchain& tool, const std::vector<Part>& parts,
                     const std::string& arch, Configuration config,
                     const std::string& program, LineSink sink, void* context) {
    Built result;

    if (parts.empty()) {
        result.output = "nothing to build\n";
        return result;
    }
    if (refusedUnwritable(program, result, sink, context)) return result;

    // One part is its compiler's own link - unless the project names
    // libraries, which cc1 and cxx1 do not take: those go to the host's
    // linker below, with the objects. The emulated target links nothing and
    // a Shalimar program links its own runtime, so those two stay.
    if (parts.size() == 1) {
        ToolchainKind only = toolchainOf(tool, parts[0]);
        if (tool.libraries.empty() || isEmulated(arch) || only == ToolShc)
            return buildTargetOnce(tool, only, parts[0].sources, parts[0].lang, arch, config,
                                   program, sink, context);
    }

    bool withCpp = false;
    for (size_t i = 0; i < parts.size(); ++i) {
        ToolchainKind kind = toolchainOf(tool, parts[i]);
        if (!prepareFor(kind)) {
            result.output = "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools\n";
            if (sink) sink(context, "no Visual Studio found - cl, ml64 and link cannot be run; name its vcvars64.bat under Tools");
            return result;
        }
        if (parts[i].lang == LangCpp) withCpp = true;
    }

    // For the emulated target the parts' assembly is the program: each part
    // writes its .s files straight into <program>.vm, and there is no link.
    const bool emulated = isEmulated(arch);
    std::string objects = emulated ? emulatedProgram(program) : temporaryDirectory((std::string(product::kLower) + "-parts").c_str());
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
        for (size_t i = 0; i < parts.size(); ++i)
            if (toolchainOf(tool, parts[i]) == ToolShc) result.shalimar = true;
        makeTiProgram(result, program, sink, context);
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

struct PartsAgain {
    const Toolchain* tool; const std::vector<Part>* parts; const std::string* arch;
    Configuration config; const std::string* program; LineSink sink; void* context;
    Built operator()() const {
        return buildPartsOnce(*tool, *parts, *arch, config, *program, sink, context);
    }
};

}

Built buildParts(const Toolchain& tool, const std::vector<Part>& parts,
                 const std::string& arch, Configuration config,
                 const std::string& program, LineSink sink, void* context) {
    PartsAgain again = {&tool, &parts, &arch, config, &program, sink, context};
    return withNativeFallback(again(), arch, sink, context, again);
}

Ran runBuilt(const std::string& program, LineSink sink, void* context, bool shalimar,
             const std::vector<std::string>& args) {
    Ran result;
    if (program.empty()) return result;

    result.built = true;
    result.ran = true;
    result.status = runCaptured(launchCommand(program, shalimar, args), result.output, sink, context);
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
    // temporary directory filled with ride-run-<pid>.dSYM bundles, one per
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
        result.status = runCaptured(launchCommand(made.program, kind == ToolShc) + noInput,
                                    result.output, sink, context);
    }

    removeProgram(made);
    return result;
}

}
