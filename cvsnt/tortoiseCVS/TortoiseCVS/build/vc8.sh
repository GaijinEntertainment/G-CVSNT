# TortoiseCVS - a Windows shell extension for easy version control
#
# VC8 functions for autobuild script
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

# Arguments <component> <configuration> <arch>
# <arch> is Win32 or x64
function vc80_get_targetpath()
{
  local VCPROJ_FILE=`cat build/vc8$3/TortoiseCVS.sln | sed -n -e "s/^Project.*\"$1\"[^\"]*\"\([^\"]*\)\".*$/\1/p"`
  concatpaths "./build/vc8$3" "$VCPROJ_FILE"
  VCPROJ_FILE=$retval
  local BIN_FILE=$(perl - "$1" "$2\\|$3" $VCPROJ_FILE <<"EOP"
    $VCPROJ=`cat $ARGV[2]`;
    $Project = $ARGV[0];
    $Config = $ARGV[1];
    $VCPROJ =~ m/Name="$Config".*?Name="VCLinkerTool".*?OutputFile="(.*?)"/sm;
    print $1;
EOP
  )
  concatpaths "`dirname "$VCPROJ_FILE"`" "$BIN_FILE"
  concatpaths ".." "$retval"
}


function vc80_get_targetpaths()
{
    if [ "$DEBUG" == "1" ]
    then
        echo "Build debug versions using VC 8.0"
        RELEASE_TYPE=RelWithDebInfo
    else
        echo "Build release versions using VC 8.0"
        RELEASE_TYPE=Release
    fi

    vc80_get_targetpath TortoiseAct $RELEASE_TYPE Win32
    TORTOISEACT_PATH=$retval
    vc80_get_targetpath TortoiseShell $RELEASE_TYPE Win32
    TORTOISESHELL_PATH=$retval
    vc80_get_targetpath TortoiseShell $RELEASE_TYPE x64
    TORTOISESHELL64_PATH=$retval
    vc80_get_targetpath TortoisePlink Release Win32
    TORTOISEPLINK_PATH=$retval
    vc80_get_targetpath PostInst $RELEASE_TYPE Win32
    POSTINST_PATH=$retval
    vc80_get_targetpath TortoiseSetupHelper $RELEASE_TYPE Win32
    TORTOISESETUPHELPER_PATH=$retval
    vc80_get_targetpath TortoiseSetupHelper $RELEASE_TYPE x64
    TORTOISESETUPHELPER64_PATH=$retval
    vc80_get_targetpath RunTimeInstaller $RELEASE_TYPE Win32
    RUNTIMEINSTALLER_PATH=$retval
    vc80_get_targetpath RunTimeInstaller $RELEASE_TYPE x64
    RUNTIMEINSTALLER64_PATH=$retval
    vc80_get_targetpath TranslateIss Release Win32
    TRANSLATEISS_PATH=$retval
}


# Arguments: <component> <configuration> <arch>
function vc80_build()
{
  echo -n "Building $1 ($2"
  if [ -n $3 ]; then
    echo -n " $3"
  fi
  echo -n ")..."
  if ! devenv "build/vc8$3/TortoiseCVS.sln" $REBUILD "$2" /project "$1" /OUT msdev.log; then
    echo "failed:"
    cat msdev.log
    die Building $1
  fi
  echo "done"
  rm -f msdev.log
}


# Build TortoiseCVS using VC 8.0
function build_vc80()
{
    local MSVCDIR=$(regtool get '\machine\Software\Microsoft\VisualStudio\8.0\InstallDir' 2> /dev/null) || True
    local MSDevDir=`cygpath "$MSVCDIR"`
    PATH="$MSDevDir":$PATH

    if [ "$MAKECLEAN" == "1" ]
    then
        echo "Perform a clean build"
        REBUILD=/REBUILD
    else
        REBUILD=/build
    fi
   
    cd build
    if [ "$MAKECLEAN" == "1" ]
    then
        rm -f $TORTOISEACT_PATH
        rm -f $TORTOISESHELL_PATH
        rm -f $TORTOISEPLINK_PATH
        rm -f $POSTINST_PATH
        rm -f $TORTOISESETUPHELPER_PATH
        rm -f $RUNTIMEINSTALLER_PATH
        rm -f $TRANSLATEISS_PATH
    fi
    cd ..

    # First update the project files
    vc80_build ZERO_CHECK $RELEASE_TYPE Win32
    vc80_build ZERO_CHECK $RELEASE_TYPE x64

    # Then the actual build
    vc80_build TortoiseAct $RELEASE_TYPE Win32
    vc80_build TortoiseShell $RELEASE_TYPE Win32
    vc80_build TortoiseShell $RELEASE_TYPE x64
    vc80_build TortoisePlink Release Win32
    vc80_build PostInst $RELEASE_TYPE Win32
    vc80_build TortoiseSetupHelper $RELEASE_TYPE Win32
    vc80_build TortoiseSetupHelper $RELEASE_TYPE x64
    vc80_build RunTimeInstaller $RELEASE_TYPE Win32
    vc80_build RunTimeInstaller $RELEASE_TYPE x64
    vc80_build TranslateIss $RELEASE_TYPE Win32

    if [ "$SAVEDBGINFO" == "1" ]
    then
        DBGDIR=`echo "./build/$BUILD.bak" | tr [\\:] [_]`
        mkdir "$DBGDIR"
        cp "./build/vc8/TortoiseAct/$RELEASE_TYPE/TortoiseAct.pdb" "$DBGDIR"
        cp "./build/vc8/TortoiseShell/$RELEASE_TYPE/TortoiseShell.pdb" "$DBGDIR"
        cp "./build/vc8/PostInst/$RELEASE_TYPE/PostInst.pdb" "$DBGDIR"
        cp "./build/vc8/TortoiseSetupHelper/$RELEASE_TYPE/TortoiseSetupHelper.pdb" "$DBGDIR"
    fi
}

# Arguments <component> <configuration> <arch>
# <arch> is Win32 or x64
function vc90_get_targetpath()
{
  local VCPROJ_FILE=`cat build/vc9$3/TortoiseCVS.sln | sed -n -e "s/^Project.*\"$1\"[^\"]*\"\([^\"]*\)\".*$/\1/p"`
  concatpaths "./build/vc9$3" "$VCPROJ_FILE"
  VCPROJ_FILE=$retval
  local BIN_FILE=$(perl - "$1" "$2\\|$3" $VCPROJ_FILE <<"EOP"
    $VCPROJ=`cat $ARGV[2]`;
    $Project = $ARGV[0];
    $Config = $ARGV[1];
    $VCPROJ =~ m/Name="$Config".*?Name="VCLinkerTool".*?OutputFile="(.*?)"/sm;
    print $1;
EOP
  )
  concatpaths "`dirname "$VCPROJ_FILE"`" "$BIN_FILE"
  concatpaths ".." "$retval"
}


function vc90_get_targetpaths()
{
    if [ "$DEBUG" == "1" ]
    then
        echo "Build debug versions using VC 9.0"
        RELEASE_TYPE=RelWithDebInfo
    else
        echo "Build release versions using VC 9.0"
        RELEASE_TYPE=Release
    fi

    vc90_get_targetpath TortoiseAct $RELEASE_TYPE Win32
    TORTOISEACT_PATH=$retval
    vc90_get_targetpath TortoiseShell $RELEASE_TYPE Win32
    TORTOISESHELL_PATH=$retval
    vc90_get_targetpath TortoiseShell $RELEASE_TYPE x64
    TORTOISESHELL64_PATH=$retval
    vc90_get_targetpath TortoisePlink Release Win32
    TORTOISEPLINK_PATH=$retval
    vc90_get_targetpath PostInst $RELEASE_TYPE Win32
    POSTINST_PATH=$retval
    vc90_get_targetpath TortoiseSetupHelper $RELEASE_TYPE Win32
    TORTOISESETUPHELPER_PATH=$retval
    vc90_get_targetpath TortoiseSetupHelper $RELEASE_TYPE x64
    TORTOISESETUPHELPER64_PATH=$retval
    vc90_get_targetpath RunTimeInstaller $RELEASE_TYPE Win32
    RUNTIMEINSTALLER_PATH=$retval
    vc90_get_targetpath RunTimeInstaller $RELEASE_TYPE x64
    RUNTIMEINSTALLER64_PATH=$retval
    vc90_get_targetpath TranslateIss Release Win32
    TRANSLATEISS_PATH=$retval
}


# Arguments: <component> <configuration> <arch>
function vc90_build()
{
  echo -n "Building $1 ($2"
  if [ -n $3 ]; then
    echo -n " $3"
  fi
  echo -n ")..."
  if ! devenv "build/vc9$3/TortoiseCVS.sln" $REBUILD "$2" /project "$1" /OUT msdev.log; then
    echo "failed:"
    cat msdev.log
    die Building $1
  fi
  echo "done"
  rm -f msdev.log
}


# Build TortoiseCVS using VC 9.0
function build_vc90()
{
    local MSVCDIR=$(regtool get '\machine\Software\Microsoft\VisualStudio\9.0\InstallDir' 2> /dev/null) || True
    local MSDevDir=`cygpath "$MSVCDIR"`
    PATH="$MSDevDir":$PATH

    if [ "$MAKECLEAN" == "1" ]
    then
        echo "Perform a clean build"
        REBUILD=/REBUILD
    else
        REBUILD=/build
    fi
   
    cd build
    if [ "$MAKECLEAN" == "1" ]
    then
        rm -f $TORTOISEACT_PATH
        rm -f $TORTOISESHELL_PATH
        rm -f $TORTOISEPLINK_PATH
        rm -f $POSTINST_PATH
        rm -f $TORTOISESETUPHELPER_PATH
        rm -f $RUNTIMEINSTALLER_PATH
        rm -f $TRANSLATEISS_PATH
    fi
    cd ..

    # First update the project files
    vc90_build ZERO_CHECK $RELEASE_TYPE Win32
    vc90_build ZERO_CHECK $RELEASE_TYPE x64

    # Then the actual build
    vc90_build TortoiseAct $RELEASE_TYPE Win32
    vc90_build TortoiseShell $RELEASE_TYPE Win32
    vc90_build TortoiseShell $RELEASE_TYPE x64
    vc90_build TortoisePlink Release Win32
    vc90_build PostInst $RELEASE_TYPE Win32
    vc90_build TortoiseSetupHelper $RELEASE_TYPE Win32
    vc90_build TortoiseSetupHelper $RELEASE_TYPE x64
    vc90_build RunTimeInstaller $RELEASE_TYPE Win32
    vc90_build RunTimeInstaller $RELEASE_TYPE x64
    vc90_build TranslateIss $RELEASE_TYPE Win32

    if [ "$SAVEDBGINFO" == "1" ]
    then
        DBGDIR=`echo "./build/$BUILD.bak" | tr [\\:] [_]`
        mkdir "$DBGDIR"
        cp "./build/vc9/TortoiseAct/$RELEASE_TYPE/TortoiseAct.pdb" "$DBGDIR"
        cp "./build/vc9/TortoiseShell/$RELEASE_TYPE/TortoiseShell.pdb" "$DBGDIR"
        cp "./build/vc9/PostInst/$RELEASE_TYPE/PostInst.pdb" "$DBGDIR"
        cp "./build/vc9/TortoiseSetupHelper/$RELEASE_TYPE/TortoiseSetupHelper.pdb" "$DBGDIR"
    fi
}
