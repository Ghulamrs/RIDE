#ifndef EDITOR_WORKSPACE_H
#define EDITOR_WORKSPACE_H

#include <string>

#include "project.h"

namespace editor {

struct Outcome {
    bool ok = false;
    std::string message;
    std::string path;
};

Outcome createFile(Project& project, const std::string& relative,
                   const std::string& group);

Outcome renameFile(Project& project, const std::string& fromAbsolute,
                   const std::string& toRelative);

Outcome deleteFile(Project& project, const std::string& absolute);

Outcome moveToGroup(Project& project, const std::string& absolute,
                    const std::string& group);

std::string groupForFile(const std::string& name);

// A file saved under the project's root joins the project, in the group its
// name puts it in, unless it is there already or lies where a project file
// may not; nothing is said when it does not apply, and the file stays saved.
Outcome adoptSaved(Project& project, const std::string& absolute);

Outcome addExisting(Project& project, const std::string& absolute,
                    const std::string& group);

Outcome removeExisting(Project& project, const std::string& absolute);

Outcome beginProject(Project& project, const std::string& directory,
                     const std::string& name, const std::string& firstFile);
Outcome saveProject(Project& project);

Outcome beginFromWhatIsThere(Project& project, const std::string& directory);

std::string demoDirectory();

}

#endif
