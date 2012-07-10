// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2002 - Torsten Martinsen
// <torsten@image.dk> - May 2002

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
#include "GraphDialog.h"
#include "GraphLogData.h"
#include "BranchTagDlg.h"
#include "MessageDialog.h"
#include "ChooseFile.h"
#include "GraphSettingsDialog.h"
#include "GraphLogData.h"
#include "WxwHelpers.h"
#include "WxwMain.h"
#include <wx/statline.h>
#include <wx/tooltip.h>
#include <wx/settings.h>
#include <wx/dcmemory.h>
#include <CVSGlue/CVSStatus.h>
#include <CVSGlue/CVSAction.h>
#include <TortoiseAct/TortoiseActVerbs.h>
#include <Utils/LogUtils.h>
#include <Utils/TimeUtils.h>
#include <Utils/PathUtils.h>
#include <Utils/ShellUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseException.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/ProcessUtils.h>
#include <Utils/Translate.h>
#include <Utils/LaunchTortoiseAct.h>
#include <Utils/Preference.h>
#include <cmath>
#include <sstream>
#include <io.h>
#include <fcntl.h>
#include <richedit.h>

// TODO:
// - hide empty branches?
// - Put the tags in the revision boxes themselves
//

#define STARTPOINT      5, 5
#define TABSTOP         1500

#define MIN_SCALE       0.05
#define SCALE_MULTIPLIER 1.5

// IDs for context menu entries
enum 
{
   GRAPH_CONTEXT_ID_DIFF_WORKFILE = 10001,
   GRAPH_CONTEXT_ID_DIFF_SELECTED,
   GRAPH_CONTEXT_ID_VIEW,
   GRAPH_CONTEXT_ID_ANNOTATE,
   GRAPH_CONTEXT_ID_GET,
   GRAPH_CONTEXT_ID_GET_CLEAN,
   GRAPH_CONTEXT_ID_SAVE_AS,
   GRAPH_CONTEXT_ID_MAKE_TAG,
   GRAPH_CONTEXT_ID_MAKE_BRANCH,
   GRAPH_CONTEXT_ID_MERGE,
   GRAPH_CONTEXT_ID_UNDO,
   GRAPH_CONTEXT_ID_SAVE_GRAPH,
   GRAPH_CONTEXT_ID_ZOOM_IN,
   GRAPH_CONTEXT_ID_ZOOM_OUT,
   GRAPH_CONTEXT_ID_HIDE,
   GRAPH_CONTEXT_ID_SHOW_ONLY,
   GRAPH_CONTEXT_ID_SHOW_ALL,

   GRAPHDLG_ID_SHOWTAGS,
   GRAPHDLG_ID_COLLAPSE,
   GRAPHDLG_ID_SETLIMITS,
   GRAPHDLG_ID_TEXTCTRL
};


BEGIN_EVENT_TABLE(GraphDialog, ExtDialog)
   EVT_COMMAND(GRAPHDLG_ID_SHOWTAGS,  wxEVT_COMMAND_CHECKBOX_CLICKED, GraphDialog::SymbolicCheckChanged)
   EVT_COMMAND(GRAPHDLG_ID_COLLAPSE,  wxEVT_COMMAND_CHECKBOX_CLICKED, GraphDialog::CollapseCheckChanged)
   EVT_COMMAND(GRAPHDLG_ID_SETLIMITS, wxEVT_COMMAND_BUTTON_CLICKED,   GraphDialog::SetLimits)
   EVT_COMMAND(GRAPHDLG_ID_TEXTCTRL,  wxEVT_COMMAND_TEXT_URL,         GraphDialog::OnTextUrl)
END_EVENT_TABLE()


bool DoGraphDialog(wxWindow* parent, const std::string& filename)
{
   bool ret = true;

   GraphDialog* dlg = new GraphDialog(parent, filename);
   wxString title(Printf(_("Revision Graph for %s"), wxTextCStr(filename)));
   CvsLogParameters settings(filename, false,
                             dlg->myCollapseRevisionsCheckBox->GetValue(),
                             dlg->myShowTagsCheckBox->GetValue(),
                             dlg->myOnlyShownBranch,
                             dlg->myHiddenBranches,
                             dlg->myStartDate,
                             dlg->myEndDate);
   dlg->myTree = GetLogGraph(title, parent, filename.c_str(), dlg->myParserData, &settings);
   if (dlg->myTree)
   {
      dlg->myRevNo = CVSStatus::GetRevisionNumber(filename);
      dlg->DoDraw(dlg->myTree);
      dlg->ShowDiskNode();

      ret = (dlg->ShowModal() == wxID_OK);
   }

   DestroyDialog(dlg);
   return ret;
}


// GraphCanvas

class GraphCanvas : public wxScrolledWindow
{
public:
   GraphCanvas(wxWindow* parent, wxWindowID, const wxPoint &pos, const wxSize &size, 
               GraphDialog* graphDlg);
   ~GraphCanvas();

   void OnPaint(wxPaintEvent &event);
   void OnMouseEvent(wxMouseEvent& event);
   void OnMouseWheel(wxMouseEvent& event);
   void OnMakeTag(wxCommandEvent& event);
   void OnMakeBranch(wxCommandEvent& event);
   void OnMenuDiffWorkfile(wxCommandEvent& event);
   void OnMenuDiffSelected(wxCommandEvent& event);
   void OnMenuMerge(wxCommandEvent& event);
   void OnMenuUndo(wxCommandEvent& event);
   void OnMenuView(wxCommandEvent& event);
   void OnMenuGet(wxCommandEvent& event);
   void OnMenuGetClean(wxCommandEvent& event);
   void OnMenuSaveAs(wxCommandEvent& event);
   void OnMenuAnnotate(wxCommandEvent& event);
   void OnMenuSaveGraph(wxCommandEvent& event);
   void OnMenuShowOnly(wxCommandEvent& event);
   void OnMenuShowAll(wxCommandEvent& event);
   void OnMenuHide(wxCommandEvent& event);
   void OnMenuZoomIn(wxCommandEvent& event);
   void OnMenuZoomOut(wxCommandEvent& event);

   void CalcImageSize();
   void OnEraseBackground(wxEraseEvent& event);
private:
   GraphDialog* myGraphDlg;
   bool         myInitialized;
   std::string  myRevision;
   std::string  myDir;
   std::string  myFile;

   DECLARE_CLASS(GraphCanvas)
   DECLARE_EVENT_TABLE()
};


class MouseWheelEventHandler : public wxEvtHandler
{
public:
   MouseWheelEventHandler(GraphCanvas* canvas)
      : myCanvas(canvas)
   {
   }

   void OnWheel(wxMouseEvent& event)
   {
      myCanvas->OnMouseWheel(event);
   }

private:
   GraphCanvas* myCanvas;

   DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MouseWheelEventHandler, wxEvtHandler)
    EVT_MOUSEWHEEL(MouseWheelEventHandler::OnWheel)
END_EVENT_TABLE()




GraphDialog::GraphDialog(wxWindow* parent, const std::string& filename)
   : ExtDialog(parent, -1, Printf(_("TortoiseCVS - Revision Graph for %s"), wxText(filename).c_str()),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | 
               wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxCLIP_CHILDREN),
     myFilename(filename),
     myTree(0),
     myNodeSelected(0),
     myOldNode(0),
     myZoom(1.0)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   myOfferView = FileIsViewable(filename);
   
   // Create splitter
   mySplitter = new ExtSplitterWindow(this, -1, wxDefaultPosition, 
                                      wxDefaultSize, wxSP_3D | wxSP_LIVE_UPDATE);
   wxPanel* topPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
   wxPanel* bottomPanel = new wxPanel(mySplitter, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
   mySplitter->SplitHorizontally(topPanel, bottomPanel, 0);
   mySplitter->SetMinimumPaneSize(wxDLG_UNIT_Y(this, 30));
   mySplitter->SetSize(0, wxDLG_UNIT_Y(this, 60));
   wxBoxSizer *sizerTopPanel = new wxBoxSizer(wxVERTICAL);
   wxBoxSizer *sizerBottomPanel = new wxBoxSizer(wxVERTICAL);
   topPanel->SetSizer(sizerTopPanel);
   bottomPanel->SetSizer(sizerBottomPanel);
   mySplitter->SplitHorizontally(topPanel, bottomPanel, 0);
   mySplitter->SetMinimumPaneSize(wxDLG_UNIT_Y(this, 50));
   mySplitter->SetSize(wxDLG_UNIT(this, wxSize(200, 200)));

   // Checkboxes etc.

   wxBoxSizer* sizerCheck = new wxBoxSizer(wxHORIZONTAL);

   myShowTagsCheckBox = new wxCheckBox(topPanel, GRAPHDLG_ID_SHOWTAGS, _("Show &tags"));
   sizerCheck->Add(myShowTagsCheckBox, 0, wxALL, 5);
     
   myCollapseRevisionsCheckBox = new wxCheckBox(topPanel, GRAPHDLG_ID_COLLAPSE, _("&Collapse revisions"));
   sizerCheck->Add(myCollapseRevisionsCheckBox, 0, wxALL, 5);

   sizerTopPanel->Add(sizerCheck, 0, wxALL, 5);
   
   myCanvas = new GraphCanvas(topPanel, -1, wxDefaultPosition, wxDLG_UNIT(this, wxSize(250, 160)), this);

   // Text control for comment etc.
   myTextCtrl = new ExtTextCtrl(bottomPanel, GRAPHDLG_ID_TEXTCTRL, wxT(""), wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_RICH2 | wxTE_AUTO_URL | wxTE_READONLY | wxBORDER_NONE);
   myTextCtrl->SetBackgroundColour(this->GetBackgroundColour());
   myTextCtrl->SetSize(-1, wxDLG_UNIT_Y(this, 40));
    
   sizerTopPanel->Add(myCanvas, 4, wxGROW | wxALL, 0);
   sizerBottomPanel->Add(myTextCtrl, 1, wxGROW | wxALL, 0);

   // OK button
   wxBoxSizer *sizerConfirm = new wxBoxSizer(wxHORIZONTAL);
   wxButton* buttonOK = new wxButton(this, wxID_OK, _("OK"));
   buttonOK->SetDefault();
   sizerConfirm->Add(buttonOK, 0, wxALL, 5);

   // Hidden Cancel button (only for making Esc close the dialog)
   wxButton* cancel = new wxButton(this, wxID_CANCEL, wxT(""));
   cancel->Hide();

   // FIXME: This ought to go on the top, but putting this button in the sizerCheck sizer makes it invisible :-(
   mySetLimitsButton = new wxButton(this, GRAPHDLG_ID_SETLIMITS, _("Set &Limits..."));
   sizerConfirm->Add(mySetLimitsButton, 0, wxALL, 5);

   // Status bar
   myStatusBar = new wxStatusBar(this, -1);
   int widths[4];
   widths[0] = wxDLG_UNIT_X(this, 60);
   widths[1] = wxDLG_UNIT_X(this, 60);
   widths[2] = wxDLG_UNIT_X(this, 60);
   widths[3] = -1;
   myStatusBar->SetFieldsCount(4, (int*) &widths);

   // Main box with everything in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);
   sizerTop->Add(mySplitter, 2, wxGROW | wxALL, 3);
   sizerTop->Add(sizerConfirm, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 10);
   sizerTop->Add(myStatusBar, 0, wxGROW | wxALL, 0);

   // Overall dialog layout settings
   SetAutoLayout(TRUE);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   GetRegistryData();

   RestoreTortoiseDialogSize(this, "Graph", wxDLG_UNIT(this, wxSize(180, 120)));
   SetTortoiseDialogPos(this, GetRemoteHandle());
   RestoreTortoiseDialogState(this, "Graph");

   topPanel->PushEventHandler(new MouseWheelEventHandler(myCanvas));
}

GraphDialog::~GraphDialog()
{
    SaveRegistryData();
    
    StoreTortoiseDialogSize(this, "Graph");

    delete myTree;
}

bool GraphDialog::ZoomInAllowed()
{
   return myZoom >= MIN_SCALE;
}

bool GraphDialog::ZoomOutAllowed()
{
   int vw, vh, cw, ch;
   myCanvas->GetVirtualSize(&vw, &vh);
   myCanvas->GetClientSize(&cw, &ch);
   return (vw > cw) || (vh > ch);
}

void GraphDialog::DoDraw(const CLogNode*)
{
   CWinLogData::SetShowTags(myShowTagsCheckBox->GetValue());
   myCanvas->CalcImageSize();
}

CLogNode* GraphDialog::GetTree() const
{
    return myTree;
}

void GraphDialog::OnMouseOver(const wxPoint& pos)
{
   if (!GetTree())
      return;
   CWinLogData* root = (CWinLogData*) (GetTree()->GetUserData());
   CWinLogData* data = root->HitTest(pos);
   // Highlighting
   if (data != myOldNode)
   {
      root->UnhighlightAll(myCanvas);
      ClearStatusBar();
      myOldNode = data;
      if (data)
      {
         ShowNodeInfo(data, true);
         if (data != myNodeSelected)
         {
            data->SetHighlighted(myCanvas, true);
         }
      }
      else
         myCanvas->SetToolTip(wxT(""));
   }
}


void GraphDialog::OnTextUrl(wxCommandEvent& cmdevent)
{
   // TODO: Downcast should not be necessary
   wxTextUrlEvent& event = (wxTextUrlEvent&) cmdevent;
   if (!event.GetMouseEvent().ButtonDown(wxMOUSE_BTN_LEFT))
      return;
   std::string url(wxAscii(myTextCtrl->GetValue().c_str()));
   url = url.substr(event.GetURLStart(), event.GetURLEnd() - event.GetURLStart());
   LaunchURL(url);
}


void GraphDialog::ShowRevisionInfo(const CRevFile& rev, bool bShowAsToolTip)
{
   if (bShowAsToolTip)
   {
#if wxUSE_UNICODE
      std::wstringstream ss;
#else
      std::stringstream ss;
#endif
      ss << wxText(rev.RevNum().str()) << wxT(";");
      wxChar buf[100];
      tm_to_local_DateTimeFormatted(&rev.RevTime(), buf, sizeof(buf), false);
      ss << buf << wxT(";") << wxText(rev.Author().c_str()) << wxT(";");
      std::string sComment = Trim(rev.DescLog().c_str());
      FindAndReplace<std::string>(sComment, "\t", " ");
#if 0
      if (sComment.length() > 50)
          sComment = sComment.substr(0, 47) + "...";
#endif

      ss << wxText(sComment);
      
      myCanvas->SetToolTip(ss.str());

      myStatusBar->SetStatusText(wxText(rev.RevNum().str()), 0);
      myStatusBar->SetStatusText(buf, 1);
      myStatusBar->SetStatusText(wxText(rev.Author().c_str()), 2);
      myStatusBar->SetStatusText(wxText(sComment), 3);
   }
   else
   {
      myTextCtrl->Clear();

      // Set tabstop
      PARAFORMAT pf;
      ZeroMemory(&pf, sizeof(pf));
      pf.cbSize = sizeof(pf);
      pf.dwMask = PFM_TABSTOPS;
      pf.cTabCount = 1;
      pf.rgxTabs[0] = TABSTOP;
      ::SendMessage(GetHwndOf(myTextCtrl), EM_SETPARAFORMAT, 0, (LPARAM) &pf);

      wxTextAttr boldattr;
      wxFont deffont;
      deffont.SetWeight(wxNORMAL);
      wxTextAttr defattr;
      defattr.SetFont(deffont);
      wxFont boldfont;
      boldfont.SetWeight(wxBOLD);
      boldattr.SetFont(boldfont);
      myTextCtrl->SetDefaultStyle(boldattr);
      wxString s(_("Revision"));
      s += _(":");
      s += wxT("\t");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      myTextCtrl->AppendText(wxText(rev.RevNum().str()));
      myTextCtrl->SetDefaultStyle(boldattr);
      s = wxT("\n");
      s += _("Date");
      s += _(":");
      s += wxT("\t");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      wxChar buf[100];
      tm_to_local_DateTimeFormatted(&rev.RevTime(), buf, sizeof(buf), false);
      myTextCtrl->AppendText(buf);
      myTextCtrl->SetDefaultStyle(boldattr);
      s = wxT("\n");
      s += _("Author");
      s += _(":");
      s += wxT("\t");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      myTextCtrl->AppendText(wxText(rev.Author().c_str()));
      if (!rev.MergePoint().empty())
      {
         myTextCtrl->SetDefaultStyle(boldattr);
         s = wxT("\n");
         s += _("Mergepoint");
         s += _(":");
         s += wxT("\n");
         myTextCtrl->AppendText(s);
         myTextCtrl->SetDefaultStyle(defattr);
         myTextCtrl->AppendText(wxText(rev.MergePoint().str()));
      }
      if (!rev.BugNumber().empty())
      {
         myTextCtrl->SetDefaultStyle(boldattr);
         s = wxT("\n");
         s += _("Bug Number");
         s += _(":");
         s += wxT("\n");
         myTextCtrl->AppendText(s);
         myTextCtrl->SetDefaultStyle(defattr);
         myTextCtrl->AppendText(wxText(rev.BugNumber().c_str()));
      }
      myTextCtrl->SetDefaultStyle(boldattr);
      s = wxT("\n");
      s += _("Comment");
      s += _(":");
      s += wxT("\n");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      std::string bugtracker = GetStringPreference("Bug tracker URL", GetDirectoryPart(myFilename));
      myTextCtrl->AppendText(wxText(HyperlinkComment(Trim(rev.DescLog().c_str()), bugtracker)));
      myTextCtrl->SetInsertionPoint(0);
   }
}


void GraphDialog::ShowHeaderInfo(const CRcsFile& file, bool bShowAsToolTip)
{
#if wxUSE_UNICODE
    std::wstringstream ss;
#else
    std::stringstream ss;
#endif
   if (bShowAsToolTip)
   {
      std::string sComment = Trim(file.DescLog().c_str());
      FindAndReplace<std::string>(sComment, "\n", " ");
      FindAndReplace<std::string>(sComment, "\t", " ");
      if (sComment.length() > 50)
      {
         sComment = sComment.substr(0, 47) + "...";
      }

      if (!sComment.empty())
      {
         ss << wxText(sComment) << wxT(";");
      }
      ss << wxText(GetFilename()) << wxT(";");
      CVSStatus::FileFormat fmt;
      CVSStatus::FileOptions fo;
      CVSStatus::KeywordOptions ko;
      CVSStatus::ParseOptions(file.KeywordSubst().c_str(), &fmt, &ko, &fo);
      ss << CVSStatus::FileFormatString(fmt)
         << wxT(";") << CVSStatus::KeywordOptionsString(ko)
         << wxT(";") << CVSStatus::FileOptionsString(fo)
         << wxT(";") << file.TotRevisions();
      myCanvas->SetToolTip(ss.str());
      
      myStatusBar->SetStatusText(CVSStatus::FileFormatString(fmt).c_str(), 0);
      myStatusBar->SetStatusText(CVSStatus::KeywordOptionsString(ko).c_str(), 1);
      myStatusBar->SetStatusText(CVSStatus::FileOptionsString(fo).c_str(), 2);
      myStatusBar->SetStatusText(wxText(sComment), 3);
   }
   else
   {
      wxTextAttr boldattr;
      wxFont deffont;
      deffont.SetWeight(wxNORMAL);
      wxTextAttr defattr;
      defattr.SetFont(deffont);
      wxFont boldfont;
      boldfont.SetWeight(wxBOLD);
      boldattr.SetFont(boldfont);
      myTextCtrl->Clear();

      // Set tabstop
      PARAFORMAT pf;
      ZeroMemory(&pf, sizeof(pf));
      pf.cbSize = sizeof(pf);
      pf.dwMask = PFM_TABSTOPS;
      pf.cTabCount = 1;
      pf.rgxTabs[0] = TABSTOP;
      ::SendMessage(GetHwndOf(myTextCtrl), EM_SETPARAFORMAT, 0, (LPARAM) &pf);

      
      CVSStatus::FileFormat fmt;
      CVSStatus::FileOptions fo;
      CVSStatus::KeywordOptions ko;
      CVSStatus::ParseOptions(file.KeywordSubst().c_str(), &fmt, &ko, &fo);

      myTextCtrl->SetDefaultStyle(boldattr);
      wxString s(_("Description"));
      s += _(":");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      myTextCtrl->AppendText(wxText(file.DescLog().c_str()));
      myTextCtrl->SetDefaultStyle(boldattr);
      s = wxT("\n");
      s += _("File Format");
      s += _(":");
      s += wxT("\t");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      myTextCtrl->AppendText(CVSStatus::FileFormatString(fmt).c_str());
      myTextCtrl->SetDefaultStyle(boldattr);
      s = wxT("\n");
      s += _("Keyword substitution");
      s += _(":");
      s += wxT("\t");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      myTextCtrl->AppendText(CVSStatus::KeywordOptionsString(ko).c_str());
      myTextCtrl->SetDefaultStyle(boldattr);
      s = wxT("\n");
      s += _("Storage options");
      s += _(":");
      s += wxT("\t");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      myTextCtrl->AppendText(CVSStatus::FileOptionsString(fo).c_str());
      myTextCtrl->SetDefaultStyle(boldattr);
      s = wxT("\n");
      s += _("Total revisions");
      s += _(":");
      s += wxT("\t");
      myTextCtrl->AppendText(s);
      myTextCtrl->SetDefaultStyle(defattr);
      ss << file.TotRevisions() << std::ends;
      myTextCtrl->AppendText(ss.str());
      myTextCtrl->SetInsertionPoint(0);
   }
}


// Show info for given tree node
void GraphDialog::ShowNodeInfo(CWinLogData* data, bool bShowAsToolTip)
{
   switch (data->GetType())
   {
   case kNodeRev:
   {
      // Show revision info
      CLogNodeRev* revnode = (CLogNodeRev*) (data->Node());
      ShowRevisionInfo(revnode->Rev(), bShowAsToolTip);
      break;
   }

   case kNodeHeader:
   {
      // Show file info
      CLogNodeHeader* headernode = (CLogNodeHeader*) (data->Node());
      ShowHeaderInfo(headernode->RcsFile(), bShowAsToolTip);
      break;
   }

   case kNodeTag:
   {
      CLogNodeTag* tagnode = (CLogNodeTag*) (data->Node());
      const CRevFile *revfile = GetRevFileFromTagNode(tagnode);
      ShowRevisionInfo(*revfile, bShowAsToolTip);

      myOldNode = data;
      break;
   }

   case kNodeBranch:
   {
      break;
   }

   default:
      break;
   }
}



void GraphDialog::OnLeftClick(const wxPoint& pos)
{
   TDEBUG_ENTER("GraphDialog::OnLeftClick");
   CWinLogData* root = (CWinLogData*) (GetTree()->GetUserData());
   CWinLogData* thisnode = root->HitTest(pos);
   TDEBUG_TRACE("thisnode " << ((void*) thisnode));
   if (thisnode && thisnode != myNodeSelected)
   {
      switch (thisnode->GetType())
      {
      case kNodeRev:
      {
         // Unselect other nodes
         root->UnselectAll(myCanvas);
         root->UnhighlightAll(myCanvas);
         // Select this node
         thisnode->SetSelected(myCanvas, true);
         myNodeSelected = thisnode;
         CLogNodeRev* revnode = (CLogNodeRev*) (thisnode->Node());
         const CRevFile& rev = revnode->Rev();
         myRevision2 = rev.RevNum().str();
         ShowNodeInfo(thisnode, false);
         // Force refresh to redraw merge arrows
         myCanvas->Refresh();
         break;
      }

      case kNodeBranch:
      {
          root->UnselectAll(myCanvas);
          CLogNodeBranch* branchNode = static_cast<CLogNodeBranch*>(thisnode->Node());
          root->HighlightBranch(myCanvas, branchNode->Branch().c_str());
          myCanvas->Refresh();
          break;
      }
          
      case kNodeHeader:
      {
         root->UnselectAll(myCanvas);
         root->HighlightTrunk(myCanvas);
         myCanvas->Refresh();
         break;
      }

      default:
         // Unselect all nodes
         root->UnselectAll(myCanvas);
         root->UnhighlightAll(myCanvas);
         myNodeSelected = 0;
         myTextCtrl->Clear();
         break;
      }
   }
   else
   {
      // Unselect all nodes
      root->UnselectAll(myCanvas);
      myNodeSelected = 0;
      myTextCtrl->Clear();
      // Force refresh to redraw merge arrows
      myCanvas->Refresh();
   }
}

void GraphDialog::Update(std::string dir, std::string file)
{
   wxBusyCursor wait;
   int x, y;
   myCanvas->GetViewStart(&x, &y);

   // Delete the old tree
   delete myTree;
   myTree = 0;
   myNodeSelected = 0;
   myOldNode = 0;
   myRevision1.erase();
   myRevision2.erase();
   myParserData.Clear();

   wxString title(Printf(_("Revision graph for %s"), wxTextCStr(myFilename)));
   CvsLogParameters settings(myFilename,
                             false,
                             myCollapseRevisionsCheckBox->GetValue(),
                             myShowTagsCheckBox->GetValue(),
                             myOnlyShownBranch,
                             myHiddenBranches,
                             myStartDate,
                             myEndDate);
   myTree = GetLogGraph(title, this, myFilename.c_str(), myParserData, &settings);
   DoDraw(myTree);
   myCanvas->Scroll(x, y);
   myCanvas->Refresh();
}

// Ensure that the user's current revision is visible in the window
void GraphDialog::ShowDiskNode()
{
   TDEBUG_ENTER("GraphDialog::ShowDiskNode");
   CWinLogData* root = (CWinLogData*) (GetTree()->GetUserData());
   TDEBUG_TRACE("Got root: " << root);
   CWinLogData* diskNode = root->GetDiskNode(myRevNo);

   if (!diskNode)
      return;

   wxRect r = diskNode->SelfBounds();
   int destY = (r.GetTop() + r.GetBottom()) / 2;
   int destX = (r.GetLeft() + r.GetRight()) / 2;

   int x, y;
   myCanvas->GetViewStart(&x, &y);
   x = x + myCanvas->GetSize().GetWidth() / 2;
   y = y + myCanvas->GetSize().GetHeight() / 2;

   myCanvas->Scroll(destX - x, destY - y);
}



void GraphDialog::ClearStatusBar()
{
   int i;
   for (i = 0; i < myStatusBar->GetFieldsCount(); i++)
   {
      myStatusBar->SetStatusText(wxT(""), i);
   }
}



const CRevFile* GraphDialog::GetRevFileFromTagNode(CLogNodeTag *tagnode)
{
   CLogNodeHeader* headernode = (CLogNodeHeader*) (GetTree());
   std::string sTag(tagnode->Tag().c_str());
   const CRcsFile& file = headernode->RcsFile();
   unsigned int i;
   for (i = 0; i < file.SymbolicList().size(); i++)
   {
      if (sTag == file.SymbolicList()[i].Tag().c_str())
         break;
   }

   const CRevNumber& rnum = file.SymbolicList()[i];

   for (i = 0; i < file.AllRevs().size(); i++)
   {
      if (file.AllRevs()[i].RevNum() == rnum)
      {
         break;
      }
   }
   return &(file.AllRevs()[i]);
}



// GraphCanvas

IMPLEMENT_CLASS(GraphCanvas, wxScrolledWindow)

BEGIN_EVENT_TABLE(GraphCanvas, wxScrolledWindow)
  EVT_PAINT(GraphCanvas::OnPaint)
  EVT_ERASE_BACKGROUND(GraphCanvas::OnEraseBackground)
  EVT_MOUSE_EVENTS(GraphCanvas::OnMouseEvent)
  EVT_MENU(GRAPH_CONTEXT_ID_MAKE_TAG,          GraphCanvas::OnMakeTag)
  EVT_MENU(GRAPH_CONTEXT_ID_MAKE_BRANCH,       GraphCanvas::OnMakeBranch)
  EVT_MENU(GRAPH_CONTEXT_ID_DIFF_WORKFILE,     GraphCanvas::OnMenuDiffWorkfile)
  EVT_MENU(GRAPH_CONTEXT_ID_DIFF_SELECTED,     GraphCanvas::OnMenuDiffSelected)
  EVT_MENU(GRAPH_CONTEXT_ID_VIEW,              GraphCanvas::OnMenuView)
  EVT_MENU(GRAPH_CONTEXT_ID_ANNOTATE,          GraphCanvas::OnMenuAnnotate)
  EVT_MENU(GRAPH_CONTEXT_ID_GET,               GraphCanvas::OnMenuGet)
  EVT_MENU(GRAPH_CONTEXT_ID_GET_CLEAN,         GraphCanvas::OnMenuGetClean)
  EVT_MENU(GRAPH_CONTEXT_ID_SAVE_AS,           GraphCanvas::OnMenuSaveAs)
  EVT_MENU(GRAPH_CONTEXT_ID_MERGE,             GraphCanvas::OnMenuMerge)
  EVT_MENU(GRAPH_CONTEXT_ID_UNDO,              GraphCanvas::OnMenuUndo)
  EVT_MENU(GRAPH_CONTEXT_ID_SAVE_GRAPH,        GraphCanvas::OnMenuSaveGraph)
  EVT_MENU(GRAPH_CONTEXT_ID_ZOOM_IN,           GraphCanvas::OnMenuZoomIn)
  EVT_MENU(GRAPH_CONTEXT_ID_ZOOM_OUT,          GraphCanvas::OnMenuZoomOut)
  EVT_MENU(GRAPH_CONTEXT_ID_HIDE,              GraphCanvas::OnMenuHide)
  EVT_MENU(GRAPH_CONTEXT_ID_SHOW_ONLY,         GraphCanvas::OnMenuShowOnly)
  EVT_MENU(GRAPH_CONTEXT_ID_SHOW_ALL,          GraphCanvas::OnMenuShowAll)
END_EVENT_TABLE()

GraphCanvas::GraphCanvas(wxWindow* parent, wxWindowID id,
                         const wxPoint& pos, const wxSize& size,
                         GraphDialog* graphDlg)
    : wxScrolledWindow(parent, id, pos, size, wxBORDER_NONE | wxCLIP_CHILDREN | wxWANTS_CHARS),
      myGraphDlg(graphDlg),
      myInitialized(false)
{
    SetBackgroundColour(* wxWHITE);

    SetCursor(wxCursor(wxCURSOR_ARROW));

    wxAcceleratorEntry entries[2];
    entries[0].Set(wxACCEL_NORMAL, (int) 'i', GRAPH_CONTEXT_ID_ZOOM_IN);
    entries[1].Set(wxACCEL_NORMAL, (int) 'o', GRAPH_CONTEXT_ID_ZOOM_OUT);
    wxAcceleratorTable accel(2, entries);
    SetAcceleratorTable(accel);
}

GraphCanvas::~GraphCanvas()
{
}

void GraphCanvas::OnPaint(wxPaintEvent&)
{
   TDEBUG_ENTER("GraphCanvas::OnPaint");
   if (!myInitialized || !myGraphDlg->GetTree())
      return;

   int w, h;
   GetSize(&w, &h);
   wxBitmap bmp(w, h);
   wxMemoryDC dc;
   dc.SelectObject(bmp);

   wxFont font(*wxSWISS_FONT);
   font.SetPointSize(static_cast<int>(font.GetPointSize() / myGraphDlg->myZoom));
   dc.SetFont(font);

   dc.SetUserScale(myGraphDlg->myZoom, myGraphDlg->myZoom);
   PrepareDC(dc);
   dc.SetBackgroundMode(wxTRANSPARENT);

   std::string filename = myGraphDlg->GetFilename();

   wxRegion rgn = GetUpdateRegion();
   wxPoint size;
   GetViewStart(&size.x, &size.y);
   rgn.Offset(size.x, size.y);

   TDEBUG_USED(wxRect r = rgn.GetBox());
   TDEBUG_TRACE("OnPaint " << r.GetX() << "," << r.GetY() << "," << r.GetWidth() 
                << "," << r.GetHeight());

   // Draw background
   wxBrush brBack(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW), wxSOLID);
   dc.SetBrush(brBack);
   dc.SetPen(*wxTRANSPARENT_PEN);
   dc.DrawRectangle(size.x, size.y, w, h);

   CWinLogData* data = (CWinLogData*) (myGraphDlg->GetTree()->GetUserData());
   data->SetScale(myGraphDlg->myZoom);
   data->Update(dc, rgn, myGraphDlg->myRevNo);

   wxPaintDC pdc(this);
   pdc.Blit(0, 0, w, h, &dc, size.x, size.y);
}

void GraphCanvas::OnMouseEvent(wxMouseEvent& event)
{
   if (event.GetEventType() == wxEVT_MOUSEWHEEL)
   {
      OnMouseWheel(event);
      return;
   }

   wxPoint pos;
   // Add scroll offset
   CalcUnscrolledPosition(event.GetPosition().x, event.GetPosition().y, &pos.x, &pos.y);

   if (event.LeftDown())
   {
      // Deselect other nodes
      myGraphDlg->OnLeftClick(pos);
      SetFocus();
      return;
   }

   if (event.RightDown())
   {
      myGraphDlg->OnMouseOver(pos);
      // Show context menu
      wxMenu* popupMenu = new wxMenu();
      CWinLogData* thisnode = ((CWinLogData*) (myGraphDlg->GetTree()->GetUserData()))->HitTest(pos);
      if (thisnode)
      {
         switch (thisnode->GetType())
         {
            case kNodeRev:
            case kNodeTag:
            {
               // Store file and directory name
               std::string filename = myGraphDlg->GetFilename();
               myFile = reinterpret_cast<CLogNodeHeader*>(myGraphDlg->GetTree())->RcsFile().WorkingFile().c_str();
               myDir = filename.substr(0, filename.length()- myFile.length()- 1);

               if (thisnode->GetType() == kNodeRev)
               {
                  CLogNodeRev* revnode = (CLogNodeRev*) (thisnode->Node());
                  myRevision = reinterpret_cast<CLogNodeRev*>(thisnode->Node())->Rev().RevNum().str();
               }
               else
               {
                  CLogNodeTag* tagnode = (CLogNodeTag*) (thisnode->Node());
                  myRevision = tagnode->Tag().c_str();
                  thisnode = (CWinLogData*) (tagnode->Root()->GetUserData());
               }
               myGraphDlg->SetRevision1(myRevision);


               if (!myGraphDlg->NodeSelected() || thisnode->Selected())
               {
                  // No revisions selected, OR this revision is the selected one
                  popupMenu->Append(GRAPH_CONTEXT_ID_DIFF_WORKFILE, _("&Diff (working file)"), wxT(""));
                  popupMenu->Append(GRAPH_CONTEXT_ID_VIEW, _("&View this revision"), wxT(""));
                  if (!myGraphDlg->myOfferView)
                     popupMenu->Enable(GRAPH_CONTEXT_ID_VIEW, false);
                  popupMenu->Append(GRAPH_CONTEXT_ID_ANNOTATE, _("&Annotate this revision"), wxT(""));
                  popupMenu->Append(GRAPH_CONTEXT_ID_GET, _("&Get this revision (sticky)"), wxT(""));
                  popupMenu->Append(GRAPH_CONTEXT_ID_GET_CLEAN, _("Get &clean copy of this revision (sticky)"), wxT(""));
                  popupMenu->Append(GRAPH_CONTEXT_ID_MAKE_TAG, _("&Tag..."));
                  popupMenu->Append(GRAPH_CONTEXT_ID_MAKE_BRANCH, _("&Branch..."));
                  popupMenu->AppendSeparator();
                  popupMenu->Append(GRAPH_CONTEXT_ID_SAVE_AS, _("&Save this revision as..."), wxT(""));
               }
               else
               {
                  popupMenu->Append(GRAPH_CONTEXT_ID_DIFF_SELECTED, 
                                    _("&Diff (selected revisions)"), wxT(""));
                  popupMenu->Append(GRAPH_CONTEXT_ID_MERGE,
                                    _("&Merge changes (selected revisions)"), wxT(""));
                  popupMenu->Append(GRAPH_CONTEXT_ID_UNDO,
                                    _("&Undo changes (selected revisions)"), wxT(""));
               }

               PopupMenu(popupMenu, event.GetPosition());
            }
            break;

            case kNodeBranch:
            {
               // Store file and directory name
               std::string filename = myGraphDlg->GetFilename();
               myFile = reinterpret_cast<CLogNodeHeader*>(myGraphDlg->GetTree())->RcsFile().WorkingFile().c_str();
               myDir = filename.substr(0, filename.length()- myFile.length()- 1);

               CLogNodeBranch* branchNode = (CLogNodeBranch*)(thisnode->Node());
               std::string branch(branchNode->Branch().c_str());
               myGraphDlg->SetRevision1(branch.c_str());
               popupMenu->Append(GRAPH_CONTEXT_ID_GET, _("&Get this branch (sticky)"), wxT(""));
               popupMenu->Append(GRAPH_CONTEXT_ID_GET_CLEAN, _("Get &clean copy of this branch (sticky)"), wxT(""));
               popupMenu->AppendSeparator();
               popupMenu->Append(GRAPH_CONTEXT_ID_HIDE, _("Always &hide this branch"), wxT(""));
               popupMenu->Append(GRAPH_CONTEXT_ID_SHOW_ONLY, _("&Only show this branch"), wxT(""));
               PopupMenu(popupMenu, event.GetPosition());
            }
            break;

            case kNodeHeader:
            {
               // Store file and directory name
               std::string filename = myGraphDlg->GetFilename();
               myFile = reinterpret_cast<CLogNodeHeader*>(myGraphDlg->GetTree())->RcsFile().WorkingFile().c_str();
               myDir = filename.substr(0, filename.length()- myFile.length()- 1);
               myGraphDlg->SetRevision1("HEAD");
               myRevision = "HEAD";
               popupMenu->Append(GRAPH_CONTEXT_ID_GET, _("&Return to HEAD branch"), wxT(""));
               popupMenu->Append(GRAPH_CONTEXT_ID_GET_CLEAN, _("R&eturn to clean copy of HEAD branch"), wxT(""));
               popupMenu->AppendSeparator();
               popupMenu->Append(GRAPH_CONTEXT_ID_SHOW_ALL, _("Show &all branches"), wxT(""));

               PopupMenu(popupMenu, event.GetPosition());
            }
            break;


         default:
            break;
         }
      }
      else
      {
         // Not over any revision
         popupMenu->Append(GRAPH_CONTEXT_ID_ZOOM_IN, _("Zoom &in (I)"), wxT(""));
         popupMenu->Append(GRAPH_CONTEXT_ID_ZOOM_OUT, _("Zoom &out (O)"), wxT(""));
         popupMenu->AppendSeparator();
         popupMenu->Append(GRAPH_CONTEXT_ID_SAVE_GRAPH, _("&Save graph as bitmap..."), wxT(""));
         
         if (!myGraphDlg->ZoomInAllowed())
            popupMenu->Enable(GRAPH_CONTEXT_ID_ZOOM_IN, false);
         // Do not allow zooming out if entire graph is visible in window
         if (!myGraphDlg->ZoomOutAllowed())
            popupMenu->Enable(GRAPH_CONTEXT_ID_ZOOM_OUT, false);
         
         PopupMenu(popupMenu, event.GetPosition());
      }
      delete popupMenu;
   }
   if (event.Moving())
   {
      myGraphDlg->OnMouseOver(pos);
   }
}


void GraphCanvas::OnMouseWheel(wxMouseEvent& event)
{
   int x, y;
   GetViewStart(&x, &y);
   Scroll(x, y - event.m_wheelRotation / 2);
}


void GraphCanvas::OnMakeTag(wxCommandEvent&)
{
   std::string tagname = TortoiseRegistry::ReadString("FailedTagName");
   bool checkunmodified;
   int action;
   if (DoTagDialog(myGraphDlg, tagname, checkunmodified, action, 
                   std::vector<std::string>(1, myDir)))
   {
      CVSAction glue(myGraphDlg);
      glue.SetProgressFinishedCaption(Printf(_("Finished tagging in %s"),
                                             wxText(myDir).c_str()));

      MakeArgs args;
      args.add_option("tag");

      if (checkunmodified)
         args.add_option("-c");

      // Add the revision to tag
      args.add_option("-r" + myRevision);

      switch (action)
      {
      case BranchTagDlg::TagActionCreate:
         break;
      case BranchTagDlg::TagActionMove:
         args.add_option("-F");
         break;
      case BranchTagDlg::TagActionDelete:
         args.add_option("-d");
         break;
      default:
         TortoiseFatalError(_("Internal error: Invalid action"));
         break;
      }
      args.add_option(tagname);

      args.add_arg(myFile);

      glue.SetProgressCaption(Printf(_("Tagging in %s"), wxText(myDir).c_str()));

      if (!glue.Command(myDir, args))
         TortoiseRegistry::WriteString("FailedTagName", tagname);
      else
         TortoiseRegistry::WriteString("FailedTagName", "");

      myGraphDlg->Update(myDir, myFile);
   }
}


void GraphCanvas::OnMakeBranch(wxCommandEvent&)
{
   std::string branchname;
   bool checkunmodified;
   int action;
   if (DoBranchDialog(myGraphDlg, branchname, checkunmodified, action,
                      std::vector<std::string>(1, myDir)))
   {
      CVSAction glue(myGraphDlg);
      glue.SetProgressFinishedCaption(Printf(_("Finished branching in %s"), wxText(myDir).c_str()));

      MakeArgs args;
      args.add_option("tag");
      if (checkunmodified)
         args.add_option("-c");

      // Add the revision to branch
      args.add_option("-r" + myRevision);

      CVSStatus::CVSVersionInfo serverversion = CVSStatus::GetServerCVSVersion(myDir, &glue);
      switch (action)
      {
      case BranchTagDlg::TagActionCreate:
         args.add_option("-b");
         break;
      case BranchTagDlg::TagActionMove:
         args.add_option("-b");
         args.add_option("-F");
         if (serverversion >= CVSStatus::CVSVersionInfo("1.11.1p2"))
            args.add_option("-B");
         break;
      case BranchTagDlg::TagActionDelete:
         args.add_option("-d");
         if (serverversion >= CVSStatus::CVSVersionInfo("1.11.1p2"))
            args.add_option("-B");
         break;
      default:
         TortoiseFatalError(_("Internal error: Invalid action"));
         break;
      }

      args.add_option(branchname);

      args.add_arg(myFile);

      glue.SetProgressCaption(Printf(_("Branching in %s"), wxText(myDir).c_str()));
      glue.Command(myDir, args);

      myGraphDlg->Update(myDir, myFile);
   }
}


void GraphCanvas::OnMenuDiffWorkfile(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsDiffVerb, myGraphDlg->GetFilename(), 
                     "-r " + Quote(myGraphDlg->myRevision1), GetHwnd()); 
}


void GraphCanvas::OnMenuDiffSelected(wxCommandEvent&)
{
   std::string rev = "-r ";
   rev += Quote(myGraphDlg->myRevision1);
   if (!myGraphDlg->myRevision2.empty())
   {
      // Pass revisions in the right order
      if (CVSRevisionDataSortCriteria::StaticCompareRevisions(myGraphDlg->myRevision1, 
         myGraphDlg->myRevision2, true) < 0)
      {
         rev += " -r " + Quote(myGraphDlg->myRevision2);
      }
      else
      {
         rev = "-r " + Quote(myGraphDlg->myRevision2) + " " + rev;
      }
   }
   LaunchTortoiseAct(false, CvsDiffVerb, myGraphDlg->myFilename, rev, GetHwnd()); 
}


void GraphCanvas::OnMenuMerge(wxCommandEvent&)
{
   std::string rev = "-r ";
   rev += Quote(myGraphDlg->myRevision1);
   if (!myGraphDlg->myRevision2.empty())
   {
      // Pass revisions in the right order
      if (CVSRevisionDataSortCriteria::StaticCompareRevisions(myGraphDlg->myRevision1, 
         myGraphDlg->myRevision2, true) < 0)
      {
         rev += " -r " + Quote(myGraphDlg->myRevision2);
      }
      else
      {
         rev = "-r " + Quote(myGraphDlg->myRevision2) + " " + rev;
      }
   }
   LaunchTortoiseAct(false, CvsMergeVerb, myGraphDlg->myFilename, rev, GetHwnd()); 
}


void GraphCanvas::OnMenuUndo(wxCommandEvent&)
{
   std::string rev = "-r ";
   rev += Quote(myGraphDlg->myRevision1);
   if (!myGraphDlg->myRevision2.empty())
   {
      // Pass revisions in the right order
      if (CVSRevisionDataSortCriteria::StaticCompareRevisions(myGraphDlg->myRevision1, 
         myGraphDlg->myRevision2, true) < 0)
      {
         rev = "-r " + Quote(myGraphDlg->myRevision2) + " " + rev;
      }
      else
      {
         rev += " -r " + Quote(myGraphDlg->myRevision2);
      }
   }
   LaunchTortoiseAct(false, CvsMergeVerb, myGraphDlg->myFilename, rev, GetHwnd()); 
}


void GraphCanvas::OnMenuView(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsViewVerb, myGraphDlg->GetFilename(), 
                     "-r " + Quote(myGraphDlg->myRevision1), GetHwnd()); 
}


void GraphCanvas::OnMenuGet(wxCommandEvent&)
{
   LaunchTortoiseAct(true, CvsGetVerb, myGraphDlg->myFilename, 
                     "-r " + Quote(myGraphDlg->myRevision1), GetHwnd()); 
   CVSStatus::InvalidateCache();
   myGraphDlg->myRevNo = CVSStatus::GetRevisionNumber(myGraphDlg->myFilename);
   myGraphDlg->Refresh();
}


void GraphCanvas::OnMenuGetClean(wxCommandEvent&)
{
   int id = MessageBox(GetHwndOf(myGraphDlg), 
                       _("Retrieving a clean copy deletes your changes."), _("Warning"),
                       MB_OKCANCEL | MB_ICONWARNING);
   if (id != IDOK)
      return;

   LaunchTortoiseAct(true, CvsGetVerb, myGraphDlg->myFilename, 
                     "-C -r " + Quote(myGraphDlg->myRevision1), GetHwnd()); 
   CVSStatus::InvalidateCache();
   myGraphDlg->myRevNo = CVSStatus::GetRevisionNumber(myGraphDlg->myFilename);
   myGraphDlg->Refresh();
}

void GraphCanvas::OnMenuSaveAs(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsSaveAsVerb, myGraphDlg->myFilename, 
                     "-r " + Quote(myGraphDlg->myRevision1), GetHwnd()); 
}

void GraphCanvas::OnMenuAnnotate(wxCommandEvent&)
{
   LaunchTortoiseAct(false, CvsAnnotateVerb, myGraphDlg->myFilename, 
                     "-r " + Quote(myGraphDlg->myRevision1), GetHwnd()); 
}

void GraphCanvas::OnMenuSaveGraph(wxCommandEvent&)
{
   TDEBUG_ENTER("OnMenuSaveGraph");
   std::string filename = GetNameBody(myGraphDlg->myFilename) + ".bmp";
   wxString s(_("Bitmap files"));
   s += wxT("|*.bmp");
   filename = DoSaveFileDialog(this, _("Save Revision Graph"), filename, s);                               
   if (!filename.empty())
   {
      // Create a device context for drawing into
      wxMemoryDC dc;
      CWinLogData* root = (CWinLogData*) (myGraphDlg->GetTree()->GetUserData());
      root->ComputeBounds(wxPoint(0, 0), dc);
      wxRect bounds = root->Bounds();
      // Allow for shadows
      bounds.Inflate(CWinLogData::SHADOW_OFFSET);
      // Allow for merge arrows
      bounds.Inflate(CWinLogData::ARROWSIZE);
      // Create bitmap of appropriate size
      wxBitmap bmp(bounds.width, bounds.height);
      // Select the bitmap into the device context
      dc.SelectObject(bmp);
      dc.SetBackgroundMode(wxTRANSPARENT);
      dc.SetFont(*wxSWISS_FONT);
      // Draw background
      wxBrush brBack(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW), wxSOLID);
      dc.SetBrush(brBack);
      dc.SetPen(*wxTRANSPARENT_PEN);
      dc.DrawRectangle(0, 0, bounds.width, bounds.height);
      // Draw nodes
      wxRegion region(bounds);
      root->Update(dc, region, myGraphDlg->myRevNo);
      // Save bitmap
      if (!bmp.SaveFile(wxText(filename), wxBITMAP_TYPE_BMP))
         DoMessageDialog(this, _("Failed to save Revision Graph bitmap"));
   }
}

void GraphCanvas::OnMenuShowOnly(wxCommandEvent&)
{
   TDEBUG_ENTER("OnMenuShowOnly");
   TDEBUG_TRACE("Show only branch '" << myGraphDlg->myRevision1 << "'");
   myGraphDlg->myOnlyShownBranch = myGraphDlg->myRevision1;
   TortoiseRegistry::WriteString("Dialogs\\Graph\\Branches\\Show", myGraphDlg->myRevision1);
   myGraphDlg->Refresh(true);
}

void GraphCanvas::OnMenuShowAll(wxCommandEvent&)
{
   myGraphDlg->myOnlyShownBranch = "";
   TortoiseRegistry::WriteString("Dialogs\\Graph\\Branches\\Show", "");
   myGraphDlg->myHiddenBranches.clear();
   TortoiseRegistry::WriteVector("Dialogs\\Graph\\Branches\\Hide", myGraphDlg->myHiddenBranches);
   myGraphDlg->Refresh(true);
}

void GraphCanvas::OnMenuHide(wxCommandEvent&)
{
   TDEBUG_ENTER("OnMenuHide");
   TDEBUG_TRACE("Hide branch '" << myGraphDlg->myRevision1 << "'");
   myGraphDlg->myHiddenBranches.push_back(myGraphDlg->myRevision1);
   TortoiseRegistry::WriteVector("Dialogs\\Graph\\Branches\\Hide", myGraphDlg->myHiddenBranches);
   myGraphDlg->Refresh(true);
}

void GraphCanvas::OnMenuZoomIn(wxCommandEvent&)
{
   if (myGraphDlg->ZoomInAllowed())
   {
      myGraphDlg->myZoom /= SCALE_MULTIPLIER;
      myGraphDlg->Refresh();
   }
}

void GraphCanvas::OnMenuZoomOut(wxCommandEvent&)
{
   if (myGraphDlg->ZoomOutAllowed())
   {
      myGraphDlg->myZoom *= SCALE_MULTIPLIER;
      myGraphDlg->Refresh();
   }
}

void GraphCanvas::CalcImageSize()
{
   const wxPoint startPoint(STARTPOINT);
   wxClientDC dc(this);
   dc.SetUserScale(myGraphDlg->myZoom, myGraphDlg->myZoom);
   CLogNode* root = myGraphDlg->GetTree();
   if (!root)
      return;

   // set all the bounds locations
   CWinLogData* data = CWinLogData::CreateNewData(root);
   root->SetUserData(data);
   data->ComputeBounds(startPoint, dc);
    
   // Reoffset from (0, 0)
   wxRect bounds = data->Bounds();
   wxPoint offset(startPoint.x - bounds.x,
                  startPoint.y - bounds.y);
   data->Offset(offset);
   bounds = data->Bounds();

   // Set the scroll size, we add the margin of the start point offset on each side
   wxSize size = bounds.GetSize() + wxSize(2*startPoint.x, 2*startPoint.y) +
      wxSize(CWinLogData::SHADOW_OFFSET, CWinLogData::SHADOW_OFFSET);
   SetScrollbars(1, 1, size.x, size.y);

   myInitialized = true;
}


void GraphCanvas::OnEraseBackground(wxEraseEvent&)
{
}

void GraphDialog::SymbolicCheckChanged(wxCommandEvent&)
{
   Refresh(true);
}

void GraphDialog::CollapseCheckChanged(wxCommandEvent&)
{
   Refresh(true);
}

void GraphDialog::SetLimits(wxCommandEvent&)
{
   if (DoGraphSettingsDialog(this, myStartDate, myEndDate))
   {
      Refresh(true);
   }
}

void GraphDialog::Refresh(bool rebuildTree)
{
   wxBusyCursor wait;
   if (rebuildTree)
   {
      delete myTree;
      CvsLogParameters settings(myFilename,
                                false,
                                myCollapseRevisionsCheckBox->GetValue(),
                                myShowTagsCheckBox->GetValue(),
                                myOnlyShownBranch,
                                myHiddenBranches,
                                myStartDate,
                                myEndDate);
      myTree = CvsLogGraph(myParserData, &settings);
   }
   int x, y;
   myCanvas->GetViewStart(&x, &y);
   
   myNodeSelected = 0;
   myOldNode = 0;
   myRevision1.erase();
   myRevision2.erase();

   DoDraw(myTree);
   myCanvas->Scroll(x, y);
   myCanvas->Refresh();
}

void GraphDialog::GetRegistryData()
{
   int pos = 80;
   TortoiseRegistry::ReadInteger("Dialogs\\Graph\\Sash Position", pos);
   mySplitter->SetRightSashPosition(wxDLG_UNIT_Y(this, pos));

   bool showTags = TortoiseRegistry::ReadBoolean("Dialogs\\Graph\\Show Tags", true);
   myShowTagsCheckBox->SetValue(showTags);
   bool collapse = TortoiseRegistry::ReadBoolean("Dialogs\\Graph\\Collapse Revisions", false);
   myCollapseRevisionsCheckBox->SetValue(collapse);

   TortoiseRegistry::ReadVector("Dialogs\\Graph\\Branches\\Hide", myHiddenBranches);
   myOnlyShownBranch = TortoiseRegistry::ReadString("Dialogs\\Graph\\Branches\\Show");
}

void GraphDialog::SaveRegistryData()
{
   int h = mySplitter->GetRightSashPosition();
   h = this->ConvertPixelsToDialog(wxSize(0, h)).GetHeight();
   TortoiseRegistry::WriteInteger("Dialogs\\Graph\\Sash Position", h);

   bool showTags = myShowTagsCheckBox->GetValue();
   TortoiseRegistry::WriteBoolean("Dialogs\\Graph\\Show Tags", showTags);
   bool collapse = myCollapseRevisionsCheckBox->GetValue();
   TortoiseRegistry::WriteBoolean("Dialogs\\Graph\\Collapse Revisions", collapse);
}
