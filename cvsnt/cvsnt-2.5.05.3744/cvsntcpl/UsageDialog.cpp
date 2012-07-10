// UsageDialog.cpp : implementation file
//

#include "stdafx.h"
#include "UsageDialog.h"
#include ".\usagedialog.h"


// CUsageDialog dialog

IMPLEMENT_DYNAMIC(CUsageDialog, CDialog)
CUsageDialog::CUsageDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CUsageDialog::IDD, pParent)
{
}

CUsageDialog::~CUsageDialog()
{
}

void CUsageDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CUsageDialog, CDialog)
END_MESSAGE_MAP()


// CUsageDialog message handlers
BOOL CUsageDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	SetDlgItemText(IDC_MESSAGE,_T("So that we can make a better product, March Hare ask that the you allow the server to send us a summary of server activity once a week.  This is collected and used to prioritise future development.\n\nNo information is ever sent that would allow us to link the information to an individual user - we don't even record the IP address.  Simply the statistics that are visible on the first page of the control panel are sent.\n\nIf you would like to help, then just press OK."));
	return TRUE;  

}
