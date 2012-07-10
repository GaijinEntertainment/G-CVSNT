@echo off
set OLDPATH=%PATH%
rem set PATH=d:\cvsbin\release builder\cvsnt\WinDebug;D:\cvsdeps\sysfiles_2504;C:\oraclexe\Ora81\bin;C:\WINDOWS\system32;C:\WINDOWS;C:\WINDOWS\system32\WBEM;C:\Python24\.;C:\Perl\bin\;c:\cygwin\bin
rem set PATH=c:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\Bin\.
rem set PATH=C:\Program Files\Microsoft Platform SDK for Windows Server 2003 R2\Bin\WinNT\.
rem set PATH=C:\Program Files (x86)\Microsoft Visual Studio .NET 2003\Common7\Tools

PATH=d:\cvsbin\release builder\cvsnt\WinDebug;%PATH%

rmdir /s /q tree
rmdir /s /q repos
rmdir /s /q repos_0
python testcvs.py %1 %2 %3 %4
set PATH=%OLDPATH%
