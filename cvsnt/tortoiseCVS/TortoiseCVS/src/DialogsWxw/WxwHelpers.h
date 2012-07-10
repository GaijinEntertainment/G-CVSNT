// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - September 2000

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

#ifndef WXW_HELPERS
#define WXW_HELPERS

#include <string>
#include <vector>
#include <windows.h>
#include <wx/wx.h>
#include <wx/colour.h>
#include "../cvstree/cvslog.h"
#include "../cvstree/cvsrepo.h"

#define wxDLG_UNIT_X(parent, x) parent->ConvertDialogToPixels(wxSize(x, 0)).GetWidth()
#define wxDLG_UNIT_Y(parent, y) parent->ConvertDialogToPixels(wxSize(0, y)).GetHeight()

// General helper
template<class T>
std::vector<wxString> ConvertToWxStringArray(const T& t)
{
   std::vector<wxString> result;
   for (typename T::const_iterator it = t.begin(); it != t.end(); ++it)
      result.push_back(it->c_str());
   return result;
}

enum
{
   // No item is selected
   COMBO_FLAG_NODEFAULT = 1,
   // Contents cannot be edited
   COMBO_FLAG_READONLY = 2,
   // An empty entry is added at the start
   COMBO_FLAG_ADDEMPTY = 4
};

// wxComboList
wxComboBox* MakeComboList(wxWindow* parent,
                          int id,
                          const std::string& registryName,
                          const std::string& extraDefault = "",
                          int flags = 0,
                          /// If nonzero, filled with contents of combolist
                          std::vector<std::string>* strings = 0,
                          wxValidator* validator = 0);
void WriteComboList(wxComboBox* widg, const std::string& registryName);

// wxChoice
void ReadOptionMenu(wxChoice* widg, const std::string& registryName);
void WriteOptionMenu(wxChoice* widg, const std::string& registryName);

// Wrapper for wxWindows destruction, as dialog-centric apps
// aren't very stable in wxWindows yet.
void DestroyDialog(wxDialog* dlg);

// Fill a combobox with the list of branches and/or tags
void FillTagBranchComboBox(wxComboBox* ComboBox, 
                           std::vector<std::string>& Elements);

// Wrap text
wxString WrapText(int width, const wxWindow &window,
                        const wxString &string);

// Find word break
int FindWordBreak(const wxString &line);

// Convert colorref to wxColour
wxColour ColorRefToWxColour(COLORREF colorref);

// Convert wxColour to colorref 
COLORREF WxColourToColorRef(const wxColour& color);

// Set dialog position
void SetTortoiseDialogPos(wxDialog* dialog, HWND hwnd = 0);

// Store dialog size
void StoreTortoiseDialogSize(wxDialog* dialog, const char* dialogName);

// Restore dialog size
void RestoreTortoiseDialogSize(wxDialog* dialog, const char* dialogName,
                               const wxSize& defSize = wxDefaultSize);

// Restore dialog state
void RestoreTortoiseDialogState(wxDialog* dialog, const char* dialogName);

// Get log for file
bool GetLog(const wxString& title,
            wxWindow* parent,
            const char* filename,
            ParserData& parserData,
            CvsLogParameters* settings = 0,
            int maxRevisions = 0);

// Get log graph for file
CLogNode* GetLogGraph(const wxString& title,
                      wxWindow* parent,
                      const char* filename,
                      struct ParserData& parserData,
                      CvsLogParameters* settings = 0,
                      int maxDirectories = 0);

// Get log graph for file
CRepoNode* GetRepoGraph(const wxString& title,
                        wxWindow* parent,
                        const char* filename,
                        struct RepoParserData& repoParserData,
                        CvsRepoParameters* settings = 0,
                        int maxDirectories = 0);

// Return remote window handle for TortoiseAct
HWND GetRemoteHandle();

// Replace "bug XXX" and "#XXX" in comment with hyperlinks
std::string HyperlinkComment(const std::string& comment, const std::string& bugtracker);

std::string UnhyperlinkComment(const std::string& comment);

#endif
