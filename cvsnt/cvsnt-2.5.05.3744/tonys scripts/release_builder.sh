#!/bin/bash
echo "*************************************************"
echo "*****                                       *****"
echo "*****   CVSNT Community/Commercial Release  *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"
(shopt -s igncr) 2>/dev/null && shopt -s igncr; # cygwin bug workaround
BRANCH=$1
if [ "x$1" == "x" ]; then
  BRANCH="CVSNT_2_0_x"
fi
WMBRANCH=$2
if [ "x$2" == "x" ]; then
  WMBRANCH="HEAD"
fi

CYGWIN="nontsec $CYGWIN"; export CYGWIN

CVSHOSTROOT=":pserver:tmh@cvs.cvsnt.org:/usr/local/cvs" 
WMHOSTROOT=":pserver:tmh@intranet.unifacecm.com:/scotty" 
if [ "x$USERNAME" == "xJohn" -o "x$USERNAME" == "xabarrett" -o "x$USERNAME" == "xArthur Barrett" -o "x$USERNAME" == "xArthur" ]; then
#  CVSHOSTROOT=":ssh:abarrett@cvs.cvsnt.org:/usr/local/cvs" 
  CVSHOSTROOT=":ssh:abarrett@intranet.unifacecm.com:/cvs" 
#  WMHOSTROOT=":ssh:abarrett@debiancvs:/scotty" 
  WMHOSTROOT=":ssh:abarrett@intranet.unifacecm.com:/scotty" 
fi

OPTIONS=$3
PROGFILES="Program Files"
if [ "x$1" != "xCVSNT_BRANCH_2_5_04_3236" -a "x$1" != "xCVSNT_2_0_x" -a "x$1" != "xTrunk" ]; then
   PATH=/cygdrive/d/cvsbin:/cygdrive/d/cvsdeps/2503/sysfiles:${PATH}:/cygdrive/c/"$PROGFILES"/winzip:/cygdrive/c/"$PROGFILES"/PuTTY:/cygdrive/c/"$PROGFILES"/"HTML Help Workshop"
   INCLUDE="/cvsdeps/2503/openssl/include;/cvsdeps/2503/iconv/include"
   LIB="/cvsdeps/2503/openssl/lib;/cvsdeps/2503/iconv/lib"
else
   if [ "x$1" != "xTrunk" ]; then
     PATH=/cygdrive/d/cvsbin:/cygdrive/d/cvsdeps/2505/sysfiles:${PATH}:/cygdrive/c/"$PROGFILES"/winzip:/cygdrive/c/"$PROGFILES"/PuTTY:/cygdrive/c/"$PROGFILES"/"HTML Help Workshop"
     INCLUDE="/cvsdeps/2505/openssl/include;/cvsdeps/2505/iconv/include"
     LIB="/cvsdeps/2505/openssl/lib;/cvsdeps/2505/iconv/lib"
   else
     PATH=/cygdrive/d/cvsbin:/cygdrive/d/cvsdeps/sysfiles:${PATH}:/cygdrive/c/"$PROGFILES"/winzip:/cygdrive/c/"$PROGFILES"/PuTTY:/cygdrive/c/"$PROGFILES"/"HTML Help Workshop"
     INCLUDE="/cvsdeps/openssl/include;/cvsdeps/iconv/include"
     LIB="/cvsdeps/openssl/lib;/cvsdeps/iconv/lib"
   fi
fi

if [ "x$3" != "xflightmode" -a "x$3" != "xquik" ]; then
  rm -rvf cvsnt
fi
TOP="`pwd`"
if [ "x$3" != "xflightmode" -a "x$3" != "xquik" ]; then
  echo "*************************************************"
  echo "*****                                       *****"
  echo "*****      Checkout CVSNT source code       *****"
  echo "*****                                       *****"
  echo "*************************************************"
  THEDATE="`date`"
  echo "$THEDATE"
  cvs -d ${CVSHOSTROOT} co -r $BRANCH cvsnt
  if [ ! -f cvsnt/src/update.cpp ]; then
    cvs -d ${CVSHOSTROOT} co -r $BRANCH cvsnt
  fi
fi

cd cvsnt
if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" ]; then
  rm -rf ReleaseWin32 winrel x64
  mkdir winrel
  mkdir ReleaseWin32
  mkdir x64
  mkdir x64\Release64
fi

if [ "x$1" != "xCVSNT_BRANCH_2_5_04_3236" -a "x$1" != "xCVSNT_2_0_x" -a "x$1" != "xTrunk" ]; then
   echo "Building Commercial Suite type of release"
   ORATYPE=1; export ORATYPE
   if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" ]; then
     devenv.com /rebuild release cvsnt.sln
   fi
else
   if [ "x$1" != "xTrunk" ]; then
     echo "Building CVSNT Community release"
     export CL=/DCOMMERCIAL_RELEASE
     if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" ]; then
       echo "*************************************************"
       echo "*****                                       *****"
       echo "*****    Build 32 bit Community Release     *****"
       echo "*****                                       *****"
       echo "*************************************************"
       THEDATE="`date`"
       echo "$THEDATE"
       devenv.com /rebuild release cvsnt.sln
       echo "*************************************************"
       echo "*****                                       *****"
       echo "*****    Build 64 bit Community Release     *****"
       echo "*****          (just libsuid64.dll)         *****"
       echo "*****                                       *****"
       echo "*************************************************"
       THEDATE="`date`"
       echo "$THEDATE"
       devenv.com cvsnt.sln /rebuild "release64|x64" /project setuid
     fi
   else
     echo I need VS 2005 to build EVS on Trunk
     read
     exit 1

   fi
fi

if [ ! -x x64/Release64/setuid64.dll ]; then
  echo SETUID64.DLL has not been successfully built - cannot continue
  read
  exit 1
fi
if [ ! -x ReleaseWin32/simcvs.exe ]; then
  echo SIMCVS.EXE has not been successfully built - cannot continue
  read
  exit 1
fi
if [ ! -x ReleaseWin32/cvsagent.exe ]; then
  echo CVSAGENT.EXE has not been successfully built - cannot continue
  read
  exit 1
fi
if [ ! -x ReleaseWin32/protocols/gserver.dll ]; then
  echo GSERVER.DLL has not been successfully built - cannot continue
  read
  exit 1
fi
cd "$TOP"
rm -f ../*
if [ -f ../cvsapi.dll ]; then
  echo Cannot build due to locked files!
  read
  exit 1
fi
export TARGET=/cygdrive/d/cvsbin
export TARGET_WIN32=d:/cvsbin
export BASE="`pwd`/cvsnt"
export TYPE=ReleaseWin32

echo "*************************************************"
echo "*****                                       *****"
echo "*****  TARGET=${TARGET}"
echo "*****  TARGET_WIN32=${TARGET_WIN32}"
echo "*****  BASE=${BASE}"
echo "*****  TYPE=${TYPE}"
echo "*****                                       *****"
echo "*****  Copy the files from build to cvsbin  *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"


cp x64/Release64/setuid64.dll  /cygdrive/d/cvsbin64
if [ -f /cygdrive/d/cvsbin64/setuid64.dll ]; then
  pushd /cygdrive/d/cvsbin64
  if [ "x$1" != "xCVSNT_BRANCH_2_5_04_3236" -a "x$1" != "xCVSNT_2_0_x" -a "x$1" != "xTrunk" ]; then
    signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll setuid64.dll
  fi
  if [ "x$1" == "xCVSNT_BRANCH_2_5_04_3236" -o "x$1" == "xCVSNT_2_0_x" ]; then
    signtool sign /n "March Hare Software Ltd" /d "CVSNT" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll setuid64.dll
  fi
  if [ "x$1" == "xTrunk" ]; then
    signtool sign /n "March Hare Software Ltd" /d "CM Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll setuid64.dll
  fi
  popd
fi

if [ ! -f cvsnt/tonys\ scripts/copy_common.sh ]; then
  echo "copy_common.sh not found - cannot continue"
  read
  exit 1
fi
if [ ! -f "${BASE}/${TYPE}/setuid.dll" ]; then 
  echo "${TYPE}/setuid.dll not found - copy_common.sh will not work"
  read
fi
if [ ! -f "${BASE}/${TYPE}/cvsapi.lib" ]; then 
  echo "${TYPE}/cvsapi.lib not found - copy_common.sh will not work"
  read
fi
sh cvsnt/tonys\ scripts/copy_common.sh
pushd "$TARGET"
for i in *.exe *.dll *.cpl database/*.dll mdns/*.dll protocols/*.dll triggers/*.dll xdiff/*.dll
do
  echo signtool sign /n "March Hare Software Ltd" /d "CVSNT" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll "$i"
  if [ "x$1" != "xCVSNT_BRANCH_2_5_04_3236" -a "x$1" != "xCVSNT_2_0_x" -a "x$1" != "xTrunk" ]; then
    signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll "$i"
  fi
  if [ "x$1" == "xCVSNT_BRANCH_2_5_04_3236" -o "x$1" == "xCVSNT_2_0_x" ]; then
    signtool sign /n "March Hare Software Ltd" /d "CVSNT" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll "$i"
  fi
  if [ "x$1" == "xTrunk" ]; then
    signtool sign /n "March Hare Software Ltd" /d "CM Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll "$i"
  fi
done
popd

echo "*************************************************"
echo "*****                                       *****"
echo "***** Change to CVSNT source code directory *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"

cd cvsnt
if [ ! -x ${TARGET}/cvsnt.exe ]; then
  if [ ! -x ${TARGET}/cvs.exe ]; then
    echo "There is no CVS.EXE or CVSNT.EXE - cannot continue"
    read
    exit -1
  else
    echo "There is no CVSNT.EXE - copy CVS.EXE to CVSNT.EXE"
    cp ${TARGET}/cvs.exe ${TARGET}/cvsnt.exe
  fi
  if [ ! -x ${TARGET}/cvsnt.dll ]; then
    if [ ! -x ${TARGET}/cvs.dll ]; then
      echo "There is no CVS.DLL or CVSNT.DLL - cannot continue"
      read
      exit -1
    else
      echo "There is no CVSNT.DLL - copy CVS.DLL to CVSNT.DLL."
      cp ${TARGET}/cvs.dll ${TARGET}/cvsnt.dll
    fi
  fi
fi
echo "'${TARGET}/cvsnt.exe' ver -q"
"${TARGET}/cvsnt.exe" ver -q
if [ $? -ne 0 ]; then
  echo "CVSNT VER failed ($?) - perhaps it is not signed, or is otherwise bad?"
  exit -1
else
  echo "CVSNT VER passed ($?)"
fi
BUILD=`"${TARGET}/cvsnt.exe" ver -q`
if [ "x$BUILD" == "x" ]; then
  echo "CVSNT cannot give a version number - it is bad"
  exit -1
fi
BUILDNO=`"${TARGET}/cvsnt.exe" ver -b`
if [ "x$BUILDNO" == "x" ]; then
  echo "CVSNT cannot give a build number - it is bad"
  exit -1
fi
echo Build=$BUILD
echo $BUILD >doc/version.inc
cat doc/version.inc

echo "*************************************************"
echo "*****                                       *****"
echo "*****     Set TAG environment variable      *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"

TAG=CVSNT_`echo $BUILD | sed 's/\./_/g'`
echo Tag=$TAG
if [ "x$3" != "xflightmode" -a "x$3" != "xquik" ]; then
  echo perl.exe /cvsdeps/cvs2cl/cvs2cl.pl -F $BRANCH --window 3600
  perl.exe /cvsdeps/cvs2cl/cvs2cl.pl -F $BRANCH --window 3600
  $TARGET/cvsnt.exe -q commit -m "Build $BUILD"
  if [ "x$USERNAME" == "xJohn" -o "x$USERNAME" == "xabarrett" -o "x$USERNAME" == "xArthur Barrett" -o "x$USERNAME" == "xArthur" ]; then
    echo "sleep to ensure the local repository cache has up to date RCS files"
    sleep 1m
  fi
  $TARGET/cvsnt.exe tag -F $TAG
  if [ "x$USERNAME" == "xJohn" -o "x$USERNAME" == "xabarrett" -o "x$USERNAME" == "xArthur Barrett" -o "x$USERNAME" == "xArthur" ]; then
    echo "sleep to ensure the local repository cache has up to date RCS files"
    sleep 1m
  fi
fi
if [ "x$3" == "xquik" ]; then
  $TARGET/cvsnt.exe tag -F $TAG *.h
  if [ "x$USERNAME" == "xJohn" -o "x$USERNAME" == "xabarrett" -o "x$USERNAME" == "xArthur Barrett" -o "x$USERNAME" == "xArthur" ]; then
    echo "sleep to ensure the local repository cache has up to date RCS files"
    sleep 1m
  fi
fi
cd "$TOP"

if [ "x$3" != "xquik" ]; then
  rm cvsnt/doc/versionsmart.inc
  cd cvsnt/doc
  ls -la ./build.bat ./pdk.cfg
  pwd
  echo "*************************************************"
  echo "*****                                       *****"
  echo "*****        Build CVS Documentation        *****"
  echo "*****                                       *****"
  echo "*************************************************"
  THEDATE="`date`"
  echo "$THEDATE"

  ./build.bat cvs
  ls -la ./*.chm
  ls -la ./build-pdk.bat
  pwd
  echo ./build-pdk.bat $BUILD
  echo "*************************************************"
  echo "*****                                       *****"
  echo "*****      Build CVS-PDK Documentation      *****"
  echo "*****                                       *****"
  echo "*************************************************"
  THEDATE="`date`"
  echo "$THEDATE"

  ./build-pdk.bat $BUILD
  echo list the chm files that resulted from build-pdk
  ls -la ./*.chm
fi
cd "$TOP"

cp cvsnt/doc/*.chm ..
cp cvsnt/ReleaseWin32/*.pdb ../pdb
cp cvsnt/x64/Release64/*.pdb  ../pdb
cp cvsnt/ReleaseWin32/protocols/*.pdb ../pdb/protocols
cp cvsnt/ReleaseWin32/triggers/*.pdb ../pdb/triggers
cp cvsnt/ReleaseWin32/xdiff/*.pdb ../pdb/xdiff

echo "*************************************************"
echo "*****                                       *****"
echo "*****      Build the March Hare Tools       *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"

cd /cygdrive/d/march-hare
if [ "x$3" != "xflightmode" -a "x$3" != "xquik" ]; then
  rm -rf build triggers WorkspaceManager vs.net releasemanager build licenselib unison certs
  cvs -z9 -d ${WMHOSTROOT} co -A -l -r ${WMBRANCH} vs.net
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} vs.net/res3
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} vs.net/scccfg
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} triggers
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} build
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} releasemanager
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} WorkspaceManager
  if [ ! -f WorkspaceManager/res/ico00001.ico ]; then
    echo WorkspaceManager did not checkout OK - cannot continue
    read
    exit 1
  fi
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} unison
  if [ ! -f WorkspaceManager/res/ico00001.ico ]; then
    echo Unison did not checkout OK - cannot continue
    read
    exit 1
  fi
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} licenselib
  if [ ! -f licenselib/MHLicense.h ]; then
    echo licenselib did not checkout OK - cannot continue
    read
    exit 1
  fi
  cvs -z9 -d ${WMHOSTROOT} co -A -r ${WMBRANCH} certs
  if [ ! -f certs/license-full.reg ]; then
    echo certs did not checkout OK - cannot continue
    read
    exit 1
  fi
fi
if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" -a "x$3" != "xquik" ]; then
  rm -rf /cygdrive/d/march-hare/binRelease/*.dll
  rm -rf /cygdrive/d/march-hare/binRelease/*.exe
  rm -rf /cygdrive/d/march-hare/tmpRelease/*.dll
  rm -rf /cygdrive/d/march-hare/tmpRelease/*.exe
  rm -rf /cygdrive/d/march-hare/build/release/*.pdb
  rm -rf /cygdrive/d/march-hare/build/release/*.exe
  rm -rf /cygdrive/d/march-hare/bin/release/*.pdb
  rm -rf /cygdrive/d/march-hare/bin/release/*.exe
  rm -rf /cygdrive/d/march-hare/bin/ReleaseTrial/*.pdb
  rm -rf /cygdrive/d/march-hare/bin/ReleaseTrial/*.exe
fi

echo "*************************************************"
echo "*****                                       *****"
echo "*****  Compile Build the March Hare TCVS    *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"

if [ "x$BRANCH" == "xCVSNT_BRANCH_2_5_04_3236" -o "x$BRANCH" == "xCVSNT_2_0_x" ]; then
  if [ ! -f /cygdrive/d/cvsbin/release\ builder/tcvs/TortoiseCVS/build/runcmake-vc9.bat ]; then
    echo runcmake-vc9.bat does not exist - cannot continue
    read
    exit 1
  fi
  if [ ! -d /cygdrive/d/march-hare/tortoisecvs/ ]; then
    mkdir /cygdrive/d/march-hare/tortoisecvs/
  fi
  if [ ! -d /cygdrive/d/march-hare/tortoisecvs/ ]; then
    echo cannot find /cygdrive/d/march-hare/tortoisecvs/
    read
    exit 1
  fi
# wxUSE_UNICODE and wxUSE_STD_IOSTREAM are importa
# INCLUDE="${INCLUDE};D:\\cvsbin\\release builder\\wxWidgets-2.8.9\\include"
# LIB="${LIB};D:\\cvsbin\\release builder\\wxWidgets-2.8.9\\lib\\vc_lib"
  PATH=/cygdrive/c/cmake-2.6.2-win32-x86/bin:${PATH}
  PATH=/cygdrive/d/gnuwin32:${PATH}
# export INCLUDE
# export LIB
  export PATH
  if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" -a "x$3" != "xquik" ]; then
    if [ "x$3" != "xnowait" -a "x$4" != "xnowait" -a "x$5" != "xnowait" ]; then
      echo "Will now prepare autobuild of TCVS"
#      read
    fi
    cd /cygdrive/d/march-hare/
    /usr/bin/find tortoisecvs -type f -not -path "*/CVS/*" -exec rm {} \; 
    cd /cygdrive/d/cvsbin/release\ builder/tcvs/TortoiseCVS/build
    if [ -d vc9Win32 ]; then
      /usr/bin/find vc9Win32 -type f -not -path "*/CVS/*" -exec rm {} \; 
      rm -rf  vc9Win32 
    fi
    if [ -d vc9x64 ]; then
      /usr/bin/find vc9x64 -type f -not -path "*/CVS/*" -exec rm {} \; 
      rm -rf  vc9x64
    fi
    if [ ! -f runcmake-vc9.bat ]; then
      echo runcmake-vc9.bat does not exist in current directory - cannot continue
      read
      exit 1
    fi
    cmd /c runcmake-vc9.bat
  fi
  cd /cygdrive/d/cvsbin/release\ builder/tcvs/TortoiseCVS
  if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" -a "x$3" != "xquik" ]; then
     ./build/autobuild -vc90 -u -c -noiscc c
  fi
  /usr/bin/find build/vc9Win32 -name "*.exe" -type f -exec cp {} /cygdrive/d/march-hare/tortoisecvs/ \;
  /usr/bin/find build/vc9Win32 -name "*.dll" -type f -exec cp {} /cygdrive/d/march-hare/tortoisecvs/ \;
  cp  build/vc9x64/TortoiseShell/Release/TortoiseShell.dll /cygdrive/d/march-hare/tortoisecvs/TortoiseShell64.dll
  cp  build/vc9x64/TortoiseSetupHelper/Release/TortoiseSetupHelper.exe TortoiseSetupHelper64.exe
  cp  build/vc9x64/RunTimeInstaller/Release/RunTimeInstaller.exe RunTimeInstaller64.exe
  cp build/TortoiseCVS.Filetypes /cygdrive/d/march-hare/tortoisecvs/
  cp build/TortoiseCVSError.wav /cygdrive/d/march-hare/tortoisecvs/
  cp build/TortoiseMenus.config /cygdrive/d/march-hare/tortoisecvs/

  cd src/Icons
  #DanAlpha, TomasLehuta and visli are new for 1.12
  if [ ! -d /cygdrive/d/march-hare/tortoisecvs/icons/DanAlpha ]; then
     mkdir /cygdrive/d/march-hare/tortoisecvs/icons/DanAlpha
  fi
  if [ ! -d /cygdrive/d/march-hare/tortoisecvs/icons/TomasLehuta ]; then
     mkdir /cygdrive/d/march-hare/tortoisecvs/icons/TomasLehuta
  fi
  if [ ! -d /cygdrive/d/march-hare/tortoisecvs/icons/visli ]; then
     mkdir /cygdrive/d/march-hare/tortoisecvs/icons/visli
  fi
  /usr/bin/find . -name "*.ico" -type f -exec cp {} /cygdrive/d/march-hare/tortoisecvs/icons/{} \;
  cd ../..
  cp web/ChangeLog.txt /cygdrive/d/march-hare/tortoisecvs/
  cp web/GPL.html /cygdrive/d/march-hare/tortoisecvs/
  cp web/Help.html /cygdrive/d/march-hare/tortoisecvs/
  cp web/astronlicense.html /cygdrive/d/march-hare/tortoisecvs/
  cp web/charlie.jpeg /cygdrive/d/march-hare/tortoisecvs/
  cp web/faq.html /cygdrive/d/march-hare/tortoisecvs/
  cp web/legal.html /cygdrive/d/march-hare/tortoisecvs/
  cp web/philosophical-gnu-sm.jpg /cygdrive/d/march-hare/tortoisecvs/
  cp web/tcvs.css /cygdrive/d/march-hare/tortoisecvs/
  cd ..
  cp po/TortoiseCVS/en_GB.mo  /cygdrive/d/march-hare/tortoisecvs/locale/en_GB
  cp po/TortoiseCVS/ca_01.mo  /cygdrive/d/march-hare/tortoisecvs/locale/ca
  cp po/TortoiseCVS/cs_CZ.mo  /cygdrive/d/march-hare/tortoisecvs/locale/cs_CZ
  cp po/TortoiseCVS/de_DE.mo  /cygdrive/d/march-hare/tortoisecvs/locale/de_DE
  cp po/TortoiseCVS/es_ES.mo  /cygdrive/d/march-hare/tortoisecvs/locale/es_ES
  cp po/TortoiseCVS/fr_FR.mo  /cygdrive/d/march-hare/tortoisecvs/locale/fr_FR
  cp po/TortoiseCVS/it_IT.mo  /cygdrive/d/march-hare/tortoisecvs/locale/it_IT
  cp po/TortoiseCVS/ja_JP.mo  /cygdrive/d/march-hare/tortoisecvs/locale/ja_JP
  cp po/TortoiseCVS/hu_HU.mo  /cygdrive/d/march-hare/tortoisecvs/locale/hu_HU
  cp po/TortoiseCVS/ka_GE.mo  /cygdrive/d/march-hare/tortoisecvs/locale/ka
  cp po/TortoiseCVS/nb_NO.mo  /cygdrive/d/march-hare/tortoisecvs/locale/nb_NO
  cp po/TortoiseCVS/nl_NL.mo  /cygdrive/d/march-hare/tortoisecvs/locale/nl_NL
  cp po/TortoiseCVS/pt_BR.mo  /cygdrive/d/march-hare/tortoisecvs/locale/pt_BR
  cp po/TortoiseCVS/ro_RO.mo  /cygdrive/d/march-hare/tortoisecvs/locale/ro_RO
  cp po/TortoiseCVS/ru_RU.mo  /cygdrive/d/march-hare/tortoisecvs/locale/ru_RU
  cp po/TortoiseCVS/sl_SI.mo  /cygdrive/d/march-hare/tortoisecvs/locale/sl_SI
  cp po/TortoiseCVS/zh_TW.mo  /cygdrive/d/march-hare/tortoisecvs/locale/zh_TW
  cp po/TortoiseCVS/pl_PL.mo  /cygdrive/d/march-hare/tortoisecvs/locale/pl_PL
#  cp po/TortoiseCVS/tr_TR.mo  /cygdrive/d/march-hare/tortoisecvs/locale/tr_TR
#  cp po/TortoiseCVS/zh_CN.mo  /cygdrive/d/march-hare/tortoisecvs/locale/zh_CN
#  cp po/TortoiseCVS/ko_KR.mo  /cygdrive/d/march-hare/tortoisecvs/locale/ko_KR
#  cp po/TortoiseCVS/da_DK.mo  /cygdrive/d/march-hare/tortoisecvs/locale/da_DK
  cp docs/UserGuide_en.chm  /cygdrive/d/march-hare/tortoisecvs/
  cp docs/UserGuide_fr.chm  /cygdrive/d/march-hare/tortoisecvs/
  cp docs/UserGuide_cn.chm  /cygdrive/d/march-hare/tortoisecvs/
  cp docs/UserGuide_ru.chm  /cygdrive/d/march-hare/tortoisecvs/
  if [ ! -f /cygdrive/d/march-hare/tortoisecvs/TortoiseAct.exe ]; then
    echo TortoiseAct.exe has not been built - cannot continue
    read
    exit 1
  fi
fi

if [ ! -f /cygdrive/d/march-hare/build/build.vc90.sln ]; then
  echo build.vc90.sln  has not been successfully checked out from scotty - cannot continue
  read
  exit 1
fi
echo "*************************************************"
echo "*****                                       *****"
echo "*****  Compile Build the March Hare Tools   *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"
cd /cygdrive/d/march-hare/build
if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" -a "x$3" != "xquik" ]; then
  devenv.com /rebuild release build.vc90.sln
fi
echo "just built march hare suite"
if [ "x$1" == "xCVSNT_BRANCH_2_5_04_3236" -o "x$1" == "xCVSNT_2_0_x" ]; then
  if [ "x$3" != "xnobuild" -a "x$4" != "xnobuild" -a "x$3" != "xquik" ]; then
    devenv.com /rebuild "Trial Release" build.vc90.sln
  fi
  echo "just built march hare suite trial"
  cd /cygdrive/d/march-hare/bin/
  if [ ! -x /cygdrive/d/march-hare/bin/ReleaseTrial/WMFree.exe ]; then
    echo WMFree.exe trial has not been successfully built - cannot continue
    read
    #exit 1
  else
    signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll ReleaseTrial\\WMFree.exe
  fi
fi
cd /cygdrive/d/march-hare/bin/
if [ ! -x /cygdrive/d/march-hare/bin/Release/triggers/bugzilla.dll ]; then
  echo bugzilla.dll  has not been successfully built - cannot continue
  read
  #exit 1
fi
if [ ! -x /cygdrive/d/march-hare/bin/Release/triggers/make.dll ]; then
  echo make.dll  has not been successfully built - cannot continue
  read
  #exit 1
fi
if [ ! -x /cygdrive/d/march-hare/bin/Release/triggers/sync.dll ]; then
  echo sync.dll  has not been successfully built - cannot continue
  read
  #exit 1
fi
if [ ! -x /cygdrive/d/march-hare/bin/Release/cvsscci.dll ]; then
  echo cvsscci.dll has not been successfully built - cannot continue
  read
  #exit 1
fi
if [ ! -x /cygdrive/d/march-hare/bin/Release/ReleaseManager.exe ]; then
  echo ReleaseManager.exe has not been successfully built - cannot continue
  read
  #exit 1
fi
if [ ! -x /cygdrive/d/march-hare/bin/Release/WorkspaceManager.exe ]; then
  echo ReleaseManager.exe has not been successfully built - cannot continue
  read
  #exit 1
fi
if [ ! -x /cygdrive/d/march-hare/bin/Release/scccfg.exe ]; then
  echo scccfg.exe has not been successfully built - cannot continue
  read
  exit 1
fi
cd /cygdrive/d/march-hare
signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll bin\\Release\\WorkspaceManager.exe
signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll bin\\Release\\scccfg.exe
signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll bin\\Release\\cvsscci.dll
signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll bin\\Release\\ReleaseManager.exe
signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll bin\\Release\\triggers\\bugzilla.dll
signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll bin\\Release\\triggers\\make.dll
signtool sign /n "March Hare Software Ltd" /d "CVS Suite" /du http://march-hare.com/cvspro/ /t http://timestamp.verisign.com/scripts/timstamp.dll bin\\Release\\triggers\\sync.dll
cd "$TOP"

cd cvsnt/installer
echo "*************************************************"
echo "*****                                       *****"
echo "*****    Build the March Hare Installer     *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"
cmd.exe /c nmake clean
cmd.exe /c nmake client
mv cvsnt-client-${BUILD}.msi ../../..
mv cvsnt-local-${BUILD}.msi ../../..
mv suite-client-trial-${BUILD}.msi ../../..
cmd.exe /c nmake clean
cmd.exe /c nmake server
mv cvsnt-server-${BUILD}.msi ../../..
#mv suite-server-trial-${BUILD}.msi ../../..
cd ../../..
echo "*************************************************"
echo "*****                                       *****"
echo "*****           Create ZIP files            *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"
wzzip -P cvsnt-${BUILD}-bin.zip *.dll *.exe *.cpl *.ini ca.pem COPYING triggers protocols xdiff
wzzip -pr cvsnt-${BUILD}-pdb.zip pdb
wzzip -Pr cvsnt-${BUILD}-dev.zip cvsnt-pdk.chm inc lib
echo ${BUILD} >release
echo "*************************************************"
echo "*****                                       *****"
echo "*****    Copy/PSCP files to acer64debian    *****"
echo "*****                                       *****"
echo "*************************************************"
THEDATE="`date`"
echo "$THEDATE"
if [ "x$3" != "xflightmodex" ]; then
  if [ "x$USERNAME" == "xJohn" -o "x$USERNAME" == "xabarrett" -o "x$USERNAME" == "xArthur Barrett" -o "x$USERNAME" == "xArthur" ]; then
    pscp -v -4 -P 8227 -i d:/arthur.ppk *.zip *.msi cvs.chm abarrett@intranet.unifacecm.com:/home/abarrett/cvs
#   pscp -v -4 -P 8227 -i d:/arthur.ppk release abarrett@intranet.unifacecm.com:/home/abarrett/cvs
  else
    pscp -i d:/tony.ppk *.zip *.msi cvs.chm release tmh@obrien:/home/tmh/cvs
  fi
fi
