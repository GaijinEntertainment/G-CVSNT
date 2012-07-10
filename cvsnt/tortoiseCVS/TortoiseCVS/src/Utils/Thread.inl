// $Id: Thread.inl,v 1.1 2012/03/04 01:06:55 aliot Exp $

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

#include <exception>

template<class RECEIVER>
Thread<RECEIVER>::Thread()
: myThreadHandle(NULL), myDieNowEvent(NULL), myNewWorkEvent(NULL)
{
}

// static 
template<class RECEIVER>
unsigned long __stdcall Thread<RECEIVER>::StaticMain(void *data)
{
	Thread<RECEIVER>* thread = (Thread<RECEIVER>*) data;
	((thread->myReceiver)->*(thread->myMainFunction))();
	return 0;
}

template<class RECEIVER>
void Thread<RECEIVER>::BeginThread(RECEIVER* receiver, MemberFunction mainFunction)
{
	myDieNowEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	if (!myDieNowEvent)
		MessageBoxA(NULL, "Failed to make myDieNowEvent in Thread.in", "SimpleMutex", MB_OK);
	myNewWorkEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
	if (!myNewWorkEvent)
		MessageBoxA(NULL, "Failed to make myNewWorkEvent in Thread.inl", "SimpleMutex", MB_OK);

	myReceiver = receiver;
	myMainFunction = mainFunction;

	myThreadHandle = ::CreateThread(
		0, // security - must be 0 on win98
		0, // stack size, or 0
		&StaticMain, // start of thread execution
		(void*) this, // argument list
		0, // can be CREATE_SUSPEND or 0.
		&myThreadID); // Thread id stored in here

	if (!myThreadHandle)
		MessageBoxA(NULL, "Failed to create thread in Thread.in", "SimpleMutex", MB_OK);
}

template<class RECEIVER>
void Thread<RECEIVER>::SignalNewWork()
{
	SetEvent(myNewWorkEvent);
}

template<class RECEIVER>
bool Thread<RECEIVER>::WaitForMoreWork()
{
	HANDLE objects[2];
	objects[0] = myDieNowEvent;
	objects[1] = myNewWorkEvent;

	DWORD dw = WaitForMultipleObjects( 2, objects, false, INFINITE );

	// Return true if we got more work, or false to die
	if (dw == WAIT_OBJECT_0 + 1 )
		return true;
	else
		return false;
}

template<class RECEIVER>
Thread<RECEIVER>::~Thread()
{
	if (myDieNowEvent && myThreadHandle)
	{
		SetEvent( myDieNowEvent );
		WaitForSingleObject(myThreadHandle, INFINITE);
	}

	if (myDieNowEvent)
		CloseHandle(myDieNowEvent);
	if (myNewWorkEvent)
		CloseHandle(myNewWorkEvent);
}
