del *.zip
rmdir /s /q cvsnt
rmdir /s /q bin
rmdir /s /q pdb
mkdir bin
mkdir pdb
set PATH=c:\winnt;c:\winnt\system32;d:\cvsbin;c:\"program files"\winzip;c:\"program files"\putty
set INCLUDE=D:\kerberos\include;D:\Openssh\Include
set LIB=D:\kerberos\lib;D:\Openssh\Lib
call C:\"Program Files"\"Microsoft Visual Studio .NET 2003"\Vc7\bin\vcvars32.bat
cvs -d :pserver:tmh@cvs.cvsnt.org:/usr/local/cvs co -r CVSNT_2_0_x cvsnt
devenv /rebuild release cvsnt\cvsnt.sln
copy cvsnt\winrel\*.exe bin
copy cvsnt\winrel\*.dll bin
copy cvsnt\winrel\cvsnt.cpl bin
copy cvsnt\protocol_map.ini bin
copy cvsnt\src\infolib.h bin
copy cvsnt\winrel\*.pdb pdb
wzzip cvsnt_release_snapshot.zip bin
wzzip cvsnt_release_snapshot_pdb.zip pdb
rmdir /s /q cvsnt
rmdir /s /q bin
rmdir /s /q pdb
pscp -i d:\tony.ppk *.zip tmh@paris.nodomain.org:/var/www/cvsnt.org
