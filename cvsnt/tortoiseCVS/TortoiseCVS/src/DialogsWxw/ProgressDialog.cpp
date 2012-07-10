// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

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

#include "ProgressDialog.h"
#include "WxwHelpers.h"
#include "WxwMain.h" // wxGetApp
#include <wx/settings.h>
#include <wx/clipbrd.h>
#include "../Utils/ProcessUtils.h"
#include "../Utils/Translate.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseRegistry.h"
#include <Utils/Preference.h>



#define WRAP_WIDTH 500


enum
{
   PROGRESSDLG_ID_SELECT_ALL = 10001,
   PROGRESSDLG_ID_COPY,
   PROGRESSDLG_ID_LISTCTRL,
   PROGRESSDLG_ID_CLOSE
};

BEGIN_EVENT_TABLE(ProgressDialog, ExtDialog)
   EVT_BUTTON(wxID_CANCEL,                      ProgressDialog::OnAbort)
   EVT_BUTTON(wxID_OK,                          ProgressDialog::OnOk)
   EVT_CHECKBOX(PROGRESSDLG_ID_CLOSE,           ProgressDialog::OnAutoClose)
   EVT_MENU(PROGRESSDLG_ID_COPY,                ProgressDialog::ListCopy)
   EVT_MENU(PROGRESSDLG_ID_SELECT_ALL,          ProgressDialog::ListSelectAll)
END_EVENT_TABLE()


// This is a virtual list control to overcome the performance problems
// inherent in the standard Win32 list control.
class ProgressListCtrl : public ExtListCtrl
{
public:
    // Constructor
    ProgressListCtrl(ProgressDialog* dialog, wxWindow* parent, wxWindowID id);
    // Destructor
    ~ProgressListCtrl();

    void AddLines(const std::vector<std::string*>& lines, const std::vector<wxColour>& colours);
    void AddLines(const std::vector<wxString*>& lines, const std::vector<wxColour>& colours);

protected:
    // Get item text
    wxString OnGetItemText(long item, long column) const;
    // Get item format
    wxListItemAttr *OnGetItemAttr(long item) const;
    // Context menu
    void OnContextMenu(wxContextMenuEvent& event);

private:
    // A line in the list control
    class ListLine
    {
    public:
#if wxUSE_UNICODE
        ListLine(const std::string* str, const wxColour& colour)
            : myLine(new wxString(MultibyteToWide(*str))),
              myColour(colour)
        {
        }

        ListLine(const wxString* str, const wxColour& colour)
            : myLine(str),
              myColour(colour)
        {
        }
#else

       ListLine(const std::string* str, const wxColour& colour)
            : myLine(str),
              myColour(colour)
        {
        }

#endif


        ~ListLine()
        {
            delete myLine;
        }
      
#if wxUSE_UNICODE
        const wxString*         myLine;
#else
        const std::string*      myLine;
#endif
        wxColour                myColour;
   };
   ProgressDialog*              myDialog;
   std::vector<ListLine*>       myLines;

   friend class ProgressDialog;

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ProgressListCtrl, ExtListCtrl)
   EVT_CONTEXT_MENU(ProgressListCtrl::OnContextMenu)
END_EVENT_TABLE()


ProgressDialog* CreateProgressDialog(wxWindow* parent, HANDLE hAbortEvent,
                                     ProgressDialog::AutoCloseSetting autoclose, bool show)
{
   return new ProgressDialog(parent, hAbortEvent, autoclose, show);
}

void DeleteProgressDialog(ProgressDialog * dialog)
{
   TDEBUG_ENTER("DeleteProgressDialog");
   ProgressDialog* progressDialog = static_cast<ProgressDialog*>(dialog);

   // We hide, because otherwise subsequent dialogs cause wxWindows to access violate
   // This happens, for example, with the log config dialog which can appear immediately
   // after the progress dialog.
   progressDialog->Hide(); 

   DestroyDialog(progressDialog);
}

ProgressDialog::ProgressDialog(wxWindow* parent, HANDLE hAbortEvent, AutoCloseSetting autoclose, bool show)
   : ExtDialog(parent, -1, _("TortoiseCVS - Working"),
               wxDefaultPosition, wxDefaultSize,
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxRESIZE_BORDER | 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN),
     myAborted(false),
     myFinished(false),
     myAbortClicked(false),
     myAutoCloseSelection(acDefault),
     mySuppressedTextTypes(TTNone),
     myFinishedCaption(_("TortoiseCVS - Finished")),
     myCheckClose(0),
     myAbortEvent(hAbortEvent)
{
   TDEBUG_ENTER("ProgressDialog");

   // Read colors
   myColorNoise = ColorRefToWxColour(GetIntegerPreference("Colour Noise Text"));
   myColorWarning = ColorRefToWxColour(GetIntegerPreference("Colour Warning Text"));
   myColorUpdated = ColorRefToWxColour(GetIntegerPreference("Colour Updated Files"));
   myColorModified = ColorRefToWxColour(GetIntegerPreference("Colour Modified Files"));
   myColorConflict = ColorRefToWxColour(GetIntegerPreference("Colour Conflict Files"));
   myColorNotInCVS = ColorRefToWxColour(GetIntegerPreference("Colour Not In CVS Files"));
   myColorError = ColorRefToWxColour(GetIntegerPreference("Colour Error Text"));
   myColorTip = ColorRefToWxColour(GetIntegerPreference("Colour Tip Text"));
   
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));
   myCanClose = true;

   // Status bar
   myStatusBar = new wxStatusBar(this, -1);
   myStatusBar->SetStatusText(_("Working..."));

   // Main box with text and button in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);

   myListCtrl = new ProgressListCtrl(this, this, PROGRESSDLG_ID_LISTCTRL);

   myOk = new wxButton(this, wxID_OK, _("OK"));
   myAbort = new wxButton(this, wxID_CANCEL, _("Abort"));
   SetDefaultItem(myOk);

   sizerTop->Add(myListCtrl, 1, wxGROW | wxALL, 5); // 1 takes up any resize

   wxSizer* sizerOuter = new wxBoxSizer(wxHORIZONTAL);
   wxSizer* sizerInner = new wxBoxSizer(wxHORIZONTAL);

   sizerInner->Add(myOk, 0, wxALIGN_CENTER | wxALL, 5);         // 0 takes no share of resize
   sizerInner->Add(myAbort, 0, wxALIGN_CENTER | wxALL, 5);      // 0 takes no share of resize

   // I have ABSOLUTELY NO IDEA why the buttons don't get centered properly.
   // It seems that using sizers is way too advanced for my feeble brain.

   TDEBUG_TRACE("autoclose: " << autoclose);
   if (autoclose != acAutoClose)
   {
      myCheckClose = new wxCheckBox(this, PROGRESSDLG_ID_CLOSE, _("Close on completion"));
      myCheckClose->SetValue(autoclose == acDefaultClose);
      TDEBUG_TRACE("Set CheckClose to " << (autoclose == acDefaultClose));
   }
   if (myCheckClose)
      sizerOuter->Add(myCheckClose, 0, wxALL, 10);
   sizerOuter->Add(10, 0, 1, wxGROW);                           // Spacer
   sizerOuter->Add(sizerInner, 0, wxALIGN_CENTER | wxTOP, 5);   // 0 takes no share of resize
   int offset = 0;
   if (myCheckClose)
   {
      wxSize size = myCheckClose->GetBestSize();
      offset = size.GetWidth();
   }
   TDEBUG_TRACE("Offset " << offset);
   sizerOuter->Add(10 + offset, 0, 1, wxGROW);                  // Spacer for centering
   sizerTop->Add(sizerOuter, 0, wxGROW);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
      
   RestoreTortoiseDialogSize(this, "Progress", wxSize(250, 200));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "Progress");

   Layout();

   Show(show);

   myOk->SetFocus();
   myOk->Enable(false);

   // Process events for initial refresh
   ::wxSafeYield();
}


ProgressDialog::~ProgressDialog()
{
   StoreTortoiseDialogSize(this, "Progress");
}


void ProgressDialog::LookBusy()
{
   // Process events
   // TODO: What is the proper way of doing this?
   // With ThreadSafeProgress we don't need this function any more,
   // instead we want something akin to WaitForAbort?
   ::wxSafeYield();

   // Check to see if aborted and return
   if (myAborted)
      return;
}

#if wxUSE_UNICODE

void ProgressDialog::NewText(const wxString& txt, TextType type)
{
   // Check to see if aborted and return if we have
   if (myAborted)
      return;
   if (type == TTDefault)
   {
      type = GetType(txt);
   }
   if (type & mySuppressedTextTypes)
      return;

   wxColour colour = GetColour(type);
   NewText(txt, &colour);
}

void ProgressDialog::NewText(const std::string& txt, TextType type)
{
    NewText(wxText(txt), type);
}

#else

void ProgressDialog::NewText(const std::string& txt, TextType type)
{
   // Check to see if aborted and return if we have
   if (myAborted)
      return;
   if (type == TTDefault)
   {
      type = GetType(txt);
   }
   if (type & mySuppressedTextTypes)
      return;

   wxColour colour = GetColour(type);
   NewText(txt, &colour);
}

#endif

void ProgressDialog::NewTextLines(const std::vector<std::string*>& lines, TextType type)
{
   // Check to see if aborted and return if we have
   if (myAborted)
      return;
    
   std::vector<wxColour> colours;
   colours.reserve(lines.size());

   if (type == TTDefault)
   {
      std::vector<std::string*>::const_iterator it;
      for (it = lines.begin(); it != lines.end(); it++)
      {
         TextType type = GetType(**it);
         if (type & mySuppressedTextTypes)
            return;
         colours.push_back(GetColour(type));
      }
   }
   else
   {
      if (type & mySuppressedTextTypes)
         return;
      wxColour colour = GetColour(type);
      for (unsigned int i = 0; i < lines.size(); i++)
      {
         colours.push_back(colour);
      }
   }
   NewTextLines(lines, colours);
}



void ProgressDialog::NewText(const std::string& txt, const wxColour* colour)
{
#if wxUSE_UNICODE
    NewText(wxText(txt), colour);
#else
    NewText(wxString(txt), colour);
#endif
}


void ProgressDialog::NewText(const wxString& txt, const wxColour* colour)
{
    // First wrap the text
    wxString wrappedText = WrapText(WRAP_WIDTH, *myListCtrl, txt.c_str());

    // Split text into lines
    const wxChar* p = wrappedText.c_str();
    const wxChar* p0 = p;
    std::vector<wxString*> lines;
    std::vector<wxColour> colours;

    while (*p != wxT('\0'))
    {
        if (*p == wxT('\r'))
        {
            lines.push_back(new wxString(p0, p - p0));
            colours.push_back(*colour);
            p++;
            p0 = p;
            if (*p == wxT('\n'))
            {
                p++;
                p0 = p;
            }
        }
        if (*p == wxT('\n'))
        {
            lines.push_back(new wxString(p0, p - p0));
            colours.push_back(*colour);
            p++;
            p0 = p;
        }
        else
            ++p;
    }
    if (p0 != p)
    {
        lines.push_back(new wxString(p0, p - p0));
        colours.push_back(*colour);
    }
    // Add lines
    myListCtrl->AddLines(lines, colours);
}



void ProgressDialog::NewTextLines(const std::vector<std::string*>& lines, 
                                  const std::vector<wxColour>& colours)
{
   // Add lines
   myListCtrl->AddLines(lines, colours);
}

void ProgressDialog::NewTextLines(const std::vector<wxString*>& lines, 
                                  const std::vector<wxColour>& colours)
{
   // Add lines
   myListCtrl->AddLines(lines, colours);
}


void ProgressDialog::SetCaption(const wxString& caption)
{
   SetTitle(caption.c_str());
}

void ProgressDialog::SetFinishedCaption(const wxString& caption)
{
   myFinishedCaption = caption;
}

const ProgressDialog::TextType ProgressDialog::GetType(const std::string& txt)
{
   TextType type = TTDefault;
   if ((txt.length() >= 2) && (txt[1] == ' '))
   {
      switch(txt[0])
      {
      default:
         break;
      case '?':
      case 'I':
         type = TTNotInCVS;
         break;
      case 'U':
      case 'N':
      case 'P':
         type = TTUpdated;
         break;
      case 'M':
      case 'A':
      case 'R':
         type = TTModified;
         break;
      case 'C':
         type = TTConflict;
         break;
      case 'W': // cvs tag: 'NOT MOVING tag'
         type = TTWarning;
         break;
      }
   }
   return type;
}

const ProgressDialog::TextType ProgressDialog::GetType(const wxString& txt)
{
#if wxUSE_UNICODE
    return GetType(wxAscii(txt.c_str()));
#else
    return GetType(std::string(txt));
#endif
}

const wxColour ProgressDialog::GetColour(TextType type)
{
   wxColour colour;

   switch (type)
   {
   case TTError:
      colour = myColorError;
      break;
   case TTWarning:
      colour = myColorWarning;
      break;
   case TTNoise:
      colour = myColorNoise;
      break;
   case TTUpdated:
      colour = myColorUpdated;
      break;
   case TTConflict:
      colour = myColorConflict;
      break;
   case TTNotInCVS:
      colour = myColorNotInCVS;
      break;
   case TTModified:
      colour = myColorModified;
      break;
   case TTTip:
      colour = myColorTip;
      break;
   default:
      colour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
      break;
   }

   return colour;
}


bool ProgressDialog::Show(bool show)
{
   return wxDialog::Show(show);
}

void ProgressDialog::Lock(bool doLock)
{
   if (doLock)
   {
      SetCursor(*wxHOURGLASS_CURSOR);
      //myAbort->Enable(false);
   }
   else
   {
      SetCursor(wxCURSOR_ARROW);
      //myAbort->Enable(true);
   }
   myCanClose = !doLock;
   //return wxWindow::Enable(!doLock);
}


   
bool ProgressDialog::UserAborted()
{
   return myAbortClicked;
}

void ProgressDialog::OnAbort(wxCommandEvent& event)
{
   if (myCanClose)
   {
      if (myAbort->IsEnabled())
      {
         myAbortClicked = true;
         myAbort->Enable(false);
         // Change "Working" to "Aborted"
         SetTitle(_("TortoiseCVS - Aborting"));
         myStatusBar->SetStatusText(_("Aborting..."));
         if (myAbortEvent != INVALID_HANDLE_VALUE)
         {
            SetEvent(myAbortEvent);
         }
      }
      else if (myOk->IsEnabled())
      {
         OnOk(event);
      }
   }
}

void ProgressDialog::OnOk(wxCommandEvent&)
{
   if (myCanClose)
   {
      myAborted = true;
   }
}


void ProgressDialog::WaitForAbort()
{
   TDEBUG_ENTER("ProgressDialog::WaitForAbort");
   if (myAborted)
   {
      TDEBUG_TRACE("Already aborted");
      return;
   }

   // and "Working" to "Finished"
   if (myAbortClicked)
   {
      TDEBUG_TRACE("Abort clicked");
      SetTitle(_("TortoiseCVS - Aborted"));
      myStatusBar->SetStatusText(_("Aborted"));
   }
   else
   {
      TDEBUG_TRACE("Set caption to finished");
      SetTitle(myFinishedCaption.c_str());
      myStatusBar->SetStatusText(_("Finished"));
   }
   myAbort->Enable(false);
   myOk->Enable(true);
   myFinished = true;
   myOk->SetDefault();
   
   // Return now closes the dialog
   // Wait for it to close
   while (!myAborted)
   {
      WaitMessage();

      MSG msg;
      while (::PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
      {
         ::TranslateMessage(&msg);
         ::DispatchMessage(&msg);
         // ESC or ENTER closes the dialog
         switch (msg.message)
         {
         case WM_SYSKEYUP:
            // Bit 29 of lParam Specifies the context code. The value is 1 if the ALT key
            // is down while the key is released; it is zero if the WM_SYSKEYDOWN message
            // is posted to the active window because no window has the keyboard focus.
            if (msg.lParam & 0x20000000)
               break;
            // fall through
         case WM_CHAR:
            if ((static_cast<TCHAR>(msg.wParam) == 27) || (static_cast<TCHAR>(msg.wParam) == 13))
               myAborted = true;
            break;

         default:
            break;
         }
      }
   }
   myFinished = true;
}

void ProgressDialog::OnURL(wxTextUrlEvent& event)
{
/*   const wxMouseEvent& mouseEvent = event.GetMouseEvent();
   if (mouseEvent.m_leftDown)
   {
      wxString url = myText->GetRange(event.GetURLStart(), event.GetURLEnd());
      LaunchURL(url.c_str());
   }*/
}


WXLRESULT ProgressDialog::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
   switch (message)
   {
   case WM_CLOSE:
      if (myCanClose)
         return ExtDialog::MSWWindowProc(message, wParam, lParam);
      return 1;

   case WM_SYNC:
      Synchronize(wParam, lParam);
      return 1;

   default:
      return ExtDialog::MSWWindowProc(message, wParam, lParam);
   }
}



void ProgressDialog::Synchronize(WPARAM wParam, LPARAM lParam)
{
   AsyncAction aa = (AsyncAction) wParam;
   switch (aa)
   {
   case aaNewText:
      {
         std::pair<const wxString*, ProgressDialog::TextType>* pair = 
            (std::pair<const wxString*, ProgressDialog::TextType> *) lParam;
         NewText(*(pair->first), pair->second);
         break;
      }

   case aaNewTextLines:
      {
         std::pair<const std::vector<std::string*>*, ProgressDialog::TextType> *myPair = 
            (std::pair<const std::vector<std::string*>*, ProgressDialog::TextType> *) lParam;
         NewTextLines(*(myPair->first), myPair->second);
         break;
      }

   case aaUserAborted:
      {
         bool* b = (bool*) lParam;
         *b = UserAborted();
         break;
      }

   case aaWaitForAbort:
      {
         WaitForAbort();
         break;
      }

   case aaShow:
      {
         bool* b = (bool*) lParam;
         *b = Show(*b);
         break;
      }

   case aaLock:
      {
         bool b = lParam ? true : false;
         Lock(b);
         break;
      }
   default:
      break;
   }
}


void ProgressDialog::OnAutoClose(wxCommandEvent&)
{
   if (myAutoCloseSelection == acDefault)
   {
      // First click
      myAutoCloseSelection = myCheckClose->GetValue() ? acUserAutoClose : acUserNoAutoClose;
   }
   else
   {
      // Revert to default
      myAutoCloseSelection = acDefault;
   }
}


// Selected "copy" from context menu
void ProgressDialog::ListCopy(wxCommandEvent&)
{
   // Get number of selected items
   int selCount = myListCtrl->GetSelectedItemCount();
   if (selCount == 0)
      return;

   // Calc string buffer size
   int sel = myListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   int size = 0;
   while (sel != -1)
   {
      size += static_cast<int>(myListCtrl->myLines[sel]->myLine->length()) + 2;
      sel = myListCtrl->GetNextItem(sel, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   }

   wxString str;
   str.reserve(size);

   sel = myListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   while (sel != -1)
   {
      str.append(*myListCtrl->myLines[sel]->myLine);
      str.append(wxT("\r\n"));
      sel = myListCtrl->GetNextItem(sel, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   }

   // Copy to clipboard
   if (wxTheClipboard->Open())
   {
       // These data objects are held by the clipboard, 
       // so do not delete them in the app.
       wxTheClipboard->SetData(new wxTextDataObject(str.c_str()));
       wxTheClipboard->Close();
   }
}


// Selected "select all" from context menu
void ProgressDialog::ListSelectAll(wxCommandEvent&)
{
   int count = myListCtrl->GetItemCount();
   int i;
   for (i = 0; i < count; i++)
   {
      myListCtrl->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
   }
}

ProgressDialog::AutoCloseSelection ProgressDialog::AutoClose() const
{
   return myAutoCloseSelection;
}

void ProgressDialog::SetAutoClose(AutoCloseSetting setting)
{
   switch (setting)
   {
   case acAutoClose:
      myCheckClose->Hide();
      break;
   case acDefaultClose:
      myCheckClose->SetValue(true);
      break;
   case acDefaultNoClose:
      myCheckClose->SetValue(false);
      break;
   }
}

void ProgressDialog::SuppressLines(TextType types)
{
   mySuppressedTextTypes = types;
}


//////////////////////////////////////////////////////////////////////////////
// ProgressListCtrl


// Constructor
ProgressListCtrl::ProgressListCtrl(ProgressDialog* dialog, wxWindow* parent, 
                                   wxWindowID id)
   : ExtListCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxLC_ALIGN_LEFT | wxLC_VIRTUAL),
     myDialog(dialog)
{
   InsertColumn(0, _("Output"), wxLIST_FORMAT_LEFT);
   int colWidth = 300;
   TortoiseRegistry::ReadInteger("Dialogs\\Progress\\Column Width", colWidth);
   SetColumnWidth(0, wxDLG_UNIT_X(this, colWidth));
}


// Destructor
ProgressListCtrl::~ProgressListCtrl()
{
   std::vector<ListLine*>::iterator it = myLines.begin();
   while (it != myLines.end())
   {
      delete *it;
      it++;
   }
   int colWidth = GetColumnWidth(0);
   colWidth = this->ConvertPixelsToDialog(wxSize(colWidth, 0)).GetWidth();
   TortoiseRegistry::WriteInteger("Dialogs\\Progress\\Column Width", colWidth);
}



void ProgressListCtrl::AddLines(const std::vector<std::string*>& lines, const std::vector<wxColour>& colours)
{
   std::vector<std::string*>::const_iterator itLines;
   std::vector<wxColour>::const_iterator itColours;
   myLines.reserve(myLines.size() + lines.size());
   itLines = lines.begin();
   itColours = colours.begin();
   while (itLines != lines.end() && itColours != colours.end())
   {
      myLines.push_back(new ListLine(*itLines, *itColours));
      itLines++;
      itColours++;
   }
   SetItemCount(static_cast<long>(myLines.size()));
   PostMessage(GetHwndOf(this), WM_VSCROLL, SB_BOTTOM, 0);
}

void ProgressListCtrl::AddLines(const std::vector<wxString*>& lines, const std::vector<wxColour>& colours)
{
   std::vector<wxString*>::const_iterator itLines;
   std::vector<wxColour>::const_iterator itColours;
   myLines.reserve(myLines.size() + lines.size());
   itLines = lines.begin();
   itColours = colours.begin();
   while (itLines != lines.end() && itColours != colours.end())
   {
      myLines.push_back(new ListLine(*itLines, *itColours));
      itLines++;
      itColours++;
   }
   SetItemCount(static_cast<long>(myLines.size()));
   PostMessage(GetHwndOf(this), WM_VSCROLL, SB_BOTTOM, 0);
}


// Get item text
wxString ProgressListCtrl::OnGetItemText(long item, long column) const
{
   ASSERT(item < (long) myLines.size());
   return myLines[item]->myLine->c_str();
}


// Get item attributes
wxListItemAttr *ProgressListCtrl::OnGetItemAttr(long item) const
{
   static wxListItemAttr attr;
   ASSERT(item < (long) myLines.size());
   attr.SetTextColour(myLines[item]->myColour);
   return &attr;
}


// Context menu on listview
void ProgressListCtrl::OnContextMenu(wxContextMenuEvent& event)
{
   // Check if an item is selected
   bool bItemsSelected = GetSelectedItemCount() > 0;
   wxMenu menu;

   menu.Append(PROGRESSDLG_ID_COPY, _("Copy"));
   menu.AppendSeparator();

   if (!bItemsSelected)
   {
      menu.Enable(PROGRESSDLG_ID_COPY, false);
   }
   menu.Append(PROGRESSDLG_ID_SELECT_ALL, _("Select All"));
   wxPoint pos = event.GetPosition();
   // Calc selection
   if (pos == wxPoint(-1, -1))
   {
      pos = GetContextMenuPos(GetNextItem(-1, 
                                          wxLIST_NEXT_ALL,
                                          wxLIST_STATE_SELECTED));
      pos = ClientToScreen(pos);
   }
   PopupMenu(&menu, ScreenToClient(pos));
}
