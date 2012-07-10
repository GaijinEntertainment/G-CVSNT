// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Torsten Martinsen
// <torsten@tiscali.dk> - September 2004

// Originally based on WinCVS MFC code

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 1
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "StdAfx.h"
#include "GraphLogData.h"

#include <algorithm>
#include <memory>
#include <memory.h>
#include <math.h>

#include "../CVSGlue/CVSStatus.h"
#include "../Utils/TortoiseDebug.h"

#define RGB_DEF_SEL             160, 160, 160
#define RGB_DEF_HIGHLIGHT       255, 255, 128
#define RGB_DEF_SHADOW          128, 128, 128
#define RGB_DEF_HEADER          255, 0, 0
#define RGB_DEF_TAG             0, 0, 0
#define RGB_DEF_BRANCH          0, 0, 255
#define RGB_DEF_NODE            0, 0, 255
#define RGB_DEF_DISKNODE        64, 255, 64
#define RGB_DEF_DEADNODE        255, 0, 0
#define RGB_DEF_USERSTATENODE   0, 128, 64
#define RGB_DEF_MERGE           255, 0, 255

std::string REV_NO(const CLogNode* n)
{
   switch (n->GetType())
   {
   case kNodeRev:
      return ((CLogNodeRev*) n)->Rev().RevNum().str();

   case kNodeBranch:
      return "[branch]";

   default:
      return "[?]";
   }
}

const static int STYLE_NORMALNODE = PS_SOLID;
const static int STYLE_DEADNODE = PS_DOT;
const static int STYLE_USERSTATENODE = PS_DASH;

const static int ROUND_RECT_RADIUS = 3;
const static int SANDBOX_NODE_PENWIDTH = 3;


int             CWinLogData::myRefCount = 0;
wxColour*       CWinLogData::myGraphSelColor = 0;
wxColour*       CWinLogData::myGraphHighlightColor = 0;
wxColour*       CWinLogData::myGraphShadowColor = 0;
wxColour*       CWinLogData::myGraphHeaderColor = 0;
wxColour*       CWinLogData::myGraphTagColor = 0;
wxColour*       CWinLogData::myGraphBranchColor = 0;
wxColour*       CWinLogData::myGraphNodeColor = 0;
wxColour*       CWinLogData::myGraphDiskNodeColor = 0;
wxColour*       CWinLogData::myGraphDeadNodeColor = 0;
wxColour*       CWinLogData::myGraphUserStateNodeColor = 0;
wxColour*       CWinLogData::myGraphMergeColor = 0;
bool            CWinLogData::ourShowTags = true;
double          CWinLogData::ourScale = 1.0;
CWinLogData::MergePairVector CWinLogData::ourMergePairs;
std::string     CWinLogData::ourSandboxRevNo;

void CWinLogData::SetShowTags(bool showtags)
{
   ourShowTags = showtags;
}

void CWinLogData::SetScale(double scale)
{
   ourScale = scale;
}

CWinLogData::CWinLogData(CLogNode* node)
   : CLogNodeData(node),
     mySelfBounds(myEmptyrect),
     myTotalBounds(myEmptyrect),
     myIsSelected(false),
     myIsHighlighted(false),
     myIsEmphasized(false)
{
   if (!myRefCount++)
      InitializeStatics();
}


CWinLogData::~CWinLogData()
{
   if (!--myRefCount)
      CleanupStatics();
}

void CWinLogData::InitializeStatics()
{
   myGraphSelColor = new wxColour(RGB_DEF_SEL);
   myGraphHighlightColor = new wxColour(RGB_DEF_HIGHLIGHT);
   myGraphShadowColor = new wxColour(RGB_DEF_SHADOW);
   myGraphHeaderColor = new wxColour(RGB_DEF_HEADER);
   myGraphTagColor = new wxColour(RGB_DEF_TAG);
   myGraphBranchColor = new wxColour(RGB_DEF_BRANCH);
   myGraphNodeColor = new wxColour(RGB_DEF_NODE);
   myGraphDiskNodeColor = new wxColour(RGB_DEF_DISKNODE);
   myGraphDeadNodeColor = new wxColour(RGB_DEF_DEADNODE);
   myGraphUserStateNodeColor = new wxColour(RGB_DEF_USERSTATENODE);
   myGraphMergeColor = new wxColour(RGB_DEF_MERGE);
}

void CWinLogData::CleanupStatics()
{
   delete myGraphSelColor;
   delete myGraphHighlightColor;
   delete myGraphShadowColor;
   delete myGraphHeaderColor;
   delete myGraphTagColor;
   delete myGraphBranchColor;
   delete myGraphNodeColor;
   delete myGraphDiskNodeColor;
   delete myGraphDeadNodeColor;
   delete myGraphUserStateNodeColor;
   delete myGraphMergeColor;
}

void CWinLogData::DrawNode(wxDC& dc,
                           const wxColour& contour, 
                           NodeShape shape,
                           int penStyle,
                           int penWidth)
{
   if (ourScale <= 1.0)
   {
      // prepare the shadow
      wxBrush shadowbrush(*myGraphShadowColor, wxSOLID);
      wxPen shadowpen(*myGraphShadowColor, 1, penStyle);

      // Draw the shadow
      wxRect shadow = mySelfBounds;
      shadow.Offset(wxPoint(SHADOW_OFFSET, SHADOW_OFFSET));

      dc.SetBrush(shadowbrush);
      dc.SetPen(shadowpen);

      switch (shape)
      {
      case kRectangle:
         dc.DrawRectangle(shadow);
         break;
      case kRoundRect:
         dc.DrawRoundedRectangle(shadow, ROUND_RECT_RADIUS);
         break;
      case kOctangle:
         DrawOctangle(dc, shadow);
         break;
      default:
         ASSERT(FALSE); // unknown type
         return;
      }
   }
   
   // Prepare selection
   wxBrush selBrush(*myGraphSelColor, wxSOLID);
   wxBrush highlightBrush(*myGraphHighlightColor, wxSOLID);
   if (myIsSelected)
      dc.SetBrush(selBrush);
   else if (myIsHighlighted)
      dc.SetBrush(highlightBrush);
   else
      dc.SetBrush(wxNullBrush);

   wxPen mpen(contour, penWidth, penStyle);
   dc.SetPen(mpen);

   // Draw the main shape
   switch (shape)
   {
   case kRectangle:
      dc.DrawRectangle(mySelfBounds);
      break;
   case kRoundRect:
      dc.DrawRoundedRectangle(mySelfBounds, ROUND_RECT_RADIUS);
      break;
   case kOctangle:
      DrawOctangle(dc, mySelfBounds);
      break;
   default:
      ASSERT(FALSE); // unknown type
      return;
   }

   // cleanup
   dc.SetPen(wxNullPen);
   if (myIsSelected)
      dc.SetBrush(wxNullBrush);
}


// Helper function to draw the figure used for branch tag representation
void CWinLogData::DrawOctangle(wxDC& dc, const wxRect& selfB)
{
   int cutLen = selfB.height / 4;
   wxPoint point1(selfB.GetLeft(), selfB.GetTop() + cutLen);
   wxPoint point2(selfB.GetLeft() + cutLen, selfB.GetTop());
   wxPoint point3(selfB.GetRight() - cutLen, selfB.GetTop());
   wxPoint point4(selfB.GetRight(), selfB.GetTop() + cutLen);
   wxPoint point5(selfB.GetRight(), selfB.GetBottom() - cutLen);
   wxPoint point6(selfB.GetRight() - cutLen, selfB.GetBottom());
   wxPoint point7(selfB.GetLeft() + cutLen, selfB.GetBottom());
   wxPoint point8(selfB.GetLeft(), selfB.GetBottom() - cutLen);
   wxPoint arrPoints[] =
   {
      point1,
      point2,
      point3,
      point4,
      point5,
      point6,
      point7,
      point8
   };

   dc.DrawPolygon(sizeof(arrPoints)/sizeof(arrPoints[0]), arrPoints);
}


static wxPoint CenterPoint(const wxRect& rect)
{
   wxPoint center(rect.x+rect.width/2, rect.y+rect.height/2);
   return center;
}

static wxRect Union(const wxRect& r1, const wxRect& r2)
{
   wxRegion reg(r1);
   reg.Union(r2);
   return reg.GetBox();
}

static void DrawCenteredClippedText(wxDC& dc, wxCoord x, wxCoord y, const wxString& text, const wxRect& boundingbox)
{
   dc.SetClippingRegion(boundingbox);
   wxCoord w, h;
   dc.GetTextExtent(text, &w, &h);
   dc.DrawText(text, x - w/2, y);
   dc.DestroyClippingRegion();
}

wxSize CWinLogHeaderData::GetBoundsSelf(wxDC& dc)
{
   CLogNodeHeader& header = *(CLogNodeHeader*) fNode;
   wxSize size;
   dc.GetTextExtent(wxText(header.RcsFile().WorkingFile().c_str()), &size.x, &size.y);
   return size;
}

void CWinLogHeaderData::UpdateSelf(wxDC& dc)
{
   // draw the node
   DrawNode(dc, *myGraphHeaderColor, kRoundRect);

   // draw the content of the node
   wxPoint center = CenterPoint(mySelfBounds);
   int nCenterX = center.x;
   int nCenterY = center.y - dc.GetCharHeight()/2;
   CLogNodeHeader& header = *(CLogNodeHeader*) fNode;
   DrawCenteredClippedText(dc, nCenterX, nCenterY, wxText(header.RcsFile().WorkingFile().c_str()), mySelfBounds);
}

wxSize CWinLogRevData::GetBoundsSelf(wxDC& dc)
{
   CLogNodeRev& rev = *(CLogNodeRev*) fNode;
   wxSize size;
   dc.GetTextExtent(wxText(rev.Rev().RevNum().str()), &size.x, &size.y);
   return size;
}

void CWinLogRevData::UpdateSelf(wxDC& dc)
{
   CLogNodeRev& rev = *(CLogNodeRev*) fNode;

   if (rev.GetThisDeleted())
      return;

   wxRect txtRect = mySelfBounds;

   // draw the node
   wxColour* contour = myGraphNodeColor;
   int penStyle = STYLE_NORMALNODE;

   int penWidth = 1;
   if (IsDiskNode(ourSandboxRevNo))
   {
      penWidth = SANDBOX_NODE_PENWIDTH;
      contour = myGraphDiskNodeColor;
   }
   if (IsDeadNode())
   {
      contour = myGraphDeadNodeColor;
      penStyle = STYLE_DEADNODE;
   }
   else if (IsUserState())
   {
      contour = myGraphUserStateNodeColor;
      penStyle = STYLE_USERSTATENODE;
   }

   DrawNode(dc, *contour, kRectangle, penStyle, penWidth/ourScale);

   // Draw the content of the node
   wxPoint center = CenterPoint(mySelfBounds);
   int nCenterX = center.x;
   int nCenterY = center.y - dc.GetCharHeight()/2;
   DrawCenteredClippedText(dc, nCenterX, nCenterY, wxText(rev.Rev().RevNum().str()), txtRect);
}

wxSize CWinLogTagData::GetBoundsSelf(wxDC& dc)
{
   wxSize size;
   CLogNodeTag* tagNode = static_cast<CLogNodeTag*>(fNode);
   if (ourShowTags)
      dc.GetTextExtent(wxText(tagNode->Tag().c_str()), &size.x, &size.y);
   return size;
}

void CWinLogTagData::UpdateSelf(wxDC& dc)
{
   if (!ourShowTags)
      return;
   
   // draw the node
   DrawNode(dc, *myGraphTagColor, kRoundRect);

   wxPoint center = CenterPoint(mySelfBounds);
   int nCenterX = center.x;
   int nCenterY = center.y - dc.GetCharHeight()/2;
   CLogNodeTag* tagNode = static_cast<CLogNodeTag*>(fNode);
   DrawCenteredClippedText(dc, nCenterX, nCenterY, wxText(tagNode->Tag().c_str()), mySelfBounds);
}

wxSize CWinLogBranchData::GetBoundsSelf(wxDC& dc)
{
   CLogNodeBranch& branch = *(CLogNodeBranch*) fNode;
   wxSize size;
   dc.GetTextExtent(wxText(branch.Branch().c_str()), &size.x, &size.y);
   return size;
}

void CWinLogBranchData::UpdateSelf(wxDC& dc)
{
    // draw the node
    DrawNode(dc, *myGraphBranchColor, kOctangle, wxSOLID, myIsEmphasized ? 3 : 1);

   // Draw the content of the node
   wxPoint center = CenterPoint(mySelfBounds);
   int nCenterX = center.x;
   int nCenterY = center.y - dc.GetCharHeight()/2;
   CLogNodeBranch& branch = *(CLogNodeBranch*) fNode;
   DrawCenteredClippedText(dc, nCenterX, nCenterY, wxText(branch.Branch().c_str()), mySelfBounds);
}

std::string CWinLogData::GetStr()
{
   switch (GetType())
   {
      case kNodeHeader:
      {
         CLogNodeHeader& header = *(CLogNodeHeader*) fNode;
         return header.RcsFile().WorkingFile().c_str();
         break;
      }
      case kNodeBranch:
      {
         CLogNodeBranch& branch = *(CLogNodeBranch*) fNode;
         return branch.Branch().c_str();
         break;
      }
      case kNodeRev:
      {
         CLogNodeRev& rev = *(CLogNodeRev*) fNode;
         return rev.Rev().RevNum().str();
         break;
      }
      case kNodeTag:
      {
         CLogNodeTag* tagNode = static_cast<CLogNodeTag*>(fNode);
         return tagNode->Tag().c_str();
         break;
      }
   }
   return 0;
}

CWinLogData* CWinLogData::CreateNewData(CLogNode* node)
{
   if (node->GetUserData())
      return (CWinLogData*) node->GetUserData();

   CWinLogData* res = 0;
   switch (node->GetType())
   {
   case kNodeHeader:
      res = new CWinLogHeaderData(node);
      break;
   case kNodeBranch:
      res = new CWinLogBranchData(node);
      break;
   case kNodeRev:
      res = new CWinLogRevData(node);
      break;
   case kNodeTag:
      res = new CWinLogTagData(node);
      break;
   }
   node->SetUserData(res);
   return res;
}


bool CWinLogData::IsDiskNode(const std::string &revNo)
{
   if (GetType() != kNodeRev)
      return false;
   if (revNo.empty())
      return false;

   return strcmp(revNo.c_str(), ((CLogNodeRev*) fNode)->Rev().RevNum().str().c_str()) == 0;
}


bool CWinLogData::IsDeadNode()
{
   if (GetType() != kNodeRev)
      return false;
   
   return strcmp("dead", ((CLogNodeRev*) fNode)->Rev().State().c_str()) == 0;
}

bool CWinLogData::IsUserState()
{
   if (GetType() != kNodeRev)
      return false;
   
   return !(strcmp("dead", ((CLogNodeRev*) fNode)->Rev().State().c_str()) == 0 || 
      strcmp("Exp", ((CLogNodeRev*) fNode)->Rev().State().c_str()) == 0);
}

// Vertical space between child nodes
const int VChildSpace = 12;
// Horizontal space between child nodes
const int HChildSpace = 40;
// How much room is around the revision number, i.e. the size of the box
const int InflateNodeSpace = 8;

void CWinLogData::Offset(const wxPoint& o)
{
    mySelfBounds.Offset(o);
    myTotalBounds.Offset(o);
    std::vector<CLogNode*>::iterator i;
    for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
    {
        CLogNode* subNode = *i;
        GetData(subNode)->Offset(o);
    }
    if (fNode->Next())
        GetData(fNode->Next())->Offset(o);
}

void CWinLogData::SetSelected(wxScrolledWindow* view, bool state)
{
   TDEBUG_ENTER("CWinLogData::SetSelected");
   myIsSelected = state;

   wxRect invalR = mySelfBounds;
   wxPoint size;
   view->GetViewStart(&size.x, &size.y);
   size.x = -size.x;
   size.y = -size.y;
   invalR.Offset(size);
   view->Refresh(false, &invalR);
   //TDEBUG_TRACE("SetSelected " << invalR.GetX() << "," << invalR.GetY() << "," << invalR.GetWidth() 
   //   << "," << invalR.GetHeight());
}

void CWinLogData::SetHighlighted(wxScrolledWindow* view, bool state)
{
   myIsHighlighted = state;

   wxRect invalR = mySelfBounds;
   wxPoint size;
   view->GetViewStart(&size.x, &size.y);
   size.x = -size.x;
   size.y = -size.y;
   invalR.Offset(size);
   view->Refresh(false, &invalR);
}

void CWinLogData::SetEmphasized(wxScrolledWindow* view, bool state)
{
   TDEBUG("this: " << this << " emph: " << state);
   myIsEmphasized = state;

   wxRect invalR = mySelfBounds;
   wxPoint size;
   view->GetViewStart(&size.x, &size.y);
   size.x = -size.x;
   size.y = -size.y;
   invalR.Offset(size);
   view->Refresh(false, &invalR);
}

void CWinLogData::UnselectAll(wxScrolledWindow* view)
{
   if (myIsSelected)
   {
      SetSelected(view, false);
   }

   std::vector<CLogNode*>::iterator i;
   for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
   {
      CLogNode* subNode = *i;
      GetData(subNode)->UnselectAll(view);
   }
   if (fNode->Next())
      GetData(fNode->Next())->UnselectAll(view);
}

void CWinLogData::UnhighlightAll(wxScrolledWindow* view)
{
   if (myIsHighlighted)
   {
      SetHighlighted(view, false);
   }
   
   for (std::vector<CLogNode*>::iterator i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
   {
      CLogNode* subNode = *i;
      GetData(subNode)->UnhighlightAll(view);
   }
   if (fNode->Next())
      GetData(fNode->Next())->UnhighlightAll(view);
}

void CWinLogData::HighlightBranch(wxScrolledWindow* view, const std::string& branch)
{
   const CLogNode* branchNode = fNode->FindBranchNode(branch, Node());
   TDEBUG("Parent: " << REV_NO(branchNode));
   HighlightBranch(view, branchNode);
}

void CWinLogData::HighlightBranch(wxScrolledWindow* view, const CLogNode* branchParent)
{
   bool foundParent = (Node() == branchParent);
   SetEmphasized(view, foundParent);
   if (foundParent)
   {
      CLogNode* next = fNode->Next();
      while (next)
      {
         TDEBUG("Set emph for " << REV_NO(next));
         static_cast<CWinLogData*>(next->GetUserData())->SetEmphasized(view, true);
         next = next->Next();
      }
      return;
   }

   for (std::vector<CLogNode*>::iterator i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
   {
      CLogNode* subNode = *i;
      TDEBUG("Kid: " << REV_NO(subNode));
      GetData(subNode)->HighlightBranch(view, branchParent);
   }
   if (fNode->Next())
   {
      GetData(fNode->Next())->HighlightBranch(view, branchParent);
   }
}

void CWinLogData::HighlightTrunk(wxScrolledWindow* view)
{
    CLogNode* next = *(fNode->Childs().begin());
    while (next)
    {
       TDEBUG("Next: " << REV_NO(next));
       static_cast<CWinLogData*>(next->GetUserData())->SetEmphasized(view, true);
       next = next->Next();
    }
}

CWinLogData* CWinLogData::HitTest(const wxPoint& point)
{
   if (!myTotalBounds.Contains(point))
      return 0;
   if (mySelfBounds.Contains(point))
      return this;

   CWinLogData* result;
   std::vector<CLogNode*>::iterator i;
   for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
   {
      CLogNode* subNode = *i;
      result = GetData(subNode)->HitTest(point);
      if (result)
         return result;
   }
   if (fNode->Next())
   {
      result = GetData(fNode->Next())->HitTest(point);
      if (result)
         return result;
   }
   return 0;
}

CWinLogData* CWinLogData::GetDiskNode(const std::string &revNo)
{
   if (IsDiskNode(revNo))
      return this;

   CWinLogData* result;
   std::vector<CLogNode*>::iterator i;
   for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
   {
      CLogNode* subNode = *i;
      result = GetData(subNode)->GetDiskNode(revNo);
      if (result)
         return result;
   }
   if (fNode->Next())
   {
      result = GetData(fNode->Next())->GetDiskNode(revNo);
      if (result)
         return result;
   }
   return 0;
}

void CWinLogData::Update(wxDC& dc, wxRegion& rgn, const std::string& revNo)
{
   TDEBUG_ENTER("Update");
   ourSandboxRevNo = revNo;
   
   // Compute merge point info
   ourMergePairs.clear();
   CollectMergePointPairPass1();

#ifdef TDEBUG_ON  
   TDEBUG_TRACE("Pass 1");
   MergePairVector::iterator it;
   for (it = ourMergePairs.begin(); it != ourMergePairs.end(); ++it)
   {
      CWinLogData* p1 = (*it).first;
      CWinLogData* p2 = (*it).second;
      TDEBUG_TRACE("(" << (p1 ? p1->GetStr() : "<null>") << ", " << (p2 ? p2->GetStr() : "<null>") << ")");
   }
#endif
   CollectMergePointPairPass2();
#ifdef TDEBUG_ON  
   TDEBUG_TRACE("Pass 2");
   for (it = ourMergePairs.begin(); it != ourMergePairs.end(); ++it)
   {
      CWinLogData* p1 = (*it).first;
      CWinLogData* p2 = (*it).second;
      TDEBUG_TRACE("(" << (p1 ? p1->GetStr() : "<null>") << ", " << (p2 ? p2->GetStr() : "<null>") << ")");
   }
#endif

   // Perform rest of work
   Update(dc, rgn);

   UpdateMergePoints(dc);
}

void CWinLogData::Update(wxDC& dc, wxRegion& rgn)
{
   TDEBUG("Rev: " << REV_NO(Node()) << " emph: " << myIsEmphasized);

   CLogNode* nextNode = fNode->Next();

   wxRect r = rgn.GetBox();
   if (rgn.Contains(r) == wxInRegion)
   {
      TDEBUG("In region: Rev: " << REV_NO(Node()) << " emph: " << myIsEmphasized);
      UpdateSelf(dc);

      int lineWidth = 2;
      if (myIsEmphasized && nextNode && static_cast<CWinLogData*>(nextNode->GetUserData())->myIsEmphasized)
      {
         TDEBUG("This: " << REV_NO(fNode) << " Next: " << REV_NO(nextNode));
         lineWidth = 4;
      }
      wxPen penBlue(*wxBLUE, static_cast<int>(lineWidth/ourScale), wxSOLID);       // Lines between revisions
      penBlue.SetCap(wxCAP_BUTT);
      wxPen penBlueDash(*wxBLUE, static_cast<int>(lineWidth/ourScale), wxUSER_DASH);
      penBlueDash.SetCap(wxCAP_BUTT);
      static const wxDash dash[] = { 0, 0, 1, 1 };
      penBlueDash.SetDashes(sizeof(dash)/sizeof(dash[0]), dash);
      wxPen penBlack(*wxBLACK, static_cast<int>(1/ourScale), wxSOLID);     // Lines between revisions and tags
      penBlack.SetCap(wxCAP_BUTT);

      int topY = mySelfBounds.GetTop() + static_cast<int>(2/ourScale);
      int botY = mySelfBounds.GetBottom() - static_cast<int>(2/ourScale);
      int lefX = mySelfBounds.GetRight() + static_cast<int>(4/ourScale);

      int count = 1;
      int tot = static_cast<int>(fNode->Childs().size()) + 1;
      
      // draw the children
      std::vector<CLogNode*>::iterator i;
      for (i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i, ++count)
      {
         CLogNode* subNode = *i;
         
         // draw the link
         wxRect& cb = GetData(subNode)->SelfBounds();
         switch (subNode->GetType())
         {
         case kNodeRev:
         case kNodeBranch:
            dc.SetPen(penBlue);
            break;

         case kNodeTag:
            dc.SetPen(penBlack);
            break;

         case kNodeHeader:
            dc.SetPen(penBlack);
         }
         float y = (float) topY + (float) (botY - topY) *
            (float) count / (float) tot;
         int rigX = cb.GetLeft() - static_cast<int>(4/ourScale);
         float x = (float) rigX - (float) (rigX - lefX) *
            (float) count / (float) tot;

         // special drawing for the header node
         // |
         // +--
         if (count == 1 && fNode->GetType() == kNodeHeader)
         {
            // |
            // +--
            dc.DrawLine(mySelfBounds.GetLeft() + HChildSpace/2, mySelfBounds.GetBottom(),
                        mySelfBounds.GetLeft() + HChildSpace/2, CenterPoint(cb).y);
            dc.DrawLine(mySelfBounds.GetLeft() + HChildSpace/2, CenterPoint(cb).y,
                        cb.GetLeft(), CenterPoint(cb).y);
         }
         else
         {
            if ((subNode->GetType() != kNodeTag) || ourShowTags)
            {
               int Delta = (int) (y-CenterPoint(cb).y);
               if (abs(Delta) > 1)
               {
                  // ---+
                  //    |
                  //    +---
                  dc.DrawLine(mySelfBounds.GetRight(), (int) y,
                              (int) x, (int) y);
                  dc.DrawLine((int) x, (int) y,
                              (int) x, CenterPoint(cb).y);
                  dc.DrawLine((int) x, CenterPoint(cb).y,
                              cb.GetLeft(), CenterPoint(cb).y);
               }
               else
               {
                  // Use straight line ----
                  dc.DrawLine(mySelfBounds.GetRight(), CenterPoint(cb).y,
                              cb.GetLeft(), CenterPoint(cb).y);
               }
            }
         }
         
         // draw the sub-node
         GetData(subNode)->Update(dc, rgn);
      }

      // draw the next node
      if (nextNode)
      {
         TDEBUG("Rev: " << REV_NO(Node()) << " emph: " << myIsEmphasized);
         // draw the link
         wxRect& nb = GetData(nextNode)->SelfBounds();
         switch (nextNode->GetType())
         {
         case kNodeRev:
            dc.SetPen(penBlue);
            if ((fNode->GetType() == kNodeRev) &&
                ((CLogNodeRev*) fNode)->GetNextDeleted() ||
                ((CLogNodeRev*) nextNode)->GetPrevDeleted())
               dc.SetPen(penBlueDash);
            break;
         case kNodeBranch:
            dc.SetPen(penBlue);
            break;
         default:
            dc.SetPen(penBlack);
         }
         int centerX = CenterPoint((mySelfBounds.GetWidth() < nb.GetWidth()) ? mySelfBounds : nb).x;
         dc.DrawLine(centerX, mySelfBounds.GetBottom(),
                     centerX, nb.GetTop());
         // draw the next node
         GetData(nextNode)->Update(dc, rgn);
      }

      dc.SetPen(wxNullPen);
      dc.SetBrush(wxNullBrush);
   }
}

void CWinLogData::ComputeBounds(const wxPoint& topLeft,
                                wxDC& dc)
{
    CLogNode* subNode;
    CLogNode* nextNode = fNode->Next();

    // first compute children's bounds
    std::vector<CLogNode*>::iterator i;
    for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
    {
        subNode = *i;
        CWinLogData::CreateNewData(subNode);
        GetData(subNode)->ComputeBounds(topLeft, dc);
    }
    if (nextNode)
        CWinLogData::CreateNewData(nextNode);
    
    // compute self place
    mySelfBounds = myEmptyrect;
    wxSize size = GetBoundsSelf(dc);
    mySelfBounds = wxRect(0, 0,
                   size.GetWidth() + InflateNodeSpace,
                   size.GetHeight() + InflateNodeSpace);

    // offset to the place assigned to this node
    mySelfBounds.Offset(topLeft - wxPoint(mySelfBounds.GetLeft(), mySelfBounds.GetTop()));
    
    // calculate the total height of the childrens
    int vSize = 0;
    for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
    {
        subNode = *i;
        wxRect& b = GetData(subNode)->Bounds();
        vSize += b.GetHeight();
    }
    if (!fNode->Childs().empty())
        vSize += static_cast<int>(fNode->Childs().size() - 1) * static_cast<int>(VChildSpace/ourScale);
    
    // offset the children relative to self
    wxPoint startChilds(topLeft.x + mySelfBounds.GetWidth() + static_cast<int>(HChildSpace/ourScale),
                        topLeft.y);

    // place the first child at the bottom of the header node so it won't take so much space for long filenames
    if ( fNode->GetType() == kNodeHeader )
    {
        startChilds = wxPoint(topLeft.x + static_cast<int>(HChildSpace/ourScale),
                              topLeft.y + mySelfBounds.GetHeight() + VChildSpace/ourScale);
    }

    for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
    {
        subNode = *i;
        if (!ourShowTags && (subNode->GetType() == kNodeTag))
            continue;
        wxRect curPos = GetData(subNode)->Bounds();
        GetData(subNode)->Offset(startChilds - wxPoint(curPos.GetLeft(), curPos.GetTop()));
        startChilds.y += curPos.GetHeight() + static_cast<int>(VChildSpace/ourScale);
    }

    // calculate the total bounds of the children
    wxRect bc;
    bc = myEmptyrect;
    for(i = fNode->Childs().begin(); i != fNode->Childs().end(); ++i)
    {
        subNode = *i;
        wxRect& b = GetData(subNode)->Bounds();
        bc = Union(bc, b);
    }

    // now we got the complete size
    myTotalBounds = Union(mySelfBounds, bc);
    
    if (nextNode)
    {
        wxPoint nextTopLeft;
        nextTopLeft.x = myTotalBounds.GetLeft();
        nextTopLeft.y = myTotalBounds.GetBottom() + static_cast<int>(VChildSpace/ourScale);
        GetData(nextNode)->ComputeBounds(nextTopLeft, dc);
        
        myTotalBounds = Union(myTotalBounds, GetData(nextNode)->Bounds());
    }
}


// Draw the merges relationship
void CWinLogData::UpdateMergePoints(wxDC& dc)
{
   TDEBUG_ENTER("UpdateMergePoints");

   wxPen pen(*myGraphMergeColor, 1, wxSOLID);
   wxPen penWide(*myGraphMergeColor, 2, wxSOLID);
   wxBrush brush(*myGraphMergeColor, wxSOLID);
  
   dc.SetBrush(brush);
   int arrowsize = static_cast<int>(ARROWSIZE/ourScale);
   if (arrowsize < 1)
      arrowsize = 1;

   MergePairVector::iterator it;   
   for (it = ourMergePairs.begin(); it != ourMergePairs.end(); ++it)
   {
      if (it->first && it->second)
      {
         wxRect rect_source = it->first->SelfBounds();
         wxRect rect_target = it->second->SelfBounds();

         bool selected = it->first->Selected() || it->second->Selected();
         dc.SetPen(selected ? penWide : pen);

         int sign = (rect_source.GetTop() > rect_target.GetTop()) ? -1 : 1;
            
         if (rect_source.GetLeft() > rect_target.GetRight())
         {
            // O <-+
            //     |
            //     +- O
            TDEBUG_TRACE("L > R");

            // Line
            wxPoint pts[4];
            pts[0] = wxPoint(rect_source.GetLeft(),
                             rect_source.GetTop() + rect_source.GetHeight()/2 + sign*arrowsize);
            pts[1] = wxPoint(pts[0].x - arrowsize/2, pts[0].y);
            pts[2] = wxPoint(rect_target.GetRight() + 2*arrowsize,
                             rect_target.GetTop() + rect_target.GetHeight()/2 - sign*arrowsize);
            pts[3] = wxPoint(pts[2].x - 2*arrowsize, pts[2].y);
            dc.DrawSpline(4, pts);

            // Arrow head
            pts[0] = wxPoint(rect_target.GetRight() + arrowsize,
                             rect_target.GetTop() + rect_target.GetHeight()/2 - arrowsize/2 - sign*arrowsize);
            pts[1] = wxPoint(pts[0].x, pts[0].y+arrowsize);
            pts[2] = wxPoint(rect_target.GetRight(),
                             rect_target.GetTop() + rect_target.GetHeight()/2 - sign*arrowsize);
            dc.DrawPolygon(3, pts);
         }
         else if (rect_source.GetLeft() == rect_target.GetLeft())
         {
            // O --+
            //     |
            // O <-+
            TDEBUG_TRACE("L == R");
            wxPoint pts[4];

            // Line
            pts[0] = wxPoint(rect_source.GetRight(),
                             rect_source.GetTop() + rect_source.GetHeight()/2 - sign*arrowsize);
            pts[1] = wxPoint(pts[0].x + 3*arrowsize, pts[0].y);
            pts[2] = wxPoint(rect_target.GetRight() + 2*arrowsize,
                             rect_target.GetTop() + rect_target.GetHeight()/2 + sign*arrowsize);
            pts[3] = wxPoint(pts[2].x - arrowsize, pts[2].y);
            dc.DrawSpline(4, pts);

            // Arrow head
            pts[0] = wxPoint(rect_target.GetRight() + arrowsize,
                             rect_target.GetTop() + rect_target.GetHeight()/2 - arrowsize/2 + sign*arrowsize);
            pts[1] = wxPoint(pts[0].x, pts[0].y+arrowsize);
            pts[2] = wxPoint(rect_target.GetRight(),
                             rect_target.GetTop() + rect_target.GetHeight()/2 + sign*arrowsize);
            dc.DrawPolygon(3, pts);
         }
         else
         {
            // O -+
            //    |
            //    +-> O
            TDEBUG_TRACE("L < R");
            wxPoint pts[7];
  
            pts[0] = wxPoint(rect_source.GetRight(), rect_source.GetTop() + rect_source.GetHeight()/2);
            pts[1] = wxPoint(pts[0].x + arrowsize/2, pts[0].y);
            pts[2] = wxPoint(rect_target.GetLeft() - 2*arrowsize, rect_target.GetTop() + rect_target.GetHeight()/2);
            pts[3] = wxPoint(pts[2].x + 2*arrowsize, pts[2].y);
            pts[4] = wxPoint(pts[3].x - arrowsize, pts[3].y-arrowsize/2);
            pts[5] = wxPoint(pts[4].x, pts[4].y+arrowsize); 
            pts[6] = pts[3];
            dc.DrawSpline(7, pts);

            pts[0] = wxPoint(rect_target.GetLeft() - arrowsize,
                             rect_target.GetTop() + rect_target.GetHeight()/2 - arrowsize/2);
            pts[1] = wxPoint(pts[0].x, pts[0].y+arrowsize);
            pts[2] = wxPoint(rect_target.GetLeft(), rect_target.GetTop() + rect_target.GetHeight()/2);
            dc.DrawPolygon(3, pts);
         }
      }
   }
 
   dc.SetPen(wxNullPen);
}

 
void CWinLogData::CollectMergePointPairPass1()
{
   CollectMergePointPairPass1Self();
 
   std::vector<CLogNode*>::iterator it = Node()->Childs().begin();
 
   while (it != Node()->Childs().end())
   {
      ((CWinLogData*) (*it)->GetUserData())->CollectMergePointPairPass1();
      ++it;
   }
 
   if (Node()->Next())
   {
      ((CWinLogData*) Node()->Next()->GetUserData())->CollectMergePointPairPass1();
   }
}
 
void CWinLogData::CollectMergePointPairPass2()
{
   CollectMergePointPairPass2Self();
 
   std::vector<CLogNode*>::iterator it = Node()->Childs().begin();
 
   while (it != Node()->Childs().end())
   {
      ((CWinLogData*) (*it)->GetUserData())->CollectMergePointPairPass2();
      ++it;
   }
 
   if (Node()->Next())
   {
      ((CWinLogData*) Node()->Next()->GetUserData())->CollectMergePointPairPass2();
   }
}
 
// Draw the node
void CWinLogData::UpdateSelf(wxDC&)
{
   TDEBUG_ENTER("CWinLogData::UpdateSelf");
}

// Get a vector of merge points pair (source-->Target).
// This is the first pass, which collects only the target
void CWinLogData::CollectMergePointPairPass1Self()
{
}
 
// Get a vector of merge points pair (source-->Target).
// This is the second pass, which makes the link between source and target
void CWinLogData::CollectMergePointPairPass2Self()
{
}
 
void CWinLogRevData::CollectMergePointPairPass1Self()
{
   const CRevFile& rev = ((CLogNodeRev*) Node())->Rev();
 
   if (!rev.MergePoint().empty())
   {
      ourMergePairs.push_back(std::make_pair<CWinLogData*, CWinLogData*>(0, this));
   }
}
 
void CWinLogRevData::CollectMergePointPairPass2Self()
{
   const CRevFile& rev = ((CLogNodeRev*) Node())->Rev();
 
   for (size_t i = 0; i < ourMergePairs.size(); ++i)
   {
      if (!ourMergePairs[i].first && (((CLogNodeRev*) ourMergePairs[i].second->Node())->Rev().MergePoint() == rev.RevNum()))
      {
         ourMergePairs[i].first = this;
//!!         rev.IsMergePointTarget() = true;
      }
   }
}
