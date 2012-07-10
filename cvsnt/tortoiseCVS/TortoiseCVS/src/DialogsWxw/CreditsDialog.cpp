// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - October 2003

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
#include "CreditsDialog.h"
#include "ExtTextCtrl.h"
#include "WxwHelpers.h"

#define WM_TCVS_SETFOCUS WM_APP + 1

// static
bool DoCreditsDialog(wxWindow* parent)
{
   bool bResult = false;
   CreditsDialog* dlg = new CreditsDialog(parent);
   if (dlg->ShowModal() == wxID_OK)
   {
      bResult = true;
   }

   DestroyDialog(dlg);
   return bResult;
}


// Constructor
CreditsDialog::CreditsDialog(wxWindow* parent)
   : ExtDialog(parent, -1,
               _("TortoiseCVS - Credits"),
               wxDefaultPosition, wxDefaultSize, 
               wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN)
{
   SetIcon(wxIcon(wxT("A_TORTOISE"), wxBITMAP_TYPE_ICO_RESOURCE));

   // Main box with everything in it
   wxBoxSizer *sizerTop = new wxBoxSizer(wxVERTICAL);

   static struct
   {
       int active;
       const wxChar* name;
       const wxChar* contribution;
   }
   contributors[] =
   {
       { 1, wxT("Adam Strzelecki"),     _("Better looking icons") },
       { 0, wxT("Alan Dabiri"),          _("Persistent dialog size patch, Annotate feature, various bugfixes and improvements") },
       { 1, wxT("Alex Guardiet"),        _("Catalan translation") },
       { 1, wxT("Alexandre Ratti"),      _("French translation") },
       { 0, wxT("Andy Hird"),            _("CVS columns in Explorer detailed list view") },
       { 1, wxT("Armin Müller"),         _("German translation") },
       { 0, wxT("Ben Campbell"),         _("Branching, tagging and merging, menu config file, various user interface improvements and bug fixes") },
       { 0, wxT("Bruno Jouhier"),        _("64-bit Windows port") },
       { 1, wxT("Carlos Mendez"),        _("Spanish translation") },
       { 0, wxT("Cedric Babault"),       _("French User Guide") },
       { 0, wxT("Claus Gårde Henriksen"),_("Danish translation") },
       { 0, wxT("David Pinelo"),         _("Spanish translation") },
       { 0, wxT("Daniel Buchmann"),      _("Norwegian translation") },
       { 0, wxT("Daniel Jackson"),       _("Icons") },
       { 0, wxT("Didier Trosset-Moreau"), _("Win32 native user interface (not used now, but provoked the migration from GTK to wxWindows)") },
       { 1, wxT("Dumitru Pletosu"),      _("Romanian translation") },
       { 1, wxT("Flavio Etrusco"),       _("Brazilian Portuguese translation") },
       { 0, wxT("Francis Irving"),       _("Project founder (no longer active), original code, Visual C++ and mingw32 compilation") },
       { 1, wxT("Frank Fesevur"),        _("Dutch translation") },
       { 0, wxT("Frank Listing"),        _("Revision Graph improvements") },
       { 0, wxT("Friedrich Brunzema"),   _("Warn on empty commit comment patch") },
       { 0, wxT("Gabriel Genellina"),    _("Visual Studio 2008 compilation") },
       { 0, wxT("Geoff Beier"),          _("Improved SSH support") },
       { 0, wxT("Georgi Beloev"),        _("Make new module patch") },
       { 1, wxT("Gottfried Ganssauge"),  _("Windows Vista patch") },
       { 0, wxT("Guru Kathiresan"),      _("Improved treelist control") },
       { 1, wxT("Gia Shervashidze"),     _("Georgian translation") },
       { 0, wxT("Hartmut Honisch"),      _("Core developer (no longer active)") },
       { 0, wxT("Ian Dees"),             _("cvs.exe pipe code prototype, Borland compilation, various bug fixes") },
       { 0, wxT("Ilya Slobodin"),        _("Commit dialog patch") },
       { 0, wxT("Jason Mills"),          _("Make New Module edit module path patch") },
       { 0, wxT("Joakim Beijar"),        _("SSPI encryption patch") },
       { 0, wxT("Jon Rowlands"),         _("CVS property page, context menu") },
       { 0, wxT("Jordan Ilchev"),        _("Default icons") },
       { 0, wxT("Juan Carlos Arevalo-Baeza"),    _("Various bug fixing patches") },
       { 1, wxT("Juan Diego Gutiérrez Gallardo"),    _("Spanish translation") },
       { 0, wxT("Keith D. Zimmerman"),   _("History dialog patch") },
       { 0, wxT("Kwon Tai-in"),          _("Korean translation") },
       { 1, wxT("Leszek Tomanek"),       _("Polish translation") },
       { 0, wxT("Liao Bin"),             _("Traditional Chinese translation") },
       { 1, wxT("Lee Jae-Hong"),         _("Korean translation") },
#ifdef MARCH_HARE_BUILD
       { 1, wxT("march-hare.com"),
         _("This build, edit options dialog, repository browser, defect tracking integration, 64-bit development platform") },
#else
       { 1, wxT("march-hare.com"),
         _("Edit options dialog, repository browser, defect tracking integration, 64-bit development platform") },
#endif
       { 0, wxT("Martin Crawford"),      _("Project management, User Guide, domain name, various patches") },
       { 1, wxT("Martin Srebotnjak"),    _("Slovenian translation") },
       { 0, wxT("Matt Powers"),          _("CVS property page improvements") },
       { 0, wxT("Nathan Thomas"),        _("Charlie Vernon Smythe logo and icon") },
       { 1, wxT("Nick Buse"),            _("Various bug fixing patches") },
       { 1, wxT("Nikolay Ponomarenko"),  _("Russian program translation") },
       { 1, wxT("Omer Koker"),           _("Turkish translation") },
       { 0, wxT("Radoslav Paskalev"),    _("rcsinfo patch") },
       { 0, wxT("Rex Tsai"),             _("Simplified Chinese translation") },
       { 0, wxT("shengfang"),            _("Simplified Chinese User Guide") },
       { 0, wxT("Simon Elén"),           _("Automatic use of Unix line endings patch") },
       { 0, wxT("Stefan Hoffmeister"),   _("Borland compilation, bug fix, CVS property page improvements") },
       { 0, wxT("Stephane Lajoie"),      _("mingw32 compilation") },
       { 0, wxT("Syed Asif"),            _("Web site design") },
       { 1, wxT("Torsten Martinsen"),    _("Core developer, release manager, documentation") },
       { 1, wxT("Vladimír Kloz"),        _("Czech translation") },
       { 1, wxT("Vladimir Serdyuk"),     _("Russian User Guide") },
       { 1, wxT("Yutaka Yokokawa"),      _("Japanese translation") },

       { 0, 0, 0 }
   };

   wxGridSizer* grid = new wxGridSizer(3, 0, 10);

   // Charlie image
   wxBitmap charlieMaskBitmap(wxT("BMP_CHARLIE_MASK"), wxBITMAP_TYPE_BMP_RESOURCE);
   wxMask* charlieMask = new wxMask(charlieMaskBitmap);
   wxBitmap charlieBitmap(wxT("BMP_CHARLIE"), wxBITMAP_TYPE_BMP_RESOURCE);
   charlieBitmap.SetMask(charlieMask);
   wxStaticBitmap* charlie = new wxStaticBitmap(this, -1, charlieBitmap, wxDefaultPosition);

   wxString s(_("People who have contributed to TortoiseCVS\n(mouse over name to see details)"));
   s += _(":");
   ExtTextCtrl* text = new ExtTextCtrl(this, -1, s,
                                       wxDefaultPosition, wxDefaultSize,
                                       wxTE_READONLY | wxBORDER_NONE | 
                                       wxTE_MULTILINE | wxTE_NO_VSCROLL | wxTE_PROCESS_TAB);
   wxSize headingsize = text->GetTextSize();
   text->SetSize(headingsize);
   text->SetBackgroundColour(this->GetBackgroundColour());
   wxBoxSizer* sizerHeading = new wxBoxSizer(wxHORIZONTAL);
   sizerHeading->Add(25, 0);
   sizerHeading->Add(charlie, 0, wxALIGN_CENTER | wxALL, 10);
   sizerHeading->Add(text, 0, wxGROW | wxALL, 10);
   sizerHeading->Add(25, 0);
   sizerHeading->SetItemMinSize(text, headingsize.GetX(), headingsize.GetY());
   sizerTop->Add(sizerHeading);
   
   for (int i = 0; contributors[i].name; ++i)
   {
      ExtTextCtrl* text = new ExtTextCtrl(this, -1, contributors[i].name,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxTE_READONLY | wxBORDER_NONE);
      text->SetMinSize(text->GetTextSize());
      text->SetBackgroundColour(this->GetBackgroundColour());
#ifdef MARCH_HARE_BUILD
      if (contributors[i].name == wxT("march-hare.com"))
            text->SetForegroundColour(wxColour(255, 0, 0));
#endif
      text->SetToolTip(wxGetTranslation(contributors[i].contribution));
      grid->Add(text);
   }
   
   sizerTop->Add(grid, 0, wxALL, 25);
   
   myOkButton = new wxButton(this, wxID_OK, _("OK"));
   sizerTop->Add(myOkButton, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

   // Overall dialog layout settings
   SetAutoLayout(true);
   SetSizer(sizerTop);
   sizerTop->SetSizeHints(this);
   sizerTop->Fit(this);

   SetTortoiseDialogPos(this, GetRemoteHandle());

   // myOkButton->SetFocus() doesn't work, so use a window message instead
   PostMessage(GetHwnd(), WM_TCVS_SETFOCUS, 0, 0);
}

// Windows callbacks
WXLRESULT CreditsDialog::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
   if (message == WM_TCVS_SETFOCUS)
   {
      myOkButton->SetFocus();      
      return 1;
   }
   return ExtDialog::MSWWindowProc(message, wParam, lParam);
}
