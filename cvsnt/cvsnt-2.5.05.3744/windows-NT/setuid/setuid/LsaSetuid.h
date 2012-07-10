#ifndef LSASETUID__H
#define LSASETUID__H

/*
	This is Win2K/XP LSA Setuid code, based on CVSNT Setuid, which is based on Cygwin Setuid
*/

NTSTATUS GetTokenInformationv2(LPCWSTR wszMachine,LPCWSTR wszDomain, LPCWSTR wszUser,LSA_TOKEN_INFORMATION_V2** TokenInformation, PLUID LogonId);

#endif