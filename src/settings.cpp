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

std::string lastProject() {
    std::string project = readAll().get("project").text("");
    if (project.empty() || !path::exists(project)) return std::string();
    return project;
}

bool rememberProject(const std::string& directory) {
    if (fileName().empty() || directory.empty()) return false;

    Json root = readAll();
    root.set("project", Json::fromText(path::absolute(directory)));

    return writeAll(root);
}

}
}
