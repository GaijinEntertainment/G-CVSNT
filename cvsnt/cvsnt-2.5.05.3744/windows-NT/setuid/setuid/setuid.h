#ifndef SETUID__H
#define SETUID__H

#ifdef __cplusplus
extern "C" {
#endif

#define SETUID_BECOME_USER 0

struct SetUidParms
{
	WORD Command;
	union
	{
		struct
		{
			WCHAR Username[256+UNLEN+1]; // Username in DOMAIN\User format
		} BecomeUser;
	};
};

#define SETUID_PACKAGENAME "Setuid"
#define SETUID_PACKAGENAME_W L"Setuid"

#ifdef __cplusplus
}
#endif

#endif
