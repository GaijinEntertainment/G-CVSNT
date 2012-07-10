/* TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2004 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - April 2004

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
*/
/*****************************/
/* Parser for cvs log output */
/*****************************/


/*************************/
/* C declarations part 1 */
/*************************/

%{

#include <Utils/TortoiseDebug.h>
#include <Utils/TimeUtils.h>
#include "common.h"
#include <iostream>
#define YYINCLUDED_STDLIB_H

#if defined(_DEBUG) || defined (FORCE_DEBUG)
#define YYDEBUG 1
#define YYFPRINTF tdebug_fprintf
#endif

#pragma warning(disable:4065)

%}


/**********************/
/* Bison declarations */
/**********************/

/* Create a pure parser */
%pure-parser

/* Rename class to avoid name clashing */
%name-prefix="cvslog"


/* Lexer result */
%union {
   int lval;
}

%lex-param{FlexLexer *lexer}
%parse-param{FlexLexer *lexer}
%parse-param{ParserData *parserData}


/* Tokens */
%token TOK_STRING
%token TOK_EOL
%token TOK_RCS_FILE
%token TOK_WORKING_FILE
%token TOK_HEAD_REV
%token TOK_BRANCH_REV
%token TOK_LOCKS
%token TOK_STRICT
%token TOK_ACCESS_LIST
%token TOK_SYMBOLIC_NAMES
%token TOK_KEYWORD_SUBST
%token TOK_TOTAL_REVS
%token TOK_SELECTED_REVS
%token TOK_DESCRIPTION
%token TOK_REVISION
%token TOK_LOCKED_BY
%token TOK_DATE
%token TOK_AUTHOR
%token TOK_STATE
%token TOK_LINES
%token TOK_KOPT
%token TOK_COMMITID
%token TOK_FILENAME
%token TOK_MERGEPOINT
%token TOK_BUGNUMBER
%token TOK_UNKNOWN
%token TOK_BRANCHES
%token TOK_COMMENT
%token TOK_EON


/*************************/
/* C declarations part 2 */
/*************************/

%{

// Calling the lexer
int yylex(YYSTYPE *lvalp, FlexLexer* lexer);

// Handle errors
void yyerror(FlexLexer* lexer, ParserData *parserData, char *s);

%}


%%

/*****************/
/* Grammar rules */
/*****************/


/* RCS nodes */
cvslog:
   /* Empty is okay */
   | cvslog rcsnode
   ;
   

/* RCS node */
rcsnode:
   rcsheaderitems rcsrevisions TOK_EON {
      TDEBUG_ENTER("rcsnode");
   }
   ;


/* The header items */
rcsheaderitems: 
   rcsheaderitem
   | rcsheaderitems rcsheaderitem
   ;


/* Header item */
rcsheaderitem: 
   rcsfile
   | workingfile
   | headrev
   | branchrev
   | locks
   | accesslist
   | symbolicnames
   | keywordsubst
   | totalrevs
   | selectedrevs
   | description 
   ;


/* RCS file */
rcsfile: 
   TOK_RCS_FILE 
   TOK_STRING {
      TDEBUG_ENTER("rcsfile");
      parserData->rcsFiles.push_back(CRcsFile());
      parserData->rcsFile = &parserData->rcsFiles.back();
      parserData->rcsFile->RcsFile() = lexer->YYText();
      TDEBUG_TRACE("RCS File: \"" << parserData->rcsFile->RcsFile().c_str() << "\"");
   }
   ;

   
/* Working file */
workingfile: 
   TOK_WORKING_FILE TOK_STRING {
      TDEBUG_ENTER("workingfile");
      parserData->rcsFile->WorkingFile() = lexer->YYText();
      TDEBUG_TRACE("Working File: \"" << parserData->rcsFile->WorkingFile().c_str() << "\"");
   }
   ;

   
/* Head revision */
headrev: 
   TOK_HEAD_REV TOK_STRING {
      TDEBUG_ENTER("headrev");
      parserData->rcsFile->HeadRev().SetDigits(lexer->YYText());
      TDEBUG_TRACE("Head revision: \"" << parserData->rcsFile->HeadRev().str() << "\"");
   }
   ;

   
/* Branch revision */
branchrev:
   TOK_BRANCH_REV 
   {
      TDEBUG_ENTER("branchrev");
      TDEBUG_TRACE("No branch revision");
   }
   | TOK_BRANCH_REV TOK_STRING {
      TDEBUG_ENTER("branchrev");
      parserData->rcsFile->BranchRev().SetDigits(lexer->YYText());
      TDEBUG_TRACE("Branch revision: \"" << parserData->rcsFile->BranchRev().str() << "\"");
   }
   ;

   
/* Locks */
locks: 
   TOK_LOCKS 
   {
      TDEBUG_ENTER("locks");
      TDEBUG_TRACE("No locks");
   }
   lockitems
   | TOK_LOCKS TOK_STRICT {
      TDEBUG_ENTER("locks");
      parserData->rcsFile->LockStrict() = true;
      TDEBUG_TRACE("Strict locks: " << parserData->rcsFile->LockStrict());
   }
   lockitems
   ;
   

/* Lock items */
lockitems:
   /* Empty is okay */
   | lockitems lockitem
   ;
   
   
/* Lock item */
lockitem:
   TOK_STRING {
      TDEBUG_ENTER("lockitem(1)");
      parserData->rcsFile->LocksList().push_back(CRevNumber());
      parserData->rcsFile->LastLock().Tag() = lexer->YYText();
   }
   TOK_STRING {
      TDEBUG_ENTER("lockitem(2)");
      parserData->rcsFile->LastLock().SetDigits(lexer->YYText());
      TDEBUG_TRACE("\"" << parserData->rcsFile->LastLock().Tag().c_str() << "\", \""
         << parserData->rcsFile->LastLock().str() << "\"");
   }
   ;

   
/* Access list */
accesslist: 
   TOK_ACCESS_LIST accessitems
   ;

   
/* Access items */
accessitems: 
   /* Empty is okay */
   | accessitems accessitem
   ;
   
   
/* Access item */
accessitem:
   TOK_STRING {
      TDEBUG_ENTER("accessitem");
      parserData->rcsFile->AccessList().push_back(lexer->YYText());
      TDEBUG_TRACE("\"" << parserData->rcsFile->LastAccess().c_str() << "\"");
   }
   ;
   
  
/* Symbolic names */
symbolicnames: 
   TOK_SYMBOLIC_NAMES symbolicitems
   ;

   
/* Symbolic items */
symbolicitems: 
   /* Empty is okay */
   | symbolicitems symbolicitem
   ;
   
   
/* Symbolic item */
symbolicitem:
   TOK_STRING {
      TDEBUG_ENTER("symbolicitem(1)");
      parserData->rcsFile->SymbolicList().push_back(CRevNumber());
      parserData->rcsFile->LastSymbName().Tag() = lexer->YYText();
   }
   TOK_STRING {
      TDEBUG_ENTER("symbolicitem(2)");
      parserData->rcsFile->SymbolicList().back().SetDigits(lexer->YYText());
      TDEBUG_TRACE("\"" << parserData->rcsFile->LastSymbName().Tag().c_str() << "\", \""
         << parserData->rcsFile->LastSymbName().str() << "\"");
   }
   ;


/* Keyword substitution */
keywordsubst: 
   TOK_KEYWORD_SUBST TOK_STRING {
      TDEBUG_ENTER("keywordsubst");
      parserData->rcsFile->KeywordSubst() = lexer->YYText();
      TDEBUG_TRACE("Keyword subst: \"" << parserData->rcsFile->KeywordSubst().c_str() << "\"");
   }
   ;


/* Total revisions */
totalrevs: 
   TOK_TOTAL_REVS TOK_STRING {
      TDEBUG_ENTER("totalrevs");
      parserData->rcsFile->TotRevisions() = atoi(lexer->YYText());
      TDEBUG_TRACE("total revisions: " << parserData->rcsFile->TotRevisions());
   }

   
/* Selected revisions */
selectedrevs:
   TOK_SELECTED_REVS TOK_STRING {
      TDEBUG_ENTER("selectedrevs");
      parserData->rcsFile->SelRevisions() = atoi(lexer->YYText());
      TDEBUG_TRACE("selected revisions: " << parserData->rcsFile->SelRevisions());
   }
   ;


/* Description */
description: 
   TOK_DESCRIPTION descriptionlines {
      TDEBUG_ENTER("description");
      TDEBUG_TRACE("Description: \"" << parserData->rcsFile->DescLog().c_str() << "\"");
   }
   ;


/* Description lines */
descriptionlines: 
   /* Empty is okay */
   | descriptionlines descriptionline
   ;

   
/* Description line */
descriptionline: 
   TOK_STRING {
      TDEBUG_ENTER("descriptionline string");
      parserData->rcsFile->DescLog() << lexer->YYText();
   }
   | TOK_EOL {
      TDEBUG_ENTER("descriptionline eol");
      parserData->rcsFile->DescLog() << "\n";
   }
   ;

   

   
/* The revisions */  
rcsrevisions:
   /* Empty is okay */
   | rcsrevision rcsrevisions
   ;
   
   
/* A revision */
rcsrevision:
   TOK_REVISION {
      TDEBUG_ENTER("rcsrevision(1)");
      parserData->rcsFile->AllRevs().push_back(CRevFile());
      parserData->revFile = &parserData->rcsFile->LastRev();
   }
   TOK_STRING {
      TDEBUG_ENTER("rcsrevision(2)");
      parserData->revFile->RevNum().SetDigits(lexer->YYText());
      parserData->revFile->CheckTags(parserData->rcsFile->SymbolicList());
      TDEBUG_TRACE("Revision: \"" << parserData->revFile->RevNum().str() << "\"");
   }
   revlockedby
   revoptions
   revbranches
   revcomment
   ;

   
/* Revision locked by */
revlockedby:
   /* Empty is okay */
   | TOK_LOCKED_BY TOK_STRING {
      TDEBUG_ENTER("revlockedby");
      parserData->revFile->Locker() = lexer->YYText();
      TDEBUG_TRACE("Locker: \"" << parserData->revFile->Locker().c_str() << "\"");
   }
   ;

   
/* Revision options */
revoptions:
   revoption
   | revoptions revoption
   ;	


/* Revision branches */
revbranches:
   /* Empty is okay */
   | TOK_BRANCHES revbranchlist
   ;
   

/* List of revision branches */
revbranchlist:
   revbranch
   | revbranchlist revbranch
   ;
   
   
/* Revision branch */
revbranch:
   TOK_STRING {
      TDEBUG_ENTER("revbranch");
      CRevNumber rev;
      rev.SetDigits(lexer->YYText());
      parserData->revFile->BranchesList().push_back(rev);
      TDEBUG_TRACE("Branch: \"" << parserData->revFile->LastBranch().str() << "\"");
   }
   ;

   
/* Revision option */
revoption:
   TOK_DATE TOK_STRING {
      TDEBUG_ENTER("revoption date");
      parserData->revFile->SetRevTime(lexer->YYText());
      TDEBUG_USED(char buf[30]);
      TDEBUG_TRACE("Date: " << asctime_r(&(parserData->revFile->RevTime()), buf));
   }
   | TOK_AUTHOR TOK_STRING {
      TDEBUG_ENTER("revoption author");
      parserData->revFile->Author() = lexer->YYText();
      TDEBUG_TRACE("Author: \"" << parserData->revFile->Author().c_str() << "\"");
   }
   | TOK_STATE TOK_STRING {
      TDEBUG_ENTER("revoption state");
      parserData->revFile->State() = lexer->YYText();
      TDEBUG_TRACE(lexer->YYText());
   }
   | TOK_LINES TOK_STRING {
      TDEBUG_ENTER("revoption lines");
      parserData->revFile->SetLines(lexer->YYText());
      TDEBUG_TRACE("Lines: +" << parserData->revFile->ChgPos() << " " << parserData->revFile->ChgNeg());
   }
   | TOK_KOPT TOK_STRING {
      TDEBUG_ENTER("revoption kopt");
      parserData->revFile->KeywordSubst() = lexer->YYText();
      TDEBUG_TRACE("Kopt: \"" << parserData->revFile->KeywordSubst().c_str() << "\"");
   }
   | TOK_COMMITID TOK_STRING {
      TDEBUG_ENTER("revoption commitid");
      parserData->revFile->CommitID() = lexer->YYText();
      TDEBUG_TRACE("CommitID: \"" << parserData->revFile->CommitID().c_str() << "\"");
   }
   | TOK_FILENAME TOK_STRING {
      TDEBUG_ENTER("revoption filename");
      std::string filename = lexer->YYText();
      FindAndReplace<std::string>(filename, "%3B", ";"); // URL-decode ';'
      parserData->revFile->Filename() = filename.c_str();
      TDEBUG_TRACE("Filename: \"" << filename << "\"");
   }
   | TOK_MERGEPOINT TOK_STRING {
      TDEBUG_ENTER("revoption mergepoint");
      parserData->revFile->MergePoint().SetDigits(lexer->YYText());
      TDEBUG_TRACE("MergePoint: \"" << parserData->revFile->MergePoint().str() << "\"");
   }
   | TOK_BUGNUMBER TOK_STRING {
      TDEBUG_ENTER("revoption bugid");
      parserData->revFile->BugNumber() = lexer->YYText();
      TDEBUG_TRACE("BugNumber: \"" << parserData->revFile->BugNumber().c_str() << "\"");
   }
   | TOK_UNKNOWN {
      TDEBUG_ENTER("revoption unknown(1)");
      TDEBUG_TRACE(lexer->YYText());
   }
   TOK_STRING {
      TDEBUG_ENTER("revoption unknown(2)");
      TDEBUG_TRACE(lexer->YYText());
   }
   ;


/* Revision comment */
revcomment:
   /* Empty is okay */
   | revcommentlines {
      TDEBUG_ENTER("revcomment");
      TDEBUG_TRACE("Comment: \"" << parserData->revFile->DescLog().c_str() << "\"");
   }
   ;
   

/* Revision comment lines */
revcommentlines:
   revcommentline
   | revcommentlines revcommentline
   ;


/* Revision comment line */
revcommentline:
   TOK_COMMENT {
      TDEBUG_ENTER("revcommentline string");
      parserData->revFile->DescLog() << lexer->YYText();
   }
   | TOK_EOL {
      TDEBUG_ENTER("revcommentline eol");
      parserData->revFile->DescLog() << "\n";
   }
   ;


%%


/*******************/
/* Additional code */
/*******************/

// Start parser
void StartCvsLogParser(FlexLexer* lexer, ParserData* parserData, bool debug)
{
#if YYDEBUG
   yydebug = debug ? 1 : 0;
#endif
   yyparse(lexer, parserData);
}


// Calling the lexer
int yylex(YYSTYPE *, FlexLexer* lexer)
{
   return lexer->yylex();
}


// Handle errors
void yyerror(FlexLexer * , ParserData *, char *s)
{
   std::cerr << "Parser error: " << s << std::endl;
}

