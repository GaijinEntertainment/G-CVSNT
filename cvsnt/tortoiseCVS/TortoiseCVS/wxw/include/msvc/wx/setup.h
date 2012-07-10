/////////////////////////////////////////////////////////////////////////////
// Name:        msvc/wx/msw/setup.h
// Purpose:     wrapper around the real wx/setup.h for Visual C++
// Author:      Vadim Zeitlin
// Modified by:
// Created:     2004-12-12
// RCS-ID:      $Id: setup.h,v 1.1 2012/03/04 01:07:22 aliot Exp $
// Copyright:   (c) 2004 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// VC++ IDE predefines _DEBUG and _UNICODE for the new projects itself, but
// the other symbols (WXUSINGDLL, __WXUNIVERSAL__, ...) should be defined
// explicitly!

#ifdef _MSC_VER
    #ifdef _UNICODE
        #ifdef WXUSINGDLL
            #ifdef _DEBUG
                #include "../../../lib/vc_dll/mswud/wx/setup.h"
            #else
                #include "../../../lib/vc_dll/mswu/wx/setup.h"
            #endif
        #else
            #ifdef _DEBUG
                #include "../../../lib/vc_lib/mswud/wx/setup.h"
            #else
                #include "../../../lib/vc_lib/mswu/wx/setup.h"
            #endif
        #endif

    #else // !_UNICODE
        #ifdef WXUSINGDLL
            #ifdef _DEBUG
                #include "../../../lib/vc_dll/mswd/wx/setup.h"
            #else
                #include "../../../lib/vc_dll/msw/wx/setup.h"
            #endif
        #else // static lib
            #ifdef _DEBUG
                #include "../../../lib/vc_lib/mswd/wx/setup.h"
            #else
                #include "../../../lib/vc_lib/msw/wx/setup.h"
            #endif
        #endif // shared/static

    #endif // _UNICODE/!_UNICODE
#else
    #error "This file should only be included when using Microsoft Visual C++"
#endif

