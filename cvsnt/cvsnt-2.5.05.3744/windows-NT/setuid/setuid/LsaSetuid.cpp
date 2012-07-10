/*	cvsnt LSA Setuid
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
#include "stdafx.h"
#include "setuid.h"
#include "sid.h"
#include "LsaSetuid.h"

DEFINE_SID1(sidEveryone, 1, 0);
DEFINE_SID1(sidLocal, 2, 0);
DEFINE_SID1(sidNetwork, 5, 2);
DEFINE_SID1(sidBatch, 5, 3);
DEFINE_SID1(sidService, 5, 6);
DEFINE_SID1(sidAnonymous, 5, 7);
DEFINE_SID1(sidInteractive, 5, 4);
DEFINE_SID1(sidAuthenticated, 5, 11);
DEFINE_SID1(sidSystem, 5, 18);
DEFINE_SID2(sidAdministrators, 5, 32, 544);

#define HEAP_SIZE 4096

/*
	This is Win2K LSA Setuid code, based on CVSNT Setuid, which is based on Cygwin Setuid.
*/

void SuidDebug(const wchar_t *fmt, ...)
{
	wchar_t buf[1024]={0};
	va_list va;
	va_start(va,fmt);
	buf[_vsnwprintf(buf,sizeof(buf),fmt,va)]='\0';
	va_end(va);
	OutputDebugString(buf);
}

LPVOID LsaAllocateLsa(ULONG size)
{
	return g_pSec->AllocateLsaHeap(size);
}

static LPVOID LsaAllocateHeap(ULONG size, LPBYTE& HeapBase, LPBYTE& HeapPtr)
{
	LPVOID p;

	if(HeapPtr + size > HeapBase + HEAP_SIZE)
	{
		DEBUG(L"Out of reserved heap space.  Increase HEAP_SIZE and recompile.");
		return NULL;
	}
	p = HeapPtr;
	HeapPtr += size;
	DEBUG(L"LsaAllocateHeap(%d) - Heapsize %d\n",size,HeapPtr-HeapBase);
	return p; 
}

void LsaFreeLsa(LPVOID p)
{
	g_pSec->FreeLsaHeap(p);
}

LSA_STRING *AllocateLsaStringLsa(LPCSTR szString)
{
	LSA_STRING *s;
	size_t len = strlen(szString);

	s = (LSA_STRING *) LsaAllocateLsa(sizeof(LSA_STRING));
	if(!s)
		return NULL;
	s->Buffer = (char *) LsaAllocateLsa((ULONG) len+1);
	s->Length = (USHORT)len;
	s->MaximumLength = (USHORT)len+1;
	strcpy(s->Buffer,  szString);
	return s;
}

static UNICODE_STRING *AllocateUnicodeStringHeap(LPCWSTR szString, LPBYTE& HeapBase, LPBYTE& HeapPtr)
{
	UNICODE_STRING *s;
	size_t len = wcslen(szString)*sizeof(wchar_t);

	s = (UNICODE_STRING *) LsaAllocateHeap((ULONG)sizeof(UNICODE_STRING)+len+sizeof(wchar_t), HeapBase, HeapPtr);
	if(!s)
		return NULL;
	s->Buffer = (wchar_t *) (s+1);
	s->Length = (USHORT)len;
	s->MaximumLength = (USHORT)len+sizeof(wchar_t);
	wcscpy(s->Buffer,  szString);
	return s;
}

UNICODE_STRING *AllocateUnicodeStringLsa(LPCWSTR szString)
{
	UNICODE_STRING *s;
	size_t len = wcslen(szString)*sizeof(wchar_t);

	s = (UNICODE_STRING *) LsaAllocateLsa((ULONG)sizeof(UNICODE_STRING));
	if(!s)
		return NULL;
	s->Buffer = (wchar_t *) LsaAllocateLsa((ULONG)len+sizeof(wchar_t));
	s->Length = (USHORT)len;
	s->MaximumLength = (USHORT)len+sizeof(wchar_t);
	wcscpy(s->Buffer,  szString);
	return s;
}

static NTSTATUS LookupSid(LPCWSTR szSid, PSID* pUserSid, SID_NAME_USE* Use, LPBYTE& HeapBase, LPBYTE& HeapPtr)
{
	DWORD UserSidSize=0,DomainSize=0;
	wchar_t szDomain[DNLEN];

	*Use=SidTypeInvalid;
	*pUserSid=NULL;

	LookupAccountNameW(NULL,szSid,NULL,&UserSidSize,NULL,&DomainSize,NULL);
	if(!UserSidSize)
	{
		DEBUG(L"LookupAccountName(%s) failed pass 1 : no account? (%08x)",szSid,GetLastError());
		return STATUS_NO_SUCH_USER;
	}
	*pUserSid=(PSID)LsaAllocateHeap(UserSidSize, HeapBase, HeapPtr);
	if(!*pUserSid)
		return STATUS_NO_MEMORY;
	if(!LookupAccountNameW(NULL,szSid,*pUserSid,&UserSidSize,szDomain,&DomainSize,Use))
	{
		*pUserSid=NULL;
		DEBUG(L"LookupAccountName(%s) failed pass 2 : no account? (%08x)",szSid,GetLastError());
		return STATUS_NO_SUCH_USER;
	}
	return STATUS_SUCCESS;
}

static NTSTATUS GetPrimaryGroup (LPCWSTR wszMachine, LPCWSTR wszUser, PSID UserSid, PSID* PrimarySid, LPBYTE& HeapBase, LPBYTE& HeapPtr)
{
  LPUSER_INFO_3 buf;
  UCHAR count;
  ULONG size = GetLengthSid(UserSid);
  DWORD err;

  if ((err=NetUserGetInfo(wszMachine, wszUser, 3, (LPBYTE *) &buf))!=0)
  {
		DEBUG(L"NetUserGetInfo failed - error %d\n",err);
		return STATUS_NO_SUCH_USER;
  }
  *PrimarySid=(PSID)LsaAllocateHeap(size, HeapBase, HeapPtr);
  if(!*PrimarySid)
  {
    NetApiBufferFree (buf);
    return STATUS_NO_MEMORY;
  }
  CopySid(size,*PrimarySid,UserSid);
  if (IsValidSid(*PrimarySid) && (count = *GetSidSubAuthorityCount (*PrimarySid)) > 1)
      *GetSidSubAuthority (*PrimarySid, count - 1) = buf->usri3_primary_group_id;
  NetApiBufferFree (buf);
  return STATUS_SUCCESS;
}

static NTSTATUS GetDacl(PACL* pAcl, PSID UserSid, PTOKEN_GROUPS Groups, LPBYTE& HeapBase, LPBYTE& HeapPtr)
{
	int n;
	ULONG daclSize = sizeof(ACL)+sizeof(ACCESS_ALLOWED_ACE)*3+GetLengthSid(UserSid)+sizeof(sidAdministrators)+sizeof(sidSystem)+64;

	*pAcl = (PACL)LsaAllocateHeap(daclSize, HeapBase, HeapPtr);
	if(!pAcl)
	  return STATUS_NO_MEMORY;
	if (!InitializeAcl(*pAcl, daclSize, ACL_REVISION))
		return STATUS_NO_MEMORY;
	for(n=0; n<(int)Groups->GroupCount; n++)
	{
		if(!Groups->Groups[n].Sid)
			continue;
		if(EqualSid(Groups->Groups[n].Sid,&sidAdministrators))
		{
			if (!AddAccessAllowedAce(*pAcl, ACL_REVISION, GENERIC_ALL, &sidAdministrators))
				return STATUS_NO_MEMORY;
			break;
		}
    }
	if (!AddAccessAllowedAce(*pAcl, ACL_REVISION, GENERIC_ALL, UserSid))
	{
		return STATUS_NO_MEMORY;
	}

	if (!AddAccessAllowedAce(*pAcl, ACL_REVISION, GENERIC_ALL, &sidSystem))
	{
		return STATUS_NO_MEMORY;
	}
	return STATUS_SUCCESS;
}

/* We must use LSA_TOKEN_INFORMATION_V2 because in XP there's a bug in the LSA that corrupts
   its own heap if you use the _V1 variant.  _V2 expects all its information in a single block of
   data, which it frees all at once - the structure is otherwise identical */
NTSTATUS GetTokenInformationv2(LPCWSTR wszMachine,LPCWSTR wszDomain, LPCWSTR wszUser,LSA_TOKEN_INFORMATION_V2** TokenInformation, PLUID LogonId)
{
	int retval;
	PSID UserSid = NULL, pTmpSid;
	LPGROUP_USERS_INFO_0 GlobalGroups = NULL;
	LPGROUP_USERS_INFO_0 LocalGroups = NULL;
	DWORD NumGlobalGroups=0,TotalGlobalGroups=0;
	DWORD NumLocalGroups=0,TotalLocalGroups=0;
	PTOKEN_GROUPS pTokenGroups = NULL;
	PTOKEN_PRIVILEGES TokenPrivs = NULL;
	wchar_t grName[256+256];
	int n,j,p,q;
	SID_NAME_USE Use;
	LSA_OBJECT_ATTRIBUTES lsa = { sizeof(LSA_OBJECT_ATTRIBUTES) };
	PUNICODE_STRING lsaUserRights;
	DWORD NumUserRights;
	PUNICODE_STRING lsaMachine;
	LSA_HANDLE hLsa=NULL;
	SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
	LPBYTE HeapPtr, HeapBase;

	try
	{
		*TokenInformation = (LSA_TOKEN_INFORMATION_V2*)LsaAllocateLsa(sizeof(LSA_TOKEN_INFORMATION_V2)+HEAP_SIZE);
		if(!*TokenInformation)
		  throw STATUS_NO_MEMORY;
		HeapPtr = HeapBase = (LPBYTE)((*TokenInformation)+1);

		g_pSec->ImpersonateClient();
		if(wszMachine && wszMachine[0])
		{
			lsaMachine = AllocateUnicodeStringLsa(wszMachine);
			if(!lsaMachine)
				throw STATUS_NO_MEMORY;
			DEBUG(L"Querying LSA database on %s\n",wszMachine);
			if((retval=LsaOpenPolicy(lsaMachine,&lsa,POLICY_EXECUTE,&hLsa))!=STATUS_SUCCESS)
			{
				DEBUG(L"LsaOpenPolicy (%s) failed (%08x:%d)\n",wszMachine,retval,LsaNtStatusToWinError(retval));
				LsaFreeLsa(lsaMachine->Buffer);
				LsaFreeLsa(lsaMachine);
				throw retval;
			}
		}
		else 
		{
			DEBUG(L"Querying local LSA database\n");
			if((retval=LsaOpenPolicy(NULL,&lsa,POLICY_EXECUTE,&hLsa))!=STATUS_SUCCESS)
			{
				DEBUG(L"LsaOpenPolicy (local) failed (%08x:%d)\n",retval,LsaNtStatusToWinError(retval));
				throw retval;
			}
		}

		if(!wszDomain || !wszDomain[0])
			wcscpy(grName,wszUser);
		else
			wsprintfW(grName,L"%s\\%s",wszDomain,wszUser); // Used for domain/user clashes

		/* Search on the specified PDC, then on the local domain, for the user.
		This allows for trusted domains to work */
		if((retval=LookupSid(grName,&UserSid,&Use,HeapBase,HeapPtr))!=STATUS_SUCCESS || Use!=SidTypeUser)
		{
			DEBUG(L"LookupSid failed (%08x,%d)\n",retval,(int)Use);
			throw retval?retval:STATUS_NO_SUCH_USER;
		}

		(*TokenInformation)->Owner.Owner=NULL;
		(*TokenInformation)->User.User.Attributes=0;
		(*TokenInformation)->User.User.Sid=UserSid;

		NetUserGetGroups(wszMachine,wszUser,0,(LPBYTE*)&GlobalGroups,MAX_PREFERRED_LENGTH,&NumGlobalGroups,&TotalGlobalGroups);
		NetUserGetLocalGroups(wszMachine,wszUser,0,0,(LPBYTE*)&LocalGroups,MAX_PREFERRED_LENGTH,&NumLocalGroups,&TotalLocalGroups);

		pTokenGroups = (PTOKEN_GROUPS)LsaAllocateHeap(sizeof(TOKEN_GROUPS)+sizeof(SID_AND_ATTRIBUTES)*(NumGlobalGroups + NumLocalGroups + NumGlobalGroups), HeapBase, HeapPtr);
		if(!pTokenGroups)
			throw STATUS_NO_MEMORY;
		pTokenGroups->GroupCount = NumGlobalGroups + NumLocalGroups;

		j=0;
		for(n=0; n<(int)NumLocalGroups; n++)
		{
			if((retval=LookupSid(LocalGroups[n].grui0_name,&pTmpSid,&Use,HeapBase,HeapPtr))==STATUS_SUCCESS && pTmpSid)
			{
				if(memcmp(GetSidIdentifierAuthority(pTmpSid),&nt,sizeof(nt)) ||
				   *GetSidSubAuthority(pTmpSid,0)!=SECURITY_BUILTIN_DOMAIN_RID)
				{
					pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_RESOURCE;
				}
				else
					pTokenGroups->Groups[j].Attributes=0;
				pTokenGroups->Groups[j].Sid=pTmpSid;
				DEBUG(L"Adding local group (%d) %s\n",j,LocalGroups[n].grui0_name);
				j++;
			}
		}

		for(n=0; n<(int)NumGlobalGroups; n++)
		{
			if((retval=LookupSid(GlobalGroups[n].grui0_name,&pTmpSid,&Use,HeapBase,HeapPtr))==STATUS_SUCCESS && pTmpSid)
			{
				for(q=0; q<j; q++)
				{
					if(!pTokenGroups->Groups[q].Sid)
						continue;
					if(EqualSid(pTokenGroups->Groups[q].Sid,pTmpSid))
						break;
				}
				if(q==j)
				{
					if(memcmp(GetSidIdentifierAuthority(pTmpSid),&nt,sizeof(nt)) ||
					*GetSidSubAuthority(pTmpSid,0)!=SECURITY_BUILTIN_DOMAIN_RID)
					{
						pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT;
					}
					else
						pTokenGroups->Groups[j].Attributes=0;
					pTokenGroups->Groups[j].Sid=pTmpSid;
					DEBUG(L"Adding global group (%d) %s\n",j,GlobalGroups[n].grui0_name);
					j++;
				}
			}
		}

		pTokenGroups->GroupCount = j;
		DEBUG(L"Added %d groups\n",j);

		if((retval=GetPrimaryGroup(wszMachine,wszUser,UserSid,&(*TokenInformation)->PrimaryGroup.PrimaryGroup,HeapBase,HeapPtr))!=STATUS_SUCCESS)
		{
			DEBUG(L"GetPrimaryGroup failed\n");
			throw retval;
		}

		j=0;
		lsaUserRights=NULL;
		NumUserRights=0;
		if((retval=LsaEnumerateAccountRights(hLsa,UserSid,&lsaUserRights,&NumUserRights))==STATUS_SUCCESS)
		{
			DEBUG(L"LsaEnumerateAccountRights (user) returned %d rights\n",NumUserRights);
			j+=NumUserRights;
			NetApiBufferFree(lsaUserRights);
		}
		else
		{
			if(LsaNtStatusToWinError(retval)!=2)
				DEBUG(L"LsaEnumerateAccountRights (user) failed (%08x:%d)\n",retval,LsaNtStatusToWinError(retval));
		}

		for(n=0; n<(int)pTokenGroups->GroupCount; n++)
		{
			lsaUserRights=NULL;
			NumUserRights=0;
			if((retval=LsaEnumerateAccountRights(hLsa,pTokenGroups->Groups[n].Sid,&lsaUserRights,&NumUserRights))==STATUS_SUCCESS)
			{
				DEBUG(L"LsaEnumerateAccountRights (group) returned %d rights\n",NumUserRights);
				j+=NumUserRights;
				NetApiBufferFree(lsaUserRights);
			}
			else
			{
				if(LsaNtStatusToWinError(retval)!=2)
					DEBUG(L"LsaEnumerateAccountRights (group) failed (%08x:%d)\n",retval,LsaNtStatusToWinError(retval));
			}
		}
		DEBUG(L"Possible %d group rights\n",j);

		TokenPrivs=(PTOKEN_PRIVILEGES)LsaAllocateHeap(sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)*j, HeapBase, HeapPtr);
		if(!TokenPrivs)
			throw STATUS_NO_MEMORY;
		TokenPrivs->PrivilegeCount=j;
		j=0;
		if((retval=LsaEnumerateAccountRights(hLsa,UserSid,&lsaUserRights,&NumUserRights))==STATUS_SUCCESS)
		{
			for(n=0; n<(int)NumUserRights; n++)
			{
				TokenPrivs->Privileges[j].Attributes=SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;
				if(!LookupPrivilegeValueW(wszMachine,lsaUserRights[n].Buffer,&TokenPrivs->Privileges[j].Luid))
				{
					DEBUG(L"LookupPrivilegeValue(%s) failed (%d)\n",lsaUserRights[n].Buffer,GetLastError());
					continue;
				}
				DEBUG(L"User: Adding (%d) %s\n",j,lsaUserRights[n].Buffer);
				j++;
			}
			NetApiBufferFree(lsaUserRights);
		}

		for(n=0; n<(int)pTokenGroups->GroupCount; n++)
		{
			if((retval=LsaEnumerateAccountRights(hLsa,pTokenGroups->Groups[n].Sid,&lsaUserRights,&NumUserRights))==STATUS_SUCCESS)
			{
				for(p=0; p<(int)NumUserRights; p++)
				{
					LUID luid;
					if(!LookupPrivilegeValueW(wszMachine,lsaUserRights[p].Buffer,&luid))
					{
						DEBUG(L"LookupPrivilegeValue(%s) failed (%d)\n",lsaUserRights[p].Buffer,GetLastError());
						continue;
					}
					for(q=0; q<j; q++)
						if(!memcmp(&luid,&TokenPrivs->Privileges[q].Luid,sizeof(luid)))
							break;
					if(q==j)
					{
						DEBUG(L"Group: Adding (%d) %s\n",j,lsaUserRights[p].Buffer);
						TokenPrivs->Privileges[j].Attributes=SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;
						TokenPrivs->Privileges[j].Luid=luid;
						j++;
					}
				}
				NetApiBufferFree(lsaUserRights);
			}
		}

		TokenPrivs->PrivilegeCount=j;
		DEBUG(L"Added %d rights\n",j);

		/* Strip out the BUILTIN stuff */
		for(n=pTokenGroups->GroupCount; n>=0; --n)
		{
			if(pTokenGroups->Groups[n].Attributes==0)
			{
				if((int)pTokenGroups->GroupCount>n)
					memcpy(pTokenGroups->Groups+n,pTokenGroups->Groups+n+1,sizeof(pTokenGroups->Groups[0])*(pTokenGroups->GroupCount-n));
				pTokenGroups->GroupCount--;
			}
		}

		DEBUG(L"%d groups after cleanup\n",pTokenGroups->GroupCount);

		if((retval=GetDacl(&(*TokenInformation)->DefaultDacl.DefaultDacl,UserSid,pTokenGroups,HeapBase,HeapPtr))!=STATUS_SUCCESS)
			throw retval;

		(*TokenInformation)->Groups = pTokenGroups;
		(*TokenInformation)->Privileges = TokenPrivs;

		retval = STATUS_SUCCESS;
	}
	catch(NTSTATUS status)
	{
		if(*TokenInformation)
		{
			LsaFreeLsa(*TokenInformation);
			*TokenInformation=NULL;
		}
		retval = status;
	}

	if(GlobalGroups)
		NetApiBufferFree(GlobalGroups);
	if(LocalGroups)
		NetApiBufferFree(LocalGroups);
	if(hLsa)
		LsaClose(hLsa);

	RevertToSelf();

	return retval;
}

