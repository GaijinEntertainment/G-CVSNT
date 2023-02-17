set PATH=%PATH%;"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Tools\MSVC\14.29.30037\bin\Hostx64\x64"
nmake -f makefile.vc BUILD=debug UNICODE=1 TARGET_CPU=AMD64
nmake -f makefile.vc BUILD=release UNICODE=1 TARGET_CPU=AMD64