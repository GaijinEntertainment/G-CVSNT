// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - January 2001

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
#include "ThreadSafeProgress.h"
#include "../Utils/ShellUtils.h"

ThreadSafeProgress::ThreadSafeProgress(ProgressDialog* forwardTo)
{
   myForwardTo = forwardTo;
   myFinishedEvent = CreateEvent(0, TRUE, FALSE, 0);
}

ThreadSafeProgress::~ThreadSafeProgress()
{
   CloseHandle(myFinishedEvent);
}

//static
ThreadSafeProgress* ThreadSafeProgress::CreateThreadSafeProgress(ProgressDialog* forwardTo)
{
   return new ThreadSafeProgress(forwardTo);
}

void ThreadSafeProgress::NewText(const wxString& txt, ProgressDialog::TextType type)
{
   std::pair<const wxString*, ProgressDialog::TextType> myPair = std::make_pair(&txt, type);
   Synchronize((WPARAM) ProgressDialog::aaNewText, (LPARAM) &myPair);
}

void ThreadSafeProgress::NewTextLines(const std::vector<std::string*>& lines, ProgressDialog::TextType type)
{
   std::pair<const std::vector<std::string*>*, ProgressDialog::TextType> myPair = std::make_pair(&lines, type);
   Synchronize((WPARAM) ProgressDialog::aaNewTextLines, (LPARAM) &myPair);
}

bool ThreadSafeProgress::UserAborted()
{
   bool bResult;
   Synchronize((WPARAM) ProgressDialog::aaUserAborted, (LPARAM) &bResult);
   return bResult;
}

void ThreadSafeProgress::WaitForAbort()
{
   Synchronize((WPARAM) ProgressDialog::aaWaitForAbort, 0);
}

bool ThreadSafeProgress::Show(bool show)
{
   bool bResult = show;
   Synchronize((WPARAM) ProgressDialog::aaShow, (LPARAM) &show);
   return bResult;
}

void ThreadSafeProgress::Lock(bool doLock)
{
   Synchronize((WPARAM) ProgressDialog::aaLock, (LPARAM) doLock);
}


bool ThreadSafeProgress::Main(HANDLE event)
{
   HANDLE events[2] = { myFinishedEvent, event };
   int c = 1;
   if (event != 0)
      c++;
   if (WaitWithMsgQueue(c, (HANDLE *) &events, false, INFINITE) == WAIT_OBJECT_0 + 1)
   {
      return true;
   }
   else
   {
      ResetEvent(myFinishedEvent);
      return false;
   }
}


void ThreadSafeProgress::Finished()
{
   Synchronize((WPARAM) ProgressDialog::aaFinished, 0);
   SetEvent(myFinishedEvent);
}



void ThreadSafeProgress::Synchronize(WPARAM wParam, LPARAM lParam)
{
    SendMessage((HWND) myForwardTo->GetHWND(), ProgressDialog::WM_SYNC, wParam, lParam);
}
