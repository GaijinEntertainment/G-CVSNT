@echo off
rem Check that we have the needed environment variable and command line arguments
if '%1'=='' goto errorparm1
if '%2'=='' goto errorparm2
if '%DOCBOOK%'=='' goto errordocbook

echo Removing and recreating temp folder with contents
rmdir /q /s _tmp
mkdir _tmp
cd _tmp
echo Running sed on the %1.dbk file
sed "s/__VERSION__/%2/" <..\%1.dbk >%1.dbk
echo Now processing with xsltproc.exe
xsltproc %DOCBOOK%\htmlhelp\htmlhelp.xsl %1.dbk
echo Compiling the help file
hhc htmlhelp.hhp
echo Copy result to final file %1.chm
copy htmlhelp.chm ..\%1.chm
cd ..
rem rmdir /q /s _tmp
goto exit

:errorparm1
echo Missing parameter1 specifying which dbk file to process
goto exit

:errorparm2
echo Missing parameter2 specifying which version number to embed in help file
goto exit

:errordocbook
echo Missing variable DOCBOOK specifying where to find the docbook templates
goto exit

:exit
