@REM This script copies the resource-only DLL file profiler_r.dll to %TEMP% and registers the custom providers with Event Tracing for Windows. Any existing copy of the DLL is first unregistered.

@ECHO OFF

SET SCRIPT_ROOT=%~dp0
SET BUILD_ROOT=%~dp0build
SET MANIFEST_ROOT=%~dp0manifest

@REM Set the list of locations to look for profiler_r.dll. It should be located in the %TEMP% directory.
IF %TEMP:~-1% == \ (
    SET TEMP_ROOT=%TEMP:~0,-1%
) ELSE (
    SET TEMP_ROOT=%TEMP%
)
SET RESOURCE_DLL="%TEMP%\profiler_r.dll"
SET MANIFEST_FILE="%TEMP%\profiler_manifest.man"
ECHO Checking for existence of instrumentation manifest profiler_manifest.man...
IF EXIST %MANIFEST_FILE% (
    GOTO FoundManifest
) ELSE (
    GOTO NoManifest
)

:FoundManifest
ECHO Found instrumentation manifest at %MANIFEST_FILE%. 
ECHO Unregistering profiler event tracing provider(s)...
DEL /F /Q %RESOURCE_DLL%
WEVTUTIL UM %MANIFEST_FILE%
IF %ERRORLEVEL% == 5 (
    GOTO NoPermissions
)
DEL /F /Q %MANIFEST_FILE%
ECHO Profiler event tracing provider(s) unregistered successfully.
EXIT /b

:NoManifest
ECHO Profiler event tracing provider(s) unregistered successfully.
EXIT /b

:NoPermissions
ECHO ERROR: This script must be run as an administrator. Run cmd.exe as an Administrator and try again.
EXIT /b

