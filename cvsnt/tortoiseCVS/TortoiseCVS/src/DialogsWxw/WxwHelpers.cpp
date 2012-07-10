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


#include "StdAfx.h"
#include "WxwHelpers.h"
#include "MessageDialog.h"
#include "ProgressDialog.h"
#include "ExtComboBox.h"
#include <list>
#include <io.h>
#include <fcntl.h>
#include <algorithm>    // for std::find
#include <Utils/StringUtils.h>
#include <Utils/Preference.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseDebug.h>
#include <cvstree/FlexLexer.h>
#include <cvstree/common.h>

// Stubs for multi monitor api
#if (_MSC_VER >= 1300) || defined(__GNUWIN32__)
#define COMPILE_MULTIMON_STUBS
#endif
#include "../Utils/MultiMon.h"


#include "../Utils/MultiMonUtils.h"
#include <windows.h> // for SendMessage

wxComboBox* MakeComboList(wxWindow* parent,
                          int id,
                          const std::string& registryName,
                          const std::string& extraDefault,
                          int flags,
                          std::vector<std::string> *strings,
                          wxValidator* validator)
{
   // Read list from registry
   std::vector<std::string> history;
   TortoiseRegistry::ReadVector(registryName, history);

   // Add extra guess default (e.g. your user name on the machine)
   if (!extraDefault.empty() && std::find(history.begin(), history.end(), extraDefault) == history.end())
      history.push_back(extraDefault);

   // Make it
   long style = 0;
   if (flags & COMBO_FLAG_READONLY)
      style |= wxCB_READONLY;
   wxComboBox* combo = new wxComboBox(parent, id, wxT(""),
                                      wxDefaultPosition, wxDefaultSize, 0, NULL, style,
                                      validator ? *validator : wxDefaultValidator);
   if (flags & COMBO_FLAG_ADDEMPTY)
      combo->Append(wxT(" "));
   for (std::vector< std::string >::iterator it = history.begin(); it != history.end(); ++it)
   {
      if (strings)
         strings->push_back(*it);
      std::string s = *it;
      // Truncate to 1024 bytes - otherwise CB_GETLBTEXT crashes in dropdown 
      // sizer (on XP and Chinese Win2K)
      const std::string::size_type truncLength = 80;
      if (s.length() > truncLength)
      {
         s = s.substr(0, truncLength-3) + "...";
      }
      FindAndReplace<std::string>(s, "\n", " ");
      FindAndReplace<std::string>(s, "\t", " ");
      combo->Append(MultibyteToWide(s).c_str());
   }
   if (!history.empty() && !(flags & COMBO_FLAG_NODEFAULT))
   {
      combo->SetSelection(0);
   }

   return combo;
}

void WriteComboList(wxComboBox* widg, const std::string& registryName)
{
   std::string newValue = wxAscii(widg->GetValue().c_str());
   if (newValue.empty())
      return;

   // Read list from registry
   std::vector< std::string > history;
   TortoiseRegistry::ReadVector(registryName, history);

   // bubble up selected value
   std::vector< std::string >::iterator it = std::find( history.begin(), history.end(),
                                                        newValue );
   if ( it != history.end() )
      history.erase( it );
   history.insert(history.begin(), newValue );

   // drop old history items
   while( history.size() > 10 )
      history.pop_back();
   TortoiseRegistry::WriteVector(registryName, history );
}

void ReadOptionMenu(wxChoice* widg, const std::string& registryName)
{
   int value = 0;
   TortoiseRegistry::ReadInteger(registryName, value);
   widg->SetSelection(value);
}

void WriteOptionMenu(wxChoice* widg, const std::string& registryName)
{
   int value = widg->GetSelection();
   TortoiseRegistry::WriteInteger(registryName, value);
}

void DestroyDialog(wxDialog* dlg)
{
   delete dlg;

   // This causes problems with latest CVS wxWindows
   // (when a second dialog is shown, while the first one
   // is being deleted in the garbage collector, the second
   // window becomes mysteriously hidden)
   // Really, I think Destroy() is the right thing to call
   // though?
   // dlg->Destroy();
}

void FillTagBranchComboBox(wxComboBox* ComboBox, 
                           std::vector<std::string>& Elements)
{
   int ElementCount = static_cast<int>(Elements.size());
   if (ElementCount > 0)
   {
      ComboBox->Clear();
      std::vector<std::string>::iterator it = Elements.begin();
      ComboBox->SetValue(wxText(it->c_str()));

      while (it != Elements.end())
      {
         ComboBox->Append(wxText(it->c_str()));
         ++it;
         --ElementCount;
         if (ElementCount <= 0)
            break;
      }
   }
}


// Wrap text
wxString WrapText(int width, const wxWindow &window, const wxString &string)
{
   wxString curLine;
   wxString newText;
   const wxChar *pc = string;
   int w;
   int wordBreak;

   while (*pc != '\0')
   {
      switch (*pc)
      {
      // Newline
      case wxT('\n'):
         newText += curLine + wxT('\n');
         curLine.Empty();
         break;

      // Whitespace
      case wxT(' '):
         curLine += wxT(' ');
         break;

      // Other character
      default:
         // Do we exceed the line width limit?
         window.GetTextExtent(curLine + *pc, &w, 0);
         while (w > width)
         {
            // Line has become too long
            if (curLine.IsEmpty())
            {
               // At least 1 character per line is mandatory
               newText += *pc + wxT('\n');
               curLine.Empty();
               break;
            }
            else
            {
               wordBreak = FindWordBreak(curLine);
               if (wordBreak != 0)
               {
                  newText += curLine.Mid(0, wordBreak) + wxT('\n');
                  curLine = curLine.Mid(wordBreak);
               }
               else
               {
                  newText += curLine + wxT('\n');
                  curLine.Empty();
               }
            }
            window.GetTextExtent(curLine + *pc, &w, 0);
         }
         curLine += *pc;
      }


      // Move to next character
      pc++;
   }
   if (!curLine.IsEmpty())
      newText += curLine;

   return newText;
}



// Find word break
int FindWordBreak(const wxString &line)
{
   const wxChar *pc = line;
   pc += line.Length();
   int res = static_cast<int>(line.Length());
   int i = res + 1;
   while (i > 0)
   {
      switch (*pc)
      {
      case ' ':
         // Word break after whitespace
         return i;
      case '-':
         // Word break after dash
         return i;
      case '\t':
         // Word break before tab
         return i - 1;
      }

      pc--;
      i--;
   }
   return res;
}



// Convert colorref to wxColour
wxColour ColorRefToWxColour(COLORREF colorref)
{
   return wxColour(GetRValue(colorref), GetGValue(colorref), 
                   GetBValue(colorref));
}


// Convert wxColour to colorref 
COLORREF WxColourToColorRef(const wxColour& color)
{
   return RGB(color.Red(), color.Green(), color.Blue());
}


// Set dialog position
void SetTortoiseDialogPos(wxDialog* dialog, HWND hwnd)
{
   TDEBUG_ENTER("SetTortoiseDialogPos");
   HWND hParent = hwnd;
   if (dialog->GetParent())
   {
      TDEBUG_TRACE("Use dialog parent");
      hParent = (HWND) dialog->GetParent()->GetHWND();
   }
   else 
      TDEBUG_TRACE("Use supplied window handle");
      
   if (hParent)
   {
      RECT r;
      if (IsWindowFullyHidden(hParent))
      {
         WINDOWPLACEMENT wndpl;
         wndpl.length = sizeof(wndpl);
         GetWindowPlacement(hParent, &wndpl);
         CenterWindowToMonitor(&wndpl.rcNormalPosition, GetHwndOf(dialog), true);
      }
      else
      {
         GetWindowRect(hParent, &r);
         int l, t, w, h;
         dialog->GetSize(&w, &h);
         l = (r.left + r.right - w) / 2;
         t = (r.top + r.bottom - h) / 2;
         TDEBUG_TRACE("Move destination: l: " << l << ", t: " << t);
         ::MoveWindow(GetHwndOf(dialog), l, t, w, h, false);
      }
   }
   else
   {
      TDEBUG_TRACE("Just center the dialog");
      dialog->Center(wxBOTH);
   }

   MakeSureWindowIsVisible((HWND) dialog->GetHWND());
}


// Store dialog size
void StoreTortoiseDialogSize(wxDialog* dialog, const char* dialogName)
{
   std::string sKey = "Dialogs\\" + std::string(dialogName) + "\\";
   // Store size
   if (!(dialog->IsMaximized() || dialog->IsIconized()))
   {
      wxSize size = dialog->ConvertPixelsToDialog(dialog->GetSize());
      TortoiseRegistry::WriteSize(sKey + "Dialog Size", size.GetWidth(), size.GetHeight());
      TortoiseRegistry::WriteBoolean(sKey + "Dialog Maximized", false);
   }
   else if (dialog->IsMaximized())
   {
      TortoiseRegistry::WriteBoolean(sKey + "Dialog Maximized", true);
   }
   else if (dialog->IsIconized())
   {
      TortoiseRegistry::WriteBoolean(sKey + "Dialog Maximized", false);
   }
}


// Restore dialog size
void RestoreTortoiseDialogSize(wxDialog* dialog, const char* dialogName,
                               const wxSize& defSize)
{
   int w, h;
   std::string sKey = "Dialogs\\" + std::string(dialogName) + "\\Dialog Size";
   if (TortoiseRegistry::ReadSize(sKey, &w, &h))
   {
      dialog->SetSize(dialog->ConvertDialogToPixels(wxSize(w, h)));
   }
   else if (defSize != wxDefaultSize)
   {
      if ((defSize.GetWidth() > 0) && (defSize.GetHeight() > 0))
      {
         dialog->SetSize(dialog->ConvertDialogToPixels(defSize));
      }
   }
}


// Restore dialog state
void RestoreTortoiseDialogState(wxDialog* dialog, const char* dialogName)
{
   std::string sKey = "Dialogs\\" + std::string(dialogName) + "\\Dialog Maximized";
   if (TortoiseRegistry::ReadBoolean(sKey, false))
   {
      dialog->Maximize(true);
   }
}


#ifndef _USRDLL

// Get repo graph for server
CRepoNode* GetRepoGraph(const wxString& title,
                        wxWindow* parent,
                        const char* filename,
                        RepoParserData& repoParserData,
                        CvsRepoParameters* settings,
                        int maxDirectories)
{
   return CvsRepoGraph(repoParserData, settings, 0);
}

// Get log graph for file
CLogNode* GetLogGraph(const wxString& title,
                      wxWindow* parent,
                      const char* filename,
                      ParserData& parserData,
                      CvsLogParameters* settings,
                      int maxRevisions)
{
   TDEBUG_ENTER("GetLogGraph");
   if (!GetLog(title, parent, filename, parserData, settings, maxRevisions))
      return 0;

   return CvsLogGraph(parserData, settings);
}

bool GetLog(const wxString& title,
            wxWindow* parent,
            const char* filename,
            ParserData& parserData,
            CvsLogParameters* settings,
            int maxRevisions)
{
#if 1
   CVSAction glue(parent);
   glue.SetCloseIfOK(true);
   glue.SetHideStdout();
   glue.SetProgressCaption(title);

   MakeArgs args;
   args.add_global_option("-f");
   args.add_option("log");

   if (settings && settings->myCurrentBranchOnly)
   {
      int revsLeft = maxRevisions;
      std::string sRev = CVSStatus::GetRevisionNumber(filename);
      bool bFirstLoop = true;
      std::string::size_type p;
      do
      {
         int lastNum = atoi(sRev.substr(sRev.find_last_of(".") + 1).c_str());
         int num = lastNum - revsLeft;
         if (num <= 0)
            num = 1;
         revsLeft -= (lastNum - num + 1);

         p = sRev.find_last_of(".");
         sRev = sRev.substr(0, p);
         std::string s;
         if (bFirstLoop)
         {
            s = PrintfA("-r%s.%d:", sRev.c_str(), num);
            bFirstLoop = false;
         }
         else
         {
            s = PrintfA("-r%s.%d:%s.%d", sRev.c_str(), num, sRev.c_str(), lastNum);
         }
         args.add_option(s);
         p = sRev.find_last_of(".");
         if (p != std::string::npos)
         {
            sRev = sRev.substr(0, sRev.find_last_of("."));
         }
      } while (revsLeft > 0 && p != std::string::npos);
   }

   args.add_arg(ExtractLastPart(filename));
   bool ok = glue.Command(StripLastPart(filename), args);

   if (!ok)
      return false;

   std::string out = glue.GetStdOutText();
#else
   // Debugging
   std::ifstream in("C:\\cvs-log-output.txt");
   std::string out;
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

   // Check if the filename contains a semicolon
   std::string file = GetLastPart(filename);
   if (file.find(";") != std::string::npos)
   {
       // Semicolons normally denote the end of the filename. So change it to something else.
       file = "filename: " + file;
       std::string escapedFilename = file;
       FindAndReplace<std::string>(escapedFilename, ";", "%3B"); // URL-encode ';'
       FindAndReplace<std::string>(out, file, escapedFilename);
   }

   if (out.empty())
   {
      wxString msg(_("This file is new and has never been committed to the server or is an empty file on the server."));
      msg += wxT("\n\n");
      msg += wxText(filename);
      DoMessageDialog(0, msg);
      return false;
   }

   std::istringstream iss;
   iss.str(out);
   std::ostringstream oss;
   FlexLexer* lexer = CreateCvsLogLexer(&iss, &oss);
   bool debug = false;
#ifdef _DEBUG
//   debug = true;
#endif
   lexer->set_debug(debug ? 1 : 0);
   StartCvsLogParser(lexer, &parserData, debug);
   delete lexer;
   
   parserData.rcsFiles[0].WorkingFile() = ExtractLastPart(filename).c_str();

   return true;
}

#endif // _USRDLL

// Helper function to perform actual substitution
static void HyperlinkComment(std::string& comment,
                             std::string::iterator pos,
                             std::string::size_type patternSize,
                             std::string::size_type& offset,
                             const std::string& bugtracker)
{
   std::string::iterator start = pos;
   pos += patternSize;
   offset = pos - comment.begin();
   // Check if the pattern is followed by a digit
   if ((pos != comment.end()) && isdigit(*pos))
   {
      // Get all digits
      do
         ++pos;
      while ((pos != comment.end()) && isdigit(*pos));
      std::string digits(start+patternSize, pos);
      // Prepare URL
      std::string url = PrintfA(bugtracker.c_str(), digits.c_str());
      // Insert it
      comment.insert(start+patternSize+digits.size(), url.begin(), url.end());
      // Compute offset so that we don't process anything twice
      offset += url.size();
   }
}

static bool nocase_compare(char c1, char c2)
{
   return toupper(c1) == toupper(c2);
}

std::string HyperlinkComment(const std::string& comment, const std::string& bugtracker)
{
   std::string newComment = comment;
   std::string bugtrackerText = " [[ " + bugtracker + " ]]";
   if (!bugtracker.empty())
   {
      // First, 'bug XXX'
      std::string pattern = "bug ";
      std::string::size_type offset = 0;
      while (1)
      {
         std::string::iterator pos = std::search(newComment.begin() + offset, newComment.end(),
                                                 pattern.begin(), pattern.end(),
                                                 nocase_compare); // comparison criterion
         if (pos == newComment.end())
            break;
         else
            HyperlinkComment(newComment, pos, pattern.size(), offset, bugtrackerText);
      }
      // second, 'Bug Number:\n'
      offset = 0;
      pattern = "Bug Number:\n";
      while (1)
      {
         std::string::iterator pos = std::search(newComment.begin() + offset, newComment.end(),
                                                 pattern.begin(), pattern.end(),
                                                 nocase_compare); // comparison criterion
         if (pos == newComment.end())
            break;
         else
            HyperlinkComment(newComment, pos, pattern.size(), offset, bugtrackerText);
      }
      // Then, '#XXX'
      offset = 0;
      while (1)
      {
         std::string::size_type pos = newComment.find("#", offset);
         if (pos == std::string::npos)
            break;
         else
            HyperlinkComment(newComment, newComment.begin() + pos, 1, offset, bugtrackerText);
      }
   }
   return newComment;
}

std::string UnhyperlinkComment(const std::string& comment)
{
   std::string newComment = comment;
   std::string startPattern = " [[ ";
   std::string endPattern = " ]]";
   std::string::size_type offset = 0;
   while (1)
   {
      std::string::iterator pos = std::search(newComment.begin() + offset, newComment.end(),
                                              startPattern.begin(), startPattern.end());
      if (pos == newComment.end())
         break;
      else
      {
         std::string::iterator endPos = std::search(pos, newComment.end(),
                                                    endPattern.begin(), endPattern.end());
         if (endPos != newComment.end())
         {
            offset = pos - newComment.begin();
            newComment.erase(pos, endPos+endPattern.size());
         }
         else
         {
            // False positive (only begin pattern present): Move offset beyond begin pattern
            offset = pos - newComment.begin() + 1;
         }
      }
   }
   return newComment;
}
