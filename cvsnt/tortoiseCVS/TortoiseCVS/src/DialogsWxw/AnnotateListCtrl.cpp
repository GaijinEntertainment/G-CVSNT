// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Alan Dabiri
// <killjoy@users.sourceforge.net> - March 2004

// Copyright (C) 2005 - Hartmut Honisch
// <Hartmut_Honisch@web.de>

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - March 2005

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
#include "AnnotateListCtrl.h"
#include "AnnotateDialog.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include "../cvstree/common.h"
#include "../TortoiseAct/TortoiseActVerbs.h"
#include "../Utils/TimeUtils.h"
#include "../Utils/TortoiseUtils.h"
#include <Utils/LaunchTortoiseAct.h>


#define ANNOTATE_HIGHLIGHT_AUTHOR_RGB       0xE0,0xFF,0xE0
#define ANNOTATE_HIGHLIGHT_REV_RGB          0xFF,0xFF,0xD0
#define ANNOTATE_HIGHLIGHT_DATE_RGB         0xDA,0xC9,0x9C
#define ANNOTATE_HIGHLIGHT_BUGNUMBER_RGB    0xE0,0xFF,0xE0

// TODO: Come up with better colors? Need an artist.
#define ANNOTATE_HIGHLIGHT_AGE_RGB1         0xFD,0x8B,0x8B      // newest color
#define ANNOTATE_HIGHLIGHT_AGE_RGB2         0xD0,0xF9,0xFF      // oldest color

// TODO: Maybe make these user configurable in the preferences menu?  If anyone cares..
static const int HIGHLIGHT_AGE_FIXED_INTERVALS  = 17;
static const int HIGHLIGHT_AGE_FIXED_PERIOD     = 21; // in days

enum 
{
   ANNOTATE_CONTEXT_ID_DIFF = 10001,
   ANNOTATE_CONTEXT_ID_VIEW,
   ANNOTATE_CONTEXT_ID_ANNOTATE,
   ANNOTATE_CONTEXT_ID_GET,
   ANNOTATE_CONTEXT_ID_SAVE_AS,
   ANNOTATE_CONTEXT_ID_LOG_MSG,
   ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_NONE,
   ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AUTHOR,
   ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_REV,
   ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_DATE,
   ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_FIXED,
   ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_SCALED,
   ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_BUGNUMBER,
   ANNOTATE_ID_FIND
};


BEGIN_EVENT_TABLE(AnnotateListCtrl, ExtListCtrl)
   EVT_CONTEXT_MENU(AnnotateListCtrl::ContextMenu)
   EVT_MENU(ANNOTATE_CONTEXT_ID_DIFF,                           AnnotateListCtrl::OnMenuDiff)
   EVT_MENU(ANNOTATE_CONTEXT_ID_VIEW,                           AnnotateListCtrl::OnMenuView)
   EVT_MENU(ANNOTATE_CONTEXT_ID_ANNOTATE,                       AnnotateListCtrl::OnMenuAnnotate)
   EVT_MENU(ANNOTATE_CONTEXT_ID_GET,                            AnnotateListCtrl::OnMenuGet)
   EVT_MENU(ANNOTATE_CONTEXT_ID_SAVE_AS,                        AnnotateListCtrl::OnMenuSaveAs)
   EVT_MENU(ANNOTATE_CONTEXT_ID_LOG_MSG,                        AnnotateListCtrl::OnMenuViewLogMessage)
   EVT_MENU(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_NONE,              AnnotateListCtrl::OnHighlightNone)
   EVT_MENU(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AUTHOR,            AnnotateListCtrl::OnHighlightAuthor)
   EVT_MENU(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_REV,               AnnotateListCtrl::OnHighlightRev)
   EVT_MENU(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_DATE,              AnnotateListCtrl::OnHighlightDate)
   EVT_MENU(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_FIXED,         AnnotateListCtrl::OnHighlightAgeFixed)
   EVT_MENU(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_SCALED,        AnnotateListCtrl::OnHighlightAgeScaled)
   EVT_MENU(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_BUGNUMBER,         AnnotateListCtrl::OnHighlightBugNumber)
END_EVENT_TABLE()


// Constructor
AnnotateListCtrl::AnnotateListCtrl(AnnotateDialog* parent,
                                   wxWindowID id,
                                   AnnotateCanvas* canvas)
   : ExtListCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
                 wxLC_REPORT | wxLC_ALIGN_LEFT | wxLC_VIRTUAL),
     myParent(parent),
     myCanvas(canvas)
{
}

// Get item text
wxString AnnotateListCtrl::OnGetItemText(long item, long column) const
{
    TDEBUG_ENTER("OnGetItemText");

    CAnnotation* annotation = (*myParent->myAnnotations)[item];
    ASSERT(annotation);

    switch (column)
    {
    case ANNOTATE_COL_LINE:
    {  
        wxString s;
        s.Printf(wxT("%d"), annotation->LineNum());
        return s;
    }

    case ANNOTATE_COL_REVISION:
        return wxText(annotation->RevNum().str());

    case ANNOTATE_COL_AUTHOR:
        return wxText(annotation->Author().c_str());

    case ANNOTATE_COL_DATE:
    {
        wxChar date[100];
        tm_to_local_DateFormatted(&annotation->Date(), date, sizeof(date), false);

        struct tm& tm(annotation->Date());
        TDEBUG_TRACE("tm " << tm.tm_mday << "/" << tm.tm_mon << "/" << (tm.tm_year+1900)
                     << " formatted " << wxAscii(date));
        return date;
    }

#if 0
    case ANNOTATE_COL_BUGNUMBER:
        return wxText(annotation->BugNumber().c_str());
#endif
        
    case ANNOTATE_COL_TEXT:
    {
        // Expand tabs
        std::string text = annotation->Text();
        FindAndReplace<std::string>(text, "\t", "    ");
        return wxText(text);
    }

    default:
        ASSERT(false);
        break;
    }

    return wxT("?");
}


// Get item attributes
wxListItemAttr *AnnotateListCtrl::OnGetItemAttr(long item) const
{
   static wxListItemAttr attr;
   attr.SetFont(myParent->myFixedFont);
   attr.SetBackgroundColour(wxNullColour);
   attr.SetBackgroundColour(*((wxColour*)(*myParent->myAnnotations)[item]->Data()));
   return &attr;
}


// Menu callbacks

// Show context menu
void AnnotateListCtrl::ContextMenu(wxContextMenuEvent& event)
{
   // Get number of selected items
   int iSelectedItems = GetSelectedItemCount();

   // Check if an item is selected
   wxMenu popupMenu;

   int checkedId = -1;
   switch (myParent->myHighlightType)
   {
   case HIGHLIGHT_BY_NONE:         checkedId = ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_NONE;          break;
   case HIGHLIGHT_BY_AGE_FIXED:    checkedId = ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_FIXED;     break;
   case HIGHLIGHT_BY_AGE_SCALED:   checkedId = ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_SCALED;    break;
   default:
       break;
   }
   if (iSelectedItems == 1)
      switch (myParent->myHighlightType) 
      {
      case HIGHLIGHT_BY_AUTHOR:       checkedId = ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AUTHOR;        break;
      case HIGHLIGHT_BY_REV:          checkedId = ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_REV;           break;
      case HIGHLIGHT_BY_DATE:         checkedId = ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_DATE;          break;
      case HIGHLIGHT_BY_BUGNUMBER:    checkedId = ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_BUGNUMBER;     break;
      default:
          break;
      }

   if (iSelectedItems == 1)
   {
      popupMenu.Append(ANNOTATE_CONTEXT_ID_DIFF,        _("&Diff (working file)"), wxT(""));
      popupMenu.Append(ANNOTATE_CONTEXT_ID_VIEW,        _("&View this revision"), wxT(""));
      popupMenu.Append(ANNOTATE_CONTEXT_ID_ANNOTATE,    _("&Annotate this revision"), wxT(""));
      popupMenu.Append(ANNOTATE_CONTEXT_ID_GET,         _("&Get this revision (sticky)"), wxT(""));
      popupMenu.Append(ANNOTATE_CONTEXT_ID_SAVE_AS,     _("&Save this revision as..."), wxT(""));
      popupMenu.AppendSeparator();
   }

   popupMenu.AppendCheckItem(ANNOTATE_CONTEXT_ID_LOG_MSG,     _("Show &log messages"), wxT(""));
   popupMenu.Check(ANNOTATE_CONTEXT_ID_LOG_MSG, myParent->myShowLogMessages);
   popupMenu.AppendSeparator();
   popupMenu.AppendRadioItem(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_NONE,
                             _("No Highlighting"), wxT(""));
   if (iSelectedItems == 1)
   {
      popupMenu.AppendRadioItem(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AUTHOR,
                                _("Highlight by Author"), wxT(""));
      popupMenu.AppendRadioItem(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_REV,
                                _("Highlight by Revision"), wxT(""));
      popupMenu.AppendRadioItem(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_DATE,
                                _("Highlight by Date"), wxT(""));
      popupMenu.AppendRadioItem(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_BUGNUMBER,
                                _("Highlight by Bug Number"), wxT(""));
   }
   popupMenu.AppendRadioItem(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_FIXED,
                             _("Highlight by Age (Fixed)"), wxT(""));
   popupMenu.AppendRadioItem(ANNOTATE_CONTEXT_ID_HIGHLIGHT_BY_AGE_SCALED,
                             _("Highlight by Age (Scaled)"), wxT(""));


   if (checkedId != -1)
      popupMenu.Check(checkedId, true);
   
   wxPoint pos = event.GetPosition();
   // Calc selection
   if (pos == wxPoint(-1, -1))
   {
      pos = GetContextMenuPos(GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED));
      pos = ClientToScreen(pos);
   }
   PopupMenu(&popupMenu, ScreenToClient(pos));
}


void AnnotateListCtrl::OnMenuDiff(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   LaunchTortoiseAct(false, CvsDiffVerb, myParent->myFilename,
                     "-r " + Quote(annotation->RevNum().str().c_str()),
                     GetRemoteHandle());
}


void AnnotateListCtrl::OnMenuView(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   LaunchTortoiseAct(false, CvsViewVerb, myParent->myFilename,
                     "-r " + Quote(annotation->RevNum().str().c_str()),
                     GetRemoteHandle());
}


void AnnotateListCtrl::OnMenuAnnotate(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   LaunchTortoiseAct(false, CvsAnnotateVerb, myParent->myFilename,
                     "-r " + Quote(annotation->RevNum().str().c_str()),
                     GetRemoteHandle());
}



void AnnotateListCtrl::OnMenuGet(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   LaunchTortoiseAct(false, CvsGetVerb, myParent->myFilename, 
                     "-r " + Quote(annotation->RevNum().str().c_str()),
                     GetRemoteHandle());
}


void AnnotateListCtrl::OnMenuSaveAs(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   LaunchTortoiseAct(false, CvsSaveAsVerb, myParent->myFilename, 
                     "-r " + Quote(annotation->RevNum().str().c_str()),
                     GetRemoteHandle());
}

void AnnotateListCtrl::OnMenuViewLogMessage(wxCommandEvent&)
{
   TDEBUG_ENTER("OnMenuViewLogMessage");

   if (!myParent->myGotLogMessages)
   {
      // Fill cache from "cvs log"
      ParserData parserData;
      if (!GetLog(Printf(_("Retrieving log messages for %s"), wxText(myParent->myFilename).c_str()),
                  this, myParent->myFilename.c_str(), parserData))
         return;

      // Fill output into our CAnnotation vector
      std::vector<CRevFile>::iterator i;
      for (i = parserData.rcsFile->AllRevs().begin(); i != parserData.rcsFile->AllRevs().end(); ++i)
      {
         TDEBUG_TRACE("Rev " << i->RevNum().str());
         for (int j = 0; j < static_cast<int>(myParent->myAnnotations->AnnotationCount()); ++j)
            if ((*myParent->myAnnotations)[j]->RevNum() == i->RevNum())
            {
               TDEBUG_TRACE("Msg " << i->DescLog().c_str());
               (*myParent->myAnnotations)[j]->SetLogMessage(i->DescLog().c_str());
            }
      }
      myParent->myGotLogMessages = true;
   }

   myParent->myShowLogMessages = !myParent->myShowLogMessages;
   myParent->myStatusBar->SetFieldsCount(myParent->myShowLogMessages ? 2 : 1);
   if (myParent->myShowLogMessages)
   {
      static const int w[] = { 120, -1 };
      myParent->myStatusBar->SetStatusWidths(2, w);

      int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
      if (item < 0 || GetSelectedItemCount() != 1)
         return;
      
      CAnnotation* annotation = (*myParent->myAnnotations)[item];
      myParent->myStatusBar->SetStatusText(wxText(annotation->LogMessage()), 1);
   }
}

void AnnotateListCtrl::OnHighlightNone(wxCommandEvent&)
{
   myParent->myHighlightType = HIGHLIGHT_BY_NONE;
   myParent->UpdateStatusBar();
   Refresh();
}

void AnnotateListCtrl::OnHighlightAuthor(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   myParent->myHighlightType = HIGHLIGHT_BY_AUTHOR;
   myParent->myHighlightAuthor = annotation->Author();

   DoHighlightAuthor(myParent->myHighlightAuthor);
}

void AnnotateListCtrl::DoHighlightAuthor(const std::string& author)
{
   int itemCount = GetItemCount();
   for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
   {
      wxColour color = (author == (*myParent->myAnnotations)[itemIndex]->Author())
                       ? wxColour(ANNOTATE_HIGHLIGHT_AUTHOR_RGB)
                       : wxNullColour;
      if (myParent->myLineColors)
      {
        *((wxColour*)(*myParent->myAnnotations)[itemIndex]->Data()) = color;
      }
   }

   myCanvas->RefreshLineColors(myParent->myAnnotations, 
                               itemCount, 
                               GetCountPerPage(),
                               myParent->myHighlightType);
   myParent->UpdateStatusBar();
   Refresh();
}

void AnnotateListCtrl::OnHighlightRev(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   myParent->myHighlightType = HIGHLIGHT_BY_REV;
   myParent->myHighlightRev = annotation->RevNum().str();

   DoHighlightRev(myParent->myHighlightRev);
}

void AnnotateListCtrl::DoHighlightRev(const std::string& revstring)
{
   TDEBUG_ENTER("DoHighlightRev");
   CRevNumber rev(revstring.c_str());
   int itemCount = GetItemCount();
   for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
   {
      const CRevNumber& currentRev = (*myParent->myAnnotations)[itemIndex]->RevNum();
      wxColour color = (rev == currentRev) ? wxColour(ANNOTATE_HIGHLIGHT_REV_RGB) : wxNullColour;
      if (myParent->myLineColors)
      {
        *((wxColour*)(*myParent->myAnnotations)[itemIndex]->Data()) = color;
      }
   }

   myCanvas->RefreshLineColors(myParent->myAnnotations, 
                               itemCount, 
                               GetCountPerPage(),
                               myParent->myHighlightType);
   myParent->UpdateStatusBar();
   Refresh();
}

void AnnotateListCtrl::OnHighlightDate(wxCommandEvent&)
{
   int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
   if (item < 0 || GetSelectedItemCount() != 1)
      return;

   CAnnotation* annotation = (*myParent->myAnnotations)[item];

   myParent->myHighlightType = HIGHLIGHT_BY_DATE;
   myParent->myHighlightDate = annotation->Date();

   DoHighlightDate(myParent->myHighlightDate);
}

void AnnotateListCtrl::DoHighlightDate(const tm& date)
{
   int itemCount = GetItemCount();
   for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
   {
      const tm& itemDate = (*myParent->myAnnotations)[itemIndex]->Date();
      bool dateEqual = date.tm_year == itemDate.tm_year
                     && date.tm_mon  == itemDate.tm_mon
                     && date.tm_mday == itemDate.tm_mday;
      wxColour color = dateEqual ? wxColour(ANNOTATE_HIGHLIGHT_DATE_RGB) : wxNullColour;
      if (myParent->myLineColors)
      {
        *((wxColour*)(*myParent->myAnnotations)[itemIndex]->Data()) = color;
      }
   }

   myCanvas->RefreshLineColors(myParent->myAnnotations, 
                               itemCount, 
                               GetCountPerPage(),
                               myParent->myHighlightType);
   myParent->UpdateStatusBar();
   Refresh();
}

void AnnotateListCtrl::OnHighlightAgeFixed(wxCommandEvent&)
{
   time_t currentDate;
   time(&currentDate);

   int itemCount = GetItemCount();
   for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
   {
       tm& itemDate = (*myParent->myAnnotations)[itemIndex]->Date();
       time_t date = mktime(&itemDate);
       double diff = difftime(currentDate, date);
       unsigned int days = (((unsigned int)diff / 3600) / 24);
       wxColour color = GetAgeColor(days, HIGHLIGHT_AGE_FIXED_INTERVALS, HIGHLIGHT_AGE_FIXED_PERIOD);
       if (myParent->myLineColors)
       {
         *((wxColour*)(*myParent->myAnnotations)[itemIndex]->Data()) = color;
       }
   }

   myParent->myHighlightType = HIGHLIGHT_BY_AGE_FIXED;
   myCanvas->RefreshLineColors(myParent->myAnnotations, 
                               itemCount, 
                               GetCountPerPage(),
                               myParent->myHighlightType);
   myParent->UpdateStatusBar();
   Refresh();
}

void AnnotateListCtrl::OnHighlightAgeScaled(wxCommandEvent&)
{
   time_t minDate = myParent->myAnnotations->MinDate();
   time_t maxDate = myParent->myAnnotations->MaxDate();

   double totalDiff = difftime(maxDate, minDate);
   unsigned int totalDays = (((unsigned int)totalDiff / 3600) / 24);

   unsigned int intervals = myParent->myAnnotations->DateCount();
   unsigned int period = totalDays / intervals;

   int itemCount = GetItemCount();
   for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
   {
      tm& itemDate = (*myParent->myAnnotations)[itemIndex]->Date();
      time_t date = mktime(&itemDate);
      double diff = difftime(maxDate, date);
      unsigned int days = (((unsigned int)diff / 3600) / 24);
      wxColour color = GetAgeColor(days, intervals, period);
      if (myParent->myLineColors)
      {
        *((wxColour*)(*myParent->myAnnotations)[itemIndex]->Data()) = color;
      }
   }

   myParent->myHighlightType = HIGHLIGHT_BY_AGE_SCALED;
   myCanvas->RefreshLineColors(myParent->myAnnotations, 
                               itemCount, 
                               GetCountPerPage(),
                               myParent->myHighlightType);
   myParent->UpdateStatusBar();
   Refresh();
}

void AnnotateListCtrl::OnHighlightBugNumber(wxCommandEvent&)
{
    int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item < 0 || GetSelectedItemCount() != 1)
        return;

    CAnnotation* annotation = (*myParent->myAnnotations)[item];
    const std::string& bugnumber = annotation->BugNumber();

    int itemCount = GetItemCount();
    for (int itemIndex = 0; itemIndex < itemCount; ++itemIndex)
    {
        wxColour color = (bugnumber == (*myParent->myAnnotations)[itemIndex]->BugNumber())
           ? wxColour(ANNOTATE_HIGHLIGHT_BUGNUMBER_RGB)
           : wxNullColour;
        if (myParent->myLineColors)
            *((wxColour*)(*myParent->myAnnotations)[itemIndex]->Data()) = color;
    }

    myParent->myHighlightType = HIGHLIGHT_BY_BUGNUMBER;
    myCanvas->RefreshLineColors(myParent->myAnnotations, 
                                itemCount, 
                                GetCountPerPage(),
                                myParent->myHighlightType);
    myParent->UpdateStatusBar();
    Refresh();
}


void AnnotateListCtrl::Refresh()
{
   ExtListCtrl::Refresh();
   myCanvas->Refresh();
}

void AnnotateListCtrl::SetHighlighting()
{
   wxCommandEvent e;
   switch (myParent->myHighlightType)
   {
   case HIGHLIGHT_BY_NONE:
      break;
   case HIGHLIGHT_BY_AUTHOR:
      DoHighlightAuthor(myParent->myHighlightAuthor);
      break;
   case HIGHLIGHT_BY_REV:
      DoHighlightRev(myParent->myHighlightRev);
      break;
   case HIGHLIGHT_BY_DATE:
      DoHighlightDate(myParent->myHighlightDate);
      break;
   case HIGHLIGHT_BY_AGE_FIXED:
      OnHighlightAgeFixed(e);
      break;
   case HIGHLIGHT_BY_AGE_SCALED:
      OnHighlightAgeScaled(e);
      break;
   case HIGHLIGHT_BY_BUGNUMBER:
       OnHighlightBugNumber(e);
       break;
   }
}

wxColour AnnotateListCtrl::GetAgeColor(unsigned int ageInDays, unsigned int intervals, unsigned int period)
{
   wxColour rgb1(ANNOTATE_HIGHLIGHT_AGE_RGB1);
   wxColour rgb2(ANNOTATE_HIGHLIGHT_AGE_RGB2);

   for (unsigned int colorIndex = 0; colorIndex < intervals; ++colorIndex)
   {
      if (ageInDays <= ((colorIndex + 1) * period))
      {
         float ratio = float(colorIndex) / float(intervals);
         unsigned char red   = (unsigned char) (rgb1.Red()   + (rgb2.Red()   - rgb1.Red())   * ratio);
         unsigned char green = (unsigned char) (rgb1.Green() + (rgb2.Green() - rgb1.Green()) * ratio);
         unsigned char blue  = (unsigned char) (rgb1.Blue()  + (rgb2.Blue()  - rgb1.Blue())  * ratio);
         return wxColour(red, green, blue);
      }
   }
   return rgb2;
}
