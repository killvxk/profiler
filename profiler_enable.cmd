@REM This script copies the resource-only DLL file profiler_r.dll to %TEMP% and registers the custom providers with Event Tracing for Windows. Any existing copy of the DLL is first unregistered.

@ECHO OFF

SET SCRIPT_ROOT=%~dp0
SET BUILD_ROOT=%~dp0build
SET MANIFEST_ROOT=%~dp0manifest

@REM Set the list of locations to look for profiler_r.dll. Typically it will be in the same directory as this file.
SET RESOURCE_DLL0="%SCRIPT_ROOT%profiler_r.dll"
SET RESOURCE_DLL1="%BUILD_ROOT%\profiler_r.dll"
SET RESOURCE_DLL=%RESOURCE_DLL0%
IF EXIST %RESOURCE_DLL% (
    GOTO FoundResourceDLL
)
SET RESOURCE_DLL=%RESOURCE_DLL1%
IF EXIST %RESOURCE_DLL% (
    GOTO FoundResourceDLL
) ELSE (
    GOTO NoResourceDLL
)

:FoundResourceDLL
@REM Set the list of locations to look for profiler_manifest.man. Typically it will be in the same directory as this file.
SET MANIFEST_FILE0="%SCRIPT_ROOT%profiler_manifest.man"
SET MANIFEST_FILE1="%BUILD_ROOT%\profiler_manifest.man"
SET MANIFEST_FILE2="%MANIFEST_ROOT%\profiler_manifest.man"
SET MANIFEST_FILE=%MANIFEST_FILE0%
IF EXIST %MANIFEST_FILE% (
    GOTO FoundManifest
)
SET MANIFEST_FILE=%MANIFEST_FILE1%
IF EXIST %MANIFEST_FILE% (
    GOTO FoundManifest
)
SET MANIFEST_FILE=%MANIFEST_FILE2%
IF EXIST %MANIFEST_FILE% (
    GOTO FoundManifest
) ELSE (
    GOTO NoManifest
)

:FoundManifest
ECHO Found profiler_manifest.man at %MANIFEST_FILE%.
ECHO Registering profiler event tracing provider(s)...
XCOPY /y %RESOURCE_DLL% %TEMP%
WEVTUTIL UM %MANIFEST_FILE%
IF %ERRORLEVEL% == 5 (
    GOTO NoPermissions
)
WEVTUTIL IM %MANIFEST_FILE%
ECHO Profiler event tracing provider(s) registered successfully.
EXIT /b

:NoResourceDLL
ECHO ERROR: The profiler resource library profiler_r.dll could not be found. Copy it to this directory and try again.
EXIT /b

:NoManifest
ECHO ERROR: The profiler instrumentation manifest profiler_manifest.man could not be found. Copy it to this directory and try again.
EXIT /b

:NoPermissions
ECHO ERROR: This script must be run as an administrator. Run cmd.exe as an Administrator and try again.
EXIT /b

