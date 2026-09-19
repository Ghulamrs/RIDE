#ifndef EDITOR_SETTINGS_H
#define EDITOR_SETTINGS_H

#include <string>
#include <vector>

namespace editor {

namespace settings {

std::string fileName();

std::string formerFileName();

std::string lastProject();

bool rememberProject(const std::string& directory);
// The last three projects opened, most recent first, as the Project menu
// lists them; lastProject() is the first of them.
std::vector<std::string> recentProjects();
// The last three files opened on their own, most recent first, for the
// File menu the same way.
std::vector<std::string> recentFiles();
bool rememberFile(const std::string& path);

bool plainFrame();

std::string configuration();
bool rememberConfiguration(const std::string& which);

// The window's font, "Consolas 11" style, and the indentation a file is
// laid out with when its project says nothing - both in the installation's
// settings.json since 2026-09-17, the Tools menu writing the font.
std::string codeFont();
bool rememberCodeFont(const std::string& described);
size_t indentWidth();
bool indentTabs();
bool rememberIndent(size_t width, bool tabs);

std::string setAside();
bool rememberPlainFrame(bool plain);

// **The installation's settings.json**, one directory above the editor's
// bin/ - beside include/ and lib/ - and read for every compile, whether or
// not a project is open. It says where the shipped headers are (include/ is
// cxx1's, lib/ is cc1's, each relative to the file when not absolute) and,
// when a person has had to name it, the batch file that sets up Visual
// Studio's tools. Empty answers mean the directory of that name beside the
// file, if it is there; failing that the compilers look for their own.
std::string installFile();
std::string includeDir();
std::string libDir();
std::string vcvars();
// **The assembler for x86_64-windows**, the project's own (Ghulamrs/MASM's
// asm) named by its path: cc1i and cxx1i then assemble through it instead of
// ml64 and clang - CC1_AS and CXX1_AS in their environment, and cxx1i told
// -masm=masm. Empty, and the compilers choose as they always did.
std::string assembler();
void overrideAssembler(const std::string& path);   // --assembler, for this run only
// **The linker for x86_64-windows**, the project's own (Ghulamrs/LINK's
// link) named by its path: a Windows build then links through it instead of
// Microsoft's link.exe. Empty, and link.exe (or CC1_LD) as always.
std::string linker();
void overrideLinker(const std::string& path);      // --linker, for this run only
// **TI's C6000 compiler directory**, for tms6747: CCS's ti-cgt-c6000 root,
// whose bin\lnk6x links what asm6x - the project's own assembler, beside the
// editor - made of a tms6747 build into a real .out, against the runtime in
// its lib\ (and in "tilib", a second directory, for the exception-handling
// build of it that CCS does not ship). Empty, and a tms6747 build stops at
// the objects; without asm6x beside the editor it stops at the assembly the
// emulator runs, as it always did.
std::string ti();
std::string tilib();
void overrideTi(const std::string& dir);           // --ti, for this run only
void overrideTilib(const std::string& dir);        // --tilib, for this run only
// **The linker for tms6747**, the project's own (Ghulamrs/LNK6X's lnk6x)
// named by its path: a tms6747 build then links its .out through it instead
// of TI's bin\lnk6x - still against TI's runtime, which "ti" and "tilib"
// name. Empty, and TI's lnk6x as always.
std::string tilinker();
void overrideTilinker(const std::string& path);    // --tilinker, for this run only
// The compiler chosen when the editor starts and no project or command
// line says otherwise: auto, cc1, cxx1, shc, msvc or c++ - "compiler" in
// the installation's settings.json. Choosing one from the menu writes it.
std::string defaultCompiler();
// Header directories every compile searches after a project's own, and
// libraries every host link takes - "includes" and "libraries" in the
// installation's settings.json, each relative to the file unless absolute.
std::vector<std::string> includes();
std::vector<std::string> libraries();
bool rememberIncludes(const std::vector<std::string>& dirs);
bool rememberLibraries(const std::vector<std::string>& files);
bool rememberDefaultCompiler(const std::string& word);
bool rememberHeaderDirs(const std::string& include, const std::string& lib);
bool rememberVcvars(const std::string& file);
bool rememberAssembler(const std::string& file);
bool rememberLinker(const std::string& file);
bool rememberTi(const std::string& dir, const std::string& lib);
bool rememberTilinker(const std::string& file);
// Writes the file with the two directories when there is none yet, so that a
// person opening the installation sees what is in force.
bool writeInstallFileIfAbsent();
// For the suite: take this directory for the installation's, in place of
// the one above the running binary. Empty puts it back.
void pretendInstalledAt(const std::string& directory);

}
}

#endif
