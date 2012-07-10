// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - October 2005

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

#include "stdafx.h"
#include <wx/intl.h>
#include <shlobj.h>
#include <Utils/StringUtils.h>
#include <Utils/LangUtils.h>
#include <Utils/PathUtils.h>
#include <Utils/TortoiseDebug.h>
#include <Utils/TortoiseException.h>


static bool Process(std::istream& input,
                    const std::string& isldir,
                    const std::string& localedir);
static bool PrintTranslation(const std::string& line,
                             const std::vector<std::string>& langNames,
                             const std::string& localedir,
                             bool customMessage);
static void PrintLanguageEntries(const std::vector<std::string>& langNames);

static void InitializeLanguageMap();

static std::vector<std::string> GetLangNames(const std::string& localedir);

static std::map<std::string, std::string> LanguageMap;


// Arguments: path_to_inno_isl_files path_to_mo_files 
int main(int argc, char** argv)
{
   InitializeLanguageMap();
   
   std::string isldir = argv[1];
   std::string localedir = argv[2];

   return Process(std::cin, isldir, localedir) ? 0 : 1;
}


static bool Process(std::istream& input,
                    const std::string& isldir,
                    const std::string& localedir)
{
   std::vector<std::string> LangNames = GetLangNames(localedir);
   
   bool inLangSection = false;
   bool inCustomMessagesSection = false;

   std::string line;
   while (getline(input, line))
   {
      Trim(line);
      if ((line.size() > 2) && (line[0] == '[') && (line[line.size()-1] == ']'))
      {
         if (line == "[Languages]")
            inLangSection = true;
         else
            inLangSection = false;
         if ((line == "[CustomMessages]") ||
             (line == "[Messages]"))
            inCustomMessagesSection = true;
         else
            inCustomMessagesSection = false;
      }

      if ((line.find("_(") != std::string::npos) ||
          (line.find("LANG(") != std::string::npos))
      {
         if (!PrintTranslation(line, LangNames, localedir, inCustomMessagesSection))
            return false;
         continue;
      }
      std::cout << line << std::endl;

      if (inLangSection)
      {
         if (line.find("Name: \"en_GB\"") != std::string::npos)
         {
            PrintLanguageEntries(LangNames);
         }
      }
   }
   return true;
}

static bool PrintTranslation(const std::string& line,
                             const std::vector<std::string>& langNames,
                             const std::string& localedir,
                             bool customMessage)
{
   for (std::vector<std::string>::const_iterator it = langNames.begin();
        it != langNames.end(); ++it)
   {
      wxLocale locale;
      locale.AddCatalogLookupPathPrefix(wxTextCStr(localedir + "\\Locale"));

      const wxLanguageInfo* info = GetLangInfoFromCanonicalName(*it);
      if (info)
      {
          if (locale.Init(info->Language, wxLOCALE_CONV_ENCODING))
          {
              if (!locale.AddCatalog(wxT("Setup")))
              {
                  std::cerr << "Error loading message catalog for locale "
                            << wxAscii(locale.GetCanonicalName())
                            << " (Path " << localedir << "). Messages will not be translated." << std::endl;
                  return false;
              }
          }
      }

      std::string::size_type underscore = line.find("_(");
      std::string::size_type arg = 0;
      if (underscore != std::string::npos)
          arg = underscore + 3;
      else
      {
          underscore = line.find("LANG(");
          if (underscore != std::string::npos)
              arg = underscore + 6;
      }
      if (underscore != std::string::npos)
      {
         std::string::size_type close = line.find("\")", underscore);
         if (close != std::string::npos)
         {
             if (customMessage)
                 std::cout << *it << ".";
             std::cout << line.substr(0, underscore)
                       << wxAscii(locale.GetString(wxTextCStr(line.substr(arg, close-arg))))
                       << line.substr(close+2);
             if (!customMessage)
                 std::cout << "; Languages: " << *it;
             std::cout << std::endl;
         }
      }
   }
   return true;
}

static void PrintLanguageEntries(const std::vector<std::string>& langNames)
{
   for (std::vector<std::string>::const_iterator it = langNames.begin();
        it != langNames.end(); ++it)
   {
      std::string lang = *it;
      MakeLowerCase(lang);
      if (lang != std::string("en_gb"))
      {
         std::map<std::string, std::string>::iterator mapIter = LanguageMap.find(lang);
         if (mapIter != LanguageMap.end())
         {
            std::cout << "Name: \"" << *it << "\"; MessagesFile: \"compiler:"
                      << mapIter->second << ".isl\"" << std::endl;
         }
      }
   }
}

static void InitializeLanguageMap()
{
   LanguageMap.insert(std::make_pair("ar_tn", "Arabic"));
   LanguageMap.insert(std::make_pair("ca_es", "Catalan"));
   LanguageMap.insert(std::make_pair("cs_cz", "Czech"));
   LanguageMap.insert(std::make_pair("da_dk", "Danish"));
   LanguageMap.insert(std::make_pair("de_de", "German"));
   LanguageMap.insert(std::make_pair("es_es", "Spanish"));
   LanguageMap.insert(std::make_pair("fi_fi", "Finnish"));
   LanguageMap.insert(std::make_pair("fr_fr", "French"));
   LanguageMap.insert(std::make_pair("hu_hu", "Hungarian"));
   LanguageMap.insert(std::make_pair("it_it", "Italian"));
   LanguageMap.insert(std::make_pair("ja_jp", "Japanese"));
   LanguageMap.insert(std::make_pair("ka_ge", "Georgian"));
   LanguageMap.insert(std::make_pair("ko_kr", "Korean"));
   LanguageMap.insert(std::make_pair("nb_no", "Norwegian"));
   LanguageMap.insert(std::make_pair("nl_nl", "Dutch"));
   LanguageMap.insert(std::make_pair("pl_pl", "Polish"));
   LanguageMap.insert(std::make_pair("pt_br", "BrazilianPortuguese"));
   LanguageMap.insert(std::make_pair("pt_pt", "Portuguese"));
   LanguageMap.insert(std::make_pair("ro_ro", "Romanian"));
   LanguageMap.insert(std::make_pair("ru_ru", "Russian"));
   LanguageMap.insert(std::make_pair("sl_si", "Slovenian"));
   LanguageMap.insert(std::make_pair("zh_cn", "ChineseTrad"));
}

std::vector<std::string> GetLangNames(const std::string& localedir)
{
   TDEBUG_ENTER("GetLangNames");
   std::vector<std::string> files;
   GetDirectoryContents(localedir, &files, 0);
   std::vector<std::string> langNames;
   for (std::vector<std::string>::iterator it = files.begin(); it != files.end(); ++it)
   {
      TDEBUG_TRACE("file " << *it);
      if (GetExtension(*it) == "mo")
      {
         TDEBUG_TRACE("mo " << *it);
         std::string lang = GetNameBody(*it);
         langNames.push_back(lang);
         std::string src = EnsureTrailingDelimiter(localedir);
         src += lang;
         src += ".mo";
         std::string dest = EnsureTrailingDelimiter(localedir);
         dest += "Locale\\";
         dest += EnsureTrailingDelimiter(lang);
         dest += "LC_MESSAGES";
         int res = SHCreateDirectoryExA(0, dest.c_str(), 0);
         if ((res != ERROR_SUCCESS) && (res != ERROR_ALREADY_EXISTS))
         {
            std::cerr << "Failed to create folder '" << dest << "': " << GetWin32ErrorMessage(res) << std::endl;
            exit(1);
         }
         dest += "\\Setup.mo";
         if (!CopyFileA(src.c_str(), dest.c_str(), FALSE))
         {
            std::cerr << "Failed to copy '" << *it << "' to '" << dest << "': "
                      << GetLastError() << std::endl;
            exit(1);
         }
         TDEBUG_TRACE("Processed " << lang);
      }
   }
   return langNames;
}
