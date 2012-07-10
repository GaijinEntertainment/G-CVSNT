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

#ifndef PROGRESS_DIALOG_H
#define PROGRESS_DIALOG_H

#include <memory>
#include <wx/wx.h>
#include "../CVSGlue/CVSAction.h"
#include "ExtDialog.h"
#include "ExtListCtrl.h"

class ProgressListCtrl;

class ProgressDialog : public ExtDialog
{
public:
   friend class ProgressListCtrl;

   enum TextType
   {
      // No type
      TTNone = 0,
      // Anything not in one of the other categories
      TTDefault = 1,
      // Error messages
      TTError = 2,
      // Warning messages
      TTWarning = 4,
      // "Unimportant" stuff like CVSROOT information
      TTNoise = 8,
      // 'U' and 'N' lines
      TTUpdated = 16,
      // 'C' lines
      TTConflict = 32,
      // '?' lines
      TTNotInCVS = 64,
      // 'M', 'A', and 'R' lines
      TTModified = 128,
      // TortoiseTips
      TTTip = 256
   };

   enum AsyncAction
   {
      aaNewText,
      aaNewTextLines,
      aaUserAborted,
      aaWaitForAbort,
      aaShow,
      aaLock,
      aaFinished
   };

   // User choice
   enum AutoCloseSelection
   {
      // User did not change checkbox setting
      acDefault,
      // User checked checkbox
      acUserAutoClose,
      // User unchecked checkbox
      acUserNoAutoClose
   };

   // Default
   enum AutoCloseSetting
   {
      // Default autoclose, overridable by user
      acDefaultClose,
      // Default no autoclose, overridable by user
      acDefaultNoClose,
      // Autoclose, not overridable by user
      acAutoClose
   };

   static const int WM_SYNC = WM_USER + 1;

   ProgressDialog(wxWindow* parent, HANDLE hAbortEvent, AutoCloseSetting autoclose, bool show = true);
   virtual ~ProgressDialog();

   void LookBusy();
#if wxUSE_UNICODE
   void NewText(const wxString& txt,
                // This must be ONE of the values in TextType (except TTNone), not a combination
                TextType type = TTDefault);
#endif
   void NewText(const std::string& txt,
                TextType type = TTDefault);
   void NewTextLines(const std::vector<std::string*>& lines, TextType type = TTDefault);
   void SetCaption(const wxString& caption);
   void SetFinishedCaption(const wxString& caption);
   bool UserAborted();
   void WaitForAbort();
   bool Show(bool show);
   void Lock(bool doLock = TRUE);
   AutoCloseSelection AutoClose() const;
   void SetAutoClose(AutoCloseSetting);

   // Suppress lines having the specified text type
   void SuppressLines(
      // This can be a combination of the values in TextType
      TextType types);

   // Windows callbacks
   WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam);

private:
   void NewText(const std::string& txt, const wxColour* colour);
   void NewText(const wxString& txt, const wxColour* colour);
   void NewTextLines(const std::vector<std::string*>& lines, const std::vector<wxColour>& colours);
   void NewTextLines(const std::vector<wxString*>& lines, const std::vector<wxColour>& colours);
   void OnAbort(wxCommandEvent& event);
   void OnOk(wxCommandEvent& event);
   void OnURL(wxTextUrlEvent& event);
   const TextType GetType(const std::string& txt);
   const TextType GetType(const wxString& txt);
   const wxColour GetColour(TextType type);
   void Synchronize(WPARAM wParam, LPARAM lParam);
   // Selected "copy" from context menu
   void ListCopy(wxCommandEvent&);
   // Selected "select all" from context menu
   void ListSelectAll(wxCommandEvent&);
   // Toggle "Close automatically"
   void OnAutoClose(wxCommandEvent&);

   ProgressListCtrl*       myListCtrl;
   wxButton*               myAbort;
   wxButton*               myOk;
   wxCheckBox*             myCheckClose;
   AutoCloseSelection      myAutoCloseSelection;

   bool                    myAborted;
   bool                    myFinished;
   bool                    myCanClose;
   bool                    myAbortClicked;
   TextType                mySuppressedTextTypes;
   wxColour                myColorNoise;
   wxColour                myColorWarning;
   wxColour                myColorUpdated;
   wxColour                myColorModified;
   wxColour                myColorConflict;
   wxColour                myColorNotInCVS;
   wxColour                myColorError;
   wxColour                myColorTip;
   wxString                myFinishedCaption;
   HANDLE                  myAbortEvent;
   wxStatusBar*            myStatusBar;

   DECLARE_EVENT_TABLE()
};

ProgressDialog* CreateProgressDialog(wxWindow* parent, HANDLE hAbortEvent,
                                     ProgressDialog::AutoCloseSetting autoclose,
                                     bool show = true);
void DeleteProgressDialog(ProgressDialog*);

#endif

