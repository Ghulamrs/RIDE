#ifndef RSTUDIO_BRIDGE_H
#define RSTUDIO_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RSTUDIO_KIND_NORMAL = 0, RSTUDIO_KIND_KEYWORD, RSTUDIO_KIND_TYPE, RSTUDIO_KIND_STRING,
    RSTUDIO_KIND_CHAR, RSTUDIO_KIND_COMMENT, RSTUDIO_KIND_PREPROC, RSTUDIO_KIND_NUMBER,
    RSTUDIO_KIND_LABEL
};
enum { RSTUDIO_LANG_PLAIN = 0, RSTUDIO_LANG_C, RSTUDIO_LANG_CPP, RSTUDIO_LANG_SHALIMAR,
       RSTUDIO_LANG_ASM, RSTUDIO_LANG_JSON };

enum { RSTUDIO_DIALECT_C = 0, RSTUDIO_DIALECT_SHALIMAR };
enum { RSTUDIO_TOOL_AUTO = 0, RSTUDIO_TOOL_CC1, RSTUDIO_TOOL_MSVC, RSTUDIO_TOOL_SHC, RSTUDIO_TOOL_CXX,
       RSTUDIO_TOOL_CXX1 };
enum { RSTUDIO_CONFIG_DEBUG = 0, RSTUDIO_CONFIG_RELEASE };

void rstudio_watch_for_faults(const char* logPath);

char* rstudio_reindent(const char* text, int width, int tabs, int caseIndent,
                   int dialect);

char* rstudio_indent_after_newline(const char* text, int row, int col,
                               int width, int tabs, int caseIndent, int dialect);

char* rstudio_indent_for(const char* text, int row, int width, int tabs, int caseIndent,
                     int dialect);

void rstudio_free(char* what);

int rstudio_find_next(const char* text, const char* needle, int row, int col,
                  int* foundRow, int* foundCol);
int rstudio_find_previous(const char* text, const char* needle, int row, int col,
                      int* foundRow, int* foundCol);

char* rstudio_replace_all(const char* text, const char* needle, const char* with,
                      int* howMany);

const char* rstudio_settings_set_aside(void);

const char* rstudio_code_font(void);
int rstudio_remember_code_font(const char* described);

void rstudio_undo_suspend(void* windowHandle);
void rstudio_undo_resume(void* windowHandle);

int rstudio_language_for(const char* path);

int rstudio_dialect_for(int language);

int rstudio_highlight(const char* line, int language, int* state,
                  unsigned char* kinds, int kindsSize);

char* rstudio_describe_build(const char* assembly);

char* rstudio_debug_note(int kind, const char* arch);

typedef struct RStudioProject RStudioProject;

RStudioProject* rstudio_project_new(void);
void rstudio_project_free(RStudioProject* project);

int rstudio_project_load(RStudioProject* project, const char* directory,
                     char* error, int errorSize);

const char* rstudio_project_name(RStudioProject* project);
int rstudio_project_groups(RStudioProject* project);
const char* rstudio_project_group_name(RStudioProject* project, int group);
int rstudio_project_files(RStudioProject* project, int group);
const char* rstudio_project_file(RStudioProject* project, int group, int file);
const char* rstudio_project_absolute(RStudioProject* project, const char* relative);

int rstudio_project_indent_width(RStudioProject* project);
int rstudio_project_indent_tabs(RStudioProject* project);
int rstudio_project_case_indent(RStudioProject* project);
int rstudio_project_toolchain(RStudioProject* project);

int rstudio_configuration(void);
void rstudio_remember_configuration(int config);
const char* rstudio_project_arch(RStudioProject* project);

int rstudio_project_allows(const char* relative, char* why, int whySize);

const char* rstudio_group_for_file(const char* name);

const char* rstudio_project_suffix(void);

int rstudio_project_save_as(RStudioProject* project, const char* file,
                            char* why, int whySize);

int rstudio_project_loaded(RStudioProject* project);
const char* rstudio_project_root(RStudioProject* project);
void rstudio_project_set_root(RStudioProject* project, const char* path);

void rstudio_project_close(RStudioProject* project);
const char* rstudio_project_relative(RStudioProject* project, const char* path);
const char* rstudio_project_file_name(void);

int rstudio_create_file(RStudioProject* project, const char* relative, const char* group);
int rstudio_rename_file(RStudioProject* project, const char* fromAbsolute, const char* toRelative);
int rstudio_delete_file(RStudioProject* project, const char* absolute);
int rstudio_move_to_group(RStudioProject* project, const char* absolute, const char* group);
int rstudio_add_existing(RStudioProject* project, const char* absolute, const char* group);

int rstudio_remove_from_project(RStudioProject* project, const char* absolute);

int rstudio_begin_from_what_is_there(RStudioProject* project, const char* directory);
const char* rstudio_last_project(void);
int rstudio_remember_project(const char* directory);
const char* rstudio_demo_directory(void);

int rstudio_begin_project(RStudioProject* project, const char* directory, const char* name,
                      const char* firstFile);
int rstudio_save_project(RStudioProject* project);

const char* rstudio_outcome_message(RStudioProject* project);
const char* rstudio_outcome_path(RStudioProject* project);

const char* rstudio_arch(int index);
const char* rstudio_toolchain_name(int kind);

const char* rstudio_language_name(int language);
const char* rstudio_config_name(int config);
int rstudio_resolve(int toolchainKind, int language);
int rstudio_can_compile(int kind, int language);
const char* rstudio_refusal(int kind, int language);
int rstudio_uses_arch(int kind);

int rstudio_runs_here(int kind, const char* arch);
const char* rstudio_why_not_run(int kind, const char* arch);
const char* rstudio_host_arch(void);

const char* rstudio_shown_command(const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind,
                              const char* source, int language, const char* arch,
                              int config);

int rstudio_converts_from(int language, int* toShalimar);
char* rstudio_find_converter(void);
char* rstudio_converted_name(const char* source, int toShalimar);

typedef struct RStudioConversion RStudioConversion;

RStudioConversion* rstudio_convert(const char* converter, const char* source,
                                   const char* output, int toShalimar);
void rstudio_conversion_free(RStudioConversion* made);
int rstudio_conversion_ran(RStudioConversion* made);
int rstudio_conversion_ok(RStudioConversion* made);
const char* rstudio_conversion_produced(RStudioConversion* made);
const char* rstudio_conversion_output(RStudioConversion* made);

typedef struct RStudioBuild RStudioBuild;

RStudioBuild* rstudio_build(const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                    int language, const char* arch, int config);

int rstudio_project_builds(RStudioProject* project);
int rstudio_project_target_ready(RStudioProject* project);
const char* rstudio_project_target_why(RStudioProject* project);
const char* rstudio_project_target_detail(RStudioProject* project);
int rstudio_project_target_language(RStudioProject* project);
int rstudio_project_target_sources(RStudioProject* project);

int rstudio_project_target_parts(RStudioProject* project);
const char* rstudio_project_part_group(RStudioProject* project, int index);
int rstudio_project_part_language(RStudioProject* project, int index);
int rstudio_project_part_toolchain(RStudioProject* project, int index, const char* cc1,
                               const char* cl, const char* shc, const char* cxx1, int kind);
const char* rstudio_project_target_source(RStudioProject* project, int index);
const char* rstudio_project_target_program(RStudioProject* project);

int rstudio_project_debug_plan(RStudioProject* project, const char* cc1, const char* cl,
                           const char* shc, const char* cxx1, int kind, const char* arch);
int rstudio_project_debug_kind(RStudioProject* project);
const char* rstudio_project_why_not_debug(RStudioProject* project);
int rstudio_project_blind_groups(RStudioProject* project);
const char* rstudio_project_blind_group(RStudioProject* project, int index);

RStudioBuild* rstudio_build_target(RStudioProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1,
                           int kind, const char* arch, int config);
void rstudio_build_free(RStudioBuild* built);

int rstudio_build_ok(RStudioBuild* built);
const char* rstudio_build_output(RStudioBuild* built);
const char* rstudio_build_assembly(RStudioBuild* built);
int rstudio_build_assembly_lines(RStudioBuild* built);
int rstudio_build_has_error(RStudioBuild* built);

const char* rstudio_build_error_file(RStudioBuild* built);
int rstudio_build_error_line(RStudioBuild* built);
int rstudio_build_error_column(RStudioBuild* built);
const char* rstudio_build_error_message(RStudioBuild* built);

typedef struct RStudioRan RStudioRan;

RStudioRan* rstudio_run(const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                int language, const char* arch, int config);

RStudioRan* rstudio_run_built(const char* program);
void rstudio_run_free(RStudioRan* ran);

int rstudio_ran_built(RStudioRan* ran);
int rstudio_ran_ran(RStudioRan* ran);
int rstudio_ran_status(RStudioRan* ran);
const char* rstudio_ran_output(RStudioRan* ran);
int rstudio_ran_has_error(RStudioRan* ran);
int rstudio_ran_error_line(RStudioRan* ran);
int rstudio_ran_error_column(RStudioRan* ran);
const char* rstudio_ran_error_message(RStudioRan* ran);

const char* rstudio_shown_run_command(const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind,
                                  const char* source, int language, const char* arch,
                                  int config);

char* rstudio_about(void);

typedef struct RStudioProgram RStudioProgram;

RStudioProgram* rstudio_build_program(const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                              int language, const char* arch, int config);
void rstudio_program_free(RStudioProgram* built);

int rstudio_program_ok(RStudioProgram* built);
const char* rstudio_program_path(RStudioProgram* built);
const char* rstudio_program_output(RStudioProgram* built);
int rstudio_program_has_error(RStudioProgram* built);
int rstudio_program_error_line(RStudioProgram* built);
int rstudio_program_error_column(RStudioProgram* built);
const char* rstudio_program_error_message(RStudioProgram* built);

int rstudio_debugger_for(int kind, const char* arch);
const char* rstudio_debugger_name(int kind);
const char* rstudio_no_debugger_because(int kind, const char* arch);

int rstudio_debugger_stops_itself(int kind);

const char* rstudio_release_cannot_stop(int kind);

const char* rstudio_why_it_did_not_start(int kind, const char* arch);

typedef struct RStudioDebugger RStudioDebugger;

RStudioDebugger* rstudio_debugger_new(void);
void rstudio_debugger_free(RStudioDebugger* debugger);

int rstudio_debugger_start(RStudioDebugger* debugger, int kind, const char* arch,
                       const char* program);
int rstudio_debugger_running(RStudioDebugger* debugger);
void rstudio_debugger_stop(RStudioDebugger* debugger);

int rstudio_debugging_shalimar(RStudioDebugger* debugger);

const char* rstudio_locals_none_because(RStudioDebugger* debugger);
const char* rstudio_cannot_watch(RStudioDebugger* debugger);
const char* rstudio_cannot_walk_stack(RStudioDebugger* debugger);

int rstudio_debugger_break(RStudioDebugger* debugger, const char* file, int line);
int rstudio_debugger_clear(RStudioDebugger* debugger);

void rstudio_debugger_run(RStudioDebugger* debugger);
void rstudio_debugger_resume(RStudioDebugger* debugger);
void rstudio_debugger_step_over(RStudioDebugger* debugger);
void rstudio_debugger_step_into(RStudioDebugger* debugger);
void rstudio_debugger_step_out(RStudioDebugger* debugger);

int rstudio_stop_stopped(RStudioDebugger* debugger);
int rstudio_stop_exited(RStudioDebugger* debugger);
int rstudio_stop_status(RStudioDebugger* debugger);
const char* rstudio_stop_file(RStudioDebugger* debugger);
int rstudio_stop_line(RStudioDebugger* debugger);
const char* rstudio_stop_function(RStudioDebugger* debugger);

const char* rstudio_stop_said(RStudioDebugger* debugger);

const char* rstudio_stop_output(RStudioDebugger* debugger);

int rstudio_stop_no_source(RStudioDebugger* debugger);

int rstudio_locals_count(RStudioDebugger* debugger);
const char* rstudio_local_name(RStudioDebugger* debugger, int index);
const char* rstudio_local_type(RStudioDebugger* debugger, int index);
const char* rstudio_local_value(RStudioDebugger* debugger, int index);

const char* rstudio_local_text(RStudioDebugger* debugger, int index);
int rstudio_locals_on_line(RStudioDebugger* debugger, const char* line);
int rstudio_set_variable(RStudioDebugger* debugger, const char* name, const char* value);
const char* rstudio_set_complaint(RStudioDebugger* debugger);

void rstudio_watch_add(RStudioDebugger* debugger, const char* expression);
int rstudio_watch_count(RStudioDebugger* debugger);
const char* rstudio_watch_text(RStudioDebugger* debugger, int index);
const char* rstudio_watch_expression(RStudioDebugger* debugger, int index);
int rstudio_watch_on_line(RStudioDebugger* debugger, const char* line);
void rstudio_watch_set(RStudioDebugger* debugger, int index, const char* expression);

int rstudio_stack_count(RStudioDebugger* debugger);
const char* rstudio_stack_function(RStudioDebugger* debugger, int index);
const char* rstudio_stack_file(RStudioDebugger* debugger, int index);
int rstudio_stack_line(RStudioDebugger* debugger, int index);

const char* rstudio_stack_text(RStudioDebugger* debugger, int index);
int rstudio_stack_on_line(RStudioDebugger* debugger, const char* line);

int rstudio_debugger_look_at(RStudioDebugger* debugger, int which);

const char* rstudio_stop_line_text(const char* file, int line, const char* function);
int rstudio_looking_at(RStudioDebugger* debugger);
const char* rstudio_looking_text(RStudioDebugger* debugger);

#ifdef __cplusplus
}
#endif

#endif
