@echo off
:: Generate VC8 projects/solutions

set CURDIR=%CD%

mkdir vc8Win32 2> nul:
pushd vc8Win32

cmake -D OPT_DEST_VISUALSTUDIO:BOOL=ON -G "Visual Studio 8 2005" %CURDIR%
popd

mkdir vc8x64 2> nul:
pushd vc8x64

cmake -D OPT_DEST_VISUALSTUDIO:BOOL=ON -G "Visual Studio 8 2005 Win64" %CURDIR%
popd
