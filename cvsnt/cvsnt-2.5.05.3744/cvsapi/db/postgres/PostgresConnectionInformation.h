/*
	CVSNT Generic API
    Copyright (C) 2006 Tony Hoyle and March-Hare Software Ltd

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
#ifndef POSTGRESCONNECTIONINFORMATION__H
#define POSTGRESCONNECTIONINFORMATION__H

class CPostgresConnectionInformation : public CSqlConnectionInformation
{
	friend class CPostgresConnection;
public:
	CPostgresConnectionInformation() { }
	virtual ~CPostgresConnectionInformation() { }

	virtual const char *getVariable(const char *name);
	virtual bool setVariable(const char *name, const char *value);
	virtual const char *enumVariableNames(size_t nVar); 

	virtual bool connectionDialog(const void *parentWindow);

protected:
	cvs::string schema;

#ifdef _WIN32
	static BOOL CALLBACK ConnectionDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
};

#endif

