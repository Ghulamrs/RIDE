#ifndef EDITOR_SETTINGS_H
#define EDITOR_SETTINGS_H

#include <string>

namespace editor {

namespace settings {

std::string fileName();

std::string formerFileName();

std::string lastProject();

bool rememberProject(const std::string& directory);

bool plainFrame();

std::string configuration();
bool rememberConfiguration(const std::string& which);

std::string codeFont();
bool rememberCodeFont(const std::string& described);

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
bool rememberHeaderDirs(const std::string& include, const std::string& lib);
bool rememberVcvars(const std::string& file);
// Writes the file with the two directories when there is none yet, so that a
// person opening the installation sees what is in force.
bool writeInstallFileIfAbsent();
// For the suite: take this directory for the installation's, in place of
// the one above the running binary. Empty puts it back.
void pretendInstalledAt(const std::string& directory);

}
}

#endif
