/**
	\file
	Server connection classes.

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
#ifndef SERVERCONNECTION__H
#define SERVERCONNECTION__H

/// Information used to connect automatically to a server.
/// This structure is used by all functions that require server
/// information, as well as the common dialogs.
struct ServerConnectionInfo
{
	ServerConnectionInfo() { isfile=user=enumerated=anonymous=invalid=isevs=false; level=-1; }
	int level;				///< Notional level within heirarchy.  0=server, 1=modules, 2+=directories.
	cvs::string server;		///< Server name.
	cvs::string port;		///< Port.  If blank assume port 2401.
	cvs::string root;		///< Full text of last root in cvsroot format.
	cvs::string directory;	///< Directory within root.
	cvs::string tag;		///< Tag selected within server heirarchy.  Not used during connection.
	cvs::string anon_user;	///< Anonymous username from server discovery.
	cvs::string anon_proto;	///< Anonymous protocol from server discovery.  Normally pserver.
	cvs::string default_proto; ///< Default protocol from server discovery.
	cvs::string protocol;	///< Current configured protocol.  A special value of 'ftp' marks this as an ftp server.
	cvs::string keywords;	///< Any keywords defined within the current root.
	cvs::string username;	///< Current username.
	cvs::string password;	///< Current password.
	cvs::string module;		///< Virtual module within heirarchy.  Not used during connection.
	cvs::string srvname;	///< Readable name of server, normally derived from discovery.
	bool isfile;			///< true if this structure refers to a file.
	bool isbinary;			///< true if this structure refers to a binary file;
	bool user;				///< true if this structure refers to a user defined server.
	bool enumerated;		///< true if this structure was found during autodiscovery.
	bool anonymous;			///< true if anonymous logins are allowed by this server.
	bool invalid;			///< true if this server could not be contacted so the information may be incorrect.
	bool isevs;				///< true if the connecting server is evs based, false if cvsnt based
};

/// Errors generated during connection.
enum ServerConnectionError
{
	SCESuccessful,			///< No error.
	SCEFailedNoAnonymous,	///< Failed to connect, and anonymous logins are not enabled for the server.
	SCEFailedBadExec,		///< Couldn't execute the cvs client to attempt login.
	SCEFailedConnection,	///< cvs client returned connection failure.
	SCEFailedBadProtocol,	///< cvs client protocol initialisaton failed.
	SCEFailedNoSupport,		///< The test command is not supported by the server.
	SCEFailedCommandAborted	///< The test command failed for an unknown reason.
};

/// Callback interface used by CServerConnection.
class CServerConnectionCallback
{
public:
	/// Ask the user for a password.
	/// \param current Current server configuration under test.
	/// \return true if user pressed OK, false otherwise.
	virtual bool AskForPassword(ServerConnectionInfo *current) =0;
	/// Report an error.
	/// \param current Current server configuration under test.
	/// \param error Type of error being generated
	virtual void Error(ServerConnectionInfo *current, ServerConnectionError error) =0;
	/// Process output from test command.
	/// \param line Line of output.  Most errors have been filtered but the client should still be capable of coping if unexpected output occurs.
	virtual void ProcessOutput(const char *line) =0;
};

/// Attempt to connect to a server.  This class will attempt to derive the
/// correct connection details from the supplied information, and will
/// call the functions in CServerConnectionCallback as required.  The calling
/// application is expected to interact with the user.
class CServerConnection
{
public:
	CVSTOOLS_EXPORT CServerConnection() { }
	CVSTOOLS_EXPORT virtual ~CServerConnection() { }

	/// Attempt the server connection.
	/// \param command Command to use to test connection.  This is usually something like 'ls' or 'ver'.
	/// \param info Initialised server information.  This will be modified by this function as needed.
	/// \param callback Callback structure which is called as the function gathers information.
	/// \param debugFn undocumented
	/// \param *userData undocumented
	/// \return true if successful, false otherwise.  The ServerConnectionInfo structure may be modified in both cases.
	CVSTOOLS_EXPORT bool Connect(const char *command, ServerConnectionInfo *info, CServerConnectionCallback* callback, int (*debugFn)(int type, const char *,size_t, void *)=NULL, void *userData=NULL);

private:
	int m_error;
	CServerConnectionCallback* m_callback;

	static int _ServerOutput(const char *data,size_t len,void *param);
	int ServerOutput(const char *data,size_t len);
};

#endif
