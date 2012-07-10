# TortoiseCVS - a Windows shell extension for easy version control
#
# MinGW functions for autobuild script
#
# Copyright 2006 Torsten Martinsen
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or (at
# your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# Arguments: <component> <Release/Debug>
function mingw_build()
{
  if [ "$2" == "Debug" ]
  then
    local PARAMS=DEBUG=1
  fi
  if [ "$MAKECLEAN" == "1" ]
  then
    make -C "src" $1_clean $PARAMS > mingw.log 2>&1 
  fi
  if ! make -C "src" $1 $PARAMS >> mingw.log 2>&1 
  then
    cat mingw.log
    die Building $1
  fi
  rm -f mingw.log
}


# Build TortoiseCVS using MingGW
function build_mingw()
{
    if [ "$DEBUG" == "1" ]
    then
        echo "Build debug versions using MinGW/GCC"
        RELEASE_TYPE=Debug
    else
        echo "Build release versions using MinGW/GCC"
        RELEASE_TYPE=Release
    fi

    if [ "$MAKECLEAN" == "1" ]
    then
        echo "Perform a clean build"
    fi
   
    TORTOISEACT_PATH=../src/TortoiseAct/$RELEASE_TYPE/TortoiseAct.exe
    TORTOISESHELL_PATH=../src/TortoiseShell/$RELEASE_TYPE/TortoiseShell.dll
    TORTOISEPLINK_PATH=../src/TortoisePlink/Release/TortoisePlink.exe
    POSTINST_PATH=../src/PostInst/$RELEASE_TYPE/PostInst.exe
    TORTOISESETUPHELPER_PATH=../src/TortoiseSetupHelper/$RELEASE_TYPE_A/TortoiseSetupHelper.exe
    TRANSLATEISS_PATH=../src/TranslateIss/$RELEASE_TYPE/TranslateIss.exe

    cd build
    if [ "$MAKECLEAN" == "1" ]
    then
        rm -f $TORTOISEACT_PATH
        rm -f $TORTOISESHELL_PATH
        rm -f $TORTOISEPLINK_PATH
        rm -f $POSTINST_PATH
        rm -f $TORTOISESETUPHELPER_PATH
        rm -f $TRANSLATEISS_PATH
      fi
    cd ..

    mingw_build TortoiseAct $RELEASE_TYPE
    mingw_build TortoiseShell $RELEASE_TYPE
    mingw_build TortoisePlink Release
    mingw_build PostInst $RELEASE_TYPE
    mingw_build TortoiseSetupHelper $RELEASE_TYPE_A
    mingw_build TranslateIss $RELEASE_TYPE

    echo "..\\src\\SharedDlls\\msvcrt.dll" >> ./build/shared.dlls.tmp
}
