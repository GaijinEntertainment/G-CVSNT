// cvsnt1.h: interface for the CcvsntCPL class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_cvsnt1_H__F52337EE_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
#define AFX_cvsnt1_H__F52337EE_30FF_11D2_8EED_00A0C94457BF__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CcvsntCPL  
{
public:
	CcvsntCPL();
	virtual ~CcvsntCPL();

	// Sent to notify CPlApplet that the user has chosen the icon associated with
	// a given dialog box. CPlApplet should display the corresponding dialog box
	// and carry out any user-specified tasks.  
	BOOL DoubleClick(UINT uAppNum, LONG lData);

	// Sent after the last CPL_STOP message and immediately before the controlling
	// application uses the FreeLibrary function to free the DLL containing the
	// Control Panel application. CPlApplet should free any remaining memory and
	// prepare to close.  
	BOOL Exit();

	// Sent after the CPL_INIT message to prompt CPlApplet to return a number that
	// indicates how many dialog boxes it supports.  
	LONG GetCount();

	// Sent immediately after the DLL containing the Control Panel application is
	// loaded. The message prompts CPlApplet to perform initialization procedures,
	// including memory allocation.  
	BOOL Init();

	// Sent after the CPL_GETCOUNT message to prompt CPlApplet to provide information
	// about a specified dialog box. The lParam2 parameter of CPlApplet points to a
	// CPLINFO structure.  
	BOOL Inquire(UINT uAppNum, LPCPLINFO lpcpli);

	// Sent after the CPL_GETCOUNT message to prompt CPlApplet to provide information
	// about a specified dialog box. The lParam2 parameter of CPlApplet points to a
	// NEWCPLINFO structure.  
	BOOL NewInquire(UINT uAppNum, LPNEWCPLINFO lpcpli);

	// Sent once for each dialog box before the controlling application closes.
	// CPlApplet should free any memory associated with the given dialog box. 
	BOOL Stop(UINT uAppNum, LONG lData);
};

#endif // !defined(AFX_cvsnt1_H__F52337EE_30FF_11D2_8EED_00A0C94457BF__INCLUDED_)
