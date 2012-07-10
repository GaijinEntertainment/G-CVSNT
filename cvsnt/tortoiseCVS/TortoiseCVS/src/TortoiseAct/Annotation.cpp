// TortoiseCVS - a Windows shell extension for easy version control

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
#include "../Utils/TortoiseDebug.h"
#include <string.h>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "Annotation.h"
#include "../Utils/TimeUtils.h"

//
// CAnnotation
//

CAnnotation::CAnnotation()
{
    Clear();
}

CAnnotation::CAnnotation(CAnnotationList* alist, unsigned int lineNum, 
                         const std::string& line)
{
   myList = alist;
   if (!SetAnnotation(lineNum, line))
      Clear();
}

CAnnotation::~CAnnotation()
{
}

bool CAnnotation::SetAnnotation(unsigned int lineNum, const std::string& line)
{
   bool result = Parse(line);
   if (result)
      this->lineNum = lineNum;
   return result;
}

void CAnnotation::SetLogMessage(const std::string& msg)
{
   myLogMessage = msg;
}

void CAnnotation::Clear()
{
   lineNum = 0;
   revNum.reset();
   author.erase();
   memset(&date, 0, sizeof(date));
   bugnumber.erase();
   text.erase();
}

// Parses a single line from a cvs annotate into its respective parts.  An example of one line is:
// 1.1          (frabcus  05-Jan-01): Building TortoiseCVS on Borland C++ Builder (version 4 or greater)
bool CAnnotation::Parse(const std::string& line)
{
   TDEBUG_ENTER("CAnnotation::Parse");
   TDEBUG_TRACE("line: " << line);
   Clear();

   if (line.empty())        
      return false;

   std::string token;
   std::string::size_type begin = 0;
   std::string::size_type end = 0;

   // revision number
   end = line.find(' ', begin);
   if (end == std::string::npos)
      return false;

   token = line.substr(begin, end);
   std::string::size_type tokenLength = token.length();
   std::string num;
   for (;;)
   {
      end = token.find('.', begin);
      num = token.substr(begin, end - begin);        
      revNum += atoi(num.c_str());        
      begin = end + 1;

      if (end == std::string::npos || end == tokenLength - 1)
         break;
   }

   // author
   begin = line.find('(', tokenLength);
   if (begin == std::string::npos)
      return false;

   ++begin;

   end = line.find(')', begin);
   if (end == std::string::npos)
      return false;

   end = line.rfind(' ', end);
   if (end == std::string::npos)
      return false;

   author = line.substr(begin, end - begin);

   // trim trailing whitespace from author
   author.erase(author.find_last_not_of(" ") + 1); 

   // date
   begin = line.find_first_not_of(' ', end);
   if (begin == std::string::npos)
      return false;

   end = line.find(')', begin);
   if (end == std::string::npos)
      return false;

   token = line.substr(begin, end - begin);
   TDEBUG_TRACE("Date string: " << token);
   if (!datestring_to_tm(token.c_str(), &date))
      return false;
   TDEBUG_TRACE("Datetime: " << asctime(&date));

   // bugnumber
   bugnumber.clear();

   // trim trailing whitespace from bugnumber
   bugnumber.erase(bugnumber.find_last_not_of(" ") + 1); 

   // text
   begin = line.find_first_not_of("):", end);
   if (begin == std::string::npos)
      return false;

   text = line.substr(begin + 1);

   return true;
}

bool CaseInsensitiveCompare(char c1, char c2)
{
   return toupper(c1) == toupper(c2);
}

bool CaseInsensitiveFind(const std::string& source,
                         const std::string& substring)
{
   std::string::const_iterator pos = std::search(source.begin(), source.end(),
      substring.begin(), substring.end(), CaseInsensitiveCompare);
   return pos != source.end();
}

bool CAnnotation::IsTextMatching(const std::string& searchString,
                                 bool caseSensitive) const
{
   if (caseSensitive)
   {
      return text.find(searchString) != std::string::npos;
   }

   return CaseInsensitiveFind(text, searchString);
}


//
// CAnnotationList
//


CAnnotationList::CAnnotationList()
{
   Clear();
}

CAnnotationList::~CAnnotationList()
{
   std::vector<CAnnotation*>::iterator it = annotationList.begin();
   while (it != annotationList.end())
   {
      delete *it;
      it++;
   }
}

unsigned int CAnnotationList::AddAnnotation(const std::string& line)
{
    TDEBUG_ENTER("AddAnnotation");
    TDEBUG_TRACE("Line: " << line);
    annotationList.push_back(new CAnnotation(this, static_cast<unsigned int>(annotationList.size() + 1), line));
    tm& date = annotationList[annotationList.size() - 1]->Date();
    TDEBUG_TRACE("Date: " << date.tm_mday << "-" << date.tm_mon << "-" << date.tm_year << " "
                 << date.tm_hour << ":" << date.tm_min);

   time_t itemDate = mktime(&date);

   if (minDate == 0 || itemDate < minDate)
      minDate = itemDate;
   if (maxDate == 0 || itemDate > maxDate)
      maxDate = itemDate;

   dateMap[itemDate] = 1;
   return static_cast<unsigned int>(annotationList.size());
}

void CAnnotationList::Clear()
{
   annotationList.clear();
   minDate = 0;
   maxDate = 0;
}


void CAnnotationList::Sort(SortColumn sortBy, bool ascending)
{
   mySortCol = sortBy;
   mySortAsc = ascending;
   std::stable_sort(annotationList.begin(), annotationList.end(), 
                    CAnnotationList::CompareAnnotations);
}



// Compare annotations
bool CAnnotationList::CompareAnnotations(CAnnotation* item1,
                                         CAnnotation* item2) 
{
   const CAnnotationList* list = item1->List();
   bool bResult = true;

   if (!list->mySortAsc)
       std::swap(item1, item2);

   switch (list->mySortCol)
   {
      case SORT_LINE: 
      {
         unsigned int lineNum1 = item1->LineNum();
         unsigned int lineNum2 = item2->LineNum();
         bResult = lineNum1 < lineNum2;
         break;
      }

      case SORT_REV:
      {
         const CRevNumber& rev1 = item1->RevNum();
         const CRevNumber& rev2 = item2->RevNum();
         bResult = rev1.cmp(rev2) < 0;
         break;
      }

      case SORT_AUTHOR:
      {
         const std::string& author1 = item1->Author();
         const std::string& author2 = item2->Author();
         bResult = strcmp(author1.c_str(), author2.c_str()) < 0;
         break;
      }

      case SORT_DATE:
      {
         time_t date1 = mktime(&(item1->Date()));
         time_t date2 = mktime(&(item2->Date()));
         bResult = int(difftime(date1, date2)) < 0;
         break;
      }

      case SORT_BUGNUMBER:
      {
         const std::string& bugnumber1 = item1->BugNumber();
         const std::string& bugnumber2 = item2->BugNumber();
         bResult = strcmp(bugnumber1.c_str(), bugnumber2.c_str()) < 0;
         break;
      }

      case SORT_TEXT:
      {
         const std::string& text1 = item1->Text();
         const std::string& text2 = item2->Text();
         bResult = strcmp(text1.c_str(), text2.c_str()) < 0;
         break;
      }

      default:
      {
         ASSERT(false);
      }
   }

   return bResult;
}
