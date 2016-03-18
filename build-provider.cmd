@ECHO OFF

SET VSVERSION_2013=12.0
SET VSVERSION_2015=14.0
SET VSVERSION=%VSVERSION_2013%

IF NOT DEFINED DevEnvDir (
    CALL "C:\Program Files (x86)\Microsoft Visual Studio %VSVERSION%\VC\vcvarsall.bat" x86_amd64
)

SET OUTPUTDIR="%~dp0build"
SET MANIFESTDIR="%~dp0manifest"
SET INCLUDES=-I..\include -I..\manifest -I..\src
SET DEFINES=/D _WIN32_WINNT=0x06000 /D UNICODE /D _UNICODE /D BUILD_STATIC /D _STDC_FORMAT_MACROS /D _CRT_SECURE_NO_WARNINGS /D _USRDLL
SET CPPFLAGS=%INCLUDES% /FC /nologo /W4 /WX /wd4505 /Zi /EHsc /LD /Ob2it
SET LIBRARIES=User32.lib Gdi32.lib Shell32.lib Advapi32.lib winmm.lib
SET LNKFLAGS=%LIBRARIES%

IF NOT EXIST %MANIFESTDIR% mkdir %MANIFESTDIR%
IF NOT EXIST %OUTPUTDIR% mkdir %OUTPUTDIR%

PUSHD %MANIFESTDIR%
mc.exe -um profiler_manifest.man -z provider
rc.exe /nologo provider.rc
POPD

ECHO Copying support files (profiler_manifest.man, profiler.wprp, profiler_enable, profiler_disable.cmd) to %OUTPUTDIR%...
XCOPY /Y /Q %MANIFESTDIR%\profiler_manifest.man %OUTPUTDIR%
XCOPY /Y /Q profiler.wprp %OUTPUTDIR%
XCOPY /Y /Q profiler_enable.cmd %OUTPUTDIR%
XCOPY /Y /Q profiler_disable.cmd %OUTPUTDIR%

PUSHD %OUTPUTDIR%
link.exe /dll /noentry /machine:x64 /nologo /out:profiler_r.dll ..\manifest\provider.res
POPD

