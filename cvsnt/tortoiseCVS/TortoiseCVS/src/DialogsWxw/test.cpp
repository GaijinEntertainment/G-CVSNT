#include "ProgressDialog.h"

class MyApp : public wxApp
{
public:
   bool OnInit();
};

IMPLEMENT_APP(MyApp)

bool MyApp::OnInit()
{
	ProgressDialog* dlg = new ProgressDialog;

	for (int i = 0; i < 50; ++i)
	{
		dlg->LookBusy();
		dlg->NewText("Hello\n", 6);
		if (dlg->UserAborted())
			break;
	}
	dlg->WaitForAbort();
	DestroyDialog(dlg);
	return FALSE;	
}


