
#include <cstdio>

#include <windows.h>

#include "MainForm.h"

#include "symbols.h"

using namespace System;
using namespace System::Windows::Forms;

// The window's own log, and the fault log beside it, live in %TEMP% - not
// in the working directory, which is wherever the shortcut said and used to
// leave RStudioGui.log in bin\ and examples\ and nowhere when the start it
// records failed before anything could be written there.
// (Spelled through the environment: <windows.h> is included above, and its
// GetTempPath macro would rewrite the .NET method's name.)
static String^ LogPath(String^ leaf) {
    String^ temp = Environment::GetEnvironmentVariable("TEMP");
    if (temp == nullptr || temp->Length == 0) temp = Environment::GetEnvironmentVariable("TMP");
    if (temp == nullptr || temp->Length == 0) temp = ".";
    return System::IO::Path::Combine(temp, leaf);
}

static void Note(String^ what) {
    try {
        System::IO::File::AppendAllText(LogPath("RStudioGui.log"),
                                        DateTime::Now.ToString("HH:mm:ss") + "  " + what +
                                            Environment::NewLine);
    } catch (Exception^) {

    }
}

static void OnUnhandled(Object^, UnhandledExceptionEventArgs^ e) {
    Note("unhandled: " + e->ExceptionObject->ToString());
}

static void QuietConsoleForChildren() {
    if (GetConsoleWindow() != NULL) return;
    if (!AllocConsole()) return;

    HWND console = GetConsoleWindow();
    if (console != NULL) ShowWindow(console, SW_HIDE);

    FILE* ignored = NULL;
    freopen_s(&ignored, "NUL", "r", stdin);
    freopen_s(&ignored, "NUL", "w", stdout);
    freopen_s(&ignored, "NUL", "w", stderr);
}

[STAThreadAttribute]
int main(array<String^>^ arguments) {
    Note("main entered");

    {
        array<Byte>^ bytes = System::Text::Encoding::UTF8->GetBytes(LogPath("RStudioGui-fault.log") + "\0");
        pin_ptr<Byte> pinned = &bytes[0];
        rstudio_watch_for_faults(reinterpret_cast<const char*>(pinned));
    }
    Note("faults watched");
    QuietConsoleForChildren();
    Note("console quiet");
    AppDomain::CurrentDomain->UnhandledException +=
        gcnew UnhandledExceptionEventHandler(OnUnhandled);

    try {
        Note("starting, " + arguments->Length + " arguments, in " +
             System::IO::Directory::GetCurrentDirectory());
        editor::installPlatformDemangler();
        Application::EnableVisualStyles();
        Application::SetCompatibleTextRenderingDefault(false);

        String^ directory = arguments->Length > 0 ? arguments[0] : nullptr;
        array<String^>^ files =
            gcnew array<String^>(arguments->Length > 1 ? arguments->Length - 1 : 0);
        for (int i = 1; i < arguments->Length; ++i) files[i - 1] = arguments[i];

        Note("building the window");
        rstudiogui::MainForm^ window = gcnew rstudiogui::MainForm(directory, files);
        Note("window built, running");
        Application::Run(window);
        Note("closed cleanly");
    } catch (Exception^ problem) {
        Note("caught: " + problem->ToString());
        return 1;
    }
    return 0;
}
