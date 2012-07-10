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
/****************************/
/* Lexer for cvs log output */
/****************************/


%{
/* Headers */
#include "bison_parser.hpp"

#include <iostream>


// This is a bugfix class - flex forgets to free its stack memory
class TortoiseLexer : public yyFlexLexer
{
public:
   TortoiseLexer(std::istream* arg_yyin = 0, std::ostream* arg_yyout = 0)
      : yyFlexLexer(arg_yyin, arg_yyout) { }

   ~TortoiseLexer() 
   { 
      if (yy_start_stack)
	  {
	     yy_flex_free(yy_start_stack); 
		 yy_start_stack = 0;
	  }
   }

   int yylex();
};


using namespace std;
%}


/***********/
/* Options */
/***********/

/* Don't use yywrap */
%option noyywrap

/* Enable start condition stack */
%option stack

/* No interactive scanning */
%option never-interactive

/* Rename class to avoid name clashing */
%option prefix="CvsLog"

/* Don't include the default rule */
%option nodefault

/* Derive from my subclass */
%option yyclass="TortoiseLexer"



/********************/
/* Start conditions */
/********************/

/* When reading the header part */
%x HEADER 
%x HEADER_LOCKS
%x HEADER_ACCESS
%x HEADER_SYMBOLIC
%x HEADER_TOTAL_REVS
%x HEADER_DESCRIPTION


/* When reading a revision */
%x REVISION
%x REVISION_DATE
%x REVISION_LOCKED_BY 
%x REVISION_OPTIONS
%x REVISION_BRANCHES 
%x REVISION_BRANCH_ITEMS
%x REVISION_COMMENT


/* Read string up to various delimiters */
%x STR_TO_EOL
%x SKIP_TO_EOL 
%x STR_TO_WS 


/* When reading an item of type "name: revision" followed by '\n' */
%x NAME_REV_ITEM
%x NAME_REV_COLON
%x NAME_REV_REV

/* When reading a list item followed by '\n' */
%x LIST_ITEM

/* When reading a key value, i.e. the value in ("key: value") followed by ';' or '\n' */
%x KEY_VALUE


/***************/
/* Definitions */
/***************/

/* Whitespace. */
WS ([ \t\r\n])

/* No whitespace. */
NWS ([^ \t\r\n])



/* Whitespace without a newline character */
WSNNL ([ \t])

/* Newline character */
NL (\r?\n)

/* No newline character */
NNL ([^\r\n])

/* Revision separator */
RSEP (\-{28})

/* Node separator */
NSEP (\={77,})

%%

   /* Skip whitespace at the beginning of the log */
<INITIAL>({WS}*{NL})?RCS\ file:{WSNNL}* { 
      BEGIN(HEADER);
      yy_push_state(STR_TO_EOL);
      return TOK_RCS_FILE;
   }



   /* Read "Working file" */
<HEADER>Working\ file:{WSNNL}* {
      yy_push_state(STR_TO_EOL);
      return TOK_WORKING_FILE;
   }



   /* Read "head" */
<HEADER>head:{WSNNL}* {
      yy_push_state(STR_TO_EOL);
      return TOK_HEAD_REV;
   }
      

   
   /* Read "branch" */
<HEADER>branch:{WSNNL}* {
      yy_push_state(STR_TO_EOL);
      return TOK_BRANCH_REV;
   }
      

   
   /* Read "locks" */
<HEADER>locks:{WSNNL}* {
      BEGIN(HEADER_LOCKS);
      return TOK_LOCKS;
   }
   
   /* Read "strict" */
<HEADER_LOCKS>strict{WSNNL}* {
      return TOK_STRICT;
   }
   
   /* Read locks list item */
<HEADER_LOCKS>{NL}{WSNNL}+ {
      yy_push_state(NAME_REV_ITEM);
   }
   
   /* End of locks list */
<HEADER_LOCKS>{NL} {
      BEGIN(HEADER);
   }

      
   
   /* Read "access list" */
<HEADER>access\ list:{WSNNL}* {
      BEGIN(HEADER_ACCESS);
      return TOK_ACCESS_LIST;
   }
   
   /* Read access list item */
<HEADER_ACCESS>{NL}{WSNNL}+ {
      yy_push_state(LIST_ITEM);
   }
   
   /* End of access list */
<HEADER_ACCESS>{NL} {
      BEGIN(HEADER);
   }


   
   /* Read "symbolic names" */
<HEADER>symbolic\ names:{NNL}* {
      BEGIN(HEADER_SYMBOLIC);
      return TOK_SYMBOLIC_NAMES;
   }

   /* Read symbolic list item */
<HEADER_SYMBOLIC>{NL}{WSNNL}+ {
      yy_push_state(NAME_REV_ITEM);
   }
   
   /* End of symbolic list */
<HEADER_SYMBOLIC>{NL} {
      BEGIN(HEADER);
   }



   /* Read "keyword substitution:" */
<HEADER>keyword\ substitution:{WSNNL}* {
      yy_push_state(STR_TO_EOL);
      return TOK_KEYWORD_SUBST;
   }



   /* Read "total revisions:" */
<HEADER>total\ revisions:{WSNNL}* {
      BEGIN(HEADER_TOTAL_REVS);
      yy_push_state(KEY_VALUE);
      return TOK_TOTAL_REVS;
   }
   
   /* Read "selected revisions:" */
<HEADER_TOTAL_REVS>selected\ revisions:{WSNNL}* {
      yy_push_state(KEY_VALUE);
      return TOK_SELECTED_REVS;
   }
   
   /* End if "total revisions" */
<HEADER_TOTAL_REVS>{NL} {
      BEGIN(HEADER);
   }



   /* Read "description:" */
<HEADER>description:{NNL}*{NL} {
      yy_push_state(HEADER_DESCRIPTION);
      return TOK_DESCRIPTION;
   }
   
   /* Read revision separator => Start reading revision */
<HEADER_DESCRIPTION>{NL}?{RSEP}{NL}/revision {
      yy_pop_state();
      BEGIN(REVISION);
   }
   
   /* Read node separator => Finished node */
<HEADER_DESCRIPTION>{NL}?{NSEP}{NL} {
      yy_pop_state();
      BEGIN(INITIAL);
      return TOK_EON;
   }
 
   /* Read description line */
<HEADER_DESCRIPTION>{NNL}+ {
      return TOK_STRING;
   }
  
   /* Read description line break */
<HEADER_DESCRIPTION>{NL} {
      return TOK_EOL;
   }

   
   
   /* Read unknown header line */
<HEADER>{NNL} {
      yy_push_state(SKIP_TO_EOL);
      ECHO;
   }
   

  
   /* Revision starts */
<REVISION>revision{WSNNL}* {
      BEGIN(REVISION_LOCKED_BY);
      yy_push_state(STR_TO_WS);
      return TOK_REVISION;
   }


   
   /* Read empty "locked by:" */
<REVISION_LOCKED_BY>{NL} {
      BEGIN(REVISION_OPTIONS);
   }
   
   /* Read "locked by:" */
<REVISION_LOCKED_BY>locked\ by:{WSNNL}* {
      yy_push_state(KEY_VALUE);
      return TOK_LOCKED_BY;
   }

   
  
   /* Read "date:" */
<REVISION_OPTIONS>date:{WSNNL}* {
      BEGIN(REVISION_OPTIONS);
      yy_push_state(KEY_VALUE);
      return TOK_DATE;
   }
   
   /* Read "author:" */
<REVISION_OPTIONS>author:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_AUTHOR;
   }
   
   /* Read "state:" */
<REVISION_OPTIONS>state:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_STATE;
   }
   
   /* Read "lines:" */
<REVISION_OPTIONS>lines:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_LINES;
   }
   
   /* Read "kopt:" */
<REVISION_OPTIONS>kopt:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_KOPT;
   }
   
   /* Read "commitid:" */
<REVISION_OPTIONS>commitid:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_COMMITID;
   }

   /* Read "filename:" */
<REVISION_OPTIONS>filename:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_FILENAME;
   }
   
   /* Read "mergepoint:" */
<REVISION_OPTIONS>mergepoint:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_MERGEPOINT;
   }
   
   /* Read "bugid:" */
<REVISION_OPTIONS>bugid:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_BUGNUMBER;
   }

   /* Read unknown option */
<REVISION_OPTIONS>[^:\r\n]*:{WS}* {
      yy_push_state(KEY_VALUE);
      return TOK_UNKNOWN;
   }
   
   /* End of options */
<REVISION_OPTIONS>{NL}/branches: {
      BEGIN(REVISION_BRANCHES);
   }
   
   /* End of options */
<REVISION_OPTIONS>{NL} {
      BEGIN(REVISION_COMMENT);
   }
   
   
  
   /* Read "branches: " */
<REVISION_BRANCHES>branches:{WSNNL}* {
      BEGIN(REVISION_BRANCH_ITEMS);
      return TOK_BRANCHES;
   }

   /* Read branch */
<REVISION_BRANCH_ITEMS>[^;\r\n]+ {
      return TOK_STRING;
   }

   /* Read delimiter */
<REVISION_BRANCH_ITEMS>;{WSNNL}* {
   }

   /* End of branches */
<REVISION_BRANCH_ITEMS>{NL} {
      BEGIN(REVISION_COMMENT);
   }	



   /* Read revision comment */
<REVISION_COMMENT>{NNL}+/{NL} {
      return TOK_COMMENT;
   }


   /* Read revision comment */
<REVISION_COMMENT>{NL} {
      return TOK_EOL;
   }


   /* Read revision comment */
<REVISION_COMMENT>{NL}?{RSEP}{NL}/revision {
      BEGIN(REVISION);
   }


   /* Read revision comment */
<REVISION_COMMENT>{NL}?{NSEP}{NL} {
      BEGIN(INITIAL);
      return TOK_EON;
   }


   /* Read string up to next newline */
<STR_TO_EOL>{NNL}+/{NL} {
      return TOK_STRING;
   }


   /* Read newline */
<STR_TO_EOL>{NL} {
      yy_pop_state();
   }
   

   /* Skip to end of line */
<SKIP_TO_EOL>{NNL}*{NL}? {
      yy_pop_state();
      ECHO;
   }
   
   
   /* Read string up to whitespace */
<STR_TO_WS>{NWS}+/{WSNNL}+ {
      return TOK_STRING;
   }
   
   
   /* Read string up to whitespace */
<STR_TO_WS>{NWS}+ {
      yy_pop_state();
      return TOK_STRING;
   }
   
   
   /* Read whitespace after string*/
<STR_TO_WS>{WSNNL}+ {
      yy_pop_state();
   }
   


   /*************************************************************/
   /* Reading an item of type "name: revision" followed by '\n' */
   /*************************************************************/

   /* Read name */
<NAME_REV_ITEM>[^:]+ {
      BEGIN(NAME_REV_COLON);
      return TOK_STRING;
   }
      
   /* Read delimiting colon */
<NAME_REV_COLON>:{WSNNL}* {
      BEGIN(NAME_REV_REV);
   }
      
   /* Read revision */
<NAME_REV_REV>{NNL}+ {
      yy_pop_state();
      return TOK_STRING;
   }

   

   /****************************************/
   /* Reading a list item followed by '\n' */
   /****************************************/

   /* Read item */
<LIST_ITEM>{NNL}+ {
      yy_pop_state();
      return TOK_STRING;
   }
      

   
   /*********************************************************************************/
   /* Reading a key value, i.e. the value in ("key: value") followed by ';' or '\n' */
   /*********************************************************************************/
   
   /* Read value followed by delimiter */
<KEY_VALUE>[^;\r\n]+/; {
      return TOK_STRING;
   }

   /* Read value followed by newline */
<KEY_VALUE>[^;\r\n]+/{NL} {
      yy_pop_state();
      return TOK_STRING;
   }

   /* Read semicolon delimiter */
<KEY_VALUE>;{WSNNL}* {
      yy_pop_state();
   }


   /* Ignore unknown input    
<*>. {
      printf("x");
   }
<*>{NL} {
      printf("\n");
   }
*/


%%


// Create the lexer
FlexLexer* CreateCvsLogLexer(std::istream *in, std::ostream *out)
{
   return new TortoiseLexer(in, out);
}
