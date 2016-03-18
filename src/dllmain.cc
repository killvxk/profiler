/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implement the entry point of the profiler DLL. This file includes 
/// the actual profiler implementation, located in profiler.cc.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*////////////////
//   Includes   //
////////////////*/
#include <stddef.h>
#include <stdint.h>

#include <windows.h>

#include "provider.h"  // generated from the profiler_etw.man instrumentation manifest.
#include "profiler.h"  // manually written profiler loader interface.
#include "profiler.cc" // the public functions of the profiler interface that forward data to ETW.

/*//////////////////
//   Data Types   //
//////////////////*/

/*//////////////////////////
//   Internal Functions   //
//////////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Implement the entry point of the dynamic link library.
/// @param dll_instance The module base address of the DLL.
/// @param call_reason The reason the entry point is being called.
/// @param reserved This value is reserved for future use.
/// @return Non-zero if the entry point returns success.
BOOL WINAPI 
DllMain
(
    HINSTANCE dll_instance, 
    DWORD      call_reason, 
    LPVOID        reserved
)
{
    UNREFERENCED_PARAMETER(dll_instance);
    UNREFERENCED_PARAMETER(call_reason);
    UNREFERENCED_PARAMETER(reserved);
    return TRUE;
}

