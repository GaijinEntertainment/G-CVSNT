// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - May 2000

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

#ifndef CVS_ACTION_H
#define CVS_ACTION_H

#include <string>
#include <map>
#include <stdio.h>
#include <wx/wx.h>
#include <map>
#include "MakeArgs.h"
#include "../Utils/SyncUtils.h"
#include "../Utils/Thread.h"
#include "../Utils/SyncUtils.h"
#include "CVSRoot.h"
#include "CVSStatus.h"
#include "../cvsgui/cvsgui_process.h"
#include "../DialogsWxw/YesNoAllDialog.h"

class ProgressDialog;
class ThreadSafeProgress;

class CVSAction
{
   friend long ConsoleOut(const char* txt, long len, const CvsProcess* process);
   friend long ConsoleErr(const char* txt, long len, const CvsProcess* process);
   friend const char* GetEnv(const char* name, const CvsProcess* process);
   friend void Exit(int code, const CvsProcess* process);
   friend void OnDestroy(const CvsProcess* process);
   friend class CVSStatus;

public:
   // The progress dialog appears for the duration of an instantiation of CVSAction
   enum
   {
      PIPE_STDOUT = 0x0001,
      HIDE_PROGRESS = 0x0002,
      SILENT_ON_ERROR = 0x0004
   };
   CVSAction(wxWindow* parent, int flags = 0);
   ~CVSAction();
   bool Command(const std::string& currentDirectory, MakeArgs& args);
   void SetCVSRoot(const CVSRoot& cvsroot);
   inline CVSRoot GetCVSRoot() { return myCVSRootHolder; }
   // You don't need to call this - the user will be prompted if you don't
   void SetPassword(const std::string& password); 

   void SetProgressCaption(const wxString& msg);
   void SetProgressFinishedCaption(const wxString& msg);
   void SetStreamFile(const std::string& file);
   void SetCloseIfOK(bool closeIfOK);
   void SetCloseAnyway(bool closeIfOK);
   void SetHideStdout(bool hide = true);
   inline bool GetHideStdout() { return !myShowStdout; }
   void SetSilent(bool silent);

   enum close_t
   {
      CloseAlways       = 3,
      CloseIfNoErrors   = 2,
      CloseIfNoMessages = 1,
      CloseNever        = 0
   };

   enum show_ui_t 
   {
       ShowPasswordDialog,
       ShowYesNoDialog
   };

   close_t GetClose() const;
   void SetClose(close_t how);

   void CloseConsoleOutput();
   
   bool AtLeastOneSuccess();
   bool Aborted();
   const std::list<std::string>& GetStdOutList();
   const std::list<std::string>& GetStdErrList();
   const std::list<std::string>& GetOutputList();
   std::string GetStdOutText(bool clear = false);
   std::string GetStdErrText();
   std::string GetOutputText();
   void LockProgressDialog(bool lock = true);

   ProgressDialog* GetProgressDialog();

private:
   class ConsoleOutputLine
   {
   public:
      ConsoleOutputLine(const char* txt, size_t len, bool stdErr)
         : m_Text(txt, len), m_StdErr(stdErr) { }
      // The text
      std::string m_Text;
      // Stderr output
      bool m_StdErr;
   };

    struct GetPasswordData
    {
        std::string password;
        bool doCancel;
    };
    
    struct YesNoData
    {
        wxString    question;
        bool        withCancel;
        std::string answer;
    };

    
   int                          myFlags;
   CvsProcess*                  myCvsProcess;

   CriticalSection              myCsOutput;
   std::list<ConsoleOutputLine> myConsoleOutput;
   std::string                  myStderrLine;
   std::string                  myStdoutLine;

   HANDLE myStreamHandle;
   HANDLE myAbortEvent;
   HANDLE myAddedToConsoleBufferEvent;
   HANDLE myRemovedFromConsoleBufferEvent;

   wxWindow* myParent;
   CVSStatus::CVSVersionInfo myServerVersion;

   std::map<std::string, std::string> myMapEnvVars;
   
   bool InternalCommand(const std::string& currentDirectory, MakeArgs& args);
   bool DoCommand(const std::string& currentDirectory, MakeArgs args);
   bool DoCommandThread(const std::string& currentDirectory, MakeArgs args);
   std::string GetLibName(const std::string& currentDirectory);
   void ThreadMain();
   bool HandleUserInput(YesNoData& yesNoData);
   std::string HandleUserInput(std::string& cvsOutput);
   void HandleUiEvent();
   bool NeedsLogin() const;
   bool NeedsInit() const;
   bool BadLockServerVersion() const;
   bool SSPIEncryptionFailed() const;

   void PipeToGUI();
   void FlushToGUI();

   // Get cvs stdout
   long ConsoleOut(const char* txt, long len);
   // Process Stdout line
   void ProcessStdoutLine(const std::string& line);
   // Get cvs stderr
   long ConsoleErr(const char* txt, long len);
   // Process Stderr line
   void ProcessStderrLine(const std::string& line);
   // Ask for env. variable
   const char* GetEnv(const char* name);

   // check if a store contains a string
   bool StoreContains(const std::list<std::string>::const_iterator first, 
                      const std::list<std::string>::const_iterator last, 
                      const std::string& str,
                      std::list<std::string>::const_iterator* line = 0,
                      std::string::size_type *pos = 0,
                      int* index = 0) const;
   bool StoreContains(const std::list<std::string>& store, const std::string& str,
                      std::list<std::string>::const_iterator* line = 0,
                      std::string::size_type *pos = 0,
                      int* index = 0) const;

   // Append lines to store
   void AppendLinesToStore(std::list<std::string>& store, 
                           const std::list<std::string>& lines);

   // Convert a store into a single string
   void StoreToString(const std::list<std::string>& store, std::string& str);

   // Handle "User not logged in" error
   bool HandleUserNotLoggedIn(const std::string& currentDirectory, bool& repeat);
   // We need to call "cvs init"
   bool CallCvsInit(const std::string& currentDirectory, bool& repeat);
   // Prepare to retry without encryption
   void RetrySSPIWithoutEncryption();
   // Detect errors in CVS output
   int DetectErrors(const std::string& command, int defaultResult);

   bool         myInUse;
   bool         myUserAborted;
   bool         myDisableSspiEncryption;
   std::string  myPassword;
   bool         myPasswordSet;
   bool         myLoginCancelled;
   CVSRoot      myCVSRootHolder;
   int          myCurrentStdoutLineNo;
   std::string  myCVSROOT;
   int          myExitCode;
   // Remember if user has answered "yes to all" or "no to all"
   YesNoAllDialog::YesNoAll     myLastYesNoResponse;

   ThreadSafeProgress* myThreadSafeProgressDialog;
   ProgressDialog* myProgressDialog;

   // Name of file to stream output to
   std::string myStreamingToFile;
   
   std::list<std::string> myTotalStdErrStore;
   std::list<std::string> myTotalStdOutStore;
   std::list<std::string> myStdErrStore;
   std::list<std::string> myStdOutStore;
   std::list<std::string> myOutputStore;
   bool keepPassword;
   bool myShowStdout;
   bool myStreamIsBinary;
   
   int myUses;
   int myErrors;
   bool myMessages;
   bool mySilent;
   close_t myCloseSetting;

   Thread<CVSAction>    myThread;
   bool                 myThreadResult;
   std::string          myThreadDirectory;
   MakeArgs             myThreadArgs;
   HANDLE               myThreadEvent;
   HANDLE               myShowUiEvent;
   HANDLE               myShowUiFinishedEvent;
   show_ui_t            myShowUiCommand;
   void*                myShowUiData;
};

#endif // CVS_ACTION_H
