#ifndef SUID__H
#define SUID__H

#ifdef __cplusplus
extern "C" {
#endif

DWORD SuidGetImpersonationTokenW(LPCWSTR szUser, LPCWSTR szDomain, DWORD dwLogonType, HANDLE *phToken);
DWORD SuidGetImpersonationTokenA(LPCSTR szUser, LPCSTR szDomain, DWORD dwLogonType, HANDLE *phToken);

#ifdef _UNICODE
#define SuidGetImpersonationToken SuidGetImpersonationTokenW
#else
#define SuidGetImpersonationToken SuidGetImpersonationTokenA
#endif

#ifdef __cplusplus
}
#endif

#endif
