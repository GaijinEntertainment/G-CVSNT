#ifndef PASSWORDDIALOG__H
#define PASSWORDDIALOG__H
#include "afxwin.h"

// CPasswordDialog dialog

class CPasswordDialog : public CDialog
{
	DECLARE_DYNAMIC(CPasswordDialog)

public:
	CPasswordDialog(bool bTopmost, CWnd* pParent = NULL);   // standard constructor
	virtual ~CPasswordDialog();

// Dialog Data
	enum { IDD = IDD_GETPASSWORD };

protected:
	bool m_bTopmost;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	std::string m_szCvsRoot;
	std::string m_szPassword;
	virtual BOOL OnInitDialog();
};

#endif
