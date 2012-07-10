@echo off
rmdir /q /s _tmp
mkdir _tmp
cd _tmp
sed 's/__VERSION__/%1/' <../pdk.cfg >pdk.cfg
doxygen pdk.cfg
copy html\cvsnt-pdk.chm ..
cd ..
rmdir /q /s _tmp
