cd D:\cvsnt\GnuWin32\bin
if errorlevel 1 goto :VCReportError
D:
if errorlevel 1 goto :VCReportError
D:\CVSNT\GnuWin32\bin\bison .exe --name-prefix=getdate_ -o D:/cvsnt/cvsnt-2.5.05.3744/tortoiseCVS/TortoiseCVS/build/vc9Win32/cvstree/bison_parser.cpp -d D:/cvsnt/cvsnt-2.5.05.3744/tortoiseCVS/TortoiseCVS/src/cvstree/parser.yy -S yacc.c
if errorlevel 1 goto :VCReportError"
