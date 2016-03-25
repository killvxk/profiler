@ECHO OFF

SET VSVERSION_2013=12.0
SET VSVERSION_2015=14.0
SET VSVERSION=%VSVERSION_2013%

IF NOT DEFINED DevEnvDir (
    CALL "C:\Program Files (x86)\Microsoft Visual Studio %VSVERSION%\VC\vcvarsall.bat" x86_amd64
)

start devenv /debugexe build\runtests.exe

