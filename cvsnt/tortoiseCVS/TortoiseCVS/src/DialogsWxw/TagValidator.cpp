// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2005 - Torsten Martinsen
// <torsten@tiscali.dk> - August 2005

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

#include "stdafx.h"
#include "TagValidator.h"

IMPLEMENT_DYNAMIC_CLASS(TagValidator, wxValidator)

BEGIN_EVENT_TABLE(TagValidator, wxValidator)
    EVT_CHAR(TagValidator::OnChar)
END_EVENT_TABLE()

TagValidator* TagValidator::Clone() const
{
   return new TagValidator(myAllowRevision);
}

bool TagValidator::Validate(wxWindow* parent)
{
   return true;
}
   
void TagValidator::OnChar(wxKeyEvent& event)
{
    if ( m_validatorWindow )
    {
        int keyCode = event.GetKeyCode();

        // we don't filter special keys and Delete
        if (!(keyCode < WXK_SPACE || keyCode == WXK_DELETE || keyCode > WXK_START))
        {
           if ((keyCode == '.') && myAllowRevision)
           {
              event.Skip();
              return;
           }
           if (!isalpha(keyCode) && !isdigit(keyCode) && (keyCode != '-') && (keyCode != '_'))
              // eat message
              return;
        }
    }

    event.Skip();
}
