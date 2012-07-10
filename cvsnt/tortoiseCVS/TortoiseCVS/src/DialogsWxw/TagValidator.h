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

#ifndef TAGVALIDATOR_H
#define TAGVALIDATOR_H

#include <wx/valgen.h>

// Validator for tags. Accepts letters, digits, dash and underscore.
class TagValidator : public wxValidator
{
public:
   TagValidator(bool allowRevision = false)
      : myAllowRevision(allowRevision)
   {
   }

   virtual ~TagValidator() {}

   virtual TagValidator* Clone() const;
      
   virtual bool Validate(wxWindow* parent);

   // Called to transfer data to the window
   virtual bool TransferToWindow() { return true; }

   // Called to transfer data to the window
   virtual bool TransferFromWindow() { return true; }

   void OnChar(wxKeyEvent& event);
   
   DECLARE_EVENT_TABLE()

   DECLARE_DYNAMIC_CLASS(TagValidator)

private:
   bool myAllowRevision;
};

#endif
