/*	CVS trigger library interface
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* _EXPORT */

#ifndef TRIGGER_INTERFACE__H
#define TRIGGER_INTERFACE__H

#ifdef __cplusplus
extern "C" {
#endif

/* Structure passed into loginfo function */
typedef struct change_info_t
{
	const char *filename;
	const char *rev_new;
	const char *rev_old;
	char type;
	const char *tag;
	const char *bugid;
} change_info;

typedef struct property_info_t
{
	const char *property;
	const char *value;
	int isglobal;
} property_info;

/* Main callback structure.  This should be returned as an intitialised structure
   by the GetCvsInfo structure. */
typedef struct trigger_interface_t
{
	plugin_interface plugin;

	int (*init)(const struct trigger_interface_t* cb, const char *command, const char *date, const char *hostname, const char *username, const char *virtual_repository, const char *physical_repository, const char *sessionid, const char *editor, int count_uservar, const char **uservar, const char **userval, const char *client_version, const char *character_set);
	int (*close)(const struct trigger_interface_t* cb);
	int (*pretag)(const struct trigger_interface_t* cb, const char *message, const char *directory, int name_list_count, const char **name_list, const char **version_list, char tag_type, const char *action, const char *tag);
	int (*verifymsg)(const struct trigger_interface_t* cb, const char *directory, const char *filename);
	int (*loginfo)(const struct trigger_interface_t* cb, const char *message, const char *status, const char *directory, int change_list_count, change_info_t *change_list);
	int (*history)(const struct trigger_interface_t* cb, char type, const char *workdir, const char *revs, const char *name, const char *bugid, const char *message);
	int (*notify)(const struct trigger_interface_t* cb, const char *message, const char *bugid, const char *directory, const char *notify_user, const char *tag, const char *type, const char *file);
	int (*precommit)(const struct trigger_interface_t* cb, int name_list_count, const char **name_list, const char *message, const char *directory);
	int (*postcommit)(const struct trigger_interface_t* cb, const char *directory);
	int (*precommand)(const struct trigger_interface_t* cb, int argc, const char **argv);
	int (*postcommand)(const struct trigger_interface_t* cb, const char *directory, int return_code);
	int (*premodule)(const struct trigger_interface_t* cb, const char *module);
	int (*postmodule)(const struct trigger_interface_t* cb, const char *module);
	int (*get_template)(const struct trigger_interface_t *cb, const char *directory, const char **template_ptr);
	int (*parse_keyword)(const struct trigger_interface_t *cb, const char *keyword,const char *directory,const char *file,const char *branch,const char *author,const char *printable_date,const char *rcs_date,const char *locker,const char *state,const char *version,const char *name,const char *bugid,const char *commitid,const property_info *props, size_t numprops, const char **value);
	int (*prercsdiff)(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed);
	int (*rcsdiff)(const struct trigger_interface_t *cb, const char *file, const char *directory, const char *oldfile, const char *newfile, const char *diff, size_t difflen, const char *type, const char *options, const char *oldversion, const char *newversion, unsigned long added, unsigned long removed);
} trigger_interface;

#ifdef __cplusplus
}
#endif

#endif
