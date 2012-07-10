// $Id: CrapLexer.h,v 1.1 2012/03/04 01:06:54 aliot Exp $

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

#ifndef CRAPLEXER_H
#define CRAPLEXER_H

#include <cstdio>
#include <string>
#include <fstream>

class CrapLexer
{
public:
   CrapLexer( std::string const& filename );

   // diagnostics for error reporting
   std::string Filename() const;
   int LineNum() const;    // current line in file (starts at 1)

   enum
   {
      typeErr,
      typeFinished,
      typeString
   };
   
   // Read the next token. Puts string in 'token', returns type enum.
   int GetTok( std::string& token );
   
private:
   CrapLexer();   // not allowed
   std::ifstream myInput;
   std::string   myFilename;
   int           myLineNum;

   int HandleQuoted( std::string& token );
};

#endif // CRAPLEXER_H
