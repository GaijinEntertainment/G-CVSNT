/////////////////////////////////////////////////////////////////////////////
// Name:        wx/fs_zip.h
// Purpose:     wxZipFSHandler typedef for compatibility
// Author:      Mike Wetherell
// Copyright:   (c) 2006 Mike Wetherell
// CVS-ID:      $Id: fs_zip.h,v 1.1 2012/03/04 01:07:25 aliot Exp $
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#ifndef _WX_FS_ZIP_H_
#define _WX_FS_ZIP_H_

#include "wx/defs.h"

#if wxUSE_FS_ZIP

#include "wx/fs_arc.h"

typedef wxArchiveFSHandler wxZipFSHandler;

#endif // wxUSE_FS_ZIP

#endif // _WX_FS_ZIP_H_
