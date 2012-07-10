# TortoiseCVS - a Windows shell extension for easy version control
#
# buildinstaller function for autobuild script
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

function buildinstaller
{
    rm -f setup.preproc

    echo "Preprocessing setup script"

    TORTOISEACT_PATH=$(cygpath -w "$TORTOISEACT_PATH")
    TORTOISESHELL_PATH=$(cygpath -w "$TORTOISESHELL_PATH")
    TORTOISESHELL64_PATH=$(cygpath -w "$TORTOISESHELL64_PATH")
    TORTOISEPLINK_PATH=$(cygpath -w "$TORTOISEPLINK_PATH")
    POSTINST_PATH=$(cygpath -w "$POSTINST_PATH")
    TORTOISESETUPHELPER_PATH=$(cygpath -w "$TORTOISESETUPHELPER_PATH")
    TORTOISESETUPHELPER64_PATH=$(cygpath -w "$TORTOISESETUPHELPER64_PATH")
    RUNTIMEINSTALLER_PATH=$(cygpath -w "$RUNTIMEINSTALLER_PATH")
    RUNTIMEINSTALLER64_PATH=$(cygpath -w "$RUNTIMEINSTALLER64_PATH")

    echo "#define TORTOISEVERSION \"$BUILD\"" >> setup.preproc
    echo "#define TORTOISEACT_PATH \"$TORTOISEACT_PATH\"" >> setup.preproc
    echo "#define TORTOISESHELL_PATH \"$TORTOISESHELL_PATH\"" >> setup.preproc
    echo "#define TORTOISEPLINK_PATH \"$TORTOISEPLINK_PATH\"" >> setup.preproc
    echo "#define POSTINST_PATH \"$POSTINST_PATH\"" >> setup.preproc
    echo "#define TORTOISEVERSIONNUMERIC \"$MAJOR_VER.$MINOR_VER.$RELEASE_VER.$BUILD_VER\"" >> setup.preproc
    echo "#define TORTOISESHELL64_PATH \"$TORTOISESHELL64_PATH\"" >> setup.preproc
    echo "#define TORTOISESETUPHELPER_PATH \"$TORTOISESETUPHELPER_PATH\"" >> setup.preproc
    echo "#define TORTOISESETUPHELPER64_PATH \"$TORTOISESETUPHELPER64_PATH\"" >> setup.preproc
    echo "#define RUNTIMEINSTALLER_PATH \"$RUNTIMEINSTALLER_PATH\"" >> setup.preproc
    echo "#define RUNTIMEINSTALLER64_PATH \"$RUNTIMEINSTALLER64_PATH\"" >> setup.preproc
    echo "#define CVSNTINSTALLER $CVSNTINSTALLER" >> setup.preproc
    echo "#define OVERLAYS32INSTALLER \"$OVERLAYS32INSTALLER\"" >> setup.preproc
    echo "#define OVERLAYS64INSTALLER \"$OVERLAYS64INSTALLER\"" >> setup.preproc
    if [ "$DEBUG" == "1" ]; then
	echo "#define DEBUG_BUILD" >> setup.preproc
    fi

    cat TortoiseCVS.iss >> setup.preproc

    echo "Translating setup script"
    rm -f setup.iss
    cat setup.preproc | "$TRANSLATEISS_PATH" "`cygpath -wa ../tools/innosetup`" "`cygpath -wa ../../po/Setup`" > setup.iss
    rm -rf ../../po/Setup/Locale

    echo "Running ISX to generate installation"
    if ! ../tools/innosetup/iscc.exe setup.iss > /dev/null; then
	../tools/innosetup/iscc.exe setup.iss
	die Building installer
    fi
    rm -f isx.log setup.preproc registry.inc ../po/*.mo

    renameexe
}
