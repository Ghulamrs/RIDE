#include "about.h"
#include "path.h"

#include <cstdio>
#include <string>

#if defined(_WIN32)
#define POPEN  _popen
#define PCLOSE _pclose
#else
#define POPEN  popen
#define PCLOSE pclose
#endif

namespace editor {
namespace about {

const char* name() { return "RStudio"; }

const char* version() { return "3.5"; }

namespace {

// **The compilers are asked rather than listed.** Writing their numbers here
// would be writing them twice, and this copy would be the one that went stale:
// the editor does not build them and has no way to know when one moved.
//
// It also answers the question a reader of this box actually has, which is not
// "what was this built against" but "what is it driving now". A copy standing
// beside a compiler from another release is exactly the case worth seeing, and
// a hard-coded string would hide it.
//
// Absence is not an error. The editor finds what it drives beside itself, and
// a copy shipped on its own is an ordinary state - so the row says so rather
// than disappearing. A missing line is harder to notice than one that reports
// what is missing.
std::string askVersion(const std::string& program) {
    const std::string found = path::besideProgram(program);
    if (found.empty()) return std::string();

    // Quoted, because a path may hold a space. `--version` is the flag all
    // four answer. cc1, shc and c2s write one line and stop; cxx1 writes its
    // banner first and its version on the second line, so the line kept is
    // the last one that names a version - and the first line when none does,
    // which is what the other three come to.
    const std::string command = "\"" + found + "\" --version 2>&1";
    FILE* pipe = POPEN(command.c_str(), "r");
    if (!pipe) return std::string();

    char buffer[256];
    std::string first, versioned;
    while (std::fgets(buffer, sizeof buffer, pipe)) {
        std::string line = buffer;
        while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r'))
            line.resize(line.size() - 1);
        if (first.empty()) first = line;
        if (line.find("ersion") != std::string::npos ||
            line.find_first_of("0123456789") != std::string::npos)
            versioned = line;
    }
    PCLOSE(pipe);

    return versioned.empty() ? first : versioned;
}

// Named by the row when the answer does not name itself: cxx1's version line
// is "Version 1.2, sealed ..." with no cxx1 in it, and a row that could be
// any of the four is a row that says nothing.
std::string cell(const std::string& program) {
    const std::string answer = askVersion(program);
    if (answer.empty()) return program + " - not beside this program";
    std::string stem = program;
    if (stem.size() > 4 && stem.compare(stem.size() - 4, 4, ".exe") == 0) stem.resize(stem.size() - 4);
    return answer.find(stem) == std::string::npos ? stem + ": " + answer : answer;
}

// **One per line, since 3.0.** Three answers used to share a row, because the
// box was seven lines before the compilers joined it and a fourth row would
// have pushed the last one off the panel. Editor::fitPanelTo grows the panel
// to what About puts in it now, and the window asks for the room it needs -
// so the row went when cxx1 arrived, rather than four answers being squeezed
// into eighty columns: cxx1 answers --version with a banner longer than a
// column, and a row that wraps reads worse than a row per compiler.
void tool(std::vector<std::string>& said, const std::string& program) {
    said.push_back("  " + cell(program));
}

}

std::vector<std::string> lines() {
    std::vector<std::string> said;
    said.push_back(std::string(name()) + " " + version());
    said.push_back("A terminal editor and a window, over one core.");
    // Asked one at a time, in the order the menus offer the languages. The
    // list replaced a sentence naming the compilers, which said less: this
    // says which ones are actually here, and which are not.
    said.push_back("the compilers it drives, as they answer for themselves:");
    tool(said, "cc1.exe");
    tool(said, "cxx1.exe");
    tool(said, "shc.exe");
    tool(said, "c2s.exe");
    said.push_back("");
    said.push_back("Copyright (c) 2026 G. R. Akhtar");
    said.push_back("Islamabad, Pakistan");
    return said;
}

}
}
