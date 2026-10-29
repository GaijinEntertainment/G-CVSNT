/* Declarations for file attribute munging features.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifndef FILEATTR_H
#define FILEATTR_H 

/* Directory owner and ACLs, as XML.  */

#define CVSREP_OLDOWNER "/.owner"
#define CVSREP_OLDPERMS "/.perms"
#define CVSREP_FILEATTR "CVS/fileattr.xml"

/* Prepare for a new directory with repository REPOS.  If REPOS is NULL,
   then prepare for a "non-directory"; the caller can call fileattr_write
   and fileattr_free, but must not call fileattr_get or fileattr_set.  */
void fileattr_startdir(const char *repos);

/* Search for a given node.  A little like xpath but not nearly as complex */
CXmlNodePtr fileattr_find(CXmlNodePtr root, const char *exp, ...);

CXmlNodePtr fileattr_getroot();

/* Return the next node on this level with this name, for walking lists */
CXmlNodePtr fileattr_next(CXmlNodePtr node);

/* Delete the node and its subtree now, and mark the tree modified. */
void fileattr_batch_delete(CXmlNodePtr root);

/* If this node has no children, delete it & recurse upwards.  Rinse. Wash. Repeat. */
void fileattr_prune(CXmlNodePtr node);

/* Get a single value from a node.  Pass null to get value of this node. */
const char *fileattr_getvalue(CXmlNodePtr root, const char *name);

/* Write out all modified attributes.  */
void fileattr_write();

/* Free all memory allocated by fileattr_*.  */
void fileattr_free();

/* Force modified */
void fileattr_modified();

void _fileattr_read(CXmlNodePtr& root, const char *repos);
CXmlNodePtr _fileattr_find(CXmlNodePtr node, const char *exp, ...);

#endif /* fileattr.h */
