// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - November 2003

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#ifndef FIX_COMPILER_BUGS_H
#define FIX_COMPILER_BUGS_H


#ifdef _MSC_VER

#pragma warning(disable:4786 4503)

#endif /* _MSC_VER */

#endif /* FIX_COMPILER_BUGS_H */


#ifdef __GNUWIN32__

/* Fixes for MinGW's commctrl.h */
#if defined(_COMMCTRL_H) && !defined(_COMMCTRL_H_FIXES)
#define _COMMCTRL_H_FIXES

#define ListView_GetCheckState(w,i) ((((UINT)(SNDMSG((w),LVM_GETITEMSTATE,(WPARAM)(i),LVIS_STATEIMAGEMASK)))>>12)-1)
#define ListView_SetCheckState(w,i,f) ListView_SetItemState(w,i,INDEXTOSTATEIMAGEMASK((f)+1),LVIS_STATEIMAGEMASK)
#define TreeView_SetItemState(w,i,d,m) \
{ \
   TVITEM _tvi;\
   _tvi.mask=TVIF_STATE;\
   _tvi.stateMask=m;\
   _tvi.state=d;\
   SNDMSG((w),TVM_SETITEM,0,(LPARAM)(TVITEM*)&_tvi);\
}
#define TreeView_GetItemState(w,i,m) (UINT)SNDMSG((w),TVM_GETITEMSTATE,(WPARAM)(i),(LPARAM)(m))

#endif /* defined(_COMMCTRL_H) && !defined(_COMMCTRL_H_FIXES) */


/* Fixes for MinGW's wingdi.h */
#if defined(_WINGDI_H) && !defined(_WINGDI_H_FIXES)
#define _WINGDI_H_FIXES

#define DISPLAY_DEVICE_ATTACHED_TO_DESKTOP 0x00000001
#define DISPLAY_DEVICE_MULTI_DRIVER        0x00000002
#define DISPLAY_DEVICE_PRIMARY_DEVICE      0x00000004
#define DISPLAY_DEVICE_MIRRORING_DRIVER    0x00000008
#define DISPLAY_DEVICE_VGA_COMPATIBLE      0x00000010

#endif /* defined(_WINGDI_H) && !defined(_WINGDI_H_FIXES) */


/* Fixes for MinGW's shlobj.h */
#if defined(_SHLOBJ_H) && !defined(_SHLOBJ_H_FIXES)
#define _SHLOBJ_H_FIXES

#if (_WIN32_IE >= 0x0500)

#define MAX_COLUMN_NAME_LEN 80
#define MAX_COLUMN_DESC_LEN 128

#endif /* _WIN32_IE >= 0x0500 */

#define ISIOI_ICONFILE 0x00000001
#define ISIOI_ICONINDEX 0x00000002

#endif /* defined(_SHLOBJ_H) && !defined(_SHLOBJ_H_FIXES) */

#endif /* __GNUWIN32__ */

