// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - November 2005

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "StdAfx.h"
// Dialogs
#include "../DialogsWxw/AboutDialog.h"
#include "../DialogsWxw/AddFilesDialog.h"
#include "../DialogsWxw/AnnotateDialog.h"
#include "../DialogsWxw/BranchTagDlg.h"
#include "../DialogsWxw/CheckQueryDlg.h"
#include "../DialogsWxw/CheckoutDialog.h"
#include "../DialogsWxw/ChooseFile.h"
#include "../DialogsWxw/CommandDialog.h"
#include "../DialogsWxw/CommitDialog.h"
#include "../DialogsWxw/ConflictDialog.h"
#include "../DialogsWxw/ConflictListDialog.h"
#include "../DialogsWxw/CreditsDialog.h"
#include "../DialogsWxw/EditDialog.h"
#include "../DialogsWxw/EditorListDialog.h"
#include "../DialogsWxw/GraphDialog.h"
#include "../DialogsWxw/HistoryDialog.h"
#include "../DialogsWxw/LogConfigDialog.h"
#include "../DialogsWxw/MakeModuleDialog.h"
#include "../DialogsWxw/MakePatchDialog.h"
#include "../DialogsWxw/MergeDlg.h"
#include "../DialogsWxw/MessageDialog.h"
#include "../DialogsWxw/PreferencesDialog.h"
#include "../DialogsWxw/ProgressDialog.h"
#include "../DialogsWxw/ReleaseDialog.h"
#include "../DialogsWxw/RenameDialog.h"
#include "../DialogsWxw/UpdateDlg.h"
//#include "../DialogsWxw/WxwHelpers.h"
#include "../DialogsWxw/YesNoDialog.h"

// Basically a dummy App, as we don't really want one
class MyTestApp : public wxApp
{
public:
   bool OnInit();
   int OnRun();
   int OnExit();
#if defined(_DEBUG)
   void OnUnhandledException();
#endif
};

DECLARE_APP(MyTestApp)

IMPLEMENT_APP(MyTestApp)

bool MyTestApp::OnInit()
{
   YesNoAllDialog::YesNoAll res = DoYesNoAllDialog(0,
                                                   "YesNoAllDialog: Buttons should be centered when the text is too long "
                                                   "to fit on a single line without line breaks", true);
   std::ostringstream ss;
   ss << "Result: " << res;
   DoMessageDialog(0, ss.str());
   
   return true;
}



int MyTestApp::OnRun()
{
   return 0;
}



int MyTestApp::OnExit()
{
   return 0;
}


#if defined(_DEBUG)
void MyTestApp::OnUnhandledException()
{
   throw;
}
#endif

HWND GetRemoteHandle()
{
   return 0;
}

