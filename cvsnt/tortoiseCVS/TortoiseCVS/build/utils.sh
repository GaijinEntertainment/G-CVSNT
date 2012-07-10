# TortoiseCVS - a Windows shell extension for easy version control
#
# Utility functions for autobuild script
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

# Arguments <filename> <search string> <replace string>
function replaceinfile()
{
    REPLACE=`echo $3 | sed -e "s/\\\\\\\\/\\\\\\\\\\\\\\\\/g"`
    cat "$1" | sed "s/$2/$REPLACE/g" > "$1.tmp"
    cp "$1.tmp" "$1"
    rm -f "$1.tmp"
}


function die()
{
    echo Error: $@
    exit 2
}


# Arguments <path1> <path2>
function concatpaths()
{
    local s2=`cygpath "$2"`
    if [ `expr substr "$s2" 1 1` == "/" ]; then
	retval=$s2
    else
	retval=`cygpath "$1"`
       retval="$retval/$s2"
    fi
}


function buildnumparse()
{
    MAJOR_VER=$1
    MINOR_VER=$2
    RELEASE_VER=$3
    BUILD_VER=$4
}

# Argument: <Folder with PO files> <Output file>
function getlangstatus()
{
    OUTFILE=$2
    # Change to po folder
    pushd $1 > /dev/null
    # Specify release
    echo "<!--#set var=\"u_release\" value=\"$BUILD\" -->" > $OUTFILE
    # Get total number of strings
    total=`grep -c "^msgid" en_GB.po`
    # First "msgid" should be discounted
    total=$(($total-1))
    # Process each .po file
    for lang in `ls *.po | grep -v en_GB | sed -e s/\.po//g`; do
	info=`msgfmt --statistics $lang.po 2>&1 > /dev/null`
	translated=`echo $info | cut -d ' ' -f 1`
	status=$((100*$translated/$total))
        # Take rounding errors into account
	if [ "$translated" != "$total" ] && [ "$status" == "100" ]; then
	    status=99
	fi
	echo "$lang: $status % complete"
	echo "<!--#set var=\"u_${lang}_status\" value=\"$status&nbsp;%\" -->" >> $OUTFILE
    done
    set -e
    # Return to starting point
    popd > /dev/null
}

function updatelangstatus()
{
    echo Application:
    OUTFILE=../../TortoiseCVS/web/lang_u_status.shtml
    PODIR=../po/TortoiseCVS
    getlangstatus $PODIR $OUTFILE
    echo Installer:
    getlangstatus ../po/Setup /dev/null
}

function build()
{
    # Start building
    if [ "$VC80" == "1" ]; then
	build_vc80
    elif [ "$VC90" == "1" ]; then
	build_vc90
    elif [ "$MINGW" == "1" ]; then
	build_mingw
	ls
    else 
	build_vc80
    fi
}

function get_targetpaths()
{
    if [ "$VC80" == "1" ]; then
	vc80_get_targetpaths
    elif [ "$VC90" == "1" ]; then
	vc90_get_targetpaths
    else 
	vc80_get_targetpaths
    fi
    CVSNTINSTALLER=`grep CVSNTINSTALLER src/CvsNt/cvsnt-version.h | cut -d ' ' -f 3`
    OVERLAYSVERSION=`cat src/SharedDlls/TortoiseOverlaysVersion | tr -d "\r"`
    OVERLAYS32INSTALLER=TortoiseOverlays-$OVERLAYSVERSION-win32.msi
    OVERLAYS64INSTALLER=TortoiseOverlays-$OVERLAYSVERSION-x64.msi
    echo "#define OVERLAYS32INSTALLER \"$OVERLAYS32INSTALLER\"" > src/RunTimeInstaller/overlays-version.h
    echo "#define OVERLAYS64INSTALLER \"$OVERLAYS64INSTALLER\"" >> src/RunTimeInstaller/overlays-version.h
}


function buildpofiles
{
    echo -n "Building application .mo files: "
    pushd ../po/Tortoisecvs
    rm -f tmp.po
    # Remove PO header
    tail -n +12 ../Common/NativeLanguageNames.po > ../Common/nlm.po
    for lang in `ls *.po | sed -e s/\.po//g`; do
	echo -n $lang " "
	cat $lang.po ../Common/nlm.po > tmp.po
	msgfmt -o $lang.mo tmp.po
	rm tmp.po
    done
    echo
    rm ../Common/nlm.po
    popd
  
    echo -n "Building setup .mo files: "
    pushd ../po/Setup
    rm -f *.mo tmp.po
    for lang in `ls *.po | sed -e s/\.po//g`; do
	COMMONPOFILE=
	if [ -f ../Common/$lang.po ]; then
	    COMMONPOFILE=../Common/$lang.po
	fi
	echo -n $lang " "
	cat $lang.po $COMMONPOFILE > tmp.po
        msgfmt -o $lang.mo tmp.po
	rm tmp.po
    done
    echo
    popd
}

# Handle options and commands. Simple commands get handled entirely by this function.
function parseoptions
{
    while [ "$1" != "" ]
      do
      case $1 in
	  -i)
	      INCREL=0
	      ;;
	  -c)
	      COMMIT=0
	      ;;
	  -u)
	      UPDATE=0
	      ;;
	  -n)
	      NOCLEAN=1
	      ;;
	  -v)
	      VERBOSE=1
	      ;;
	  -r)
	      CLEAN=1
	      ;;
	  -b)
	      UPDATE=0
	      COMMIT=0
	      INCREL=0
	      INCBUILD=0
	      NOCOMPILE=1
	      ;;
	  -mingw)
	      MINGW=1
	      ;;
	  -vc80)
	      VC80=1
	      VC90=0
	      ;;
	  -vc90)
	      VC90=1
	      VC80=0
	      ;;
          -iscc)
              DOISCC=1
	      ;;
          -noiscc)
              DOISCC=0
	      ;;
	  -w)
	      UNICODE=1
	      ;;
	  -h)
	      cat<<EOF
Usage:
        autobuild [options] [release_type]

    where 'release_type' is one of

        scratch
        rc
        release
        debug
        custom
        src

    The release type can also be omitted, in which case the installer is simply run.

Options:
        -i      Do not increment release number when building release version
        -c      Do not run "cvs commit"
        -u      Do not run "cvs update"
        -r      Perform a clean build
        -v      Verbose (currently no effect)
        -n      Do not perform a clean build when building release version
        -b	Do not build executables, only build installer
        -mingw  Use GCC on MinGW for building
        -vc80    Use MS Visual C++ 8.0 for building (default)
        -vc90    Use MS Visual C++ 9.0 for building
        -iscc    Use InnoSetup (default)
        -noiscc  Do not use InnoSetup
EOF
	      exit 0
	      ;;
	  re*)
	      echo "Release build"
	      RELEASE=1
	      MAKECLEAN=1
	      ;;
	  rc)
	      if [ "$2" == "" ]
		  then
		  echo "You must specify number of RC build"
		  exit 1
	      fi
	      RCNUM=$2
	      echo "Building Release Candidate $RCNUM"
	      STABLE=1
	      shift
	      ;;
	  sc*)
	      echo "Building test release"
	      ;;
	  d*)
	      echo "Building debug release"
	      DEBUG=1
	      if [ "$VC80" == "1" ]; then
		  SAVEDBGINFO=1
	      fi
	      ;;
	  c*)
	      echo "Building customized release"
	      CUSTOM=1
	      [ -d build/custom ] || die "A customized release requires customizations in \"build/custom\"."
	      [ -e build/custom/buildinfo.txt ] || die "A customized release must contain build information in \"build/custom/buildinfo.txt\nso that it will be properly identified in the about box."
	      ;;
	  u*)
	      echo "Updating translation status"
	      BUILD=`cat src/BuildInfo | sed -n "1p" | sed "s/\./ /g"`
	      buildnumparse $BUILD
	      BUILD=$MAJOR_VER.$MINOR_VER.$RELEASE_VER
	      updatelangstatus
	      exit 0
	      ;;
	  s*)
	      echo "Creating source zip"
	      BUILD=`cat src/BuildInfo | sed -n "1p" | sed "s/\./ /g"`
	      buildnumparse $BUILD
	      BUILD=$MAJOR_VER.$MINOR_VER.$RELEASE_VER
	      exit 0
	      ;;
	  w*)
	      echo "Creating wxWidgets zip"
	      BUILD=`cat src/BuildInfo | sed -n "1p" | sed "s/\./ /g"`
	      buildnumparse $BUILD
	      BUILD=$MAJOR_VER.$MINOR_VER.$RELEASE_VER
	      exit 0
	      ;;

	  "")
            # Scratch build
	      ;;
	  *)
	      echo "Error: Unrecognized build type '$1'"
	      exit 1
	      ;;
      esac
      shift
    done
}

# Arguments <iswin64>
function renameexe
{
    if [ "$RELEASE" == "0" ]; then
	if [ "$STABLE" == "0" ]; then
	    if [ "$DEBUG" == "1" ]; then
		EXEFILE=TortoiseCVS-debug.exe
	    else
		EXEFILE=TortoiseCVS-test.exe
	    fi
	else
	    EXEFILE=TortoiseCVS-`echo $BUILD | sed -e 's/ /-/' |sed -e 's/ //'`.exe
	fi
    else
	EXEFILE=TortoiseCVS-$BUILD.exe
    fi
    mv TortoiseCVS.exe $EXEFILE
    retval=$EXEFILE
}
