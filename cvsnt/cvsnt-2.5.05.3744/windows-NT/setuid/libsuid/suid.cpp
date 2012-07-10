/*	cvsnt suid library
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
// suid.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "suid.h"
#include "../setuid/setuid.h"

static int EnableTcbPrivilege()
{
	PTOKEN_PRIVILEGES NewToken;
	ULONG NewTokenLength;
	int n;
	LUID TempLuid;
	HANDLE hProcessToken;

	/* Enable seTcbName */
	if(!LookupPrivilegeValue(NULL,SE_TCB_NAME,&TempLuid))
		return GetLastError();

	if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hProcessToken))
		return GetLastError();

	GetTokenInformation(hProcessToken,TokenPrivileges,NULL,0,&NewTokenLength);
	NewToken=(PTOKEN_PRIVILEGES)malloc(NewTokenLength);
	GetTokenInformation(hProcessToken,TokenPrivileges,NewToken,NewTokenLength,&NewTokenLength);
	for(n=0; n<(int)NewToken->PrivilegeCount; n++)
	{
		if(!memcmp(&NewToken->Privileges[n].Luid,&TempLuid,sizeof(LUID)))
		{
			NewToken->Privileges[n].Attributes=SE_PRIVILEGE_ENABLED;
			break;
		}
	}
	if(n==(int)NewToken->PrivilegeCount)
	{
		CloseHandle(hProcessToken);
		free(NewToken);
		return ERROR_ACCESS_DENIED;
	}
	if(!AdjustTokenPrivileges(hProcessToken,FALSE,NewToken,0,0,NULL))
	{
		CloseHandle(hProcessToken);
		free(NewToken);
		return GetLastError();
	}
	CloseHandle(hProcessToken);
	free(NewToken);

	return 0;
}

/* Switch to user w/o password.  Uses the setuid authentication package */
DWORD SuidGetImpersonationTokenW(LPCWSTR szUser, LPCWSTR szDomain, DWORD dwLogonType, HANDLE *phToken)
{
	LSA_HANDLE hLsa;
	ULONG Id;
	NTSTATUS err,stat;
	SetUidParms in;
	LSA_OPERATIONAL_MODE mode;
	void *Profile;
	ULONG ProfileLen;
	LUID Luid;
	TOKEN_SOURCE Source = { SETUID_PACKAGENAME, { 0, 101 } };
	LSA_STRING Origin = { (USHORT)strlen(SETUID_PACKAGENAME), (USHORT)sizeof(SETUID_PACKAGENAME), SETUID_PACKAGENAME };
	QUOTA_LIMITS Quota = {0};

	/* Setup the command packet */
	in.Command=SETUID_BECOME_USER;
	wcscpy(in.BecomeUser.Username,szDomain);
	wcscat(in.BecomeUser.Username,L"\\");
	wcscat(in.BecomeUser.Username,szUser);

	err = EnableTcbPrivilege();
	if(err)
		return err;

	/* Authenticate our process with LSASS - this does the relevant privilege checks */
	if(!err)
		err = LsaRegisterLogonProcess(&Origin,&hLsa,&mode);
	/* Find the setuid package and call it */
	if(!err)
		err = LsaLookupAuthenticationPackage(hLsa,&Origin,&Id);
	if(!err)
		err = LsaLogonUser(hLsa, &Origin, (SECURITY_LOGON_TYPE)dwLogonType, Id, (PVOID)&in,(ULONG)sizeof(in),NULL, &Source, &Profile, &ProfileLen, &Luid, phToken, &Quota, &stat);
	if(err)
	{
		if(hLsa)
			LsaDeregisterLogonProcess(hLsa);
		return LsaNtStatusToWinError(err);
	}

	/* Finished with the privileged bit */
	LsaDeregisterLogonProcess(hLsa);

	return 0;
}

DWORD SuidGetImpersonationTokenA(LPCSTR szUser, LPCSTR szDomain, DWORD dwLogonType, HANDLE *phToken)
{
	wchar_t wszUser[UNLEN+1];
	wchar_t wszDomain[DNLEN+1];

	wszUser[MultiByteToWideChar(CP_THREAD_ACP,0,szUser,-1,wszUser,sizeof(wszUser))]='\0';
	wszDomain[MultiByteToWideChar(CP_THREAD_ACP,0,szDomain,-1,wszDomain,sizeof(wszDomain))]='\0';
	return SuidGetImpersonationTokenW(wszUser,wszDomain,dwLogonType,phToken);
}
