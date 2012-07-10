#!/bin/sh
(shopt -s igncr) 2>/dev/null && shopt -s igncr; # cygwin bug workaround

function copy_headers {
   cp `grep -l _EXPORT $1/*.h` "${TARGET}/$2" 
}

rm -f ${TARGET}/*
rm -rf "${TARGET}/pdb"
rm -rf "${TARGET}/inc"
rm -rf "${TARGET}/lib"
rm -rf "${TARGET}/protocols"
rm -rf "${TARGET}/triggers"
rm -rf "${TARGET}/xdiff"
rm -rf "${TARGET}/database"
rm -rf "${TARGET}/mdns"
rm -rf "${TARGET}/sql"
mkdir "${TARGET}/inc"
mkdir "${TARGET}/inc/win32"
mkdir "${TARGET}/inc/unix"
mkdir "${TARGET}/inc/lib"
mkdir "${TARGET}/inc/diff"
mkdir "${TARGET}/lib"
mkdir "${TARGET}/pdb"
mkdir "${TARGET}/pdb/triggers"
mkdir "${TARGET}/pdb/protocols"
mkdir "${TARGET}/pdb/xdiff"
mkdir "${TARGET}/triggers"
mkdir "${TARGET}/protocols"
mkdir "${TARGET}/xdiff"
mkdir "${TARGET}/database"
mkdir "${TARGET}/mdns"
mkdir "${TARGET}/sql"
cd "${BASE}"
cp ${TYPE}/*.exe "${TARGET}"
cp ${TYPE}/*.dll "${TARGET}"
cp ${TYPE}/*.cpl "${TARGET}"
cp ${TYPE}/triggers/*.dll "${TARGET}/triggers"
cp ${TYPE}/protocols/*.dll "${TARGET}/protocols"
cp ${TYPE}/xdiff/*.dll "${TARGET}/xdiff"
cp ${TYPE}/database/*.dll "${TARGET}/database"
cp ${TYPE}/mdns/*.dll "${TARGET}/mdns"
cp ${TYPE}/setuid.dll "${SYSTEMROOT}/system32"
cp ${TYPE}/setuid.dll "${SYSTEMROOT}/system32/setuid2.dll"
cp ${TYPE}/cvsapi.lib "${TARGET}/lib"
cp ${TYPE}/cvstools.lib "${TARGET}/lib"
cp triggers/sql/*.sql "${TARGET}/sql"
cp relnotes.rtf "${TARGET}"
cp ca.pem "${TARGET}"
cp extnt.ini "${TARGET}"
cp protocol_map.ini "${TARGET}"
cp COPYING "${TARGET}"
cp doc/*.chm "${TARGET}"
cp version*.h "${TARGET}"/inc
cp build.h "${TARGET}"/inc
cp cvsapi/cvsapi.h "${TARGET}/inc"
cp cvsapi/win32/config.h "${TARGET}/inc/win32"
cp cvstools/cvstools.h "${TARGET}/inc"
copy_headers cvstools/win32 inc/win32
# copy_headers cvstools/unix inc/unix
copy_headers cvstools inc
copy_headers cvsapi/win32 inc/win32
# copy_headers cvsapi/unix inc/unix
copy_headers cvsapi/lib inc/lib
copy_headers cvsapi/diff inc/diff
copy_headers cvsapi inc

cd ${TARGET}
mv cvsnt.dll cvs.dll
for i in co cvs cvsservice extnt cvslock genkey rcsdiff rlog; do
  cp simcvs.exe $i.exe
done
cp simcpl.cpl cvsntcpl.cpl
