/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implement the entry point of the profile visualizer application.
///////////////////////////////////////////////////////////////////////////80*/

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

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Defines the data associated with a parsed command line.
struct COMMAND_LINE
{
    TCHAR *TraceFile;
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

/// @summary Find the end of a zero-terminated string.
/// @param str The zero-terminated string.
/// @return A pointer to the zero terminator, or NULL if the input string is NULL.
internal_function TCHAR*
StringEnd
(
    TCHAR *str
)
{
    if (str != NULL)
    {
        TCHAR  *iter = str;
        while (*iter)
        {
            ++iter;
        }
        return iter;
    }
    return NULL;
}

/// @summary Determine whether a character represents a decimal digit.
/// @param ch The character to inspect.
/// @return true if the specified character is a decimal digit (0-9).
internal_function bool
IsDigit
(
    TCHAR ch
)
{
    return (ch >= _T('0') && ch <= _T('9'));
}

/// @summary Parse a string representation of an unsigned 32-bit integer.
/// @param first A pointer to the first codepoint to inspect.
/// @param last A pointer to the last codepoint to inspect.
/// @param result On return, stores the integer value.
/// @return A pointer to the first codepoint that is not part of the number, in [first, last].
internal_function TCHAR*
StrToUInt32
(
    TCHAR     *first, 
    TCHAR      *last, 
    uint32_t &result

)
{
    uint32_t num = 0;
    for ( ; first != last && IsDigit(*first); ++first)
    {
        num = 10 * num + (*first - _T('0'));
    }
    result = num;
    return first;
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

/// @summary Retrieve a 32-bit unsigned integer property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The integer value.
internal_function inline uint32_t
ProfilerGetUInt32
(
    EVENT_RECORD           *ev, 
    TRACE_EVENT_INFO *info_buf, 
    size_t               index
)
{
    PROPERTY_DATA_DESCRIPTOR dd;
    uint32_t  value =  0;
    dd.PropertyName = (ULONGLONG)((uint8_t*) info_buf + info_buf->EventPropertyInfoArray[index].NameOffset);
    dd.ArrayIndex   =  ULONG_MAX;
    dd.Reserved     =  0;
    TdhGetProperty(ev, 0, NULL, 1, &dd, (ULONG) sizeof(uint32_t), (PBYTE) &value);
    return value;
}

/// @summary Retrieve an 8-bit signed integer property value from an event record.
/// @param ev The EVENT_RECORD passed to TaskProfilerRecordEvent.
/// @param info_buf The TRACE_EVENT_INFO containing event metadata.
/// @param index The zero-based index of the property to retrieve.
/// @return The integer value.
internal_function inline int8_t
ProfilerGetSInt8
(
    EVENT_RECORD           *ev, 
    TRACE_EVENT_INFO *info_buf, 
    size_t               index
)
{
    PROPERTY_DATA_DESCRIPTOR dd;
    int8_t    value =  0;
    dd.PropertyName = (ULONGLONG)((uint8_t*) info_buf + info_buf->EventPropertyInfoArray[index].NameOffset);
    dd.ArrayIndex   =  ULONG_MAX;
    dd.Reserved     =  0;
    TdhGetProperty(ev, 0, NULL, 1, &dd, (ULONG) sizeof(int8_t), (PBYTE) &value);
    return value;
}

/// @summary Callback invoked for each event reported by Event Tracing for Windows.
/// @param ev Data associated with the event being reported.
internal_function void WINAPI
ProfilerRecordEvent
(
    EVENT_RECORD *ev
)
{
    WIN32_PROFILER_EVENTS *profiler = (WIN32_PROFILER_EVENTS*) ev->UserContext;
    TRACE_EVENT_INFO      *info_buf = (TRACE_EVENT_INFO*) profiler->EventBuffer;
    ULONG                  size_buf = (ULONG) profiler->EventBufferSize;

    // attempt to parse out the context switch information from the EVENT_RECORD.
    if (TdhGetEventInformation(ev, 0, NULL, info_buf, &size_buf) == ERROR_SUCCESS)
    {   // this involves some ridiculous parsing of data in an opaque buffer.
        // there are some complications with context switch events:
        // 1. we probably don't know what the 'profiled' process and thread IDs are yet.
        //    - this means that all of the event info needs to be buffered.
        // 2. the ProcessId and ThreadId values in the event header are not valid.
        //    - need to look at Opcode 1,2,3,4 to retrieve thread start/stop information.
        // 
        if (IsEqualGUID(info_buf->ProviderGuid, SystemTraceControlGuid) && 
            IsEqualGUID(info_buf->EventGuid   , KernelThreadEventGuid)  && 
            info_buf->EventDescriptor.Opcode == 36)
        {   // opcode 36 corresponds to a CSwitch event.
            // see https://msdn.microsoft.com/en-us/library/windows/desktop/aa964744%28v=vs.85%29.aspx
            // all context switch event handling occurs on the same thread.
            WIN32_CONTEXT_SWITCH              cswitch;
            cswitch.CurrThreadId            = ProfilerGetUInt32(ev, info_buf,  0); // NewThreadId
            cswitch.PrevThreadId            = ProfilerGetUInt32(ev, info_buf,  1); // OldThreadId
            cswitch.WaitTimeMs              = ProfilerGetUInt32(ev, info_buf, 10); // NewThreadWaitTime
            cswitch.WaitReason              = ProfilerGetSInt8 (ev, info_buf,  6); // OldThreadWaitReason
            cswitch.WaitMode                = ProfilerGetSInt8 (ev, info_buf,  7); // OldThreadWaitMode
            cswitch.PrevThreadPriority      = ProfilerGetSInt8 (ev, info_buf,  3); // OldThreadPriority
            cswitch.CurrThreadPriority      = ProfilerGetSInt8 (ev, info_buf,  2); // NewThreadPriority
            profiler->CSwitchTime.push_back(uint64_t(ev->EventHeader.TimeStamp.QuadPart));
            profiler->CSwitchData.push_back(cswitch);
        }
    }
}

/// @summary Implements the entry point of the thread that dispatches ETW context switch events.
/// @param argp A pointer to the WIN32_TASK_PROFILER to which events will be logged.
/// @return Zero (unused).
internal_function unsigned int __stdcall
EventConsumerThreadMain
(
    void *argp
)
{   // wait for the go signal from the main thread.
    // this ensures that state is properly set up.
    DWORD               wait_result =  WAIT_OBJECT_0;
    WIN32_PROFILER_EVENTS *profiler = (WIN32_PROFILER_EVENTS*) argp;
    if ((wait_result = WaitForSingleObject(profiler->ConsumerLaunch, INFINITE)) != WAIT_OBJECT_0)
    {   // the wait failed for some reason, so terminate early.
        ConsoleError("ERROR (%S): Event consumer wait for launch failed with result %08X (%08X).\n", __FUNCTION__, wait_result, GetLastError());
        return 1;
    }

    // because real-time event capture is being used, the ProcessTrace function 
    // doesn't return until the capture session is stopped by calling CloseTrace.
    // ProcessTrace sorts events by timestamp and calls TaskProfilerRecordEvent.
    ULONG result  = ProcessTrace(&profiler->ConsumerHandle, 1, NULL, NULL);
    if   (result != ERROR_SUCCESS)
    {   // the thread is going to terminate because an error occurred.
        ConsoleError("ERROR (%S): Context switch consumer terminating with result %08X.\n", __FUNCTION__, result);
        return 1;
    }

    // all events have been consumed, so close the trace session.
    CloseHandle(profiler->ConsumerLaunch); profiler->ConsumerLaunch = NULL;
    CloseTrace(profiler->ConsumerHandle); profiler->ConsumerHandle = NULL;
    return 0;
}

/// @summary Implements the WndProc for the main game window.
/// @param window The window receiving the message.
/// @param message The message identifier.
/// @param wparam Additional message-specific data.
/// @param lparam Additional message-specific data.
/// @return The message-specific result code.
internal_function LRESULT CALLBACK
MainWindowCallback
(
    HWND    window, 
    UINT   message, 
    WPARAM  wparam, 
    LPARAM  lparam
)
{   // WM_NCCREATE performs special handling to store the WIN32_DISPLAY_THREAD_ARGS pointer in the window data.
    // the handler for WM_NCCREATE executes before the call to CreateWindowEx returns in CreateWindowOnDisplay.
    if (message == WM_NCCREATE)
    {   // store the WIN32_DISPLAY_THREAD_ARGS in the window user data.
        CREATESTRUCT *cs = (CREATESTRUCT*) lparam;
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR) cs->lpCreateParams);
        return DefWindowProc(window, message, wparam, lparam);
    }

    // this WndProc may receive several messages before receiving WM_NCCREATE.
    // if the user data hasn't been set yet, pass through to the default handler.
    /*WIN32_DISPLAY_THREAD_ARGS *thread_args = (WIN32_DISPLAY_THREAD_ARGS*) GetWindowLongPtr(window, GWLP_USERDATA);
    if (thread_args == NULL)
    {   // let the default implementation handle the message.
        return DefWindowProc(window, message, wparam, lparam);
    }*/

    // process all other messages sent (or posted) to the window.
    LRESULT result = 0;
    switch (message)
    {
        case WM_ACTIVATE:
            {   // wparam is TRUE if the window is being activated, or FALSE if the window is being deactivated. 
            } break;

        case WM_CLOSE:
            {   // completely destroy the main window. a WM_DESTROY message is 
                // posted that in turn causes the WM_QUIT message to be posted.
                DestroyWindow(window);
            } break;

        case WM_DESTROY:
            {   // post the WM_QUIT message to be picked up in the main loop.
                PostQuitMessage(0);
            } break;

        default:
            {   // pass the message to the default handler.
                result = DefWindowProc(window, message, wparam, lparam);
            } break;
    }
    return result;
}

/// @summary Create a new window on a given display.
/// @param this_instance The HINSTANCE of the application (passed to WinMain) or GetModuleHandle(NULL).
/// @param width The width of the window, or 0 to use the entire width of the display.
/// @param height The height of the window, or 0 to use the entire height of the display.
/// @return The handle of the new window, or NULL.
internal_function HWND
CreateMainWindow
(
    HINSTANCE this_instance,
    void       *thread_args,
    int               width=CW_USEDEFAULT, 
    int              height=CW_USEDEFAULT
)
{
    TCHAR const *class_name = _T("ProfViz_WndClass");
    WNDCLASSEX     wndclass = {};

    // register the window class, if necessary.
    if (!GetClassInfoEx(this_instance, class_name, &wndclass))
    {   // the window class hasn't been registered yet.
        wndclass.cbSize         = sizeof(WNDCLASSEX);
        wndclass.cbClsExtra     = 0;
        wndclass.cbWndExtra     = sizeof(void*);
        wndclass.hInstance      = this_instance;
        wndclass.lpszClassName  = class_name;
        wndclass.lpszMenuName   = NULL;
        wndclass.lpfnWndProc    = MainWindowCallback;
        wndclass.hIcon          = LoadIcon  (0, IDI_APPLICATION);
        wndclass.hIconSm        = LoadIcon  (0, IDI_APPLICATION);
        wndclass.hCursor        = LoadCursor(0, IDC_ARROW);
        wndclass.style          = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        wndclass.hbrBackground  = NULL;
        if (!RegisterClassEx(&wndclass))
        {   // unable to register the window class, cannot proceed.
            return NULL;
        }
    }

    // create a new window (not yet visible) at location (0, 0) on the display.
    int   x        = CW_USEDEFAULT;
    int   y        = CW_USEDEFAULT;
    DWORD style_ex = 0;
    DWORD style    =(WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME) | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    TCHAR*title    =_T("Profile Visualizer");
    HWND  hwnd     = CreateWindowEx(style_ex, class_name, title, style, x, y, width, height, NULL, NULL, this_instance, thread_args);
    if   (hwnd  == NULL)
    {   // the window cannot be created.
        return NULL;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    return hwnd;
}

/// @summary Allocate resources for a new WIN32_PROFILER_EVENTS container, open a trace session and consume the event data.
/// @param ev The profiler events container to initialize.
/// @param trace_file A NULL-terminated string specifying the path of the file to load.
internal_function int
NewProfilerEvents
(
    WIN32_PROFILER_EVENTS         *ev, 
    TCHAR const           *trace_file
)
{   
    TRACEHANDLE           trace = INVALID_PROCESSTRACE_HANDLE;
    EVENT_TRACE_LOGFILE logfile = {};
    HANDLE               thread = NULL;
    unsigned int            tid = 0;

    // initialize the fields of the events structure.
    ev->EventBuffer      = NULL;
    ev->EventBufferSize  = 0;
    ev->ConsumerHandle   = INVALID_PROCESSTRACE_HANDLE;
    ev->ConsumerLaunch   = CreateEvent(NULL, TRUE, FALSE, NULL); // manual-reset
    ev->ConsumerThread   = NULL;
    ev->ConsumerThreadId = 0;

    // attempt to open the trace file from the supplied path.
    logfile.LogFileName         = (TCHAR*)trace_file;
    logfile.LoggerName          = NULL;
    logfile.ProcessTraceMode    = PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    logfile.EventRecordCallback = ProfilerRecordEvent;
    logfile.Context             = ev;
    if ((trace = OpenTrace(&logfile)) == INVALID_PROCESSTRACE_HANDLE)
    {   // if the trace cannot be opened, there's no point in continuing.
        ConsoleError("ERROR (%S): Unable to open the trace session (%08X).\n", __FUNCTION__, GetLastError());
        CloseHandle(ev->ConsumerLaunch); ev->ConsumerLaunch = NULL;
        return -1;
    }

    // start a background thread to receive events read from the trace file.
    if ((thread = (HANDLE)_beginthreadex(NULL, 0, EventConsumerThreadMain, ev, 0, &tid)) == NULL)
    {   // without the background thread, the profiler can't receive the events.
        ConsoleError("ERROR (%S): Unable to start event consumer thread (errno = %d).\n", __FUNCTION__, errno);
        CloseHandle(ev->ConsumerLaunch); ev->ConsumerLaunch = NULL;
        CloseTrace(trace);
        return -1;
    }

    // TODO(rlk): might want to move to a memory arena system based around VirtualAlloc.
    // this would provide better performance and probably lead to less memory waste.
    ev->EventBuffer      = (uint8_t*) malloc(64 * 1024); // 64KB
    ev->EventBufferSize  = 64 * 1024;
    ev->ConsumerHandle   = trace;
    ev->ConsumerThread   = thread;
    ev->ConsumerThreadId = tid;

    // initialize the storage for the context switch track. context switch events
    // occur at an extremely high frequency (10000+/core/second).
    ev->CSwitchTime.clear(); ev->CSwitchTime.reserve(1000000);
    ev->CSwitchData.clear(); ev->CSwitchData.reserve(1000000);

    // TODO(rlk): initialize other event data containers here.
    // ...

    // start populating our data structures with event data from the trace file.
    SetEvent(ev->ConsumerLaunch);
    return 0;
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
    WIN32_PROFILER_EVENTS events;
    COMMAND_LINE            argv;
    HWND             main_window;
    bool           keep_running = true;

    UNREFERENCED_PARAMETER(this_instance);
    UNREFERENCED_PARAMETER(prev_instance);
    UNREFERENCED_PARAMETER(command_line);
    UNREFERENCED_PARAMETER(show_command);

    if (!ParseCommandLine(&argv))
    {   // bail out if the command line cannot be parsed successfully.
        DebugPrintf(_T("ERROR: Unable to parse the command line.\n"));
        return 0;
    }
    if ((main_window = CreateMainWindow(this_instance, NULL)) == NULL)
    {   // bail out if the main window cannot be created.
        DebugPrintf(_T("ERROR: Unable to create the main application window.\n"));
        return 0;
    }
    CreateConsoleAndRedirectStdio();

    // test test load a trace file
    if (argv.TraceFile != NULL)
    {
        NewProfilerEvents(&events, argv.TraceFile);
    }

    // run the main thread loop.
    while (keep_running)
    {   
        MSG msg;

        // dispatch windows messages while messages are available.
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message != WM_QUIT)
            {   // dispatch the message to MainWindowCallback.
                TranslateMessage(&msg);
                DispatchMessage (&msg);
            }
            else
            {   // terminate the main loop because the window was closed.
                keep_running = false;
                break;
            }
        }

        if (!keep_running)
        {   // the render thread has been instructed to terminate.
            break;
        }

        // TODO(rlk): Present the backbuffer
        Sleep(16);
    }

    if (events.ConsumerHandle != NULL)
    {
        WaitForSingleObject(events.ConsumerThread, INFINITE);
    }
    return 0;
}

