// cvsnt.h : main header file for the cvsntCPL DLL
//

#if !defined(AFX_cvsntCPL_H__F52337E7_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
#define AFX_cvsntCPL_H__F52337E7_30FF_11D2_8EED_00A0C94457BF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CcvsntApp
// See cvsntCPL.cpp for the implementation of this class
//

class CcvsntApp : public CWinApp
{
public:
	CcvsntApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CcvsntApp)
	//}}AFX_VIRTUAL

	//{{AFX_MSG(CcvsntApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
/* Compatibility with old headers */

#endif // !defined(AFX_cvsntCPL_H__F52337E7_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
