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

#ifndef CONFLICT_DIALOG_H
#define CONFLICT_DIALOG_H

#include "../Utils/FixCompilerBugs.h"
#include <string>
#include <wx/wx.h>
#include <wx/wizard.h>
#include "../Utils/PathUtils.h"
#include "ExtDialog.h"


class ConflictDialog : public wxWizard
{
public:
   friend bool DoConflictDialog(wxWindow* parent, 
                                const std::string& file);
private:
   enum MergeMode 
   {
      MERGE_UNDEF,
      MERGE_EDIT,
      MERGE_TWO_WAY,
      MERGE_THREE_WAY
   };

   //////////////////////////////////////////////////////////
   // Wizard pages

   // Page for choosing merge mode
   class PageMergeMode : public wxWizardPage
   {
   public:
      // Constructor
      PageMergeMode(wxWizard* parent);
      // Next page
      wxWizardPage* GetNext() const;
      // Previous page
      wxWizardPage* GetPrev() const;

      wxRadioButton* myButtonDirectEdit;
      wxRadioButton* myButtonTwoWayMerge;
      wxRadioButton* myButtonThreeWayMerge;
   };
   friend class PageMergeMode;

   // Page for performing direct merge
   class PageDirectMerge : public wxWizardPageSimple
   {
   public:
      // Constructor
      PageDirectMerge(wxWizard* parent);
   };

   // Page for performing two-way merge
   class PageTwoWayMerge : public wxWizardPageSimple
   {
   public:
      // Constructor
      PageTwoWayMerge(wxWizard* parent);
   };

   // Page for performing three-way merge
   class PageThreeWayMerge : public wxWizardPageSimple
   {
   public:
      // Constructor
      PageThreeWayMerge(wxWizard* parent);
   };

   
   
   // Constructor
   ConflictDialog(wxWindow* parent, const std::string& file);
   // Destructor
   ~ConflictDialog();
   // Run wizard
   bool RunWizard();
   // Has next page
   bool HasNextPage(wxWizardPage *page);
   // After changing the page
   void OnPageChanged(wxWizardEvent& event);
   // Before changing the page
   void OnPageChanging(wxWizardEvent& event);
   // Create merge files
   bool CreateMergeFiles();
   // Cleanup merge files
   bool CleanupMergeFiles();


   MergeMode myMergeMode;
   std::string myFile;

   PageMergeMode* myPageMergeMode;
   PageDirectMerge* myPageDirectMerge;
   PageTwoWayMerge* myPageTwoWayMerge;
   PageThreeWayMerge* myPageThreeWayMerge;

   std::string myMergeDir;
   std::string myFileDirect;
   FileChangeParams myFileDirectFcp;
   std::string myFileMine;
   FileChangeParams myFileMineFcp;
   std::string myFileAncestor;
   FileChangeParams myFileAncestorFcp;
   std::string myFileYours;
   FileChangeParams myFileYoursFcp;

   DECLARE_EVENT_TABLE()
};

#endif

