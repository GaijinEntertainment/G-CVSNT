#include "cvs.h"

#include <windows.h>
#include <shlwapi.h>
#define SECURITY_WIN32
#include <security.h>
#include <ntsecapi.h>
#include <ntdsapi.h>
#include <dsgetdc.h>
#include <lm.h>
#include <activeds.h>
#include <tchar.h>
#include <malloc.h>

#pragma comment(lib,"adsiid.lib")

#include <comdef.h>
struct  _LARGE_INTEGER_X {
 struct {
  DWORD LowPart;
  LONG HighPart;
 } u;
};
// This stops the compiler from complaining about negative values in unsigned longs.
#pragma warning(disable:4146)
#pragma warning(disable:4192)
#import <activeds.tlb>  rename("_LARGE_INTEGER","_LARGE_INTEGER_X") rename("GetObject","_GetObject")
#pragma warning(default:4146)
#pragma warning(default:4192)

#include "sid.h"

static MAKE_SID1(sidEveryone, 1, 0);
static MAKE_SID1(sidLocal, 2, 0);
static MAKE_SID1(sidNetwork, 5, 2);
static MAKE_SID1(sidBatch, 5, 3);
static MAKE_SID1(sidService, 5, 6);
static MAKE_SID1(sidAnonymous, 5, 7);
static MAKE_SID1(sidInteractive, 5, 4);
static MAKE_SID1(sidAuthenticated, 5, 11);
static MAKE_SID1(sidSystem, 5, 18);
static MAKE_SID2(sidAdministrators, 5, 32, 544);

/*

  This is a funny bit of code... it emulates setuid on NT.
  Any process with 'create token' can become any other user, with a bit of magic.
  (This is normally just LocalSystem).

  I did actually think of this independently, but realised very quickly that the
  cygwin folks had already done something like this.  This is based mostly on
  their implementation, with a bit of re-interpretation of MSDN by me.

  It leaks a bit (not 100% sure why) but it's only called a couple of times
  during the execution of cvs so that's not a problem.

  Prototype:
	void nt_setuid_init();  -- Initialise
	int nt_setuid(LPCSTR szMachine, LPCSTR szUser); -- Do the stuff

*/

#ifndef __cplusplus
typedef enum { false, true } bool;
#endif

typedef NTSTATUS (NTAPI *NtCreateToken_t)
		(PHANDLE, /* Address of created token */
		 ACCESS_MASK, /* Access granted to object (TOKEN_ALL_ACCESS) */
		 PLSA_OBJECT_ATTRIBUTES,
	     TOKEN_TYPE, /* Token type (Primary/Impersonation) */
		 PLUID, /* Authentication Id (From GetTokenInformation) or SYSTEM_LUID */
		 PLARGE_INTEGER, /* exp - unknown 0x7FFFFFFFFFFFFFFFLL */
		 PTOKEN_USER,	/* Owner SID for this token */
		 PTOKEN_GROUPS, /* Group SIDs in this token */
		 PTOKEN_PRIVILEGES, /* List of rights for this user */
		 PTOKEN_OWNER, /* Default SID for created objects */
		 PTOKEN_PRIMARY_GROUP, /* Primary group for created objects */
		 PTOKEN_DEFAULT_DACL, /* Default DACL for created objects */
		 PTOKEN_SOURCE /* Source of this token (App name) */
		 );

static NtCreateToken_t NtCreateToken=NULL;

static bool LookupSid(LPCWSTR szMachine, LPCWSTR szUser, PSID* pUserSid, SID_NAME_USE* Use)
{
	DWORD UserSidSize=0,DomainSize=0;
	wchar_t *szDomain = NULL;

	*Use=SidTypeInvalid;

	LookupAccountNameW(szMachine,szUser,NULL,&UserSidSize,NULL,&DomainSize,NULL);
	if(!UserSidSize || !DomainSize)
		return false;
	*pUserSid=(PSID)malloc(UserSidSize);
	szDomain=(wchar_t*)malloc(DomainSize*sizeof(wchar_t));
	if(!LookupAccountNameW(szMachine,szUser,*pUserSid,&UserSidSize,szDomain,&DomainSize,Use))
	{
		free(szDomain);
		free(pUserSid);
		return false;
	}
	free(szDomain);
	return true;
}

void nt_setuid_init()
{
	HMODULE hNtDll;

	hNtDll=LoadLibrary(_T("ntdll.dll"));
	NtCreateToken = (NtCreateToken_t)GetProcAddress(hNtDll,"NtCreateToken");
}

static bool GetPrimaryGroup (LPCWSTR wszMachine, LPCWSTR wszUser, PSID UserSid, PSID* PrimarySid)
{
  LPUSER_INFO_3 buf;
  bool ret = false;
  UCHAR count;

  if (NetUserGetInfo (wszMachine, wszUser, 3, (LPBYTE *) &buf))
    return false;
  *PrimarySid=malloc(_msize(UserSid));
  CopySid(_msize(UserSid),*PrimarySid,UserSid);
  if (IsValidSid (*PrimarySid) && (count = *GetSidSubAuthorityCount (*PrimarySid)) > 1)
  {
      *GetSidSubAuthority (*PrimarySid, count - 1) = buf->usri3_primary_group_id;
      ret = true;
  }
  NetApiBufferFree (buf);
  return ret;
}

static bool GetDacl(PACL* pAcl, PSID UserSid, PTOKEN_GROUPS Groups)
{
	int n;

	*pAcl = (PACL)malloc(4096);
	if (!InitializeAcl(*pAcl, 4096, ACL_REVISION))
	{
		free(*pAcl);
		return false;
	}
	for(n=0; n<(int)Groups->GroupCount; n++)
	{
		if(EqualSid(Groups->Groups[n].Sid,&sidAdministrators))
		{
			if (!AddAccessAllowedAce(*pAcl, ACL_REVISION, GENERIC_ALL, &sidAdministrators))
			{
				free(*pAcl);
				return false;
			}
			break;
		}
    }
	if (!AddAccessAllowedAce(*pAcl, ACL_REVISION, GENERIC_ALL, UserSid))
	{
		free(*pAcl);
		return false;
	}

	if (!AddAccessAllowedAce(*pAcl, ACL_REVISION, GENERIC_ALL, &sidSystem))
	{
		free(*pAcl);
		return false;
	}
	return true;
}

static bool GetAuthLuid(LUID* pLuid)
{
	HANDLE hToken;
	TOKEN_STATISTICS stats;
	DWORD size;

	if (!OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &hToken))
		return false;
	if (!GetTokenInformation (hToken, TokenStatistics, &stats, sizeof stats, &size))
		return false; 

	*pLuid=stats.AuthenticationId; 
	return true;
}

int nt_setuid(LPCWSTR wszMachine, LPCWSTR wszUser, HANDLE *phToken)
{
	int retval = -1;
	PSID UserSid = NULL, pTmpSid;
	LPGROUP_USERS_INFO_0 GlobalGroups = NULL;
	LPGROUP_USERS_INFO_0 LocalGroups = NULL;
	DWORD NumGlobalGroups=0,TotalGlobalGroups=0;
	DWORD NumLocalGroups=0,TotalLocalGroups=0;
	TOKEN_USER _TokenUser = {0};
	TOKEN_OWNER _TokenOwner = {0};
	PTOKEN_GROUPS pTokenGroups = NULL;
	PTOKEN_PRIVILEGES TokenPrivs = NULL;
	TOKEN_PRIMARY_GROUP _TokenPrimaryGroup = {0};
	TOKEN_SOURCE _TokenSource = {0};
	TOKEN_DEFAULT_DACL TokenDacl = {0};
	wchar_t grName[256];
	int n,j,p,q;
	SID_NAME_USE Use;
	LSA_OBJECT_ATTRIBUTES lsa = { sizeof(LSA_OBJECT_ATTRIBUTES) };
	LSA_HANDLE hLsa=NULL;
	LSA_UNICODE_STRING lsaMachine;
	PLSA_UNICODE_STRING lsaUserRights;
	DWORD NumUserRights;
	LUID AuthLuid;
	HANDLE hToken = INVALID_HANDLE_VALUE;
	HANDLE hPrimaryToken = INVALID_HANDLE_VALUE;
	HANDLE hProcessToken = INVALID_HANDLE_VALUE;
	DWORD err;
	
	SECURITY_QUALITY_OF_SERVICE sqos =
		{ sizeof sqos, SecurityImpersonation, SECURITY_STATIC_TRACKING, FALSE };
	LSA_OBJECT_ATTRIBUTES oa =
		{ sizeof oa, 0, 0, 0, 0, &sqos };
	SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
	LARGE_INTEGER exp = { 0xffffffff,0x7fffffff } ;

	PTOKEN_PRIVILEGES NewToken;
	DWORD NewTokenLength;
	LUID TempLuid;

	/* If init failed, or not called, then exit immediately */
	if(!NtCreateToken)
	   return -1;

	if(wszMachine)
	{
		lsaMachine.Length=wcslen(wszMachine)*sizeof(wchar_t);
		lsaMachine.MaximumLength=lsaMachine.Length+sizeof(wchar_t);
		lsaMachine.Buffer=(wchar_t*)wszMachine;
	}

	if(!LookupPrivilegeValue(NULL,SE_CREATE_TOKEN_NAME,&TempLuid))
	{
		goto nt_setuid_out;
	}
	if(!OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY,&hProcessToken))
	{
		goto nt_setuid_out;
	}
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
		goto nt_setuid_out;
	}
	if(!AdjustTokenPrivileges(hProcessToken,FALSE,NewToken,0,0,NULL))
	{
		goto nt_setuid_out;
	}

	if((err=LsaOpenPolicy(wszMachine?&lsaMachine:NULL,&lsa,POLICY_EXECUTE,&hLsa))!=ERROR_SUCCESS)
	{
		err=LsaNtStatusToWinError(err);
		goto nt_setuid_out;
	}

	/* From Q185246 - username = domain name */
	wsprintfW(grName,L"%s\\%s",wszUser,wszUser); // Used for domain/user clashes

	/* Search on the specified PDC, then on the local domain, for the user.
	   This allows for trusted domains to work */
	if((!LookupSid(wszMachine,wszUser,&UserSid,&Use) || Use!=SidTypeUser) &&
	   (!LookupSid(wszMachine,grName,&UserSid,&Use) || Use!=SidTypeUser) &&
	   (!LookupSid(NULL,wszUser,&UserSid,&Use) || Use!=SidTypeUser) &&
	   (!LookupSid(NULL,grName,&UserSid,&Use) || Use!=SidTypeUser))
	{
		goto nt_setuid_out;
	}

	_TokenOwner.Owner=UserSid;
	_TokenUser.User.Attributes=0;
	_TokenUser.User.Sid=UserSid;


	NetUserGetGroups(wszMachine,wszUser,0,(LPBYTE*)&GlobalGroups,MAX_PREFERRED_LENGTH,&NumGlobalGroups,&TotalGlobalGroups);
	NetUserGetLocalGroups(wszMachine,wszUser,0,0,(LPBYTE*)&LocalGroups,MAX_PREFERRED_LENGTH,&NumLocalGroups,&TotalLocalGroups);

	pTokenGroups = (PTOKEN_GROUPS)malloc(sizeof(TOKEN_GROUPS)+sizeof(SID_AND_ATTRIBUTES)*(NumGlobalGroups + NumLocalGroups + NumGlobalGroups + 5));
	pTokenGroups->GroupCount = NumGlobalGroups + NumLocalGroups + 5;

	for(n=0,j=0; n<(int)NumGlobalGroups; n++)
	{
		if(LookupSid(wszMachine,GlobalGroups[n].grui0_name,&pTmpSid,&Use))
		{
			pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT;
			pTokenGroups->Groups[j].Sid=pTmpSid;
			j++;
		}
	}
	for(n=0; n<(int)NumLocalGroups; n++)
	{
		if(LookupSid(wszMachine,LocalGroups[n].grui0_name,&pTmpSid,&Use))
		{
			pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY;
			pTokenGroups->Groups[j].Sid=pTmpSid;
			j++;
		}
	}

	if(!GetAuthLuid(&AuthLuid))
	{
		goto nt_setuid_out;
	}
	if(!GetPrimaryGroup(wszMachine,wszUser,UserSid,&_TokenPrimaryGroup.PrimaryGroup))
	{
		goto nt_setuid_out;
	}

	pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY;
	pTokenGroups->Groups[j].Sid=&sidLocal;
	j++;
	pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY;
	pTokenGroups->Groups[j].Sid=&sidInteractive;
	j++;
	pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY;
	pTokenGroups->Groups[j].Sid=&sidAuthenticated;
	j++;
	pTokenGroups->Groups[j].Attributes=SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY;
	pTokenGroups->Groups[j].Sid=&sidEveryone;
	j++;
	{
		PSID pUserSid;
		SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
		AllocateAndInitializeSid(&nt,3,SECURITY_LOGON_IDS_RID,AuthLuid.HighPart,AuthLuid.LowPart,0,0,0,0,0,&pUserSid);
		pTokenGroups->Groups[j].Attributes=SE_GROUP_LOGON_ID|SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY;
		pTokenGroups->Groups[j].Sid=pUserSid;
		j++;
	}

	TokenPrivs=(PTOKEN_PRIVILEGES)calloc(1,sizeof(TOKEN_PRIVILEGES));
	j=0; NumUserRights=0;
	if(LsaEnumerateAccountRights(hLsa,UserSid,&lsaUserRights,&NumUserRights)==ERROR_SUCCESS)
	{
		TokenPrivs->PrivilegeCount=NumUserRights;
		TokenPrivs=(PTOKEN_PRIVILEGES)realloc(TokenPrivs,sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)*TokenPrivs->PrivilegeCount);

		for(n=0,j=0; n<(int)NumUserRights; n++)
		{
			TokenPrivs->Privileges[j].Attributes=SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;
			LookupPrivilegeValueW(wszMachine,lsaUserRights->Buffer,&TokenPrivs->Privileges[j].Luid);
			j++;
		}
		NetApiBufferFree(lsaUserRights);
	}
	TokenPrivs->PrivilegeCount=j;

	for(n=0; n<(int)pTokenGroups->GroupCount; n++)
	{
		if(LsaEnumerateAccountRights(hLsa,pTokenGroups->Groups[n].Sid,&lsaUserRights,&NumUserRights)==ERROR_SUCCESS)
		{
			TokenPrivs->PrivilegeCount+=NumUserRights;
			TokenPrivs=(PTOKEN_PRIVILEGES)realloc(TokenPrivs,sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)*TokenPrivs->PrivilegeCount);
			for(p=0; p<(int)NumUserRights; p++)
			{
				LUID luid;
				if(!wcscmp(lsaUserRights[p].Buffer,L"SeAssignPrimaryTokenPrivilege") ||
				   !wcscmp(lsaUserRights[p].Buffer,L"SeUndockPrivilege") ||
				   !wcscmp(lsaUserRights[p].Buffer,L"SeEnableDelegationPrivilege"))
					continue; /* NTCreateToken will barf if these are set */
				if(!LookupPrivilegeValueW(wszMachine,lsaUserRights[p].Buffer,&luid))
					continue;
				for(q=0; q<j; q++)
					if(!memcmp(&luid,&TokenPrivs->Privileges[q].Luid,sizeof(luid)))
						break;
				if(q==j)
				{
					TokenPrivs->Privileges[j].Attributes=SE_PRIVILEGE_ENABLED | SE_PRIVILEGE_ENABLED_BY_DEFAULT;
					TokenPrivs->Privileges[j].Luid=luid;
					j++;
				}
			}
			NetApiBufferFree(lsaUserRights);
		}
	}

	TokenPrivs->PrivilegeCount=j;
	TokenPrivs=(PTOKEN_PRIVILEGES)realloc(TokenPrivs,sizeof(TOKEN_PRIVILEGES)+sizeof(LUID_AND_ATTRIBUTES)*TokenPrivs->PrivilegeCount);

	memset(&_TokenSource,0,sizeof(_TokenSource));
	strcpy(_TokenSource.SourceName,"cvsnt");
	_TokenSource.SourceIdentifier.HighPart = 0;
	_TokenSource.SourceIdentifier.LowPart = 0x0101;

	if(!GetDacl(&TokenDacl.DefaultDacl,UserSid,pTokenGroups))
	{
		goto nt_setuid_out;
	}

	if((err=NtCreateToken (&hToken, TOKEN_ALL_ACCESS, &oa, TokenImpersonation,
		   &AuthLuid, &exp, &_TokenUser, pTokenGroups, TokenPrivs, &_TokenOwner, &_TokenPrimaryGroup,
		   &TokenDacl, &_TokenSource))!=ERROR_SUCCESS)
	{
		err=LsaNtStatusToWinError(err);
		goto nt_setuid_out;
	}

	if(!DuplicateTokenEx(hToken,TOKEN_ALL_ACCESS,&sa,SecurityImpersonation,TokenImpersonation,&hPrimaryToken))
	{
		goto nt_setuid_out;
	}

	if(phToken)
		*phToken = hPrimaryToken;
	else
		CloseHandle(hPrimaryToken);

	retval = 0;

nt_setuid_out:
	free(UserSid);
	NetApiBufferFree(GlobalGroups);
	NetApiBufferFree(LocalGroups);
	free(pTokenGroups);
	free(TokenPrivs); 
	free(_TokenPrimaryGroup.PrimaryGroup); 
	free(NewToken);
	if(hToken!=INVALID_HANDLE_VALUE)
		CloseHandle(hToken);
	if(hProcessToken!=INVALID_HANDLE_VALUE)
		CloseHandle(hProcessToken);
	free(TokenDacl.DefaultDacl);
	LsaClose(hLsa); 
	return retval;
}

/* S4U call on Win2k3 */
/* XP actually implements this but drops out early in the processing
	because it's in workstation mode not server */
/* Also for this to succeed your PDC needs to be a Win2k3 machine */
int nt_s4u(LPCWSTR wszDomain, LPCWSTR wszUser, HANDLE *phToken)
{
	DWORD err;
	struct
	{
		KERB_S4U_LOGON s4uLogon;
		WCHAR Upn[UNLEN+DNLEN+2];
	} Buf = { { KerbS4ULogon } };
	LSA_STRING Origin = { 4,5, "CVSNT" };
	TOKEN_SOURCE Source = { "CVSNT", 0, 101 };
	LUID Luid, TempLuid;
	NTSTATUS SubStatus;
	BYTE* Profile;
	ULONG ProfileLength;
	HANDLE hLsa, hProcessToken;
	LSA_OPERATIONAL_MODE Mode;
	PTOKEN_PRIVILEGES NewToken;
	ULONG NewTokenLength;
	int n;
	QUOTA_LIMITS Quotas;
	TOKEN_GROUPS Tok = {0};

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
		return ERROR_ACCESS_DENIED;
	}
	if(!AdjustTokenPrivileges(hProcessToken,FALSE,NewToken,0,0,NULL))
	{
		CloseHandle(hProcessToken);
		return GetLastError();
	}
	CloseHandle(hProcessToken);
	free(NewToken);

	/* Logon */
	err = LsaRegisterLogonProcess(&Origin, &hLsa, &Mode);
	if(err)
		return LsaNtStatusToWinError(err);

	try
	{
		ActiveDs::IADsNameTranslatePtr info(CLSID_NameTranslate);

		wsprintfW(Buf.Upn,L"%s\\%s",wszDomain,wszUser);
		TRACE(3,"S4U untranslated name: %S",Buf.Upn);

		info->Init(ADS_NAME_INITTYPE_GC,L"");
		info->Set(ADS_NAME_TYPE_NT4,Buf.Upn);
		lstrcpyW(Buf.Upn,info->Get(ADS_NAME_TYPE_USER_PRINCIPAL_NAME));

		TRACE(3,"S4U UPN: %S",Buf.Upn);
	}
	catch(_com_error e)
	{
		TRACE(3,"IADS query failed: %S",e.ErrorMessage());
		return ERROR_INVALID_FUNCTION;
	}

	Buf.s4uLogon.ClientUpn.Buffer=Buf.Upn;
	Buf.s4uLogon.ClientUpn.MaximumLength=Buf.s4uLogon.ClientUpn.Length=wcslen(Buf.Upn)*sizeof(wchar_t);
	
	err = LsaLogonUser(hLsa, &Origin, Network, LOGON32_PROVIDER_DEFAULT, &Buf, sizeof(Buf), &Tok, &Source, (PVOID*)&Profile, &ProfileLength, &Luid, phToken, &Quotas, &SubStatus);
	LsaDeregisterLogonProcess(hLsa);
	TRACE(3,"S4U: LsaLogonUser returned %08x (%08x)",err,LsaNtStatusToWinError(err));
	return LsaNtStatusToWinError(err);
}
