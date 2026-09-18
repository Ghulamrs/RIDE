#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "editor.h"
#include "path.h"
#include "settings.h"
#include "symbols.h"
#include "workspace.h"

static std::string calledIt(const char* argv0) {
    std::string name = (argv0 == 0 || *argv0 == 0) ? "RIDE" : argv0;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() > 4 && name.compare(name.size() - 4, 4, ".exe") == 0)
        name.resize(name.size() - 4);
    return name;
}

int main(int argc, char** argv) {
    const std::string me = calledIt(argc > 0 ? argv[0] : 0);
    std::string file;
    std::string cc1;
    std::string project;
    std::string toolchain;
    std::string config;
    std::string cl;
    std::string shc;
    std::string cxx;
    std::string cxx1;
    std::string c2s;
    std::string arch;
    bool build = false, runIt = false;
    long width = 0;
    int plain = 0;
    int tabs = -1;
    int caseIndent = -1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cc1") == 0 && i + 1 < argc) {
            cc1 = argv[++i];
        } else if (std::strcmp(argv[i], "--toolchain") == 0 && i + 1 < argc) {
            toolchain = argv[++i];
        } else if (std::strcmp(argv[i], "--cl") == 0 && i + 1 < argc) {
            cl = argv[++i];
        } else if (std::strcmp(argv[i], "--shc") == 0 && i + 1 < argc) {
            shc = argv[++i];
        } else if (std::strcmp(argv[i], "--cxx") == 0 && i + 1 < argc) {
            cxx = argv[++i];
        } else if (std::strcmp(argv[i], "--cxx1") == 0 && i + 1 < argc) {
            cxx1 = argv[++i];
        } else if (std::strcmp(argv[i], "--c2s") == 0 && i + 1 < argc) {
            c2s = argv[++i];
        } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config = argv[++i];
        } else if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
            project = argv[++i];
        } else if (std::strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            long w = std::atol(argv[++i]);
            if (w >= 1 && w <= 16) width = w;
        } else if (std::strcmp(argv[i], "--arch") == 0 && i + 1 < argc) {
            arch = argv[++i];
        } else if (std::strcmp(argv[i], "--build") == 0) {
            build = true;
        } else if (std::strcmp(argv[i], "--run") == 0) {
            build = true; runIt = true;
        } else if (std::strcmp(argv[i], "--plain") == 0) {
            plain = 1;
        } else if (std::strcmp(argv[i], "--tabs") == 0) {
            tabs = 1;
        } else if (std::strcmp(argv[i], "--case-indent") == 0) {
            caseIndent = 1;
        } else if (std::strcmp(argv[i], "-h") == 0 ||
                   std::strcmp(argv[i], "--help") == 0) {
            std::printf(
                "usage: %s [file] [--project dir] [--toolchain auto|cc1|cxx1|msvc|shc|c++]\n"
                "           [--config debug|release] [--cc1 path] [--cxx1 path] [--cl path]\n"
                "           [--shc path] [--cxx path] [--c2s path]\n"
                "           [--width n] [--tabs] [--case-indent] [--plain]\n"
                "       %s <project.pro or dir> [--arch a] --build | --run\n"
                "  RStudio - the console half, which is RStudio.exe on Linux and\n"
                "  macOS and RStudioConsole.exe on Windows. RStudioGui is the same\n"
                "  editor in a window, over the same core.\n"
                "\n"
                "  --toolchain    auto (the default) lets the file choose: C goes\n"
                "                 to cc1, C++ to cxx1 and Shalimar to shc. C and C++\n"
                "                 each have a second answer - this machine's own\n"
                "                 compiler, cl on Windows and c++ elsewhere - and\n"
                "                 Shalimar goes to the only thing that reads it.\n"
                "                 Naming one uses it for everything, and it says so\n"
                "                 where it cannot take the file\n"
                "  --config       debug (the default) or release. For cl that is\n"
                "                 /Od /Zi /D_DEBUG or /O2 /DNDEBUG; for cc1 and cxx1,\n"
                "                 -g and the define on the targets that carry a line\n"
                "                 table, and the define alone on the one that does not\n"
                "  --cc1, --cxx1, the programs to run; $CC1, $CXX1, $SHC and $CXX\n"
                "  --cl, --shc,   name them too, and without either a cc1, cxx1 or\n"
                "  --cxx          shc beside this editor is used, and failing that\n"
                "                 PATH is asked. cl is also found through Visual\n"
                "                 Studio 2022 itself, so no Developer Command Prompt\n"
                "                 is needed. --cxx is c++ by default, which is clang++\n"
                "                 on a Mac and g++ on Linux; a project file never\n"
                "                 names it, because which one it is, is a fact about\n"
                "                 a machine\n"
                "  --project      what the pane on the left shows; the file's own\n"
                "                 directory by default\n"
                "  --build, --run build the project's program the way F4 does - and\n"
                "                 run it, for --run - with no screen: the console is\n"
                "                 printed and the status is 0 when it built (and ran).\n"
                "                 --arch names the target, else the project's own\n"
                "  --width n      columns per indent step (4)\n"
                "  --tabs         indent with tabs instead of spaces\n"
                "  --plain        frame the screen with - | + instead of the box\n"
                "                 characters, for a console that draws those from\n"
                "                 a second font and breaks the lines at every join\n"
                "  --case-indent  put case labels one step inside their switch\n"
                "                 rather than in its own column\n"
                "\n"
                "  F10 menu   Ctrl-B build this file   F5 run this file\n"
                "  F4 build the project's program   Ctrl-A lay out\n"
                "  F9 breakpoint   F8 debug   F7/F6 step over/into\n"
                "  F1 keys    Ctrl-Q quit\n",
                me.c_str(), me.c_str());
            return 0;
        } else if (argv[i][0] == '-' && argv[i][1] != '\0') {
            std::fprintf(stderr, "%s: unknown option %s\n", me.c_str(), argv[i]);
            return 2;
        } else {
            file = argv[i];
        }
    }

    if (!toolchain.empty() && toolchain != "auto" && toolchain != "cc1" &&
        toolchain != "msvc" && toolchain != "cl" && toolchain != "shc" &&
        toolchain != "cxx1" && toolchain != "c++" && toolchain != "cxx" &&
        toolchain != "g++" && toolchain != "clang++") {
        std::fprintf(stderr, "%s: unknown toolchain %s\n", me.c_str(), toolchain.c_str());
        return 2;
    }

    editor::installPlatformDemangler();

    editor::Editor ed;
    // The installation's indentation, before the command line's say.
    ed.setIndentWidth(editor::settings::indentWidth());
    if (editor::settings::indentTabs()) ed.setTabs(true);

    if (!file.empty() && project.empty() && editor::path::isDirectory(file)) {
        project = file;
        file.clear();
    }
    // A .pro named on the line is the project, for --build and --run.
    if (build && !file.empty() && project.empty() && file.size() > 4 &&
        file.compare(file.size() - 4, 4, ".pro") == 0) {
        project = file;
        file.clear();
    }
    if (build) ed.setBatch(true);

    bool onItsOwn = false;
    if (project.empty() && !file.empty()) {
        size_t at = file.find_last_of("/\\");
        std::string beside = (at == std::string::npos) ? std::string(".") : file.substr(0, at);

        beside = editor::path::absolute(beside);

        std::string up = editor::path::parent(beside);
        if (!editor::Project::fileIn(beside).empty()) project = beside;
        else if (!up.empty() && !editor::Project::fileIn(up).empty()) project = up;
        else onItsOwn = true;
    }

    // The installation's settings.json, written once with its two directories.
    editor::settings::writeInstallFileIfAbsent();
    // Nothing named opens nothing: the last project is remembered and
    // offered under Project > Recent, never opened on its own.
    (void)onItsOwn;

    if (!project.empty()) ed.openProject(project);

    // The command line's compiler, else the installation's default from
    // settings.json; a project opened above may have set its own already.
    if (!toolchain.empty()) ed.setToolchain(editor::toolchainFrom(toolchain));
    else if (project.empty()) ed.setToolchain(editor::toolchainFrom(editor::settings::defaultCompiler()));

    if (config == "release") ed.setConfig(editor::ConfigRelease);
    else if (config == "debug") ed.setConfig(editor::ConfigDebug);
    else if (!config.empty()) {
        std::fprintf(stderr, "%s: unknown configuration %s\n", me.c_str(), config.c_str());
        return 2;
    }
    else if (editor::settings::configuration() == "release")
        ed.setConfig(editor::ConfigRelease);

    if (width > 0) ed.setIndentWidth(static_cast<size_t>(width));

    if (plain || editor::settings::plainFrame()) ed.setPlainFrame(true);
    if (tabs >= 0) ed.setTabs(true);
    if (caseIndent >= 0) ed.setCaseIndent(1);

    if (!cc1.empty()) ed.setCc1(cc1);
    if (!cl.empty()) ed.setCl(cl);
    if (!shc.empty()) ed.setShc(shc);
    if (!cxx1.empty()) ed.setCxx1(cxx1);
    if (!c2s.empty()) ed.setConverter(c2s);

    if (cxx.empty()) {
        const char* fromEnv = std::getenv("CXX");
        if (fromEnv && *fromEnv) cxx = fromEnv;
    }
    if (!cxx.empty()) ed.setCxx(cxx);

    if (build) {
        if (project.empty()) { std::fprintf(stderr, "%s: --build needs a project\n", me.c_str()); return 2; }
        if (!arch.empty() && !ed.setArchNamed(arch)) {
            std::fprintf(stderr, "%s: unknown target %s\n", me.c_str(), arch.c_str());
            return 2;
        }
        ed.buildProjectBatch(runIt);
        const std::vector<std::string>& lines = ed.consoleLines();
        for (size_t i = 0; i < lines.size(); ++i) std::printf("%s\n", lines[i].c_str());
        if (!ed.lastBuildOk()) return 1;
        return runIt ? (ed.lastRunStatus() == 0 ? 0 : 3) : 0;
    }

    if (!file.empty()) ed.open(file);
    else ed.openFirstFile();
    ed.run();
    return 0;
}
