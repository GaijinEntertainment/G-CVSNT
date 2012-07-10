/*	cvsnt LSA setuid
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
// setuid.cpp : Defines the entry point for the DLL application.
//

// Add the dll name (setuid) to
// HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Lsa\Authentication Packages
// and copy to the windows system directory.

#include "stdafx.h"
#include "setuid.h"
#include "LsaSetuid.h"

PLSA_SECPKG_FUNCTION_TABLE g_pSec;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    return TRUE;
}

NTSTATUS NTAPI LsaApCallPackage(
  PLSA_CLIENT_REQUEST ClientRequest,
  PVOID ProtocolAuthenticationInformation,
  PVOID ClientBufferBase,
  ULONG AuthenticationInformationLength,
  PVOID* ProtocolReturnBuffer,
  PULONG ReturnBufferLength,
  PNTSTATUS ProtocolStatus
)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTAPI LsaApCallPackageUntrusted(
  PLSA_CLIENT_REQUEST ClientRequest,
  PVOID ProtocolAuthenticationInformation,
  PVOID ClientBufferBase,
  ULONG AuthenticationInformationLength,
  PVOID* ProtocolReturnBuffer,
  PULONG ReturnBufferLength,
  PNTSTATUS ProtocolStatus
)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTAPI LsaApCallPackagePassthrough(
  PLSA_CLIENT_REQUEST ClientRequest,
  PVOID ProtocolAuthenticationInformation,
  PVOID ClientBufferBase,
  ULONG AuthenticationInformationLength,
  PVOID* ProtocolReturnBuffer,
  PULONG ReturnBufferLength,
  PNTSTATUS ProtocolStatus
)
{
	return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS NTAPI LsaApInitializePackage(
  ULONG AuthenticationPackageId,
  PLSA_DISPATCH_TABLE LsaDispatchTable,
  PLSA_STRING Database,
  PLSA_STRING Confidentiality,
  PLSA_STRING* AuthenticationPackageName
)
{
	/* The cast isn't documented, but it's really the same structure */
	g_pSec = (PLSA_SECPKG_FUNCTION_TABLE)LsaDispatchTable;

	(*AuthenticationPackageName) = AllocateLsaStringLsa(SETUID_PACKAGENAME);
	return STATUS_SUCCESS;
}

NTSTATUS NTAPI LsaApLogonUser(
  PLSA_CLIENT_REQUEST ClientRequest,
  SECURITY_LOGON_TYPE LogonType,
  PVOID AuthenticationInformation,
  PVOID ClientAuthenticationBase,
  ULONG AuthenticationInformationLength,
  PVOID* ProfileBuffer,
  PULONG ProfileBufferLength,
  PLUID LogonId,
  PNTSTATUS SubStatus,
  PLSA_TOKEN_INFORMATION_TYPE TokenInformationType,
  PVOID* TokenInformation,
  PUNICODE_STRING* AccountName,
  PUNICODE_STRING* AuthenticatingAuthority
)
{
	__try
	{
		PLSA_TOKEN_INFORMATION_V2 LocalTokenInformation;
		NTSTATUS err;
		wchar_t wszDomain[DNLEN+1] ={0};
		wchar_t wszUser[UNLEN+1];
		wchar_t wszComputer[DNLEN+1];
		DWORD dwLen;
		USER_INFO_1 *ui1 = NULL;
		PDOMAIN_CONTROLLER_INFOW pdi = NULL;

		DEBUG(L"Setuid (%S) started\n", __DATE__);
		if(AuthenticationInformationLength!=sizeof(SetUidParms))
		{
			DEBUG(L"AuthenticationInformationLength != sizeof(SetUidParms_In): INVALID_PARAMETER\n");
			return STATUS_INVALID_PARAMETER;
		}

		SetUidParms *in = (SetUidParms *)AuthenticationInformation;

		if(in->Command!=SETUID_BECOME_USER)
		{
			DEBUG(L"in->Command!=SETUID_BECOME_USER: INVALID_PARAMETER\n");
			return STATUS_INVALID_PARAMETER;
		}

		/* Username is always domain\user */
		if(!in->BecomeUser.Username || !wcschr(in->BecomeUser.Username, '\\'))
		{
			DEBUG(L"in->Username invalid (%s): INVALID_PARAMETER\n",in->BecomeUser.Username?in->BecomeUser.Username:L"null");
			return STATUS_INVALID_PARAMETER;
		}

		wcsncpy(wszDomain,in->BecomeUser.Username,wcschr(in->BecomeUser.Username,'\\')-in->BecomeUser.Username);
		wcscpy(wszUser,wcschr(in->BecomeUser.Username,'\\')+1);

		if(AccountName)
			*AccountName = AllocateUnicodeStringLsa(wszUser);

		if(AuthenticatingAuthority)
		{
			if(wszDomain[0])
				*AuthenticatingAuthority = AllocateUnicodeStringLsa(wszDomain);
			else
			{
				DWORD dwLen = sizeof(wszDomain);
				GetComputerNameW(wszDomain,&dwLen);
				*AuthenticatingAuthority = AllocateUnicodeStringLsa(wszDomain);
				wszDomain[0]='\0';
			}
		}

		dwLen = sizeof(wszComputer)/sizeof(wszComputer[0]);
		GetComputerNameW(wszComputer,&dwLen);

		DEBUG(L"Domain: %s\n",wszDomain);
		DEBUG(L"Computer: %s\n",wszComputer);
		DEBUG(L"User: %s\n",wszUser);

		if(!wcscmp(wszComputer,wszDomain))
		{
			wszDomain[0]='\0';
			DEBUG(L"Local computer authentication\n");
		}

		if(wszDomain[0] && (err=DsGetDcNameW(NULL,wszDomain,NULL,NULL,DS_IS_FLAT_NAME,&pdi) && DsGetDcNameW(NULL,wszDomain,NULL,NULL,DS_IS_DNS_NAME,&pdi))!=0)
		{
			DEBUG(L"Domain not found (%s): err=%0x08x\n",wszDomain,err);
			if(pdi) NetApiBufferFree(pdi);
			return STATUS_NO_SUCH_DOMAIN;
		}
		
		if(pdi)
		{
			DEBUG(L"Domain Controller: %s\n",pdi->DomainControllerName);
		}
		else
		{
			DEBUG(L"No domain controller found\n");
		}

		if((err=NetUserGetInfo(pdi?pdi->DomainControllerName:NULL, wszUser, 1, (LPBYTE*)&ui1))!=0)
		{
			switch(err)
			{
			case ERROR_ACCESS_DENIED:
				DEBUG(L"User not found (%s): ACCESS DENIED\n",wszUser);
				err = STATUS_ACCESS_DENIED;
				break;
			case NERR_InvalidComputer:
				DEBUG(L"User not found (%s): Invalid computer\n",wszUser);
				err = STATUS_NO_SUCH_DOMAIN;
				break;
			case NERR_UserNotFound:
				DEBUG(L"User not found (%s): No such user\n",wszUser);
				err = STATUS_NO_SUCH_USER;
				break;
			default:
				DEBUG(L"User not found (%s): Unknown error %08x\n",wszUser,err);
				err = STATUS_NO_SUCH_USER;
				break;
			}
			if(pdi) NetApiBufferFree(pdi);
			if(ui1) NetApiBufferFree(ui1);
			return err;
		}

		if(ui1->usri1_flags&UF_ACCOUNTDISABLE)
		{
			DEBUG(L"Account disabled: ACCOUNT_DISABLED\n");
			*SubStatus = STATUS_ACCOUNT_DISABLED;
			if(pdi) NetApiBufferFree(pdi);
			if(ui1) NetApiBufferFree(ui1);
			return STATUS_ACCOUNT_RESTRICTION;
		}
		if(ui1->usri1_flags&UF_PASSWORD_EXPIRED)
		{
			DEBUG(L"Password expired: PASSWORD_EXPIRED\n");
			*SubStatus = STATUS_PASSWORD_EXPIRED;
			if(pdi) NetApiBufferFree(pdi);
			if(ui1) NetApiBufferFree(ui1);
			return STATUS_ACCOUNT_RESTRICTION;
		}
		if(ui1->usri1_flags&(UF_WORKSTATION_TRUST_ACCOUNT|UF_SERVER_TRUST_ACCOUNT|UF_INTERDOMAIN_TRUST_ACCOUNT))
		{
			DEBUG(L"Is a trust account: ACCOUNT_RESTRICTION\n");
			*SubStatus = STATUS_ACCOUNT_DISABLED;
			if(pdi) NetApiBufferFree(pdi);
			if(ui1) NetApiBufferFree(ui1);
			return STATUS_ACCOUNT_RESTRICTION;
		}
		NetApiBufferFree(ui1);

		if(!AllocateLocallyUniqueId(LogonId))
		{
			DEBUG(L"AllocateLocallyUniqueId failed (%d)\n",GetLastError());
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		err = GetTokenInformationv2(pdi?pdi->DomainControllerName:NULL,wszDomain,wszUser,&LocalTokenInformation,LogonId);

		if(err) 
		{
			DEBUG(L"GetTokenInformationv2 failed (%08x)\n",err);
			if(pdi) NetApiBufferFree(pdi);
			return err;
		}
		if(pdi) NetApiBufferFree(pdi);

		err = g_pSec->CreateLogonSession(LogonId);
		if(err)
		{
			DEBUG(L"CreateLogonSession failed (%d)\n",err);
			if(LocalTokenInformation)
				NetApiBufferFree(LocalTokenInformation);
			return err;
		}

		if(ProfileBuffer)
		{
			*ProfileBuffer=NULL;
			*ProfileBufferLength=0;
		}

		(*TokenInformationType)=LsaTokenInformationV2;
		(*TokenInformation)=LocalTokenInformation;

		DEBUG(L"Setuid Logon for %s\\%s returned OK\n", wszDomain, wszUser);
	}

	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		DEBUG(L"Setuid Logon generated NT exception\n");
		return GetExceptionCode();
	}

	return STATUS_SUCCESS;
}

void NTAPI LsaApLogonTerminated(
  PLUID LogonId
)
{
//	DEBUG(L"Setuid logon deleted\n");
}
