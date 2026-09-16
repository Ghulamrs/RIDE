#pragma once

#include "bridge.h"

namespace rstudiogui {

using namespace System;
using namespace System::Windows::Forms;

ref class Sheet {
public:
    String^ path;
    RichTextBox^ box;
    Panel^ gutter;
    TabPage^ page;
};

ref class Gutter : public Panel {
public:
    Gutter() { DoubleBuffered = true; }
};

value struct Spot {
    int x;
    int y;
};

// The file tabs, with a close × built into the tab view itself. It owner-draws
// each tab (name, then a × at the right) and hit-tests clicks on the ×, raising
// TabCloseRequested(index) - so the close button is part of the control rather
// than something painted over it from outside.
public delegate void TabCloseHandler(int index);

public ref class ClosableTabControl : public System::Windows::Forms::TabControl {
public:
    event TabCloseHandler^ TabCloseRequested;

    ClosableTabControl() {
        this->DrawMode = System::Windows::Forms::TabDrawMode::OwnerDrawFixed;
        this->Padding = System::Drawing::Point(16, 3);
    }

protected:
    System::Drawing::Rectangle CloseRect(System::Drawing::Rectangle tab) {
        int s = 14;
        return System::Drawing::Rectangle(tab.Right - s - 4,
                                          tab.Y + (tab.Height - s) / 2, s, s);
    }

    virtual void OnDrawItem(System::Windows::Forms::DrawItemEventArgs^ e) override {
        if (e->Index < 0 || e->Index >= this->TabCount) return;
        System::Drawing::Rectangle r = this->GetTabRect(e->Index);
        bool selected = (e->Index == this->SelectedIndex);
        e->Graphics->FillRectangle(selected ? System::Drawing::SystemBrushes::Window
                                            : System::Drawing::SystemBrushes::Control, r);
        System::Drawing::Rectangle textRect(r.X + 6, r.Y, r.Width - 26, r.Height);
        System::Windows::Forms::TextRenderer::DrawText(
            e->Graphics, this->TabPages[e->Index]->Text, this->Font, textRect,
            System::Drawing::SystemColors::ControlText,
            static_cast<System::Windows::Forms::TextFormatFlags>(
                static_cast<int>(System::Windows::Forms::TextFormatFlags::Left) |
                static_cast<int>(System::Windows::Forms::TextFormatFlags::VerticalCenter) |
                static_cast<int>(System::Windows::Forms::TextFormatFlags::EndEllipsis)));
        System::Windows::Forms::TextRenderer::DrawText(
            e->Graphics, gcnew System::String(static_cast<wchar_t>(0x00D7), 1), this->Font, CloseRect(r),
            System::Drawing::Color::FromArgb(110, 110, 110),
            static_cast<System::Windows::Forms::TextFormatFlags>(
                static_cast<int>(System::Windows::Forms::TextFormatFlags::HorizontalCenter) |
                static_cast<int>(System::Windows::Forms::TextFormatFlags::VerticalCenter)));
    }

    virtual void OnMouseDown(System::Windows::Forms::MouseEventArgs^ e) override {
        for (int i = 0; i < this->TabCount; ++i) {
            if (CloseRect(this->GetTabRect(i)).Contains(e->Location)) {
                TabCloseRequested(i);
                return;
            }
        }
        System::Windows::Forms::TabControl::OnMouseDown(e);
    }
};

public ref class MainForm : public Form {
public:
    MainForm() { Start(nullptr, nullptr); }
    MainForm(String^ projectDirectory, array<String^>^ files) {
        paneMode_ = PaneMode::PaneProject;
        Start(projectDirectory, files);
    }

protected:

    virtual void OnResize(EventArgs^ e) override {
        Form::OnResize(e);
        if (stopBar_ != nullptr) PlaceStopBar();
    }

    virtual bool ProcessCmdKey(System::Windows::Forms::Message% message, Keys keys) override {
        if (keys == static_cast<Keys>(Keys::Control | Keys::D)) { NextConfig(); return true; }
        if (keys == static_cast<Keys>(Keys::Control | Keys::K)) { NextTool(); return true; }
        if (keys == static_cast<Keys>(Keys::Control | Keys::T)) { NextTarget(); return true; }
        if (keys == static_cast<Keys>(Keys::Control | Keys::Up)) { LookAlongStack(1); return true; }
        if (keys == static_cast<Keys>(Keys::Control | Keys::Down)) {
            LookAlongStack(-1);
            return true;
        }
        return Form::ProcessCmdKey(message, keys);
    }

    virtual void OnFormClosing(System::Windows::Forms::FormClosingEventArgs^ e) override {
        for (int i = 0; i < sheets_->Count; ++i)
            if (!MayDiscard(sheets_[i])) {
                e->Cancel = true;
                return;
            }
        RememberOpen();
        Form::OnFormClosing(e);
    }

    virtual void OnShown(EventArgs^ e) override {
        Form::OnShown(e);
        Arrange();
        // A start with nothing to open has no sheet to select - the
        // genuinely empty environment CloseSheet leaves, from the first.
        if (text_ != nullptr) {
            text_->Select(0, 0);
            text_->Focus();
        }
        Recolour();

        for (int i = 0; i < sheets_->Count; ++i) {
            sheets_[i]->box->Modified = false;
            MarkTab(sheets_[i]);
        }
    }

    static String^ ProductName() { return "RIDE"; }

    // The window title: the product and its version, then the project it is in,
    // then the file in front - "RIDE 3.5 - demo - main.c". With no project it
    // is "RIDE 3.5 - main.c"; with neither, just "RIDE 3.5". One place, so
    // opening a file, loading or closing a project and saving-as all say it the
    // same way.
    void RefreshTitle() {
        String^ title = ProductName() + " " + FromUtf8(rstudio_version());
        String^ project = project_ == nullptr ? nullptr
                                              : FromUtf8(rstudio_project_name(project_));
        if (project != nullptr && project->Length > 0) title += " - " + project;
        if (path_ != nullptr && path_->Length > 0)
            title += " - " + System::IO::Path::GetFileName(path_);
        Text = title;
    }

    ~MainForm() { this->!MainForm(); }
    !MainForm() {
        if (project_ != nullptr) {
            rstudio_project_free(project_);
            project_ = nullptr;
        }

        if (built_ != nullptr) {
            rstudio_program_free(built_);
            built_ = nullptr;
        }
        if (targetBuilt_ != nullptr) {
            rstudio_build_free(targetBuilt_);
            targetBuilt_ = nullptr;
        }
        if (debugger_ != nullptr) {
            rstudio_debugger_free(debugger_);
            debugger_ = nullptr;
        }
    }

private:
    RStudioProject* project_;

    String^ arch_;
    String^ cc1_;
    String^ cl_;
    String^ shc_;
    String^ cxx1_;
    int toolKind_;
    int config_;
    int indentWidth_;
    int indentTabs_;
    int indentCase_;

    RStudioDebugger* debugger_;
    RStudioProgram* built_;

    ToolStripMenuItem^ upTheStack_;
    ToolStripMenuItem^ downTheStack_;
    ToolStripMenuItem^ watchItem_;
    bool busy_;
    int pending_;
    int workResult_;
    int workKind_;
    int workLanguage_;

    String^ workProgram_;

    RStudioBuild* targetBuilt_;

    System::Collections::Generic::Dictionary<String^,
        System::Collections::Generic::List<int>^>^ breaks_;
    System::Collections::Generic::Dictionary<String^, String^>^ breakNames_;

    int errorLine_;
    int errorColumn_;
    String^ errorMessage_;
    String^ errorFile_;
    String^ stopFile_;
    String^ stopFunction_;
    String^ lookingFile_;
    int lookingLine_;
    int stopLine_;

    int highlightRow_;

    Panel^ stopBar_;

    System::Drawing::Font^ codeFont_;

    String^ path_;
    String^ projectDirectory_;
    String^ needle_;
    bool colouring_;

    bool stateGood_;
    int stateRow_;
    int stateAt_;

    Timer^ settle_;

    SplitContainer^ outer_;
    SplitContainer^ upper_;

    TreeView^ tree_;
    ClosableTabControl^ files_;
    System::Collections::Generic::List<Sheet^>^ sheets_;

    RichTextBox^ text_;
    TabControl^ panel_;

    bool numbers_;

    System::Collections::Generic::List<ToolStripMenuItem^>^ targetItems_;
    ToolStripMenuItem^ toolAutoItem_;
    ToolStripMenuItem^ toolCc1Item_;
    ToolStripMenuItem^ toolClItem_;
    ToolStripMenuItem^ toolShcItem_;
    ToolStripMenuItem^ toolCxx1Item_;
    ToolStripMenuItem^ langAutoItem_;
    ToolStripMenuItem^ langCItem_;
    ToolStripMenuItem^ langCppItem_;
    ToolStripMenuItem^ langShalimarItem_;
    ToolStripMenuItem^ langJsonItem_;
    ToolStripMenuItem^ langTextItem_;
    ToolStripMenuItem^ convertItem_;
    ToolStripMenuItem^ debugConfigItem_;
    ToolStripMenuItem^ releaseConfigItem_;
    ToolStripMenuItem^ numbersItem_;
    ToolStripMenuItem^ paneItem_;
    ToolStripMenuItem^ panelItem_;
    TextBox^ console_;
    TextBox^ debug_;
    RichTextBox^ assembly_;
    StatusStrip^ status_;
    ToolStripStatusLabel^ build_;
    ToolStripStatusLabel^ where_;
    ToolStripStatusLabel^ what_;
    ToolStripStatusLabel^ root_;
    // The compiler in use, shown at the right end of the menu bar, plain text,
    // changing with the file, the Language menu and the Tools menu.
    ToolStripLabel^ compilerHint_;
    System::Windows::Forms::Timer^ recolourTimer_;

    static array<Byte>^ Utf8Of(String^ text) {
        array<Byte>^ raw = System::Text::Encoding::UTF8->GetBytes(text == nullptr ? "" : text);
        array<Byte>^ out = gcnew array<Byte>(raw->Length + 1);
        Array::Copy(raw, out, raw->Length);
        return out;
    }

    static String^ FromUtf8(const char* text) {
        if (text == nullptr) return String::Empty;
        int length = 0;
        while (text[length] != '\0') ++length;
        if (length == 0) return String::Empty;

        array<Byte>^ bytes = gcnew array<Byte>(length);
        Runtime::InteropServices::Marshal::Copy(IntPtr(const_cast<char*>(text)), bytes, 0,
                                                length);
        return System::Text::Encoding::UTF8->GetString(bytes);
    }

    static String^ TakeUtf8(char* text) {
        String^ out = FromUtf8(text);
        rstudio_free(text);
        return out;
    }

    void Start(String^ projectDirectory, array<String^>^ files) {
        project_ = rstudio_project_new();
        arch_ = "x86_64-windows";

        // The i-line names, since 3.5: the compilers docked beside the editor
        // are cc1i, cxx1i and shci (the Toolchain struct defaults to the same).
        // Named looks for "<name>.exe" beside the editor, so the plain names
        // used to find a stale cc1.exe / cxx1.exe left over from a 3.0 build in
        // the same directory - an old cc1 then read a project's .cpp as C and
        // said "expected a type". cl stays the host compiler, found on PATH.
        cc1_ = Named("CC1", "cc1i");
        cl_ = Named("CL", "cl");
        shc_ = Named("SHC", "shci");
        cxx1_ = Named("CXX1", "cxx1i");
        toolKind_ = rstudio_default_compiler();
        languageChoice_ = -1;
        config_ = RSTUDIO_CONFIG_DEBUG;
        debugger_ = rstudio_debugger_new();
        built_ = nullptr;
        targetBuilt_ = nullptr;
        workProgram_ = nullptr;
        busy_ = false;
        pending_ = 0;
        workResult_ = 0;
        workKind_ = 0;
        workLanguage_ = 0;
        breaks_ = gcnew System::Collections::Generic::Dictionary<String^,
            System::Collections::Generic::List<int>^>();
        breakNames_ = gcnew System::Collections::Generic::Dictionary<String^, String^>();
        stopFile_ = nullptr;
        stopLine_ = 0;
        lookingFile_ = nullptr;
        lookingLine_ = 0;
        highlightRow_ = -1;
        stopBar_ = nullptr;
        codeFont_ = RememberedFont();
        numbers_ = true;
        ForgetError();
        indentWidth_ = 4;
        indentTabs_ = 0;
        indentCase_ = 0;

        Lay();

        // The last project is remembered and offered - Project > Recent -
        // never opened on its own: a start with nothing named is empty.
        if (projectDirectory != nullptr) LoadProject(projectDirectory);
        RefreshRecent();

        bool anyNamed = false;
        if (files != nullptr)
            for (int i = 0; i < files->Length; ++i)
                if (files[i] != nullptr) {
                    OpenPath(files[i]);
                    anyNamed = true;
                }

        if (!anyNamed) OpenFirstOfProject();
        started_ = true;

        // Nothing opened: a genuinely empty environment, not an untitled
        // sheet with a bare group name beside it. The sheet Lay() made is
        // the spare OpenPath would have taken, so it goes when unused.
        if (sheets_->Count == 1 && sheets_[0]->path == nullptr &&
            sheets_[0]->box->TextLength == 0 && !sheets_[0]->box->Modified) {
            Sheet^ spare = sheets_[0];
            sheets_->Remove(spare);
            files_->TabPages->Remove(spare->page);
            text_ = nullptr;
            path_ = nullptr;
            if (rstudio_project_loaded(project_) == 0) paneMode_ = PaneMode::PaneFiles;
            RefreshTitle();
            FillTree();
            SayBuild();
            what_->Text = "ready";
        }
    }

    void Lay() {
        RefreshTitle();
        Width = 1100;
        Height = 760;
        MinimumSize = System::Drawing::Size(840, 560);
        StartPosition = FormStartPosition::CenterScreen;
        DoubleBuffered = true;
        colouring_ = false;
        stateGood_ = false;
        stateRow_ = 0;
        stateAt_ = 0;

        settle_ = gcnew Timer();
        settle_->Interval = 250;
        settle_->Tick += gcnew EventHandler(this, &MainForm::OnSettled);

        MenuStrip^ bar = gcnew MenuStrip();

        ToolStripMenuItem^ file = gcnew ToolStripMenuItem("&File");
        file->DropDownItems->Add("New", nullptr,
                                 gcnew EventHandler(this, &MainForm::OnNewBuffer));

        file->DropDownItems->Add(gcnew ToolStripSeparator());
        file->DropDownItems->Add("Open file...", nullptr,
                                 gcnew EventHandler(this, &MainForm::OnOpenFile));
        ToolStripMenuItem^ save = gcnew ToolStripMenuItem(
            "Save", nullptr, gcnew EventHandler(this, &MainForm::OnSave));
        save->ShortcutKeys = static_cast<Keys>(Keys::Control | Keys::S);
        file->DropDownItems->Add(save);
        file->DropDownItems->Add("Save as...", nullptr,
                                 gcnew EventHandler(this, &MainForm::OnSaveAs));
        file->DropDownItems->Add("Rename...", nullptr,
                                 gcnew EventHandler(this, &MainForm::OnRenameFile));
        file->DropDownItems->Add(
            Item("Close", Keys::Control | Keys::W, gcnew EventHandler(this, &MainForm::OnCloseFile)));
        file->DropDownItems->Add(gcnew ToolStripSeparator());

        file->DropDownItems->Add(
            Item("Next file", Keys::Control | Keys::PageDown,
                 gcnew EventHandler(this, &MainForm::OnNextFile)));
        file->DropDownItems->Add(
            Item("Previous file", Keys::Control | Keys::PageUp,
                 gcnew EventHandler(this, &MainForm::OnPreviousFile)));
        file->DropDownItems->Add(
            Item("Exit", Keys::Control | Keys::Q, gcnew EventHandler(this, &MainForm::OnExit)));
        bar->Items->Add(file);

        ToolStripMenuItem^ edit = gcnew ToolStripMenuItem("&Edit");
        edit->DropDownItems->Add(Item("Undo", Keys::Control | Keys::Z, gcnew EventHandler(this, &MainForm::OnUndo)));
        edit->DropDownItems->Add(Item("Redo", Keys::Control | Keys::Y, gcnew EventHandler(this, &MainForm::OnRedo)));
        edit->DropDownItems->Add(gcnew ToolStripSeparator());
        edit->DropDownItems->Add(Item("Cut", Keys::Control | Keys::X, gcnew EventHandler(this, &MainForm::OnCut)));
        edit->DropDownItems->Add(Item("Copy", Keys::Control | Keys::C, gcnew EventHandler(this, &MainForm::OnCopy)));
        edit->DropDownItems->Add(Item("Paste", Keys::Control | Keys::V, gcnew EventHandler(this, &MainForm::OnPaste)));
        edit->DropDownItems->Add(
            Item("Select all", Keys::Control | Keys::A, gcnew EventHandler(this, &MainForm::OnSelectAll)));
        edit->DropDownItems->Add(gcnew ToolStripSeparator());
        edit->DropDownItems->Add(Item("Find...", Keys::Control | Keys::F, gcnew EventHandler(this, &MainForm::OnFind)));
        edit->DropDownItems->Add(Item("Find next", Keys::F3, gcnew EventHandler(this, &MainForm::OnFindNext)));
        edit->DropDownItems->Add(
            Item("Find previous", Keys::Shift | Keys::F3, gcnew EventHandler(this, &MainForm::OnFindPrevious)));
        edit->DropDownItems->Add(
            Item("Replace...", Keys::Control | Keys::H, gcnew EventHandler(this, &MainForm::OnReplace)));
        edit->DropDownItems->Add(gcnew ToolStripSeparator());
        edit->DropDownItems->Add(
            Item("Re-indent", Keys::Control | Keys::L, gcnew EventHandler(this, &MainForm::OnLayOut)));
        bar->Items->Add(edit);

        ToolStripMenuItem^ project = gcnew ToolStripMenuItem("&Project");

        project->DropDownItems->Add("New...", nullptr,
                                    gcnew EventHandler(this, &MainForm::OnNewProject));
        project->DropDownItems->Add("Open...", nullptr,
                                    gcnew EventHandler(this, &MainForm::OnOpenProjectFile));
        project->DropDownItems->Add("Save", nullptr,
                                    gcnew EventHandler(this, &MainForm::OnSaveProject));
        project->DropDownItems->Add("Save as...", nullptr,
                                    gcnew EventHandler(this, &MainForm::OnSaveProjectAs));
        project->DropDownItems->Add("Close", nullptr,
                                    gcnew EventHandler(this, &MainForm::OnCloseProject));

        project->DropDownItems->Add(gcnew ToolStripSeparator());
        project->DropDownItems->Add(
            Item("New File", Keys::Control | Keys::N,
                 gcnew EventHandler(this, &MainForm::OnNewFile)));
        project->DropDownItems->Add("Add File", nullptr,
                                    gcnew EventHandler(this, &MainForm::OnAddThisFile));
        project->DropDownItems->Add("Remove File", nullptr,
                                    gcnew EventHandler(this, &MainForm::OnRemoveFromProject));
        // The last three projects, most recent first, to recall one by name.
        project->DropDownItems->Add(gcnew ToolStripSeparator());
        recentItems_ = gcnew System::Collections::Generic::List<ToolStripMenuItem^>();
        for (int i = 0; i < 3; ++i) {
            ToolStripMenuItem^ one = gcnew ToolStripMenuItem(
                "Recent", nullptr, gcnew EventHandler(this, &MainForm::OnOpenRecent));
            one->Tag = i;
            one->Visible = false;
            recentItems_->Add(one);
            project->DropDownItems->Add(one);
        }
        bar->Items->Add(project);

        ToolStripMenuItem^ build = gcnew ToolStripMenuItem("&Build");
        ToolStripMenuItem^ compile = gcnew ToolStripMenuItem(
            "Compile", nullptr, gcnew EventHandler(this, &MainForm::OnCompile));

        compile->ShortcutKeys = static_cast<Keys>(Keys::Control | Keys::B);
        build->DropDownItems->Add(compile);
        ToolStripMenuItem^ runIt = gcnew ToolStripMenuItem(
            "Run", nullptr, gcnew EventHandler(this, &MainForm::OnRun));
        runIt->ShortcutKeys = Keys::F5;
        build->DropDownItems->Add(runIt);

        build->DropDownItems->Add(gcnew ToolStripSeparator());
        build->DropDownItems->Add(
            Item("Build project", Keys::F4,
                 gcnew EventHandler(this, &MainForm::OnBuildProject)));
        build->DropDownItems->Add("Run project", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnRunProject));
        debugConfigItem_ = gcnew ToolStripMenuItem(
            "Debug build", nullptr, gcnew EventHandler(this, &MainForm::OnDebugConfig));

        debugConfigItem_->ShortcutKeyDisplayString = "Ctrl+D";
        build->DropDownItems->Add(debugConfigItem_);
        releaseConfigItem_ = gcnew ToolStripMenuItem(
            "Release build", nullptr, gcnew EventHandler(this, &MainForm::OnReleaseConfig));
        releaseConfigItem_->ShortcutKeyDisplayString = "Ctrl+D";
        build->DropDownItems->Add(releaseConfigItem_);
        bar->Items->Add(build);

        ToolStripMenuItem^ debug = gcnew ToolStripMenuItem("&Debug");
        debug->DropDownItems->Add(Item("Start / continue", Keys::F8,
                                       gcnew EventHandler(this, &MainForm::OnDebug)));

        debug->DropDownItems->Add("Debug project", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnDebugProject));

        debug->DropDownItems->Add(gcnew ToolStripSeparator());
        debug->DropDownItems->Add(Item("Toggle breakpoint", Keys::F9,
                                       gcnew EventHandler(this, &MainForm::OnToggleBreak)));
        debug->DropDownItems->Add(Item("Step over", Keys::F7,
                                       gcnew EventHandler(this, &MainForm::OnStepOver)));
        debug->DropDownItems->Add(Item("Step into", Keys::F6,
                                       gcnew EventHandler(this, &MainForm::OnStepInto)));
        debug->DropDownItems->Add("Step out", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnStepOut));
        debug->DropDownItems->Add(gcnew ToolStripSeparator());

        upTheStack_ = gcnew ToolStripMenuItem(
            "Up the stack", nullptr, gcnew EventHandler(this, &MainForm::OnFrameUp));
        upTheStack_->ShortcutKeyDisplayString = "Ctrl+Up";
        debug->DropDownItems->Add(upTheStack_);
        downTheStack_ = gcnew ToolStripMenuItem(
            "Down the stack", nullptr, gcnew EventHandler(this, &MainForm::OnFrameDown));
        downTheStack_->ShortcutKeyDisplayString = "Ctrl+Down";
        debug->DropDownItems->Add(downTheStack_);
        watchItem_ = gcnew ToolStripMenuItem(
            "Watch expression...", nullptr, gcnew EventHandler(this, &MainForm::OnWatch));
        debug->DropDownItems->Add(watchItem_);

        debug->DropDownOpening +=
            gcnew EventHandler(this, &MainForm::OnDebugMenuOpening);

        debug->DropDownItems->Add(gcnew ToolStripSeparator());
        debug->DropDownItems->Add("Stop debugging", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnDebugStop));
        bar->Items->Add(debug);

        ToolStripMenuItem^ view = gcnew ToolStripMenuItem("&View");

        view->DropDownItems->Add(Item("Project pane", Keys::Control | Keys::D0,
                                      gcnew EventHandler(this, &MainForm::OnFocusTree)));
        view->DropDownItems->Add(Item("The file", Keys::Control | Keys::D4,
                                      gcnew EventHandler(this, &MainForm::OnFocusText)));
        view->DropDownItems->Add(gcnew ToolStripSeparator());
        view->DropDownItems->Add(Item("Console", Keys::Control | Keys::D1,
                                      gcnew EventHandler(this, &MainForm::OnShowConsole)));
        view->DropDownItems->Add(Item("Debug", Keys::Control | Keys::D2,
                                      gcnew EventHandler(this, &MainForm::OnShowDebug)));
        view->DropDownItems->Add(Item("Assembly", Keys::Control | Keys::D3,
                                      gcnew EventHandler(this, &MainForm::OnShowAssembly)));
        view->DropDownItems->Add(gcnew ToolStripSeparator());

        numbersItem_ = Item("Show line numbers", Keys::None,
                            gcnew EventHandler(this, &MainForm::OnToggleNumbers));
        numbersItem_->Checked = true;
        view->DropDownItems->Add(numbersItem_);

        paneItem_ = Item("Show project pane", Keys::Control | Keys::P,
                         gcnew EventHandler(this, &MainForm::OnTogglePane));
        paneItem_->Checked = true;
        view->DropDownItems->Add(paneItem_);

        panelItem_ = Item("Show bottom panel", Keys::Control | Keys::E,
                          gcnew EventHandler(this, &MainForm::OnTogglePanel));
        panelItem_->Checked = true;
        view->DropDownItems->Add(panelItem_);

        bar->Items->Add(view);

        ToolStripMenuItem^ target = gcnew ToolStripMenuItem("&Target");
        targetItems_ = gcnew System::Collections::Generic::List<ToolStripMenuItem^>();
        for (int i = 0; i < rstudio_arch_count(); ++i) {
            ToolStripMenuItem^ one = gcnew ToolStripMenuItem(
                FromUtf8(rstudio_arch(i)), nullptr, gcnew EventHandler(this, &MainForm::OnTarget));
            one->ShortcutKeyDisplayString = "Ctrl+T";
            targetItems_->Add(one);
            target->DropDownItems->Add(one);
        }

        ToolStripMenuItem^ language = gcnew ToolStripMenuItem("Lan&guage");
        langAutoItem_ = gcnew ToolStripMenuItem(
            "By extension", nullptr, gcnew EventHandler(this, &MainForm::OnLangAuto));
        language->DropDownItems->Add(langAutoItem_);
        langCItem_ = gcnew ToolStripMenuItem(
            "C", nullptr, gcnew EventHandler(this, &MainForm::OnLangC));
        language->DropDownItems->Add(langCItem_);
        langCppItem_ = gcnew ToolStripMenuItem(
            "C++", nullptr, gcnew EventHandler(this, &MainForm::OnLangCpp));
        language->DropDownItems->Add(langCppItem_);
        langShalimarItem_ = gcnew ToolStripMenuItem(
            "Shalimar", nullptr, gcnew EventHandler(this, &MainForm::OnLangShalimar));
        language->DropDownItems->Add(langShalimarItem_);
        langJsonItem_ = gcnew ToolStripMenuItem(
            "JSON", nullptr, gcnew EventHandler(this, &MainForm::OnLangJson));
        language->DropDownItems->Add(langJsonItem_);
        langTextItem_ = gcnew ToolStripMenuItem(
            "Plain text", nullptr, gcnew EventHandler(this, &MainForm::OnLangText));
        language->DropDownItems->Add(langTextItem_);
        language->DropDownItems->Add(gcnew ToolStripSeparator());
        convertItem_ = gcnew ToolStripMenuItem(
            "Convert (c2s / s2c)", nullptr, gcnew EventHandler(this, &MainForm::OnConvert));
        language->DropDownItems->Add(convertItem_);
        bar->Items->Add(language);

        ToolStripMenuItem^ tools = gcnew ToolStripMenuItem("Too&ls");
        toolAutoItem_ = gcnew ToolStripMenuItem(
            "By language", nullptr, gcnew EventHandler(this, &MainForm::OnToolAuto));
        toolAutoItem_->ShortcutKeyDisplayString = "Ctrl+K";
        tools->DropDownItems->Add(toolAutoItem_);
        toolCc1Item_ = gcnew ToolStripMenuItem(
            "cc1", nullptr, gcnew EventHandler(this, &MainForm::OnToolCc1));
        toolCc1Item_->ShortcutKeyDisplayString = "Ctrl+K";
        tools->DropDownItems->Add(toolCc1Item_);
        toolCxx1Item_ = gcnew ToolStripMenuItem(
            "cxx1", nullptr, gcnew EventHandler(this, &MainForm::OnToolCxx1));
        toolCxx1Item_->ShortcutKeyDisplayString = "Ctrl+K";
        tools->DropDownItems->Add(toolCxx1Item_);

        toolShcItem_ = gcnew ToolStripMenuItem(
            "shc", nullptr, gcnew EventHandler(this, &MainForm::OnToolShc));
        toolShcItem_->ShortcutKeyDisplayString = "Ctrl+K";
        tools->DropDownItems->Add(toolShcItem_);
        toolClItem_ = gcnew ToolStripMenuItem(
            "MSVC (cl)", nullptr, gcnew EventHandler(this, &MainForm::OnToolCl));
        toolClItem_->ShortcutKeyDisplayString = "Ctrl+K";
        tools->DropDownItems->Add(toolClItem_);
        tools->DropDownItems->Add(gcnew ToolStripSeparator());

        tools->DropDownItems->Add("Font...", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnFont));
        tools->DropDownItems->Add(gcnew ToolStripSeparator());
        tools->DropDownItems->Add("Header directories...", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnHeaderDirs));
        tools->DropDownItems->Add("Project include paths...", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnProjectIncludes));
        tools->DropDownItems->Add("Project libraries...", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnProjectLibraries));
        tools->DropDownItems->Add("Locate vcvars64.bat...", nullptr,
                                  gcnew EventHandler(this, &MainForm::OnLocateVcvars));
        bar->Items->Add(tools);
        bar->Items->Add(target);

        ToolStripMenuItem^ help = gcnew ToolStripMenuItem("&Help");
        help->DropDownItems->Add(Item("Keys", Keys::F1,
                                      gcnew EventHandler(this, &MainForm::OnKeys)));
        help->DropDownItems->Add("About " + ProductName(), nullptr,
                                 gcnew EventHandler(this, &MainForm::OnAbout));
        bar->Items->Add(help);

        // The compiler in use, at the right end of the menu bar: plain text,
        // no box, changing as the file, the Language menu or the Tools menu
        // change it. SayBuild fills it in beside the status bar's fuller line.
        compilerHint_ = gcnew ToolStripLabel();
        compilerHint_->Alignment = ToolStripItemAlignment::Right;
        compilerHint_->ForeColor = System::Drawing::Color::FromArgb(90, 90, 90);
        compilerHint_->Font = gcnew System::Drawing::Font(compilerHint_->Font,
                                                          System::Drawing::FontStyle::Bold);
        // Keep it off the right edge - a little breathing room.
        compilerHint_->Margin = System::Windows::Forms::Padding(0, 1, 16, 2);
        bar->Items->Add(compilerHint_);

        // Re-highlighting is deferred a beat after the last scroll so the mouse
        // wheel does not fight the highlighter (which selects text as it colours
        // and made the view tremble when it ran on every wheel tick).
        recolourTimer_ = gcnew System::Windows::Forms::Timer();
        recolourTimer_->Interval = 70;
        recolourTimer_->Tick += gcnew EventHandler(this, &MainForm::OnRecolourTick);

        MainMenuStrip = bar;
        Controls->Add(bar);
        ShowChoices();

        outer_ = gcnew SplitContainer();
        outer_->Dock = DockStyle::Fill;
        outer_->Orientation = Orientation::Horizontal;
        outer_->SplitterWidth = 5;
        outer_->BackColor = System::Drawing::Color::FromArgb(222, 222, 222);
        SplitContainer^ outer = outer_;

        upper_ = gcnew SplitContainer();
        upper_->Dock = DockStyle::Fill;
        upper_->SplitterWidth = 5;
        upper_->BackColor = System::Drawing::Color::FromArgb(222, 222, 222);
        SplitContainer^ upper = upper_;

        tree_ = gcnew TreeView();
        tree_->Dock = DockStyle::Fill;
        tree_->BorderStyle = System::Windows::Forms::BorderStyle::None;
        tree_->BackColor = System::Drawing::Color::FromArgb(250, 250, 250);
        tree_->ItemHeight = 20;
        tree_->FullRowSelect = true;
        tree_->HideSelection = false;
        tree_->ShowLines = false;
        tree_->ShowRootLines = false;
        tree_->Indent = 16;
        tree_->NodeMouseDoubleClick +=
            gcnew TreeNodeMouseClickEventHandler(this, &MainForm::OnTreeOpen);

        tree_->KeyDown += gcnew KeyEventHandler(this, &MainForm::OnTreeKey);
        upper->Panel1->Controls->Add(tree_);

        sheets_ = gcnew System::Collections::Generic::List<Sheet^>();
        files_ = gcnew ClosableTabControl();
        files_->Dock = DockStyle::Fill;
        files_->SelectedIndexChanged +=
            gcnew EventHandler(this, &MainForm::OnSheetChanged);
        files_->TabCloseRequested += gcnew TabCloseHandler(this, &MainForm::OnTabClose);
        upper->Panel2->Controls->Add(files_);
        outer->Panel1->Controls->Add(upper);

        panel_ = gcnew TabControl();
        panel_->Dock = DockStyle::Fill;

        console_ = ReadOnlyBox();

        console_->KeyDown += gcnew KeyEventHandler(this, &MainForm::OnConsoleKey);
        console_->DoubleClick += gcnew EventHandler(this, &MainForm::OnConsoleDoubleClick);
        debug_ = ReadOnlyBox();

        debug_->KeyDown += gcnew KeyEventHandler(this, &MainForm::OnDebugKey);
        debug_->DoubleClick += gcnew EventHandler(this, &MainForm::OnDebugDoubleClick);
        assembly_ = gcnew RichTextBox();
        assembly_->Dock = DockStyle::Fill;
        assembly_->BorderStyle = System::Windows::Forms::BorderStyle::None;
        assembly_->Font = gcnew System::Drawing::Font("Consolas", 10.0f);
        assembly_->ReadOnly = true;
        assembly_->WordWrap = false;

        TabPage^ one = gcnew TabPage("Console");
        one->Controls->Add(console_);
        TabPage^ two = gcnew TabPage("Debug");
        two->Controls->Add(debug_);
        TabPage^ three = gcnew TabPage("Assembly");
        three->Controls->Add(assembly_);
        panel_->TabPages->Add(one);
        panel_->TabPages->Add(two);
        panel_->TabPages->Add(three);
        outer->Panel2->Controls->Add(panel_);

        Controls->Add(outer);
        outer->BringToFront();

        status_ = gcnew StatusStrip();
        what_ = gcnew ToolStripStatusLabel("no file");
        what_->Spring = true;
        what_->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;

        build_ = gcnew ToolStripStatusLabel("");
        build_->BorderSides = ToolStripStatusLabelBorderSides::Left;
        build_->ToolTipText =
            "language, debug or release, the compiler that will run "
            "(* when the file chose it) and the target when it matters";
        root_ = gcnew ToolStripStatusLabel("no project");
        root_->BorderSides = ToolStripStatusLabelBorderSides::Left;
        root_->ForeColor = System::Drawing::Color::FromArgb(90, 90, 90);
        where_ = gcnew ToolStripStatusLabel("1:1");
        where_->BorderSides = ToolStripStatusLabelBorderSides::Left;
        status_->Items->Add(what_);
        status_->Items->Add(build_);
        status_->Items->Add(root_);
        status_->Items->Add(where_);
        Controls->Add(status_);
        SayBuild();

        String^ kept = FromUtf8(rstudio_settings_set_aside());
        if (kept != nullptr && kept->Length > 0)
            what_->Text = "bad configuration file - kept as " +
                          System::IO::Path::GetFileName(kept) + ", a new one made";

        upper->FixedPanel = FixedPanel::Panel1;
        outer->FixedPanel = FixedPanel::Panel2;

        Sheet^ first = MakeSheet(nullptr, "");
        text_ = first->box;

        console_->Text = "cc1 or cl output appears here.  Ctrl-B builds, F5 runs.";
        SayDebugTab(nullptr);
        SayWhere();
    }

    void Arrange() {
        const int forTree = 120;
        const int forCode = 240;
        const int forUpper = 160;
        const int forPanel = 80;

        if (upper_ != nullptr && upper_->Width > forTree + forCode) {
            upper_->Panel1MinSize = forTree;
            upper_->Panel2MinSize = forCode;
            upper_->SplitterDistance =
                Math::Max(forTree, Math::Min(240, upper_->Width - forCode));
        }

        if (outer_ != nullptr &&
            outer_->Height > forUpper + forPanel + outer_->SplitterWidth) {
            outer_->Panel1MinSize = forUpper;
            outer_->Panel2MinSize = forPanel;

            int deep = Math::Max(120, Math::Min(220, outer_->Height / 4));
            int distance = outer_->Height - deep - outer_->SplitterWidth;
            int most = outer_->Height - forPanel - outer_->SplitterWidth;
            outer_->SplitterDistance = Math::Max(forUpper, Math::Min(distance, most));
        }
    }

    // The project's own choice of file first, then the one that defines
    // main, then the first file there is.
    void OpenFirstOfProject() {
        String^ relative = FromUtf8(rstudio_project_file_to_open(project_));
        if (relative->Length == 0) return;
        array<Byte>^ bytes = Utf8Of(relative);
        pin_ptr<Byte> pinned = &bytes[0];
        String^ full = FromUtf8(
            rstudio_project_absolute(project_, reinterpret_cast<const char*>(pinned)));
        if (full->Length > 0 && System::IO::File::Exists(full)) OpenPath(full);
    }

    void RememberOpen() {
        if (path_ == nullptr || path_->Length == 0) return;
        array<Byte>^ bytes = Utf8Of(path_);
        pin_ptr<Byte> pinned = &bytes[0];
        rstudio_remember_open(project_, reinterpret_cast<const char*>(pinned));
    }

    String^ RootNow() {
        String^ root = FromUtf8(rstudio_project_root(project_));
        if (root == nullptr || root->Length == 0) root = projectDirectory_;
        return root;
    }

    // {app} is the folder above bin\, where RStudio.exe lives. New projects
    // default under {app}\projects and single programs under {app}\programs, so
    // they land in a known place beside the install instead of on the Desktop or
    // in the read-only directory the shortcut starts in. Made on demand (the
    // installer also creates them, user-writable).
    String^ AppDir() {
        try {
            System::IO::DirectoryInfo^ above =
                System::IO::Directory::GetParent(Application::StartupPath);
            if (above != nullptr) return above->FullName;
        } catch (Exception^) { }
        return Application::StartupPath;
    }
    String^ MadeUnderApp(String^ leaf) {
        String^ d = System::IO::Path::Combine(AppDir(), leaf);
        try { System::IO::Directory::CreateDirectory(d); } catch (Exception^) { }
        return d;
    }
    String^ ProjectsDir() { return MadeUnderApp("projects"); }
    String^ ProgramsDir() { return MadeUnderApp("programs"); }

    void SayWhere() {
        String^ root = RootNow();
        root_->Text = root == nullptr || root->Length == 0 ? "no project" : root;
    }

    void ShowConsoleEnd() {
        console_->SelectionStart = console_->TextLength;
        console_->SelectionLength = 0;
        console_->ScrollToCaret();
    }

    static String^ Lines(String^ text) {
        if (String::IsNullOrEmpty(text)) return text;
        return text->Replace("\r\n", "\n")->Replace("\r", "\n")->Replace("\n", "\r\n");
    }

    static String^ Named(String^ variable, String^ orElse) {
        String^ said = Environment::GetEnvironmentVariable(variable);
        if (said != nullptr && said->Length != 0) return said;
        String^ here = Application::StartupPath;
        if (here != nullptr && here->Length != 0) {
            String^ beside = System::IO::Path::Combine(here, orElse + ".exe");
            if (System::IO::File::Exists(beside)) return beside;
        }
        return orElse;
    }

    [System::Runtime::InteropServices::DllImport("user32.dll", SetLastError = true)]
    static int GetWindowLong(System::IntPtr window, int index);
    [System::Runtime::InteropServices::DllImport("user32.dll", SetLastError = true)]
    static int SetWindowLong(System::IntPtr window, int index, int value);
    [System::Runtime::InteropServices::DllImport("user32.dll", SetLastError = true)]
    static bool SetLayeredWindowAttributes(System::IntPtr window, int key, unsigned char alpha,
                                           int flags);

    System::Drawing::Font^ RememberedFont() {
        String^ said = FromUtf8(rstudio_code_font());
        if (said != nullptr && said->Length > 0) {
            int cut = said->LastIndexOf(' ');
            if (cut > 0) {
                String^ name = said->Substring(0, cut);
                double points = 0;
                if (Double::TryParse(said->Substring(cut + 1),
                                     System::Globalization::NumberStyles::Float,
                                     System::Globalization::CultureInfo::InvariantCulture,
                                     points) && points >= 6 && points <= 48) {
                    try {
                        System::Drawing::Font^ made =
                            gcnew System::Drawing::Font(name, (float)points);

                        if (made->FontFamily->Name == name) return made;
                    } catch (Exception^) { }
                }
            }
        }
        return gcnew System::Drawing::Font("Consolas", 11.0f);
    }

    ToolStripMenuItem^ Item(String^ label, Keys key, EventHandler^ handler) {
        ToolStripMenuItem^ item = gcnew ToolStripMenuItem(label, nullptr, handler);
        item->ShortcutKeys = key;
        return item;
    }

    String^ Ask(String^ title, String^ initial) { return Ask(title, nullptr, initial); }

    String^ Ask(String^ title, String^ note, String^ initial) {
        Form^ box = gcnew Form();
        box->Text = title;
        box->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedDialog;
        box->StartPosition = System::Windows::Forms::FormStartPosition::CenterParent;
        box->MinimizeBox = false;
        box->MaximizeBox = false;

        int lift = note == nullptr || note->Length == 0 ? 0 : 24;
        box->ClientSize = System::Drawing::Size(480, 96 + lift);

        if (lift > 0) {
            Label^ says = gcnew Label();
            says->Text = note;
            says->AutoEllipsis = true;
            says->ForeColor = System::Drawing::Color::FromArgb(90, 90, 90);
            says->SetBounds(12, 12, 456, 20);
            box->Controls->Add(says);
        }

        TextBox^ entry = gcnew TextBox();
        entry->Text = initial == nullptr ? "" : initial;
        entry->SetBounds(12, 16 + lift, 456, 26);
        entry->Font = gcnew System::Drawing::Font("Consolas", 10.0f);
        entry->SelectAll();

        Button^ yes = gcnew Button();
        yes->Text = "OK";
        yes->DialogResult = System::Windows::Forms::DialogResult::OK;
        yes->SetBounds(306, 56 + lift, 78, 28);

        Button^ no = gcnew Button();
        no->Text = "Cancel";
        no->DialogResult = System::Windows::Forms::DialogResult::Cancel;
        no->SetBounds(390, 56 + lift, 78, 28);

        box->Controls->Add(entry);
        box->Controls->Add(yes);
        box->Controls->Add(no);
        box->AcceptButton = yes;
        box->CancelButton = no;

        if (box->ShowDialog(this) != System::Windows::Forms::DialogResult::OK) return nullptr;
        return entry->Text;
    }

    int LeadingOf(int row) {
        if (row < 0 || row >= text_->Lines->Length) return 0;
        String^ line = text_->Lines[row];
        int lead = 0;
        while (lead < line->Length && (line[lead] == ' ' || line[lead] == '\t')) ++lead;
        return lead;
    }

    int CaretRow() { return text_->GetLineFromCharIndex(text_->SelectionStart); }
    int CaretColumn() {
        return text_->SelectionStart - text_->GetFirstCharIndexFromLine(CaretRow());
    }

    int CharacterColumn(int row, int byteColumn) {
        if (row < 0 || row >= text_->Lines->Length) return 0;
        array<Byte>^ bytes = Utf8Of(text_->Lines[row]);
        int usable = bytes->Length - 1;
        if (byteColumn > usable) byteColumn = usable;
        if (byteColumn <= 0) return 0;
        return System::Text::Encoding::UTF8->GetString(bytes, 0, byteColumn)->Length;
    }

    int ByteColumn(int row, int characterColumn) {
        if (row < 0 || row >= text_->Lines->Length) return 0;
        String^ line = text_->Lines[row];
        if (characterColumn > line->Length) characterColumn = line->Length;
        if (characterColumn <= 0) return 0;
        return System::Text::Encoding::UTF8->GetByteCount(line->Substring(0, characterColumn));
    }

    array<Byte>^ WholeText() { return Utf8Of(text_->Text->Replace("\r\n", "\n")); }

    Sheet^ Current() {
        int at = files_->SelectedIndex;
        if (at < 0 || at >= sheets_->Count) return nullptr;
        return sheets_[at];
    }

    static String^ OneName(String^ path) {
        if (path == nullptr) return nullptr;
        String^ full = path;
        try {
            full = System::IO::Path::GetFullPath(path);
        } catch (Exception^) {

        }
        return full->Replace('/', '\\')->TrimEnd('\\')->ToLowerInvariant();
    }

    static bool SamePath(String^ one, String^ other) {
        if (one == nullptr || other == nullptr) return false;
        return String::Equals(OneName(one), OneName(other), StringComparison::Ordinal);
    }

    Sheet^ SheetFor(String^ path) {
        for (int i = 0; i < sheets_->Count; ++i)
            if (SamePath(sheets_[i]->path, path)) return sheets_[i];
        return nullptr;
    }

    // The type spelled in full: inside a Form, the bare name is the
    // control's own ContextMenuStrip property.
    System::Windows::Forms::ContextMenuStrip^ EditMenuFor(RichTextBox^ box) {
        System::Windows::Forms::ContextMenuStrip^ menu = gcnew System::Windows::Forms::ContextMenuStrip();
        menu->Items->Add("Undo", nullptr, gcnew EventHandler(this, &MainForm::OnUndo));
        menu->Items->Add("Redo", nullptr, gcnew EventHandler(this, &MainForm::OnRedo));
        menu->Items->Add(gcnew ToolStripSeparator());
        menu->Items->Add("Cut", nullptr, gcnew EventHandler(this, &MainForm::OnCut));
        menu->Items->Add("Copy", nullptr, gcnew EventHandler(this, &MainForm::OnCopy));
        menu->Items->Add("Paste", nullptr, gcnew EventHandler(this, &MainForm::OnPaste));
        menu->Items->Add(gcnew ToolStripSeparator());
        menu->Items->Add("Select all", nullptr, gcnew EventHandler(this, &MainForm::OnSelectAll));
        // Before it opens: Cut and Copy want a selection, Paste text on the
        // clipboard, Undo and Redo something to undo or redo.
        menu->Opening += gcnew System::ComponentModel::CancelEventHandler(this, &MainForm::OnEditMenuOpening);
        menu->Tag = box;
        return menu;
    }

    void OnEditMenuOpening(Object^ sender, System::ComponentModel::CancelEventArgs^) {
        System::Windows::Forms::ContextMenuStrip^ menu =
            safe_cast<System::Windows::Forms::ContextMenuStrip^>(sender);
        RichTextBox^ box = safe_cast<RichTextBox^>(menu->Tag);
        bool selected = box->SelectionLength > 0;
        menu->Items[0]->Enabled = box->CanUndo;
        menu->Items[1]->Enabled = box->CanRedo;
        menu->Items[3]->Enabled = selected;
        menu->Items[4]->Enabled = selected;
        menu->Items[5]->Enabled = Clipboard::ContainsText();
        menu->Items[7]->Enabled = box->TextLength > 0;
    }

    Sheet^ MakeSheet(String^ path, String^ contents) {
        Sheet^ sheet = gcnew Sheet();
        sheet->path = path;

        sheet->box = gcnew RichTextBox();
        sheet->box->Dock = DockStyle::Fill;
        sheet->box->Font = codeFont_;
        sheet->box->WordWrap = false;
        sheet->box->AcceptsTab = true;
        sheet->box->HideSelection = false;
        sheet->box->BorderStyle = System::Windows::Forms::BorderStyle::None;
        sheet->box->Text = contents == nullptr ? "" : contents;
        sheet->box->KeyDown += gcnew KeyEventHandler(this, &MainForm::OnKeyDown);
        sheet->box->KeyUp += gcnew KeyEventHandler(this, &MainForm::OnKeyUp);
        sheet->box->SelectionChanged += gcnew EventHandler(this, &MainForm::OnCaretMoved);
        // The right-click menu: the Edit menu's own handlers, so that a paste
        // from here is the paste Ctrl-V does. A RichTextBox has none of its
        // own, which read as a dead right button.
        sheet->box->ContextMenuStrip = EditMenuFor(sheet->box);
        sheet->box->TextChanged += gcnew EventHandler(this, &MainForm::OnTextChanged);
        sheet->box->VScroll += gcnew EventHandler(this, &MainForm::OnScrolled);

        sheet->gutter = gcnew Gutter();
        sheet->gutter->Dock = DockStyle::Left;
        sheet->gutter->Width = 52;
        sheet->gutter->BackColor = System::Drawing::Color::FromArgb(245, 245, 245);
        sheet->gutter->Tag = sheet->box;
        sheet->gutter->Paint += gcnew PaintEventHandler(this, &MainForm::OnGutterPaint);
        sheet->gutter->Visible = numbers_;
        sheet->box->Tag = sheet->gutter;

        sheet->page = gcnew TabPage(path == nullptr
                                        ? "untitled"
                                        : System::IO::Path::GetFileName(path));
        sheet->page->Controls->Add(sheet->box);
        sheet->page->Controls->Add(sheet->gutter);
        sheet->box->BringToFront();

        sheets_->Add(sheet);
        PaneFollowsTabs();
        files_->TabPages->Add(sheet->page);
        files_->SelectedTab = sheet->page;
        // The first tab of an empty environment is selected as it is
        // added, and selecting it again raises no change - so the sheet is
        // made current here, not left to the event. Without this, File >
        // New after an empty start had a sheet nothing could paste into.
        OnSheetChanged(nullptr, nullptr);
        return sheet;
    }

    void OnSheetChanged(Object^, EventArgs^) {
        Sheet^ sheet = Current();
        if (sheet == nullptr) return;

        text_ = sheet->box;
        path_ = sheet->path;
        RefreshTitle();
        what_->Text = path_ == nullptr
                          ? "untitled"
                          : System::IO::Path::GetFileName(path_) + "  " +
                                text_->Lines->Length + " lines";
        text_->Focus();
        SayBuild();
        PlaceStopBar();
        sheet->gutter->Invalidate();

        Recolour();
    }

    literal int kDrawing = 0x000B;
    literal int kWhereScrolled = 0x04DD;
    literal int kScrollTo = 0x04DE;

    [System::Runtime::InteropServices::DllImport("user32.dll", EntryPoint = "SendMessageW")]
    static IntPtr Tell(IntPtr window, int message, IntPtr one, IntPtr two);

    [System::Runtime::InteropServices::DllImport("user32.dll", EntryPoint = "SendMessageW")]
    static IntPtr Tell(IntPtr window, int message, IntPtr one, Spot% where);

    static void Drawing(Control^ box, bool allowed) {
        Tell(box->Handle, kDrawing, IntPtr(allowed ? 1 : 0), IntPtr::Zero);
        if (allowed) {
            box->Invalidate();
            box->Update();
        }
    }

    void OnTextChanged(Object^ sender, EventArgs^) {

        if (colouring_) return;

        Sheet^ sheet = Current();
        if (sheet == nullptr) return;

        int digits = sheet->box->Lines->Length < 1 ? 1
                                                   : sheet->box->Lines->Length.ToString()->Length;
        int wanted = 22 + 9 * digits;
        if (wanted > sheet->gutter->Width) sheet->gutter->Width = wanted;
        sheet->gutter->Invalidate();

        if (sender == text_) {

            RecolourLine(CaretRow());
            settle_->Stop();
            settle_->Start();
            MarkTab(sheet);
        }
    }

    void OnScrolled(Object^, EventArgs^) {
        PlaceStopBar();
        Sheet^ sheet = Current();
        if (sheet == nullptr) return;

        sheet->gutter->Invalidate();   // line numbers keep up while scrolling
        recolourTimer_->Stop();        // recolour once the wheel settles, so it
        recolourTimer_->Start();       // does not fight the scroll and tremble
    }

    void OnRecolourTick(Object^, EventArgs^) {
        recolourTimer_->Stop();
        Recolour();
        Sheet^ sheet = Current();
        if (sheet != nullptr) sheet->gutter->Invalidate();
    }

    void OnGutterPaint(Object^ sender, PaintEventArgs^ e) {
        Panel^ panel = safe_cast<Panel^>(sender);
        RichTextBox^ box = safe_cast<RichTextBox^>(panel->Tag);
        if (box == nullptr) return;

        System::Drawing::Pen^ edge =
            gcnew System::Drawing::Pen(System::Drawing::Color::FromArgb(228, 228, 228));
        e->Graphics->DrawLine(edge, panel->Width - 1, 0, panel->Width - 1, panel->Height);

        int lines = box->Lines->Length;
        if (lines < 1) lines = 1;

        int first = box->GetLineFromCharIndex(box->GetCharIndexFromPosition(
            System::Drawing::Point(1, 1)));
        int caretLine = box->GetLineFromCharIndex(box->SelectionStart);

        System::Drawing::Brush^ quiet =
            gcnew System::Drawing::SolidBrush(System::Drawing::Color::FromArgb(150, 150, 150));
        System::Drawing::Brush^ here =
            gcnew System::Drawing::SolidBrush(System::Drawing::Color::FromArgb(60, 60, 60));

        String^ file = nullptr;
        for each (Sheet^ sheet in sheets_)
            if (sheet->gutter == panel) file = sheet->path;

        System::Collections::Generic::List<int>^ marks = nullptr;
        if (file != nullptr) breaks_->TryGetValue(OneName(file), marks);

        bool sameFile = file != nullptr && stopFile_ != nullptr &&
                        System::IO::Path::GetFileName(file) ==
                            System::IO::Path::GetFileName(stopFile_);

        System::Drawing::Brush^ breakMark =
            gcnew System::Drawing::SolidBrush(System::Drawing::Color::FromArgb(200, 60, 60));
        System::Drawing::Brush^ stopMark =
            gcnew System::Drawing::SolidBrush(System::Drawing::Color::FromArgb(40, 150, 60));

        System::Drawing::Pen^ lookMark =
            gcnew System::Drawing::Pen(System::Drawing::Color::FromArgb(40, 150, 60));

        bool sameLookFile = file != nullptr && lookingFile_ != nullptr &&
                            System::IO::Path::GetFileName(file) ==
                                System::IO::Path::GetFileName(lookingFile_);

        for (int row = first; row < lines; ++row) {
            int at = box->GetFirstCharIndexFromLine(row);
            if (at < 0) break;
            System::Drawing::Point where = box->GetPositionFromCharIndex(at);
            if (where.Y > panel->Height) break;

            float top = static_cast<float>(where.Y) + 3.0f;
            bool standingHere = sameFile && stopLine_ == row + 1;
            bool lookingHere = sameLookFile && lookingLine_ == row + 1;
            if (standingHere || lookingHere) {
                array<System::Drawing::PointF>^ arrow = gcnew array<System::Drawing::PointF>(3);
                arrow[0] = System::Drawing::PointF(3.0f, top);
                arrow[1] = System::Drawing::PointF(12.0f, top + 4.5f);
                arrow[2] = System::Drawing::PointF(3.0f, top + 9.0f);
                if (standingHere) e->Graphics->FillPolygon(stopMark, arrow);
                else e->Graphics->DrawPolygon(lookMark, arrow);
            } else if (marks != nullptr && marks->Contains(row + 1)) {
                e->Graphics->FillEllipse(breakMark, 3.0f, top, 9.0f, 9.0f);
            }

            String^ number = (row + 1).ToString();
            System::Drawing::SizeF size = e->Graphics->MeasureString(number, box->Font);
            e->Graphics->DrawString(number, box->Font,
                                    row == caretLine ? here : quiet,
                                    panel->Width - size.Width - 6,
                                    static_cast<float>(where.Y));
        }
    }

    TextBox^ ReadOnlyBox() {
        TextBox^ box = gcnew TextBox();
        box->Dock = DockStyle::Fill;
        box->Multiline = true;
        box->ReadOnly = true;
        box->ScrollBars = ScrollBars::Both;
        box->WordWrap = false;
        box->Font = gcnew System::Drawing::Font("Consolas", 10.0f);
        return box;
    }

    void SayDebugTab(String^ assembly) {
        array<Byte>^ bytes = Utf8Of(assembly == nullptr ? "" : assembly);
        pin_ptr<Byte> pinned = &bytes[0];
        String^ found = TakeUtf8(rstudio_describe_build(reinterpret_cast<const char*>(pinned)));

        array<Byte>^ archBytes = Utf8Of(arch_ == nullptr ? "" : arch_);
        pin_ptr<Byte> archPin = &archBytes[0];
        String^ note = TakeUtf8(rstudio_debug_note(
            rstudio_resolve(toolKind_, LanguageNow()),
            reinterpret_cast<const char*>(archPin)));

        debug_->Text = String::Join(
            "\r\n",
            gcnew array<String^>{note->Replace("\n", "\r\n"), "",
                                 found->Replace("\n", "\r\n")});
    }

    void RefreshDebugTab() {
        SayDebugTab(assembly_->Text->Replace("\r\n", "\n"));
    }

    int languageChoice_;

    int LanguageNow() {
        if (languageChoice_ >= 0) return languageChoice_;
        array<Byte>^ bytes = Utf8Of(path_ == nullptr ? "" : path_);
        pin_ptr<Byte> pinned = &bytes[0];
        return rstudio_language_for(reinterpret_cast<const char*>(pinned));
    }

    int DialectNow() { return rstudio_dialect_for(LanguageNow()); }

    void OnLayOut(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }
        array<Byte>^ bytes = Utf8Of(text_->Text->Replace("\r\n", "\n"));
        pin_ptr<Byte> pinned = &bytes[0];

        String^ laid = TakeUtf8(rstudio_reindent(reinterpret_cast<const char*>(pinned),
                                             indentWidth_, indentTabs_, indentCase_,
                                             DialectNow()));

        int caret = text_->SelectionStart;
        int length = text_->SelectionLength;

        array<String^>^ was = text_->Lines;
        array<String^>^ now = laid->Split('\n');

        int howManyNow = now->Length;
        if (howManyNow > 0 && now[howManyNow - 1]->Length == 0) --howManyNow;

        if (length > 0 && howManyNow == was->Length) {
            int first = text_->GetLineFromCharIndex(caret);
            int last = text_->GetLineFromCharIndex(caret + length - 1);
            if (last >= was->Length) last = was->Length - 1;

            for (int row = first; row <= last; ++row) was[row] = now[row];

            text_->Text = String::Join("\r\n", was);
            text_->SelectionStart = Math::Min(caret, text_->TextLength);
            text_->SelectionLength = 0;
            Recolour();
            int howMany = last - first + 1;
            what_->Text = String::Format("laid out {0} line{1} of the selection",
                                         howMany, howMany == 1 ? "" : "s");
            return;
        }

        text_->Text = laid->Replace("\n", "\r\n");
        text_->SelectionStart = Math::Min(caret, text_->TextLength);
        Recolour();
        what_->Text = String::Format("laid out - {0} lines", text_->Lines->Length);
    }

    void OnKeyDown(Object^, KeyEventArgs^ e) {
        if (e->KeyCode == Keys::Tab && !e->Control && !e->Shift) {
            e->SuppressKeyPress = true;

            int row = CaretRow();
            int column = CaretColumn();
            String^ line = text_->Lines->Length > row ? text_->Lines[row] : "";
            int lead = 0;
            while (lead < line->Length && (line[lead] == ' ' || line[lead] == '\t')) ++lead;

            if (column <= lead) {
                Realign(row);
                text_->SelectionStart = text_->GetFirstCharIndexFromLine(row) +
                                        LeadingOf(row);
            } else if (indentTabs_ != 0) {
                text_->SelectedText = "\t";
            } else {
                text_->SelectedText = gcnew String(' ', indentWidth_);
            }
            return;
        }

        if (e->KeyCode != Keys::Enter || e->Control || e->Shift) return;

        int caret = text_->SelectionStart;
        int row = text_->GetLineFromCharIndex(caret);
        int column = caret - text_->GetFirstCharIndexFromLine(row);

        array<Byte>^ bytes = Utf8Of(text_->Text->Replace("\r\n", "\n"));
        pin_ptr<Byte> pinned = &bytes[0];

        String^ lead = TakeUtf8(rstudio_indent_after_newline(
            reinterpret_cast<const char*>(pinned), row, column, indentWidth_, indentTabs_,
            indentCase_, DialectNow()));

        e->SuppressKeyPress = true;
        text_->SelectedText = "\r\n" + lead;
    }

    void BeginColouring() {
        colouring_ = true;
        if (text_ != nullptr && text_->IsHandleCreated)
            rstudio_undo_suspend(text_->Handle.ToPointer());
    }

    void EndColouring() {
        if (text_ != nullptr && text_->IsHandleCreated)
            rstudio_undo_resume(text_->Handle.ToPointer());
        colouring_ = false;
    }

    void PaintRow(int row, bool on) {
        if (text_ == nullptr || row < 0 || row >= text_->Lines->Length) return;
        int start = text_->GetFirstCharIndexFromLine(row);
        int length = text_->Lines[row]->Length;
        if (row < text_->Lines->Length - 1) length += 1;
        int caret = text_->SelectionStart;
        int chosen = text_->SelectionLength;
        bool touched = text_->Modified;

        BeginColouring();
        text_->Select(start, length);
        text_->SelectionBackColor =
            on ? System::Drawing::Color::FromArgb(214, 234, 255) : text_->BackColor;
        text_->Select(caret, chosen);
        text_->Modified = touched;
        EndColouring();
    }

    void ShowStoppedLine(int row) {
        if (highlightRow_ != row) {
            if (highlightRow_ >= 0) PaintRow(highlightRow_, false);
            highlightRow_ = row;
            if (row >= 0) PaintRow(row, true);
        }
        PlaceStopBar();
    }

    void MakeStopBar() {
        if (stopBar_ != nullptr) return;
        stopBar_ = gcnew Panel();
        stopBar_->BackColor = System::Drawing::Color::FromArgb(214, 234, 255);
        stopBar_->Visible = false;
        stopBar_->TabStop = false;
        Controls->Add(stopBar_);
        stopBar_->BringToFront();
    }

    void PlaceStopBar() {
        MakeStopBar();
        if (text_ == nullptr || highlightRow_ < 0 || !text_->IsHandleCreated) {
            stopBar_->Visible = false;
            return;
        }

        if (path_ == nullptr || stopFile_ == nullptr ||
            System::IO::Path::GetFileName(stopFile_) != System::IO::Path::GetFileName(path_)) {
            stopBar_->Visible = false;
            return;
        }
        if (highlightRow_ >= text_->Lines->Length) { stopBar_->Visible = false; return; }

        int index = text_->GetFirstCharIndexFromLine(highlightRow_);
        System::Drawing::Point where = text_->GetPositionFromCharIndex(index);
        int height = System::Windows::Forms::TextRenderer::MeasureText("Ay", text_->Font).Height;

        if (where.Y < -height || where.Y > text_->ClientSize.Height) {
            stopBar_->Visible = false;
            return;
        }

        int after = index + text_->Lines[highlightRow_]->Length;
        System::Drawing::Point ends = text_->GetPositionFromCharIndex(after);
        int from = ends.Y == where.Y ? ends.X : where.X;

        System::Drawing::Point corner =
            PointToClient(text_->PointToScreen(System::Drawing::Point(from, where.Y)));
        int width = text_->ClientSize.Width - from;
        if (width <= 0) { stopBar_->Visible = false; return; }

        stopBar_->SetBounds(corner.X, corner.Y, width, height);
        stopBar_->Visible = true;
        stopBar_->BringToFront();
    }

    void Recolour() {
        if (colouring_) return;
        if (text_ == nullptr || !text_->IsHandleCreated) return;
        BeginColouring();

        array<String^>^ all = text_->Lines;
        int language = LanguageNow();

        int top = text_->GetLineFromCharIndex(
            text_->GetCharIndexFromPosition(System::Drawing::Point(1, 1)));
        int bottom = text_->GetLineFromCharIndex(text_->GetCharIndexFromPosition(
            System::Drawing::Point(1, Math::Max(1, text_->ClientSize.Height - 2))));
        int deep = Math::Max(1, bottom - top + 1);
        int from = Math::Max(0, top - deep);
        int to = Math::Min(all->Length - 1, bottom + deep);

        bool touched = text_->Modified;
        int caret = text_->SelectionStart;
        int length = text_->SelectionLength;

        Spot scrolled;
        stateGood_ = false;
        Tell(text_->Handle, kWhereScrolled, IntPtr::Zero, scrolled);
        Drawing(text_, false);

        int state = 0;
        for (int row = 0; row < from; ++row) {
            array<Byte>^ above = Utf8Of(all[row]);
            pin_ptr<Byte> abovePin = &above[0];
            array<Byte>^ ignored = gcnew array<Byte>(above->Length);
            pin_ptr<Byte> ignoredPin = &ignored[0];
            rstudio_highlight(reinterpret_cast<const char*>(abovePin), language, &state,
                          ignoredPin, ignored->Length);
        }

        if (from <= to) {
            int start = text_->GetFirstCharIndexFromLine(from);
            int end = to + 1 < all->Length ? text_->GetFirstCharIndexFromLine(to + 1)
                                           : text_->TextLength;
            if (start >= 0 && end > start) {
                text_->Select(start, end - start);
                text_->SelectionColor = System::Drawing::Color::Black;
            }
        }

        for (int row = from; row <= to; ++row) {
            array<Byte>^ bytes = Utf8Of(all[row]);
            pin_ptr<Byte> linePin = &bytes[0];

            array<Byte>^ kinds = gcnew array<Byte>(bytes->Length);
            pin_ptr<Byte> kindPin = &kinds[0];

            int howMany = rstudio_highlight(reinterpret_cast<const char*>(linePin), language,
                                        &state, kindPin, kinds->Length);

            int at = text_->GetFirstCharIndexFromLine(row);
            if (at < 0) break;

            int column = 0;
            int byte = 0;
            while (byte < howMany) {
                Byte kind = kinds[byte];
                int end = byte;
                while (end < howMany && kinds[end] == kind) ++end;

                int width =
                    System::Text::Encoding::UTF8->GetString(bytes, byte, end - byte)->Length;
                if (width > 0 && kind != RSTUDIO_KIND_NORMAL) {
                    text_->Select(at + column, width);
                    text_->SelectionColor = ColourOf(kind);
                }
                column += width;
                byte = end;
            }
        }

        text_->Select(caret, length);
        text_->SelectionColor = System::Drawing::Color::Black;
        Tell(text_->Handle, kScrollTo, IntPtr::Zero, scrolled);
        text_->Modified = touched;
        Drawing(text_, true);
        EndColouring();
    }

    void OnSettled(Object^, EventArgs^) {
        settle_->Stop();
        Recolour();
    }

    void RecolourLine(int row) {
        if (colouring_ || text_ == nullptr || !text_->IsHandleCreated) return;

        array<String^>^ all = text_->Lines;
        if (row < 0 || row >= all->Length) return;

        BeginColouring();
        int language = LanguageNow();

        int state = 0;
        if (stateGood_ && stateRow_ == row) {
            state = stateAt_;
        } else {
            for (int above = 0; above < row; ++above) {
                array<Byte>^ bytes = Utf8Of(all[above]);
                pin_ptr<Byte> linePin = &bytes[0];
                array<Byte>^ kinds = gcnew array<Byte>(bytes->Length);
                pin_ptr<Byte> kindPin = &kinds[0];
                rstudio_highlight(reinterpret_cast<const char*>(linePin), language, &state,
                              kindPin, kinds->Length);
            }
            stateGood_ = true;
            stateRow_ = row;
            stateAt_ = state;
        }

        bool touched = text_->Modified;
        int caret = text_->SelectionStart;
        int length = text_->SelectionLength;

        int at = text_->GetFirstCharIndexFromLine(row);
        if (at >= 0) {
            text_->Select(at, all[row]->Length);
            text_->SelectionColor = System::Drawing::Color::Black;

            array<Byte>^ bytes = Utf8Of(all[row]);
            pin_ptr<Byte> linePin = &bytes[0];
            array<Byte>^ kinds = gcnew array<Byte>(bytes->Length);
            pin_ptr<Byte> kindPin = &kinds[0];
            int howMany = rstudio_highlight(reinterpret_cast<const char*>(linePin), language,
                                        &state, kindPin, kinds->Length);

            int column = 0;
            int byte = 0;
            while (byte < howMany) {
                Byte kind = kinds[byte];
                int end = byte;
                while (end < howMany && kinds[end] == kind) ++end;

                int width =
                    System::Text::Encoding::UTF8->GetString(bytes, byte, end - byte)->Length;
                if (width > 0 && kind != RSTUDIO_KIND_NORMAL) {
                    text_->Select(at + column, width);
                    text_->SelectionColor = ColourOf(kind);
                }
                column += width;
                byte = end;
            }
        }

        text_->Select(caret, length);
        text_->SelectionColor = System::Drawing::Color::Black;
        text_->Modified = touched;
        EndColouring();
    }

    System::Drawing::Color ColourOf(Byte kind) {
        switch (kind) {
            case RSTUDIO_KIND_KEYWORD: return System::Drawing::Color::Blue;
            case RSTUDIO_KIND_TYPE:    return System::Drawing::Color::Teal;
            case RSTUDIO_KIND_STRING:  return System::Drawing::Color::FromArgb(0, 128, 0);
            case RSTUDIO_KIND_CHAR:    return System::Drawing::Color::FromArgb(0, 128, 0);
            case RSTUDIO_KIND_COMMENT: return System::Drawing::Color::Gray;
            case RSTUDIO_KIND_PREPROC: return System::Drawing::Color::Purple;
            case RSTUDIO_KIND_NUMBER:  return System::Drawing::Color::FromArgb(180, 100, 0);
            case RSTUDIO_KIND_LABEL:   return System::Drawing::Color::FromArgb(150, 120, 0);
            default:               return System::Drawing::Color::Black;
        }
    }

    void OnUndo(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }

        if (!text_->CanUndo) { what_->Text = "nothing to undo"; return; }
        text_->Undo();
    }
    void OnRedo(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }
        if (!text_->CanRedo) { what_->Text = "nothing to redo"; return; }
        text_->Redo();
    }
    void OnCut(Object^, EventArgs^) { if (text_ != nullptr) text_->Cut(); }
    void OnCopy(Object^, EventArgs^) { if (text_ != nullptr) text_->Copy(); }
    void OnPaste(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }

        if (!text_->CanPaste(DataFormats::GetFormat(DataFormats::Text))) {
            what_->Text = "there is nothing to paste";
            return;
        }
        text_->Paste();
        Recolour();
    }
    void OnSelectAll(Object^, EventArgs^) { if (text_ != nullptr) text_->SelectAll(); }

    void OnFind(Object^, EventArgs^) {
        String^ want = Ask("Find", needle_);

        if (want == nullptr || want->Length == 0) {
            what_->Text = "nothing looked for";
            return;
        }
        needle_ = want;
        Seek(CaretRow(), ByteColumn(CaretRow(), CaretColumn()), true);
    }

    void OnFindNext(Object^, EventArgs^) {
        if (needle_ == nullptr) { OnFind(nullptr, nullptr); return; }
        Seek(CaretRow(), ByteColumn(CaretRow(), CaretColumn()) + 1, true);
    }

    void OnFindPrevious(Object^, EventArgs^) {
        if (needle_ == nullptr) { OnFind(nullptr, nullptr); return; }
        Seek(CaretRow(), ByteColumn(CaretRow(), CaretColumn()), false);
    }

    void Seek(int row, int column, bool forwards) {
        if (text_ == nullptr) return;
        array<Byte>^ text = WholeText();
        pin_ptr<Byte> textPin = &text[0];
        array<Byte>^ needle = Utf8Of(needle_);
        pin_ptr<Byte> needlePin = &needle[0];

        int foundRow = 0, foundColumn = 0;
        int found = forwards ? rstudio_find_next(reinterpret_cast<const char*>(textPin),
                                             reinterpret_cast<const char*>(needlePin), row,
                                             column, &foundRow, &foundColumn)
                             : rstudio_find_previous(reinterpret_cast<const char*>(textPin),
                                                 reinterpret_cast<const char*>(needlePin), row,
                                                 column, &foundRow, &foundColumn);
        if (found == 0) {
            what_->Text = needle_ + " is not in this file";
            return;
        }

        int at = text_->GetFirstCharIndexFromLine(foundRow) +
                 CharacterColumn(foundRow, foundColumn);
        text_->Select(at, needle_->Length);
        text_->ScrollToCaret();

        Recolour();
        text_->Focus();
        what_->Text = String::Format("{0} - line {1}", needle_, foundRow + 1);
    }

    void OnReplace(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }

        String^ want = Ask("Replace what", needle_);
        if (want == nullptr || want->Length == 0) {
            what_->Text = "nothing replaced";
            return;
        }
        String^ with = Ask("Replace \"" + want + "\" with", "");
        if (with == nullptr) {
            what_->Text = "nothing replaced";
            return;
        }

        array<Byte>^ text = WholeText();
        pin_ptr<Byte> textPin = &text[0];
        array<Byte>^ needle = Utf8Of(want);
        pin_ptr<Byte> needlePin = &needle[0];
        array<Byte>^ replacement = Utf8Of(with);
        pin_ptr<Byte> replacementPin = &replacement[0];

        int howMany = 0;
        String^ changed = TakeUtf8(rstudio_replace_all(
            reinterpret_cast<const char*>(textPin), reinterpret_cast<const char*>(needlePin),
            reinterpret_cast<const char*>(replacementPin), &howMany));

        if (howMany == 0) {
            what_->Text = want + " is not in this file";
            return;
        }

        int caret = text_->SelectionStart;
        text_->Text = changed->Replace("\n", "\r\n");
        text_->SelectionStart = Math::Min(caret, text_->TextLength);
        needle_ = with;
        Recolour();
        what_->Text = String::Format("{0} change{1} - Ctrl-Z puts them back", howMany,
                                     howMany == 1 ? "" : "s");
    }

    void Realign(int row) {
        if (row < 0 || row >= text_->Lines->Length) return;

        String^ line = text_->Lines[row];
        int lead = 0;
        while (lead < line->Length && (line[lead] == ' ' || line[lead] == '\t')) ++lead;

        array<Byte>^ text = WholeText();
        pin_ptr<Byte> textPin = &text[0];
        String^ want = TakeUtf8(rstudio_indent_for(reinterpret_cast<const char*>(textPin), row,
                                               indentWidth_, indentTabs_, indentCase_,
                                               DialectNow()));
        if (want == line->Substring(0, lead)) return;

        int start = text_->GetFirstCharIndexFromLine(row);
        int caret = text_->SelectionStart;

        text_->Select(start, lead);
        text_->SelectedText = want;
        text_->SelectionStart = Math::Max(0, Math::Min(caret + want->Length - lead,
                                                       text_->TextLength));
    }

    void OnKeyUp(Object^, KeyEventArgs^) {

        int caret = text_->SelectionStart;
        if (caret <= 0 || caret > text_->TextLength) return;

        wchar_t just = text_->Text[caret - 1];
        if (just != '}' && just != '#' && just != ':') return;

        int row = text_->GetLineFromCharIndex(caret - 1);
        String^ line = text_->Lines[row];
        int column = (caret - 1) - text_->GetFirstCharIndexFromLine(row);

        if (just != ':') {

            for (int i = 0; i < column && i < line->Length; ++i)
                if (line[i] != ' ' && line[i] != '\t') return;
        }
        Realign(row);
    }

    void OnCaretMoved(Object^, EventArgs^) {

        if (colouring_) return;
        stateGood_ = false;

        int caret = text_->SelectionStart;
        int row = text_->GetLineFromCharIndex(caret);
        where_->Text =
            String::Format("{0}:{1}", row + 1, caret - text_->GetFirstCharIndexFromLine(row) + 1);

        Sheet^ sheet = Current();
        if (sheet != nullptr) sheet->gutter->Invalidate();
    }

    void OnOpenProject(Object^, EventArgs^) {
        FolderBrowserDialog^ pick = gcnew FolderBrowserDialog();
        if (pick->ShowDialog() != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "no project opened";
            return;
        }
        LoadProject(pick->SelectedPath);
    }

    void LoadProject(String^ where) {

        paneMode_ = PaneMode::PaneProject;
        bool named = System::IO::File::Exists(where);

        String^ directory = named ? System::IO::Path::GetDirectoryName(where) : where;
        projectDirectory_ = directory;
        tree_->Nodes->Clear();

        array<Byte>^ bytes = Utf8Of(where);
        pin_ptr<Byte> pinned = &bytes[0];

        array<Byte>^ error = gcnew array<Byte>(512);
        pin_ptr<Byte> errorPin = &error[0];

        int loaded = rstudio_project_load(project_, reinterpret_cast<const char*>(pinned),
                                      reinterpret_cast<char*>(errorPin), error->Length);
        if (loaded == 0) {
            String^ why = FromUtf8(reinterpret_cast<const char*>(errorPin));

            array<Byte>^ dirBytes = Utf8Of(directory);
            pin_ptr<Byte> dirPin = &dirBytes[0];
            if (why->Length == 0 &&
                rstudio_begin_from_what_is_there(project_,
                                             reinterpret_cast<const char*>(dirPin)) != 0) {
                FillTree();
                indentWidth_ = rstudio_project_indent_width(project_);
                indentTabs_ = rstudio_project_indent_tabs(project_);
                indentCase_ = rstudio_project_case_indent(project_);
                toolKind_ = rstudio_project_toolchain(project_) != RSTUDIO_TOOL_AUTO
                        ? rstudio_project_toolchain(project_) : rstudio_default_compiler();
                config_ = rstudio_configuration();
                arch_ = FromUtf8(rstudio_project_arch(project_));
                ShowChoices();
                rstudio_remember_project(reinterpret_cast<const char*>(pinned));
                RefreshRecent();
                what_->Text = FromUtf8(rstudio_outcome_message(project_));
                SayWhere();
                RefreshTitle();
                return;
            }

            what_->Text = why->Length > 0 ? why : "no RStudio.json in that directory";

            rstudio_project_set_root(project_, reinterpret_cast<const char*>(pinned));
            SayWhere();
            return;
        }

        FillTree();

        indentWidth_ = rstudio_project_indent_width(project_);
        indentTabs_ = rstudio_project_indent_tabs(project_);
        indentCase_ = rstudio_project_case_indent(project_);
        toolKind_ = rstudio_project_toolchain(project_) != RSTUDIO_TOOL_AUTO
                        ? rstudio_project_toolchain(project_) : rstudio_default_compiler();
        config_ = rstudio_configuration();
        arch_ = FromUtf8(rstudio_project_arch(project_));
        ShowChoices();

        array<Byte>^ opened = Utf8Of(directory);
        pin_ptr<Byte> openedPin = &opened[0];
        rstudio_remember_project(reinterpret_cast<const char*>(openedPin));
        RefreshRecent();

        what_->Text = String::Format("ready - {0}, {1} groups",
                                     FromUtf8(rstudio_project_name(project_)),
                                     rstudio_project_groups(project_));
        SayWhere();
        RefreshTitle();

        // The project's own file comes to the front whichever way the
        // project was opened - Start() asks the same after the command
        // line's files, and skips this when one of them is named.
        if (started_) {
            String^ said = what_->Text;
            OpenFirstOfProject();
            what_->Text = said;
        }
    }

    void PaneFollowsTabs() {

        if (project_ == nullptr || tree_ == nullptr) return;

        FillTree();
    }

    void FillTree() {
        tree_->Nodes->Clear();

        if (paneMode_ == PaneMode::PaneFiles || rstudio_project_loaded(project_) == 0) {
            for (int i = 0; i < sheets_->Count; ++i) {
                String^ full = sheets_[i]->path;

                String^ shown = (full == nullptr || full->Length == 0)
                                    ? "untitled"
                                    : System::IO::Path::GetFileName(full);
                TreeNode^ leaf = gcnew TreeNode(shown);
                leaf->Tag = full;
                tree_->Nodes->Add(leaf);
            }
            return;
        }

        int groups = rstudio_project_groups(project_);
        for (int group = 0; group < groups; ++group) {
            TreeNode^ node = gcnew TreeNode(FromUtf8(rstudio_project_group_name(project_, group)));
            int files = rstudio_project_files(project_, group);
            for (int file = 0; file < files; ++file) {
                String^ relative = FromUtf8(rstudio_project_file(project_, group, file));
                TreeNode^ leaf = gcnew TreeNode(relative);

                array<Byte>^ rel = Utf8Of(relative);
                pin_ptr<Byte> relPin = &rel[0];
                leaf->Tag = FromUtf8(
                    rstudio_project_absolute(project_, reinterpret_cast<const char*>(relPin)));
                node->Nodes->Add(leaf);
            }
            tree_->Nodes->Add(node);
        }
        tree_->ExpandAll();

        indentWidth_ = rstudio_project_indent_width(project_);
        indentTabs_ = rstudio_project_indent_tabs(project_);
        indentCase_ = rstudio_project_case_indent(project_);
        toolKind_ = rstudio_project_toolchain(project_) != RSTUDIO_TOOL_AUTO
                        ? rstudio_project_toolchain(project_) : rstudio_default_compiler();
        config_ = rstudio_configuration();
        arch_ = FromUtf8(rstudio_project_arch(project_));
        ShowChoices();

        what_->Text = String::Format("ready - {0}, {1} groups",
                                     FromUtf8(rstudio_project_name(project_)), groups);
    }

    String^ TargetFile() {
        if (tree_->SelectedNode != nullptr && tree_->SelectedNode->Tag != nullptr)
            return safe_cast<String^>(tree_->SelectedNode->Tag);
        return path_;
    }

    String^ GroupUnderCursor() {
        TreeNode^ node = tree_->SelectedNode;
        while (node != nullptr && node->Tag != nullptr) node = node->Parent;
        return node == nullptr ? "Sources" : node->Text;
    }

    bool Did(int outcome) {
        what_->Text = FromUtf8(rstudio_outcome_message(project_));
        return outcome != 0;
    }

    String^ OutcomePath() { return FromUtf8(rstudio_outcome_path(project_)); }

    String^ GroupForFile(String^ name) {
        if (name == nullptr || name->Length == 0) return "";
        array<Byte>^ leaf = Utf8Of(System::IO::Path::GetFileName(name));
        pin_ptr<Byte> pinned = &leaf[0];
        return FromUtf8(rstudio_group_for_file(reinterpret_cast<const char*>(pinned)));
    }

    void OnNewFile(Object^, EventArgs^) {
        String^ root = RootNow();

        // No project open: a single program is made under {app}\programs rather
        // than in the read-only directory the shortcut started the editor in.
        if (root == nullptr || root->Length == 0) {
            String^ programs = ProgramsDir();
            String^ only = Ask("New program (name, or one directory and a name)",
                               "It will be made in " + programs, "");
            if (only == nullptr || only->Length == 0) { what_->Text = "nothing made"; return; }
            // The extension picks the compiler; a name without one gets the
            // chosen compiler's, and C when the choice is automatic.
            if (System::IO::Path::GetFileName(only)->IndexOf('.') < 0)
                only += toolKind_ == RSTUDIO_TOOL_SHC ? ".shl"
                      : toolKind_ == RSTUDIO_TOOL_CC1 || toolKind_ == RSTUDIO_TOOL_AUTO ? ".c"
                                                                                        : ".cpp";
            String^ target = System::IO::Path::Combine(programs, only);
            if (System::IO::File::Exists(target)) {
                what_->Text = only + " is already there"; return;
            }
            try {
                String^ parent = System::IO::Path::GetDirectoryName(target);
                if (parent != nullptr && parent->Length > 0)
                    System::IO::Directory::CreateDirectory(parent);
                System::IO::File::Create(target)->Close();
            } catch (Exception^ ex) {
                what_->Text = "could not make " + only + " - " + ex->Message; return;
            }
            OpenPath(target);
            what_->Text = only + " made in " + programs;
            return;
        }

        String^ name = Ask("New file (name, or one directory and a name)",
                           "It will be made in " + root,
                           "");
        if (name == nullptr || name->Length == 0) { what_->Text = "nothing made"; return; }

        array<Byte>^ relative = Utf8Of(name);
        pin_ptr<Byte> relativePin = &relative[0];

        String^ wanted = GroupForFile(name);
        if (wanted->Length == 0) wanted = GroupUnderCursor();
        array<Byte>^ group = Utf8Of(wanted);
        pin_ptr<Byte> groupPin = &group[0];

        if (!Did(rstudio_create_file(project_, reinterpret_cast<const char*>(relativePin),
                                 reinterpret_cast<const char*>(groupPin), toolKind_)))
            return;

        FillTree();
        OpenPath(OutcomePath());
    }

    void OnFont(Object^, EventArgs^) {
        FontDialog^ pick = gcnew FontDialog();
        pick->Font = codeFont_;
        pick->FixedPitchOnly = true;
        pick->ShowEffects = false;
        pick->ShowColor = false;
        pick->MinSize = 6;
        pick->MaxSize = 48;
        if (pick->ShowDialog(this) != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "font unchanged";
            return;
        }

        UseFont(pick->Font);

        String^ said = String::Format(
            System::Globalization::CultureInfo::InvariantCulture, "{0} {1}",
            codeFont_->FontFamily->Name, codeFont_->SizeInPoints);
        array<Byte>^ bytes = Utf8Of(said);
        pin_ptr<Byte> pinned = &bytes[0];
        rstudio_remember_code_font(reinterpret_cast<const char*>(pinned));
        what_->Text = said;
    }

    void UseFont(System::Drawing::Font^ chosen) {
        codeFont_ = chosen;
        for each (Sheet^ sheet in sheets_) {
            bool touched = sheet->box->Modified;
            sheet->box->Font = codeFont_;
            sheet->box->Modified = touched;
            if (sheet->gutter != nullptr) sheet->gutter->Invalidate();
        }
        Recolour();
        PlaceStopBar();
    }

    void OnToggleNumbers(Object^, EventArgs^) {
        numbers_ = !numbers_;
        numbersItem_->Checked = numbers_;
        for each (Sheet^ sheet in sheets_)
            if (sheet->gutter != nullptr) sheet->gutter->Visible = numbers_;
        what_->Text = numbers_ ? "line numbers on" : "line numbers off";
    }

    void OnTogglePane(Object^, EventArgs^) {
        bool hidden = !upper_->Panel1Collapsed;
        upper_->Panel1Collapsed = hidden;
        paneItem_->Checked = !hidden;
        what_->Text = hidden ? "project pane hidden" : "project pane showing";
    }

    void OnTogglePanel(Object^, EventArgs^) {
        bool hidden = !outer_->Panel2Collapsed;
        outer_->Panel2Collapsed = hidden;
        panelItem_->Checked = !hidden;
        what_->Text = hidden ? "bottom panel hidden" : "bottom panel showing";
    }

    void OnNewBuffer(Object^, EventArgs^) {

        paneMode_ = PaneMode::PaneFiles;
        MakeSheet(nullptr, "");
        what_->Text = "new file - Ctrl+S names it";
    }

    void OnNextFile(Object^, EventArgs^) { StepFile(1); }
    void OnPreviousFile(Object^, EventArgs^) { StepFile(-1); }

    void StepFile(int by) {
        int count = files_->TabPages->Count;
        if (count < 2) { what_->Text = "only one file is open"; return; }
        int at = (files_->SelectedIndex + count + by) % count;
        files_->SelectedTab = files_->TabPages[at];
    }

    void OnRenameFile(Object^, EventArgs^) {
        String^ target = TargetFile();
        if (target == nullptr) { what_->Text = "no file to rename"; return; }

        array<Byte>^ was = Utf8Of(target);
        pin_ptr<Byte> wasPin = &was[0];
        // Offered by its name in the project, or by its bare name when it
        // is not in one - which is what the new name is taken relative to.
        String^ shown = rstudio_project_holds(project_, reinterpret_cast<const char*>(wasPin)) != 0
                            ? FromUtf8(rstudio_project_relative(project_, reinterpret_cast<const char*>(wasPin)))
                            : System::IO::Path::GetFileName(target);

        String^ name = Ask("Rename " + shown + " to", shown);
        if (name == nullptr || name->Length == 0) { what_->Text = "not renamed"; return; }

        array<Byte>^ from = Utf8Of(target);
        pin_ptr<Byte> fromPin = &from[0];
        array<Byte>^ to = Utf8Of(name);
        pin_ptr<Byte> toPin = &to[0];

        if (!Did(rstudio_rename_file(project_, reinterpret_cast<const char*>(fromPin),
                                 reinterpret_cast<const char*>(toPin))))
            return;

        String^ now = OutcomePath();
        for (int i = 0; i < sheets_->Count; ++i) {
            if (sheets_[i]->path == nullptr) continue;
            if (!SamePath(sheets_[i]->path, target)) continue;
            sheets_[i]->path = now;
            MarkTab(sheets_[i]);
            PaneFollowsTabs();
        }
        if (SamePath(path_, target)) {
            path_ = now;
            RefreshTitle();
            SayBuild();
        }

        String^ wasKey = OneName(target);
        System::Collections::Generic::List<int>^ hadBreaks = nullptr;
        if (breaks_->TryGetValue(wasKey, hadBreaks)) {
            breaks_->Remove(wasKey);
            breakNames_->Remove(wasKey);
            String^ nowKey = OneName(now);
            breaks_[nowKey] = hadBreaks;
            breakNames_[nowKey] = now;
            Sheet^ showing = Current();
            if (showing != nullptr && showing->gutter != nullptr)
                showing->gutter->Invalidate();
        }

        FillTree();
    }

    void OnDeleteFile(Object^, EventArgs^) {
        String^ target = TargetFile();
        if (target == nullptr) { what_->Text = "no file to delete"; return; }

        System::Windows::Forms::DialogResult answer = MessageBox::Show(
            this, "Delete " + System::IO::Path::GetFileName(target) + " from disk?",
            "Delete", MessageBoxButtons::YesNo, MessageBoxIcon::Warning,
            MessageBoxDefaultButton::Button2);
        if (answer != System::Windows::Forms::DialogResult::Yes) {
            what_->Text = "not deleted";
            return;
        }

        array<Byte>^ path = Utf8Of(target);
        pin_ptr<Byte> pathPin = &path[0];
        if (!Did(rstudio_delete_file(project_, reinterpret_cast<const char*>(pathPin)))) return;

        for (int i = sheets_->Count - 1; i >= 0; --i) {
            if (sheets_[i]->path == nullptr) continue;
            if (!SamePath(sheets_[i]->path, target)) continue;
            Sheet^ sheet = sheets_[i];
            sheets_->Remove(sheet);
            files_->TabPages->Remove(sheet->page);
            PaneFollowsTabs();
        }

        String^ key = OneName(target);
        breaks_->Remove(key);
        breakNames_->Remove(key);

        if (sheets_->Count == 0) MakeSheet(nullptr, "");
        OnSheetChanged(nullptr, nullptr);
        FillTree();
    }

    void OnMoveToGroup(Object^, EventArgs^) {
        String^ target = TargetFile();
        if (target == nullptr) { what_->Text = "no file to move"; return; }

        String^ group = Ask("Move to group", GroupUnderCursor());
        if (group == nullptr || group->Length == 0) { what_->Text = "not moved"; return; }

        array<Byte>^ path = Utf8Of(target);
        pin_ptr<Byte> pathPin = &path[0];
        array<Byte>^ into = Utf8Of(group);
        pin_ptr<Byte> intoPin = &into[0];

        if (Did(rstudio_move_to_group(project_, reinterpret_cast<const char*>(pathPin),
                                  reinterpret_cast<const char*>(intoPin))))
            FillTree();
    }

    void OnAddThisFile(Object^, EventArgs^) {
        if (path_ == nullptr) {
            what_->Text = "save the file first, so it has a name";
            return;
        }

        String^ wanted = GroupForFile(path_);
        if (wanted->Length == 0) wanted = "Sources";

        String^ group = Ask("Add to group", wanted);
        if (group == nullptr || group->Length == 0) { what_->Text = "not added"; return; }

        array<Byte>^ path = Utf8Of(path_);
        pin_ptr<Byte> pathPin = &path[0];
        array<Byte>^ into = Utf8Of(group);
        pin_ptr<Byte> intoPin = &into[0];

        if (Did(rstudio_add_existing(project_, reinterpret_cast<const char*>(pathPin),
                                 reinterpret_cast<const char*>(intoPin))))
            FillTree();
    }

    enum class PaneMode { PaneProject, PaneFiles };
    PaneMode paneMode_;
    // Set once Start() has opened what the command line and the project
    // asked for; LoadProject opens the project's file only after that.
    bool started_ = false;

    void OnRemoveFromProject(Object^, EventArgs^) {
        if (path_ == nullptr) {
            what_->Text = "this buffer has no name to look for";
            return;
        }

        array<Byte>^ path = Utf8Of(path_);
        pin_ptr<Byte> pathPin = &path[0];

        if (Did(rstudio_remove_from_project(project_, reinterpret_cast<const char*>(pathPin))))
            FillTree();
    }

    // The two lists a project keeps beside its groups, edited as one line
    // each with ';' between the entries, and written back at once.
    void OnProjectIncludes(Object^, EventArgs^) {
        if (rstudio_project_loaded(project_) == 0) {
            what_->Text = "there is no project - make one first";
            return;
        }
        String^ line = Ask("Include paths", "relative to " + FromUtf8(rstudio_project_root(project_)) +
                           ", ';' between them",
                           FromUtf8(rstudio_project_includes(project_)));
        if (line == nullptr) { what_->Text = "include paths unchanged"; return; }
        array<Byte>^ bytes = Utf8Of(line);
        pin_ptr<Byte> pinned = &bytes[0];
        if (Did(rstudio_project_set_includes(project_, reinterpret_cast<const char*>(pinned))))
            what_->Text = "include paths: " + FromUtf8(rstudio_project_includes(project_)) +
                          " - " + what_->Text;
    }

    void OnProjectLibraries(Object^, EventArgs^) {
        if (rstudio_project_loaded(project_) == 0) {
            what_->Text = "there is no project - make one first";
            return;
        }
        String^ line = Ask("Libraries", "relative to " + FromUtf8(rstudio_project_root(project_)) +
                           ", ';' between them, linked after the objects",
                           FromUtf8(rstudio_project_libraries(project_)));
        if (line == nullptr) { what_->Text = "libraries unchanged"; return; }
        array<Byte>^ bytes = Utf8Of(line);
        pin_ptr<Byte> pinned = &bytes[0];
        if (Did(rstudio_project_set_libraries(project_, reinterpret_cast<const char*>(pinned))))
            what_->Text = "libraries: " + FromUtf8(rstudio_project_libraries(project_)) +
                          " - " + what_->Text;
    }

    // The installation's settings.json: where the shipped headers are.
    void OnHeaderDirs(Object^, EventArgs^) {
        String^ file = FromUtf8(rstudio_install_file());
        if (file->Length == 0) { what_->Text = "no installation directory to keep this in"; return; }
        String^ include = Ask("cxx1's headers (include)", "kept in " + file,
                              FromUtf8(rstudio_include_dir()));
        if (include == nullptr) { what_->Text = "header directories unchanged"; return; }
        String^ lib = Ask("cc1's headers (lib)", "kept in " + file, FromUtf8(rstudio_lib_dir()));
        if (lib == nullptr) { what_->Text = "header directories unchanged"; return; }
        array<Byte>^ a = Utf8Of(include);
        pin_ptr<Byte> aPin = &a[0];
        array<Byte>^ b = Utf8Of(lib);
        pin_ptr<Byte> bPin = &b[0];
        if (rstudio_remember_header_dirs(reinterpret_cast<const char*>(aPin),
                                         reinterpret_cast<const char*>(bPin)) == 0) {
            what_->Text = "cannot write " + file;
            return;
        }
        what_->Text = "include " + FromUtf8(rstudio_include_dir()) + ", lib " +
                      FromUtf8(rstudio_lib_dir()) + " - written to " + file;
    }

    // Visual Studio's tools, when the editor's own search did not find them.
    void OnLocateVcvars(Object^, EventArgs^) {
        String^ file = FromUtf8(rstudio_install_file());
        if (file->Length == 0) { what_->Text = "no installation directory to keep this in"; return; }
        OpenFileDialog^ pick = gcnew OpenFileDialog();
        pick->Title = "Locate vcvars64.bat (Visual Studio\\VC\\Auxiliary\\Build)";
        pick->Filter = "vcvars64.bat|vcvars64.bat|Batch files (*.bat)|*.bat";
        String^ now = FromUtf8(rstudio_vcvars());
        if (now->Length > 0) pick->InitialDirectory = System::IO::Path::GetDirectoryName(now);
        if (pick->ShowDialog(this) != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "vcvars unchanged";
            return;
        }
        array<Byte>^ bytes = Utf8Of(pick->FileName);
        pin_ptr<Byte> pinned = &bytes[0];
        if (rstudio_remember_vcvars(reinterpret_cast<const char*>(pinned)) == 0) {
            what_->Text = "cannot write " + file;
            return;
        }
        what_->Text = "Visual Studio's tools come from " + pick->FileName + " - written to " + file;
    }

    // The last three projects remembered, named at the end of the Project
    // menu; an entry whose project is gone is not shown.
    System::Collections::Generic::List<ToolStripMenuItem^>^ recentItems_;

    void RefreshRecent() {
        if (recentItems_ == nullptr) return;
        for (int i = 0; i < recentItems_->Count; ++i) {
            String^ where = FromUtf8(rstudio_recent_project(i));
            ToolStripMenuItem^ item = recentItems_[i];
            if (where->Length == 0) { item->Visible = false; continue; }
            String^ shown = System::IO::File::Exists(where)
                                ? System::IO::Path::GetFileNameWithoutExtension(where)
                                : System::IO::Path::GetFileName(where);
            item->Text = String::Format("&{0} {1}", i + 1, shown);
            item->ToolTipText = where;
            item->Visible = true;
        }
    }

    void OnOpenRecent(Object^ sender, EventArgs^) {
        ToolStripMenuItem^ item = safe_cast<ToolStripMenuItem^>(sender);
        String^ where = FromUtf8(rstudio_recent_project(safe_cast<int>(item->Tag)));
        if (where->Length == 0) { what_->Text = "no project remembered"; return; }
        LoadProject(where);
    }

    void OnNewProject(Object^, EventArgs^) {
        FolderBrowserDialog^ pick = gcnew FolderBrowserDialog();
        pick->Description = "Where to put the project";
        pick->ShowNewFolderButton = true;
        String^ start = RootNow();
        if (start == nullptr || start->Length == 0) start = ProjectsDir();
        if (start != nullptr && start->Length > 0) pick->SelectedPath = start;
        if (pick->ShowDialog(this) != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "no project made";
            return;
        }

        String^ name = Ask("Project name", "It will be made in " + pick->SelectedPath,
                           "Project");
        if (name == nullptr || name->Length == 0) {
            what_->Text = "no project made";
            return;
        }

        array<Byte>^ where = Utf8Of(pick->SelectedPath);
        pin_ptr<Byte> wherePin = &where[0];
        array<Byte>^ called = Utf8Of(name);
        pin_ptr<Byte> calledPin = &called[0];
        array<Byte>^ first = Utf8Of(path_ == nullptr ? "" : path_);
        pin_ptr<Byte> firstPin = &first[0];

        if (Did(rstudio_begin_project(project_, reinterpret_cast<const char*>(wherePin),
                                  reinterpret_cast<const char*>(calledPin),
                                  reinterpret_cast<const char*>(firstPin)))) {
            projectDirectory_ = pick->SelectedPath;
            FillTree();
            SayWhere();
            RefreshTitle();
        }
    }

    void OnSaveProject(Object^, EventArgs^) { Did(rstudio_save_project(project_)); }

    void OnOpenProjectFile(Object^, EventArgs^) {
        String^ suffix = FromUtf8(rstudio_project_suffix());

        OpenFileDialog^ pick = gcnew OpenFileDialog();
        pick->Title = "Open project file";
        pick->Filter = "RIDE projects (*" + suffix + ")|*" + suffix +
                       "|All files (*.*)|*.*";
        if (projectDirectory_ != nullptr) pick->InitialDirectory = projectDirectory_;
        if (pick->ShowDialog() != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "no project opened";
            return;
        }

        LoadProject(pick->FileName);
    }

    void OnSaveProjectAs(Object^, EventArgs^) {
        if (rstudio_project_loaded(project_) == 0) {
            what_->Text = "there is no project to save";
            return;
        }

        String^ suffix = FromUtf8(rstudio_project_suffix());
        String^ offered = FromUtf8(rstudio_project_name(project_)) + suffix;

        SaveFileDialog^ pick = gcnew SaveFileDialog();
        pick->Title = "Save as project file";
        pick->FileName = offered;
        pick->Filter = "RIDE projects (*" + suffix + ")|*" + suffix;
        pick->InitialDirectory = FromUtf8(rstudio_project_root(project_));
        if (pick->ShowDialog() != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "not saved";
            return;
        }

        array<Byte>^ where = Utf8Of(pick->FileName);
        pin_ptr<Byte> wherePin = &where[0];
        array<Byte>^ why = gcnew array<Byte>(512);
        pin_ptr<Byte> whyPin = &why[0];

        if (rstudio_project_save_as(project_, reinterpret_cast<const char*>(wherePin),
                                    reinterpret_cast<char*>(whyPin), why->Length) == 0) {
            what_->Text = FromUtf8(reinterpret_cast<const char*>(whyPin));
            return;
        }

        projectDirectory_ = System::IO::Path::GetDirectoryName(pick->FileName);
        FillTree();
        what_->Text = System::IO::Path::GetFileName(pick->FileName) +
                      " written - the project is saved there from now on";
    }

    void OnCloseProject(Object^, EventArgs^) {
        if (rstudio_project_loaded(project_) == 0) {
            what_->Text = "there is no project open";
            return;
        }
        String^ was = FromUtf8(rstudio_project_name(project_));
        RememberOpen();

        // The project's files go with it - every file it lists that is
        // open; one merely under its directory stays - each unsaved one
        // asking first, and one refusal keeps the project open with
        // everything as it was.
        System::Collections::Generic::List<Sheet^>^ theirs = gcnew System::Collections::Generic::List<Sheet^>();
        for (int i = 0; i < sheets_->Count; ++i) {
            if (sheets_[i]->path == nullptr) continue;
            array<Byte>^ bytes = Utf8Of(sheets_[i]->path);
            pin_ptr<Byte> pinned = &bytes[0];
            if (rstudio_project_holds(project_, reinterpret_cast<const char*>(pinned)) != 0) theirs->Add(sheets_[i]);
        }
        for (int i = 0; i < theirs->Count; ++i)
            if (!MayDiscard(theirs[i])) { what_->Text = "not closed - " + System::IO::Path::GetFileName(theirs[i]->path) + " has unsaved changes"; return; }
        int closed = theirs->Count;
        for (int i = 0; i < theirs->Count; ++i) {
            sheets_->Remove(theirs[i]);
            files_->TabPages->Remove(theirs[i]->page);
        }
        if (sheets_->Count == 0) { text_ = nullptr; path_ = nullptr; }
        else if (files_->SelectedTab != nullptr) {
            Sheet^ now = SheetForPage(files_->SelectedTab);
            if (now != nullptr) { text_ = now->box; path_ = now->path; }
        }

        rstudio_project_close(project_);

        paneMode_ = PaneMode::PaneFiles;
        FillTree();
        RefreshTitle();
        SayBuild();
        console_->Text = "";
        what_->Text = was + " closed" + (closed > 0 ? String::Format(", and its {0} file(s) with it", closed) : "");
    }


    void OnTreeOpen(Object^, TreeNodeMouseClickEventArgs^ e) {
        if (e->Node == nullptr || e->Node->Tag == nullptr) return;
        OpenPath(safe_cast<String^>(e->Node->Tag));
    }

    void OnFocusTree(Object^, EventArgs^) {
        if (tree_->Nodes->Count == 0) { what_->Text = "no project is open"; return; }
        if (tree_->SelectedNode == nullptr) tree_->SelectedNode = tree_->Nodes[0];
        tree_->Focus();
        what_->Text = "the project pane - enter opens, Ctrl-4 goes back to the file";
    }

    void OnFocusText(Object^, EventArgs^) {
        if (text_ != nullptr) text_->Focus();
    }

    void OnTreeKey(Object^, KeyEventArgs^ e) {
        if (e->KeyCode != Keys::Return) return;
        TreeNode^ node = tree_->SelectedNode;
        if (node == nullptr) return;

        e->Handled = true;
        e->SuppressKeyPress = true;

        if (node->Tag == nullptr) {
            if (node->IsExpanded) node->Collapse();
            else node->Expand();
            return;
        }
        OpenPath(safe_cast<String^>(node->Tag));
    }

    void OnOpenFile(Object^, EventArgs^) {

        paneMode_ = PaneMode::PaneFiles;
        OpenFileDialog^ pick = gcnew OpenFileDialog();
        pick->Filter = "C and C++|*.c;*.h;*.cpp;*.hpp|All files|*.*";
        if (pick->ShowDialog() != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "not opened";
            return;
        }
        OpenPath(pick->FileName);
    }

    void OpenPath(String^ path) {

        Sheet^ already = SheetFor(path);
        if (already != nullptr) {
            files_->SelectedTab = already->page;
            return;
        }

        String^ contents;
        try {
            contents = System::IO::File::ReadAllText(path);
        } catch (Exception^ problem) {
            what_->Text = problem->Message;
            return;
        }

        Sheet^ spare = Current();
        Sheet^ sheet;
        if (spare != nullptr && spare->path == nullptr && spare->box->TextLength == 0 &&
            !spare->box->Modified) {
            spare->path = path;
            spare->box->Text = contents;

            spare->box->Modified = false;
            sheet = spare;

            PaneFollowsTabs();
        } else {
            sheet = MakeSheet(path, contents);
        }
        text_ = sheet->box;
        path_ = path;
        RefreshTitle();
        SayBuild();
        Recolour();
        OnTextChanged(nullptr, nullptr);

        sheet->box->Modified = false;
        MarkTab(sheet);
        text_->Select(0, 0);
        text_->Focus();
        what_->Text = System::IO::Path::GetFileName(path) + "  " + text_->Lines->Length + " lines";
    }

    void OnCloseFile(Object^, EventArgs^) { CloseSheet(Current()); }

    void CloseSheet(Sheet^ sheet) {
        if (sheet == nullptr) return;
        if (!MayDiscard(sheet)) return;

        sheets_->Remove(sheet);
        files_->TabPages->Remove(sheet->page);
        PaneFollowsTabs();

        if (sheets_->Count == 0) {
            // Leave a genuinely empty environment - no untitled buffer, and no
            // entry in the pane - until the next New or Open.
            text_ = nullptr;
            path_ = nullptr;
            RefreshTitle();
            FillTree();
            SayBuild();
            console_->Text = "";
            what_->Text = "no file open";
            return;
        }

        console_->Text = "";
        what_->Text = "closed";
    }

    Sheet^ SheetForPage(TabPage^ page) {
        for (int i = 0; i < sheets_->Count; ++i)
            if (sheets_[i]->page == page) return sheets_[i];
        return nullptr;
    }
    // The tab-view raises this from its built-in × (see ClosableTabControl);
    // MayDiscard prompts first if the file has unsaved changes.
    void OnTabClose(int index) {
        if (index < 0 || index >= files_->TabPages->Count) return;
        Sheet^ s = SheetForPage(files_->TabPages[index]);
        if (s != nullptr) CloseSheet(s);
    }

    void OnSave(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }
        if (path_ == nullptr) {
            OnSaveAs(nullptr, nullptr);
            return;
        }
        try {

            System::IO::File::WriteAllText(path_, text_->Text->Replace("\r\n", "\n"));
        } catch (Exception^ problem) {
            what_->Text = problem->Message;
            return;
        }

        text_->Modified = false;
        MarkTab(Current());
        what_->Text = System::IO::Path::GetFileName(path_) + " written";
    }

    void OnSaveAs(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }
        Sheet^ sheet = Current();
        if (sheet == nullptr) return;

        SaveFileDialog^ pick = gcnew SaveFileDialog();
        pick->Filter = "C and C++|*.c;*.h;*.cpp;*.hpp|All files|*.*";
        if (sheet->path != nullptr) {
            pick->InitialDirectory = System::IO::Path::GetDirectoryName(sheet->path);
            pick->FileName = System::IO::Path::GetFileName(sheet->path);
        } else if (projectDirectory_ != nullptr) {
            pick->InitialDirectory = projectDirectory_;
        }
        if (pick->ShowDialog(this) != System::Windows::Forms::DialogResult::OK) {
            what_->Text = "not saved";
            return;
        }

        sheet->path = pick->FileName;
        path_ = pick->FileName;
        RefreshTitle();
        SayBuild();
        OnSave(nullptr, nullptr);

        // Saved into the project's directory is saved into the project.
        array<Byte>^ saved = Utf8Of(pick->FileName);
        pin_ptr<Byte> savedPin = &saved[0];
        if (rstudio_adopt_saved(project_, reinterpret_cast<const char*>(savedPin)) != 0)
            what_->Text = FromUtf8(rstudio_outcome_message(project_));
        FillTree();
    }

    void MarkTab(Sheet^ sheet) {
        if (sheet == nullptr || sheet->page == nullptr) return;
        String^ name = sheet->path == nullptr
                           ? "[no name]"
                           : System::IO::Path::GetFileName(sheet->path);
        sheet->page->Text = sheet->box->Modified ? name + "*" : name;
        if (files_ != nullptr) files_->Invalidate();
    }

    void OnExit(Object^, EventArgs^) { Close(); }

    bool MayDiscard(Sheet^ sheet) {
        if (sheet == nullptr || !sheet->box->Modified) return true;

        String^ named = sheet->path == nullptr
                            ? "This file has never been saved."
                            : System::IO::Path::GetFileName(sheet->path) + " has changes.";
        System::Windows::Forms::DialogResult answer =
            MessageBox::Show(this, named + "\r\n\r\nSave it before closing?",
                             ProductName(), MessageBoxButtons::YesNoCancel,
                             MessageBoxIcon::Warning);

        if (answer == System::Windows::Forms::DialogResult::Cancel) return false;
        if (answer == System::Windows::Forms::DialogResult::No) return true;

        files_->SelectedTab = sheet->page;
        OnSheetChanged(nullptr, nullptr);
        OnSave(nullptr, nullptr);
        return !sheet->box->Modified;
    }

    void OnKeys(Object^, EventArgs^) {
        System::Text::StringBuilder^ table = gcnew System::Text::StringBuilder();
        System::Windows::Forms::KeysConverter^ spelling =
            gcnew System::Windows::Forms::KeysConverter();

        for each (ToolStripItem^ top in MainMenuStrip->Items) {
            ToolStripMenuItem^ menu = dynamic_cast<ToolStripMenuItem^>(top);
            if (menu == nullptr) continue;

            System::Text::StringBuilder^ under = gcnew System::Text::StringBuilder();
            for each (ToolStripItem^ each in menu->DropDownItems) {
                ToolStripMenuItem^ item = dynamic_cast<ToolStripMenuItem^>(each);
                if (item == nullptr) continue;

                String^ key = item->ShortcutKeys == Keys::None
                                  ? item->ShortcutKeyDisplayString
                                  : spelling->ConvertToString(item->ShortcutKeys);
                if (key == nullptr || key->Length == 0) continue;
                under->AppendFormat("  {0,-18}{1}\r\n", key, item->Text->Replace("&", ""));
            }

            if (under->Length == 0) continue;
            table->Append(menu->Text->Replace("&", ""))->Append("\r\n");
            table->Append(under->ToString())->Append("\r\n");
        }

        table->Append("Editing\r\n");
        table->Append("  Tab               lay this line out, in the leading space\r\n");
        table->Append("  Enter             on the Console, go to the error it is about\r\n");

        Form^ box = gcnew Form();
        box->Text = "Keys";
        box->FormBorderStyle = System::Windows::Forms::FormBorderStyle::SizableToolWindow;
        box->StartPosition = System::Windows::Forms::FormStartPosition::CenterParent;
        box->ClientSize = System::Drawing::Size(460, 560);
        box->ShowInTaskbar = false;

        TextBox^ shown = gcnew TextBox();
        shown->Multiline = true;
        shown->ReadOnly = true;
        shown->WordWrap = false;
        shown->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
        shown->Dock = DockStyle::Fill;
        shown->BorderStyle = System::Windows::Forms::BorderStyle::None;
        shown->BackColor = System::Drawing::Color::White;
        shown->Font = gcnew System::Drawing::Font("Consolas", 10.0f);
        shown->Text = table->ToString();
        box->Controls->Add(shown);

        box->Shown += gcnew EventHandler(this, &MainForm::OnKeysShown);
        box->ShowDialog(this);
    }

    void OnKeysShown(Object^ sender, EventArgs^) {
        Form^ box = dynamic_cast<Form^>(sender);
        if (box == nullptr || box->Controls->Count == 0) return;
        TextBox^ shown = dynamic_cast<TextBox^>(box->Controls[0]);
        if (shown == nullptr) return;
        shown->Select(0, 0);
    }

    void OnAbout(Object^, EventArgs^) {
        MessageBox::Show(this, TakeUtf8(rstudio_about())->Replace("\n", "\r\n"),
                         "About " + ProductName(),
                         MessageBoxButtons::OK, MessageBoxIcon::Information);
    }

    void OnCompile(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }
        if (busy_) { what_->Text = "still working - give it a moment"; return; }
        ForgetError();
        if (path_ == nullptr) {
            what_->Text = "open a file first";
            return;
        }
        OnSave(nullptr, nullptr);

        int language = LanguageNow();
        int kind = rstudio_resolve(toolKind_, language);
        if (rstudio_can_compile(kind, language) == 0) {
            what_->Text = FromUtf8(rstudio_refusal(kind, language));
            return;
        }

        array<Byte>^ sourceBytes = Utf8Of(path_);
        pin_ptr<Byte> source = &sourceBytes[0];
        array<Byte>^ cc1Bytes = Utf8Of(cc1_);
        pin_ptr<Byte> cc1 = &cc1Bytes[0];
        array<Byte>^ clBytes = Utf8Of(cl_);
        pin_ptr<Byte> cl = &clBytes[0];
        array<Byte>^ shcBytes = Utf8Of(shc_);
        pin_ptr<Byte> shc = &shcBytes[0];
        array<Byte>^ cxx1Bytes = Utf8Of(cxx1_);
        pin_ptr<Byte> cxx1 = &cxx1Bytes[0];
        array<Byte>^ archBytes = Utf8Of(arch_);
        pin_ptr<Byte> arch = &archBytes[0];

        console_->Text =
            "$ " +
            FromUtf8(rstudio_shown_command(project_, reinterpret_cast<const char*>(cc1),
                                       reinterpret_cast<const char*>(cl),
                                       reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1), kind,
                                       reinterpret_cast<const char*>(source), language,
                                       reinterpret_cast<const char*>(arch), config_)) +
            "\r\n";
        panel_->SelectedIndex = 0;
        Application::DoEvents();

        RStudioBuild* built = rstudio_build(project_, reinterpret_cast<const char*>(cc1),
                                    reinterpret_cast<const char*>(cl),
                                       reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1), kind,
                                    reinterpret_cast<const char*>(source), language,
                                    reinterpret_cast<const char*>(arch), config_);

        console_->Text += FromUtf8(rstudio_build_output(built))->Replace("\n", "\r\n");
        ShowConsoleEnd();

        if (rstudio_build_has_error(built) != 0) {
            int line = rstudio_build_error_line(built);
            int column = rstudio_build_error_column(built);
            String^ message = FromUtf8(rstudio_build_error_message(built));
            rstudio_build_free(built);

            RememberError(line, column, message, nullptr);
            GoTo(line, column);
            panel_->SelectedIndex = 0;
            what_->Text = String::Format("{0}:{1}: error: {2}", line, column, message);
            return;
        }

        if (rstudio_build_ok(built) == 0) {
            what_->Text = FromUtf8(rstudio_toolchain_name(kind)) + " failed - see the console";
            rstudio_build_free(built);
            return;
        }

        String^ produced = FromUtf8(rstudio_build_assembly(built));
        assembly_->Text = produced->Replace("\n", "\r\n");
        SayDebugTab(produced);
        int lines = rstudio_build_assembly_lines(built);
        rstudio_build_free(built);

        panel_->SelectedIndex = 2;
        what_->Text = String::Format("{0} lines of assembly", lines);
    }

    void OnRun(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }
        if (busy_) { what_->Text = "still working - give it a moment"; return; }
        ForgetError();
        if (path_ == nullptr) {
            what_->Text = "open a file first";
            return;
        }
        OnSave(nullptr, nullptr);

        int language = LanguageNow();
        int kind = rstudio_resolve(toolKind_, language);
        if (rstudio_can_compile(kind, language) == 0) {
            what_->Text = FromUtf8(rstudio_refusal(kind, language));
            return;
        }

        array<Byte>^ sourceBytes = Utf8Of(path_);
        pin_ptr<Byte> source = &sourceBytes[0];
        array<Byte>^ cc1Bytes = Utf8Of(cc1_);
        pin_ptr<Byte> cc1 = &cc1Bytes[0];
        array<Byte>^ clBytes = Utf8Of(cl_);
        pin_ptr<Byte> cl = &clBytes[0];
        array<Byte>^ shcBytes = Utf8Of(shc_);
        pin_ptr<Byte> shc = &shcBytes[0];
        array<Byte>^ cxx1Bytes = Utf8Of(cxx1_);
        pin_ptr<Byte> cxx1 = &cxx1Bytes[0];
        array<Byte>^ archBytes = Utf8Of(arch_);
        pin_ptr<Byte> arch = &archBytes[0];

        if (rstudio_runs_here(kind, reinterpret_cast<const char*>(arch)) == 0) {
            what_->Text = FromUtf8(rstudio_why_not_run(kind, reinterpret_cast<const char*>(arch)));
            return;
        }

        console_->Text =
            "$ " +
            FromUtf8(rstudio_shown_run_command(project_, reinterpret_cast<const char*>(cc1),
                                           reinterpret_cast<const char*>(cl),
                                       reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1), kind,
                                           reinterpret_cast<const char*>(source), language,
                                           reinterpret_cast<const char*>(arch), config_)) +
            "\r\n";
        panel_->SelectedIndex = 0;
        Application::DoEvents();

        RStudioRan* ran = rstudio_run(project_, reinterpret_cast<const char*>(cc1),
                              reinterpret_cast<const char*>(cl),
                                       reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1), kind,
                              reinterpret_cast<const char*>(source), language,
                              reinterpret_cast<const char*>(arch), config_);

        console_->Text += FromUtf8(rstudio_ran_output(ran))->Replace("\n", "\r\n");
        ShowConsoleEnd();

        if (rstudio_ran_has_error(ran) != 0) {
            int line = rstudio_ran_error_line(ran);
            int column = rstudio_ran_error_column(ran);
            String^ message = FromUtf8(rstudio_ran_error_message(ran));
            rstudio_run_free(ran);

            RememberError(line, column, message, nullptr);
            GoTo(line, column);
            panel_->SelectedIndex = 0;
            what_->Text = String::Format("{0}:{1}: error: {2}", line, column, message);
            return;
        }

        if (rstudio_ran_built(ran) == 0) {
            what_->Text = FromUtf8(rstudio_toolchain_name(kind)) + " built no program - see the console";
            rstudio_run_free(ran);
            return;
        }

        int status = rstudio_ran_status(ran);
        rstudio_run_free(ran);

        console_->Text += String::Format("\r\n[program returned {0}]\r\n", status);
        ShowConsoleEnd();
        what_->Text = String::Format("{0} ran - it returned {1}",
                                     System::IO::Path::GetFileName(path_), status);
    }

    void OnBuildProject(Object^, EventArgs^) { BuildProject(false); }
    void OnRunProject(Object^, EventArgs^) { BuildProject(true); }

    void BuildProject(bool andRun) {
        if (busy_) { what_->Text = "still working - give it a moment"; return; }
        ForgetError();

        if (rstudio_project_target_ready(project_) == 0) {
            String^ why = FromUtf8(rstudio_project_target_why(project_));
            String^ detail = FromUtf8(rstudio_project_target_detail(project_));
            what_->Text = why;
            console_->Text = detail->Length > 0 ? why + "\r\n\r\n" + detail : why;
            panel_->SelectedIndex = 0;
            return;
        }

        SaveEveryDirty();

        // The compilers, pinned before the checks: naming each part's compiler
        // needs them, and the build below does too.
        array<Byte>^ cc1Bytes = Utf8Of(cc1_);
        pin_ptr<Byte> cc1 = &cc1Bytes[0];
        array<Byte>^ clBytes = Utf8Of(cl_);
        pin_ptr<Byte> cl = &clBytes[0];
        array<Byte>^ shcBytes = Utf8Of(shc_);
        pin_ptr<Byte> shc = &shcBytes[0];
        array<Byte>^ cxx1Bytes = Utf8Of(cxx1_);
        pin_ptr<Byte> cxx1 = &cxx1Bytes[0];
        array<Byte>^ archBytes = Utf8Of(arch_);
        pin_ptr<Byte> arch = &archBytes[0];

        // **Every part, not the target as a whole.** A group of C and C++ is
        // split into a part each, and the build sends each to its own compiler
        // through toolKind_ (Auto for an auto project) - so the build call must
        // pass toolKind_, not a single kind resolved from the first part's
        // language. Resolving the whole target to that one kind forced every
        // part to the first language's compiler: a .cpp went to cc1, which read
        // `virtual` as C and said "expected a type" while the status bar,
        // resolving the open file on its own, still showed cxx1. The check and
        // the header ask the same question of each part that buildParts will.
        int parts = rstudio_project_target_parts(project_);
        System::Collections::Generic::List<String^>^ compilers =
            gcnew System::Collections::Generic::List<String^>();
        for (int i = 0; i < parts; ++i) {
            int partLang = rstudio_project_part_language(project_, i);
            int partKind = rstudio_project_part_toolchain(
                project_, i, reinterpret_cast<const char*>(cc1),
                reinterpret_cast<const char*>(cl), reinterpret_cast<const char*>(shc),
                reinterpret_cast<const char*>(cxx1), toolKind_);
            if (rstudio_can_compile(partKind, partLang) == 0) {
                what_->Text = FromUtf8(rstudio_refusal(partKind, partLang));
                return;
            }
            if (andRun && rstudio_runs_here(partKind, reinterpret_cast<const char*>(arch)) == 0) {
                what_->Text =
                    FromUtf8(rstudio_why_not_run(partKind, reinterpret_cast<const char*>(arch)));
                return;
            }
            String^ word = FromUtf8(rstudio_toolchain_name(partKind));
            if (!compilers->Contains(word)) compilers->Add(word);
        }

        String^ program = FromUtf8(rstudio_project_target_program(project_));
        int howMany = rstudio_project_target_sources(project_);

        System::Text::StringBuilder^ said = gcnew System::Text::StringBuilder();
        said->Append("$ " + String::Join(", ", compilers->ToArray()) + " " + howMany +
                     (howMany == 1 ? " source -o " : " sources -o ") + program + "\r\n");
        for (int i = 0; i < howMany; ++i)
            said->Append("    " + FromUtf8(rstudio_project_target_source(project_, i)) + "\r\n");
        console_->Text = said->ToString();
        panel_->SelectedIndex = 0;
        what_->Text = "building " + System::IO::Path::GetFileName(program) + " ...";
        Application::DoEvents();

        RStudioBuild* made = rstudio_build_target(project_, reinterpret_cast<const char*>(cc1),
                                          reinterpret_cast<const char*>(cl),
                                       reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1), toolKind_,
                                          reinterpret_cast<const char*>(arch), config_);
        if (made == nullptr) {
            what_->Text = FromUtf8(rstudio_project_target_why(project_));
            return;
        }

        console_->Text += FromUtf8(rstudio_build_output(made))->Replace("\n", "\r\n");
        ShowConsoleEnd();

        if (rstudio_build_has_error(made) != 0) {
            int line = rstudio_build_error_line(made);
            int column = rstudio_build_error_column(made);
            String^ message = FromUtf8(rstudio_build_error_message(made));
            String^ where = FromUtf8(rstudio_build_error_file(made));
            rstudio_build_free(made);

            if (where->Length > 0) {
                if (!System::IO::Path::IsPathRooted(where)) {
                    array<Byte>^ relative = Utf8Of(where);
                    pin_ptr<Byte> relativePin = &relative[0];
                    where = FromUtf8(rstudio_project_absolute(
                        project_, reinterpret_cast<const char*>(relativePin)));
                }
                if (System::IO::File::Exists(where)) OpenPath(where);
            }

            RememberError(line, column, message, where);
            GoTo(line, column);
            panel_->SelectedIndex = 0;
            what_->Text = String::Format("{0}:{1}:{2}: error: {3}",
                                         System::IO::Path::GetFileName(where), line, column,
                                         message);
            return;
        }

        bool ok = rstudio_build_ok(made) != 0;
        rstudio_build_free(made);

        if (!ok) {
            what_->Text = String::Join(", ", compilers->ToArray()) +
                          " did not build it - see the console";
            return;
        }

        if (!andRun) {
            console_->Text += "\r\n[built " + program + "]\r\n";
            ShowConsoleEnd();
            what_->Text = "built " + System::IO::Path::GetFileName(program) + " from " +
                          howMany + (howMany == 1 ? " source" : " sources");
            return;
        }

        array<Byte>^ programBytes = Utf8Of(program);
        pin_ptr<Byte> programPin = &programBytes[0];
        RStudioRan* ran = rstudio_run_built(reinterpret_cast<const char*>(programPin));
        console_->Text += FromUtf8(rstudio_ran_output(ran))->Replace("\n", "\r\n");
        int status = rstudio_ran_status(ran);
        rstudio_run_free(ran);

        console_->Text += String::Format("\r\n[program returned {0}]\r\n", status);
        ShowConsoleEnd();
        what_->Text = String::Format("ran {0} - it returned {1}",
                                     System::IO::Path::GetFileName(program), status);
    }

    void SaveEveryDirty() {
        for (int i = 0; i < sheets_->Count; ++i) {
            Sheet^ sheet = sheets_[i];
            if (sheet->path == nullptr || !sheet->box->Modified) continue;
            try {
                System::IO::File::WriteAllText(sheet->path,
                                               sheet->box->Text->Replace("\r\n", "\n"));
                sheet->box->Modified = false;
                MarkTab(sheet);
            } catch (Exception^ problem) {
                what_->Text = problem->Message;
            }
        }
    }

    literal int WorkBuild = 1;
    literal int WorkStart = 2;
    literal int WorkGo = 3;
    literal int WorkStepOver = 4;
    literal int WorkStepInto = 5;
    literal int WorkStepOut = 6;
    literal int WorkResume = 7;
    literal int WorkBuildTarget = 8;

    void DoPendingWork() {
        array<Byte>^ archBytes = Utf8Of(arch_ == nullptr ? "" : arch_);
        pin_ptr<Byte> arch = &archBytes[0];

        switch (pending_) {
            case WorkBuild: {
                array<Byte>^ sourceBytes = Utf8Of(path_);
                pin_ptr<Byte> source = &sourceBytes[0];
                array<Byte>^ cc1Bytes = Utf8Of(cc1_);
                pin_ptr<Byte> cc1 = &cc1Bytes[0];
                array<Byte>^ clBytes = Utf8Of(cl_);
                pin_ptr<Byte> cl = &clBytes[0];
                array<Byte>^ shcBytes = Utf8Of(shc_);
                pin_ptr<Byte> shc = &shcBytes[0];
                array<Byte>^ cxx1Bytes = Utf8Of(cxx1_);
                pin_ptr<Byte> cxx1 = &cxx1Bytes[0];

                built_ = rstudio_build_program(project_, reinterpret_cast<const char*>(cc1),
                                           reinterpret_cast<const char*>(cl),
                                           reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1), workKind_,
                                           reinterpret_cast<const char*>(source),
                                           workLanguage_,
                                           reinterpret_cast<const char*>(arch), config_);
                workResult_ = rstudio_program_ok(built_);
                break;
            }
            case WorkBuildTarget: {

                array<Byte>^ cc1Bytes = Utf8Of(cc1_);
                pin_ptr<Byte> cc1 = &cc1Bytes[0];
                array<Byte>^ clBytes = Utf8Of(cl_);
                pin_ptr<Byte> cl = &clBytes[0];
                array<Byte>^ shcBytes = Utf8Of(shc_);
                pin_ptr<Byte> shc = &shcBytes[0];
                array<Byte>^ cxx1Bytes = Utf8Of(cxx1_);
                pin_ptr<Byte> cxx1 = &cxx1Bytes[0];

                targetBuilt_ = rstudio_build_target(project_, reinterpret_cast<const char*>(cc1),
                                                reinterpret_cast<const char*>(cl),
                                                reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1),
                                                toolKind_,
                                                reinterpret_cast<const char*>(arch), config_);
                workResult_ = (targetBuilt_ != nullptr && rstudio_build_ok(targetBuilt_) != 0)
                                  ? 1 : 0;
                break;
            }
            case WorkStart: {

                array<Byte>^ programBytes = Utf8Of(workProgram_);
                pin_ptr<Byte> program = &programBytes[0];
                workResult_ = rstudio_debugger_start(debugger_, workKind_,
                                                 reinterpret_cast<const char*>(arch),
                                                 reinterpret_cast<const char*>(program));
                break;
            }
            case WorkGo:       rstudio_debugger_run(debugger_); break;
            case WorkResume:   rstudio_debugger_resume(debugger_); break;
            case WorkStepOver: rstudio_debugger_step_over(debugger_); break;
            case WorkStepInto: rstudio_debugger_step_into(debugger_); break;
            case WorkStepOut:  rstudio_debugger_step_out(debugger_); break;
            default: break;
        }
    }

    bool WhileBusy(int what) {
        if (busy_) { what_->Text = "still working - give it a moment"; return false; }

        busy_ = true;
        pending_ = what;
        workResult_ = 0;

        System::Threading::Thread^ worker = gcnew System::Threading::Thread(
            gcnew System::Threading::ThreadStart(this, &MainForm::DoPendingWork));
        worker->IsBackground = true;
        worker->Start();

        while (!worker->Join(50)) Application::DoEvents();

        busy_ = false;
        return true;
    }

    System::Collections::Generic::List<int>^ BreaksFor(String^ file) {
        if (file == nullptr) return nullptr;
        String^ key = OneName(file);
        System::Collections::Generic::List<int>^ lines = nullptr;
        if (!breaks_->TryGetValue(key, lines)) {
            lines = gcnew System::Collections::Generic::List<int>();
            breaks_[key] = lines;
        }
        breakNames_[key] = file;
        return lines;
    }

    int CaretLine() {
        return text_->GetLineFromCharIndex(text_->SelectionStart) + 1;
    }

    void OnToggleBreak(Object^, EventArgs^) {
        if (path_ == nullptr) {
            what_->Text = "save the file first - a breakpoint is on a line of a file";
            return;
        }

        System::Collections::Generic::List<int>^ lines = BreaksFor(path_);
        int line = CaretLine();

        if (lines->Contains(line)) {
            lines->Remove(line);
            if (rstudio_debugger_running(debugger_) != 0) SetEveryBreakpoint();
            what_->Text = String::Format("breakpoint off line {0}", line);
        } else {
            lines->Add(line);
            if (rstudio_debugger_running(debugger_) != 0) {
                array<Byte>^ bytes = Utf8Of(path_);
                pin_ptr<Byte> pinned = &bytes[0];
                rstudio_debugger_break(debugger_, reinterpret_cast<const char*>(pinned), line);
            }
            what_->Text = String::Format("breakpoint on line {0}", line);
        }
        Current()->gutter->Invalidate();
    }

    void SetEveryBreakpoint() {
        rstudio_debugger_clear(debugger_);
        for each (System::Collections::Generic::KeyValuePair<String^,
                      System::Collections::Generic::List<int>^> pair in breaks_) {
            String^ named = nullptr;
            if (!breakNames_->TryGetValue(pair.Key, named)) named = pair.Key;
            array<Byte>^ bytes = Utf8Of(named);
            pin_ptr<Byte> pinned = &bytes[0];
            for each (int line in pair.Value)
                rstudio_debugger_break(debugger_, reinterpret_cast<const char*>(pinned), line);
        }
    }

    void OnDebug(Object^, EventArgs^) { Debug(false); }
    void OnDebugProject(Object^, EventArgs^) { Debug(true); }

    void Debug(bool project) {
        if (rstudio_debugger_running(debugger_) != 0) {

            if (!WhileBusy(WorkResume)) return;
            ShowStop();
            return;
        }

        ForgetError();

        array<Byte>^ archBytes = Utf8Of(arch_);
        pin_ptr<Byte> arch = &archBytes[0];
        array<Byte>^ cc1Bytes = Utf8Of(cc1_);
        pin_ptr<Byte> cc1 = &cc1Bytes[0];
        array<Byte>^ clBytes = Utf8Of(cl_);
        pin_ptr<Byte> cl = &clBytes[0];
        array<Byte>^ shcBytes = Utf8Of(shc_);
        pin_ptr<Byte> shc = &shcBytes[0];
        array<Byte>^ cxx1Bytes = Utf8Of(cxx1_);
        pin_ptr<Byte> cxx1 = &cxx1Bytes[0];

        int kind = 0;
        int language = 0;

        if (project) {

            if (rstudio_project_target_ready(project_) == 0) {
                String^ why = FromUtf8(rstudio_project_target_why(project_));
                String^ detail = FromUtf8(rstudio_project_target_detail(project_));
                what_->Text = why;
                console_->Text = detail->Length > 0 ? why + "\r\n\r\n" + detail : why;
                panel_->SelectedIndex = 0;
                return;
            }
            SaveEveryDirty();
            language = rstudio_project_target_language(project_);

            if (rstudio_project_debug_plan(project_, reinterpret_cast<const char*>(cc1),
                                       reinterpret_cast<const char*>(cl),
                                       reinterpret_cast<const char*>(shc),
                                       reinterpret_cast<const char*>(cxx1), toolKind_,
                                       reinterpret_cast<const char*>(arch)) == 0) {
                what_->Text = FromUtf8(rstudio_project_why_not_debug(project_));
                return;
            }
            kind = rstudio_project_debug_kind(project_);
        } else {
            if (path_ == nullptr) { what_->Text = "open a file first"; return; }
            OnSave(nullptr, nullptr);

            language = LanguageNow();
            kind = rstudio_resolve(toolKind_, language);
            if (rstudio_can_compile(kind, language) == 0) {
                what_->Text = FromUtf8(rstudio_refusal(kind, language));
                return;
            }

            if (rstudio_debugger_stops_itself(kind) == 0 &&
                rstudio_debugger_for(kind, reinterpret_cast<const char*>(arch)) == 0) {
                what_->Text = FromUtf8(
                    rstudio_no_debugger_because(kind, reinterpret_cast<const char*>(arch)));
                return;
            }
        }

        if (rstudio_runs_here(kind, reinterpret_cast<const char*>(arch)) == 0) {
            what_->Text = FromUtf8(rstudio_why_not_run(kind, reinterpret_cast<const char*>(arch)));
            return;
        }
        if (config_ != RSTUDIO_CONFIG_DEBUG) {

            what_->Text =
                FromUtf8(rstudio_release_cannot_stop(kind)) + " - choose Debug build, then F8";
            return;
        }

        console_->Text = project ? "$ building the project for the debugger\r\n"
                                 : "$ building for the debugger\r\n";
        if (project) {
            int howMany = rstudio_project_target_sources(project_);
            for (int i = 0; i < howMany; ++i)
                console_->Text += "    " +
                    FromUtf8(rstudio_project_target_source(project_, i)) + "\r\n";

            int blind = rstudio_project_blind_groups(project_);
            for (int i = 0; i < blind; ++i)
                console_->Text += "  (" + FromUtf8(rstudio_project_blind_group(project_, i)) +
                    " carries no debug information - the debugger cannot stop in it)\r\n";
        }
        panel_->SelectedIndex = 0;
        what_->Text = "building for the debugger ...";
        Application::DoEvents();

        if (built_ != nullptr) { rstudio_program_free(built_); built_ = nullptr; }
        if (targetBuilt_ != nullptr) { rstudio_build_free(targetBuilt_); targetBuilt_ = nullptr; }

        workKind_ = kind;
        workLanguage_ = language;
        if (!WhileBusy(project ? WorkBuildTarget : WorkBuild)) return;

        if (project && targetBuilt_ == nullptr) {
            what_->Text = FromUtf8(rstudio_project_target_why(project_));
            return;
        }

        console_->Text += FromUtf8(project ? rstudio_build_output(targetBuilt_)
                                           : rstudio_program_output(built_))->Replace("\n", "\r\n");
        ShowConsoleEnd();

        if (workResult_ == 0) { DebugBuildFailed(project, kind); return; }

        workProgram_ = project ? FromUtf8(rstudio_project_target_program(project_))
                               : FromUtf8(rstudio_program_path(built_));

        what_->Text = rstudio_debugger_stops_itself(kind) != 0
                          ? "starting the program ..."
                          : "starting the debugger ...";
        if (!WhileBusy(WorkStart)) return;
        if (workResult_ == 0) {

            what_->Text =
                FromUtf8(rstudio_why_it_did_not_start(kind, reinterpret_cast<const char*>(arch)));
            EndDebugging();
            return;
        }

        SetEveryBreakpoint();
        if (!WhileBusy(WorkGo)) return;
        ShowStop();
    }

    void DebugBuildFailed(bool project, int kind) {
        bool told = project ? rstudio_build_has_error(targetBuilt_) != 0
                            : rstudio_program_has_error(built_) != 0;
        if (told) {
            int line = project ? rstudio_build_error_line(targetBuilt_)
                               : rstudio_program_error_line(built_);
            int column = project ? rstudio_build_error_column(targetBuilt_)
                                 : rstudio_program_error_column(built_);
            String^ message = FromUtf8(project ? rstudio_build_error_message(targetBuilt_)
                                               : rstudio_program_error_message(built_));
            String^ where = project ? FromUtf8(rstudio_build_error_file(targetBuilt_)) : nullptr;

            if (where != nullptr && where->Length > 0) {
                if (!System::IO::Path::IsPathRooted(where)) {
                    array<Byte>^ relative = Utf8Of(where);
                    pin_ptr<Byte> relativePin = &relative[0];
                    where = FromUtf8(rstudio_project_absolute(
                        project_, reinterpret_cast<const char*>(relativePin)));
                }
                if (System::IO::File::Exists(where)) OpenPath(where);
            }

            RememberError(line, column, message, where);
            GoTo(line, column);
            panel_->SelectedIndex = 0;
            what_->Text = String::Format("{0}:{1}: error: {2}", line, column, message);
        } else {
            what_->Text = FromUtf8(rstudio_toolchain_name(kind)) +
                          " built no program - see the console";
        }
        EndDebugging();
    }

    void OnDebugMenuOpening(Object^, EventArgs^) {
        bool itsOwn = rstudio_debugging_shalimar(debugger_) != 0;
        upTheStack_->Enabled = !itsOwn;
        downTheStack_->Enabled = !itsOwn;
        watchItem_->Enabled = !itsOwn;
    }

    void OnStepOver(Object^, EventArgs^) { Step(0); }
    void OnStepInto(Object^, EventArgs^) { Step(1); }
    void OnStepOut(Object^, EventArgs^) { Step(2); }

    void OnFrameUp(Object^, EventArgs^) { LookAlongStack(1); }
    void OnFrameDown(Object^, EventArgs^) { LookAlongStack(-1); }

    void Step(int how) {
        if (rstudio_debugger_running(debugger_) == 0) {
            what_->Text = "nothing is running - F8 starts it";
            return;
        }
        int what = (how == 1) ? WorkStepInto : (how == 2) ? WorkStepOut : WorkStepOver;
        if (!WhileBusy(what)) return;
        ShowStop();
    }

    void OnDebugStop(Object^, EventArgs^) {
        if (rstudio_debugger_running(debugger_) == 0) {
            what_->Text = "nothing is running";
            return;
        }
        EndDebugging();
        what_->Text = "debugging stopped";
    }

    void EndDebugging() {
        rstudio_debugger_stop(debugger_);

        if (built_ != nullptr) { rstudio_program_free(built_); built_ = nullptr; }
        if (targetBuilt_ != nullptr) { rstudio_build_free(targetBuilt_); targetBuilt_ = nullptr; }
        workProgram_ = nullptr;
        stopFile_ = nullptr;
        stopLine_ = 0;
        lookingFile_ = nullptr;
        lookingLine_ = 0;
        ShowStoppedLine(-1);
        Current()->gutter->Invalidate();
    }

    void ShowStop() {

        String^ printed = Lines(FromUtf8(rstudio_stop_output(debugger_)));
        if (!String::IsNullOrEmpty(printed)) {
            console_->AppendText(printed);
            ShowConsoleEnd();
        }

        panel_->SelectedIndex = 1;

        if (rstudio_stop_exited(debugger_) != 0) {
            int status = rstudio_stop_status(debugger_);
            debug_->Text = String::Format(
                "the program ran to the end and returned {0}\r\n\r\n"
                "F8 starts it again. The breakpoints are still where you put them.", status);
            EndDebugging();
            what_->Text = String::Format("the program returned {0}", status);
            return;
        }

        if (rstudio_stop_stopped(debugger_) == 0) {
            String^ heard = Lines(FromUtf8(rstudio_stop_said(debugger_)));

            if (rstudio_stop_no_source(debugger_) != 0) {
                stopFile_ = nullptr;
                stopLine_ = 0;
                lookingFile_ = nullptr;
                lookingLine_ = 0;
                ShowStoppedLine(-1);
                Current()->gutter->Invalidate();
                debug_->Text =
                    "stopped where there is no source to show\r\n\r\n"
                    "Stepping past the end of main arrives in the code that\r\n"
                    "started it, which was not compiled here. F8 carries on to\r\n"
                    "the end, and Stop debugging leaves it.\r\n\r\n" + heard;
                what_->Text = "stopped where there is no source - F8 carries on";
                return;
            }

            debug_->Text = String::IsNullOrEmpty(heard)
                ? "the debugger stopped answering"
                : "the debugger stopped answering\r\n\r\n" + heard;
            EndDebugging();
            what_->Text = "the debugger stopped answering - see the Debug tab";
            return;
        }

        stopFile_ = FromUtf8(rstudio_stop_file(debugger_));
        stopLine_ = rstudio_stop_line(debugger_);
        String^ function = FromUtf8(rstudio_stop_function(debugger_));

        if (path_ != nullptr && stopLine_ > 0 &&
            System::IO::Path::GetFileName(stopFile_) == System::IO::Path::GetFileName(path_)) {
            GoTo(stopLine_, 1);
            ShowStoppedLine(stopLine_ - 1);
        }

        stopFunction_ = function;
        lookingFile_ = nullptr;
        lookingLine_ = 0;
        WriteDebugTab();

        Current()->gutter->Invalidate();
        what_->Text = String::Format("{0}:{1}{2}", System::IO::Path::GetFileName(stopFile_),
                                     stopLine_,
                                     String::IsNullOrEmpty(function) ? "" : " in " + function);
    }

    void RememberError(int line, int column, String^ message, String^ file) {
        errorLine_ = line;
        errorColumn_ = column;
        errorMessage_ = message;
        errorFile_ = file;
    }

    void ForgetError() {
        errorLine_ = 0;
        errorColumn_ = 0;
        errorMessage_ = nullptr;
        errorFile_ = nullptr;
    }

    void GoToError() {
        if (errorMessage_ == nullptr) { what_->Text = "no error to go to"; return; }
        if (errorFile_ != nullptr && !SamePath(path_, errorFile_) &&
            System::IO::File::Exists(errorFile_))
            OpenPath(errorFile_);
        GoTo(errorLine_, errorColumn_);
        what_->Text = String::Format("{0}:{1}: error: {2}", errorLine_, errorColumn_,
                                     errorMessage_);
    }

    void OnConsoleKey(Object^, KeyEventArgs^ e) {
        if (e->KeyCode != Keys::Enter) return;
        e->SuppressKeyPress = true;
        GoToError();
    }

    void OnConsoleDoubleClick(Object^, EventArgs^) { GoToError(); }

    void WriteDebugTab() {
        System::Text::StringBuilder^ said = gcnew System::Text::StringBuilder();
        said->AppendFormat("{0}\r\n\r\n", StopLine());

        String^ looking = FromUtf8(rstudio_looking_text(debugger_));
        if (looking->Length > 0) said->AppendFormat("{0}\r\n\r\n", looking);

        int howMany = rstudio_locals_count(debugger_);
        if (howMany == 0) {

            said->AppendFormat("{0}\r\n", FromUtf8(rstudio_locals_none_because(debugger_)));
        } else {
            for (int i = 0; i < howMany; ++i)
                said->AppendFormat("{0}\r\n", FromUtf8(rstudio_local_text(debugger_, i)));
        }

        int watching = rstudio_watch_count(debugger_);
        if (watching > 0) {
            said->Append("\r\nwatching\r\n");
            for (int i = 0; i < watching; ++i)
                said->AppendFormat("{0}\r\n", FromUtf8(rstudio_watch_text(debugger_, i)));
        }

        int deep = rstudio_stack_count(debugger_);
        if (deep > 1) {
            said->Append("\r\ncalled from\r\n");
            for (int i = 1; i < deep; ++i)
                said->AppendFormat("{0}\r\n", FromUtf8(rstudio_stack_text(debugger_, i)));
        }

        said->Append("\r\nF8 carries on   F7 steps over   F6 steps into   F9 sets a breakpoint");

        if (rstudio_debugging_shalimar(debugger_) == 0) {
            said->Append("\r\nDouble-click a variable, or press enter on it, to set it");
            if (watching > 0)
                said->Append("\r\nThe same on a watch changes it, and an empty answer drops it");
            if (deep > 1) {
                said->Append("\r\nCtrl+Up looks at what called this   Ctrl+Down comes back down");
                said->Append("\r\nThe same on a frame looks at it, and on the top line goes back");
            }
        }
        debug_->Text = said->ToString();

        debug_->SelectionStart = 0;
        debug_->SelectionLength = 0;
    }

    String^ StopLine() {
        pin_ptr<Byte> file = &Utf8Of(stopFile_)[0];
        pin_ptr<Byte> function = &Utf8Of(stopFunction_)[0];
        return FromUtf8(rstudio_stop_line_text(reinterpret_cast<const char*>(file), stopLine_,
                                           reinterpret_cast<const char*>(function)));
    }

    void OnDebugKey(Object^, KeyEventArgs^ e) {
        if (e->KeyCode != Keys::Enter) return;
        e->SuppressKeyPress = true;
        GoToFrame();
    }

    void OnDebugDoubleClick(Object^, EventArgs^) { GoToFrame(); }

    void GoToFrame() {
        if (debug_->Lines->Length == 0) return;
        int row = debug_->GetLineFromCharIndex(debug_->SelectionStart);
        if (row < 0 || row >= debug_->Lines->Length) return;

        String^ row_text = debug_->Lines[row];
        pin_ptr<Byte> line = &Utf8Of(row_text)[0];
        int which = rstudio_stack_on_line(debugger_, reinterpret_cast<const char*>(line));

        if (which < 0 && rstudio_stack_count(debugger_) > 0 && row_text == StopLine()) which = 0;

        if (which < 0) {

            int variable = rstudio_locals_on_line(debugger_,
                                              reinterpret_cast<const char*>(line));
            if (variable >= 0) { EditVariable(variable); return; }

            int watch = rstudio_watch_on_line(debugger_, reinterpret_cast<const char*>(line));
            if (watch >= 0) { EditWatch(watch); return; }

            what_->Text = "that line is neither a frame nor a variable nor a watch";
            return;
        }

        LookAt(which);
    }

    void OnWatch(Object^, EventArgs^) {

        String^ no = FromUtf8(rstudio_cannot_watch(debugger_));
        if (no->Length > 0) { what_->Text = no; return; }

        String^ what = Ask("watch expression", "");
        if (what == nullptr || what->Length == 0) { what_->Text = "nothing to watch"; return; }

        pin_ptr<Byte> wanted = &Utf8Of(what)[0];
        rstudio_watch_add(debugger_, reinterpret_cast<const char*>(wanted));
        panel_->SelectedIndex = 1;
        if (stopLine_ > 0) WriteDebugTab();
        what_->Text = rstudio_debugger_running(debugger_) != 0
                          ? "watching " + what
                          : "watching " + what + " - it is read when the program stops";
    }

    void EditWatch(int which) {
        String^ was = FromUtf8(rstudio_watch_expression(debugger_, which));
        String^ what = Ask("watch, or empty to drop it", was);
        if (what == nullptr) { what_->Text = was + " is still watched"; return; }

        pin_ptr<Byte> wanted = &Utf8Of(what)[0];
        rstudio_watch_set(debugger_, which, reinterpret_cast<const char*>(wanted));
        WriteDebugTab();
        what_->Text = what->Length == 0 ? "stopped watching " + was : "watching " + what;
    }

    void EditVariable(int which) {
        String^ name = FromUtf8(rstudio_local_name(debugger_, which));
        String^ was = FromUtf8(rstudio_local_value(debugger_, which));
        if (name->Length == 0) return;

        String^ value = Ask(String::Format("set {0}", name), was);
        if (value == nullptr || value->Length == 0) {
            what_->Text = String::Format("{0} is still {1}", name, was);
            return;
        }

        pin_ptr<Byte> named = &Utf8Of(name)[0];
        pin_ptr<Byte> wanted = &Utf8Of(value)[0];
        if (rstudio_set_variable(debugger_, reinterpret_cast<const char*>(named),
                             reinterpret_cast<const char*>(wanted)) == 0) {
            String^ complaint = FromUtf8(rstudio_set_complaint(debugger_));
            what_->Text = complaint->Length > 0
                              ? complaint
                              : String::Format("the debugger would not set {0}", name);
            return;
        }

        WriteDebugTab();
        what_->Text = String::Format("{0} is {1} now", name,
                                     FromUtf8(rstudio_local_value(debugger_, which)));
    }

    void LookAlongStack(int by) {
        int deep = rstudio_stack_count(debugger_);
        if (rstudio_debugger_running(debugger_) == 0 || deep == 0) {
            what_->Text = "nothing is stopped, so there is no stack to walk";
            return;
        }

        String^ no = FromUtf8(rstudio_cannot_walk_stack(debugger_));
        if (no->Length > 0) { what_->Text = no; return; }

        int looking = rstudio_looking_at(debugger_);
        if (by > 0) {
            if (looking + 1 >= deep) {
                what_->Text = String::Format("nothing called {0}, which is the top",
                                             FromUtf8(rstudio_stack_function(debugger_, deep - 1)));
                return;
            }
            LookAt(looking + 1);
            return;
        }
        if (looking == 0) {
            what_->Text = "this is where the program stopped - there is nothing below it";
            return;
        }
        LookAt(looking - 1);
    }

    void LookAt(int which) {
        if (rstudio_debugger_look_at(debugger_, which) == 0) {
            what_->Text = "the debugger would not go to that frame";
            return;
        }

        WriteDebugTab();

        String^ file = FromUtf8(rstudio_stack_file(debugger_, which));
        int at = rstudio_stack_line(debugger_, which);

        lookingFile_ = which == 0 ? nullptr : file;
        lookingLine_ = which == 0 ? 0 : at;

        if (file->Length > 0 && !SamePath(path_, file) && System::IO::File::Exists(file))
            OpenPath(file);
        GoTo(at, 1);
        Current()->gutter->Invalidate();
        what_->Text = String::Format("{0}:{1} in {2} - {3}",
                                     System::IO::Path::GetFileName(file), at,
                                     FromUtf8(rstudio_stack_function(debugger_, which)),
                                     which == 0 ? "back where it stopped"
                                                : "where the call came from");
    }

    void GoTo(int line, int column) {
        int row = line - 1;
        if (row < 0) row = 0;
        if (row >= text_->Lines->Length) row = text_->Lines->Length - 1;

        int at = text_->GetFirstCharIndexFromLine(row) + CharacterColumn(row, column - 1);
        if (at < 0) at = 0;
        text_->Select(at, 0);
        text_->ScrollToCaret();
        Recolour();
        text_->Focus();
    }

    void ShowPanel(int which) {
        panel_->SelectedIndex = which;
        if (which == 0) console_->Focus();
        else if (which == 1) debug_->Focus();
        else assembly_->Focus();

        console_->SelectionLength = 0;
        debug_->SelectionLength = 0;
        assembly_->SelectionLength = 0;
    }

    void OnShowConsole(Object^, EventArgs^) { ShowPanel(0); }
    void OnShowDebug(Object^, EventArgs^) { ShowPanel(1); }
    void OnShowAssembly(Object^, EventArgs^) { ShowPanel(2); }

    void SayBuild() {
        if (build_ == nullptr) return;
        int language = LanguageNow();
        int kind = rstudio_resolve(toolKind_, language);
        String^ said = FromUtf8(rstudio_language_name(language)) + "  " +
                       FromUtf8(rstudio_config_name(config_)) + "  " +
                       PrettyCompiler(FromUtf8(rstudio_toolchain_name(kind)));

        if (toolKind_ == RSTUDIO_TOOL_AUTO) said += "*";

        if (rstudio_uses_arch(kind) != 0) said += "  " + arch_;
        build_->Text = said;

        // The menu-bar hint: just the compiler, in plain words and bold so it
        // stands out. It changes whenever the resolved compiler does. (The
        // status bar still marks an auto-chosen compiler with a star.)
        if (compilerHint_ != nullptr) {
            compilerHint_->Text = PrettyCompiler(FromUtf8(rstudio_toolchain_name(kind)));
        }
    }

    // Friendlier compiler names for what the user sees (the driver binaries stay
    // cc1/cxx1/shc). Display only - the core and its tests are untouched.
    String^ PrettyCompiler(String^ name) {
        if (name == "cc1") return "cc";
        if (name == "cxx1" || name == "c++") return "c++";
        if (name == "shc") return "shalimar";
        return name;
    }

    void ShowChoices() {
        for each (ToolStripMenuItem^ one in targetItems_)
            one->Checked = String::Equals(one->Text, arch_, StringComparison::Ordinal);
        toolAutoItem_->Checked = toolKind_ == RSTUDIO_TOOL_AUTO;
        toolCc1Item_->Checked = toolKind_ == RSTUDIO_TOOL_CC1;
        toolCxx1Item_->Checked = toolKind_ == RSTUDIO_TOOL_CXX1;
        toolClItem_->Checked = toolKind_ == RSTUDIO_TOOL_MSVC;
        toolShcItem_->Checked = toolKind_ == RSTUDIO_TOOL_SHC;
        if (langAutoItem_ != nullptr) {
            langAutoItem_->Checked = languageChoice_ < 0;
            langCItem_->Checked = languageChoice_ == RSTUDIO_LANG_C;
            langCppItem_->Checked = languageChoice_ == RSTUDIO_LANG_CPP;
            langShalimarItem_->Checked = languageChoice_ == RSTUDIO_LANG_SHALIMAR;
            langJsonItem_->Checked = languageChoice_ == RSTUDIO_LANG_JSON;
            langTextItem_->Checked = languageChoice_ == RSTUDIO_LANG_PLAIN;
        }
        debugConfigItem_->Checked = config_ == RSTUDIO_CONFIG_DEBUG;
        releaseConfigItem_->Checked = config_ == RSTUDIO_CONFIG_RELEASE;
        SayBuild();
    }

    void NextConfig() {
        if (config_ == RSTUDIO_CONFIG_DEBUG) OnReleaseConfig(nullptr, nullptr);
        else OnDebugConfig(nullptr, nullptr);
    }

    void NextTool() {
        if (toolKind_ == RSTUDIO_TOOL_AUTO) OnToolCc1(nullptr, nullptr);
        else if (toolKind_ == RSTUDIO_TOOL_CC1) OnToolCxx1(nullptr, nullptr);
        else if (toolKind_ == RSTUDIO_TOOL_CXX1) OnToolShc(nullptr, nullptr);
        else if (toolKind_ == RSTUDIO_TOOL_SHC) OnToolCl(nullptr, nullptr);
        else OnToolAuto(nullptr, nullptr);
    }

    void NextTarget() {
        if (targetItems_ == nullptr || targetItems_->Count == 0) return;
        int at = 0;
        for (int i = 0; i < targetItems_->Count; ++i)
            if (String::Equals(targetItems_[i]->Text, arch_, StringComparison::Ordinal)) {
                at = i;
                break;
            }

        OnTarget(targetItems_[(at + 1) % targetItems_->Count], nullptr);
    }

    void OnDebugConfig(Object^, EventArgs^) {
        config_ = RSTUDIO_CONFIG_DEBUG;
        rstudio_remember_configuration(config_);
        ShowChoices();
        what_->Text = "debug";
    }
    void OnReleaseConfig(Object^, EventArgs^) {
        config_ = RSTUDIO_CONFIG_RELEASE;
        rstudio_remember_configuration(config_);
        ShowChoices();
        what_->Text = "release";
    }
    void OnTarget(Object^ sender, EventArgs^) {
        arch_ = safe_cast<ToolStripMenuItem^>(sender)->Text;
        ShowChoices();
        RefreshDebugTab();
        what_->Text = "target: " + arch_;
    }
    void OnToolAuto(Object^, EventArgs^) {
        toolKind_ = RSTUDIO_TOOL_AUTO;
        rstudio_remember_default_compiler(toolKind_);
        ShowChoices();
        RefreshDebugTab();
        what_->Text = "compiler: chosen by the file";
    }
    void OnToolCc1(Object^, EventArgs^) {
        toolKind_ = RSTUDIO_TOOL_CC1;
        rstudio_remember_default_compiler(toolKind_);
        ShowChoices();
        RefreshDebugTab();
        what_->Text = "compiler: cc1";
    }
    void OnToolCl(Object^, EventArgs^) {
        toolKind_ = RSTUDIO_TOOL_MSVC;
        rstudio_remember_default_compiler(toolKind_);
        ShowChoices();
        RefreshDebugTab();
        what_->Text = "compiler: cl";
    }
    void OnToolCxx1(Object^, EventArgs^) {
        toolKind_ = RSTUDIO_TOOL_CXX1;
        rstudio_remember_default_compiler(toolKind_);
        ShowChoices();
        RefreshDebugTab();
        what_->Text = "compiler: cxx1";
    }
    void OnToolShc(Object^, EventArgs^) {
        toolKind_ = RSTUDIO_TOOL_SHC;
        rstudio_remember_default_compiler(toolKind_);
        ShowChoices();
        RefreshDebugTab();
        what_->Text = "compiler: shc";
    }

    void ChooseLanguage(int language, String^ said) {
        languageChoice_ = language;
        ShowChoices();
        RefreshDebugTab();
        Recolour();
        what_->Text = said;
    }
    void OnLangAuto(Object^, EventArgs^) {
        ChooseLanguage(-1, "language: chosen by the name");
    }
    void OnConvert(Object^, EventArgs^) {
        if (text_ == nullptr) { what_->Text = "no file is open"; return; }
        if (busy_) { what_->Text = "still working - give it a moment"; return; }
        ForgetError();
        if (path_ == nullptr) { what_->Text = "open a file first"; return; }

        int toShalimar = 0;
        if (rstudio_converts_from(LanguageNow(), &toShalimar) == 0) {
            what_->Text = "c2s converts between C and Shalimar - set the language above "
                          "if that is wrong";
            return;
        }

        OnSave(nullptr, nullptr);
        if (path_ == nullptr) return;

        char* whereRaw = rstudio_find_converter();
        String^ converter = FromUtf8(whereRaw);
        rstudio_free(whereRaw);
        if (converter->Length == 0) {
            what_->Text = "no c2s beside this editor - build Converter-C2S here, or set C2S";
            return;
        }

        array<Byte>^ sourceBytes = Utf8Of(path_);
        pin_ptr<Byte> source = &sourceBytes[0];

        char* namedRaw =
            rstudio_converted_name(reinterpret_cast<const char*>(source), toShalimar);
        String^ produced = FromUtf8(namedRaw);
        rstudio_free(namedRaw);
        if (produced->Length == 0 || String::Equals(produced, path_)) {
            what_->Text = "that would write over the file it is reading";
            return;
        }

        array<Byte>^ converterBytes = Utf8Of(converter);
        pin_ptr<Byte> where = &converterBytes[0];
        array<Byte>^ producedBytes = Utf8Of(produced);
        pin_ptr<Byte> into = &producedBytes[0];

        console_->Text = "$ c2s " + (toShalimar ? "--to-shalimar " : "--to-c ") +
                         produced + "\r\n";
        panel_->SelectedIndex = 0;
        Application::DoEvents();

        RStudioConversion* made =
            rstudio_convert(reinterpret_cast<const char*>(where),
                            reinterpret_cast<const char*>(source),
                            reinterpret_cast<const char*>(into), toShalimar);

        console_->Text += FromUtf8(rstudio_conversion_output(made))->Replace("\n", "\r\n");
        ShowConsoleEnd();

        if (rstudio_conversion_ran(made) == 0) {
            what_->Text = "could not run " + converter;
            rstudio_conversion_free(made);
            return;
        }

        String^ written = FromUtf8(rstudio_conversion_produced(made));
        int ok = rstudio_conversion_ok(made);
        rstudio_conversion_free(made);

        if (written->Length == 0) {
            what_->Text = "nothing was written - c2s could not read or write a file";
            return;
        }

        OpenPath(written);
        what_->Text = ok != 0
            ? System::IO::Path::GetFileName(written) + " - converted"
            : System::IO::Path::GetFileName(written) +
                  " - written with unconverted parts marked; search for BEYOND";
    }

    void OnLangC(Object^, EventArgs^) { ChooseLanguage(RSTUDIO_LANG_C, "language: C"); }
    void OnLangCpp(Object^, EventArgs^) { ChooseLanguage(RSTUDIO_LANG_CPP, "language: C++"); }
    void OnLangShalimar(Object^, EventArgs^) {
        ChooseLanguage(RSTUDIO_LANG_SHALIMAR, "language: Shalimar");
    }
    void OnLangJson(Object^, EventArgs^) {
        ChooseLanguage(RSTUDIO_LANG_JSON, "language: JSON");
    }
    void OnLangText(Object^, EventArgs^) {
        ChooseLanguage(RSTUDIO_LANG_PLAIN, "language: plain text");
    }
};

}
