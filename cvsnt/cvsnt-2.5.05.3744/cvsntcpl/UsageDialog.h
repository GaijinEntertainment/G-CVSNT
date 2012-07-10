#pragma once

#include "resource.h"

// CUsageDialog dialog

class CUsageDialog : public CDialog
{
	DECLARE_DYNAMIC(CUsageDialog)

public:
	CUsageDialog(CWnd* pParent = NULL);   // standard constructor
	virtual ~CUsageDialog();

// Dialog Data
	enum { IDD = IDD_DIALOG1 };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
};
