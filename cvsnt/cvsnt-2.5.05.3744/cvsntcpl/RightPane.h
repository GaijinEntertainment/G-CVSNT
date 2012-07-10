#pragma once


// CRightPane view

class CRightPane : public CView
{
	DECLARE_DYNCREATE(CRightPane)

protected:
	CRightPane();           // protected constructor used by dynamic creation
	virtual ~CRightPane();

public:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	DECLARE_MESSAGE_MAP()
};


