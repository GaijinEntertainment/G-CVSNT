# TortoiseCVS - a Windows shell extension for easy version control
#
# Copyright (C) 2008 - Torsten Martinsen
# <torsten@bullestock.net> - June 2008
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

PROJECT(TortoisePlink)

INCLUDE_DIRECTORIES(
   ${WX_INCLUDE_PATH}
   
)

ADD_DEFINITIONS(
    -DHAVE_ERRNO_H
    -D_NO_OLDNAMES
    -DNO_MULTIMON
    -DWIN32S_COMPAT
    -DRELEASE="0.56"
)

SET(C_SOURCE
    be_all.c
    cmdline.c
    console.c
    cproxy.c
    ldisc.c
    logging.c
    misc.c
    noise.c
    pageantc.c
    plink.c
    portfwd.c
    pproxy.c
    proxy.c
    raw.c
    rlogin.c
    settings.c
    ssh.c
    sshaes.c
    sshblowf.c
    sshbn.c
    sshcrc.c
    sshcrcda.c
    sshdes.c
    sshdh.c
    sshdss.c
    sshmd5.c
    sshpubk.c
    sshrand.c
    sshrsa.c
    sshsh512.c
    sshsha.c
    sshzlib.c
    telnet.c
    tree234.c
    version.c
    wildcard.c
    windefs.c
    winmisc.c
    winnet.c
    winstore.c
    x11fwd.c
)

SET_SOURCE_FILES_PROPERTIES(
    ${C_SOURCE}
    PROPERTIES COMPILE_FLAGS -TC)

SET(CPP_SOURCE
    LoginDialog.cpp
)

LINK_LIBRARIES(
    ${WX_LIBS}
    ws2_32.lib
)

SET_CFLAGS(TortoisePlink)

ADD_EXECUTABLE(TortoisePlink ${C_SOURCE} ${CPP_SOURCE} PLINK.RC)
