#ifndef TOOLTIPPROPERTYPAGE__H
#define TOOLTIPPROPERTYPAGE__H

class CTooltipPropertyPage : public CPropertyPage
{
public:
	CTooltipPropertyPage();
	CTooltipPropertyPage(int nID);
	virtual ~CTooltipPropertyPage();

protected:
	CToolTipCtrl m_wndToolTip;

	DECLARE_DYNAMIC(CTooltipPropertyPage)
	DECLARE_MESSAGE_MAP()

	afx_msg void OnDestroy();

	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG *pMsg);
};

#endif
