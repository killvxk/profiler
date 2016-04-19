@ECHO OFF

SET VSVERSION_2013=12.0
SET VSVERSION_2015=14.0
SET VSVERSION=%VSVERSION_2013%

IF NOT DEFINED DevEnvDir (
    CALL "C:\Program Files (x86)\Microsoft Visual Studio %VSVERSION%\VC\vcvarsall.bat" x86_amd64
)

SET OUTPUTDIR="%~dp0build"
SET INCLUDES=-I..\include -I..\manifest -I..\src
SET LIBRARIES=User32.lib Gdi32.lib Shell32.lib Advapi32.lib Comdlg32.lib winmm.lib tdh.lib glfw3dll.lib opengl32.lib

SET DEFINES_COMMON=/D _WIN32_WINNT=0x0600 /D UNICODE /D _UNICODE /D _STDC_FORMAT_MACROS
SET DEFINES_DEBUG=%DEFINES_COMMON% /D DEBUG /D _DEBUG
SET DEFINES_RELEASE=%DEFINES_COMMON% /D NDEBUG /D _NDEBUG

SET CPPFLAGS_COMMON=%INCLUDES% /FC /nologo /W4 /WX /wd4505 /Zi /EHsc
SET CPPFLAGS_DEBUG=%CPPFLAGS_COMMON% /Od
SET CPPFLAGS_RELEASE=%CPPFLAGS_COMMON% /Ob2it

SET LNKFLAGS=%LIBRARIES%

IF NOT EXIST %OUTPUTDIR% mkdir %OUTPUTDIR%

@ECHO Copying GLFW3 support dll to "%OUTPUTDIR%...
XCOPY .\lib\%VSVERSION%\glfw3.dll %OUTPUTDIR% /Y /Q

IF [%1] NEQ [] (
    IF "%1" == "debug" (
        SET DEFINES=%DEFINES_DEBUG%
        SET CPPFLAGS=%CPPFLAGS_DEBUG%
        @ECHO Building debug configuration...
    ) ELSE (
        SET DEFINES=%DEFINES_RELEASE%
        SET CPPFLAGS=%CPPFLAGS_RELEASE%
        @ECHO Building release configuration - did you mean to specify "debug" instead of "%1"?
    )
) ELSE (
    SET DEFINES=%DEFINES_RELEASE%
    SET CPPFLAGS=%CPPFLAGS_RELEASE%
    @ECHO Building release configuration...
)

PUSHD %OUTPUTDIR%
cl %CPPFLAGS% ..\src\visualizer.cc %DEFINES% %LNKFLAGS% /Feprofviz.exe /link /LIBPATH:..\lib\%VSVERSION%
POPD

@ECHO Build complete.

