// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - December 2003

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
#include "ConflictDialog.h"
#include <wx/statline.h>
#include "WxwHelpers.h"
#include "WxwMain.h"
#include "ExtStaticText.h"
#include <Utils/Preference.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/StringUtils.h>
#include <Utils/Translate.h>


BEGIN_EVENT_TABLE(ConflictDialog, wxWizard)
   EVT_WIZARD_PAGE_CHANGED(-1, ConflictDialog::OnPageChanged)
   EVT_WIZARD_PAGE_CHANGING(-1, ConflictDialog::OnPageChanging)
END_EVENT_TABLE()


//static
bool DoConflictDialog(wxWindow* parent, 
                      const std::string& file)
{
   ConflictDialog* dlg = new ConflictDialog(parent, file);
   bool ret = dlg->RunWizard();
   
   DestroyDialog(dlg);
   return ret;
}


/////////////////////////////////////////////////////////////////////
// ConflictDialog

ConflictDialog::ConflictDialog(wxWindow* parent, 
                               const std::string& file)
   : wxWizard(parent, -1, _("TortoiseCVS - Merge Conflicts")),
     myMergeMode(MERGE_UNDEF),
     myFile(file)
    

{
   TDEBUG_ENTER("ConflictDialog::ConflictDialog");
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));


/*   TortoiseRegistry::ReadInteger("Merge Mode", myMergeMode);

   if (myMergeMode == 1)
   {
      SetTitle(_("TortoiseCVS - Two-way Merge"));
   }
   else
   {
      SetTitle(_("TortoiseCVS - Three-way Merge"));
   }

   wxStaticText *label1 = new wxStaticText(this, -1, 
                                           _("TortoiseCVS has created the following temporary "
                                             "files for the merge operation."));

   // Grid with filenames
   wxFlexGridSizer *flex = new wxFlexGridSizer(2, 5, 5);
   flex->AddGrowableCol(1);

   wxStaticText *labelLocal = new wxStaticText(this, -1, _("Your local changes are in:"));
   flex->Add(labelLocal, 0, wxGROW | wxTOP | wxBOTTOM, 0);
   wxStaticText *fileLocal = new wxStaticText(this, -1, _("local file.txt"));
   flex->Add(fileLocal, 1, wxGROW | wxTOP | wxBOTTOM, 0);
   wxStaticText *labelRepository = new wxStaticText(this, -1, _("The repository changes are in:"));
   flex->Add(labelRepository, 0, wxGROW | wxTOP | wxBOTTOM, 0);
   wxStaticText *fileRepository = new wxStaticText(this, -1, _("remote file.txt"));
   flex->Add(fileRepository, 1, wxGROW | wxTOP | wxBOTTOM, 0);

   wxStaticText *labelTarget = new wxStaticText(this, -1, _("The merge target is:"));
   flex->Add(labelTarget, 0, wxGROW | wxTOP | wxBOTTOM, 0);
   if (myMergeMode != 1)
   {
      wxStaticText *fileTarget = new wxStaticText(this, -1, _("merge target.txt"));
      flex->Add(fileTarget, 1, wxGROW | wxTOP | wxBOTTOM, 0);
   }
   else
   {
      wxStaticText *fileTarget = new wxStaticText(this, -1, _("remote file.txt"));
      flex->Add(fileTarget, 1, wxGROW | wxTOP | wxBOTTOM, 0);
   }


   // Instructions
   wxStaticText *label2 = new wxStaticText(this, -1, 
                                           _("Please proceed as follows:"));

   wxGridSizer *grid = new wxGridSizer(1, 5);
   wxStaticText *label3 = new wxStaticText(this, -1, 
                                           _("1. "));
   wxButton *buttonLaunch = new wxButton(this, -1, _("Launch merge application"));

   wxBoxSizer *sizerLaunch = new wxBoxSizer(wxHORIZONTAL);
   sizerLaunch->Add(label3, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
   sizerLaunch->Add(buttonLaunch, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 3);

   grid->Add(sizerLaunch, 0, wxALIGN_CENTER_VERTICAL, 0);

   wxString target;
   if (myMergeMode == 1)
   {
      target = "remote file";
   }
   else
   {
      target = "merge target file";
   }

   wxStaticText *label4 = new wxStaticText(this, -1, 
                                           Printf(_("2. Merge the changes into the target file \"%s\"."),
                                           target.c_str()).c_str());
   grid->Add(label4, 0, wxALIGN_CENTER_VERTICAL, 0);

   wxStaticText *label5 = new wxStaticText(this, -1, 
                                           Printf(_("3. Save the target file \"%s\" and close the merge application."),
                                           target.c_str()).c_str());
   grid->Add(label5, 0, wxALIGN_CENTER_VERTICAL, 0);

   // buttons
   wxBoxSizer* sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   myOK = new wxButton(this, wxID_OK, _("OK"));
   myOK->SetDefault();
   myOK->Enable(false);

   myCancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
   sizerConfirm->Add(myOK, 0, wxGROW | wxALL, 5);
   sizerConfirm->Add(myCancel, 0, wxGROW | wxALL, 5);

   // Main box with everything in it
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(label1, 0, wxGROW | wxALL, 5);
   sizerTop->Add(5, 5);
   sizerTop->Add(flex, 0, wxGROW | wxALL, 5);
   sizerTop->Add(new wxStaticLine(this, -1), 0, wxGROW | wxALL, 5);
   sizerTop->Add(label2, 0, wxGROW | wxALL, 5);
   sizerTop->Add(grid, 0, wxGROW | wxALL, 5);
   sizerTop->Add(5, 5);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);*/

   SetTortoiseDialogPos(this, GetRemoteHandle());
}


ConflictDialog::~ConflictDialog()
{
   CleanupMergeFiles();
}


bool ConflictDialog::RunWizard()
{
   // Create pages
   myPageMergeMode = new PageMergeMode(this);
   myPageDirectMerge = new PageDirectMerge(this);
   myPageTwoWayMerge = new PageTwoWayMerge(this);
   myPageThreeWayMerge = new PageThreeWayMerge(this);

   // Chain pages
   myPageDirectMerge->SetPrev(myPageMergeMode);
   myPageTwoWayMerge->SetPrev(myPageMergeMode);
   myPageThreeWayMerge->SetPrev(myPageMergeMode);

   // Calc size
   wxSize minSize, tmpSize;
   wxWindow* pages[] = 
   { 
      myPageMergeMode, 
      myPageDirectMerge, 
      myPageTwoWayMerge,
      myPageThreeWayMerge
   };

   unsigned int i;
   for (i = 0; i < sizeof(pages) / sizeof(*pages); i++)
   {
      tmpSize = pages[i]->GetSizer()->CalcMin();
      if (tmpSize.GetWidth() > minSize.GetWidth())
         minSize.SetWidth(tmpSize.GetWidth());
      if (tmpSize.GetHeight() > minSize.GetHeight())
         minSize.SetHeight(tmpSize.GetHeight());
   }
   SetPageSize(minSize);

   bool ret = wxWizard::RunWizard(myPageMergeMode);
   delete myPageMergeMode;
   delete myPageDirectMerge;
   delete myPageTwoWayMerge;
   delete myPageThreeWayMerge;
   return ret;
}



// Has next page
bool ConflictDialog::HasNextPage(wxWizardPage *page)
{
   if (page == myPageMergeMode)
      return true;
   else
      return wxWizard::HasNextPage(page);
}



// After changing the page
void ConflictDialog::OnPageChanged(wxWizardEvent& event)
{
}



// Before changing the page
void ConflictDialog::OnPageChanging(wxWizardEvent& event)
{
   // On the first page clicked "next"
   if (event.GetPage() == myPageMergeMode && event.GetDirection() == true)
   {
      // Determine new merge mode
      MergeMode newMergeMode = MERGE_UNDEF;
      if (myPageMergeMode->myButtonDirectEdit->GetValue())
         newMergeMode = MERGE_EDIT;
      else if (myPageMergeMode->myButtonTwoWayMerge->GetValue())
         newMergeMode = MERGE_TWO_WAY;
      else if (myPageMergeMode->myButtonThreeWayMerge->GetValue())
         newMergeMode = MERGE_THREE_WAY;

      
      // User has changed merge mode
      if (myMergeMode != MERGE_UNDEF)
      {
         if (newMergeMode != myMergeMode)
         {
            // Remove old merge files
            CleanupMergeFiles();
         }
      }
      
      myMergeMode = newMergeMode;
      // Create merge files
      CreateMergeFiles();
   }
}



// Create merge files
bool ConflictDialog::CreateMergeFiles()
{
   bool result = false;
   // Create new directory for merge
   myMergeDir = UniqueTemporaryDir();

   if (myMergeMode == MERGE_EDIT)
   {
      // New filename
      myFileDirect = myMergeDir + "\\" 
         + ExtractLastPart(myFile);

      // Remove read-only flag so overwriting works in case file exists
      SetFileReadOnly(myFileDirect.c_str(), false);
      // Copy file to merge dir
      if (!CopyFileA(myFile.c_str(), myFileDirect.c_str(), false))
         return false;
      // Remove read-only flag so user can edit the file
      SetFileReadOnly(myFileDirect.c_str(), false);
      result = true;
   }
   else if (myMergeMode == MERGE_TWO_WAY)
   {
      // New filenames
      myFileMine = myMergeDir + "\\" 
         + GetNameBody(ExtractLastPart(myFile)) + ".mine." + GetExtension(myFile);
      myFileYours = myMergeDir + "\\" 
         + GetNameBody(ExtractLastPart(myFile)) + ".yours." + GetExtension(myFile);
      // Remove read-only flags so overwriting works in case files exists
      SetFileReadOnly(myFileMine.c_str(), false);
      SetFileReadOnly(myFileYours.c_str(), false);
      
      // Create files from conflict file
/*      ConflictParser::ParseFile(myFile, 
         CVSStatus::GetFileFormat(myFile) == CVSStatus::
         
         myFileMine, myFileYours*/

      // Remove read-only flag so user can edit the file
      SetFileReadOnly(myFileMine.c_str(), false);
      SetFileReadOnly(myFileYours.c_str(), false);
   }
   else if (myMergeMode == MERGE_THREE_WAY)
   {
      // New filenames
      myFileMine = myMergeDir + "\\" 
         + GetNameBody(ExtractLastPart(myFile)) + ".mine." + GetExtension(myFile);
      myFileYours = myMergeDir + "\\" 
         + GetNameBody(ExtractLastPart(myFile)) + ".yours." + GetExtension(myFile);
      myFileAncestor = myMergeDir + "\\" 
         + GetNameBody(ExtractLastPart(myFile)) + ".ancestor." + GetExtension(myFile);
   }

   return result;
}



// Cleanup merge files
bool ConflictDialog::CleanupMergeFiles()
{
   if (!myMergeDir.empty())
   {
      // Remove entire merge sandbox
      DeleteDirectoryRec(myMergeDir);
   }
   myFileDirect.erase();
   myFileMine.erase();
   myFileYours.erase();
   myFileAncestor.erase();
   return true;
}



/////////////////////////////////////////////////////////////////////
// ConflictDialog::PageMergeMode

// Constructor
ConflictDialog::PageMergeMode::PageMergeMode(wxWizard* parent)
    : wxWizardPage(parent)
{
    wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);

    wxStaticText *text = new wxStaticText(this, -1, _("Choose Merge Operation"));
    wxFont titleFont(text->GetFont());
    titleFont.SetWeight(wxBOLD);
    text->SetFont(titleFont);
   
    sizerTop->Add(text, 0, wxBOTTOM, 5);
    sizerTop->Add(new wxStaticLine(this, -1), 0, wxGROW | wxTOP | wxBOTTOM, 5);

    text = new wxStaticText(this, -1, 
                            _("How would you like to merge the conflicts?"));
    sizerTop->Add(text, 0, wxTOP | wxBOTTOM, 5);
    
    myButtonDirectEdit = new wxRadioButton(this, -1, 
                                           _("Edit with default text editor"), 
                                           wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    sizerTop->Add(myButtonDirectEdit, 0, wxALL, 5);

    myButtonTwoWayMerge = new wxRadioButton(this, -1, 
                                            _("Two-way merge"), 
                                            wxDefaultPosition, wxDefaultSize, 0);
    sizerTop->Add(myButtonTwoWayMerge, 0, wxALL, 5);

    myButtonThreeWayMerge = new wxRadioButton(this, -1, 
                                              _("Three-way merge"), 
                                              wxDefaultPosition, wxDefaultSize, 0);
    sizerTop->Add(myButtonThreeWayMerge, 0, wxALL, 5);

    wxString tip(_("Hint: Three-way merges are only possible if your merge application supports them."));

    text = new ExtStaticText(this, -1, tip.c_str(), wxDefaultPosition, 
                             wxDLG_UNIT(this, wxSize(60, 10)));
    text->SetForegroundColour(ColorRefToWxColour(GetIntegerPreference("Colour Tip Text", "")));
    sizerTop->Add(5,5);
    sizerTop->Add(text, 0, wxALL | wxGROW, 5);

    // Overall dialog layout settings
    SetAutoLayout(TRUE);
    SetSizer(sizerTop);
    sizerTop->SetSizeHints(this);
    sizerTop->Fit(this);
}


// Next page
wxWizardPage* ConflictDialog::PageMergeMode::GetNext() const
{
   if (myButtonTwoWayMerge->GetValue())
      return ((ConflictDialog*) GetParent())->myPageTwoWayMerge;
   else if (myButtonThreeWayMerge->GetValue())
      return ((ConflictDialog*) GetParent())->myPageThreeWayMerge;
   else
      return ((ConflictDialog*) GetParent())->myPageDirectMerge;
}


// Previous page
wxWizardPage* ConflictDialog::PageMergeMode::GetPrev() const
{
   return 0;
}


/////////////////////////////////////////////////////////////////////
// ConflictDialog::PageDirectMerge

// Constructor
ConflictDialog::PageDirectMerge::PageDirectMerge(wxWizard* parent)
   : wxWizardPageSimple(parent)
{
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);

   wxStaticText *text = new wxStaticText(this, -1, _("Perform Direct Merge"));
   wxFont titleFont(text->GetFont());
   titleFont.SetWeight(wxBOLD);
   text->SetFont(titleFont);
   
   sizerTop->Add(text, 0, wxBOTTOM, 5);
   sizerTop->Add(new wxStaticLine(this, -1), 0, wxGROW | wxTOP | wxBOTTOM, 5);

   text = new wxStaticText(this, -1, 
      _("TortoiseCVS has created the following temporary merge file:"));
   sizerTop->Add(text, 0, wxTOP | wxBOTTOM, 5);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);
}



/////////////////////////////////////////////////////////////////////
// ConflictDialog::PageTwoWayMerge

// Constructor
ConflictDialog::PageTwoWayMerge::PageTwoWayMerge(wxWizard* parent)
   : wxWizardPageSimple(parent)
{
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);

   wxStaticText *text = new wxStaticText(this, -1, _("Perform Two-Way Merge"));
   wxFont titleFont(text->GetFont());
   titleFont.SetWeight(wxBOLD);
   text->SetFont(titleFont);
   
   sizerTop->Add(text, 0, wxBOTTOM, 5);
   sizerTop->Add(new wxStaticLine(this, -1), 0, wxGROW | wxTOP | wxBOTTOM, 5);

   text = new wxStaticText(this, -1, 
      _("TortoiseCVS has created the following temporary merge files:"));
   sizerTop->Add(text, 0, wxTOP | wxBOTTOM, 5);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);
}



/////////////////////////////////////////////////////////////////////
// ConflictDialog::PageThreeWayMerge

// Constructor
ConflictDialog::PageThreeWayMerge::PageThreeWayMerge(wxWizard* parent)
   : wxWizardPageSimple(parent)
{
   wxBoxSizer* sizerTop = new wxBoxSizer(wxVERTICAL);

   wxStaticText *text = new wxStaticText(this, -1, _("Perform Three-Way Merge"));
   wxFont titleFont(text->GetFont());
   titleFont.SetWeight(wxBOLD);
   text->SetFont(titleFont);
   
   sizerTop->Add(text, 0, wxBOTTOM, 5);
   sizerTop->Add(new wxStaticLine(this, -1), 0, wxGROW | wxTOP | wxBOTTOM, 5);

   text = new wxStaticText(this, -1, 
      _("TortoiseCVS has created the following temporary merge files:"));
   sizerTop->Add(text, 0, wxTOP | wxBOTTOM, 5);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);
}
