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


#include <StdAfx.h>
#include "DefaultMenus.h"
#include "HasMenu.h"
#include <Utils/CrapLexer.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/StringUtils.h>
#ifdef __GNUC__
#  include <ctype.h>  // for tolower
#else
#  include <locale>   // for tolower
#endif
#include <algorithm>    // for transform
#include <map>
#include <sstream>
#include <stdlib.h>

#include <Utils/Translate.h>
#include <Utils/TortoiseDebug.h>

static bool SlurpFlags(CrapLexer& lex, int& flags);
static void ShowLexErr(CrapLexer& lex, const wxString& msg = _("Syntax error"));

void FillInDefaultMenus(std::vector<MenuDescription>& menus)
{
   TDEBUG_ENTER("FillInDefaultMenus");
   CrapLexer lex(GetTortoiseDirectory() + "TortoiseMenus.config");

   bool done = false;
   while (!done)
   {
      int type;
      std::string tok;

      type = lex.GetTok(tok);
      if (type == CrapLexer::typeFinished)
         break;
      else if (type == CrapLexer::typeErr)
      {
         ShowLexErr(lex, _("Unexpected end of file"));
         return;
      }
      else
      {
         MakeLowerCase(tok);
         if (tok == "item")
         {
            std::string verb;
            std::string helptext;
            std::string menutext;
            std::string icon;
            int flags;

            if (lex.GetTok(verb) != CrapLexer::typeString)
            {
               ShowLexErr(lex);
               return;
            }
            if (lex.GetTok(menutext) != CrapLexer::typeString)
            {
               ShowLexErr(lex);
               return;
            }
            if (!SlurpFlags(lex, flags))
            {
               ShowLexErr(lex);
               return;
            }
            if (lex.GetTok(helptext) != CrapLexer::typeString)
            {
               ShowLexErr(lex);
               return;
            }
            if (lex.GetTok(icon) != CrapLexer::typeString)
            {
               ShowLexErr(lex);
               return;
            }

            menus.push_back(MenuDescription(verb, menutext, flags, helptext, icon));
         }
         else if (tok == "separator")
         {
            int flags;
            if (!SlurpFlags(lex, flags))
            {
               ShowLexErr(lex);
               return;
            }
            menus.push_back(MenuDescription("", "", flags, "", ""));
         }
         else
         {
            ShowLexErr(lex);
            return;
            //error
         }
      }
   }
}




static bool SlurpFlags(CrapLexer& lex, int& flags)
{
   TDEBUG_ENTER("SlurpFlags");
   static bool init = false;
   static std::map< std::string, int > flagtable;
   if (!init)
   {
      flagtable["on_sub_menu"]           = ON_SUB_MENU;
      flagtable["exactly_one"]           = EXACTLY_ONE;
      flagtable["always_has"]            = ALWAYS_HAS;
      flagtable["has_edit"]              = HAS_EDIT;
      flagtable["has_excl_edit"]         = HAS_EXCL_EDIT;
      flagtable["all_must_match_type"]   = ALL_MUST_MATCH_TYPE;
      flagtable["type_not_in_cvs"]       = TYPE_NOTINCVS;
      flagtable["type_nowhere_near_cvs"] = TYPE_NOWHERENEAR_CVS;
      flagtable["type_added"]            = TYPE_ADDED;
      flagtable["type_in_cvs_rw"]        = TYPE_INCVS_RW;
      flagtable["type_in_cvs_ro"]        = TYPE_INCVS_RO;
      flagtable["type_in_cvs_locked"]    = TYPE_INCVS_LOCKED;
      flagtable["type_ignored"]          = TYPE_IGNORED;
      flagtable["type_changed"]          = TYPE_CHANGED;
      flagtable["type_renamed"]          = TYPE_RENAMED;
      flagtable["type_conflict"]         = TYPE_CONFLICT;
      flagtable["type_static"]           = TYPE_STATIC;
      flagtable["type_outer_dir"]        = TYPE_OUTERDIRECTORY;
      flagtable["type_directory"]        = TYPE_DIRECTORY;
      flagtable["type_file"]             = TYPE_FILE;
      init = true;
   }

   flags = 0;
   std::string tok;
   if (lex.GetTok(tok) != CrapLexer::typeString)
      return false;
   if (tok != "(")
      return false;

   while (true)
   {
      if (lex.GetTok(tok) != CrapLexer::typeString)
         return false;

      MakeLowerCase(tok);

      if (tok == ")")
         break;   // done.

      std::map< std::string, int >::const_iterator it =
         flagtable.find(tok);

      if (it == flagtable.end())
         return false;

      flags |= it->second;
   }
   return true;
}


static void ShowLexErr(CrapLexer& TDEBUG_USED(lex), const wxString& TDEBUG_USED(msg))
{
#if defined _DEBUG
    wxString s = Printf(_("Error while parsing config file\n(\"%s\" line %d): %s"),
                        wxTextCStr(lex.Filename()), lex.LineNum(), msg.c_str());
    ::MessageBox(NULL, s.c_str(), wxT("TortoiseShell.dll"), MB_OK);
#endif
}
