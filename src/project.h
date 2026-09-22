#ifndef EDITOR_PROJECT_H
#define EDITOR_PROJECT_H

#include <string>
#include <vector>

#include "indent.h"
#include "toolchain.h"

namespace editor {

struct Group {
    std::string name;
    std::vector<std::string> files;

    ToolchainKind toolchain;

    Group() : toolchain(ToolAuto) {}
};

struct Part {
    std::string group;
    ToolchainKind toolchain;
    Language lang;
    std::vector<std::string> sources;

    Part() : toolchain(ToolAuto), lang(LangPlain) {}
};

struct Target {
    std::string name;
    std::vector<std::string> groups;
    /*  What to pass the program when it is run. A project whose target is a
     *  tool rather than a demonstration needs something to work on - the
     *  compilerpp project is a compiler, and Run with nothing after it only
     *  ever printed that compiler's usage. Each entry is one argument,
     *  already split, so a path with a space in it stays one argument. A
     *  relative path is taken from the project's root, which is where the
     *  program is run from. */
    std::vector<std::string> args;
};

ToolchainKind toolchainOf(const Toolchain& tool, const Part& part);

class Project {
public:
    Project();

    static const char* fileName();
    static const char* formerFileName();

    static std::string fileIn(const std::string& directory);

    static const char* suffix();

    static std::vector<std::string> projectFilesIn(const std::string& directory);

    bool load(const std::string& dir, std::string& error);
    bool save(std::string& error);

    bool saveAs(const std::string& file, std::string& error);

    bool loaded() const { return loaded_; }
    const std::string& root() const { return root_; }

    void setRoot(const std::string& path) { root_ = path; }
    const std::string& file() const { return file_; }
    const std::string& name() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    const std::vector<Group>& groups() const { return groups_; }

    const Target& target() const { return target_; }
    bool builds() const { return !target_.groups.empty(); }
    void setTarget(const Target& target) { target_ = target; }

    bool targetSources(std::vector<std::string>& sources, Language& lang,
                       std::string& why, std::string* detail = 0) const;
    // **Run file on one source of a several-source build runs the project.**
    // The file alone links against nothing, and a wall of unresolved names
    // is what Run file gave on CXX1Lab. Answers the count of sources the
    // file is one of, or 0 when it is not part of the build or stands alone.
    size_t runsAsProject(const std::string& file) const;

    bool targetParts(std::vector<Part>& parts, std::string& why,
                     std::string* detail = 0) const;

    ToolchainKind toolchainFor(const std::string& group) const;
    void setGroupToolchain(const std::string& group, ToolchainKind kind);

    static std::string stemOf(const std::string& leaf);

    std::string targetProgram() const;
    /*  The run arguments, as written; and resolved against the root, which
     *  is what actually reaches the program. */
    const std::vector<std::string>& targetArgs() const { return target_.args; }
    std::vector<std::string> absoluteTargetArgs() const;
    void setTargetArgs(const std::vector<std::string>& args) { target_.args = args; }
    const IndentStyle& indent() const { return indent_; }
    ToolchainKind toolchain() const { return toolchain_; }
    const std::string& arch() const { return arch_; }

    // Header directories the project's sources include from, and libraries
    // its program links, each relative to the root as written in the file
    // (an absolute one stays absolute); the second form has them resolved.
    const std::vector<std::string>& includes() const { return includes_; }
    const std::vector<std::string>& libraries() const { return libraries_; }
    void setIncludes(const std::vector<std::string>& dirs) { includes_ = dirs; }
    void setLibraries(const std::vector<std::string>& files) { libraries_ = files; }
    std::vector<std::string> absoluteIncludes() const;
    std::vector<std::string> absoluteLibraries() const;

    // The file to open with the project, relative to the root: what the file
    // says, else the one that defines main, else the first there is. Empty
    // when the project holds no file at all.
    const std::string& openFile() const { return open_; }
    void setOpenFile(const std::string& relative) { open_ = relative; }
    std::string fileToOpen() const;
    std::string mainFile() const;

    void setIndent(const IndentStyle& style) { indent_ = style; indentSaid_ = true; }
    void setToolchain(ToolchainKind kind) { toolchain_ = kind; }
    void setArch(const std::string& arch) { arch_ = arch; }

    void begin(const std::string& dir, const std::string& name);

    static bool allows(const std::string& relative, std::string& why);

    std::vector<std::string> directories() const;

    void close();

    void addGroup(const std::string& group);
    bool addFile(const std::string& relative, const std::string& group);
    bool removeFile(const std::string& relative);
    bool renameFile(const std::string& from, const std::string& to);
    bool moveToGroup(const std::string& relative, const std::string& group);

    size_t groupOf(const std::string& relative) const;

    std::string absolute(const std::string& relative) const;
    std::string relative(const std::string& path) const;

private:
    bool loaded_;
    std::string root_;
    std::string file_;
    std::string name_;
    std::vector<Group> groups_;
    Target target_;
    IndentStyle indent_;

    bool oneShalimarProgram(std::vector<std::string>& sources, std::string& why,
                            std::string* detail) const;
    ToolchainKind toolchain_;
    std::string arch_;
    std::vector<std::string> includes_;
    std::vector<std::string> libraries_;
    std::string open_;
    // Whether the file names its own indentation; unsaid, the
    // installation's settings.json answers and nothing is written.
    bool indentSaid_ = false;
};

}

#endif
