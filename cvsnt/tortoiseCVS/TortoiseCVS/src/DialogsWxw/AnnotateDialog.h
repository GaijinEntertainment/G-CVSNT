// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Alan Dabiri
// <killjoy@users.sourceforge.net> - March 2004

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

#ifndef ANNOTATE_DIALOG_H
#define ANNOTATE_DIALOG_H

#include <string>
#include <vector>

#include <wx/wx.h>
#include <wx/imaglist.h>

#include <windows.h>
#include "ExtDialog.h"
#include "../Utils/LogUtils.h"
#include "../TortoiseAct/Annotation.h"


class CVSRevisionData;
class CVSRevisionDataSortCriteria;
class CRcsFile;
class CRevFile;
class CRevNumber;
class AnnotateDialogEvtHandler;
class AnnotateListCtrl;
class FindDialog;
class wxListEvent;

enum HIGHLIGHT_TYPE
{
   HIGHLIGHT_BY_NONE = 0,
   HIGHLIGHT_BY_AUTHOR,
   HIGHLIGHT_BY_REV,
   HIGHLIGHT_BY_DATE,
   HIGHLIGHT_BY_BUGNUMBER,
   HIGHLIGHT_BY_AGE_FIXED,
   HIGHLIGHT_BY_AGE_SCALED
};

enum ANNOTATE_COL
{
   ANNOTATE_COL_LINE = 0,
   ANNOTATE_COL_REVISION,
   ANNOTATE_COL_AUTHOR,
   ANNOTATE_COL_DATE,
#if 0
   ANNOTATE_COL_BUGNUMBER,
#endif
   ANNOTATE_COL_TEXT,
   NUM_ANNOTATE_COL
};


class AnnotateCanvas;
class AnnotateListCtrl;


class AnnotateDialog : public ExtDialog
{
public:
   friend bool DoAnnotateDialog(wxWindow* parent, const std::string& filename, const std::string& rev);
   friend class AnnotateListCtrl;

   void FindAndMarkAll(const std::string& searchString, bool caseSensitive);
   void FindNext(const std::string& searchString, bool caseSensitive,
                 bool searchUp, bool wrapSearch);

private:
   AnnotateDialog(wxWindow* parent, const std::string& filename, const std::string& rev);
   ~AnnotateDialog();

   // Apply sort settings
   void ApplySort();

   // Get the annotation list
   CAnnotationList* GetAnnotationList(wxWindow* parent);

   void Populate(CAnnotationList* annotationList);

   void AddAnnotation(CAnnotation* item, unsigned int annotationIndex);

   void ListItemClick(wxListEvent& event);
   void ListColumnClick(wxListEvent& e);

   void GetRegistryData();
   void SaveRegistryData();
    
   bool SetNewLineFoundIfMatches(unsigned int index, const std::string& searchString, bool caseSensitive);

   int Compare(CAnnotation* annotation1, CAnnotation* annotation2) const;
   static int wxCALLBACK CompareFunc(long item1, long item2, long sortData);

   void UpdateStatusBar();

   void OnFind(wxCommandEvent& event);

   std::string                      myFilename;
   AnnotateCanvas* myAnnotateCanvas;
   AnnotateListCtrl*                myListCtrl;
   ANNOTATE_COL                     mySortCol;
   bool                             mySortAscending;
   wxColour*                        myLineColors;
   std::vector<int>                 myColumnWidths;
   int                              myListIndex;       
   CAnnotationList*                 myAnnotations;
   HIGHLIGHT_TYPE                   myHighlightType;
   std::string                      myHighlightAuthor;
   std::string                      myHighlightRev;
   tm                               myHighlightDate;
   AnnotateDialogEvtHandler*        myEvtHandler;
   wxFont                           myFixedFont;
   wxStatusBar*                     myStatusBar;
   std::string                      myRevision;
   bool                             myGotLogMessages;
   bool                             myShowLogMessages;
   FindDialog*                      myFindDialog;
   unsigned int                     myFoundLineIndex;
   wxColour                         myFoundLinePreviousColour;

   DECLARE_EVENT_TABLE()
};

class AnnotateCanvas : public wxPanel
{
public:
   AnnotateCanvas() {}
   AnnotateCanvas(wxWindow* parent, wxWindowID, const wxPoint& pos, const wxSize& size);
   ~AnnotateCanvas();

   void RefreshLineColors(CAnnotationList* annotations, 
                          unsigned int    numLinesTotal, 
                          unsigned int    numLinesVisible,
                          HIGHLIGHT_TYPE  highlightType);

   void OnPaint(wxPaintEvent& event);

protected:
   CAnnotationList* myAnnotations;
   unsigned int    myNumLinesTotal;
   unsigned int    myNumLinesVisible;
   HIGHLIGHT_TYPE  myHighlightType;

private:
   DECLARE_EVENT_TABLE()
};


#endif //ANNOTATE_DIALOG_H
