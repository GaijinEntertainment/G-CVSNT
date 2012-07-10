/**
	\file
	Root splitter classes.

	\author
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd
*/
/*
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
#ifndef ROOTSPLITTER__H
#define ROOTSPLITTER__H

///< Split a root string into its elements.
class CRootSplitter
{
public:
	CVSTOOLS_EXPORT CRootSplitter() { }
	CVSTOOLS_EXPORT virtual ~CRootSplitter() { }

	/// Split string.  Initialises member variables.
	/// with details.  Understands format
	/// :protocol;keywords:user:password\@server:port:/directory*module.

	CVSTOOLS_EXPORT bool Split(const char *root);

	/// Rejoin the string, optionally including password if available.
	CVSTOOLS_EXPORT const char *Join(bool password = false);

	cvs::string m_root;		///< Full root passed to last Split call or built using last Join.
	cvs::string m_protocol; ///< Connection protocol.
	cvs::string m_keywords; ///< Any extra keywords supplied.
	cvs::string m_username; ///< Username.
	cvs::string m_password; ///< Password, if any.
	cvs::string m_server;	///< Server.
	cvs::string m_port;		///< Port.  If this is blank assume port 2401.
	cvs::string m_directory; ///< Repository.
	cvs::string m_module;	///< Module within repository.
};

#endif

