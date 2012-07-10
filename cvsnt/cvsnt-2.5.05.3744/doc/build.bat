@echo off
rmdir /q /s _tmp
mkdir _tmp
cd _tmp
sed 's/__VERSION__/%2/' <../%1.dbk >%1.dbk
xsltproc /usr/share/docbook-xsl/htmlhelp/htmlhelp.xsl %1.dbk
hhc htmlhelp.hhp 
cp htmlhelp.chm ../%1.chm
cd ..
rmdir /q /s _tmp
