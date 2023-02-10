@echo off
:: Generate VC9 projects/solutions
set CMAKE_ROOT=D:\cvsnt\cmake\bin\
set CURDIR=%CD%


mkdir vc16x64 2> nul:
pushd vc16x64

cmake -D OPT_DEST_VISUALSTUDIO:BOOL=ON -G "Visual Studio 16 2019" -A x64 "%CURDIR%"
popd
