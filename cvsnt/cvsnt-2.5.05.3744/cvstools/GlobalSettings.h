/*
	CVSNT Helper application API
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
#ifndef GLOBAL_SETTINGS__H
#define GLOBAL_SETTINGS__H

#ifdef _WIN32
typedef __int64 LONGINT;
#else
typedef long long LONGINT;
#endif

class CGlobalSettings
{
public:
	enum GLDType
	{
		GLDLib,
		GLDProtocols,
		GLDTriggers,
		GLDXdiff,
		GLDMdns,
		GLDDatabase
	};

	CVSTOOLS_EXPORT CGlobalSettings() { } 
	CVSTOOLS_EXPORT virtual ~CGlobalSettings() { }

	CVSTOOLS_EXPORT static int GetUserValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len);
	CVSTOOLS_EXPORT static int GetUserValue(const char *product, const char *key, const char *value, cvs::string& sval);
	CVSTOOLS_EXPORT static int GetUserValue(const char *product, const char *key, const char *value, int& ival);
	CVSTOOLS_EXPORT static int GetUserValue(const char *product, const char *key, const char *value, LONGINT& lival);
	CVSTOOLS_EXPORT static int SetUserValue(const char *product, const char *key, const char *value, const char *buffer);
	CVSTOOLS_EXPORT static int SetUserValue(const char *product, const char *key, const char *value, int ival);
	CVSTOOLS_EXPORT static int SetUserValue(const char *product, const char *key, const char *value, LONGINT lival);
	CVSTOOLS_EXPORT static int EnumUserValues(const char *product, const char *key, int value_num, char *value, int value_len, char *buffer, int buffer_len);
	CVSTOOLS_EXPORT static int EnumUserKeys(const char *product, const char *key, int value_num, char *value, int value_len);
	CVSTOOLS_EXPORT static int DeleteUserValue(const char *product, const char *key, const char *value)
		{ return SetUserValue(product, key,value,(char*)NULL); }
	CVSTOOLS_EXPORT static int DeleteUserKey(const char *product, const char *key);

	CVSTOOLS_EXPORT static int GetGlobalValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len);
	CVSTOOLS_EXPORT static int GetGlobalValue(const char *product, const char *key, const char *value, cvs::string& sval);
	CVSTOOLS_EXPORT static int GetGlobalValue(const char *product, const char *key, const char *value, int& ival);
	CVSTOOLS_EXPORT static int GetGlobalValue(const char *product, const char *key, const char *value, LONGINT& lival);
	CVSTOOLS_EXPORT static int SetGlobalValue(const char *product, const char *key, const char *value, const char *buffer);
	CVSTOOLS_EXPORT static int SetGlobalValue(const char *product, const char *key, const char *value, int ival);
	CVSTOOLS_EXPORT static int SetGlobalValue(const char *product, const char *key, const char *value, LONGINT lival);
	CVSTOOLS_EXPORT static int EnumGlobalValues(const char *product, const char *key, int value_num, char *value, int value_len, char *buffer, int buffer_len);
	CVSTOOLS_EXPORT static int EnumGlobalKeys(const char *product, const char *key, int value_num, char *value, int value_len);
	CVSTOOLS_EXPORT static int DeleteGlobalValue(const char *product, const char *key, const char *value)
		{ return SetGlobalValue(product,key,value,(char*)NULL); }
	CVSTOOLS_EXPORT static int DeleteGlobalKey(const char *product, const char *key);

	CVSTOOLS_EXPORT static const char *GetConfigDirectory();
	CVSTOOLS_EXPORT static const char *GetCvsCommand();
	CVSTOOLS_EXPORT static const char *GetLibraryDirectory(GLDType type);
	CVSTOOLS_EXPORT static bool SetConfigDirectory(const char *directory);
	CVSTOOLS_EXPORT static bool SetLibraryDirectory(const char *directory);
	CVSTOOLS_EXPORT static bool SetCvsCommand(const char *command);

#ifdef _WIN32
	CVSTOOLS_EXPORT static bool isAdmin();
#endif

protected:
	CVSTOOLS_EXPORT static int _GetUserValue(const char *product, const char *key, const char *value, char *buffer, int buffer_len);
	CVSTOOLS_EXPORT static int _SetUserValue(const char *product, const char *key, const char *value, const char *buffer);

};

#endif
