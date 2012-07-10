#pragma once


// CLeftPane view

class CLeftPane : public CTreeView
{
	DECLARE_DYNCREATE(CLeftPane)

protected:
	CLeftPane();           // protected constructor used by dynamic creation
	virtual ~CLeftPane();

public:
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
};


