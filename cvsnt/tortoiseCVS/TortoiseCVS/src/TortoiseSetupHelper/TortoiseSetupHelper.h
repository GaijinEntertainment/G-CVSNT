// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2006 - Torsten Martinsen
// <torsten@vip.cybercity.dk> - September 2006

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

#include <wx/app.h>

// Basically a dummy App, as we don't really want one
class SetupHelperApp : public wxApp
{
public:
   bool OnInit();
   int OnRun();
   int OnExit();
#if defined(_DEBUG)
   void OnUnhandledException();
#endif
};

DECLARE_APP(SetupHelperApp)
