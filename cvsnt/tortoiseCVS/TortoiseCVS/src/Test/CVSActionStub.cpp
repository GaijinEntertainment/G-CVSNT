// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - November 2005

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "StdAfx.h"
#include "../CVSGlue/CVSAction.h"

CVSAction::CVSAction(wxWindow* parent, int flags)
{
}

void CVSAction::SetStreamFile(const std::string& file)
{
}

void CVSAction::SetCloseIfOK(bool closeIfOK)
{
}

void CVSAction::SetCloseAnyway(bool closeAnyway)
{
}

void CVSAction::SetClose(close_t how)
{
}

CVSAction::close_t CVSAction::GetClose() const
{
    return CloseNever;
}

void CVSAction::SetSilent(bool silent)
{
}


bool CVSAction::Command(const std::string& currentDirectory, MakeArgs& args)
{
   return true;
}


CVSAction::~CVSAction()
{
}

bool CVSAction::AtLeastOneSuccess()
{
   return true;
}


void CVSAction::LockProgressDialog(bool)
{
}

std::string CVSAction::GetOutputText()
{
   return "";
}



void CVSAction::SetCVSRoot(const CVSRoot& cvsroot)
{
}

void CVSAction::SetProgressCaption(const wxString& msg)
{
}

void CVSAction::SetProgressFinishedCaption(const wxString& msg)
{
}

ProgressDialog* CVSAction::GetProgressDialog()
{
    return 0;
}

void CVSAction::SetHideStdout(bool hide)
{
}

bool CVSAction::Aborted()
{
   return false;
}
