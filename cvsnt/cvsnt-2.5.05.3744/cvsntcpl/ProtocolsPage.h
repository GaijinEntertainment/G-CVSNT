#pragma once

// CProtocolsPage dialog

class CProtocolsPage : public CPropertyPage
{
	DECLARE_DYNAMIC(CProtocolsPage)

public:
	CProtocolsPage();
	virtual ~CProtocolsPage();

// Dialog Data
	enum { IDD = IDD_PAGE7 };

protected:
	void InitSub(HANDLE hFind, LPTSTR buf, LPTSTR bp, WIN32_FIND_DATA& wfd, int& n);

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL OnInitDialog();

	DECLARE_MESSAGE_MAP()
public:
	CListCtrl m_lcProtocols;
	afx_msg void OnNMDblclkList1(NMHDR *pNMHDR, LRESULT *pResult);
	CButton m_btConfigure;
//	afx_msg void OnLvnItemActivateList1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedButton1();
	afx_msg void OnLvnItemchangedList1(NMHDR *pNMHDR, LRESULT *pResult);
};
