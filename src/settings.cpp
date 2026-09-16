#include "settings.h"

#include <cctype>
#include <cstdio>

#include "json.h"
#include "path.h"

namespace editor {
namespace settings {

std::string fileName() {
    std::string home = path::homeDir();
    if (home.empty()) return std::string();
    return path::join(path::join(home, ".rstudio"), "config.json");
}

// The one name the settings file had before it became ~/.rstudio/config.json,
// kept so a machine that still holds it is read once and migrated forward.
const char* const kFormerName = ".rstudioconfig.json";

std::string formerFileName() {
    std::string home = path::homeDir();
    if (home.empty()) return std::string();

    return path::join(home, kFormerName);
}

namespace {

std::string toRead() {
    std::string now = fileName();
    if (!now.empty() && path::exists(now)) return now;

    std::string home = path::homeDir();
    if (home.empty()) return std::string();

    std::string old = path::join(home, kFormerName);
    if (path::exists(old)) return old;
    return std::string();
}

}

namespace {

std::string* moved = 0;
bool movedTo() { return moved != 0; }
void rememberMoved(const std::string& where) { moved = new std::string(where); }

bool writeAll(const Json& root);

Json readAll() {
    std::string where = toRead();
    if (where.empty()) return Json::object();

    FILE* in = std::fopen(where.c_str(), "rb");
    if (!in) return Json::object();
    std::string text;
    char chunk[1024];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof chunk, in)) > 0) text.append(chunk, got);
    std::fclose(in);

    std::string why;
    Json root = Json::parse(text, why);
    if (why.empty() && root.is(Json::Object)) return root;

    bool anything = false;
    for (size_t i = 0; i < text.size(); ++i)
        if (!std::isspace(static_cast<unsigned char>(text[i]))) { anything = true; break; }

    if (anything && !movedTo()) {
        std::string aside = where + ".error";
        path::remove(aside);
        if (path::rename(where, aside)) {
            rememberMoved(aside);

            writeAll(Json::object());
        }
    }
    return Json::object();
}

bool writeAll(const Json& root) {
    std::string where = fileName();
    if (where.empty()) return false;

    path::makeDirectories(path::parent(where));

    FILE* out = std::fopen(where.c_str(), "wb");
    if (!out) return false;
    std::string text = root.write();
    std::fwrite(text.data(), 1, text.size(), out);
    std::fclose(out);

    std::string home = path::homeDir();
    if (!home.empty()) {
        std::string old = path::join(home, kFormerName);
        if (old != where && path::exists(old)) path::remove(old);
    }
    return true;
}

}

bool plainFrame() { return readAll().get("plain").boolean(false); }

bool rememberPlainFrame(bool plain) {
    Json root = readAll();
    root.set("plain", Json::fromBool(plain));
    return writeAll(root);
}

std::string setAside() {
    readAll();
    return moved ? *moved : std::string();
}

std::string configuration() {
    std::string said = readAll().get("config").text("debug");
    return said == "release" ? said : std::string("debug");
}

bool rememberConfiguration(const std::string& which) {
    Json root = readAll();
    root.set("config", Json::fromText(which == "release" ? "release" : "debug"));
    return writeAll(root);
}

std::string codeFont() { return readAll().get("font").text(std::string()); }

bool rememberCodeFont(const std::string& described) {
    Json root = readAll();
    root.set("font", Json::fromText(described));
    return writeAll(root);
}

namespace {

// A pointer and never a std::string: this file is linked into the C++/CLI
// window, where a native global with a destructor registers itself with
// atexit during start-up and corrupts the onexit table before main - the
// window died with STATUS_HEAP_CORRUPTION under register_onexit_function,
// the same stack the README records for Json::get. `moved` above is a
// pointer for the same reason.
std::string* pretended = 0;

std::string installDir() {
    if (pretended && !pretended->empty()) return *pretended;
    std::string where = path::programDirectory();
    return where.empty() ? std::string() : path::parent(where);
}

Json readInstall() {
    std::string file = installFile();
    if (file.empty() || !path::exists(file)) return Json::object();

    FILE* in = std::fopen(file.c_str(), "rb");
    if (!in) return Json::object();
    std::string text;
    char chunk[1024];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof chunk, in)) > 0) text.append(chunk, got);
    std::fclose(in);

    std::string why;
    Json root = Json::parse(text, why);
    if (!why.empty() || !root.is(Json::Object)) return Json::object();
    return root;
}

bool writeInstall(const Json& root) {
    std::string file = installFile();
    if (file.empty()) return false;
    FILE* out = std::fopen(file.c_str(), "wb");
    if (!out) return false;
    std::string text = root.write() + "\n";
    size_t written = std::fwrite(text.data(), 1, text.size(), out);
    bool ok = written == text.size();
    if (std::fclose(out) != 0) ok = false;
    return ok;
}

// A directory named in the file, made absolute against it; else the
// directory of the key's name beside it, when that is there.
std::string installedDir(const char* key) {
    std::string base = installDir();
    if (base.empty()) return std::string();

    std::string said = readInstall().get(key).text(std::string());
    bool rooted = !said.empty() && (said[0] == '/' || said[0] == '\\' ||
                                    (said.size() > 1 && said[1] == ':'));
    std::string dir = said.empty() ? path::join(base, key)
                    : rooted     ? path::absolute(said)
                                 : path::absolute(path::join(base, said));
    return path::isDirectory(dir) ? dir : std::string();
}

}

void pretendInstalledAt(const std::string& directory) {
    if (!pretended) pretended = new std::string();
    *pretended = directory;
}

std::string installFile() {
    std::string base = installDir();
    return base.empty() ? std::string() : path::join(base, "settings.json");
}

std::string includeDir() { return installedDir("include"); }
std::string libDir() { return installedDir("lib"); }

std::string vcvars() {
    std::string said = readInstall().get("vcvars").text(std::string());
    return (!said.empty() && path::exists(said)) ? said : std::string();
}

bool rememberHeaderDirs(const std::string& include, const std::string& lib) {
    Json root = readInstall();
    root.set("include", Json::fromText(include));
    root.set("lib", Json::fromText(lib));
    return writeInstall(root);
}

bool rememberVcvars(const std::string& file) {
    Json root = readInstall();
    root.set("vcvars", Json::fromText(file));
    return writeInstall(root);
}

bool writeInstallFileIfAbsent() {
    std::string file = installFile();
    if (file.empty() || path::exists(file)) return true;
    // Only where there is an installation to describe - both directories
    // above the binary. A checkout built in place has at most one and gets
    // no file.
    std::string base = installDir();
    if (!path::isDirectory(path::join(base, "include")) || !path::isDirectory(path::join(base, "lib")))
        return true;
    Json root = Json::object();
    root.set("include", Json::fromText("include"));
    root.set("lib", Json::fromText("lib"));
    root.set("vcvars", Json::fromText(""));
    return writeInstall(root);
}

std::vector<std::string> recentProjects() {
    std::vector<std::string> out;
    Json root = readAll();
    const Json& recent = root.get("recent");
    for (size_t i = 0; i < recent.size() && out.size() < 3; ++i) {
        std::string one = recent.at(i).text("");
        if (!one.empty() && path::exists(one)) out.push_back(one);
    }
    // The single "project" of earlier versions, carried in as the first.
    std::string project = root.get("project").text("");
    if (out.empty() && !project.empty() && path::exists(project)) out.push_back(project);
    return out;
}

std::string lastProject() {
    std::vector<std::string> recent = recentProjects();
    return recent.empty() ? std::string() : recent[0];
}

bool rememberProject(const std::string& directory) {
    if (fileName().empty() || directory.empty()) return false;

    std::string now = path::absolute(directory);
    std::vector<std::string> recent = recentProjects();
    Json list = Json::array();
    list.push(Json::fromText(now));
    for (size_t i = 0; i < recent.size() && list.size() < 3; ++i)
        if (path::oneName(recent[i]) != path::oneName(now)) list.push(Json::fromText(recent[i]));

    Json root = readAll();
    root.set("project", Json::fromText(now));
    root.set("recent", list);
    return writeAll(root);
}

}
}
