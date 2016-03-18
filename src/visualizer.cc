/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implement the entry point of the game server application.
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

/// @summary Define the maximum number of saved indentation levels.
#ifndef MAX_SAVED_INDENT_LEVELS
    #define MAX_SAVED_INDENT_LEVELS            64
#endif

/*////////////////
//   Includes   //
////////////////*/
#include <iostream>

#include <stddef.h>
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

#include <tchar.h>
#include <windows.h>
#include <Shellapi.h>

#include <conio.h>
#include <fcntl.h>
#include <io.h>

#include "profiler.h"

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Defines the data associated with a parsed command line.
struct COMMAND_LINE
{
    int Port;
};

/// @summary Provides scoped modification of the indentation level for the calling thread.
struct INDENT_SCOPE
{
    size_t Marker;               /// The marker returned by SaveConsoleIndent.
    INDENT_SCOPE(int indent=-1); /// Save the current indentation level and possibly apply a new level.
   ~INDENT_SCOPE(void);          /// Restore the indentation level at scope entry.
    void   Set(int indent);      /// Save the current indentation level and apply a new indentation level.
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary The current output indentation level.
global_variable __declspec(thread) int  GlobalIndent = 0;

/// @summary The zero-based index specifying the current top-of-stack for saved indent levels.
global_variable __declspec(thread) int  GlobalIndentTOS = 0;

/// @summary A stack used to save and restore the global indentation level.
global_variable __declspec(thread) int  GlobalIndentStack[MAX_SAVED_INDENT_LEVELS] = {};

/// @summary Helper macro to write a formatted string to stderr. The output will not be visible unless a console window is opened.
#define ConsoleError(formatstr, ...) \
    _ftprintf(stderr, _T("%*s") _T(formatstr), (GlobalIndent*2), _T(""), __VA_ARGS__)

/// @summary Helper macro to write a formatting string to stdout. The output will not be visible unless a console window is opened.
#define ConsoleOutput(formatstr, ...) \
    _ftprintf(stdout, _T("%*s") _T(formatstr), (GlobalIndent*2), _T(""), __VA_ARGS__)

/*//////////////////////////
//   Internal Functions   //
//////////////////////////*/
/// @summary Retrieves the current indentation level.
/// @return The current output indentation level for the calling thread.
internal_function int
GetConsoleIndent
(
    void
)
{
    return GlobalIndent;
}

/// @summary Sets the current indentation level for the calling thread, without saving the existing value.
/// @param indent The new indentation level.
internal_function void
SetConsoleIndent
(
    int indent
)
{
    if (indent >= 0)
    {   // only allow indentation levels >= 0.
        GlobalIndent = indent;
    }
}

/// @summary Saves the current indentation level for the calling thread, and applies a new indentation level.
/// @param new_indent The new indentation level, or -1 to keep the current indentation level.
/// @return The indentation stack marker representing the current level of indentation.
internal_function size_t
SaveConsoleIndent
(
    int new_indent = -1
)
{
    if (GlobalIndentTOS <  MAX_SAVED_INDENT_LEVELS)
    {
        size_t marker = GlobalIndentTOS;
        GlobalIndentStack[GlobalIndentTOS++] = GlobalIndent;
        SetConsoleIndent(new_indent);
        return marker;
    }
    return MAX_SAVED_INDENT_LEVELS-1;
}

/// @summary Restores the indentation level for the calling thread to the current value at the time of the last SaveIndent call.
internal_function void
RestoreConsoleIndent
(
    void
)
{
    if (GlobalIndentTOS >= 0)
    {   // restore the saved indentation level.
        int saved_indent = GlobalIndentStack[GlobalIndentTOS];
        SetConsoleIndent(saved_indent);
    }
    if (GlobalIndentTOS >  0)
    {   // pop from the stack.
        GlobalIndentTOS--;
    }
}

/// @summary Restores the indentation level for the calling thread to the value at the specified SaveIndent marker.
/// @param marker The value returned by a previous call to SaveIndent.
internal_function void
RestoreConsoleIndent
(
    size_t marker
)
{
    int saved_indent = -1;
    while (GlobalIndentTOS >= marker)
    {
        saved_indent =  GlobalIndentStack[GlobalIndentTOS];
        if (marker > 0) GlobalIndentTOS--;
    }
    SetConsoleIndent(saved_indent);
}

/// @summary Ignore the indentation level for the current thread. The current indentation level is saved; use RestoreIndent to restore it.
internal_function void
IgnoreConsoleIndent
(
    void
)
{
    SaveConsoleIndent(0);
}

/// @summary Increase the current indentation level for the calling thread by 1.
internal_function void
IncreaseConsoleIndent
(
    void
)
{
    SetConsoleIndent(GetConsoleIndent()+1);
}

/// @summary Decrease the current indentation level for the calling thread by 1.
internal_function void
DecreaseConsoleIndent
(
    void
)
{
    SetConsoleIndent(GetConsoleIndent()-1);
}

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

/// @summary Save the current indentation level and possibly apply a new indentation level.
/// @param indent The indentation level to apply, or -1 to leave the current level unchanged.
inline INDENT_SCOPE::INDENT_SCOPE(int indent /*=-1*/)
{
    Marker = SaveConsoleIndent(indent);
}

/// @summary Restore the saved indentation level when the object goes out of scope.
inline INDENT_SCOPE::~INDENT_SCOPE(void)
{
    RestoreConsoleIndent(Marker);
}

/// @summary Save the current indentation level and possibly apply a new indentation level.
/// @param indent The indentation level to apply, or -1 to leave the current level unchanged.
inline void INDENT_SCOPE::Set(int indent)
{
    Marker = SaveConsoleIndent(indent);
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
        args->Port = 0;
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
    //command_line_args_t cl = {};
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
            if (KeyMatch(key, _T("p")) || KeyMatch(key, _T("port")))
            {   // create a console window to view debug output.
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

    // finished with command line argument parsing; clean up and return.
    LocalFree((HLOCAL) argv);
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
    COMMAND_LINE argv;

    UNREFERENCED_PARAMETER(this_instance);
    UNREFERENCED_PARAMETER(prev_instance);
    UNREFERENCED_PARAMETER(command_line);
    UNREFERENCED_PARAMETER(show_command);

    if (!ParseCommandLine(&argv))
    {   // bail out if the command line cannot be parsed successfully.
        DebugPrintf(_T("ERROR: Unable to parse the command line.\n"));
        return 0;
    }
    CreateConsoleAndRedirectStdio();
    return 0;
}

