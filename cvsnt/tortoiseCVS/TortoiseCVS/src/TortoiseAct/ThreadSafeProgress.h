// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2000 - Francis Irving
// <francis@flourish.org> - Janurary 2001

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


#ifndef THREAD_SAFE_PROGRESS_H
#define THREAD_SAFE_PROGRESS_H

#include "../DialogsWxw/ProgressDialog.h"

#include <vector>

// Adaptor class to make any ProgressDialogAbstract thread safe
class ThreadSafeProgress
{
public:
   ThreadSafeProgress(ProgressDialog* forwardTo);
   virtual ~ThreadSafeProgress();

   // Factory
   static ThreadSafeProgress* CreateThreadSafeProgress(ProgressDialog* forwardTo);
   
   // Call by main thread to wait
   virtual bool Main(HANDLE event);

   // These can be called in a separate thread, while the
   // main GUI thread sits in Main() above
   virtual void NewText(const wxString& txt, 
                        ProgressDialog::TextType type = ProgressDialog::TTDefault);
   virtual void NewTextLines(const std::vector<std::string*>& txt, 
                             ProgressDialog::TextType type = ProgressDialog::TTDefault);
   virtual bool UserAborted();
   virtual void WaitForAbort();
   virtual bool Show(bool show);
   virtual void Lock(bool doLock = TRUE);
   // Call when done in worker thread
   virtual void Finished();
   
private:
   ProgressDialog* myForwardTo;
   HANDLE          myFinishedEvent;

   void Synchronize(WPARAM wParam, LPARAM lParam);
};


#endif
