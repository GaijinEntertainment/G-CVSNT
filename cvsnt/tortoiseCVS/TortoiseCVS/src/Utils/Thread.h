// $Id: Thread.h,v 1.1 2012/03/04 01:06:55 aliot Exp $

// InstallLib - library of code for making Windows installation programs
// Copyright (c) 2001, CyberLife Technology Ltd.
// All rights reserved.
 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//  * Neither the name of CyberLife Technology Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//    
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CYBERLIFE TECHNOLOGY 
// LTD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Class to encapsulate threading, for portability, and
// for ease of use with C++ (rather than static or C
// functions).

#ifndef TORTOISE_CVS_THREAD
#define TORTOISE_CVS_THREAD

#include "FixCompilerBugs.h"

#include <windows.h>
#include "FixWinDefs.h"
#include <process.h>

// Example thread main:
// void MyThread::ThreadMain()
// {
//		while (WaitForMoreWork())
//		{
//			// do work
//		}
//  }

// Instantiate Thread with RECEIVER as the class 
// which contains the thread main function that
// you want to call.
template<class RECEIVER>
class Thread
{
public:
	Thread<RECEIVER>();
	~Thread<RECEIVER>();

	// This is a pointer to a member function
	typedef void (RECEIVER::* MemberFunction) ();

	// Start a new thread.  The thread main function is in the
	// specificed receiver object, choosen with the member function
	// pointer.
	void BeginThread(RECEIVER* receiver, MemberFunction mainFunction);

	// Call this to wake the thread up - there's more work to do!
	void SignalNewWork();

	// Use this inside your thread main function
	// This waits until either SignalNewWork is called by the
	// outer code, or the thread is to terminate (when it
	// returns false).
	bool WaitForMoreWork();

private:
	static unsigned long __stdcall StaticMain(void *data);

	HANDLE myThreadHandle;
	unsigned long myThreadID;
	HANDLE myDieNowEvent;
	HANDLE myNewWorkEvent;

	RECEIVER* myReceiver;
	MemberFunction myMainFunction;
};

#include "Thread.inl"

#endif

