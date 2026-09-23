#ifndef RIDE_BRIDGE_H
#define RIDE_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    RIDE_KIND_NORMAL = 0, RIDE_KIND_KEYWORD, RIDE_KIND_TYPE, RIDE_KIND_STRING,
    RIDE_KIND_CHAR, RIDE_KIND_COMMENT, RIDE_KIND_PREPROC, RIDE_KIND_NUMBER,
    RIDE_KIND_LABEL
};
enum { RIDE_LANG_PLAIN = 0, RIDE_LANG_C, RIDE_LANG_CPP, RIDE_LANG_SHALIMAR,
       RIDE_LANG_ASM, RIDE_LANG_JSON };

enum { RIDE_DIALECT_C = 0, RIDE_DIALECT_SHALIMAR };
enum { RIDE_TOOL_AUTO = 0, RIDE_TOOL_CC1, RIDE_TOOL_MSVC, RIDE_TOOL_SHC, RIDE_TOOL_CXX,
       RIDE_TOOL_CXX1 };
enum { RIDE_CONFIG_DEBUG = 0, RIDE_CONFIG_RELEASE };

void ride_watch_for_faults(const char* logPath);

char* ride_reindent(const char* text, int width, int tabs, int caseIndent,
                   int dialect);

char* ride_indent_after_newline(const char* text, int row, int col,
                               int width, int tabs, int caseIndent, int dialect);

char* ride_indent_for(const char* text, int row, int width, int tabs, int caseIndent,
                     int dialect);

void ride_free(char* what);

int ride_find_next(const char* text, const char* needle, int row, int col,
                  int* foundRow, int* foundCol);
int ride_find_previous(const char* text, const char* needle, int row, int col,
                      int* foundRow, int* foundCol);

char* ride_replace_all(const char* text, const char* needle, const char* with,
                      int* howMany);

const char* ride_settings_set_aside(void);

const char* ride_code_font(void);
int ride_remember_code_font(const char* described);

void ride_undo_suspend(void* windowHandle);
void ride_undo_resume(void* windowHandle);

int ride_language_for(const char* path);

int ride_dialect_for(int language);

int ride_highlight(const char* line, int language, int* state,
                  unsigned char* kinds, int kindsSize);

char* ride_describe_build(const char* assembly);

char* ride_debug_note(int kind, const char* arch);

typedef struct RIDEProject RIDEProject;

RIDEProject* ride_project_new(void);
void ride_project_free(RIDEProject* project);

int ride_project_load(RIDEProject* project, const char* directory,
                     char* error, int errorSize);

const char* ride_project_name(RIDEProject* project);
int ride_project_groups(RIDEProject* project);
const char* ride_project_group_name(RIDEProject* project, int group);
int ride_project_files(RIDEProject* project, int group);
const char* ride_project_file(RIDEProject* project, int group, int file);
const char* ride_project_absolute(RIDEProject* project, const char* relative);

int ride_project_indent_width(RIDEProject* project);
int ride_project_indent_tabs(RIDEProject* project);
int ride_project_case_indent(RIDEProject* project);
int ride_project_toolchain(RIDEProject* project);

int ride_configuration(void);
void ride_remember_configuration(int config);
const char* ride_project_arch(RIDEProject* project);
/* Run file on one source of a several-source build: the count of sources it
   is one of, or 0 - then the window runs the project instead. */
int ride_project_runs_as_project(RIDEProject* project, const char* source);
// The project's own target and compiler, set from the Target and Tools
// menus while it is open and written to its .pro at once; 0 with no project.
int ride_project_set_arch(RIDEProject* project, const char* arch);
int ride_project_set_toolchain(RIDEProject* project, int kind);

int ride_project_allows(const char* relative, char* why, int whySize);

const char* ride_group_for_file(const char* name);

const char* ride_project_suffix(void);
// The product's name, from product.h: the window's title, its message boxes, its logs.
const char* ride_product_name(void);
const char* ride_version(void);

// A project's header directories and libraries, as one ';'-separated line
// each, relative to the root as the file has them; setting one saves.
const char* ride_project_includes(RIDEProject* project);
const char* ride_project_libraries(RIDEProject* project);
int ride_project_set_includes(RIDEProject* project, const char* line);
int ride_project_set_libraries(RIDEProject* project, const char* line);

// The installation's settings.json, above bin/: where cxx1's headers
// (include) and cc1's (lib) are, and the vcvars64.bat named there if any.
const char* ride_install_file(void);
// The installation's include directories and libraries, one ';'-separated
// line each as settings.json has them; setting one writes the file.
const char* ride_includes(void);
const char* ride_libraries(void);
int ride_set_includes(const char* line);
/* The window's way of asking the build's question - "the project's own
   masm/link/lnk6x did not build it; use the native tools instead?" - a yes
   builds again through them (compile.h: setAskNative). Installed once, at
   start; the function is called on whichever thread is building. */
void ride_ask_native(int (*ask)(const char* question));
int ride_set_libraries(const char* line);
const char* ride_include_dir(void);
const char* ride_lib_dir(void);
int ride_remember_header_dirs(const char* include, const char* lib);
const char* ride_vcvars(void);
// The default compiler in settings.json, as an RIDE_TOOL_* number, and
// the choice written back when one is made from the menu.
int ride_default_compiler(void);
int ride_default_indent_width(void);
int ride_default_indent_tabs(void);
int ride_remember_default_compiler(int kind);
int ride_remember_vcvars(const char* file);
// The assembler named for x86_64-windows, and the choice written back.
const char* ride_assembler(void);
int ride_remember_assembler(const char* file);
// TI's C6000 compiler directory and the runtime directory beside it, for a
// tms6747 build's .out, and the choice written back.
// The linkers named for x86_64-windows and tms6747, and the choices written back.
const char* ride_linker(void);
int ride_remember_linker(const char* file);
const char* ride_tilinker(void);
int ride_remember_tilinker(const char* file);
const char* ride_ti(void);
const char* ride_tilib(void);
int ride_remember_ti(const char* dir, const char* lib);

int ride_project_save_as(RIDEProject* project, const char* file,
                            char* why, int whySize);

int ride_project_loaded(RIDEProject* project);
const char* ride_project_root(RIDEProject* project);
void ride_project_set_root(RIDEProject* project, const char* path);

void ride_project_close(RIDEProject* project);
const char* ride_project_relative(RIDEProject* project, const char* path);

// kind is the compiler chosen (RIDE_TOOL_*): a name with no extension
// gets that compiler's, or with the choice on automatic the project's usual.
int ride_create_file(RIDEProject* project, const char* relative, const char* group, int kind);
int ride_rename_file(RIDEProject* project, const char* fromAbsolute, const char* toRelative);
int ride_delete_file(RIDEProject* project, const char* absolute);
int ride_move_to_group(RIDEProject* project, const char* absolute, const char* group);
int ride_add_existing(RIDEProject* project, const char* absolute, const char* group);
// A file just saved under the project's root joins it; 0 when that did not
// apply, and nothing to say then.
int ride_adopt_saved(RIDEProject* project, const char* absolute);
// Whether the project lists this file, in any group.
int ride_project_holds(RIDEProject* project, const char* absolute);
// The file the project opens with, relative to its root - its own choice,
// else the one defining main, else the first - and remembering the one in
// front when the project is closed or the window left.
const char* ride_project_file_to_open(RIDEProject* project);
int ride_remember_open(RIDEProject* project, const char* absolute);

int ride_remove_from_project(RIDEProject* project, const char* absolute);

int ride_begin_from_what_is_there(RIDEProject* project, const char* directory);
const char* ride_last_project(void);
int ride_remember_project(const char* directory);
// The last three projects opened, most recent first; empty past the end.
const char* ride_recent_project(int index);
const char* ride_recent_file(int index);
int ride_remember_file(const char* path);
const char* ride_demo_directory(void);

int ride_begin_project(RIDEProject* project, const char* directory, const char* name,
                      const char* firstFile);
int ride_save_project(RIDEProject* project);

const char* ride_outcome_message(RIDEProject* project);
const char* ride_outcome_path(RIDEProject* project);

const char* ride_arch(int index);
int ride_arch_count(void);
const char* ride_toolchain_name(int kind);

const char* ride_language_name(int language);
const char* ride_config_name(int config);
int ride_resolve(int toolchainKind, int language);
int ride_can_compile(int kind, int language);
const char* ride_refusal(int kind, int language);
int ride_uses_arch(int kind);

int ride_runs_here(int kind, const char* arch);
const char* ride_why_not_run(int kind, const char* arch);
const char* ride_host_arch(void);

const char* ride_shown_command(RIDEProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind,
                              const char* source, int language, const char* arch,
                              int config);

int ride_converts_from(int language, int* toShalimar);
char* ride_find_converter(void);
char* ride_converted_name(const char* source, int toShalimar);

typedef struct RIDEConversion RIDEConversion;

RIDEConversion* ride_convert(const char* converter, const char* source,
                                   const char* output, int toShalimar);
void ride_conversion_free(RIDEConversion* made);
int ride_conversion_ran(RIDEConversion* made);
int ride_conversion_ok(RIDEConversion* made);
const char* ride_conversion_produced(RIDEConversion* made);
const char* ride_conversion_output(RIDEConversion* made);

typedef struct RIDEBuild RIDEBuild;

RIDEBuild* ride_build(RIDEProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                    int language, const char* arch, int config);

int ride_project_builds(RIDEProject* project);
int ride_project_target_ready(RIDEProject* project);
const char* ride_project_target_why(RIDEProject* project);
const char* ride_project_target_detail(RIDEProject* project);
int ride_project_target_language(RIDEProject* project);
int ride_project_target_sources(RIDEProject* project);

int ride_project_target_parts(RIDEProject* project);
const char* ride_project_part_group(RIDEProject* project, int index);
int ride_project_part_language(RIDEProject* project, int index);
int ride_project_part_toolchain(RIDEProject* project, int index, const char* cc1,
                               const char* cl, const char* shc, const char* cxx1, int kind);
const char* ride_project_target_source(RIDEProject* project, int index);
const char* ride_project_target_program(RIDEProject* project);

int ride_project_debug_plan(RIDEProject* project, const char* cc1, const char* cl,
                           const char* shc, const char* cxx1, int kind, const char* arch);
int ride_project_debug_kind(RIDEProject* project);
const char* ride_project_why_not_debug(RIDEProject* project);
int ride_project_blind_groups(RIDEProject* project);
const char* ride_project_blind_group(RIDEProject* project, int index);

RIDEBuild* ride_build_target(RIDEProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1,
                           int kind, const char* arch, int config);
void ride_build_free(RIDEBuild* built);

int ride_build_ok(RIDEBuild* built);
const char* ride_build_output(RIDEBuild* built);
const char* ride_build_assembly(RIDEBuild* built);
int ride_build_assembly_lines(RIDEBuild* built);
int ride_build_has_error(RIDEBuild* built);

const char* ride_build_error_file(RIDEBuild* built);
int ride_build_error_line(RIDEBuild* built);
int ride_build_error_column(RIDEBuild* built);
const char* ride_build_error_message(RIDEBuild* built);

typedef struct RIDERan RIDERan;

RIDERan* ride_run(RIDEProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                int language, const char* arch, int config);

RIDERan* ride_run_built(const char* program);
void ride_run_free(RIDERan* ran);

int ride_ran_built(RIDERan* ran);
int ride_ran_ran(RIDERan* ran);
int ride_ran_status(RIDERan* ran);
const char* ride_ran_output(RIDERan* ran);
int ride_ran_has_error(RIDERan* ran);
int ride_ran_error_line(RIDERan* ran);
int ride_ran_error_column(RIDERan* ran);
const char* ride_ran_error_message(RIDERan* ran);

const char* ride_shown_run_command(RIDEProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind,
                                  const char* source, int language, const char* arch,
                                  int config);

char* ride_about(void);

typedef struct RIDEProgram RIDEProgram;

RIDEProgram* ride_build_program(RIDEProject* project, const char* cc1, const char* cl, const char* shc, const char* cxx1, int kind, const char* source,
                              int language, const char* arch, int config);
void ride_program_free(RIDEProgram* built);

int ride_program_ok(RIDEProgram* built);
const char* ride_program_path(RIDEProgram* built);
const char* ride_program_output(RIDEProgram* built);
int ride_program_has_error(RIDEProgram* built);
int ride_program_error_line(RIDEProgram* built);
int ride_program_error_column(RIDEProgram* built);
const char* ride_program_error_message(RIDEProgram* built);

int ride_debugger_for(int kind, const char* arch);
const char* ride_debugger_name(int kind);
const char* ride_no_debugger_because(int kind, const char* arch);

int ride_debugger_stops_itself(int kind);

const char* ride_release_cannot_stop(int kind);

const char* ride_why_it_did_not_start(int kind, const char* arch);

typedef struct RIDEDebugger RIDEDebugger;

RIDEDebugger* ride_debugger_new(void);
void ride_debugger_free(RIDEDebugger* debugger);

int ride_debugger_start(RIDEDebugger* debugger, int kind, const char* arch,
                       const char* program);
int ride_debugger_running(RIDEDebugger* debugger);
void ride_debugger_stop(RIDEDebugger* debugger);

int ride_debugging_shalimar(RIDEDebugger* debugger);

const char* ride_locals_none_because(RIDEDebugger* debugger);
const char* ride_cannot_watch(RIDEDebugger* debugger);
const char* ride_cannot_walk_stack(RIDEDebugger* debugger);

int ride_debugger_break(RIDEDebugger* debugger, const char* file, int line);
int ride_debugger_clear(RIDEDebugger* debugger);

void ride_debugger_run(RIDEDebugger* debugger);
void ride_debugger_resume(RIDEDebugger* debugger);
void ride_debugger_step_over(RIDEDebugger* debugger);
void ride_debugger_step_into(RIDEDebugger* debugger);
void ride_debugger_step_out(RIDEDebugger* debugger);

int ride_stop_stopped(RIDEDebugger* debugger);
int ride_stop_exited(RIDEDebugger* debugger);
int ride_stop_status(RIDEDebugger* debugger);
const char* ride_stop_file(RIDEDebugger* debugger);
int ride_stop_line(RIDEDebugger* debugger);
const char* ride_stop_function(RIDEDebugger* debugger);

const char* ride_stop_said(RIDEDebugger* debugger);

const char* ride_stop_output(RIDEDebugger* debugger);

int ride_stop_no_source(RIDEDebugger* debugger);

int ride_locals_count(RIDEDebugger* debugger);
const char* ride_local_name(RIDEDebugger* debugger, int index);
const char* ride_local_type(RIDEDebugger* debugger, int index);
const char* ride_local_value(RIDEDebugger* debugger, int index);

const char* ride_local_text(RIDEDebugger* debugger, int index);
int ride_locals_on_line(RIDEDebugger* debugger, const char* line);
int ride_set_variable(RIDEDebugger* debugger, const char* name, const char* value);
const char* ride_set_complaint(RIDEDebugger* debugger);

void ride_watch_add(RIDEDebugger* debugger, const char* expression);
int ride_watch_count(RIDEDebugger* debugger);
const char* ride_watch_text(RIDEDebugger* debugger, int index);
const char* ride_watch_expression(RIDEDebugger* debugger, int index);
int ride_watch_on_line(RIDEDebugger* debugger, const char* line);
void ride_watch_set(RIDEDebugger* debugger, int index, const char* expression);

int ride_stack_count(RIDEDebugger* debugger);
const char* ride_stack_function(RIDEDebugger* debugger, int index);
const char* ride_stack_file(RIDEDebugger* debugger, int index);
int ride_stack_line(RIDEDebugger* debugger, int index);

const char* ride_stack_text(RIDEDebugger* debugger, int index);
int ride_stack_on_line(RIDEDebugger* debugger, const char* line);

int ride_debugger_look_at(RIDEDebugger* debugger, int which);

const char* ride_stop_line_text(const char* file, int line, const char* function);
int ride_looking_at(RIDEDebugger* debugger);
const char* ride_looking_text(RIDEDebugger* debugger);

#ifdef __cplusplus
}
#endif

#endif
