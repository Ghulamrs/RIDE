#include "debugger.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "path.h"

namespace editor {

namespace {

const char* const kMarker = "<<rstudio-done>>";

std::string markerCommand(DebuggerKind kind) {
    if (kind == DebuggerGdb) return "echo <<rstudio-done>>\\n";
    if (kind == DebuggerCdb) {

        return ".printf \"<<rstudio%cdone>>\\n\", 0x2d";
    }
    return "script print(\"<<rstudio\" + \"-done>>\")";
}

void sayMarker(Process& child, DebuggerKind kind) {
    child.say(markerCommand(kind));
}

bool onPath(const std::string& name) {
#ifdef _WIN32
    (void)name;
    return false;
#else
    const char* where = std::getenv("PATH");
    if (!where) return false;
    std::string all = where;
    size_t from = 0;
    while (from <= all.size()) {
        size_t end = all.find(':', from);
        std::string dir = all.substr(from, end == std::string::npos ? std::string::npos : end - from);
        if (!dir.empty() && path::exists(path::join(dir, name))) return true;
        if (end == std::string::npos) break;
        from = end + 1;
    }
    return false;
#endif
}

std::vector<std::string> preamble(DebuggerKind kind) {
    std::vector<std::string> said;
    if (kind == DebuggerGdb) {
        said.push_back("set confirm off");

        said.push_back("set pagination off");

        said.push_back("set width unlimited");
        said.push_back("set breakpoint pending on");

        if (onPath("stdbuf")) said.push_back("set exec-wrapper stdbuf -o0 -e0");
    } else if (kind == DebuggerCdb) {

        said.push_back(".lines -e");
        said.push_back("l+t");
        said.push_back("n 10");
    } else {

        said.push_back("script lldb.debugger.SetAsync(False)");
        said.push_back("settings set auto-confirm true");

        said.push_back("settings set target.input-path /dev/null");
    }
    return said;
}

std::string quoted(const std::string& text) { return "\"" + text + "\""; }

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> out;
    std::string line;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            if (!line.empty() && line[line.size() - 1] == '\r') line.resize(line.size() - 1);
            out.push_back(line);
            line.clear();
            continue;
        }
        line += text[i];
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

std::string withoutPrompt(const std::string& line) {
    std::string out = line;
    for (;;) {
        size_t at = 0;
        if (out.compare(0, 6, "(gdb) ") == 0) {
            at = 6;
        } else if (out.compare(0, 7, "(lldb) ") == 0) {
            at = 7;
        } else {

            size_t i = 0;
            while (i < out.size() && out[i] >= '0' && out[i] <= '9') ++i;
            if (i == 0 || i >= out.size() || out[i] != ':') break;
            size_t j = i + 1;
            while (j < out.size() && std::isxdigit(static_cast<unsigned char>(out[j]))) ++j;

            if (j == i + 1 || out.compare(j, 1, ">") != 0) break;
            at = j + 1;
            if (at < out.size() && out[at] == ' ') ++at;
        }
        out = out.substr(at);
    }
    return out;
}

std::string trimmed(const std::string& text) {
    size_t from = text.find_first_not_of(" \t");
    if (from == std::string::npos) return std::string();
    size_t to = text.find_last_not_of(" \t");
    return text.substr(from, to - from + 1);
}

bool digits(const std::string& text) {
    if (text.empty()) return false;
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] < '0' || text[i] > '9') return false;
    return true;
}

size_t number(const std::string& text) {
    return static_cast<size_t>(std::strtoul(text.c_str(), 0, 10));
}

bool startsWith(const std::string& text, const char* prefix) {
    const size_t n = std::strlen(prefix);
    return text.size() >= n && text.compare(0, n, prefix) == 0;
}

bool endsWith(const std::string& text, char c) {
    return !text.empty() && text[text.size() - 1] == c;
}

DebuggerKind promptOn(const std::string& line) {
    if (startsWith(line, "(gdb)")) return DebuggerGdb;
    if (startsWith(line, "(lldb)")) return DebuggerLldb;
    size_t i = 0;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
    if (i == 0 || i >= line.size() || line[i] != ':') return DebuggerNone;
    size_t j = i + 1;
    while (j < line.size() && std::isxdigit(static_cast<unsigned char>(line[j]))) ++j;
    if (j == i + 1 || line.compare(j, 1, ">") != 0) return DebuggerNone;
    return DebuggerCdb;
}

bool sourceEcho(const std::string& line) {
    std::string rest = trimmed(line);
    if (startsWith(rest, "-> ")) rest = trimmed(rest.substr(3));
    size_t i = 0;
    while (i < rest.size() && rest[i] >= '0' && rest[i] <= '9') ++i;
    if (i == 0) return false;
    while (i < rest.size() && rest[i] == ' ') ++i;
    return i < rest.size() && rest[i] == '\t';
}

bool lldbOwn(const std::string& line) {
    const std::string bare = trimmed(line);
    if (startsWith(bare, "* thread #")) return true;
    if (startsWith(bare, "frame #")) return true;
    if (bare == "^") return true;
    if (startsWith(line, "Process ") || startsWith(line, "Target ")) return true;
    return false;
}

bool gdbOwn(const std::string& line) {
    if (startsWith(line, "Starting program:")) return true;
    if (startsWith(line, "Continuing.")) return true;
    if (startsWith(line, "Breakpoint ")) return true;
    if (startsWith(line, "[")) return true;
    if (startsWith(line, "Using host ")) return true;
    if (startsWith(line, "Reading symbols")) return true;

    if (startsWith(line, "Missing ") &&
        (line.find("debuginfo") != std::string::npos || line.find("try:") != std::string::npos))
        return true;
    return false;
}

bool ourCommand(const std::string& bare) {
    static const char* const said[] = {
        "g", "p", "t", "gu", "k", "q", "ln", "dv", "l+t", "n 10",
        ".lines -e", ".lastevent", ".echo", ".printf"
    };
    for (size_t i = 0; i < sizeof said / sizeof said[0]; ++i)
        if (bare == said[i]) return true;
    return startsWith(bare, "bp ") || startsWith(bare, "bu ") ||
           startsWith(bare, "bc ") || startsWith(bare, ".printf ");
}

bool cdbOwn(const std::string& line) {
    const std::string bare = trimmed(line);
    if (ourCommand(bare)) return true;
    if (startsWith(bare, "Breakpoint ")) return true;
    if (startsWith(bare, "Last event:")) return true;
    if (startsWith(bare, "debugger time:")) return true;
    if (startsWith(bare, "ModLoad:")) return true;

    if (bare.find('!') != std::string::npos && endsWith(bare, ':')) return true;

    if (startsWith(bare, "(") && bare.find('!') != std::string::npos) return true;

    if (bare.find('`') != std::string::npos &&
        std::isxdigit(static_cast<unsigned char>(bare[0]))) return true;

    if (endsWith(bare, ')') &&
        (bare.find(".c(") != std::string::npos || bare.find(".cpp(") != std::string::npos ||
         bare.find(".h(") != std::string::npos)) return true;
    return false;
}

}

bool dbg_stoppedWithNoSource(const std::string& said) {
    return said.find("stop reason") != std::string::npos ||
           said.find("frame #0") != std::string::npos ||
           said.find("#0  0x") != std::string::npos;
}

std::string dbg_programOutput(DebuggerKind kind, const std::string& said) {
    const std::vector<std::string> all = lines(said);
    std::string out;

    for (size_t i = 0; i < all.size(); ++i) {
        std::string line = all[i];

        const DebuggerKind prompt = promptOn(line);
        if (prompt == DebuggerLldb) continue;
        if (prompt != DebuggerNone) line = withoutPrompt(line);

        if (trimmed(line).empty()) continue;
        if (line.find("<<rstudio") != std::string::npos) continue;
        if (sourceEcho(line)) continue;

        if (kind == DebuggerLldb && lldbOwn(line)) continue;
        if (kind == DebuggerGdb && gdbOwn(line)) continue;
        if (kind == DebuggerCdb && cdbOwn(line)) continue;

        out += line;
        out += "\n";
    }
    return out;
}

std::string dbg_withoutEscapes(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\x1b') { out += text[i]; continue; }
        if (i + 1 >= text.size()) break;
        const char kind = text[i + 1];
        if (kind == '[') {
            size_t at = i + 2;
            while (at < text.size() && !(text[at] >= '@' && text[at] <= '~')) ++at;
            i = at;
        } else if (kind == ']') {
            size_t at = i + 2;
            while (at < text.size() && text[at] != '\x07' &&
                   !(text[at] == '\x1b' && at + 1 < text.size() && text[at + 1] == '\\')) ++at;
            if (at < text.size() && text[at] == '\x1b') ++at;
            i = at;
        } else {
            ++i;
        }
    }
    return out;
}

std::string dbg_withoutEcho(const std::string& said, const std::string& asked,
                            const std::string& marker) {
    std::vector<std::string> all = lines(said);
    bool droppedAsked = false, droppedMarker = false;
    std::string out;
    for (size_t i = 0; i < all.size(); ++i) {
        const std::string bare = trimmed(all[i]);
        if (!droppedAsked && bare == trimmed(asked)) { droppedAsked = true; continue; }
        if (!droppedMarker && bare == trimmed(marker)) { droppedMarker = true; continue; }
        out += all[i];
        out += "\n";
    }
    return out;
}

DebuggerKind dbg_here() {
#if defined(_WIN32)
    return DebuggerNone;
#elif defined(__APPLE__)
    return DebuggerLldb;
#else
    return DebuggerGdb;
#endif
}

const char* dbg_name(DebuggerKind kind) {
    switch (kind) {
        case DebuggerLldb: return "lldb";
        case DebuggerGdb:  return "gdb";
        case DebuggerCdb:  return "cdb";
        default:           return "none";
    }
}

const char* dbg_program(DebuggerKind kind) {
    if (kind != DebuggerCdb) return dbg_name(kind);

    static std::string* found = 0;
    if (found) return found->c_str();

    const char* under[2] = {"ProgramFiles(x86)", "ProgramFiles"};
    for (size_t i = 0; i < 2; ++i) {
        const char* root = std::getenv(under[i]);
        if (!root) continue;
        std::string where = std::string(root) + "\\Windows Kits\\10\\Debuggers\\x64\\cdb.exe";
        if (path::exists(where)) {
            found = new std::string(where);
            return found->c_str();
        }
    }
    found = new std::string("cdb");
    return found->c_str();
}

DebuggerKind dbg_for(ToolchainKind kind, const std::string& arch) {

    if (!emitsDebugInfo(kind, arch)) return DebuggerNone;

    if (kind == ToolMsvc)
        return path::exists(dbg_program(DebuggerCdb)) ? DebuggerCdb : DebuggerNone;

    return dbg_here();
}

bool dbg_stopsItself(ToolchainKind kind) { return kind == ToolShc; }

DebugPlan dbg_planFor(const Toolchain& tool, const std::vector<Part>& parts,
                      const std::string& arch) {
    DebugPlan plan;
    if (parts.empty()) return plan;

    plan.kind = toolchainOf(tool, parts[0]);
    for (size_t i = 0; i < parts.size(); ++i) {
        ToolchainKind each = toolchainOf(tool, parts[i]);
        DebuggerKind theirs = dbg_for(each, arch);
        if (theirs == DebuggerNone) {

            plan.blind.push_back(parts[i].group.empty() ? std::string(toolchainName(each))
                                                        : parts[i].group);
            continue;
        }
        if (plan.engine == DebuggerNone) { plan.engine = theirs; plan.kind = each; }
    }

    plan.stopsItself = dbg_stopsItself(toolchainOf(tool, parts[0]));
    if (plan.stopsItself) {
        plan.kind = ToolShc;

        plan.blind.clear();
    }
    return plan;
}

std::string dbg_whyNot(ToolchainKind kind, const std::string& arch) {
    if (dbg_for(kind, arch) != DebuggerNone) return std::string();

    if (dbg_stopsItself(kind))
        return "a Shalimar program stops itself - it needs no debugger at all";
    if (isEmulated(arch))
        return arch + " runs on vm6747, the VM6747 emulator, which is not a debugger "
               "yet - F5 runs it, and vm6747 -t traces every instruction";
    if (kind == ToolMsvc)
        return "cl writes a .pdb and cdb reads one, but cdb is not installed - "
               "add Debugging Tools for Windows";
    if (!emitsDebugInfo(kind, arch))
        return std::string(toolchainName(kind)) + " generates MASM for " + arch +
               ", which carries no line table";
    return std::string("no ") + dbg_name(dbg_here()) + " on this machine";
}

namespace {

bool placeIn(const std::string& line, StackFrame* frame) {
    size_t at = line.rfind(" at ");
    if (at == std::string::npos) return false;

    std::string where = trimmed(line.substr(at + 4));
    size_t colon = where.find_last_of(':');
    if (colon == std::string::npos) return false;

    std::string tail = where.substr(colon + 1);
    std::string head = where.substr(0, colon);
    if (!digits(tail)) return false;

    size_t second = head.find_last_of(':');
    if (second != std::string::npos && digits(head.substr(second + 1))) {
        tail = head.substr(second + 1);
        head = head.substr(0, second);
    }

    frame->file = head;
    frame->line = number(tail);

    std::string front = line.substr(0, at);
    size_t tick = front.find_last_of('`');
    if (tick != std::string::npos) front = front.substr(tick + 1);

    size_t bracket = front.find('(');
    if (bracket != std::string::npos) front = front.substr(0, bracket);

    size_t comma = front.find_last_of(',');
    if (comma != std::string::npos) front = front.substr(comma + 1);

    size_t in = front.rfind(" in ");
    if (in != std::string::npos) front = front.substr(in + 4);

    frame->function = trimmed(front);
    return true;
}

}

Stop dbg_readCdbStop(const std::string& said) {
    Stop stop;
    stop.said = said;

    std::vector<std::string> all = lines(said);
    for (size_t i = 0; i < all.size(); ++i) {
        std::string line = trimmed(withoutPrompt(all[i]));

        size_t ended = line.find("Exit process");
        if (ended != std::string::npos) {
            stop.exited = true;
            stop.stopped = false;
            size_t code = line.find("code ", ended);
            if (code != std::string::npos)
                stop.status = static_cast<int>(std::strtol(line.c_str() + code + 5, 0, 16));
            continue;
        }
        if (line.find("No runnable debuggees") != std::string::npos) {
            stop.exited = true;
            continue;
        }
        if (stop.stopped || stop.exited) continue;

        size_t open = line.rfind('(');
        if (open == std::string::npos || open == 0) continue;
        size_t close = line.find(')', open);
        if (close == std::string::npos) continue;

        std::string number = line.substr(open + 1, close - open - 1);
        if (!digits(number)) continue;

        stop.file = line.substr(0, open);
        stop.line = editor::number(number);
        stop.stopped = true;

        for (size_t j = i + 1; j < all.size() && stop.function.empty(); ++j) {
            std::string under = withoutPrompt(all[j]);
            size_t bang = under.find('!');
            if (bang == std::string::npos) continue;
            std::string rest = under.substr(bang + 1);
            size_t end = rest.find_first_of("+ \t|(");
            stop.function = trimmed(end == std::string::npos ? rest : rest.substr(0, end));
        }
    }
    return stop;
}

Stop dbg_readStop(DebuggerKind kind, const std::string& said) {
    if (kind == DebuggerCdb) return dbg_readCdbStop(said);

    Stop stop;
    stop.said = said;

    std::vector<std::string> all = lines(said);
    for (size_t i = 0; i < all.size(); ++i) {
        std::string line = withoutPrompt(all[i]);

        if (line.find("exited with status") != std::string::npos ||
            line.find("exited with code") != std::string::npos ||
            line.find("exited normally") != std::string::npos) {
            stop.exited = true;
            stop.status = 0;

            size_t at = line.find("status = ");
            int base = 10;
            if (at != std::string::npos) {
                at += 9;
            } else {
                at = line.find("code ");
                if (at != std::string::npos) { at += 5; base = 8; }
            }
            if (at != std::string::npos)
                stop.status = static_cast<int>(std::strtol(line.c_str() + at, 0, base));
            continue;
        }

        if (line.compare(0, 18, "Run till exit from") == 0) continue;

        bool interesting = (kind == DebuggerLldb) ? line.find("frame #0:") != std::string::npos
                                                  : line.find(" at ") != std::string::npos;
        if (!interesting || stop.stopped) continue;

        StackFrame here;
        if (!placeIn(line, &here)) continue;

        stop.file = here.file;
        stop.line = here.line;
        stop.function = here.function;
        stop.stopped = true;
    }

    if (!stop.stopped && !stop.exited) {
        for (size_t i = 0; i < all.size(); ++i) {
            std::string line = withoutPrompt(all[i]);
            size_t tab = line.find('\t');
            if (tab == std::string::npos || tab == 0) continue;
            if (!digits(line.substr(0, tab))) continue;
            stop.line = number(line.substr(0, tab));
            stop.stopped = true;
            break;
        }
    }

    return stop;
}

namespace {

unsigned long long lldbFrameAddress(const std::string& said) {
    std::vector<std::string> all = lines(said);
    for (size_t i = 0; i < all.size(); ++i) {
        std::string line = withoutPrompt(all[i]);
        size_t frame = line.find("frame #0:");
        if (frame == std::string::npos) continue;
        size_t hex = line.find("0x", frame);
        if (hex == std::string::npos) continue;
        return std::strtoull(line.c_str() + hex, 0, 16);
    }
    return 0;
}

}

bool dbg_wentNowhere(DebuggerKind kind, const Stop& before, const Stop& after) {

    if (kind != DebuggerLldb) return false;

    if (!before.stopped || !after.stopped || after.exited) return false;
    if (after.file.empty() || after.function.empty() || after.line == 0) return false;

    if (after.file != before.file || after.line != before.line) return false;
    if (after.function != before.function) return false;

    if (after.said.find("stop reason = breakpoint") != std::string::npos) return false;

    unsigned long long was = lldbFrameAddress(before.said);
    unsigned long long now = lldbFrameAddress(after.said);
    return was != 0 && now > was;
}

std::vector<Variable> dbg_readVariables(DebuggerKind kind, const std::string& said) {
    std::vector<Variable> found;
    std::vector<std::string> all = lines(said);

    for (size_t i = 0; i < all.size(); ++i) {
        std::string line = trimmed(withoutPrompt(all[i]));
        if (line.empty() || line == kMarker) continue;

        std::string type;
        if (kind == DebuggerLldb) {
            if (line.empty() || line[0] != '(') continue;
            size_t close = line.find(')');
            if (close == std::string::npos) continue;
            type = line.substr(1, close - 1);
            line = trimmed(line.substr(close + 1));
        }

        size_t equals = line.find(" = ");
        if (equals == std::string::npos) continue;

        Variable variable;
        variable.name = trimmed(line.substr(0, equals));
        variable.type = type;
        variable.value = trimmed(line.substr(equals + 3));

        if (kind == DebuggerCdb && variable.value.compare(0, 2, "0n") == 0)
            variable.value = variable.value.substr(2);
        if (variable.name.empty() || variable.name.find(' ') != std::string::npos) continue;
        found.push_back(variable);
    }
    return found;
}

std::string afterFrameNumber(DebuggerKind kind, const std::string& line) {
    if (kind == DebuggerLldb) {
        size_t frame = line.find("frame #");
        if (frame == std::string::npos) return std::string();
        size_t colon = line.find(':', frame);
        if (colon == std::string::npos) return std::string();
        return trimmed(line.substr(colon + 1));
    }

    if (line.empty() || line[0] != '#') return std::string();
    size_t i = 1;
    while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
    if (i == 1) return std::string();
    return trimmed(line.substr(i));
}

bool cdbFrameIn(const std::string& line, StackFrame* frame) {
    size_t open = line.rfind('[');
    size_t close = line.rfind(']');
    if (open == std::string::npos || close == std::string::npos || close < open) return false;

    std::string where = line.substr(open + 1, close - open - 1);
    size_t sign = where.rfind(" @ ");
    if (sign == std::string::npos) return false;
    std::string tail = trimmed(where.substr(sign + 3));
    if (!digits(tail)) return false;

    frame->file = trimmed(where.substr(0, sign));
    frame->line = number(tail);

    std::string call = trimmed(line.substr(0, open));
    size_t space = call.find_last_of(" \t");
    if (space != std::string::npos) call = call.substr(space + 1);
    size_t bang = call.find('!');
    if (bang == std::string::npos) return false;
    call = call.substr(bang + 1);
    size_t end = call.find_first_of("+ \t(");
    frame->function = trimmed(end == std::string::npos ? call : call.substr(0, end));
    return true;
}

std::vector<StackFrame> dbg_readFrames(DebuggerKind kind, const std::string& said) {
    std::vector<StackFrame> found;
    std::vector<std::string> all = lines(said);

    for (size_t i = 0; i < all.size(); ++i) {
        std::string line = trimmed(withoutPrompt(all[i]));
        if (line.empty() || line == kMarker) continue;

        StackFrame frame;
        if (kind == DebuggerCdb) {
            if (!cdbFrameIn(line, &frame)) continue;
        } else {
            std::string rest = afterFrameNumber(kind, line);
            if (rest.empty() || !placeIn(rest, &frame)) continue;
        }

        found.push_back(frame);

        if (frame.function == "main") break;
    }
    return found;
}

std::string dbg_frameLine(const StackFrame& frame, bool looking) {

    return std::string(looking ? "> " : "  ") + frame.function + "   " +
           path::filename(frame.file) + ":" + std::to_string(frame.line);
}

size_t dbg_frameOnLine(const std::vector<StackFrame>& stack, const std::string& line) {
    const std::string bare = trimmed(line);
    for (size_t i = 0; i < stack.size(); ++i) {

        if (trimmed(dbg_frameLine(stack[i], false)) == bare) return i;
        if (trimmed(dbg_frameLine(stack[i], true)) == bare) return i;
    }
    return stack.size();
}

std::string dbg_variableLine(const Variable& variable) {
    std::string said = "  " + variable.name + " = " + variable.value;
    if (!variable.type.empty()) said += "   [" + variable.type + "]";
    return said;
}

size_t dbg_variableOnLine(const std::vector<Variable>& locals, const std::string& line) {
    const std::string bare = trimmed(line);
    for (size_t i = 0; i < locals.size(); ++i)
        if (trimmed(dbg_variableLine(locals[i])) == bare) return i;
    return locals.size();
}

namespace {

std::string complaintIn(const std::string& answer) {
    const char* const words[] = {
        "error",
        "No symbol",
        "not an lvalue",
        "Couldn't",
        "cannot be",
        "Syntax error",
        "Type conflict",
        "Bad register error"
    };

    std::vector<std::string> all = lines(answer);
    for (size_t i = 0; i < all.size(); ++i) {
        std::string line = trimmed(withoutPrompt(all[i]));
        for (size_t w = 0; w < sizeof words / sizeof words[0]; ++w) {
            if (line.find(words[w]) == std::string::npos) continue;

            size_t at = line.find("error: ");
            if (at != std::string::npos) line = line.substr(at + 7);
            return line;
        }
    }
    return std::string();
}

}

std::string dbg_watchLine(const Watch& watch) {
    return "  " + watch.expression + " = " + (watch.ok ? watch.value : "[" + watch.value + "]");
}

size_t dbg_watchOnLine(const std::vector<Watch>& watches, const std::string& line) {
    const std::string bare = trimmed(line);
    for (size_t i = 0; i < watches.size(); ++i)
        if (trimmed(dbg_watchLine(watches[i])) == bare) return i;
    return watches.size();
}

std::string dbg_readValue(DebuggerKind kind, const std::string& said) {
    std::vector<std::string> all = lines(said);

    for (size_t i = 0; i < all.size(); ++i) {
        const std::string line = trimmed(withoutPrompt(all[i]));
        if (line.empty() || line == kMarker) continue;

        if (kind == DebuggerCdb) {

            if (line[0] == '?') continue;

            size_t space = line.find_last_of(' ');
            if (space == std::string::npos || space + 1 >= line.size()) continue;
            std::string value = line.substr(space + 1);
            if (value.compare(0, 2, "0n") == 0) value = value.substr(2);

            if (value.find_first_of("0123456789") == std::string::npos) continue;
            return value;
        }

        size_t dollar = line.find('$');
        if (dollar == std::string::npos) continue;
        size_t equals = line.find(" = ", dollar);
        if (equals == std::string::npos) continue;
        return trimmed(line.substr(equals + 3));
    }
    return std::string();
}

std::string dbg_stopLine(const std::string& file, size_t line,
                         const std::string& function) {
    return "stopped at " + path::filename(file) + ":" + std::to_string(line) +
           (function.empty() ? std::string() : " in " + function);
}

std::string dbg_lookingAt(const StackFrame& frame) {
    std::string where = path::filename(frame.file) + ":" + std::to_string(frame.line);
    if (frame.function.empty()) return "the variables are those of " + where;
    return "the variables are " + frame.function + "'s, at " + where;
}

Debugger::Debugger() : kind_(DebuggerNone), onConsole_(false) {}
Debugger::~Debugger() { stop(); }

bool Debugger::start(DebuggerKind kind, const std::string& executable,
                     const std::string& program) {
    stop();

    kind_ = kind;
    if (kind_ == DebuggerNone) return false;

    executable_ = executable;
    std::string run = program.empty() ? dbg_program(kind_) : program;

    std::string command = quoted(run) + " " + quoted(executable);
    if (kind_ == DebuggerCdb) {

        command = quoted(run) + " -y " + quoted(path::parent(executable)) +
                  " " + quoted(executable);
    }

    onConsole_ = false;
    if (kind_ == DebuggerCdb && child_.startOnConsole(command)) {
        onConsole_ = true;
    } else if (!child_.start(command)) {
        kind_ = DebuggerNone;
        return false;
    }

    std::vector<std::string> first = preamble(kind_);
    for (size_t i = 0; i < first.size(); ++i) child_.say(first[i]);

    bool found = false;
    sayMarker(child_, kind_);
    child_.readUntil(kMarker, &found);
    if (!found) {
        child_.stop();
        kind_ = DebuggerNone;
        return false;
    }
    return true;
}

void Debugger::stop() {
    if (child_.running()) {
        child_.say("quit");
        child_.stop();
    }
    kind_ = DebuggerNone;
}

std::string Debugger::ask(const std::string& command) {
    if (!running()) return std::string();

    child_.say(command);
    sayMarker(child_, kind_);

    bool found = false;
    std::string said = child_.readUntil(kMarker, &found);
    if (!found) child_.stop();

    if (onConsole_) said = dbg_withoutEcho(dbg_withoutEscapes(said), command, markerCommand(kind_));
    return said;
}

bool Debugger::breakAt(const std::string& file, size_t line) {
    if (!running()) return false;

    char digitsIn[32];
    std::snprintf(digitsIn, sizeof digitsIn, "%lu", static_cast<unsigned long>(line));

    std::string said;
    if (kind_ == DebuggerGdb) {
        said = ask("break " + path::filename(file) + ":" + digitsIn);
    } else if (kind_ == DebuggerCdb) {

        said = ask("bp `" + path::filename(file) + ":" + digitsIn + "`");
        return said.find("Couldn't resolve") == std::string::npos &&
               said.find("Bp expression") == std::string::npos;
    } else {
        said = ask("breakpoint set --file " + quoted(path::filename(file)) + " --line " + digitsIn);
    }

    return said.find("Breakpoint") != std::string::npos ||
           said.find("breakpoint") != std::string::npos;
}

bool Debugger::clearBreakpoints() {
    if (!running()) return false;
    ask(kind_ == DebuggerGdb ? "delete"
                             : (kind_ == DebuggerCdb ? "bc *" : "breakpoint delete --force"));
    return true;
}

Stop Debugger::afterMoving(const std::string& command) {
    std::string said = ask(command);

    if (kind_ == DebuggerCdb) {
        std::string event = ask(".lastevent");
        said += "\n" + event;
        if (event.find("Exit process") == std::string::npos) said += "\n" + ask("ln");
    }

    Stop stop = dbg_readStop(kind_, said);
    if (!running()) stop.stopped = false;

    if (stop.stopped) {
        if (stop.file.empty()) stop.file = last_.file;
        if (stop.function.empty()) stop.function = last_.function;
        last_ = stop;
    } else if (stop.exited) {
        last_ = Stop();
    }

    readWatches();
    return stop;
}

Stop Debugger::run() {

    if (kind_ == DebuggerCdb) return afterMoving("g");

    return afterMoving(kind_ == DebuggerGdb ? "run < /dev/null" : "run");
}

Stop Debugger::afterStepping(const std::string& command) {
    Stop before = last_;
    Stop stop = afterMoving(command);
    std::string said = stop.said;

    for (int again = 0; again < 16 && dbg_wentNowhere(kind_, before, stop); ++again) {
        before = stop;
        stop = afterMoving(command);

        said += "\n" + stop.said;
    }

    stop.said = said;
    return stop;
}

Stop Debugger::resume() { return afterMoving(kind_ == DebuggerCdb ? "g" : "continue"); }
Stop Debugger::stepOver() { return afterStepping(kind_ == DebuggerCdb ? "p" : "next"); }
Stop Debugger::stepInto() { return afterStepping(kind_ == DebuggerCdb ? "t" : "step"); }
Stop Debugger::stepOut() { return afterMoving(kind_ == DebuggerCdb ? "gu" : "finish"); }

std::vector<Variable> Debugger::locals() {
    if (!running()) return std::vector<Variable>();

    if (kind_ == DebuggerCdb) return dbg_readVariables(kind_, ask("dv"));
    if (kind_ != DebuggerGdb) return dbg_readVariables(kind_, ask("frame variable"));

    std::vector<Variable> found = dbg_readVariables(kind_, ask("info args"));
    std::vector<Variable> locals = dbg_readVariables(kind_, ask("info locals"));
    for (size_t i = 0; i < locals.size(); ++i) found.push_back(locals[i]);
    return found;
}

std::string Debugger::evaluate(const std::string& expression, bool* ok) {
    if (ok) *ok = false;
    if (!running() || expression.empty()) return std::string();

    std::string answer;
    if (kind_ == DebuggerCdb) {
        answer = ask("?? " + expression);
    } else if (kind_ == DebuggerGdb) {
        answer = ask("print " + expression);
    } else {
        answer = ask("expression " + expression);
    }

    const std::string value = dbg_readValue(kind_, answer);
    if (!value.empty()) {
        if (ok) *ok = true;
        return value;
    }

    const std::string why = complaintIn(answer);
    return why.empty() ? "no answer" : why;
}

void Debugger::addWatch(const std::string& expression) {
    if (expression.empty()) return;
    Watch watch;
    watch.expression = expression;
    watches_.push_back(watch);
    if (running()) watches_[watches_.size() - 1].value =
        evaluate(expression, &watches_[watches_.size() - 1].ok);
}

void Debugger::setWatch(size_t which, const std::string& expression) {
    if (which >= watches_.size()) return;
    if (expression.empty()) { removeWatch(which); return; }
    watches_[which].expression = expression;
    watches_[which].value.clear();
    watches_[which].ok = false;
    if (running()) watches_[which].value = evaluate(expression, &watches_[which].ok);
}

void Debugger::removeWatch(size_t which) {
    if (which >= watches_.size()) return;
    watches_.erase(watches_.begin() + static_cast<long>(which));
}

void Debugger::readWatches() {
    for (size_t i = 0; i < watches_.size(); ++i) {
        if (!running()) {
            watches_[i].value = "not running";
            watches_[i].ok = false;
            continue;
        }
        watches_[i].value = evaluate(watches_[i].expression, &watches_[i].ok);
    }
}

bool Debugger::setVariable(const std::string& name, const std::string& value,
                           std::string* said) {
    if (said) said->clear();
    if (!running() || name.empty() || value.empty()) return false;

    std::string answer;
    if (kind_ == DebuggerCdb) {
        answer = ask("?? " + name + " = " + value);
    } else if (kind_ == DebuggerGdb) {
        answer = ask("set variable " + name + " = " + value);
    } else {
        answer = ask("expression " + name + " = " + value);
    }

    const std::string why = complaintIn(answer);
    if (!why.empty()) {
        if (said) *said = why;
        return false;
    }

    readWatches();
    return true;
}

bool Debugger::selectFrame(size_t which) {
    if (!running()) return false;

    const std::string number = std::to_string(which);
    std::string said;
    if (kind_ == DebuggerCdb) {

        said = ask(".frame " + number);
    } else if (kind_ == DebuggerGdb) {
        said = ask("frame " + number);
    } else {
        said = ask("frame select " + number);
    }

    if (said.find("error:") != std::string::npos) return false;
    if (said.find("No frame at level") != std::string::npos) return false;
    if (said.find("Invalid frame") != std::string::npos) return false;

    readWatches();
    return true;
}

std::vector<StackFrame> Debugger::frames() {
    if (!running()) return std::vector<StackFrame>();

    if (kind_ == DebuggerCdb) return dbg_readFrames(kind_, ask("k"));
    if (kind_ == DebuggerGdb) return dbg_readFrames(kind_, ask("backtrace"));
    return dbg_readFrames(kind_, ask("thread backtrace"));
}

}
