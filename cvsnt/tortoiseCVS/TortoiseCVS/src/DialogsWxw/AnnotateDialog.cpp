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

#include "StdAfx.h"

#include <limits> // std::numeric_limits<>
#include "AnnotateDialog.h"
#include "AnnotateListCtrl.h"
#include "MessageDialog.h"
#include "FindDialog.h"
#include "YesNoDialog.h"
#include "WxwHelpers.h"
#include "wx/statline.h"
#include "../Utils/Translate.h"
#include "../Utils/TortoiseUtils.h"
#include "../Utils/PathUtils.h"
#include "../Utils/TortoiseDebug.h"
#include "../Utils/TortoiseRegistry.h"
#include "../Utils/LogUtils.h"
#include "../Utils/TimeUtils.h"
#include "../Utils/StringUtils.h"
#include <Utils/Preference.h>
#include "../CVSGlue/CVSStatus.h"
#include "../CVSGlue/CVSAction.h"
#include "FilenameText.h"


static const int DEF_COLUMN_SIZE[NUM_ANNOTATE_COL] = {40, 50, 60, 70, 500};
static const int MAX_COLUMN_SIZE = 2000;

#define ANNOTATE_HIGHLIGHT_FIND_RGB         0x00,0xFF,0xFF


bool DoAnnotateDialog(wxWindow* parent, const std::string& filename, const std::string& rev)
{
   AnnotateDialog* dlg = new AnnotateDialog(parent, filename, rev);

   CAnnotationList* annotationList = dlg->GetAnnotationList(parent);
   bool ret = false;
   if (annotationList)
   {
      dlg->Populate(annotationList);
      ret = (dlg->ShowModal() == wxID_OK);
      delete annotationList;
   }
   DestroyDialog(dlg);
   return ret;
}

enum
{
   ANNOTATEDLG_ID_LISTCTRL = 10001
};

BEGIN_EVENT_TABLE(AnnotateDialog, ExtDialog)
   EVT_LIST_ITEM_SELECTED(ANNOTATEDLG_ID_LISTCTRL,      AnnotateDialog::ListItemClick)
   EVT_LIST_COL_CLICK(ANNOTATEDLG_ID_LISTCTRL,          AnnotateDialog::ListColumnClick)
   EVT_COMMAND(wxID_FIND, wxEVT_COMMAND_BUTTON_CLICKED, AnnotateDialog::OnFind)
END_EVENT_TABLE()
   
void AnnotateDialog::Populate(CAnnotationList* annotationList) 
{
    TDEBUG_ENTER("AnnotateDialog::Populate");
    myAnnotations = annotationList;
    unsigned int annotateCount = annotationList->AnnotationCount();
    myListCtrl->SetItemCount(annotateCount);

    // Create the line color array
    delete[] myLineColors;
    myLineColors = new wxColour[annotateCount];
    for (unsigned int i = 0; i < annotateCount; i++)
    {
        CAnnotation* item = (*annotationList)[i];
        TDEBUG_TRACE("Item " << i << ": " << asctime(&(item->Date())) << item->Text());
        item->SetData(&myLineColors[i]);
    }

    ApplySort();
}

int AnnotateDialog::Compare(CAnnotation* annotation1, CAnnotation* annotation2) const
{
    if (!mySortAscending)
        std::swap(annotation1, annotation2);

   switch (mySortCol)
   {
     case ANNOTATE_COL_LINE: 
     {
        unsigned int lineNum1 = annotation1->LineNum();
        unsigned int lineNum2 = annotation2->LineNum();
        if (lineNum1 > lineNum2)
            return 1;
        else if (lineNum1 < lineNum2)
            return -1;
        else
            return 0;
     }

     case ANNOTATE_COL_REVISION:
     {
        const CRevNumber& rev1 = annotation1->RevNum();
        const CRevNumber& rev2 = annotation2->RevNum();
        return rev1.cmp(rev2);
     }

     case ANNOTATE_COL_AUTHOR:
     {
        const std::string& author1 = annotation1->Author();
        const std::string& author2 = annotation2->Author();
        return author1.compare(author2);
     }

     case ANNOTATE_COL_DATE:
     {
        time_t date1 = mktime(&(annotation1->Date()));
        time_t date2 = mktime(&(annotation2->Date()));
        return int(difftime(date1, date2));
     }

#if 0
     case ANNOTATE_COL_BUGNUMBER:
     {
        const std::string& bugnumber1 = annotation1->BugNumber();
        const std::string& bugnumber2 = annotation2->BugNumber();
        return bugnumber1.compare(bugnumber2);
     }
#endif
     
     case ANNOTATE_COL_TEXT:
     {
        const std::string& text1 = annotation1->Text();
        const std::string& text2 = annotation2->Text();
        return text1.compare(text2);
     }
     default:
         ASSERT(0);
   }
   
   return 0;
}

AnnotateDialog::AnnotateDialog(wxWindow* parent, const std::string& filename, const std::string& rev)
   : ExtDialog(parent, -1, wxString(_("TortoiseCVS - Annotate")),
               wxDefaultPosition, wxSize(500,400),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX),
     myFilename(filename),
     mySortCol(ANNOTATE_COL_LINE),
     mySortAscending(true),
     myLineColors(0),
     myHighlightType(HIGHLIGHT_BY_NONE),
     myRevision(rev),
     myGotLogMessages(false),
     myShowLogMessages(false),
     myFindDialog(0),
     myFoundLineIndex(std::numeric_limits<unsigned int>::max())
{
    TDEBUG_ENTER("AnnotateDialog");
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   wxString sTitle = GetTitle();
   sTitle += wxT(" ");
   sTitle += wxText(ExtractLastPart(filename));
   if (!rev.empty())
   {
      sTitle += wxT(" (");
      sTitle += wxText(rev);
      sTitle += wxT(")");
   }
   SetTitle(sTitle);

   myAnnotateCanvas = new AnnotateCanvas(this, -1, wxDefaultPosition, wxSize(10,10));
   // List control    
   myListCtrl = new AnnotateListCtrl(this, ANNOTATEDLG_ID_LISTCTRL, myAnnotateCanvas);

   int idx = 0;
   myListCtrl->InsertColumn(idx++, _("Line"),      wxLIST_FORMAT_LEFT);
   myListCtrl->InsertColumn(idx++, _("Revision"),  wxLIST_FORMAT_LEFT);
   myListCtrl->InsertColumn(idx++, _("Author"),    wxLIST_FORMAT_LEFT);
   myListCtrl->InsertColumn(idx++, _("Date"),      wxLIST_FORMAT_LEFT);
#if 0
   myListCtrl->InsertColumn(idx++, _("Bug Number"),wxLIST_FORMAT_LEFT);
#endif
   myListCtrl->InsertColumn(idx++, _("Text"),      wxLIST_FORMAT_LEFT);

   myFixedFont = wxSystemSettings::GetFont(wxSYS_ANSI_FIXED_FONT); 

   wxBoxSizer *sizerAnnotate = new wxBoxSizer(wxHORIZONTAL);
   sizerAnnotate->Add(myListCtrl, 3, wxGROW | wxALL, 3);

   wxBoxSizer* sizerCanvas = new wxBoxSizer(wxVERTICAL);
   sizerCanvas->Add(myAnnotateCanvas, 1, wxGROW | wxTOP | wxBOTTOM, 21);
   sizerCanvas->Add(10, 19);

   sizerAnnotate->Add(sizerCanvas, 0, wxGROW);

   wxBoxSizer *sizer1 = new wxBoxSizer(wxVERTICAL);
   sizer1->Add(sizerAnnotate, 3, wxGROW | wxLEFT | wxUP | wxDOWN, 3);

   // OK button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* buttonOK = new wxButton(this, wxID_OK, _("OK"));
   buttonOK->SetDefault();
   sizerConfirm->Add(buttonOK, 0, wxALL, 5);
   wxButton* buttonSearch = new wxButton(this, wxID_FIND, _("&Find"));
   sizerConfirm->Add(buttonSearch, 0, wxALL, 5);
   SetDefaultItem(buttonOK);

   // Hidden Cancel button (only for making Esc close the dialog)
   wxButton* cancel = new wxButton(this, wxID_CANCEL, wxT(""));
   cancel->Hide();

   // Status bar
   myStatusBar = new wxStatusBar(this, -1);

   // Main box with everything in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(sizer1, 2, wxGROW | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   RestoreTortoiseDialogSize(this, "Annotate", wxSize(500, 400));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "Annotate");

   GetRegistryData();
   UpdateStatusBar();

   // Get column widths from registry (if not outdated)
   unsigned int numSavedColumns = static_cast<unsigned int>(myColumnWidths.size());
   if (numSavedColumns == NUM_ANNOTATE_COL)
   {
       for (unsigned int i = 0; i < NUM_ANNOTATE_COL; ++i)
       {
           int colWidth = (i < numSavedColumns) ? myColumnWidths[i] : DEF_COLUMN_SIZE[i];
           if (colWidth <= 0 || colWidth > MAX_COLUMN_SIZE)
               colWidth = DEF_COLUMN_SIZE[i];
           
           myListCtrl->SetColumnWidth(i, wxDLG_UNIT_X(this, colWidth));
       }
   }
}

AnnotateDialog::~AnnotateDialog()
{
    // Delete line colors
    delete[] myLineColors;
    if (myFindDialog)
        DestroyDialog(myFindDialog);

    SaveRegistryData();

    StoreTortoiseDialogSize(this, "Annotate");
}



// Apply sort settings
void AnnotateDialog::ApplySort()
{
   CAnnotationList::SortColumn col = CAnnotationList::SORT_LINE;
   switch (mySortCol)
   {
      case ANNOTATE_COL_LINE:
         col = CAnnotationList::SORT_LINE;
         break;
      case ANNOTATE_COL_REVISION:
         col = CAnnotationList::SORT_REV;
         break;
      case ANNOTATE_COL_AUTHOR:
         col = CAnnotationList::SORT_AUTHOR;
         break;
      case ANNOTATE_COL_DATE:
         col = CAnnotationList::SORT_DATE;
         break;
#if 0
      case ANNOTATE_COL_BUGNUMBER:
         col = CAnnotationList::SORT_BUGNUMBER;
         break;
#endif
      case ANNOTATE_COL_TEXT:
         col = CAnnotationList::SORT_TEXT;
         break;
      default:
         ASSERT(false);
   }

   myAnnotations->Sort(col, mySortAscending);
   myListCtrl->SetSortIndicator(mySortCol, mySortAscending);
   myListCtrl->SetHighlighting();
   myListCtrl->Refresh();
   // Reset the Find operation
   myFoundLineIndex = std::numeric_limits<unsigned int>::max();
}




void AnnotateDialog::ListItemClick(wxListEvent& event)
{
   if (myShowLogMessages)
   {
      CAnnotation* annotation = (*myAnnotations)[event.GetIndex()];
      myStatusBar->SetStatusText(wxText(annotation->LogMessage()), 1);
   }
}


void AnnotateDialog::ListColumnClick(wxListEvent& e)
{
   int column = e.GetColumn();
   if (column == mySortCol) 
   {
      mySortAscending = !mySortAscending;
   }
   else
   {
      mySortCol = (ANNOTATE_COL) column;
      mySortAscending = true;
   }

   ApplySort();
}


int wxCALLBACK AnnotateDialog::CompareFunc(long annotateIndex1, long annotateIndex2, long sortData)
{
   AnnotateDialog* dlg = reinterpret_cast<AnnotateDialog*>(sortData);
   CAnnotation* annotation1 = (*dlg->myAnnotations)[annotateIndex1];
   CAnnotation* annotation2 = (*dlg->myAnnotations)[annotateIndex2];
   return dlg->Compare(annotation1, annotation2);
}

void AnnotateDialog::GetRegistryData()
{
   myColumnWidths.clear();
   TortoiseRegistry::ReadVector("Dialogs\\Annotate\\Column Width", myColumnWidths);
   int sortCol = ANNOTATE_COL_LINE;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\SortCol", sortCol);
   if (sortCol < 0 || sortCol >= NUM_ANNOTATE_COL)
      sortCol = ANNOTATE_COL_LINE;  // discard corrupt registry entry
   mySortCol = (ANNOTATE_COL) sortCol;
   mySortAscending = TortoiseRegistry::ReadBoolean("Dialogs\\Annotate\\SortAscending", true);
   int highlightType = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\Highlight", highlightType);
   myHighlightType = (HIGHLIGHT_TYPE) highlightType;
   myHighlightAuthor = TortoiseRegistry::ReadString("Dialogs\\Annotate\\HighlightAuthor");
   myHighlightRev = TortoiseRegistry::ReadString("Dialogs\\Annotate\\HighlightRevision");
   myHighlightDate.tm_sec = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\HighlightDateSec", myHighlightDate.tm_sec);
   myHighlightDate.tm_min = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\HighlightDateMin", myHighlightDate.tm_min);
   myHighlightDate.tm_hour = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\HighlightDateHour", myHighlightDate.tm_hour);
   myHighlightDate.tm_mday = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\HighlightDateDay", myHighlightDate.tm_mday);
   myHighlightDate.tm_mon = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\HighlightDateMonth", myHighlightDate.tm_mon);
   myHighlightDate.tm_year = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\HighlightDateYear", myHighlightDate.tm_year);
   myHighlightDate.tm_wday = 0;
   myHighlightDate.tm_yday = 0;
   myHighlightDate.tm_isdst = 0;
   TortoiseRegistry::ReadInteger("Dialogs\\Annotate\\HighlightDateDst", myHighlightDate.tm_isdst);
}

void AnnotateDialog::SaveRegistryData()
{
   myColumnWidths.clear();
   for (int i = 0; i < NUM_ANNOTATE_COL; ++i)
   {
      int w = myListCtrl->GetColumnWidth(i);
      w = this->ConvertPixelsToDialog(wxSize(w, 0)).GetWidth();
      myColumnWidths.push_back(w);
   }
   TortoiseRegistry::WriteVector("Dialogs\\Annotate\\Column Width", myColumnWidths);   
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\SortCol", mySortCol);
   TortoiseRegistry::WriteBoolean("Dialogs\\Annotate\\SortAscending", mySortAscending);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\Highlight", myHighlightType);
   TortoiseRegistry::WriteString("Dialogs\\Annotate\\HighlightAuthor", myHighlightAuthor);
   TortoiseRegistry::WriteString("Dialogs\\Annotate\\HighlightRevision", myHighlightRev);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\HighlightDateSec", myHighlightDate.tm_sec);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\HighlightDateMin", myHighlightDate.tm_min);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\HighlightDateHour", myHighlightDate.tm_hour);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\HighlightDateDay", myHighlightDate.tm_mday);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\HighlightDateMonth", myHighlightDate.tm_mon);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\HighlightDateYear", myHighlightDate.tm_year);
   TortoiseRegistry::WriteInteger("Dialogs\\Annotate\\HighlightDateDst", myHighlightDate.tm_isdst);
}

void AnnotateDialog::UpdateStatusBar()
{
   switch(myHighlightType)
   {
      case HIGHLIGHT_BY_AUTHOR:
      {
         myStatusBar->SetStatusText(_("Highlight by Author"));
         break;
      }
      case HIGHLIGHT_BY_REV:
      {
         myStatusBar->SetStatusText(_("Highlight by Revision"));
         break;
      }
      case HIGHLIGHT_BY_DATE:
      {
         myStatusBar->SetStatusText(_("Highlight by Date"));
         break;
      }
      case HIGHLIGHT_BY_BUGNUMBER:
      {
         myStatusBar->SetStatusText(_("Highlight by Bug Number"));
         break;
      }
      case HIGHLIGHT_BY_AGE_FIXED:
      {
         myStatusBar->SetStatusText(_("Highlight by Age (Fixed)"));
         break;
      }
      case HIGHLIGHT_BY_AGE_SCALED:
      {
         myStatusBar->SetStatusText(_("Highlight by Age (Scaled)"));
         break;
      }
      default:
      {
         myStatusBar->SetStatusText(_("No Highlighting"));
      }
   }
}

void AnnotateDialog::OnFind(wxCommandEvent& event)
{
   if (!myFindDialog)
   {
      myFindDialog = new FindDialog(this);
   }
   myFindDialog->Show(true);
}


// Get the annotation list
CAnnotationList* AnnotateDialog::GetAnnotationList(wxWindow* parent)
{
    TDEBUG_ENTER("GetAnnotationList");
    wxBusyCursor();

   // We do not want any progress dialog here
   CVSAction glue(parent);
   glue.SetCloseIfOK(true);
   glue.SetHideStdout();
   wxString title = Printf(_("Annotate %s"), wxText(myFilename).c_str());
   glue.SetProgressCaption(title);

   MakeArgs args;
   args.add_option("annotate");
   // Avoid truncating user names
   args.add_option("-w");
   args.add_option("30");       // Ought to be enough
   // If revision is given, specify it
   if (!myRevision.empty())
   {
      args.add_option("-r");
      args.add_option(myRevision);
   }
   // If we are working on a branch, specify the branch using -r
   else if (CVSStatus::HasStickyTag(myFilename))
   {
      args.add_option("-r");
      args.add_option(CVSStatus::GetStickyTag(myFilename));
   }
   args.add_arg(ExtractLastPart(myFilename));
   bool ok = glue.Command(StripLastPart(myFilename), args);
   if (!ok)
   {
      return 0;
   }
   
   std::string out = glue.GetOutputText();
#if 0
   // Debugging
   std::ifstream in("C:\\cvs-log-output.txt");
   out.clear();
   while (true)
   {
      std::string line;
      std::getline(in, line);
      if (line.empty() && in.eof())
         break;
      out += line;
      out += "\n";
   }
   in.close();
#endif
   FindAndReplace<std::string>(out, "\r\n", "\n");

   if (out.empty())
   {
      wxString s(_("This file is new and has never been committed to the server or is an empty file on the server."));
      s += wxT("\n\n");
      s += wxText(myFilename);
      DoMessageDialog(0, s);
      return 0;
   }

   CAnnotationList* annotationList = new CAnnotationList();

   std::stringstream ifs(out.c_str());

   // Parsing the annotations
   while (true)
   {
      std::string line;
      if (!std::getline(ifs, line) && line.empty())
         break;

      // We need to skip any text which comes before the actual annotations.  A valid
      // annotation starts with the revision number, so we do a simple check to see if
      // the first character is a digit.  This may need to be revised if this assumption
      // is not valid.
      if (!isdigit(line[0]))
         continue;

      annotationList->AddAnnotation(line);
   }
   return annotationList;
}


void AnnotateDialog::FindAndMarkAll(const std::string& searchString,
                                    bool caseSensitive)
{
   wxColour foundColour(ANNOTATE_HIGHLIGHT_FIND_RGB);
   unsigned int itemCount = myAnnotations->AnnotationCount();
   for (unsigned int i = 0; i < itemCount; ++i)
   {
      if ((*myAnnotations)[i]->IsTextMatching(searchString, caseSensitive))
      {
         *((wxColour*)(*myAnnotations)[i]->Data()) = foundColour;
      }
      else if (foundColour == *((wxColour*)(*myAnnotations)[i]->Data()))
      {
         *((wxColour*)(*myAnnotations)[i]->Data()) = wxNullColour;
      }
   }
   myAnnotateCanvas->RefreshLineColors(myAnnotations,
                                       itemCount,
                                       myListCtrl->GetCountPerPage(),
                                       myHighlightType);
   myListCtrl->Refresh();
}

bool AnnotateDialog::SetNewLineFoundIfMatches(unsigned int index,
                                              const std::string& searchString,
                                              bool caseSensitive)
{
   CAnnotation* annotation = (*myAnnotations)[index];
   if (annotation->IsTextMatching(searchString, caseSensitive))
   {
      if (index != myFoundLineIndex)
      {
         if (myFoundLineIndex != std::numeric_limits<unsigned int>::max())
         {
            *((wxColour*)(*myAnnotations)[myFoundLineIndex]->Data()) =
               myFoundLinePreviousColour;
            myListCtrl->RefreshItem(myFoundLineIndex);
         }
         myFoundLinePreviousColour = *((wxColour*)annotation->Data());
         myFoundLineIndex = index;
         *((wxColour*)annotation->Data()) = wxColour(ANNOTATE_HIGHLIGHT_FIND_RGB);
      }
      myAnnotateCanvas->RefreshLineColors(myAnnotations,
                                          myAnnotations->AnnotationCount(),
                                          myListCtrl->GetCountPerPage(),
                                          myHighlightType);

      myListCtrl->EnsureVisible(myFoundLineIndex);
      myListCtrl->RefreshItem(myFoundLineIndex);
      return true;
   }
   return false;
}

void AnnotateDialog::FindNext(const std::string& searchString,
                              bool caseSensitive,
                              bool searchUp,
                              bool wrapSearch)
{
   unsigned int max = std::numeric_limits<unsigned int>::max();
   bool isFound = false;
   bool isFirstFind = (myFoundLineIndex == max);

   if (searchUp)
   {
      unsigned int begin = (isFirstFind
                            ? myAnnotations->AnnotationCount()-1
                            : myFoundLineIndex-1);
      unsigned int end = 0;

      // As this is unsigned and we're going backwards we must check against
      // max to prevent unsigned int overflow
      for (unsigned int i = begin; i >= end && !isFound && i != max; --i)
         isFound = SetNewLineFoundIfMatches(i, searchString, caseSensitive);

      if (!isFound && !isFirstFind &&
          (wrapSearch || DoYesNoDialog(myFindDialog,
                                       _("Continue search from the end of the file?"), true)))
      {
         begin = myAnnotations->AnnotationCount()-1;
         end = myFoundLineIndex;
         // As this is unsigned and we're going backwards we must check against
         // max to prevent unsigned int overflow
         for (unsigned int i = begin; i >= end && !isFound && i != max; --i)
            isFound = SetNewLineFoundIfMatches(i, searchString, caseSensitive);
      }
   }
   else
   {
      unsigned int begin = (isFirstFind ? 0 : myFoundLineIndex+1);
      unsigned int end = myAnnotations->AnnotationCount();

      for (unsigned int i = begin; i < end && !isFound; ++i)
      {
         isFound = SetNewLineFoundIfMatches(i, searchString, caseSensitive);
      }
      if (!isFound && !isFirstFind &&
          (wrapSearch || DoYesNoDialog(myFindDialog,
                                       _("Continue search from the beginning of the file?"), true)))
      {
         begin = 0;
         end = myFoundLineIndex+1;
         for (unsigned int i = begin; i < end && !isFound; ++i)
            isFound = SetNewLineFoundIfMatches(i, searchString, caseSensitive);
      }
   }
   if (!isFound)
      DoMessageDialog(myFindDialog, _("Search complete"));
}

BEGIN_EVENT_TABLE(AnnotateCanvas, wxPanel)
   EVT_PAINT(AnnotateCanvas::OnPaint)
END_EVENT_TABLE()

AnnotateCanvas::AnnotateCanvas(wxWindow *parent, 
                               wxWindowID id,
                               const wxPoint &pos, 
                               const wxSize &size)
   : wxPanel(parent, id, pos, size, wxFULL_REPAINT_ON_RESIZE),
     myAnnotations(0),
     myNumLinesTotal(0),
     myNumLinesVisible(0)
{
}

AnnotateCanvas::~AnnotateCanvas()
{
}

void AnnotateCanvas::RefreshLineColors(CAnnotationList* annotations, 
                                       unsigned int    numLinesTotal, 
                                       unsigned int    numLinesVisible,
                                       HIGHLIGHT_TYPE  highlightType)
{
   if (!annotations || !numLinesTotal)
      return;

   myAnnotations = annotations;
   myNumLinesTotal = numLinesTotal;
   myNumLinesVisible = numLinesVisible;
   myHighlightType = highlightType;
   Refresh();
}

void AnnotateCanvas::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
   wxPaintDC dc(this);
   PrepareDC(dc);

   if (!myAnnotations || !myNumLinesTotal)
      return;

   int width;
   int height;
   GetClientSize(&width, &height);

   float lineHeight = float(height) / float(myNumLinesTotal);

   unsigned int startLine = 0;
   unsigned int endLine = 0;
   wxColour blockColor(*((wxColour*)(*myAnnotations)[startLine]->Data()));
   for (unsigned int i = 1; i < myAnnotations->AnnotationCount(); ++i)
   {
      bool lastLine = (i == (myNumLinesTotal - 1));

      // We are currently in a run of lines, and this line also matches the block color
      // Adjust the ending line of the block and continue processing lines.
      if (blockColor == *((wxColour*)(*myAnnotations)[i]->Data()) && !lastLine)
         endLine = i;

      // We had a run of lines that has now ended, we need to draw the colored rect
      // for this block, and then continue processing lines.
      else if (blockColor != *((wxColour*)(*myAnnotations)[i]->Data()) || lastLine) 
      {
         if (lastLine)
            endLine = i;

         wxCoord yCoord = (wxCoord) (startLine * lineHeight);
         wxCoord height = (wxCoord) ((endLine + 1 - startLine) * lineHeight);

         bool isDefault = (blockColor == wxNullColour);

         wxBrush brush;
         wxPen pen;

         // Set the brush
         if (isDefault)
             brush = *wxTRANSPARENT_BRUSH;
         else
             brush = wxBrush(blockColor, wxSOLID);
         dc.SetBrush(brush);
         // Set the pen
         if (isDefault)
            pen = *wxTRANSPARENT_PEN;
         else if (myHighlightType == HIGHLIGHT_BY_AGE_FIXED || myHighlightType == HIGHLIGHT_BY_AGE_SCALED)
            pen = wxPen(blockColor, 1, wxSOLID);
         else
            pen = wxPen(wxColour(0x00, 0x00, 0x00), 1, wxSOLID);
         dc.SetPen(pen);

         dc.DrawRectangle(0, yCoord, 10, height);

         // Set up the next block
         blockColor = *((wxColour*)(*myAnnotations)[i]->Data());
         startLine = endLine = i;
      }
   }

   // Draw a border rect for the age types
   if (myHighlightType == HIGHLIGHT_BY_AGE_FIXED || myHighlightType == HIGHLIGHT_BY_AGE_SCALED)
   {
      dc.SetBrush(*wxTRANSPARENT_BRUSH);
      dc.SetPen(wxPen(wxColour(0x00,0x00,0x00), 1, wxSOLID));
      dc.DrawRectangle(0, 0, width, height);
   }
}
