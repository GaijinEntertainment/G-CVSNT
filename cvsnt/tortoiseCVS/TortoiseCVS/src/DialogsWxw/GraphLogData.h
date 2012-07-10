/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef GRAPHLOGDATA_H
#define GRAPHLOGDATA_H

#include <string>
#include <vector>
#include <wx/wx.h>
#include <windows.h>

#include "../cvstree/CvsLog.h"
#include "../CVSGlue/CvsEntries.h"


class CWinLogData : public CLogNodeData
{
public:
   enum NodeShape
   {
      kRectangle,
      kRoundRect,
      kOctangle
   };

   // Graphics constants
   enum
   {
      ARROWSIZE     = 8,
      SHADOW_OFFSET = 4
   };


   typedef std::vector< std::pair<CWinLogData*, CWinLogData*> > MergePairVector;

   CWinLogData(CLogNode* node);
   virtual ~CWinLogData();

   static void InitializeStatics();
   static void CleanupStatics();

   inline kLogNode GetType(void) const { return fNode->GetType(); }
   std::string GetStr();

   void ComputeBounds(const wxPoint& topLeft, wxDC& hdc);
   void Offset(const wxPoint& o);
   // This is called by clients
   void Update(wxDC& hdc, wxRegion& rgn, const std::string &revNo);
   
   inline wxRect& Bounds(void) { return myTotalBounds; }
   inline wxRect& SelfBounds(void) { return mySelfBounds; }

   inline bool Selected(void) const { return myIsSelected; }
   void SetSelected(wxScrolledWindow* view, bool state);
   void SetHighlighted(wxScrolledWindow* view, bool state);
   void SetEmphasized(wxScrolledWindow* view, bool state);
   void UnselectAll(wxScrolledWindow* view);
   void UnhighlightAll(wxScrolledWindow* view);

   void HighlightBranch(wxScrolledWindow* view, const std::string& branch);
   void HighlightTrunk(wxScrolledWindow* view);

   CWinLogData* HitTest(const wxPoint& point);
   CWinLogData* GetDiskNode(const std::string &revNo);

   static CWinLogData* CreateNewData(CLogNode* node);

   static void SetShowTags(bool showtags);
   static void SetScale(double scale);

   bool IsDiskNode(const std::string &revNo);
   bool IsDeadNode();
   bool IsUserState();

   void DrawNode(wxDC& dc,
                 const wxColour& contour, 
                 NodeShape shape,
                 int penStyle = wxSOLID,
                 int penWidth = 1);
   static void DrawOctangle(wxDC& dc, const wxRect& selfB);

protected:
   // Recursive worker method
   void Update(wxDC& hdc, wxRegion& rgn);

   void HighlightBranch(wxScrolledWindow* view, const CLogNode* branchParent);

   // Compute merge point data and draw merge arrows
   void UpdateMergePoints(wxDC& dc);
   
   virtual void UpdateSelf(wxDC& hdc) = 0;
   virtual wxSize GetBoundsSelf(wxDC& hdc) = 0;

   void CollectMergePointPairPass1();
   void CollectMergePointPairPass2();
   virtual void CollectMergePointPairPass1Self();
   virtual void CollectMergePointPairPass2Self();
    
   inline CWinLogData* GetData(CLogNode* node) { return (CWinLogData*) node->GetUserData(); }

   static wxColour* myGraphSelColor;
   static wxColour* myGraphHighlightColor;
   static wxColour* myGraphShadowColor;
   static wxColour* myGraphHeaderColor;
   static wxColour* myGraphTagColor;
   static wxColour* myGraphBranchColor;
   static wxColour* myGraphNodeColor;
   static wxColour* myGraphDiskNodeColor;
   static wxColour* myGraphDeadNodeColor;
   static wxColour* myGraphUserStateNodeColor;
   static wxColour* myGraphMergeColor;
   static int       myRefCount;

   static bool                  ourShowTags;
   static double                ourScale;
   static MergePairVector       ourMergePairs;
   static std::string           ourSandboxRevNo;
   
   wxRect myEmptyrect;
   wxRect mySelfBounds;
   wxRect myTotalBounds;
   bool myIsSelected;
   bool myIsHighlighted;
   bool myIsEmphasized;
};

class CWinLogHeaderData : public CWinLogData
{
public:
    CWinLogHeaderData(CLogNode* node) : CWinLogData(node) {}
    virtual ~CWinLogHeaderData() {}

protected:
    virtual void UpdateSelf(wxDC& hdc);
    virtual wxSize GetBoundsSelf(wxDC& hdc);
};

class CWinLogRevData : public CWinLogData
{
public:
    CWinLogRevData(CLogNode* node) : CWinLogData(node) {}
    virtual ~CWinLogRevData() {}
   
protected:
    virtual void UpdateSelf(wxDC& hdc);
    virtual wxSize GetBoundsSelf(wxDC& hdc);
    virtual void CollectMergePointPairPass1Self();
    virtual void CollectMergePointPairPass2Self();
};

class CWinLogTagData : public CWinLogData
{
public:
    CWinLogTagData(CLogNode* node) : CWinLogData(node) {}
    virtual ~CWinLogTagData() {}
   
protected:
    virtual void UpdateSelf(wxDC& hdc);
    virtual wxSize GetBoundsSelf(wxDC& hdc);
};

class CWinLogBranchData : public CWinLogData
{
public:
    CWinLogBranchData(CLogNode* node) : CWinLogData(node) {}
    virtual ~CWinLogBranchData() {}
   
protected:
    virtual void UpdateSelf(wxDC& hdc);
    virtual wxSize GetBoundsSelf(wxDC& hdc);
};

#endif
