#ifndef TORTOISE_TRANSLATE_H
#define TORTOISE_TRANSLATE_H

#if defined(NOTRANSLATE)
// Suppress translations
#undef _
#  if wxUSE_UNICODE || defined(UNICODE)
#    define _(msg)  L##msg
#  else
#    define _(msg)  msg
#  endif
#  define UnicodeTranslate(msg)  msg
#  define wxGetTranslation(msg)  msg
#else
#  if wxUSE_UNICODE
#    define UnicodeTranslate(msg)  GetString(wxT(msg))
#  else
#    define UnicodeTranslate(msg)  MultibyteToWide(_(msg)).c_str()
#  endif
#  if defined(_USRDLL)
#     define WXINTL_NO_GETTEXT_MACRO
#     include <wx/string.h>
      extern const wxChar* GetString(const wxChar* str);
#     undef _
#     define _(str) GetString(wxT(str))
#  endif
#  ifdef __cplusplus
#     include <wx/intl.h>
#  endif
#endif

// CVSPARSE represents text output from command line CVS
// which TortoiseCVS scans and responds to somehow.  This
// text needs the identical translation as for the 
// translation of cvs.exe being used.
#define CVSPARSE(X) X

#endif

