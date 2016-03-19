@ECHO OFF

SET VSVERSION_2013=12.0
SET VSVERSION_2015=14.0
SET VSVERSION=%VSVERSION_2013%

IF NOT DEFINED DevEnvDir (
    CALL "C:\Program Files (x86)\Microsoft Visual Studio %VSVERSION%\VC\vcvarsall.bat" x86_amd64
)

SET OUTPUTDIR="%~dp0build"
SET INCLUDES=-I..\include -I..\manifest -I..\src
SET DEFINES=/D _WIN32_WINNT=0x06000 /D UNICODE /D _UNICODE /D BUILD_STATIC /D _STDC_FORMAT_MACROS /D _CRT_SECURE_NO_WARNINGS /D _USRDLL /D ENABLE_PROFILER=1
SET CPPFLAGS=%INCLUDES% /FC /nologo /W4 /WX /wd4505 /Zi /EHsc /Ob2it
SET LIBRARIES=User32.lib Gdi32.lib Shell32.lib Advapi32.lib winmm.lib
SET LNKFLAGS=%LIBRARIES%

IF NOT EXIST %OUTPUTDIR% mkdir %OUTPUTDIR%

PUSHD %OUTPUTDIR%
cl %CPPFLAGS% ..\src\dllmain.cc %DEFINES% %LNKFLAGS% /Feprofiler_p.dll /link /dll /def:..\profiler.def
POPD

