/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implement the entry point of the profile visualizer application.
///////////////////////////////////////////////////////////////////////////80*/

// TODO(rlk): Here are some notes on how things will have to be organized.
// 5. Know that task scheduler setup events may not be present - they could have been overwritten in the trace.
//    - Need to be able to reconstruct this information from the state transition events.
//    - Pool and source index. are available as part of the task ID.
//    - Pool index can be calculated from source index; see scheduler source code.

/*////////////////////
//   Preprocessor   //
////////////////////*/
/// @summary Tag used to mark a function as available for public use, but not exported outside of the translation unit.
#ifndef public_function
    #define public_function                    static
#endif

/// @summary Tag used to mark a function internal to the translation unit.
#ifndef internal_function
    #define internal_function                  static
#endif

/// @summary Tag used to mark a variable as local to a function, and persistent across invocations of that function.
#ifndef local_persist
    #define local_persist                      static
#endif

/// @summary Tag used to mark a variable as global to the translation unit.
#ifndef global_variable
    #define global_variable                    static
#endif

/// @summary Helper macro to write a formatted string to stderr. The output will not be visible unless a console window is opened.
#define ConsoleError(formatstr, ...) \
    _ftprintf(stderr, _T(formatstr), __VA_ARGS__)

/// @summary Helper macro to write a formatting string to stdout. The output will not be visible unless a console window is opened.
#define ConsoleOutput(formatstr, ...) \
    _ftprintf(stdout, _T(formatstr), __VA_ARGS__)

/// @summary Include the GUIDs for ETW loggers in the executable.
#define INITGUID

/*////////////////
//   Includes   //
////////////////*/
#include <iostream>
#include <vector>

#include <stddef.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

#include <tchar.h>
#include <windows.h>
#include <Shellapi.h>

#include <strsafe.h>
#include <wmistr.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>

#include <process.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>

#include "profiler.h"
#include "visualizer_types.h"

#include "trace_loader.cc"

#include "imgui.cpp"
#include "imgui_draw.cpp"
#include "imgui_demo.cpp"
#include "imgui_impl_glfw.cpp"

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Define identifiers for the possible top-level user interface states.
enum UI_STATE_ID : int
{
    UI_STATE_ID_NO_TRACE_LOADED   = 0, 
    UI_STATE_ID_TRACE_LOADING     = 1,
    UI_STATE_ID_TRACE_LOADED      = 2, 
    UI_STATE_ID_TRACE_LOAD_ERROR  = 3,
};

/// @summary Defines the data associated with a parsed command line.
struct COMMAND_LINE
{
    TCHAR                 *TraceFile;
};

/// @summary Define the global user interface state.
struct UI_STATE
{
    int                    TopLevelState;
    WIN32_PROFILER_EVENTS *EventData;
    COMMAND_LINE          *CommandLine;
    GLFWwindow            *MainWindow;
    bool                   ShowConsole;
    TCHAR                  TracePath[32768];
};

/*///////////////
//   Globals   //
///////////////*/

/*//////////////////////////
//   Internal Functions   //
//////////////////////////*/
/// @summary Output a message to the debugger output stream using a printf-style format string.
/// @param format The format string. See https://msdn.microsoft.com/en-us/library/56e442dc.aspx
internal_function void
DebugPrintf
(
    TCHAR const *format, 
    ...
)
{
    size_t const BUFFER_SIZE_CHARS = 2048;
    TCHAR    fmt_buffer[BUFFER_SIZE_CHARS];
    int      str_chars = 0;
    va_list  arg_list;
    va_start(arg_list, format);
    if ((str_chars = _vsntprintf_s(fmt_buffer, BUFFER_SIZE_CHARS, _TRUNCATE, format, arg_list)) >= 0)
    {   // send the formatted output to the debugger channel.
        OutputDebugString(fmt_buffer);
    }
    else
    {   // TODO(rlk): make it easier to debug failure.
        OutputDebugString(_T("ERROR: DebugPrintf invalid arguments or buffer too small...\n"));
    }
    va_end(arg_list);
}

/// @summary Compare two command line argument keys for equality. By default, the comparison ignores case differences.
/// @param key The zero-terminated key string, typically returned by ArgumentKeyAndValue.
/// @param match The zero-terminated key string to compare with, typically a string literal.
/// @param ignore_case Specify true to ignore differences in character case.
/// @return true if the string key is equivalent to the string match.
internal_function bool
KeyMatch
(
    TCHAR const        *key, 
    TCHAR const      *match, 
    bool        ignore_case = true
)
{
    if (ignore_case) return _tcsicmp(key, match) == 0;
    else return _tcscmp(key, match) == 0;
}

/// @summary Parse a command-line argument to retrieve the key or key-value pair. Codepoint '-', '--' or '/' are supported as lead characters on the key, and key-value pairs may be specified as 'key=value'.
/// @param arg The input argument string. Any equal codepoint '=' is replaced with a zero codepoint.
/// @param key On return, points to the start of the zero-terminated argument key, or to the zero codepoint.
/// @param val On return, points to the start of the zero-terminated argument value, or to the zero codepoint.
/// @return true if a key or key-value pair was extracted, or false if the input string is NULL or empty.
internal_function bool
ArgumentKeyAndValue
(
    TCHAR  *arg, 
    TCHAR **key, 
    TCHAR **val
)
{   // if the input string is NULL, return false - no key-value pair is returned.
    if (arg == NULL)
        return false;

    // the key and the value both start out pointing to the start of the argument string.
    *key = arg; *val = arg;

    // strip off any leading characters to find the start of the key.
    TCHAR  ch = *arg;
    while (ch)
    {
        if (ch == _T('/') || ch == _T('-') || ch == _T(' '))
        {   // skip the lead character.
           *key = ++arg;
        }
        else if (ch == _T('='))
        {   // replace the key-value separator with a zero codepoint.
           *arg++ = 0;
            break;
        }
        else
        {   // this character is part of the key, advance to the next codepoint.
            // this is the most likely case; we will advance until the end of the
            // string is reached or until the key-value separator is reached.
            arg++;
        }
        ch = *arg;
    }
    // arg points to the start of the value, or to the zero codepoint.
    *val = arg;
    // only return true if the key is not empty.
    return (*key[0] != 0);
}


/// @summary Initializes a command line arguments definition with the default program configuration.
/// @param args The command line arguments definition to populate.
/// @return true if the input command line arguments structure is valid.
internal_function bool
DefaultCommandLineArguments
(
    COMMAND_LINE *args
)
{
    if (args != NULL)
    {   // set the default command-line argument values.
        args->TraceFile = NULL;
        return true;
    }
    return false;
}

/// @summary Parse the command line specified when the application was launched.
/// @param args The command-line argument options to populate.
/// @return true if the command-line arguments were parsed.
internal_function bool
ParseCommandLine
(
    COMMAND_LINE *args
)
{
    LPTSTR  command_line   = GetCommandLine();
    int     argc           = 1;
#if defined(_UNICODE) || defined(UNICODE)
    LPTSTR *argv           = CommandLineToArgvW(command_line, &argc);
#else
    LPTSTR *argv           = CommandLineToArgvA(command_line, &argc);
#endif

    if (argc <= 1)
    {   // no command-line arguments were specified. return the default configuration.
        LocalFree((HLOCAL) argv);
        return DefaultCommandLineArguments(args);
    }

    for (size_t i = 1, n = size_t(argc); i < n; ++i)
    {
        TCHAR *arg = argv[i];
        TCHAR *key = NULL;
        TCHAR *val = NULL;

        if (ArgumentKeyAndValue(arg, &key, &val)) // may mutate arg
        {
            if (KeyMatch(key, _T("f")) || KeyMatch(key, _T("file")))
            {   // set the trace file to load.
                args->TraceFile = val;
            }
            else
            {   // the key is not recognized. output a debug message, but otherwise ignore the error.
                DebugPrintf(_T("Unrecognized command-line option %Id; got key = %s and val = %s.\n"), i, key, val);
            }
        }
        else
        {   // the key-value pair could not be parsed. output a debug message, but otherwise ignore the error.
            DebugPrintf(_T("Unparseable command-line option %Id; got key = %s and val = %s.\n"), i, key, val);
        }
    }

    return true;
}

/// @summary Attaches a console window to the application. This is useful for viewing debug output.
internal_function void
CreateConsoleAndRedirectStdio
(
    void
)
{
    if (AllocConsole())
    {   // a process can only have one console associated with it. this function just 
        // allocated that console, so perform the buffer setup and I/O redirection.
        // the default size is 120 characters wide and 30 lines tall.
        CONSOLE_SCREEN_BUFFER_INFO    buffer_info;
        SHORT const                 lines_visible = 9999;
        SHORT const                 chars_visible = 120;
        HANDLE                      console_stdin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE                     console_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE                     console_stderr = GetStdHandle(STD_ERROR_HANDLE);

        // set up the console size to be the Windows default of 120x30.
        GetConsoleScreenBufferInfo(console_stdout, &buffer_info);
        buffer_info.dwSize.X = chars_visible;
        buffer_info.dwSize.Y = lines_visible;
        SetConsoleScreenBufferSize(console_stdout,  buffer_info.dwSize);

        // redirect stdin to the new console window:
        int      new_stdin   = _open_osfhandle((intptr_t) console_stdin , _O_TEXT);
        FILE     *fp_stdin   = _fdopen(new_stdin, "r");
        *stdin = *fp_stdin;  setvbuf(stdin, NULL, _IONBF, 0);

        // redirect stdout to the new console window:
        int      new_stdout  = _open_osfhandle((intptr_t) console_stdout, _O_TEXT);
        FILE     *fp_stdout  = _fdopen(new_stdout, "w");
        *stdout= *fp_stdout; setvbuf(stdout, NULL, _IONBF, 0);

        // redirect stderr to the new console window:
        int      new_stderr  = _open_osfhandle((intptr_t) console_stderr, _O_TEXT);
        FILE     *fp_stderr  = _fdopen(new_stderr, "w");
        *stderr= *fp_stderr; setvbuf(stderr, NULL, _IONBF, 0);

        // synchronize everything that uses <iostream>.
        std::ios::sync_with_stdio();
    }
}

/// @summary Callback invoked by GLFW when an error occurs.
/// @param error The GLFW error code.
/// @param description A string description of the error.
internal_function void
GlfwErrorCallback
(
    int               error, 
    char const *description
)
{
    ConsoleError("ERROR: (GLFW 0x%08X) %s.\n", error, description); 
}

/// @summary Initialize the UI_STATE::TracePath field. The buffer is either initialized to empty or is initialized with an existing zero-terminated string.
/// @param dst The destination UI_STATE instance.
/// @param path A zero-terminated path string to copy, or NULL.
internal_function void
InitTracePath
(
    UI_STATE     *dst, 
    TCHAR const *path
)
{
    if (path == NULL)
    {   // zero out the path buffer.
        ZeroMemory(dst->TracePath, 32768 * sizeof(TCHAR));
    }
    else
    {   // copy the path string into the buffer.
        StringCchCopy(dst->TracePath, 32768, path);
    }
}

/// @summary Allocate and initialize a new application state instance. No trace file is loaded, and the command line trace path is not copied into the UI_STATE.
/// @param command_line Command-line arguments used to launch the application.
/// @param main_window The main application window.
/// @return A pointer to the new UI_STATE instance, or NULL.
internal_function UI_STATE*
NewUIState
(
    COMMAND_LINE *command_line, 
    GLFWwindow    *main_window
)
{   assert(command_line != NULL);
    assert(main_window  != NULL);

    UI_STATE *ui = NULL;
    if ((ui = (UI_STATE*) malloc(sizeof(UI_STATE))) == NULL)
    {
        ConsoleError("ERROR: Insufficient memory to create new UI_STATE.\n");
        return NULL;
    }
    ZeroMemory(ui, sizeof(UI_STATE));
    ui->TopLevelState   = UI_STATE_ID_NO_TRACE_LOADED;
    ui->CommandLine     = command_line;
    ui->MainWindow      = main_window;
    ui->ShowConsole     = false;
    return ui;
}

/// @summary Allocate and initialize a new application state instance, copying user interface state from an existing instance.
/// @param existing The existing user interface state to copy.
/// @return A pointer to the new UI_STATE instance, or NULL.
internal_function UI_STATE*
NewUIState
(
    UI_STATE *existing
)
{   assert(existing != NULL);
    UI_STATE *ui = NewUIState(existing->CommandLine, existing->MainWindow);
    if (ui)
    {   // copy over actual user interface state values, but not trace data.
        ui->ShowConsole = existing->ShowConsole;
        // TODO(rlk): copy other relevant fields...
    }
    return ui;
}

/// @summary Free resources associated with an application state instance.
/// @param ui The user interface state data to free.
internal_function void
DeleteUIState
(
    UI_STATE *ui
)
{
    if (ui != NULL)
    {
        if (ui->EventData != NULL)
        {   // this also sets ui->EventData to NULL.
            DeleteProfilerEvents(&ui->EventData);
        }
        free(ui);
    }
}

/// @summary Display a system open file dialog to select a trace to load. The trace path is copied into the UI_STATE::TracePath field.
/// @param ui The application user interface state data to update.
/// @return true if a trace file was selected.
internal_function bool
SelectTraceFile
(
    UI_STATE *ui
)
{
    OPENFILENAME   ofn;
    TCHAR  path[32768];
    ZeroMemory(&ofn , sizeof(OPENFILENAME));
    ZeroMemory(path , 32768 * sizeof(WCHAR));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner   = glfwGetWin32Window(ui->MainWindow);
    ofn.lpstrFilter = _T("Trace Files (*.etl)\0*.etl\0All Files (*.*)\0*.*\0");
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = 32767;
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    ofn.lpstrDefExt = _T("etl");
    
    if (GetOpenFileName(&ofn))
    {   // the new path is stored in 'path'.
        InitTracePath(ui, path);
        return true;
    }
    else
    {   // the user cancelled, or an error occurred.
        return false;
    }
}

/// @summary Construct and implement the logic for the primary application user interface.
/// @param ui The application user interface state to update.
internal_function void
BuildImgui
(
    UI_STATE *&ui
)
{
    ImGuiIO                       &io  = ImGui::GetIO();
    ImVec2           main_window_size  = io.DisplaySize;
    bool             main_window_open  = true;
    ImGuiWindowFlags main_window_flags = 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize   | 
        ImGuiWindowFlags_NoMove     | 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_MenuBar;
    
    bool exit_application = false;
    bool load_trace_file = false;
    bool show_console = false;

    // create the main IMGUI window within the client rect of the GLFW window.
    if (ImGui::Begin("TopLevel", &main_window_open, main_window_flags))
    {   // create the main menu:
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                ImGui::MenuItem("Load Trace...", NULL, &load_trace_file);
                ImGui::MenuItem("Exit", NULL, &exit_application);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Console", NULL, &show_console);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        // ensure that the main window takes up the entire display area.
        ImGui::SetWindowPos (ImVec2(0, 0));
        ImGui::SetWindowSize(io.DisplaySize);
        // render different views based on whether or not a trace is loaded.
        switch (ui->TopLevelState)
        {
            case UI_STATE_ID_NO_TRACE_LOADED:  break;
            case UI_STATE_ID_TRACE_LOADING:    break;
            case UI_STATE_ID_TRACE_LOADED:     break;
            case UI_STATE_ID_TRACE_LOAD_ERROR: break;
            default: break; /* serious error */
        }
        ImGui::End();
    }

    if (show_console && !ui->ShowConsole)
    {   // display the console output. an application is only allowed one 
        // console, so if the user closes it, the application exits.
        CreateConsoleAndRedirectStdio();
        ui->ShowConsole = true;
    }
    if (load_trace_file)
    {
        UI_STATE *new_ui = NewUIState(ui);
        if (SelectTraceFile(new_ui))
        {
            // TODO(rlk): a new trace file was loaded. re-initialize the UI.
            DeleteUIState(ui); ui = new_ui;
            ui->TopLevelState = UI_STATE_ID_TRACE_LOADING;
            ui->EventData = NewProfilerEvents(new_ui->TracePath);
            // ...
        }
        else
        {   // the user cancelled.
            DeleteUIState(new_ui);
        }
    }
    if (exit_application)
    {   // close the main window to trigger application shutdown.
        PostMessage(glfwGetWin32Window(ui->MainWindow), WM_CLOSE, 0, 0);
    }
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Implements the entry point of the application.
/// @param this_instance The module base address of the executable
/// @param prev_instance Unused. This value is always NULL.
/// @param command_line The zero-terminated command line string.
/// @param show_command One of SW_MAXIMIZED, etc. indicating how the main window should be displayed.
/// @return The application exit code, or 0 if the application terminates before entering the main message loop.
int WINAPI
WinMain
(
    HINSTANCE this_instance, 
    HINSTANCE prev_instance, 
    LPSTR      command_line, 
    int        show_command
)
{
    COMMAND_LINE         argv = {};
    GLFWwindow   *main_window = NULL;
    UI_STATE              *ui = NULL;

    UNREFERENCED_PARAMETER(this_instance);
    UNREFERENCED_PARAMETER(prev_instance);
    UNREFERENCED_PARAMETER(command_line);
    UNREFERENCED_PARAMETER(show_command);

    if (!ParseCommandLine(&argv))
    {   // bail out if the command line cannot be parsed successfully.
        DebugPrintf(_T("ERROR: Unable to parse the command line.\n"));
        return 1;
    }

    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
    {   // GlfwErrorCallback will have printed additional error information.
        return 1;
    }
    if ((main_window = glfwCreateWindow(1280, 720, "Thread Profile Visualizer", NULL, NULL)) == NULL)
    {   // GlfwErrorCallback will have printed additional error information.
        return 1;
    }
    glfwMakeContextCurrent(main_window);
    ImGui_ImplGlfw_Init(main_window, true);

    // run the main thread loop.
    ImVec4 clear_color = ImColor(114, 144, 154);
    if ((ui = NewUIState(&argv, main_window)) == NULL)
    {
        DebugPrintf(_T("ERROR: Unable to initialize application user interface state.\n"));
        return 1;
    }
    InitTracePath(ui, argv.TraceFile);

    // Main loop
    while (!glfwWindowShouldClose(main_window))
    {
        glfwPollEvents();
        ImGui_ImplGlfw_NewFrame();

        // construct the user interface.
        // this call may upadate the ui pointer.
        BuildImgui(ui);

        // convert the user interface to command buffer form and render.
        int display_w, display_h;
        glfwGetFramebufferSize(main_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        glfwSwapBuffers(main_window);
    }

    ImGui_ImplGlfw_Shutdown();
    DeleteUIState(ui);
    glfwTerminate();
    return 0;
}

