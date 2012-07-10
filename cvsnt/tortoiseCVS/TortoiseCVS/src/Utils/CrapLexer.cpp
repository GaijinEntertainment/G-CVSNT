// $Id: CrapLexer.cpp,v 1.1 2012/03/04 01:06:54 aliot Exp $

// Copyright (c) 2001, Ben Campbell.
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//  * Neither the name of the international TortoiseCVS community
//    nor the names of its contributors may be used to endorse or
//    promote products derived from this software without specific
//    prior written permission.
//    
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CYBERLIFE TECHNOLOGY 
// LTD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include "StdAfx.h"
#include "CrapLexer.h"
#include <ctype.h>

CrapLexer::CrapLexer( std::string const& filename ) :
   myInput( filename.c_str() ), myFilename( filename ), myLineNum(-1)
{
   if( myInput.good() )
      myLineNum = 1;
}


int CrapLexer::LineNum() const
{
   return myLineNum;
}


std::string CrapLexer::Filename() const
{
   return myFilename;
}

int CrapLexer::GetTok( std::string& token )
{
   int c = 0;
   token.erase();

   //skip to first char of token
   while( myInput.good() )
   {
      c = myInput.get();
      if( c=='#' )
      {
         do
         {
            c = myInput.get();
            if( c=='\n' )
               ++myLineNum;
         }
         while( myInput.good() && c != '\n' );
      }
      else if( !isspace(c) )
         break;
      else if( c=='\n' )
         ++myLineNum;
   }
   if( myInput.eof() )
      return typeFinished;
   if( !myInput.good() )
      return typeErr;


   if( c == '\"' )
      return HandleQuoted( token );
   else
   {
      // non-quoted string - slurp until whitespace (or eof or error)
      do
      {
         token += static_cast<char>(c);
         c = myInput.get();
      } while( myInput.good() && !isspace(c) );

      if( myInput.good() || myInput.eof() )
      {
         myInput.putback(static_cast<char>(c));
         return typeString;
      }
      return typeErr;
   }
}


// quoted, can handle spaces and control chars
int CrapLexer::HandleQuoted( std::string& token )
{
   // (leading quote already removed from stream)
   while( myInput.good() )
   {
      int c = myInput.get();
      switch(c)
      {
      case '\\':
         switch( myInput.get() )
         {
            case '\\': token += '\\'; break;
            case '0': token += '\0'; break;
            case '\"': token += '\"'; break;
            case 'r': token += '\r'; break;
            case 'n': token += '\n'; break;
            case 't': token += '\t'; break;
            default:
               return typeErr;   // unknown ctrl char
         }
         break;
      case '\n':
         return typeErr;   // unexpected end-of-line
         break;
      case '\"':
         return typeString;   // done
         break;
      default:
         token += static_cast<char>(c);
         break;
      }
   }
   return typeErr;   // read error or unexpected EOF
}

