@echo off
IF %1~==~ GOTO none
IF %2~==~ GOTO none
call "c:\Program Files\Microsoft Visual Studio 9.0\VC\vcvarsall.bat" x86
set PATH=%PATH%;D:\cvsdeps\x32\libxml2\bin
set CYGWIN=nontsec %CYGWIN%
bash -x release_builder.sh %1 %2 %3 %4 2>&1 >release_builder.log
pause
exit /b
:none
echo You must specify a branch name (eg: CVSNT_2_0_x)
echo Usage: release_builder.bat cvsnt-branch wm-branch [flightmode] [nobuild]
pause
exit /b
