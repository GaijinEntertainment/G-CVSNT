// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - October 2000

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
#include "TortoisePreferences.h"
#include <Utils/LangUtils.h>
#include <Utils/PathUtils.h>
#include <Utils/StringUtils.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/Translate.h>
#include <Utils/UnicodeStrings.h>
#include <Utils/ShellUtils.h>


TortoisePreferences::TortoisePreferences()
{
   CreateMainPreferences();
   CreateAdvancedPreferences();
   CreateToolsPreferences();
   CreatePolicyPreferences();
   CreateEditPreferences();
   CreateDebugPreferences();
   CreateAppearancePreferences();
   CreateCachePreferences();

   // Set page order
   AddPage(_("Main"));
   AddPage(_("Policy"));
   AddPage(_("Edit"));
   AddPage(_("Tools"));
   AddPage(_("Advanced"));
   AddPage(_("Appearance"));
   AddPage(_("Cache"));
#if defined(_DEBUG) || defined (FORCE_DEBUG)
   AddPage(_("Debug"));
#endif
    
   // Read initial values from registry
   for (unsigned int i = 0; i < myPrefs.size(); ++i)
      myPrefs[i]->ReadFromRegistry();
}


void TortoisePreferences::SetScope(const std::string& abbreviatedRoot)
{
   myAbbreviatedRoot = abbreviatedRoot;
   std::string* cvsroot = 0;
   if (!myAbbreviatedRoot.empty())
      cvsroot = &myAbbreviatedRoot;
   for (unsigned int i = 0; i < myPrefs.size(); ++i)
      myPrefs[i]->ReadFromRegistry(cvsroot);
}


TortoisePreferences::~TortoisePreferences()
{
    TDEBUG_ENTER("~TortoisePreferences");
    
   // Write changed values to registry
   std::string* cvsroot = 0;
   if (!myAbbreviatedRoot.empty())
      cvsroot = &myAbbreviatedRoot;
   for (unsigned int i = 0; i < myPrefs.size(); ++i)
   {
      myPrefs[i]->WriteToRegistry(cvsroot);
      delete myPrefs[i];
   }

   // Write appevents name
   TortoiseRegistry::WriteString("\\HKEY_CURRENT_USER\\AppEvents\\EventLabels\\TCVS_Error\\",
                                 _("Error"));                
   TortoiseRegistry::WriteString("\\HKEY_CURRENT_USER\\AppEvents\\Schemes\\Apps\\TortoiseAct\\", 
                                 _("TortoiseCVS"));

   // Write default values for app events
   std::vector<std::string> schemes;
   std::vector<std::string>::const_iterator it;
   TortoiseRegistry::ReadKeys("\\HKEY_CURRENT_USER\\AppEvents\\Schemes\\Names", schemes);
   it = schemes.begin();
   bool bExists = false;
   while (it != schemes.end())
   {
      if (stricmp(it->c_str(), ".None") != 0)
      {
         std::string key = "\\HKEY_CURRENT_USER\\AppEvents\\Schemes\\Apps\\TortoiseAct\\TCVS_Error\\";
         key += *it + "\\";
         TortoiseRegistry::ReadString(key, "", &bExists);
         if (!bExists) 
         {
            TortoiseRegistry::WriteString(key, GetTortoiseDirectory() + "TortoiseCVSError.wav");
         }
      }

      it++;
   }

   // Set current scheme
   std::string currentScheme = TortoiseRegistry::ReadString("\\HKEY_CURRENT_USER\\AppEvents\\Schemes\\");
   if (!currentScheme.empty())
   {
      std::string key = "\\HKEY_CURRENT_USER\\AppEvents\\Schemes\\Apps\\TortoiseAct\\TCVS_Error\\.Current\\";
      TortoiseRegistry::ReadString(key, "", &bExists);
      if (!bExists) 
      {
         if (stricmp(currentScheme.c_str(), ".None") != 0)
         {
            TortoiseRegistry::WriteString(key, GetTortoiseDirectory() + "TortoiseCVSError.wav");
         }
         else
         {
            TortoiseRegistry::WriteString(key, "");
         }
      }
   }
}

size_t TortoisePreferences::GetCount()
{
   return myPrefs.size();
}

Preference& TortoisePreferences::Lookup(int index)
{
   return *myPrefs[index];
}


Preference& TortoisePreferences::Lookup(const std::string& regKey)
{
   std::vector<Preference*>::iterator it = myPrefs.begin();
   std::string sRegKey = regKey;
   std::string s;
   MakeLowerCase(sRegKey);
   while (it != myPrefs.end())
   {  
      s = (*it)->myRegistryKey;
      MakeLowerCase(s);
      if (s == sRegKey)
         break;
      it++;
   }
   return **it;
}


const std::vector<wxString>& TortoisePreferences::GetPageOrder() const
{
   return myPageOrder;
}


void TortoisePreferences::AddPage(const wxString& page)
{
   myPageOrder.push_back(page);
}


void TortoisePreferences::CreateMainPreferences()
{
    TDEBUG_ENTER("CreateMainPreferences");

   // Fill in the details for each preference
   Preference* closeIfNoError = new Preference(_("Main"), _("&Progress dialog"), 
_("Automatically close the progress window when a command completes \
successfully.  You can hold Ctrl down while selecting a CVS \
command, or as one is finishing, to prevent this on a one off basis."), 
                                               "Close Automatically");
   std::vector<wxString> closePossibilities;
   closePossibilities.push_back(_("Close manually"));
   closePossibilities.push_back(_("Close if no messages"));
   closePossibilities.push_back(_("Close if no errors"));
   closeIfNoError->SetIntEnumeration(closePossibilities, 0);
   myPrefs.push_back(closeIfNoError);
    
   Preference* quietness = new Preference(_("Main"), _("Progress &messages"),
_("How detailed CVS is about what it is doing.  Errors are always printed, \
even at the most silent levels."), "Quietness");
   std::vector<wxString> quietnessPossibilities;
   quietnessPossibilities.push_back(_("Loud"));
   quietnessPossibilities.push_back(_("Quiet"));
   quietnessPossibilities.push_back(_("Really Quiet"));
   quietness->SetIntEnumeration(quietnessPossibilities, 1);
   myPrefs.push_back(quietness);

   Preference* icons = new Preference(_("Main"), _("&Overlay icons"),
                                      _("The set of overlay icons TortoiseCVS uses."), "Icons");
   icons->SetMessage(_("Changes won't take effect until you log off or reboot."));
   std::vector<std::string> iconPossibilities;
   iconPossibilities.push_back("");
   std::string iconPath(GetSpecialFolder(CSIDL_PROGRAM_FILES_COMMON));
   iconPath = EnsureTrailingDelimiter(iconPath) + "TortoiseOverlays/icons";
   std::vector<std::string> iconDirs;
   GetDirectoryContents(iconPath, 0, &iconDirs);
   for (std::vector<std::string>::iterator it = iconDirs.begin(); it != iconDirs.end(); ++it)
       iconPossibilities.push_back(*it);
   icons->SetIconSetEnumeration(iconPossibilities, "TortoiseCVS");
   myPrefs.push_back(icons);

   std::vector<wxString> changedDirsOptions;
   changedDirsOptions.push_back(_("Don't change icon."));
   changedDirsOptions.push_back(_("Change icon when folder contains changed files."));
   changedDirsOptions.push_back(_("Change icon when folder or subfolders contain changed files."));
    
   Preference* showChangedDirs = new Preference(_("Main"), _("Folder overlay icons"),
                                                _("Change folder icon when folder or subfolders contain changed files."), 
                                                "Recursive folder overlays");
   showChangedDirs->SetIntEnumeration(changedDirsOptions, 1);
   myPrefs.push_back(showChangedDirs);


   Preference* autoloadIcons = new Preference(_("Main"), _("Autoload changed folder icons"),
_("Enabled: Changed folder icons are updated automatically.\n\
Disabled: Changed folder icons are only updated explicitly through the context menu." ),
                                              "Autoload folder overlays");
   autoloadIcons->SetBoolean(false, _("Autoload folder icon overlays"));
   myPrefs.push_back(autoloadIcons);



   Preference* language = new Preference(_("Main"), _("L&anguage"),
                                         _("The language used in TortoiseCVS dialogs and menus."),
                                         "LanguageIso");

   std::vector<wxString> langNames;
   std::vector<std::string> langKeys;
   std::map<wxString, std::string> langMap;
   langMap[L"English"] = "en_GB";
   
   TortoiseRegistry::ReadKeys("Languages", langKeys);

   std::vector<std::string>::iterator it = langKeys.begin();
   while (it != langKeys.end())
   {
       std::string langKey(*it++);
       std::string subKey = "Languages\\" + langKey + "\\";
       std::string enabled = subKey  + "Enabled";
       if (!TortoiseRegistry::ReadBoolean(enabled, false))
           continue;
       std::string name = subKey + "Name";
       std::string langName = TortoiseRegistry::ReadString(name);
       langMap[Utf8ToUcs2(langName)] = langKey;
   }
   langKeys.clear();
   // First entry
   langKeys.push_back("");
   langNames.push_back(_("Windows default"));
   std::map<wxString, std::string>::iterator mapIter = langMap.begin();
   while (mapIter != langMap.end())
   {
       langKeys.push_back(mapIter->second);
       langNames.push_back(mapIter->first);
       ++mapIter;
   }
   TDEBUG_TRACE("Names " << langNames.size() << " Keys " << langKeys.size());

   language->SetStrKeyEnumeration(langNames, langKeys, "");
   myPrefs.push_back(language);
}


void TortoisePreferences::CreateAdvancedPreferences()
{
   std::string CalculatedHomeDirectory;
   bool GotHomeDir = GetCalculatedHomeDirectory(CalculatedHomeDirectory);

   if (!GotHomeDir)
      CalculatedHomeDirectory = wxAscii(_("<failed to calculate home folder>"));

   Preference* recalculateHome = new Preference(_("Advanced"), _("Home folder"), 
_("The home folder is where the global .cvsignore file is stored. \
SSH connections also expect your .ssh folder with keys and known \
hosts to be in your home folder.\n\
By default, TortoiseCVS works out a home folder for you. \
It looks for folders in this order:\n\
 - An environment variable called HOME\n\
 - WinCVS specified home folder\n\
 - My Documents.\n\
You can turn off this automatic calculation, and instead \
specify your own custom home folder below.\n\
Currently the calculation result is: ") + wxText(CalculatedHomeDirectory),
                                                "Always Recalculate Home");
   recalculateHome->SetBoolean(true, _("Always recalculate &home folder"));
   myPrefs.push_back(recalculateHome);
    

   Preference* homeDirectory = new Preference(_("Advanced"), _("Custom home &folder"), 
_("If you turn off automatic home folder calculation, you can enter an explicit path here."), "HOME");
   homeDirectory->SetGhostNegative(static_cast<int>(myPrefs.size()) - 1);
   homeDirectory->SetDirectory(CalculatedHomeDirectory);
   myPrefs.push_back(homeDirectory);
    

   Preference* compression = new Preference(_("Advanced"), _("&Network compression"), 
_("Reduce network load by compressing traffic to and from the CVS server. High levels may be slow to calculate."),
                                            "Compression Level");
   std::vector<wxString> compressionPossibilities;
   compressionPossibilities.push_back(_("0 - None"));
   compressionPossibilities.push_back(_("3 - Low"));
   compressionPossibilities.push_back(_("6 - Good"));
   compressionPossibilities.push_back(_("9 - Best"));
   compression->SetIntEnumeration(compressionPossibilities, 0);
   myPrefs.push_back(compression);

   // Exclude directories from TortoiseCVS
   Preference* excludeDirs = new Preference(_("Advanced"), _("&Excluded folders"),
_("Semicolon-delimited list of (absolute) folders for which overlay icons and context menus are disabled."), 
                                            "Excluded Directories", true);
   excludeDirs->SetPath("");
   myPrefs.push_back(excludeDirs);


   // Include directories for TortoiseCVS
   Preference* includeDirs = new Preference(_("Advanced"), _("&Included folders"),
_("Semicolon-delimited list of (absolute) folders for which overlay icons and \
context menus are enabled. An empty list enables all folders."), 
                                            "Included Directories", true);
   includeDirs->SetPath("");
   myPrefs.push_back(includeDirs);


   Preference* menuIcons = new Preference(_("Advanced"), _("Menu icons"),
                                          _("Enable/disable"),
                                          "ContextIcons");
   menuIcons->SetBoolean(true, _("Enable menu &icons"));
   myPrefs.push_back(menuIcons);
   
   Preference* graphMaxTags = new Preference(_("Advanced"),
                                             _("Maximum number of tags for graph"),
_("Controls how many tags the graph dialog will display per revision.  Enter -1 to show them all."),
                                             "GraphMaxTags");
   graphMaxTags->SetString("-1");
   myPrefs.push_back(graphMaxTags);

   std::vector<wxString> sandboxTypeOptions;
   sandboxTypeOptions.push_back(_("Autodetect (default to DOS)"));
   sandboxTypeOptions.push_back(_("Autodetect (default to UNIX)"));
   sandboxTypeOptions.push_back(_("DOS"));
   sandboxTypeOptions.push_back(_("UNIX"));

   Preference* sandboxType = new Preference(_("Advanced"), _("Sandbox DOS/UNIX"),
                                            _("Use either LF (UNIX) or CRLF (DOS) line separators in text files."), 
                                            "SandboxType");
   sandboxType->SetIntEnumeration(sandboxTypeOptions, 0);
   myPrefs.push_back(sandboxType);
}
    

void TortoisePreferences::CreateToolsPreferences()
{
   Preference* externalDiff = new Preference(_("Tools"), _("&Diff application"),
                                             _("Specify the program you use to compare files."),
                                             "External Diff Application");
   externalDiff->SetFile("?", wxString(_("Executables")) + wxString(wxT("(*.exe;*.bat;*.cmd)|*.exe;*.bat;*.cmd")));
   myPrefs.push_back(externalDiff);


   Preference* externalDiff2Params = new Preference(_("Tools"), _("&Two-way diff parameters"),
                                                    _("Parameters for two-way diffs.")
                                                    + wxString(wxT(" "))
                                                    + _("The default parameters are:")
                                                    + wxString(wxT("\n\n\"%1\" \"%2\"\n"))
                                                    + wxString(wxT("\n%1: ")) + _("Name of file selected for diff.")
                                                    + wxString(wxT("\n%2: ")) + _("Name of file selected for diff."),
                                                    "External Diff2 Params");
   externalDiff2Params->SetString("\"%1\" \"%2\"");
   myPrefs.push_back(externalDiff2Params);
    
    
#if 0 // Not ready to use yet
   Preference* externalDiff3Params = new Preference(_("Tools"), _("T&hree-way diff parameters"),
                                                    _("Parameters for three-way diffs.")
                                                    + wxString(wxT(" "))
                                                    + ("The default parameters are:")
                                                    + wxString(wxT("\n\n\"%1\" \"%2\"\n"))
                                                    + wxString(wxT("\n%1: "))
                                                    + wxString(_("Name of file selected for diff."))
                                                    + wxString(wxT("\n%2: "))
                                                    + wxString(_("Name of file selected for diff."))
                                                    + wxString(wxT("\n%3: "))
                                                    + wxString(_("Name of file selected for diff.")),
                                                    "External Diff3 Params");
   externalDiff3Params->SetString("\"%1\" \"%2\" \"%3\"");
   myPrefs.push_back(externalDiff3Params);
#endif

    
   Preference* externalMerge = new Preference(_("Tools"), _("&Merge application"),
                                              _("Specify the program you use to merge files."),
                                              "External Merge Application");
   externalMerge->SetFile("?", wxString(_("Executables")) + wxString(wxT("(*.exe;*.bat;*.cmd)|*.exe;*.bat;*.cmd")));
   myPrefs.push_back(externalMerge);

   Preference* externalMerge2Params = new Preference(_("Tools"), _("&Two-way merge parameters"),
                                                     wxString(_("Parameters for two-way merges."))
                                                     + wxString(wxT(" ")) 
                                                     + wxString(_("The default parameters are:"))
                                                     + wxString(wxT("\n\n\"%mine\" \"%yours\"\n"))
                                                     + wxString(wxT("\n%mine: ")) + _("File containing my changes.")
                                                     + wxString(wxT("\n%yours: "))
                                                     + wxString(_("File containing changes from repository.")),
                                                     "External Merge2 Params");
   externalMerge2Params->SetString("\"%mine\" \"%yours\"");
   myPrefs.push_back(externalMerge2Params);

   
#if 0 // Not ready to use yet
   Preference* externalMerge3Params = new Preference(_("Tools"), _("T&hree-way merge parameters"),
                                                     wxString(_("Parameters for three-way merges."))
                                                     + wxString(wxT(" ")) 
                                                     + ("The default parameters are:")
                                                     + wxString(_("\n\n\"%mine\" \"%ancestor\" \"%yours\"\n"))
                                                     + wxString(_("\n%mine: "))
                                                     + wxString(_("File containing my changes."))
                                                     + wxString(_("\n%ancestor: "))
                                                     + wxString(_("File containing common ancestor data."))
                                                     + wxString(_("\n%yours: ")
                                                     + wxString(_("File containing changes from repository.")),
                                                     "External Merge3 Params");
   externalMerge3Params->SetString("\"%mine\" \"%ancestor\" \"%yours\"");
   myPrefs.push_back(externalMerge3Params);
#endif

   Preference* rshApp = new Preference(_("Tools"), _("SSH &application (:ext: only)"),
                                       _("Specify the CVS_EXT application (e.g. ssh.exe, plink.exe)"),
                                       "External SSH Application");
   rshApp->SetFile(EnsureTrailingDelimiter(GetTortoiseDirectory()) 
                   + "TortoisePlink.exe",
                   wxString(_("Executables")) + wxString(wxT(" (*.exe;*.bat;*.cmd)|*.exe;*.bat;*.cmd)")));
   myPrefs.push_back(rshApp);

   Preference* rshParams = new Preference(_("Tools"), _("SSH &parameters (:ext: only)"),
                                          wxString(_("Parameters for the CVS_EXT application. The default parameters are:")) +
                                          wxString(wxT("\n-l \"%u\" \"%h\"\n")) + 
                                          wxString(_("CVS replaces %u with the username and %h with the hostname.")),
                                          "External SSH Params");
   rshParams->SetString("-l \"%u\" \"%h\"");
   myPrefs.push_back(rshParams);

   Preference* rshEnv = new Preference(_("Tools"), _("SSH &cvs server (:ext: and :ssh:)"),
                                          wxString(_("Specify the CVS_SERVER application (e.g. /usr/local/bin/cvsnt)")),
                                          "Server CVS Application");
   rshEnv->SetString("cvs");

   myPrefs.push_back(rshEnv);

   Preference* bugtracker = new Preference(_("Tools"), _("&Bug tracker URL"),
_("In the History and Revision Graph dialogs, TortoiseCVS can \
automatically create hyperlinks from log messages containing \
'bug XXX' or '#XXX' where XXX is a number. This setting \
tells TortoiseCVS how to build that URL. %s will be replaced \
with the bug number."),
                                           "Bug tracker URL");
   bugtracker->SetString("");
   myPrefs.push_back(bugtracker);
}


void TortoisePreferences::CreatePolicyPreferences()
{
   Preference* pruneEmptyDirs =
      new Preference(_("Policy"), _("Empty folders"),
                     _("Hide folders which contain no files. CVS doesn't directly manage folders, \
so the assumption here is that if you remove all the files from a folder you \
would like to remove the folder."),
                     "Prune Empty Directories");
   pruneEmptyDirs->SetBoolean(true, _("Prune &empty folders"));
   myPrefs.push_back(pruneEmptyDirs);

#ifdef MARCH_HARE_BUILD
   Preference* readOnlyDefault =
      new Preference(_("Policy"), _("Read Only Default"),
                     _("Checkout files read only by default, return files to read only with unedit and commit"),
                     "Checkout files Read Only");
   readOnlyDefault->SetBoolean(true, _("Read &Only Default"));
   myPrefs.push_back(readOnlyDefault);
#endif
   
   Preference* createDirs =
      new Preference(_("Policy"),
                     _("New folders"),
                     _("Create any folders that exist in the repository if they are missing from the working folder."),
                     "Create Added Directories");
   createDirs->SetBoolean(true, _("Create new &folders on update"));
   myPrefs.push_back(createDirs);
    
   Preference* mostRecentRevision =
      new Preference(_("Policy"),
                     _("Force head revision"),
                     _("If you've specified a tag/branch/date on checkout or update, \
and it matches no revision for a file, CVS will get you the head revision instead."),
                     "Force Head Revision");
   mostRecentRevision->SetBoolean(false, _("If no matching revision is found, use the most recent one"));
   myPrefs.push_back(mostRecentRevision);

   Preference* autoCommit =
      new Preference(_("Policy"), 
                     _("Automatic commit"),
                     _("If this is checked and you perform a 'CVS Add' and/or a 'CVS Remove', \
TortoiseCVS will automatically perform a commit of the file.\n\
It is recommended that you only enable this option if you work on documents, \
not source code, or if you are the only person working on the project."),
                     "Automatic commit");
   std::vector<wxString> autoCommitPossibilities;
   autoCommitPossibilities.push_back(_("Do not automatically commit after Add/Remove"));
   autoCommitPossibilities.push_back(_("Automatically commit after Add"));
   autoCommitPossibilities.push_back(_("Automatically commit after Add and Remove"));
   autoCommit->SetIntEnumeration(autoCommitPossibilities, 0);
   myPrefs.push_back(autoCommit);

   // Control whether to enable the shell extension functions on remote network shares
   Preference* allowNetworkDrives =
      new Preference(_("Policy"),
                     _("Allow Network Drives"),
                     _("If this is checked, sandboxes on network mounted drives \
will be controlled by TortoiseCVS. It also suppresses \
CVS errors when accessing a repository via :local: on \
a network share.\n\
NOTE: Placing your sandbox or repository on a network drive is known to cause many problems \
and is discouraged by the CVS developers."),
                     "Allow Network Drives");
   allowNetworkDrives->SetBoolean(false, _("Permit sandboxes and repositories on &network (remote) drives"));
   myPrefs.push_back(allowNetworkDrives);

   // Control whether to enable the shell extension functions on removable
   Preference* allowRemovableDrives =
      new Preference(_("Policy"),
                     _("Allow Removable Drives"),
                     _("If this is checked, sandboxes on drives with removable media \
will be controlled by TortoiseCVS.\n\
NOTE: Placing your sandbox on a removable drive is known to cause many problems \
and is discouraged by the CVS developers."),
                     "Allow Removable Drives");
   allowRemovableDrives->SetBoolean(false, _("Permit sandboxes on &removable drives"));
   myPrefs.push_back(allowRemovableDrives);

   // Enable/disable inclusion of ignored files in Add Contents dialog
   Preference* includeIgnored =
      new Preference(_("Policy"),
                     _("Add Ignored Files"),
                     _("If this is checked, the Add Contents dialog will include files listed in \
.cvsignore. Useful if you want to add files which are normally ignored, \
but makes the dialog more cluttered."),
                     "Add Ignored Files");
   includeIgnored->SetBoolean(false, _("Show .cvs&ignored files in the Add Contents dialog"));
   myPrefs.push_back(includeIgnored);
   
   // Always show CVS columns in explorer
   Preference* hideColumns =
      new Preference(_("Policy"),
                     _("Explorer Columns"),
                     _("If this is checked, Tortoise will show the CVS Explorer columns \
only for folders that are under CVS control"),
                     "Hide Columns");
   hideColumns->SetBoolean(true, _("Show columns only for CVS-controlled folders"));
   myPrefs.push_back(hideColumns);

   // Issue a reminder if no commit comment provided
   Preference* nagIfNoCommitComment =
      new Preference(_("Policy"),
                     _("Empty comment check"),
                     _("If this is checked, the Commit dialog will issue a warning if no commit comment has \
been provided by the user.  You can then either continue or abort the commit operation."),
                     "Warn On Missing Commit Comment");
   nagIfNoCommitComment->SetBoolean(true, _("Issue &warning if no comment provided in commit dialog"));
   myPrefs.push_back(nagIfNoCommitComment);
}

void TortoisePreferences::CreateEditPreferences()
{
   std::vector<wxString> editPolicyOptions;
   editPolicyOptions.push_back(_("Do not use Edit/Unedit"));                     // 0
   editPolicyOptions.push_back(_("Concurrent editing"));                         // 1
   editPolicyOptions.push_back(_("Concurrent or exclusive editing (CVSNT servers only)"));            // 2 
   editPolicyOptions.push_back(_("Exclusive editing only (CVSNT servers only)"));                     // 3
   editPolicyOptions.push_back(_("Exclusive editing and set ACL (CVSNT servers only)"));              // 4
   
   Preference* editPolicy =
      new Preference(_("Edit"), 
                     _("Edit policy"),
                     _("How to handle the CVS Edit and Unedit commands."),
                     "Edit policy");
   editPolicy->SetIntEnumeration(editPolicyOptions, 0);
   myPrefs.push_back(editPolicy);
   // Save for ghosting
   int editPolicyIndex = static_cast<int>(myPrefs.size()) - 1;

   Preference* showOptEdit = new Preference(_("Edit"),
                                            _("Edit options dialog"),
                                            _("Whether to show the edit options dialog" ),
                                            "Show edit options");
   showOptEdit->SetGhostIfLarger(editPolicyIndex, 0);
   showOptEdit->SetBoolean(false, _("Show edit &options dialog"));
   myPrefs.push_back(showOptEdit);

   Preference* autoUnedit =
      new Preference(_("Edit"), 
                     _("Automatic unedit"),
                     _("If this is checked and you run 'CVS Commit', \
TortoiseCVS will do a 'cvs unedit' in order to remove any edits you may have on files which you have not modified. \
NOTE that this requires the use of 'cvs watch on' for all files!"),
                     "Automatic unedit");
   autoUnedit->SetGhostIfLarger(editPolicyIndex, 0);
   autoUnedit->SetBoolean(false, _("Automatically &unedit after Commit"));
   myPrefs.push_back(autoUnedit);
}

void TortoisePreferences::CreateDebugPreferences()
{
#if defined(_DEBUG) || defined (FORCE_DEBUG)
   Preference* debugTortoiseAct = new Preference(_("Debug"), 
                                                 _("Main program"), 
                                                 _("Enable debug logs for TortoiseCVS main program"), 
                                                 "Debug TortoiseAct");
   debugTortoiseAct->SetBoolean(false, _("Enable debug logs"));
   myPrefs.push_back(debugTortoiseAct);

   Preference* debugTortoiseShl = new Preference(_("Debug"), 
                                                 _("Shell extension"), 
                                                 _("Enable debug logs for TortoiseCVS shell extension"), 
                                                 "Debug TortoiseShell");
   debugTortoiseShl->SetBoolean(false, _("Enable debug logs"));
   myPrefs.push_back(debugTortoiseShl);

   // Enable / disable debug logs
   Preference* enableDebug = new Preference(_("Debug"),
                                            _("Debug logs"),
                                            _("Write TortoiseCVS debug information into the folder specified below."),
                                            "Debug Logs");
   std::vector<wxString> debugLogOptions;
   debugLogOptions.push_back(_("Don't save debug logs"));
   debugLogOptions.push_back(_("Save debug logs"));
   debugLogOptions.push_back(_("Save debug logs, no log file caching"));
   enableDebug->SetIntEnumeration(debugLogOptions, 0);
   myPrefs.push_back(enableDebug);

   Preference* logDirectory = new Preference(_("Debug"), _("Debug log &folder"), 
                                             _("Debug log files will be created in this folder."), "Log Directory");
   logDirectory->SetDirectory(GetTemporaryPath());
   myPrefs.push_back(logDirectory);

   Preference* clientLog = new Preference(_("Debug"), _("CVSNT logging"), 
                                          _("Makes files cvsclient.log.in and cvsclient.log.out \
in the TortoiseCVS folder for debugging of the CVS protocol."),
                                          "Client Log");
   clientLog->SetBoolean(false, _("Create CVSNT client &debug logs"));
   myPrefs.push_back(clientLog);

#endif
}


void TortoisePreferences::CreateAppearancePreferences()
{
   // Colours
   Preference* colourNoise = new Preference(_("Appearance"),
                                            _("Noise text colour"),
                                            _("Colour for noise text in the progress dialog"),
                                            "Colour Noise Text");
   colourNoise->SetColor(RGB(0x60, 0x60, 0x60), _("Colour for noise text in the progress dialog"));
   myPrefs.push_back(colourNoise);

   Preference* colourWarning = new Preference(_("Appearance"),
                                              _("Warning text colour"),
                                              _("Colour for warning text in the progress dialog"),
                                              "Colour Warning Text");
   colourWarning->SetColor(RGB(0xFF, 0x80, 0x80), _("Colour for warning text in the progress dialog"));
   myPrefs.push_back(colourWarning);

   Preference* colourUpdated = new Preference(_("Appearance"),
                                              _("Updated files colour"),
                                              _("Colour for updated files in the progress dialog"),
                                              "Colour Updated Files");
   colourUpdated->SetColor(RGB(0x00, 0xA0, 0x00), _("Colour for updated files in the progress dialog"));
   myPrefs.push_back(colourUpdated);

   Preference* colourModified = new Preference(_("Appearance"),
                                               _("Modified files colour"),
                                               _("Colour for modified files in the progress dialog"),
                                               "Colour Modified Files");
   colourModified->SetColor(RGB(0xFF, 0x00, 0xFF), _("Colour for updated files in the progress dialog"));
   myPrefs.push_back(colourModified);

   Preference* colourConflict = new Preference(_("Appearance"),
                                               _("Conflict files colour"),
                                               _("Colour for conflict files in the progress dialog"),
                                               "Colour Conflict Files");
   colourConflict->SetColor(RGB(0xFF, 0x00, 0x00), _("Colour for conflict files in the progress dialog"));
   myPrefs.push_back(colourConflict);
   
   Preference* colourIgnored = new Preference(_("Appearance"),
                                              _("Files not in CVS colour"),
                                              _("Colour for files not in CVS in the progress dialog"),
                                              "Colour Not In CVS Files");
   colourIgnored->SetColor(RGB(0x60, 0x60, 0x00), _("Colour for files not in CVS in the progress dialog"));
   myPrefs.push_back(colourIgnored);

   Preference* colourError = new Preference(_("Appearance"),
                                            _("Error text colour"),
                                            _("Colour for error text in the progress dialog"),
                                            "Colour Error Text");
   colourError->SetColor(RGB(0xFF, 0x00, 0x00), _("Colour for error text in the progress dialog"));
   myPrefs.push_back(colourError);

   Preference* colourTip = new Preference(_("Appearance"),
                                          _("Tip text colour"),
                                          _("Colour for tip text"),
                                          "Colour Tip Text");
   colourTip->SetColor(RGB(0x00, 0x00, 0xFF), _("Colour for tip text"));
   myPrefs.push_back(colourTip);
}


void TortoisePreferences::CreateCachePreferences()
{
   Preference* serverVersionCache = new Preference(_("Cache"),
                                                   _("Server version"),
                                                   _("Clear the cache that stores the CVS server version"),
                                                   "Cache\\ServerVersions");
   serverVersionCache->SetCache(_("Clear cache"));
   myPrefs.push_back(serverVersionCache);
    
   Preference* moduleListCache = new Preference(_("Cache"),
                                                _("Module list"),
                                                _("Clear the cache that stores the CVS module list"),
                                                "Cache\\Modules");
   moduleListCache->SetCache(_("Clear cache"));
   myPrefs.push_back(moduleListCache);
    
   Preference* branchTagListCache = new Preference(_("Cache"),
                                                   _("Branch/Tag list"),
                                                   _("Clear the cache that stores the CVS branches and tags"),
                                                   "Cache\\BranchesTags");
   branchTagListCache->SetCache(_("Clear cache"));
   myPrefs.push_back(branchTagListCache);
}
