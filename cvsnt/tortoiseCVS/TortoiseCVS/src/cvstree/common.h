#ifndef COMMON_H
#define COMMON_H

/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <iostream>
#include "FlexLexer.h"
#include "CvsLog.h"
#include "CvsRepo.h"
#include "../CVSGlue/CvsEntries.h"


struct RepoParserData
{
   void Clear()
   {
      entries.clear();
      ent = 0;
      newnode = 0;
   }

   // Array of entries
   EntnodeMap entries;
   // Current entry
   EntnodeData* ent;
   // Current node
   ENTNODE* newnode;
};


struct ParserData
{
   void Clear()
   {
      rcsFiles.clear();
      rcsFile = 0;
      revFile = 0;
   }

   // Array of files 
   std::vector<CRcsFile> rcsFiles;
   // Current file
   CRcsFile *rcsFile;
   // Current revision
   CRevFile *revFile;   
};


// Create the lexer
FlexLexer* CreateCvsLogLexer(std::istream *in, std::ostream *out);

// Start parser
void StartCvsLogParser(FlexLexer* lexer, ParserData* parserData, bool debug);


#endif /* COMMON_H */
