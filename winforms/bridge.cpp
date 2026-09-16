
#include "bridge.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <richedit.h>
#include <richole.h>
#include <tom.h>

#pragma comment(lib, "user32.lib")
#endif

#include "about.h"
#include "compile.h"
#include "convert.h"
#include "debugger.h"
#include "shalimar/session.h"
#include "find.h"
#include "indent.h"
#include "project.h"
#include "symbols.h"
#include "syntax.h"
#include "settings.h"
#include "workspace.h"

namespace {

static_assert(RSTUDIO_KIND_KEYWORD == static_cast<int>(editor::KindKeyword), "kind numbering has drifted");
static_assert(RSTUDIO_KIND_LABEL == static_cast<int>(editor::KindLabel), "kind numbering has drifted");
static_assert(RSTUDIO_LANG_CPP == static_cast<int>(editor::LangCpp), "language numbering has drifted");
static_assert(RSTUDIO_LANG_SHALIMAR == static_cast<int>(editor::LangShalimar), "language numbering has drifted");
static_assert(RSTUDIO_LANG_ASM == static_cast<int>(editor::LangAsm), "language numbering has drifted");
static_assert(RSTUDIO_LANG_JSON == static_cast<int>(editor::LangJson), "language numbering has drifted");
static_assert(RSTUDIO_TOOL_MSVC == static_cast<int>(editor::ToolMsvc), "toolchain numbering has drifted");
static_assert(RSTUDIO_TOOL_SHC == static_cast<int>(editor::ToolShc), "toolchain numbering has drifted");
static_assert(RSTUDIO_TOOL_CXX == static_cast<int>(editor::ToolCxx), "toolchain numbering has drifted");
static_assert(RSTUDIO_TOOL_CXX1 == static_cast<int>(editor::ToolCxx1), "toolchain numbering has drifted");
static_assert(RSTUDIO_CONFIG_RELEASE == static_cast<int>(editor::ConfigRelease), "config numbering has drifted");

char* give(const std::string& text) {
    char* out = static_cast<char*>(std::malloc(text.size() + 1));
    if (!out) return 0;
    std::memcpy(out, text.c_str(), text.size() + 1);
    return out;
}

std::vector<std::string> split(const char* text) {
    std::vector<std::string> lines;
    std::string line;
    for (const char* p = text ? text : ""; *p; ++p) {
        if (*p == '\n') {
            lines.push_back(line);
            line.clear();
        } else if (*p != '\r') {
            line += *p;
        }
    }
    lines.push_back(line);
    return lines;
}

std::string join(const std::vector<std::string>& lines) {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i) out += "\n";
        out += lines[i];
    }
    return out;
}

editor::IndentStyle styleOf(int width, int tabs, int caseIndent, int dialect) {
    editor::IndentStyle style;
    if (width >= 1 && width <= 16) style.width = static_cast<size_t>(width);
    style.tabs = tabs != 0;
    style.caseIndent = caseIndent ? 1 : 0;
    style.dialect = dialect == RSTUDIO_DIALECT_SHALIMAR ? editor::DialectShalimar
                                                    : editor::DialectC;
    return style;
}

// **Made once and never destroyed, and every string this file hands back
// goes through it.** A function-local static with a destructor registers an
// atexit handler when it is first reached, and in this mixed native/managed
// image that registration corrupts the heap - Json::get did it before the
// window first opened, and rstudio_group_for_file did it again with a plain
// `static std::string answer` on 2026-09-11, so that New File took a name and
// died in _onexit. tests/test.cpp scans the window's sources for the shape.
std::string& scratch() {
    static std::string* kept = new std::string();
    return *kept;
}

#ifdef _WIN32

char faultLog[MAX_PATH] = "RStudioGui-fault.log";

void write(FILE* f, const char* text) { std::fputs(text, f); }

LONG CALLBACK onFault(EXCEPTION_POINTERS* info) {
    static bool inside = false;
    if (inside) return EXCEPTION_CONTINUE_SEARCH;
    inside = true;

    DWORD code = info->ExceptionRecord->ExceptionCode;

    // 0xE0434352 is a managed exception, seen here first-chance - handled
    // ones too - so the log holds every one and the last is the one that
    // killed the window.
    const DWORD kManaged = 0xE0434352;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != STATUS_HEAP_CORRUPTION &&
        code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
        code != kManaged) {
        inside = false;
        return EXCEPTION_CONTINUE_SEARCH;
    }

    FILE* f = std::fopen(faultLog, "a");
    if (!f) {
        inside = false;
        return EXCEPTION_CONTINUE_SEARCH;
    }

    std::fprintf(f, "\nexception 0x%08lX at %p%s\n", static_cast<unsigned long>(code),
                 info->ExceptionRecord->ExceptionAddress,
                 code == kManaged ? " (managed, first chance)" : "");

    // A managed one is noted and no more: they are routine and handled, and
    // symbolising one would load the .pdb and hold it - which kept a build
    // from writing it while a window was open.
    if (code == kManaged) {
        std::fclose(f);
        inside = false;
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (code == EXCEPTION_ACCESS_VIOLATION &&
        info->ExceptionRecord->NumberParameters >= 2) {
        std::fprintf(f, "  %s address %p\n",
                     info->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                     reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    }

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, NULL, TRUE);

    void* frames[62];
    USHORT got = RtlCaptureStackBackTrace(0, 62, frames, NULL);

    char room[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(room);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 500;

    for (USHORT i = 0; i < got; ++i) {
        DWORD64 address = reinterpret_cast<DWORD64>(frames[i]);
        DWORD64 offset = 0;

        if (SymFromAddr(process, address, &offset, symbol)) {
            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD column = 0;
            if (SymGetLineFromAddr64(process, address, &column, &line))
                std::fprintf(f, "  %2u  %s  (%s:%lu)\n", i, symbol->Name, line.FileName,
                             static_cast<unsigned long>(line.LineNumber));
            else
                std::fprintf(f, "  %2u  %s + 0x%llX\n", i, symbol->Name,
                             static_cast<unsigned long long>(offset));
        } else {
            std::fprintf(f, "  %2u  %p\n", i, frames[i]);
        }
    }

    SymCleanup(process);
    std::fclose(f);
    inside = false;
    return EXCEPTION_CONTINUE_SEARCH;
}

#endif

std::string joinedList(const std::vector<std::string>& parts) {
    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) out += (i ? ";" : "") + parts[i];
    return out;
}

std::vector<std::string> splitList(const char* line) {
    std::vector<std::string> out;
    std::string text = line ? line : "";
    size_t at = 0;
    while (at <= text.size()) {
        size_t cut = text.find(';', at);
        if (cut == std::string::npos) cut = text.size();
        std::string part = text.substr(at, cut - at);
        size_t a = part.find_first_not_of(" \t"), b = part.find_last_not_of(" \t");
        if (a != std::string::npos) out.push_back(part.substr(a, b - a + 1));
        at = cut + 1;
    }
    return out;
}




}

struct RStudioProject {
    editor::Project project;
    std::string answer;
    editor::Outcome last;

    std::vector<std::string> sources;

    std::vector<editor::Part> parts;
    std::string program;
    std::string why;
    std::string detail;
    int language;

    editor::DebugPlan plan;
    std::string whyNot;
};

namespace {

// The toolchain every build is given: the compilers as named, the
// installation's header directories, and the project's own paths when a
// project is open. The window has one project and passes it, loaded or not.
editor::Toolchain toolFrom(RStudioProject* project, const char* cc1, const char* cl,
                           const char* shc, const char* cxx1) {
    editor::Toolchain tool;
    if (cc1 && *cc1) tool.cc1 = cc1;
    if (cl && *cl) tool.cl = cl;
    if (shc && *shc) tool.shc = shc;
    if (cxx1 && *cxx1) tool.cxx1 = cxx1;
    tool.include = editor::settings::includeDir();
    tool.lib = editor::settings::libDir();
    if (project && project->project.loaded()) {
        tool.includes = project->project.absoluteIncludes();
        tool.libraries = project->project.absoluteLibraries();
    }
    return tool;
}

}

struct RStudioBuild {
    editor::Build built;
    std::string assembly;
};

struct RStudioRan {
    editor::Ran ran;
};

struct RStudioProgram {
    editor::Built built;
};

struct RStudioDebugger {
    editor::Debugger debugger;

    shalimar::Session shm;
    editor::Stop stop;
    std::vector<editor::Variable> locals;
    std::vector<editor::StackFrame> stack;
    size_t looking;
    std::string frameLine;
    std::string variableLine;
    std::string watchLine;
    std::string lookingLine;
    std::string complaint;
    std::string answer;
    std::string output;
    std::string refusal;

    RStudioDebugger() : looking(0) {}
};

extern "C" {

void rstudio_watch_for_faults(const char* logPath) {
#ifdef _WIN32
    static bool watching = false;
    if (logPath && *logPath) {
        std::strncpy(faultLog, logPath, sizeof faultLog - 1);
        faultLog[sizeof faultLog - 1] = '\0';
    }
    if (!watching) AddVectoredExceptionHandler(1, onFault);
    watching = true;
#else
    (void)logPath;
#endif
}

#ifdef _WIN32
// Watching from the first native initialiser, before main - a window that
// dies on the way to main leaves nothing otherwise, and one did.
namespace {
struct EarlyWatch {
    EarlyWatch() {
        char temp[MAX_PATH];
        DWORD n = GetEnvironmentVariableA("TEMP", temp, MAX_PATH);
        std::string where = (n > 0 && n < MAX_PATH) ? std::string(temp) + "\\RStudioGui-fault.log"
                                                    : std::string("RStudioGui-fault.log");
        rstudio_watch_for_faults(where.c_str());
    }
} earlyWatch;
}
#endif

#ifdef _WIN32

static void undoRecording(void* windowHandle, long how) {
    HWND window = reinterpret_cast<HWND>(windowHandle);
    if (!window) return;

    IRichEditOle* ole = 0;
    SendMessage(window, EM_GETOLEINTERFACE, 0, reinterpret_cast<LPARAM>(&ole));
    if (!ole) return;

    ITextDocument* document = 0;
    if (SUCCEEDED(ole->QueryInterface(__uuidof(ITextDocument),
                                      reinterpret_cast<void**>(&document))) &&
        document) {
        document->Undo(how, 0);
        document->Release();
    }
    ole->Release();
}
#endif

const char* rstudio_settings_set_aside(void) {
    scratch() = editor::settings::setAside();
    return scratch().c_str();
}

const char* rstudio_code_font(void) {
    scratch() = editor::settings::codeFont();
    return scratch().c_str();
}

int rstudio_remember_code_font(const char* described) {
    return editor::settings::rememberCodeFont(described ? described : "") ? 1 : 0;
}

void rstudio_undo_suspend(void* windowHandle) {
#ifdef _WIN32
    undoRecording(windowHandle, tomSuspend);
#else
    (void)windowHandle;
#endif
}

void rstudio_undo_resume(void* windowHandle) {
#ifdef _WIN32
    undoRecording(windowHandle, tomResume);
#else
    (void)windowHandle;
#endif
}

char* rstudio_reindent(const char* text, int width, int tabs, int caseIndent, int dialect) {
    return give(join(editor::reindent(split(text),
                                      styleOf(width, tabs, caseIndent, dialect))));
}

char* rstudio_indent_after_newline(const char* text, int row, int col,
                               int width, int tabs, int caseIndent, int dialect) {
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    return give(editor::indentAfterNewline(split(text), static_cast<size_t>(row),
                                           static_cast<size_t>(col),
                                           styleOf(width, tabs, caseIndent, dialect)));
}

char* rstudio_indent_for(const char* text, int row, int width, int tabs, int caseIndent,
                     int dialect) {
    if (row < 0) row = 0;
    std::vector<std::string> lines = split(text);
    return give(editor::indentFor(lines, static_cast<size_t>(row),
                                  styleOf(width, tabs, caseIndent, dialect)));
}

void rstudio_free(char* what) { std::free(what); }

char* rstudio_about(void) { return give(join(editor::about::lines())); }

char* rstudio_describe_build(const char* assembly) {
    return give(join(editor::describe(editor::symbolsIn(split(assembly)))));
}

char* rstudio_debug_note(int kind, const char* arch) {
    return give(join(editor::debugNote(static_cast<editor::ToolchainKind>(kind),
                                       arch ? arch : "")));
}

int rstudio_find_next(const char* text, const char* needle, int row, int col,
                  int* foundRow, int* foundCol) {
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    editor::Match match = editor::findNext(split(text), needle ? needle : "",
                                           static_cast<size_t>(row),
                                           static_cast<size_t>(col));
    if (!match.found) return 0;
    if (foundRow) *foundRow = static_cast<int>(match.row);
    if (foundCol) *foundCol = static_cast<int>(match.col);
    return 1;
}

int rstudio_find_previous(const char* text, const char* needle, int row, int col,
                      int* foundRow, int* foundCol) {
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    editor::Match match = editor::findPrevious(split(text), needle ? needle : "",
                                               static_cast<size_t>(row),
                                               static_cast<size_t>(col));
    if (!match.found) return 0;
    if (foundRow) *foundRow = static_cast<int>(match.row);
    if (foundCol) *foundCol = static_cast<int>(match.col);
    return 1;
}

char* rstudio_replace_all(const char* text, const char* needle, const char* with,
                      int* howMany) {
    std::vector<std::string> lines = split(text);
    size_t count = editor::replaceAll(lines, needle ? needle : "", with ? with : "");
    if (howMany) *howMany = static_cast<int>(count);
    return give(join(lines));
}

int rstudio_language_for(const char* path) {
    return static_cast<int>(editor::languageFor(path ? path : ""));
}

int rstudio_dialect_for(int language) {
    return language == RSTUDIO_LANG_SHALIMAR ? RSTUDIO_DIALECT_SHALIMAR : RSTUDIO_DIALECT_C;
}

int rstudio_highlight(const char* line, int language, int* state,
                  unsigned char* kinds, int kindsSize) {
    editor::SyntaxState carried;
    if (state) {
        carried.comment = (*state & 1) != 0;
        carried.string = (*state & 2) != 0;
    }

    std::vector<unsigned char> worked =
        editor::highlight(line ? line : "", static_cast<editor::Language>(language), carried);

    if (state) *state = (carried.comment ? 1 : 0) | (carried.string ? 2 : 0);

    int n = static_cast<int>(worked.size());
    if (n > kindsSize) n = kindsSize;
    for (int i = 0; i < n; ++i) kinds[i] = worked[static_cast<size_t>(i)];
    return n;
}

RStudioProject* rstudio_project_new(void) { return new RStudioProject(); }
void rstudio_project_free(RStudioProject* project) { delete project; }

int rstudio_project_load(RStudioProject* project, const char* directory,
                     char* error, int errorSize) {
    std::string why;
    bool loaded = project->project.load(directory ? directory : ".", why);
    if (error && errorSize > 0) {
        std::strncpy(error, why.c_str(), static_cast<size_t>(errorSize) - 1);
        error[errorSize - 1] = '\0';
    }
    return loaded ? 1 : 0;
}

const char* rstudio_project_name(RStudioProject* project) {
    project->answer = project->project.name();
    return project->answer.c_str();
}

int rstudio_project_groups(RStudioProject* project) {
    return static_cast<int>(project->project.groups().size());
}

const char* rstudio_project_group_name(RStudioProject* project, int group) {
    if (group < 0 || group >= rstudio_project_groups(project)) return "";
    project->answer = project->project.groups()[static_cast<size_t>(group)].name;
    return project->answer.c_str();
}

int rstudio_project_files(RStudioProject* project, int group) {
    if (group < 0 || group >= rstudio_project_groups(project)) return 0;
    return static_cast<int>(
        project->project.groups()[static_cast<size_t>(group)].files.size());
}

const char* rstudio_project_file(RStudioProject* project, int group, int file) {
    if (file < 0 || file >= rstudio_project_files(project, group)) return "";
    project->answer =
        project->project.groups()[static_cast<size_t>(group)].files[static_cast<size_t>(file)];
    return project->answer.c_str();
}

const char* rstudio_project_absolute(RStudioProject* project, const char* relative) {
    project->answer = project->project.absolute(relative ? relative : "");
    return project->answer.c_str();
}

int rstudio_project_indent_width(RStudioProject* project) {
    return static_cast<int>(project->project.indent().width);
}
int rstudio_project_indent_tabs(RStudioProject* project) {
    return project->project.indent().tabs ? 1 : 0;
}
int rstudio_project_case_indent(RStudioProject* project) {
    return static_cast<int>(project->project.indent().caseIndent);
}
int rstudio_project_toolchain(RStudioProject* project) {
    return static_cast<int>(project->project.toolchain());
}
int rstudio_configuration(void) {
    return editor::settings::configuration() == "release" ? RSTUDIO_CONFIG_RELEASE : 0;
}

void rstudio_remember_configuration(int config) {
    editor::settings::rememberConfiguration(config == RSTUDIO_CONFIG_RELEASE ? "release" : "debug");
}
const char* rstudio_project_arch(RStudioProject* project) {
    project->answer = project->project.arch();
    return project->answer.c_str();
}

int rstudio_project_allows(const char* relative, char* why, int whySize) {
    std::string reason;
    bool fine = editor::Project::allows(relative ? relative : "", reason);
    if (why && whySize > 0) {
        std::strncpy(why, reason.c_str(), static_cast<size_t>(whySize) - 1);
        why[whySize - 1] = '\0';
    }
    return fine ? 1 : 0;
}

int rstudio_project_loaded(RStudioProject* project) { return project->project.loaded() ? 1 : 0; }

const char* rstudio_project_root(RStudioProject* project) {
    project->answer = project->project.root();
    return project->answer.c_str();
}

void rstudio_project_set_root(RStudioProject* project, const char* path) {
    project->project.setRoot(path ? path : ".");
}

void rstudio_project_close(RStudioProject* project) { project->project.close(); }

const char* rstudio_project_suffix(void) { return editor::Project::suffix(); }

const char* rstudio_version(void) { return editor::about::version(); }

int rstudio_project_save_as(RStudioProject* project, const char* file,
                            char* why, int whySize) {
    std::string error;
    bool ok = project->project.saveAs(file ? file : "", error);
    if (!ok && why && whySize > 0) {
        std::string said = error.empty() ? std::string("could not save the project") : error;
        std::strncpy(why, said.c_str(), static_cast<size_t>(whySize) - 1);
        why[whySize - 1] = '\0';
    }
    return ok ? 1 : 0;
}

const char* rstudio_group_for_file(const char* name) {
    scratch() = editor::groupForFile(name ? name : "");
    return scratch().c_str();
}

const char* rstudio_project_relative(RStudioProject* project, const char* path) {
    project->answer = project->project.relative(path ? path : "");
    return project->answer.c_str();
}

const char* rstudio_project_file_name(void) { return editor::Project::fileName(); }

const char* rstudio_outcome_message(RStudioProject* project) {
    return project->last.message.c_str();
}

const char* rstudio_outcome_path(RStudioProject* project) { return project->last.path.c_str(); }

int rstudio_create_file(RStudioProject* project, const char* relative, const char* group) {
    project->last = editor::createFile(project->project, relative ? relative : "",
                                       group ? group : "");
    return project->last.ok ? 1 : 0;
}

int rstudio_rename_file(RStudioProject* project, const char* fromAbsolute, const char* toRelative) {
    project->last = editor::renameFile(project->project, fromAbsolute ? fromAbsolute : "",
                                       toRelative ? toRelative : "");
    return project->last.ok ? 1 : 0;
}

int rstudio_delete_file(RStudioProject* project, const char* absolute) {
    project->last = editor::deleteFile(project->project, absolute ? absolute : "");
    return project->last.ok ? 1 : 0;
}

int rstudio_move_to_group(RStudioProject* project, const char* absolute, const char* group) {
    project->last = editor::moveToGroup(project->project, absolute ? absolute : "",
                                        group ? group : "");
    return project->last.ok ? 1 : 0;
}

int rstudio_add_existing(RStudioProject* project, const char* absolute, const char* group) {
    project->last = editor::addExisting(project->project, absolute ? absolute : "",
                                        group ? group : "");
    return project->last.ok ? 1 : 0;
}

int rstudio_adopt_saved(RStudioProject* project, const char* absolute) {
    if (!project) return 0;
    editor::Outcome joined = editor::adoptSaved(project->project, absolute ? absolute : "");
    if (joined.ok) project->last = joined;
    return joined.ok ? 1 : 0;
}

int rstudio_project_holds(RStudioProject* project, const char* absolute) {
    if (!project || !absolute || !*absolute || !project->project.loaded()) return 0;
    std::string relative = project->project.relative(absolute);
    return project->project.groupOf(relative) < project->project.groups().size() ? 1 : 0;
}

const char* rstudio_project_file_to_open(RStudioProject* project) {
    scratch() = project ? project->project.fileToOpen() : std::string();
    return scratch().c_str();
}

int rstudio_remember_open(RStudioProject* project, const char* absolute) {
    if (!project) return 0;
    return editor::rememberOpen(project->project, absolute ? absolute : "").ok ? 1 : 0;
}

int rstudio_remove_from_project(RStudioProject* project, const char* absolute) {
    project->last = editor::removeExisting(project->project, absolute ? absolute : "");
    return project->last.ok ? 1 : 0;
}

int rstudio_begin_project(RStudioProject* project, const char* directory, const char* name,
                      const char* firstFile) {
    project->last = editor::beginProject(project->project, directory ? directory : ".",
                                         name ? name : "Project",
                                         firstFile ? firstFile : "");
    return project->last.ok ? 1 : 0;
}


const char* rstudio_project_includes(RStudioProject* project) {
    scratch() = project ? joinedList(project->project.includes()) : std::string();
    return scratch().c_str();
}

const char* rstudio_project_libraries(RStudioProject* project) {
    scratch() = project ? joinedList(project->project.libraries()) : std::string();
    return scratch().c_str();
}

int rstudio_project_set_includes(RStudioProject* project, const char* line) {
    if (!project) return 0;
    project->project.setIncludes(splitList(line));
    project->last = editor::saveProject(project->project);
    return project->last.ok ? 1 : 0;
}

int rstudio_project_set_libraries(RStudioProject* project, const char* line) {
    if (!project) return 0;
    project->project.setLibraries(splitList(line));
    project->last = editor::saveProject(project->project);
    return project->last.ok ? 1 : 0;
}

const char* rstudio_install_file(void) {
    scratch() = editor::settings::installFile();
    return scratch().c_str();
}

const char* rstudio_include_dir(void) {
    scratch() = editor::settings::includeDir();
    return scratch().c_str();
}

const char* rstudio_lib_dir(void) {
    scratch() = editor::settings::libDir();
    return scratch().c_str();
}

int rstudio_remember_header_dirs(const char* include, const char* lib) {
    return editor::settings::rememberHeaderDirs(include ? include : "", lib ? lib : "") ? 1 : 0;
}

const char* rstudio_vcvars(void) {
    scratch() = editor::settings::vcvars();
    return scratch().c_str();
}

int rstudio_remember_vcvars(const char* file) {
    return editor::settings::rememberVcvars(file ? file : "") ? 1 : 0;
}

int rstudio_save_project(RStudioProject* project) {
    project->last = editor::saveProject(project->project);
    return project->last.ok ? 1 : 0;
}

const char* rstudio_arch(int index) {
    if (index < 0 || index >= static_cast<int>(editor::kArchCount)) index = 0;
    return editor::kArches[index];
}

int rstudio_arch_count(void) { return static_cast<int>(editor::kArchCount); }

const char* rstudio_toolchain_name(int kind) {
    return editor::toolchainName(static_cast<editor::ToolchainKind>(kind));
}

const char* rstudio_language_name(int language) {
    return editor::languageName(static_cast<editor::Language>(language));
}

const char* rstudio_config_name(int config) {
    return editor::configName(static_cast<editor::Configuration>(config));
}

int rstudio_resolve(int toolchainKind, int language) {
    editor::Toolchain tool;
    tool.kind = static_cast<editor::ToolchainKind>(toolchainKind);
    return static_cast<int>(editor::resolve(tool, static_cast<editor::Language>(language)));
}

int rstudio_can_compile(int kind, int language) {
    return editor::canCompile(static_cast<editor::ToolchainKind>(kind),
                              static_cast<editor::Language>(language))
               ? 1 : 0;
}

const char* rstudio_refusal(int kind, int language) {
    scratch() = editor::refusal(static_cast<editor::ToolchainKind>(kind),
                                static_cast<editor::Language>(language));
    return scratch().c_str();
}

int rstudio_uses_arch(int kind) {
    return editor::usesArch(static_cast<editor::ToolchainKind>(kind)) ? 1 : 0;
}

const char* rstudio_shown_command(RStudioProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind,
                              const char* source, int language, const char* arch,
                              int config) {
    editor::Toolchain tool = toolFrom(project, cc1, cl, shc, cxx1);

    scratch() = editor::shownCommand(tool, static_cast<editor::ToolchainKind>(kind),
                                     source ? source : "",
                                     static_cast<editor::Language>(language),
                                     arch ? arch : "",
                                     static_cast<editor::Configuration>(config));
    return scratch().c_str();
}

int rstudio_runs_here(int kind, const char* arch) {
    return editor::runsHere(static_cast<editor::ToolchainKind>(kind), arch ? arch : "") ? 1 : 0;
}

const char* rstudio_why_not_run(int kind, const char* arch) {
    scratch() = editor::whyNotRun(static_cast<editor::ToolchainKind>(kind), arch ? arch : "");
    return scratch().c_str();
}

const char* rstudio_host_arch(void) { return editor::hostArch(); }

const char* rstudio_shown_run_command(RStudioProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind,
                                  const char* source, int language, const char* arch,
                                  int config) {
    editor::Toolchain tool = toolFrom(project, cc1, cl, shc, cxx1);

    scratch() = editor::shownProgramCommand(tool, static_cast<editor::ToolchainKind>(kind),
                                            source ? source : "",
                                            static_cast<editor::Language>(language),
                                            arch ? arch : "",
                                            static_cast<editor::Configuration>(config));
    return scratch().c_str();
}

RStudioRan* rstudio_run(RStudioProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                int language, const char* arch, int config) {
    editor::Toolchain tool = toolFrom(project, cc1, cl, shc, cxx1);

    RStudioRan* out = new RStudioRan();
    out->ran = editor::runProgram(tool, static_cast<editor::ToolchainKind>(kind),
                                  source ? source : "",
                                  static_cast<editor::Language>(language),
                                  arch ? arch : "",
                                  static_cast<editor::Configuration>(config));
    return out;
}

void rstudio_run_free(RStudioRan* ran) { delete ran; }

int rstudio_ran_built(RStudioRan* ran) { return ran->ran.built ? 1 : 0; }
int rstudio_ran_ran(RStudioRan* ran) { return ran->ran.ran ? 1 : 0; }
int rstudio_ran_status(RStudioRan* ran) { return ran->ran.status; }
const char* rstudio_ran_output(RStudioRan* ran) { return ran->ran.output.c_str(); }
int rstudio_ran_has_error(RStudioRan* ran) { return ran->ran.diag.present ? 1 : 0; }
int rstudio_ran_error_line(RStudioRan* ran) { return static_cast<int>(ran->ran.diag.line); }
int rstudio_ran_error_column(RStudioRan* ran) { return static_cast<int>(ran->ran.diag.col); }
const char* rstudio_ran_error_message(RStudioRan* ran) { return ran->ran.diag.message.c_str(); }

RStudioProgram* rstudio_build_program(RStudioProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                              int language, const char* arch, int config) {
    editor::Toolchain tool = toolFrom(project, cc1, cl, shc, cxx1);

    RStudioProgram* out = new RStudioProgram();
    out->built = editor::buildProgram(tool, static_cast<editor::ToolchainKind>(kind),
                                      source ? source : "",
                                      static_cast<editor::Language>(language),
                                      arch ? arch : "",
                                      static_cast<editor::Configuration>(config));
    return out;
}

void rstudio_program_free(RStudioProgram* built) {
    if (!built) return;
    editor::removeProgram(built->built);
    delete built;
}

int rstudio_program_ok(RStudioProgram* built) { return built->built.ok ? 1 : 0; }
const char* rstudio_program_path(RStudioProgram* built) { return built->built.program.c_str(); }
const char* rstudio_program_output(RStudioProgram* built) { return built->built.output.c_str(); }
int rstudio_program_has_error(RStudioProgram* built) { return built->built.diag.present ? 1 : 0; }
int rstudio_program_error_line(RStudioProgram* built) {
    return static_cast<int>(built->built.diag.line);
}
int rstudio_program_error_column(RStudioProgram* built) {
    return static_cast<int>(built->built.diag.col);
}
const char* rstudio_program_error_message(RStudioProgram* built) {
    return built->built.diag.message.c_str();
}

int rstudio_debugger_for(int kind, const char* arch) {
    return static_cast<int>(editor::dbg_for(static_cast<editor::ToolchainKind>(kind),
                                                arch ? arch : ""));
}

const char* rstudio_debugger_name(int kind) {
    return editor::dbg_name(static_cast<editor::DebuggerKind>(kind));
}

const char* rstudio_no_debugger_because(int kind, const char* arch) {
    scratch() = editor::dbg_whyNot(static_cast<editor::ToolchainKind>(kind),
                                          arch ? arch : "");
    return scratch().c_str();
}

int rstudio_debugger_stops_itself(int kind) {
    return editor::dbg_stopsItself(static_cast<editor::ToolchainKind>(kind)) ? 1 : 0;
}

const char* rstudio_release_cannot_stop(int kind) {
    return editor::dbg_stopsItself(static_cast<editor::ToolchainKind>(kind))
               ? shalimar::releaseHasNoSession()
               : "release is built without -g";
}

const char* rstudio_why_it_did_not_start(int kind, const char* arch) {
    if (editor::dbg_stopsItself(static_cast<editor::ToolchainKind>(kind))) {

        scratch() = shalimar::didNotArm();
        return scratch().c_str();
    }
    scratch() = std::string(editor::dbg_name(
                    editor::dbg_for(static_cast<editor::ToolchainKind>(kind),
                                    arch ? arch : ""))) +
                " could not be started - is it installed?";
    return scratch().c_str();
}

RStudioDebugger* rstudio_debugger_new(void) { return new RStudioDebugger(); }
void rstudio_debugger_free(RStudioDebugger* debugger) { delete debugger; }

int rstudio_debugger_start(RStudioDebugger* debugger, int kind, const char* arch,
                       const char* program) {
    debugger->stop = editor::Stop();
    debugger->locals.clear();
    debugger->stack.clear();
    debugger->looking = 0;

    const editor::ToolchainKind tool = static_cast<editor::ToolchainKind>(kind);

    if (editor::dbg_stopsItself(tool))
        return debugger->shm.start(program ? program : "") ? 1 : 0;

    return debugger->debugger.start(editor::dbg_for(tool, arch ? arch : ""),
                                    program ? program : "") ? 1 : 0;
}

int rstudio_debugger_running(RStudioDebugger* debugger) {
    return (debugger->debugger.running() || debugger->shm.running()) ? 1 : 0;
}

int rstudio_debugging_shalimar(RStudioDebugger* debugger) {
    return debugger->shm.running() ? 1 : 0;
}

void rstudio_debugger_stop(RStudioDebugger* debugger) {
    debugger->debugger.stop();
    debugger->shm.stop();
    debugger->stop = editor::Stop();
    debugger->locals.clear();
    debugger->stack.clear();
    debugger->looking = 0;
}

int rstudio_debugger_break(RStudioDebugger* debugger, const char* file, int line) {
    if (line < 1) return 0;
    if (debugger->shm.running())
        return debugger->shm.breakAt(file ? file : "", static_cast<size_t>(line)) ? 1 : 0;
    return debugger->debugger.breakAt(file ? file : "", static_cast<size_t>(line)) ? 1 : 0;
}

int rstudio_debugger_clear(RStudioDebugger* debugger) {
    if (debugger->shm.running()) return debugger->shm.clearBreakpoints() ? 1 : 0;
    return debugger->debugger.clearBreakpoints() ? 1 : 0;
}

namespace {

void afterMoving(RStudioDebugger* debugger, const editor::Stop& stop, bool itself) {
    debugger->stop = stop;
    debugger->locals.clear();
    debugger->stack.clear();
    debugger->looking = 0;
    if (!stop.stopped) return;

    if (itself) {
        debugger->stack = debugger->shm.frames();
        return;
    }

    debugger->locals = debugger->debugger.locals();
    debugger->stack = debugger->debugger.frames();
}
}

void rstudio_debugger_run(RStudioDebugger* debugger) {
    const bool itself = debugger->shm.running();
    afterMoving(debugger, itself ? debugger->shm.run() : debugger->debugger.run(), itself);
}
void rstudio_debugger_resume(RStudioDebugger* debugger) {
    const bool itself = debugger->shm.running();
    afterMoving(debugger, itself ? debugger->shm.resume() : debugger->debugger.resume(), itself);
}
void rstudio_debugger_step_over(RStudioDebugger* debugger) {
    const bool itself = debugger->shm.running();
    afterMoving(debugger, itself ? debugger->shm.stepOver() : debugger->debugger.stepOver(), itself);
}
void rstudio_debugger_step_into(RStudioDebugger* debugger) {
    const bool itself = debugger->shm.running();
    afterMoving(debugger, itself ? debugger->shm.stepInto() : debugger->debugger.stepInto(), itself);
}
void rstudio_debugger_step_out(RStudioDebugger* debugger) {
    const bool itself = debugger->shm.running();
    afterMoving(debugger, itself ? debugger->shm.stepOut() : debugger->debugger.stepOut(), itself);
}

int rstudio_stop_stopped(RStudioDebugger* debugger) { return debugger->stop.stopped ? 1 : 0; }
int rstudio_stop_exited(RStudioDebugger* debugger) { return debugger->stop.exited ? 1 : 0; }
int rstudio_stop_status(RStudioDebugger* debugger) { return debugger->stop.status; }
const char* rstudio_stop_file(RStudioDebugger* debugger) { return debugger->stop.file.c_str(); }
int rstudio_stop_line(RStudioDebugger* debugger) { return static_cast<int>(debugger->stop.line); }
const char* rstudio_stop_function(RStudioDebugger* debugger) { return debugger->stop.function.c_str(); }
const char* rstudio_stop_said(RStudioDebugger* debugger) { return debugger->stop.said.c_str(); }

int rstudio_stop_no_source(RStudioDebugger* debugger) {
    return editor::dbg_stoppedWithNoSource(debugger->stop.said) ? 1 : 0;
}

const char* rstudio_stop_output(RStudioDebugger* debugger) {

    if (debugger->shm.ownsTheStop()) {
        debugger->output = debugger->stop.said;
        return debugger->output.c_str();
    }

    debugger->output = editor::dbg_programOutput(debugger->debugger.kind(), debugger->stop.said);
    return debugger->output.c_str();
}

int rstudio_locals_count(RStudioDebugger* debugger) {
    return static_cast<int>(debugger->locals.size());
}

namespace {
bool holds(RStudioDebugger* debugger, int index) {
    return index >= 0 && static_cast<size_t>(index) < debugger->locals.size();
}
}

const char* rstudio_local_name(RStudioDebugger* debugger, int index) {
    return holds(debugger, index) ? debugger->locals[index].name.c_str() : "";
}
const char* rstudio_local_type(RStudioDebugger* debugger, int index) {
    return holds(debugger, index) ? debugger->locals[index].type.c_str() : "";
}
const char* rstudio_local_value(RStudioDebugger* debugger, int index) {
    return holds(debugger, index) ? debugger->locals[index].value.c_str() : "";
}

int rstudio_stack_count(RStudioDebugger* debugger) {
    return static_cast<int>(debugger->stack.size());
}

namespace {
bool reaches(RStudioDebugger* debugger, int index) {
    return index >= 0 && static_cast<size_t>(index) < debugger->stack.size();
}
}

const char* rstudio_stack_function(RStudioDebugger* debugger, int index) {
    return reaches(debugger, index) ? debugger->stack[index].function.c_str() : "";
}
const char* rstudio_stack_file(RStudioDebugger* debugger, int index) {
    return reaches(debugger, index) ? debugger->stack[index].file.c_str() : "";
}
int rstudio_stack_line(RStudioDebugger* debugger, int index) {
    return reaches(debugger, index) ? static_cast<int>(debugger->stack[index].line) : 0;
}

const char* rstudio_stack_text(RStudioDebugger* debugger, int index) {

    debugger->frameLine =
        reaches(debugger, index)
            ? editor::dbg_frameLine(debugger->stack[index],
                                    static_cast<size_t>(index) == debugger->looking)
            : std::string();
    return debugger->frameLine.c_str();
}

const char* rstudio_local_text(RStudioDebugger* debugger, int index) {
    debugger->variableLine = holds(debugger, index)
                                 ? editor::dbg_variableLine(debugger->locals[index])
                                 : std::string();
    return debugger->variableLine.c_str();
}

int rstudio_locals_on_line(RStudioDebugger* debugger, const char* line) {
    size_t which = editor::dbg_variableOnLine(debugger->locals, line ? line : "");
    return which < debugger->locals.size() ? static_cast<int>(which) : -1;
}

int rstudio_set_variable(RStudioDebugger* debugger, const char* name, const char* value) {
    debugger->complaint.clear();
    if (!debugger->debugger.setVariable(name ? name : "", value ? value : "",
                                        &debugger->complaint))
        return 0;

    debugger->locals = debugger->debugger.locals();
    return 1;
}

const char* rstudio_set_complaint(RStudioDebugger* debugger) { return debugger->complaint.c_str(); }

void rstudio_watch_add(RStudioDebugger* debugger, const char* expression) {
    debugger->debugger.addWatch(expression ? expression : "");
}

int rstudio_watch_count(RStudioDebugger* debugger) {
    return static_cast<int>(debugger->debugger.watches().size());
}

namespace {
bool watched(RStudioDebugger* debugger, int index) {
    return index >= 0 && static_cast<size_t>(index) < debugger->debugger.watches().size();
}
}

const char* rstudio_watch_text(RStudioDebugger* debugger, int index) {
    debugger->watchLine = watched(debugger, index)
                              ? editor::dbg_watchLine(debugger->debugger.watches()[index])
                              : std::string();
    return debugger->watchLine.c_str();
}

const char* rstudio_watch_expression(RStudioDebugger* debugger, int index) {
    return watched(debugger, index)
               ? debugger->debugger.watches()[index].expression.c_str()
               : "";
}

int rstudio_watch_on_line(RStudioDebugger* debugger, const char* line) {
    size_t which = editor::dbg_watchOnLine(debugger->debugger.watches(), line ? line : "");
    return which < debugger->debugger.watches().size() ? static_cast<int>(which) : -1;
}

void rstudio_watch_set(RStudioDebugger* debugger, int index, const char* expression) {
    if (!watched(debugger, index)) return;
    debugger->debugger.setWatch(static_cast<size_t>(index), expression ? expression : "");
}

int rstudio_debugger_look_at(RStudioDebugger* debugger, int which) {
    if (!reaches(debugger, which)) return 0;

    if (debugger->shm.running()) {
        debugger->looking = 0;
        return which == 0 ? 1 : 0;
    }

    if (!debugger->debugger.selectFrame(static_cast<size_t>(which))) return 0;

    debugger->looking = static_cast<size_t>(which);
    debugger->locals = debugger->debugger.locals();
    return 1;
}

const char* rstudio_locals_none_because(RStudioDebugger* debugger) {
    debugger->refusal = debugger->shm.running()
                            ? "  (" + std::string(shalimar::saysWhereOnly()) + ")"
                            : std::string("  (nothing in scope here)");
    return debugger->refusal.c_str();
}

const char* rstudio_cannot_watch(RStudioDebugger* debugger) {

    debugger->refusal = debugger->shm.running()
                            ? std::string(shalimar::saysWhereOnly()) +
                                  " - nothing to watch with"
                            : std::string();
    return debugger->refusal.c_str();
}

const char* rstudio_cannot_walk_stack(RStudioDebugger* debugger) {

    debugger->refusal = debugger->shm.running() ? std::string(shalimar::saysHowDeepOnly())
                                                : std::string();
    return debugger->refusal.c_str();
}

const char* rstudio_stop_line_text(const char* file, int line, const char* function) {

    scratch() = editor::dbg_stopLine(file ? file : "",
                                     static_cast<size_t>(line < 0 ? 0 : line),
                                     function ? function : "");
    return scratch().c_str();
}

int rstudio_looking_at(RStudioDebugger* debugger) {
    return static_cast<int>(debugger->looking);
}

const char* rstudio_looking_text(RStudioDebugger* debugger) {
    debugger->lookingLine =
        (debugger->looking > 0 && debugger->looking < debugger->stack.size())
            ? editor::dbg_lookingAt(debugger->stack[debugger->looking])
            : std::string();
    return debugger->lookingLine.c_str();
}

int rstudio_stack_on_line(RStudioDebugger* debugger, const char* line) {
    size_t which = editor::dbg_frameOnLine(debugger->stack, line ? line : "");
    return which < debugger->stack.size() ? static_cast<int>(which) : -1;
}

int rstudio_begin_from_what_is_there(RStudioProject* project, const char* directory) {
    if (!project) return 0;
    project->last = editor::beginFromWhatIsThere(project->project, directory ? directory : "");
    project->answer = project->last.message;
    return project->last.ok ? 1 : 0;
}

const char* rstudio_last_project(void) {

    scratch() = editor::settings::lastProject();
    return scratch().c_str();
}

const char* rstudio_recent_project(int index) {
    std::vector<std::string> recent = editor::settings::recentProjects();
    scratch() = (index >= 0 && index < static_cast<int>(recent.size())) ? recent[static_cast<size_t>(index)]
                                                                         : std::string();
    return scratch().c_str();
}

int rstudio_remember_project(const char* directory) {
    return editor::settings::rememberProject(directory ? directory : "") ? 1 : 0;
}

const char* rstudio_demo_directory(void) {
    scratch() = editor::demoDirectory();
    return scratch().c_str();
}

int rstudio_project_builds(RStudioProject* project) {
    return project && project->project.builds() ? 1 : 0;
}

int rstudio_project_target_ready(RStudioProject* project) {
    if (!project) return 0;

    project->sources.clear();
    bool ok = project->project.targetParts(project->parts, project->why, &project->detail);
    if (ok)
        for (size_t i = 0; i < project->parts.size(); ++i)
            for (size_t f = 0; f < project->parts[i].sources.size(); ++f)
                project->sources.push_back(project->parts[i].sources[f]);

    project->language = ok && !project->parts.empty()
                            ? static_cast<int>(project->parts[0].lang)
                            : static_cast<int>(editor::LangPlain);
    project->program = ok ? project->project.targetProgram() : std::string();
    return ok ? 1 : 0;
}

int rstudio_project_target_parts(RStudioProject* project) {
    return project ? static_cast<int>(project->parts.size()) : 0;
}

const char* rstudio_project_part_group(RStudioProject* project, int index) {
    if (!project || index < 0 || index >= static_cast<int>(project->parts.size())) return "";
    return project->parts[static_cast<size_t>(index)].group.c_str();
}

int rstudio_project_part_language(RStudioProject* project, int index) {
    if (!project || index < 0 || index >= static_cast<int>(project->parts.size())) return 0;
    return static_cast<int>(project->parts[static_cast<size_t>(index)].lang);
}

int rstudio_project_part_toolchain(RStudioProject* project, int index, const char* cc1,
                               const char* cl, const char* shc, const char* cxx1, int kind) {
    if (!project || index < 0 || index >= static_cast<int>(project->parts.size()))
        return static_cast<int>(editor::ToolAuto);
    editor::Toolchain tool;
    tool.kind = static_cast<editor::ToolchainKind>(kind);
    if (cc1 && *cc1) tool.cc1 = cc1;
    if (cl && *cl) tool.cl = cl;
    if (shc && *shc) tool.shc = shc;
    if (cxx1 && *cxx1) tool.cxx1 = cxx1;
    return static_cast<int>(
        editor::toolchainOf(tool, project->parts[static_cast<size_t>(index)]));
}

const char* rstudio_project_target_why(RStudioProject* project) {
    return project ? project->why.c_str() : "";
}

const char* rstudio_project_target_detail(RStudioProject* project) {
    return project ? project->detail.c_str() : "";
}

int rstudio_project_target_language(RStudioProject* project) {
    return project ? project->language : 0;
}

int rstudio_project_target_sources(RStudioProject* project) {
    return project ? static_cast<int>(project->sources.size()) : 0;
}

const char* rstudio_project_target_source(RStudioProject* project, int index) {
    if (!project || index < 0 || index >= static_cast<int>(project->sources.size())) return "";
    return project->sources[static_cast<size_t>(index)].c_str();
}

const char* rstudio_project_target_program(RStudioProject* project) {
    return project ? project->program.c_str() : "";
}

int rstudio_project_debug_plan(RStudioProject* project, const char* cc1, const char* cl,
                           const char* shc, const char* cxx1, int kind, const char* arch) {
    if (!project) return 0;
    project->plan = editor::DebugPlan();
    project->whyNot.clear();
    if (!rstudio_project_target_ready(project)) {
        project->whyNot = project->why;
        return 0;
    }

    editor::Toolchain tool = toolFrom(project, cc1, cl, shc, cxx1);
    tool.kind = static_cast<editor::ToolchainKind>(kind);

    project->plan = editor::dbg_planFor(tool, project->parts, arch ? arch : "");
    if (project->plan.possible()) return 1;

    project->whyNot = editor::dbg_whyNot(project->plan.kind, arch ? arch : "");
    return 0;
}

int rstudio_project_debug_kind(RStudioProject* project) {
    return project ? static_cast<int>(project->plan.kind) : static_cast<int>(editor::ToolAuto);
}

const char* rstudio_project_why_not_debug(RStudioProject* project) {
    return project ? project->whyNot.c_str() : "";
}

int rstudio_project_blind_groups(RStudioProject* project) {
    return project ? static_cast<int>(project->plan.blind.size()) : 0;
}

const char* rstudio_project_blind_group(RStudioProject* project, int index) {
    if (!project || index < 0 || index >= static_cast<int>(project->plan.blind.size()))
        return "";
    return project->plan.blind[static_cast<size_t>(index)].c_str();
}

RStudioBuild* rstudio_build_target(RStudioProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1,
                           int kind, const char* arch, int config) {
    if (!rstudio_project_target_ready(project)) return 0;

    editor::Toolchain tool = toolFrom(project, cc1, cl, shc, cxx1);

    tool.kind = static_cast<editor::ToolchainKind>(kind);

    RStudioBuild* out = new RStudioBuild();
    editor::Built made = editor::buildParts(
        tool, project->parts, arch ? arch : "",
        static_cast<editor::Configuration>(config), project->program);

    out->built.ok = made.ok;
    out->built.diag = made.diag;
    out->built.output = made.output;
    return out;
}

RStudioRan* rstudio_run_built(const char* program) {
    RStudioRan* out = new RStudioRan();
    out->ran = editor::runBuilt(program ? program : "");
    return out;
}

struct RStudioConversion {
    editor::Conversion made;
};

int rstudio_converts_from(int language, int* toShalimar) {
    bool wanted = false;
    if (!editor::convertsFrom(static_cast<editor::Language>(language), &wanted)) {
        return 0;
    }
    if (toShalimar) *toShalimar = wanted ? 1 : 0;
    return 1;
}

char* rstudio_find_converter(void) {
    return give(editor::findConverter());
}

char* rstudio_converted_name(const char* source, int toShalimar) {
    return give(editor::convertedName(source ? source : "", toShalimar != 0));
}

RStudioConversion* rstudio_convert(const char* converter, const char* source,
                                   const char* output, int toShalimar) {
    RStudioConversion* out = new RStudioConversion();
    out->made = editor::convert(converter ? converter : "", source ? source : "",
                                output ? output : "", toShalimar != 0);
    return out;
}

void rstudio_conversion_free(RStudioConversion* made) { delete made; }
int rstudio_conversion_ran(RStudioConversion* made) { return made->made.ran ? 1 : 0; }
int rstudio_conversion_ok(RStudioConversion* made) { return made->made.ok ? 1 : 0; }
const char* rstudio_conversion_produced(RStudioConversion* made) {
    return made->made.produced.c_str();
}
const char* rstudio_conversion_output(RStudioConversion* made) {
    return made->made.output.c_str();
}

RStudioBuild* rstudio_build(RStudioProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                    int language, const char* arch, int config) {
    editor::Toolchain tool = toolFrom(project, cc1, cl, shc, cxx1);

    RStudioBuild* out = new RStudioBuild();
    out->built = editor::build(tool, static_cast<editor::ToolchainKind>(kind),
                               source ? source : "",
                               static_cast<editor::Language>(language),
                               arch ? arch : "",
                               static_cast<editor::Configuration>(config));
    out->assembly = join(out->built.asmLines);
    return out;
}

void rstudio_build_free(RStudioBuild* built) { delete built; }

int rstudio_build_ok(RStudioBuild* built) { return built->built.ok ? 1 : 0; }
const char* rstudio_build_output(RStudioBuild* built) { return built->built.output.c_str(); }
const char* rstudio_build_assembly(RStudioBuild* built) { return built->assembly.c_str(); }
int rstudio_build_assembly_lines(RStudioBuild* built) {
    return static_cast<int>(built->built.asmLines.size());
}
int rstudio_build_has_error(RStudioBuild* built) { return built->built.diag.present ? 1 : 0; }
const char* rstudio_build_error_file(RStudioBuild* built) {
    return built ? built->built.diag.file.c_str() : "";
}

int rstudio_build_error_line(RStudioBuild* built) {
    return static_cast<int>(built->built.diag.line);
}
int rstudio_build_error_column(RStudioBuild* built) {
    return static_cast<int>(built->built.diag.col);
}
const char* rstudio_build_error_message(RStudioBuild* built) {
    return built->built.diag.message.c_str();
}

}
