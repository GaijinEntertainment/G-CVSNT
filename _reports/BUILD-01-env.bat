@echo off
rem ---- Standalone MSVC toolchain (no Visual Studio installed) ----
set VCTOOLS=D:\devtools\vc2022_17.14.4
set WSDK=D:\devtools\win.sdk.100
set SDKVER=10.0.22621.0
set RCDIR=%WSDK%\bin\10.0.18362.0\x64

set PATH=%VCTOOLS%\bin\Hostx64\x64;%RCDIR%;%PATH%

set INCLUDE=%VCTOOLS%\include;%WSDK%\include\%SDKVER%\ucrt;%WSDK%\include\%SDKVER%\shared;%WSDK%\include\%SDKVER%\um;%WSDK%\include\%SDKVER%\winrt;%WSDK%\include\%SDKVER%\cppwinrt

set LIB=%VCTOOLS%\lib\x64;%WSDK%\lib\%SDKVER%\ucrt\x64;%WSDK%\lib\%SDKVER%\um\x64

set LIBPATH=%LIB%
