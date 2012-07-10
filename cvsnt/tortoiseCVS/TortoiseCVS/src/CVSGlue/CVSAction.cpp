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


#include "StdAfx.h"
#include "CVSAction.h"
#include "CVSStatus.h"
#include "CVSRoot.h"
#include <Utils/Keyboard.h>
#include <Utils/PathUtils.h>
#include <Utils/ProcessUtils.h>
#include <Utils/SandboxUtils.h>
#include <Utils/SoundUtils.h>
#include <Utils/TortoiseException.h>
#include <Utils/TortoiseRegistry.h>
#include <Utils/TortoiseUtils.h>
#include <Utils/Preference.h>
#include <TortoiseAct/TortoiseTip.h>
#include <Utils/TortoiseDebug.h>
#include <TortoiseAct/ThreadSafeProgress.h>
#include <Utils/Translate.h>
#include <Utils/StringUtils.h>
#include <DialogsWxw/LoginDialog.h>
#include <DialogsWxw/CheckQueryDlg.h>
#include <DialogsWxw/WxwMain.h>
#include <DialogsWxw/WxwHelpers.h>

#include <sstream>
#include <time.h>

#define MAX_CONSOLE_BUF_LINES 15

const wxString repositoryDirectoryFailure = _(
"Tortoise Tip:  Sometimes you get this error if you have the repository folder \
wrong in the Checkout dialog.  Make sure you have a slash at the start of the \
folder name, and that the case and path are correct.  Watch out for spurious trailing \
characters.\n\n\
Examples are:  \"/cvsroot\", \"/usr/local/cvs-repository\", \"/cvsroot/cvsgui\"");



// Get cvs stdout
long ConsoleOut(const char* txt, long len, const CvsProcess* process)
{
   TDEBUG_ENTER("ConsoleOut");
   TDEBUG_TRACE(std::string(txt, len));
   return ((CVSAction *) process->appData)->ConsoleOut(txt, len);
}


// Get cvs stderr
long ConsoleErr(const char* txt, long len, const CvsProcess* process)
{
   TDEBUG_ENTER("ConsoleErr");
   TDEBUG_TRACE(std::string(txt, len));
   return ((CVSAction *) process->appData)->ConsoleErr(txt, len);
}


// Ask about env. variable
const char* GetEnv(const char* name, const CvsProcess* process)
{
   return ((CVSAction *) process->appData)->GetEnv(name);
}



// Tells the exit code 
void Exit(int code, const CvsProcess* process)
{
   TDEBUG_ENTER("Exit");
   ((CVSAction *) process->appData)->myExitCode = code;
}

void OnDestroy(const CvsProcess* process)
{
   
}

static CvsProcessCallbacks sCallTable =
{
   ConsoleOut,
   ConsoleErr,
   GetEnv,
   Exit,
   OnDestroy
};



void CVSAction::PipeToGUI()
{
   CSHelper cs(myCsOutput, true);
   
   std::list<ConsoleOutputLine> lines;
   lines.swap(myConsoleOutput);
   SetEvent(myRemovedFromConsoleBufferEvent);
   cs.Leave();

   std::list<ConsoleOutputLine>::iterator itLines;
   std::vector<std::string*> vLines;
   vLines.reserve(lines.size());
   for (itLines = lines.begin(); itLines != lines.end(); itLines++)
   {
      // Stdout line
      if (!itLines->m_StdErr)
      {
         ProcessStdoutLine(itLines->m_Text);
         if (myShowStdout && !(myFlags & PIPE_STDOUT))
         {
            std::string* pstr = new std::string(itLines->m_Text);
            FindAndReplace<std::string>(*pstr, "\t", "   ");
            vLines.push_back(pstr);
         }
      }
      // Stderr line
      else
      {
         std::string* pstr = new std::string(itLines->m_Text);
         FindAndReplace<std::string>(*pstr, "\t", "   ");
         vLines.push_back(pstr);
         ProcessStderrLine(itLines->m_Text);
      }
   }
   if (!vLines.empty() && myThreadSafeProgressDialog)
   {
      myThreadSafeProgressDialog->NewTextLines(vLines);
   }
}


void CVSAction::FlushToGUI()
{
   CSHelper cs(myCsOutput, true);
   if (!myStdoutLine.empty())
   {
      myConsoleOutput.push_back(ConsoleOutputLine(myStdoutLine.c_str(),
                                                  static_cast<int>(myStdoutLine.length()), false));
      myStdoutLine.erase();
   }
   if (!myStderrLine.empty())
   {
      myConsoleOutput.push_back(ConsoleOutputLine(myStderrLine.c_str(),
                                                  static_cast<int>(myStderrLine.length()), true));
      myStderrLine.erase();
   }
   cs.Leave();
   PipeToGUI();
}


CVSAction::CVSAction(wxWindow* parent, int flags)
   : myFlags(flags),
     myCvsProcess(0),
     myStreamHandle(INVALID_HANDLE_VALUE),
     myAbortEvent(INVALID_HANDLE_VALUE),
     myAddedToConsoleBufferEvent(INVALID_HANDLE_VALUE),
     myRemovedFromConsoleBufferEvent(INVALID_HANDLE_VALUE),
     myParent(parent),
     myInUse(false),
     myUserAborted(false),
     myDisableSspiEncryption(false),
     myPasswordSet(false),
     myLoginCancelled(false),
     myExitCode(0),
     myLastYesNoResponse(YesNoAllDialog::No),
     myThreadSafeProgressDialog(0),
     myProgressDialog(0),
     keepPassword(false),
     myShowStdout(true),
     myStreamIsBinary(false),
     myUses(0),
     myErrors(0),
     myMessages(false),
     mySilent(false),
     myCloseSetting(CloseNever),
     myThreadResult(false),
     myThreadEvent(INVALID_HANDLE_VALUE),
     myShowUiEvent(INVALID_HANDLE_VALUE),
     myShowUiFinishedEvent(INVALID_HANDLE_VALUE),
     myShowUiCommand(ShowPasswordDialog),
     myShowUiData(0)
{
   myAbortEvent = CreateEvent(0, true, false, 0);
   myAddedToConsoleBufferEvent = CreateEvent(0, true, false, 0);
   myRemovedFromConsoleBufferEvent = CreateEvent(0, true, false, 0);

   // This could be a scoped preference, but I don't think anyone wants that.
   myCloseSetting = static_cast<close_t>(GetIntegerPreference("Close Automatically"));

   if (!(flags & HIDE_PROGRESS))
   {
      ProgressDialog::AutoCloseSetting ac = ProgressDialog::acDefaultNoClose;
      switch (myCloseSetting)
      {
      case CloseNever:
         ac = ProgressDialog::acDefaultNoClose;
         break;
      case CloseIfNoErrors:
      case CloseIfNoMessages:
         ac = ProgressDialog::acDefaultClose;
         break;
      case CloseAlways:
         ac = ProgressDialog::acAutoClose;
         break;
      }
      myProgressDialog = CreateProgressDialog(parent, myAbortEvent, ac, true);
      myProgressDialog->Update();
      myThreadSafeProgressDialog = ThreadSafeProgress::CreateThreadSafeProgress(myProgressDialog);
   }
   
   myUses = 0;
   myErrors = 0;

   int tick = static_cast<int>(time(0));
   TortoiseRegistry::WriteInteger("\\HKEY_CURRENT_USER\\Software\\Cvsnt\\cvsadvert\\LastAdvert", tick);

   // Create unnamed event for synchronization when we do not have a progress dialog
   myThreadEvent = CreateEvent(0, FALSE, FALSE, 0);
   // Create unnamed events for synchronization with UI events
   myShowUiEvent = CreateEvent(0, TRUE, FALSE, 0);
   myShowUiFinishedEvent = CreateEvent(0, TRUE, FALSE, 0);
   // Start the worker threads
   myThread.BeginThread(this, &CVSAction::ThreadMain);
}

void CVSAction::SetStreamFile(const std::string& file)
{
   myStreamingToFile = file;
   DeleteFileA(myStreamingToFile.c_str());
}

void CVSAction::SetCloseIfOK(bool closeIfOK)
{
   myCloseSetting = closeIfOK ? CloseIfNoErrors : CloseNever;
   if (myProgressDialog)
      myProgressDialog->SetAutoClose(closeIfOK ?
                                     ProgressDialog::acDefaultClose :
                                     ProgressDialog::acDefaultNoClose);
}

void CVSAction::SetCloseAnyway(bool closeAnyway)
{
   if (closeAnyway)
   {
      myCloseSetting = CloseAlways;
      if (myProgressDialog)
         myProgressDialog->SetAutoClose(ProgressDialog::acAutoClose);
   }
   // If argument is 'false', restore default setting
   if (!closeAnyway && (myCloseSetting == CloseAlways))
       // This could be a scoped preference, but I don't think anyone wants that.
       myCloseSetting = static_cast<close_t>(GetIntegerPreference("Close Automatically"));
}

void CVSAction::SetClose(close_t how)
{
   myCloseSetting = how;
}

CVSAction::close_t CVSAction::GetClose() const
{
    return myCloseSetting;
}

void CVSAction::SetSilent(bool silent)
{
   mySilent = silent;
}


bool CVSAction::Command(const std::string& currentDirectory, MakeArgs& args)
{
   TDEBUG_ENTER("CVSAction::Command");
   if (myCVSROOT.empty())
   {
      myCVSROOT = CVSStatus::CVSRootForPath(currentDirectory);
      myCVSRootHolder.SetCVSROOT(myCVSROOT);
   }

   TDEBUG_TRACE("CVSAction::Command: CVSROOT '" << myCVSRootHolder.GetCVSROOT() << "'");

#if 0
   // Use "update -3" for CVSNT >= 2.0.18
   if (args.main_command() == "update")
   {
      CVSServerFeatures features;
      features.Initialize(this);
      if (features.SupportsThreeWayDiffs()) 
      {
         args.add_option("-3");
      }
   }
#endif 

   return InternalCommand(currentDirectory, args);
}


bool CVSAction::InternalCommand(const std::string& currentDirectory, MakeArgs& args)
{
   TDEBUG_ENTER("CVSAction::InternalCommand");
   // If user aborted an earlier command, do not attempt to run this one
   if (myUserAborted)
      return false;
   bool ret;
   bool repeat;

   int myErrorsOld = myErrors;

   if ((args.main_command() == "update") && IsUnixSandbox(currentDirectory))
   {
       CVSServerFeatures features;
       features.Initialize(this);
       features.AddUnixLineEndingsFlag(args);
   }
   
   do
   {
      repeat = false;
      ret = DoCommand(currentDirectory, args);
      TDEBUG_TRACE("After DoCommand: " << ret);

      if (myUserAborted)
         break;
      
      // Look for instructions to run "cvs login",
      // and ask for their password
      if (NeedsLogin())
      {
         ret = HandleUserNotLoggedIn(currentDirectory, repeat);
      }

      // If SSPI fails because encryption is not supported (on Linux),
      // try again with encryption disabled
      if (SSPIEncryptionFailed())
      {
         RetrySSPIWithoutEncryption();
         repeat = true;
      }
      else
      {
         if (myDisableSspiEncryption)
         {
            // The retry succeeded
            TortoiseRegistry::WriteBoolean("DisableSSPIEncryption\\" + myCVSRootHolder.GetServer(), true);
         }
         // Make sure we only retry once
         myDisableSspiEncryption = false;
      }
      // Look for instructions to "cvs init" and give them the option
      if (NeedsInit())
      {
         ret = CallCvsInit(currentDirectory, repeat);
      }
      if (BadLockServerVersion() && myProgressDialog)
         myProgressDialog->NewText(_(
"Tortoise Tip: Apparently you are using the :local: protocol, and \
you have an older version of CVSNT installed than the one included \
with TortoiseCVS. To resolve this problem, do one of the following:\n\
1) Don't use :local: - add the repository to your local CVSNT server\n\
   (Control Panel/CVS for NT/Repositories) and use a network protocol\n\
   (e.g. :pserver: or :sspi:) to access the repository.\n\
   This requires a fresh checkout.\n\
2) Remove your local CVSNT server installation if you don't need it.\n\
3) Update your local CVSNT server installation to the same version included\n\
   with TortoiseCVS."),
                                   ProgressDialog::TTTip);
    }
    while (repeat);
    TDEBUG_TRACE("After the loop");

    // Clear other stuff so values don't confuse if we reuse CVSAction later
    myCVSROOT.clear();
    myCVSRootHolder.Clear();
    myStreamingToFile.clear();
    myShowStdout = true;
    TDEBUG_TRACE("After clearing");

    if (!mySilent && (myErrors > myErrorsOld))
       PlayErrorSound();
    TDEBUG_TRACE("After sound: " << ret);
    
    return ret;
}

bool CVSAction::DoCommandThread(const std::string& currentDirectory, MakeArgs args)
{
    TDEBUG_ENTER("CVSAction::DoCommandThread");
    // Prevent more than one at once, as the progress dialog can't cope with it.
    if (myInUse)
    {
       TortoiseFatalError(_("Internal error, CVSAction::DoCommand used twice within same process."));
    }
    myInUse = true;
    myUserAborted = false;

    bool ret = false;

    myStdErrStore.clear();
    myStdOutStore.clear();
    myOutputStore.clear();

    int result = 0;

    myMapEnvVars.clear();
    myMapEnvVars["USER"] = myCVSRootHolder.GetUser();
    myMapEnvVars["LOGNAME"] = myCVSRootHolder.GetUser();
    std::string sCVSREAD = GetEnvVar("CVSREAD");
    if (!sCVSREAD.empty())
      myMapEnvVars["CVSREAD"] = sCVSREAD;
    if (!myCVSROOT.empty())
       myMapEnvVars["CVSROOT"] = myCVSROOT;
    else
       myMapEnvVars["CVSROOT"] = GetEnvVar("CVSROOT");
    std::string sEnvVar = GetEnvVar("TEMP");
    if (!sEnvVar.empty())
      myMapEnvVars["TEMP"] = sEnvVar;
    sEnvVar = GetEnvVar("TMP");
    if (!sEnvVar.empty())
      myMapEnvVars["TMP"] = sEnvVar;

    // Set envvars for :ext: protocol
    std::string loginApplication(GetStringPreference("External SSH Application", currentDirectory));
    std::string serverApplication(GetStringPreference("Server CVS Application", currentDirectory));
    std::string loginParams(GetStringPreference("External SSH Params", currentDirectory));
    std::string loginString;
    if (!loginApplication.empty())
    {
       loginString = "\"" + loginApplication + "\"";
    }
    else
    {
       loginString = "\"" + EnsureTrailingDelimiter(GetTortoiseDirectory()) 
          + "TortoisePlink.exe\"";
    }

    if (!loginParams.empty())
    {
       loginString += " " + loginParams;
    }

    SetEnvVar("PLINK_PROTOCOL", "ssh");

    TDEBUG_TRACE("loginString: " << loginString);
    TDEBUG_TRACE("serverString: " << serverApplication);
    SetEnvVar("CVS_SERVER", serverApplication);
    SetEnvVar("CVS_EXT", loginString);

    char buf[20] = "";
    if (myProgressDialog)
       _snprintf(buf, sizeof(buf), "%p", GetHwndOf(myProgressDialog));
    else
       _snprintf(buf, sizeof(buf), "%p", GetRemoteHandle());

    if (strlen(buf))
       SetEnvVar("TCVS_HWND", buf);
    
    if (GetBooleanPreference("Client Log", currentDirectory))
        myMapEnvVars["CVS_CLIENT_LOG"] = GetTortoiseDirectory() + "cvsclient.log";
    std::string homeDir;

    GetHomeDirectory(homeDir);
    myMapEnvVars["HOME"] = homeDir;

    // Override CVS network share error
    if ((myCVSRootHolder.GetProtocol() == "local") &&
        GetBooleanPreference("Allow Network Drives"))
    {
        args.add_global_option("-N");
    }

    // Instruct CVS to use data encryption (-x) on protocols that
    // supports it (unless disabled in the registry)
    bool useEncryption = false;
    bool encryptionEnabled = GetBooleanPreference("Data Encryption", currentDirectory);
    if (myCVSRootHolder.GetProtocol() == "sspi")
    {
       if (encryptionEnabled &&                                         // - Encryption not disabled in registry
           !myDisableSspiEncryption &&                                  // - Not retrying SSPI without encryption
           !TortoiseRegistry::ReadBoolean("DisableSSPIEncryption\\" +   // - Encryption not permanently disabled
                                          myCVSRootHolder.GetServer(),  //  for this server
                                          false))
          useEncryption = true;
    }
    if ((myCVSRootHolder.GetProtocol() == "gserver") && encryptionEnabled)
       useEncryption = true;
    if (useEncryption)
       args.add_global_option("-x");

    myUserAborted = false;
    std::string arguments;
    unsigned int argc;
    char **argv;
    args.get_arguments(argc, argv, arguments);

    if (!myStreamingToFile.empty())
    {
       myStreamHandle = CreateFileA(myStreamingToFile.c_str(),
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    0,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    0);
    }

    TDEBUG_TRACE("Command: " << arguments);
    if (myThreadSafeProgressDialog)
    {
       wxString whatDoing = Printf(_("In %s: %s\nCVSROOT=%s"), 
                                   wxText(currentDirectory).c_str(),
                                   wxText(arguments).c_str(),
                                   wxText(myCVSROOT).c_str());
       whatDoing += wxT("\n\n");
       myThreadSafeProgressDialog->NewText(whatDoing, ProgressDialog::TTNoise);
    }

    CvsProcessStartupInfo startupInfo;
    startupInfo.hasTty = 0;
    startupInfo.currentDirectory = currentDirectory.c_str();

    myCvsProcess = cvs_process_run(argv[0], argc - 1, &argv[1],
                                   &sCallTable, &startupInfo, this);
    for (unsigned int i = 0; i < argc; i++)
       delete argv[i];
    delete argv;

    if (!myCvsProcess)
    {
       if (myFlags & SILENT_ON_ERROR)
          result = 1;
       else
       {
          std::string s1;
          // If the command line is very long, the message box can become larger than the screen.
          // We want to avoid that.
          if (arguments.length() > 1024)
             s1 = arguments.substr(0, 1024) + "...";
          else
             s1 = arguments;
          wxString msg = Printf(_(
"Trouble launching the CVS process:\n%s\nOn folder: %s\n%s\n"),
                                wxText(s1).c_str(), wxText(currentDirectory).c_str(), 
                                GetWin32ErrorMessage(GetLastError()).c_str());
          TortoiseFatalError(msg);
       }
    }
    else
    {
       TDEBUG_TRACE("Started CVS process");
       myCurrentStdoutLineNo = 0;
       while (true)
       {
          //TDEBUG_TRACE("Pipe loop");
          HANDLE waitHandles[2] = { myAbortEvent, myAddedToConsoleBufferEvent };
          DWORD dwWaitResult = WaitForMultipleObjects(2, waitHandles, FALSE, 200);
          if (dwWaitResult == WAIT_OBJECT_0)
          {
             ResetEvent(myAbortEvent);
             TDEBUG_TRACE("Stopping CVS");
             myUserAborted = true;
             cvs_process_stop(myCvsProcess);
             TDEBUG_TRACE("Stopped CVS");
          }
          else if (dwWaitResult == WAIT_OBJECT_0 + 1)
          {
             ResetEvent(myAddedToConsoleBufferEvent);
          }
          
          PipeToGUI();
          if (!cvs_process_is_active(myCvsProcess))
          {
             TDEBUG_TRACE("Process not active");
             break;
          }
       } 
       
       // Make sure all text is written to progress dialog
       FlushToGUI();
            
       result = myExitCode;
    }

    TDEBUG_TRACE("done: " << DWORD(result));

    // Detect errors in CVS output
    result = DetectErrors(args.main_command(), result);
   
    myUses++;
    wxString message;
    ProgressDialog::TextType type = ProgressDialog::TTNoise;
    if (myUserAborted) 
    {
        message = _("CVS operation aborted by user request");
        type = ProgressDialog::TTError;
        myErrors++;
    }
    else if (result == 0)
    {
        ret = true;
        message = _("Success, CVS operation completed");
    }
    else
    {
        message = _("Error, CVS operation failed");
        type = ProgressDialog::TTError;
        myErrors++;
    }

    // Display the message
    if (myThreadSafeProgressDialog)
    {
       if (!myStdOutStore.empty() || !myStdErrStore.empty())
          message = wxT("\n") + message;
       message += wxT("\n\n");
       myThreadSafeProgressDialog->NewText(message, type);

       // Add the Tortoise tip
       std::vector<std::list<std::string>* > scanTexts;
       scanTexts.push_back(&myStdOutStore);
       scanTexts.push_back(&myStdErrStore);
       wxString tip = TortoiseTip(scanTexts);
       if (!tip.empty())
          myThreadSafeProgressDialog->NewText(tip + wxT("\n\n"), ProgressDialog::TTTip);
    }

    myInUse = false;

    return ret && !myUserAborted;
}

CVSAction::~CVSAction()
{
   TDEBUG_ENTER("CVSAction::~CVSAction");
    // Check for control being pressed again (it is also checked
    // when the dialog is first made, so people can hold it down while
    // they select a menu item), and keep the dialog open if it is
    if (IsControlDown())
        myCloseSetting = CloseNever;

    myMessages = !myTotalStdOutStore.empty() || !myTotalStdErrStore.empty();

    // If there were lots of operations, and one failed, remind the user
    if (myErrors > 0 && myUses > 1)
    {
        wxString message = _("Error, one of the CVS operations failed");
        std::string asciiMessage(message.mb_str());
        myStdErrStore.push_back(asciiMessage);
        myTotalStdErrStore.push_back(asciiMessage);
        if (myThreadSafeProgressDialog)
           myThreadSafeProgressDialog->NewText(message, ProgressDialog::TTError);
    }

    if (myProgressDialog)
    {
       // Bring to the front if something especially interesting to say
       if (myErrors > 0)
          myProgressDialog->Show(true);

       // Determine whether to autoclose dialog based on preference settings
       bool closeDialog = false;
       if (!myErrors)
       {
          if (myCloseSetting == CloseIfNoErrors)
             closeDialog = true;
          else if ((myCloseSetting == CloseIfNoMessages) && !myMessages)
             closeDialog = true;
       }
       if (myCloseSetting == CloseAlways)
          closeDialog = true;

       // Override with user selection if no errors
       if ((myProgressDialog->AutoClose() != ProgressDialog::acDefault) && !myErrors)
          closeDialog = (myProgressDialog->AutoClose() == ProgressDialog::acUserAutoClose);

       if (!closeDialog)
          myProgressDialog->WaitForAbort();

       delete myThreadSafeProgressDialog;
       myThreadSafeProgressDialog = NULL;
       DeleteProgressDialog(myProgressDialog);
       myProgressDialog = 0;
    }
    
    CloseConsoleOutput();

    if (myAbortEvent != INVALID_HANDLE_VALUE)
    {
       CloseHandle(myAbortEvent);
    }
    if (myAddedToConsoleBufferEvent != INVALID_HANDLE_VALUE)
    {
       CloseHandle(myAddedToConsoleBufferEvent);
    }
    if (myRemovedFromConsoleBufferEvent != INVALID_HANDLE_VALUE)
    {
       CloseHandle(myRemovedFromConsoleBufferEvent);
    }
    if (myThreadEvent != INVALID_HANDLE_VALUE)
    {
       CloseHandle(myThreadEvent);
    }
    if (myShowUiEvent != INVALID_HANDLE_VALUE)
    {
       CloseHandle(myShowUiEvent);
    }
    if (myShowUiFinishedEvent != INVALID_HANDLE_VALUE)
    {
       CloseHandle(myShowUiFinishedEvent);
    }
}


void CVSAction::CloseConsoleOutput()
{
   TDEBUG_ENTER("CVSAction::CloseConsoleOutput");
   if (myStreamHandle != INVALID_HANDLE_VALUE)
   {
      CloseHandle(myStreamHandle);
      myStreamHandle = INVALID_HANDLE_VALUE;
   }
}

bool CVSAction::AtLeastOneSuccess()
{
    return (myErrors != myUses);
}

const std::list<std::string>& CVSAction::GetStdOutList()
{
    return myStdOutStore;
}

const std::list<std::string>& CVSAction::GetStdErrList()
{
    return myStdErrStore;
}

const std::list<std::string>& CVSAction::GetOutputList()
{
    return myOutputStore;
}


std::string CVSAction::GetStdOutText(bool clear)
{
    std::string str;
    StoreToString(myStdOutStore, str);
    if (clear)
    {
        // Clear both myStdOutStore and streaming file
        myStdOutStore.clear();
        if (myStreamHandle != INVALID_HANDLE_VALUE)
        {
            SetFilePointer(myStreamHandle, 0, 0, FILE_BEGIN);
            SetEndOfFile(myStreamHandle);
        }
    }
   return str;
}


std::string CVSAction::GetStdErrText()
{
   // calculate the size
   std::string str;
   StoreToString(myStdErrStore, str);
   return str;
}


std::string CVSAction::GetOutputText()
{
   // calculate the size
   std::string str;
   StoreToString(myOutputStore, str);
   return str;
}



void CVSAction::LockProgressDialog(bool lock)
{
   TDEBUG_ENTER("CVSAction::LockProgressDialog");
   if (myThreadSafeProgressDialog)
      myThreadSafeProgressDialog->Lock(lock);
}


void CVSAction::SetCVSRoot(const CVSRoot& cvsroot)
{
    myCVSRootHolder = cvsroot;
    myCVSROOT = cvsroot.GetCVSROOT();
}
   
void CVSAction::SetPassword(const std::string& password)
{
    myPassword = password;
    myPasswordSet = true;
}

void CVSAction::SetProgressCaption(const wxString& msg)
{
   if (myProgressDialog)
   {
      wxString caption(msg);
      caption += wxT(" - TortoiseCVS");
      myProgressDialog->SetCaption(caption);
   }
}

void CVSAction::SetProgressFinishedCaption(const wxString& msg)
{
   if (myProgressDialog)
   {
      wxString caption(msg);
      caption += wxT(" - TortoiseCVS");
      myProgressDialog->SetFinishedCaption(caption);
   }
}

void CVSAction::ThreadMain()
{
    while (myThread.WaitForMoreWork())
    {
        myThreadResult = DoCommandThread(myThreadDirectory, myThreadArgs);
        // This signals ThreadSafeProgress::Main() to exit its loop
        if (myThreadSafeProgressDialog)
           myThreadSafeProgressDialog->Finished();
        else
           SetEvent(myThreadEvent);
    }
}


bool CVSAction::DoCommand(const std::string& currentDirectory, MakeArgs args)
{
   TDEBUG_ENTER("DoCommand");
   myThreadDirectory = currentDirectory;
   myThreadArgs = args;

   // New stuff for the thread!
   myThread.SignalNewWork();

   // Make sure the dialog responds, and wait until the thread is done
   if (myThreadSafeProgressDialog)
   {
      TDEBUG_TRACE("Main loop");
      while (myThreadSafeProgressDialog->Main(myShowUiEvent))
      {
         HandleUiEvent();
         ResetEvent(myShowUiEvent);
         SetEvent(myShowUiFinishedEvent);
      }
   }
   else
   {
      HANDLE handles[2] = {myThreadEvent, myShowUiEvent};
      DWORD waitResult = 0;
      TDEBUG_TRACE("Wait for thread end");
      do
      {
         waitResult = WaitForMultipleObjects(2, (HANDLE *) &handles, false, INFINITE);
         if (waitResult == WAIT_OBJECT_0 + 1) 
         {
            HandleUiEvent();
            ResetEvent(myShowUiEvent);
            SetEvent(myShowUiFinishedEvent);
         }
      } while (waitResult == WAIT_OBJECT_0 + 1);
   }

   TDEBUG_TRACE("Result: " << myThreadResult);
   return myThreadResult;
}

ProgressDialog* CVSAction::GetProgressDialog()
{
    return myProgressDialog;
}



// If CVS asks for input, try to write an appropriate response to the child process' stdin.
bool CVSAction::HandleUserInput(YesNoData& yesNoData)
{
   TDEBUG_ENTER("CVSAction::HandleUserInput");
   std::string::size_type pos;

   std::string line;
   
   std::list<std::string>::const_iterator itLine;

   // cvs edit: "%s has been modified; revert changes? "
   int index = myCurrentStdoutLineNo;
   if (StoreContains(myStdOutStore.begin(), myStdOutStore.end(), 
                     CVSPARSE("has been modified; revert changes?"), &itLine, &pos, &index))
   {
      // Make sure all text is written to progress dialog
      FlushToGUI();

      line = *itLine;

      // Extract file name
      std::string filename = line.substr(0, pos);
      yesNoData.question = Printf(_(
"%s has been modified. Do you want to discard your local changes and revert to the most current version in the repository?\n\
Select \"Yes\" to discard your changes, or \"No\" to cancel the Unedit command (take no action)."),
                                  wxText(filename).c_str());
      yesNoData.withCancel = false;

      ++itLine;
      myCurrentStdoutLineNo = index;
      return true;
   }
      
   //  cvs release: "Are you sure you want to release %sdirectory `%s':"
   if (StoreContains(myStdOutStore, 
                     CVSPARSE("Are you sure you want to release "), &itLine, &pos, &index))
   {
       yesNoData.question = _(
"You have altered files in this repository. See the main TortoiseCVS dialog for details.\n\
Do you want to discard your local changes?\n\
Select \"Yes\" to discard your changes, or \"No\" to cancel the \
Release command (take no action).");
       yesNoData.withCancel = false;

       ++itLine;
       myCurrentStdoutLineNo = index;
       return true;
   }

   //  ssh protocol: Adding server's fingerprint to registry
   if (StoreContains(myStdOutStore, 
                     CVSPARSE("The server's key fingerprint is"), &itLine, &pos, &index))
   {
      // Make sure all text is written to progress dialog
      FlushToGUI();

      itLine++;
      line = *itLine;

      // Extract server key
      std::string key = line;

      yesNoData.question = Printf(_(
"The server's host key is not cached in the registry.\nKey fingerprint: %s\n\
If you trust this host, hit \"Yes\" to add the key to the cache.\n\
If you want to connect just once, hit \"No\".\n\
If you don't trust this host, hit \"Cancel\".\n"), wxText(key).c_str());
      yesNoData.withCancel = true;

      itLine++;
      myCurrentStdoutLineNo = index;
      return true;
   }

   // Generic CVSGUI question
   if (StoreContains(myStdOutStore, 
                     CVSPARSE("Question: "), &itLine, &pos, &index))
   {
      // Make sure all text is written to progress dialog
      FlushToGUI();

      line = *itLine;

      // Extract question
      std::string title = line.substr(pos + strlen(CVSPARSE("Question: ")));
      ++itLine;
      std::string question = *itLine++;

      // Should we show a Cancel button?
      line = *itLine;
      yesNoData.withCancel = (line.find(CVSPARSE("Cancel")) != std::string::npos);

      yesNoData.question = Printf(wxT("%s\n\n%s"),
                                   wxText(title).c_str(),
                                   wxText(question).c_str());


      itLine++;
      myCurrentStdoutLineNo = index;
      return true;
   }

   return false;
}



void CVSAction::HandleUiEvent()
{
   switch (myShowUiCommand)
   {
   case ShowPasswordDialog:
   {
      GetPasswordData* data = (GetPasswordData *) myShowUiData;
      data->doCancel = !DoLoginDialog(myProgressDialog, data->password);
      break;
   }

   case ShowYesNoDialog:
   {
       YesNoData* data = reinterpret_cast<YesNoData*>(myShowUiData);
       YesNoAllDialog::YesNoAll response;
       if ((myLastYesNoResponse == YesNoAllDialog::YesAll) ||
           (myLastYesNoResponse == YesNoAllDialog::NoAll))
           response = myLastYesNoResponse;
       else
           response = DoYesNoAllDialog(myProgressDialog, data->question, false, data->withCancel);
       
       switch (response)
       {
       case YesNoAllDialog::Yes:
       case YesNoAllDialog::YesAll:
           data->answer = "y";
           break;

       case YesNoAllDialog::No:
       case YesNoAllDialog::NoAll:
           data->answer = "n";
           break;

       case YesNoAllDialog::Cancel:
           data->answer = "c";
           break;
       }

       myLastYesNoResponse = response;
       break;
   }

   default:
       ASSERT(false);
   }
}


bool CVSAction::NeedsLogin() const
{
   return (myErrors &&
      (StoreContains(myStdErrStore, CVSPARSE("Authentication failed")) || // this one for SSPI
       StoreContains(myStdErrStore, CVSPARSE("authentication failed")) ||
       StoreContains(myStdErrStore, CVSPARSE("authorization failed:")) ||
       StoreContains(myStdErrStore, CVSPARSE("Authentication protocol rejected access")) ||
       StoreContains(myStdErrStore, CVSPARSE("unrecognized auth response")) ||
       StoreContains(myStdErrStore, CVSPARSE(": no such user"))));
}


bool CVSAction::NeedsInit() const
{
   return myErrors &&
      (StoreContains(myStdErrStore, CVSPARSE("CVSROOT: No such file or directory")) ||
       StoreContains(myStdErrStore, CVSPARSE("CVSROOT\nNo such file or directory"))) &&
      (myCVSRootHolder.GetProtocol() == "local");
}


bool CVSAction::BadLockServerVersion() const
{
   return myErrors &&
      StoreContains(myStdErrStore, CVSPARSE("Invalid Lockserver version"));
}


bool CVSAction::SSPIEncryptionFailed() const
{
   return myErrors &&
      StoreContains(myStdErrStore, CVSPARSE("end of file from server")) &&
      (myCVSRootHolder.GetProtocol() == "sspi") &&
      !myDisableSspiEncryption &&
      !TortoiseRegistry::ReadBoolean("DisableSSPIEncryption\\" + myCVSRootHolder.GetServer(),
                                     false);
}


// Get cvs stdout
long CVSAction::ConsoleOut(const char* txt, long len)
{
   bool isBinaryData = myStreamIsBinary;

   // If we're in binary mode, switch back to text mode for the next message
   if (myStreamIsBinary)
   {
      myStreamIsBinary = false;
   }

   // If this is binary data, write it to a possible stream file and exit
   if (isBinaryData)
   {
      if (myStreamHandle != INVALID_HANDLE_VALUE)
      {
         DWORD dwWritten;
         WriteFile(myStreamHandle, txt, len, &dwWritten, 0);
      }
      return len;
   }

   // An empty message signals an upcoming binary message
   if (len == 0)
   {
      // Switch to binary for the next line
      myStreamIsBinary = true;
      return len;
   }

   // Remove CR
   std::string line(txt, len);
   FindAndReplace<std::string>(line, "\r", "");
   FindAndReplace<std::string>(line, "\n", "\r\n");
   
   // Write to stream file if required
   if (myStreamHandle != INVALID_HANDLE_VALUE)
   {
      DWORD dwWritten;
      WriteFile(myStreamHandle, line.c_str(), static_cast<DWORD>(line.size()), &dwWritten, 0);
      return len;
   }

   // Split into lines
   CSHelper cs(myCsOutput, true);

   // The buffer should not grow too large
   while (myConsoleOutput.size() > MAX_CONSOLE_BUF_LINES)
   {
      cs.Leave();
      Sleep(50);
      cs.Enter();
   }
   size_t i = 0;
   size_t p = 0;
   bool addedToBuffer = false;
   while (i < line.size())
   {
      // Look for newline characters
      if (line[i] == '\n')
      {
         // Include line buffer
         if (myStdoutLine.empty())
             // Nothing saved, so we already have the entire line
            myConsoleOutput.push_back(ConsoleOutputLine(line.c_str()+p, i - p - 1, false)); 
         else
         {
             // Add this chunk to what we previously had
             if (i != p)
                 myStdoutLine += line.substr(p, i - p - 1);
             // Save new line
             myConsoleOutput.push_back(ConsoleOutputLine(myStdoutLine.c_str(),
                                                         static_cast<int>(myStdoutLine.length()), false)); 
             myStdoutLine.erase();
         }
         addedToBuffer = true;
         p = i + 1;
      }
      ++i;
   }
   // put remains into line buffer
   myStdoutLine += line.substr(p, i - p);
   if (addedToBuffer)
      SetEvent(myAddedToConsoleBufferEvent);
   cs.Leave();
   return len;
}


// Process Stdout line
void CVSAction::ProcessStdoutLine(const std::string& line)
{
   myStdOutStore.push_back(line);
   myTotalStdOutStore.push_back(line);
   myOutputStore.push_back(line);
}



// Get cvs stderr
long CVSAction::ConsoleErr(const char* txt, long len)
{
   if (len == 0)
   {
      return len;
   }

   // Remove CR
   std::string line(txt, len);
   FindAndReplace<std::string>(line, "\r", "");

   // Split into lines
   CSHelper cs(myCsOutput, true);

   // The buffer should not grow too large
   while (myConsoleOutput.size() > MAX_CONSOLE_BUF_LINES)
   {
      cs.Leave();
      Sleep(50);
      cs.Enter();
   }
   size_t i = 0;
   size_t p = 0;
   bool addedToBuffer = false;
   while (i < line.size())
   {
      // Look for newline characters
      if (line[i] == '\n')
      {
         // Include line buffer
         if (myStderrLine.empty())
            myConsoleOutput.push_back(ConsoleOutputLine(line.c_str()+p, i - p, true)); 
         else
         {
            if (i != p)
               myStderrLine += line.substr(p, i - p);

            myConsoleOutput.push_back(ConsoleOutputLine(myStderrLine.c_str(),
                                                        static_cast<int>(myStderrLine.length()), true)); 
            myStderrLine.erase();
         }
         addedToBuffer = true;
         p = i + 1;
      }
      ++i;;
   }
   // put remains into line buffer
   myStderrLine += line.substr(p, i - p);
   if (addedToBuffer)
      SetEvent(myAddedToConsoleBufferEvent);
   cs.Leave();
   return len;
}


// Process Stderr line
void CVSAction::ProcessStderrLine(const std::string& line)
{
   myStdErrStore.push_back(line);
   myTotalStdErrStore.push_back(line);
   myOutputStore.push_back(line);
}



// Ask about env. variable
const char* CVSAction::GetEnv(const char* name)
{  
   static std::string value;
   std::string sName(name);
   std::map<std::string, std::string>::const_iterator it = myMapEnvVars.find(sName);
   if (it != myMapEnvVars.end())
   {
      value = it->second;
      return value.c_str();
   }
   else if (sName == "CVS_GETPASS")
   {
      PipeToGUI();
      if (myPasswordSet)
      {
         value = myPassword;
         return value.c_str();
      }
      else
      {
         GetPasswordData data;
         myShowUiCommand = ShowPasswordDialog;
         myShowUiData = &data;
         SetEvent(myShowUiEvent);
         WaitForSingleObject(myShowUiFinishedEvent, INFINITE);
         ResetEvent(myShowUiFinishedEvent);
         myLoginCancelled = data.doCancel;
         if (!data.doCancel)
         {
            value = data.password;
            return value.c_str();
         }
         else
            return 0;
      }
   }
   else if (sName == "CVSLIB_YESNO")
   {
       Sleep(500);
       PipeToGUI();
       YesNoData data;
       HandleUserInput(data);
       myShowUiCommand = ShowYesNoDialog;
       myShowUiData = &data;
       SetEvent(myShowUiEvent);
       WaitForSingleObject(myShowUiFinishedEvent, INFINITE);
       ResetEvent(myShowUiFinishedEvent);
       value = data.answer;
       return value.c_str();
   }

   return 0;
}


void CVSAction::SetHideStdout(bool hide)
{
   myShowStdout = !hide;
}



bool CVSAction::Aborted()
{
   return myUserAborted;  
}


// check if a store contains a string
bool CVSAction::StoreContains(const std::list<std::string>::const_iterator first, 
                              const std::list<std::string>::const_iterator last, 
                              const std::string& str,
                              std::list<std::string>::const_iterator* line,
                              std::string::size_type *pos,
                              int* index) const
{
   std::list<std::string>::const_iterator it = first;
   if (index)
       advance(it, *index);
   int lines = 0;
   while (it != last)
   {
      ++lines;
      std::string::size_type findpos = it->find(str, 0);
      if (findpos != std::string::npos)
      {
         if (line)
            *line = it;
         if (pos)
            *pos = findpos;
         if (index)
             *index += lines;
         return true;
      }
      ++it;
   }
   return false;
}



bool CVSAction::StoreContains(const std::list<std::string>& store,
                              const std::string& str,
                              std::list<std::string>::const_iterator* line,
                              std::string::size_type *pos,
                              int* index) const
{
   return StoreContains(store.begin(), store.end(), str, line, pos, index);
}




// Append lines to store
void CVSAction::AppendLinesToStore(std::list<std::string>& store, 
                        const std::list<std::string>& lines)
{
   TDEBUG_ENTER("CVSAction::AppendLinesToStore");
   if (store.size() == 0)
   {
      // If store is empty, simply assign the lines
      store.assign(lines.begin(), lines.end());
   }
   else
   {
      // Append the first line to the last store line
      std::list<std::string>::const_iterator it = lines.begin();
      store.back().append(*it);
      TDEBUG_TRACE("Last line: " << store.back());
      it++;

      // append remaining lines to the store
      if (it != lines.end())
      {
         store.insert(store.end(), it, lines.end());
      }
   }
}



// Convert a store into a single string
void CVSAction::StoreToString(const std::list<std::string>& store, std::string& str)
{
   // Calculate size
   int size = 0;
   std::list<std::string>::const_iterator it = store.begin();
   while (it != store.end())
   {
      size += static_cast<int>(it->length());
      it++;
   }

   // room for newlines
   size += static_cast<int>(store.size()) * 2;
   str.erase();
   str.reserve(size);

   // construct the string
   it = store.begin();
   while (it != store.end())
   {
      str.append(*it);
      str.append("\r\n");
      it++;
   }
}



// Handle "User not logged in" error
bool CVSAction::HandleUserNotLoggedIn(const std::string& currentDirectory, bool& repeat)
{
   TDEBUG_ENTER("CVSAction::HandleUserNotLoggedIn");
   --myErrors; // not a real error, as we hook it!
   bool ret = false;

   // Determine which server we need to log in to
   std::string directory = currentDirectory;
   std::string line;
   std::list<std::string>::const_iterator it;
   std::string::size_type rejectedPos;
   if (StoreContains(myStdErrStore, CVSPARSE(" rejected access"), &it, &rejectedPos))
   {
      const char* serverString = "server ";
      line = *it;
      std::string::size_type serverPos = line.find(CVSPARSE(serverString));
      if (serverPos != std::string::npos)
      {
         // Skip to start of server name
         serverPos += strlen(serverString);
         std::string serverName = line.substr(serverPos, rejectedPos - serverPos);
         TDEBUG_TRACE("serverName: " << serverName);
         directory = CVSStatus::RecursiveFindRoot(currentDirectory, serverName);
         TDEBUG_TRACE("directory: " << directory);
      }
   }
   MakeArgs loginArgs;
   loginArgs.add_option("login");
   myLoginCancelled = false;
   repeat = true;

   do
   {
      ret = DoCommand(directory, loginArgs);
      if (myLoginCancelled)
      {
         repeat = false;
         break;
      }

      if (!ret)
      {
         if (!NeedsLogin())
         {
            repeat = false;
            break;
         }
      }
      else
      {
         repeat = true;
         break;
      }
      myErrors--;
   } while (true);



   // Delete streaming file (we don't want data from login etc.,
   // only from the operations performed successfully)
   if (!myStreamingToFile.empty())
      DeleteFileA(myStreamingToFile.c_str());
   return ret;
}


void CVSAction::RetrySSPIWithoutEncryption()
{
   // Encryption is not disabled for this server
   --myErrors; // not a real error, as we hook it!
   // Try once more without encryption
   myDisableSspiEncryption = true;
   // Do not remember this error
   if (!myStreamingToFile.empty())
      DeleteFileA(myStreamingToFile.c_str());
   if (myProgressDialog)
      myProgressDialog->NewText(_("Retrying command with encryption disabled\n"), ProgressDialog::TTTip);
}         

// We need to call "cvs init"
bool CVSAction::CallCvsInit(const std::string& currentDirectory, bool& repeat)
{
   TDEBUG_ENTER("CallCvsInit");
   --myErrors; // not a real error, as we hook it!
   bool ret = false;

   wxString message;
   if (myCVSRootHolder.AllowServer() && !myCVSRootHolder.GetServer().empty())
   {
      message = Printf(_("There is no CVS repository in this folder:\n\n%s on %s"),
                       wxText(myCVSRootHolder.GetDirectory()).c_str(),
                       wxText(myCVSRootHolder.GetServer()).c_str());
   }
   else
   {
      message = Printf(_("There is no CVS repository in this folder:\n\n%s"),
                       wxText(myCVSRootHolder.GetDirectory()).c_str());
   }

   message += wxT("\n\n");
   message += _(
"If this is the wrong folder, correct it and try again. If you \
want to initialise a new repository with \"cvs init\" then check \
the box below and TortoiseCVS will try again.");

   if (DoCheckQueryDialog(myProgressDialog, message, false,
                          _("&Initialise a new repository here (makes a CVSROOT folder)")))
   {
      MakeArgs initArgs;
      initArgs.add_option("init");
      ret = DoCommand(currentDirectory, initArgs);
      if (!ret)
         --myErrors;
      repeat = true;
   }
   else
   {
      SetCloseIfOK(false);
      if (myProgressDialog)
         myProgressDialog->NewText(repositoryDirectoryFailure + wxT("\n"), ProgressDialog::TTTip);
   }

   // Delete streaming file (we don't want data from login etc.,
   // only from the operations performed successfully)
   if (!myStreamingToFile.empty())
      DeleteFileA(myStreamingToFile.c_str());

   return ret;
}



// Detect errors in CVS output
int CVSAction::DetectErrors(const std::string& command, int defaultResult)
{
   TDEBUG_ENTER("CVSAction::DetectErrors");
   int result = defaultResult;

   // Error codes for diff are stuffed (1 means there was a difference)
   // so we fudge our own more useful ones, based on if the CVS
   // operation wrote to stderr.

   if (command == "diff")
   {
      result = 0;
      if (!myStdErrStore.empty())
      {
         // OK, it might look as if there were differences. But we have to handle some special cases (yikes):
         // 1) For some reason there might be an empty line written to stderr.
         // 2) If CVSROOT specifies user 'anonymous', we might get the message
         //    'cvs [diff|server]: Empty password used - try 'cvs login' with a real password'.
         // 3) If the user has added but un-committed files, we will get
         //    'cvs [diff|server]: <file> is a new entry, no comparison available'.
         // 4) If the user has removed but un-committed files, we will get
         //    'cvs [diff|server]: <file> was removed, no comparison available'.

         // Examine output lines
         std::list<std::string>::const_iterator it = myStdErrStore.begin();
         while (it != myStdErrStore.end())
         {
            const std::string& line = *it++;
            TDEBUG_TRACE("Processing '" << line << "'");
            // Accept empty lines
            std::string::size_type x;
            if ((x = line.find_first_not_of("\r\n ")) == std::string::npos)
               continue;
            // Accept login message
            if (line.find(CVSPARSE("Empty password used - try 'cvs login' with a real password")) !=
               std::string::npos)
               continue;
            // Accept 'new entry' message
            if (line.find(CVSPARSE("is a new entry, no comparison available")) != std::string::npos)
               continue;
            // Accept 'was removed' message
            if (line.find(CVSPARSE("was removed, no comparison available")) != std::string::npos)
               continue;
            // Accept 'cvs[.exe] diff: Diffing <dir>' message
            if (line.find(CVSPARSE(" diff: Diffing ")) != std::string::npos)
               continue;
            // Everything else means trouble.
            result = 1;
            break;
         }
      }
   }
   // This is fixed in CVSNT, but as it's a server issue, we need this workaround
   else if (command == "remove")
   {
      if (!myStdErrStore.empty())
      {
         // Examine output lines
         std::list<std::string>::const_iterator it = myStdErrStore.begin();
         while (it != myStdErrStore.end())
         {
            // Look for errors
            if ((it->find(CVSPARSE("cvs server: cannot remove file ")) == 0) ||
               (it->find(CVSPARSE("cvs remove: cannot remove file ")) == 0))
            {
               result = 1;
               break;
            }
            it++;
         }
      }
    }
    // This is fixed in CVSNT, but as it's a server issue, we need this workaround
   else if (command == "tag")
   {
      if (!myStdErrStore.empty())
      {
         // Examine output lines
         std::list<std::string>::const_iterator it = myStdErrStore.begin();
         while (it != myStdErrStore.end())
         {
            // Look for errors
            if (((it->find(CVSPARSE("cvs server:")) == 0) ||
               (it->find(CVSPARSE("cvs tag:")) == 0))
               && ((it->find(CVSPARSE("lock file")) != std::string::npos)
               || (it->find(CVSPARSE("locally modified")) != std::string::npos)))
            {
               result = 1;
               break;
            }
            it++;
         }
      }
   }

   return result;
}

