#pragma once

#include "resource.h"
#include "afxwin.h"
#include <vector>

// CCompatibiltyPage dialog

#include "TooltipPropertyPage.h"
class CCompatibiltyPage : public CTooltipPropertyPage
{
	DECLARE_DYNAMIC(CCompatibiltyPage)

public:
	CCompatibiltyPage();
	virtual ~CCompatibiltyPage();

// Dialog Data
	enum { IDD = IDD_PAGE5 };

protected:

	struct Compatibility_t
	{
		Compatibility_t() { OldVersion=HideStatus=OldCheckout=IgnoreWrappers=0; }
		int OldVersion;
		int HideStatus;
		int OldCheckout;
		int IgnoreWrappers;
	};

	std::vector<Compatibility_t> CompatibilityData;

	DWORD QueryDword(LPCTSTR szKey, DWORD dwDefault);
	void SetDword(LPCTSTR szKey, DWORD dwVal);
	void FillClientType();

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	CButton m_btOldVersionCvsnt;
	CButton m_btOldCheckoutCvsnt;
	CButton m_btHideStatusCvsnt;
	CButton m_btIgnoreWrappersCvsnt;
	CButton m_btOldVersionNonCvsnt;
	CButton m_btOldCheckoutNonCvsnt;
	CButton m_btHideStatusNonCvsnt;
	CButton m_btIgnoreWrappersNonCvsnt;
	afx_msg void OnBnClickedHidestatusCvsnt();
	afx_msg void OnBnClickedIgnorewrappersCvsnt();
	afx_msg void OnBnClickedOldversionCvsnt();
	afx_msg void OnBnClickedOldcheckoutCvsnt();
	afx_msg void OnBnClickedHidestatusNonCvsnt();
	afx_msg void OnBnClickedIgnorewrappersNonCvsnt();
	afx_msg void OnBnClickedOldversionNonCvsnt();
	afx_msg void OnBnClickedOldcheckoutNonCvsnt();
	virtual BOOL OnApply();
	CComboBox m_cbAllowedClients;
	afx_msg void OnCbnSelendokAllowedclients();
};
