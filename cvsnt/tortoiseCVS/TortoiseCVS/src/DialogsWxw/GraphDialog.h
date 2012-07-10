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

#ifndef GRAPH_DIALOG_H
#define GRAPH_DIALOG_H

#include <string>
#include <vector>

#include <wx/wx.h>
#include <windows.h>
#include "ExtDialog.h"
#include "ExtTextCtrl.h"
#include "ExtSplitterWindow.h"
#include "../cvstree/common.h"


class CVSRevisionData;
class GraphDialog;

class CLogNode;
class CLogNodeRev;
class CLogNodeHeader;
class CWinLogData;

class GraphDialog : public ExtDialog
{
public:
   friend bool DoGraphDialog(wxWindow* parent, const std::string& filename);
   friend class GraphCanvas;
    
   CLogNode* GetTree() const;
   const std::string& GetFilename() const   { return myFilename; }

   void OnMouseOver(const wxPoint& pos);
   void OnLeftClick(const wxPoint& pos);
    void OnTextUrl(wxCommandEvent& event);

   CWinLogData* NodeSelected() const                    { return myNodeSelected; }

   void SetRevision1(const std::string& rev)    { myRevision1 = rev; }
      
   void Update(std::string dir, std::string file);
   const CRevFile* GetRevFileFromTagNode(CLogNodeTag *tagnode);

private:
   GraphDialog(wxWindow* parent, const std::string& filename);
   ~GraphDialog();

   void DoDraw(const CLogNode* node);
   void ShowRevisionInfo(const CRevFile& rev, bool bShowAsToolTip);
   void ShowHeaderInfo(const CRcsFile& file, bool bShowAsToolTip);
   // Show info for given tree node
   void ShowNodeInfo(CWinLogData* data, bool bShowAsToolTip);
   void ShowDiskNode();
   void ClearStatusBar();
   void SymbolicCheckChanged(wxCommandEvent& e);
   void CollapseCheckChanged(wxCommandEvent& e);
   void SetLimits(wxCommandEvent& e);

   bool ZoomInAllowed();
   bool ZoomOutAllowed();
   
   void Refresh(bool rebuildTree = false);
   
   void GetRegistryData();
   void SaveRegistryData();

   // Controls
   
   GraphCanvas*                myCanvas;
   ExtTextCtrl*                myTextCtrl;
   wxStaticText*               myCommentLabel;
   ExtSplitterWindow*          mySplitter;
   wxStatusBar*                myStatusBar;
   wxCheckBox*                 myShowTagsCheckBox;
   wxCheckBox*                 myCollapseRevisionsCheckBox;
   wxButton*                   mySetLimitsButton;
   ParserData                  myParserData;

   // Tree information
   std::string                 myFilename;
   bool                        myOfferView;
   CLogNode*                   myTree;
   CWinLogData*                myNodeSelected;
   CWinLogData*                myOldNode;
   std::string                 myRevision1;
   std::string                 myRevision2;
   std::string                 myRevNo;
   std::vector<std::string>    myHiddenBranches;
   std::string                 myOnlyShownBranch;
   double                      myZoom;
   std::string                 myStartDate;
   std::string                 myEndDate;

   DECLARE_EVENT_TABLE()
};

#endif //GRAPH_DIALOG_H
