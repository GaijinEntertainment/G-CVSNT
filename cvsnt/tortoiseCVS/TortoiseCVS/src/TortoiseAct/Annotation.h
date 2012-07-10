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

#ifndef ANNOTATION_H
#define ANNOTATION_H

#include <vector>
#include <map>
#include <time.h>
#include <stdio.h>

#include "../cvstree/CvsLog.h"


class CAnnotationList;

// an annotated line from a file
class CAnnotation
{
public:
   CAnnotation();
   CAnnotation(CAnnotationList* alist, unsigned int lineNum, const std::string& line);
   virtual ~CAnnotation();

   inline unsigned int LineNum() const { return lineNum; }    
   inline const CRevNumber & RevNum() const { return revNum; }
   inline const std::string& Author() const { return author; }    
   inline const struct tm & Date() const { return date; }
   inline struct tm & Date() { return date; }
   inline const std::string& BugNumber() const { return bugnumber; }    
   inline const std::string& Text() const { return text; }    
   inline const void* Data() const { return data; }
   inline const void SetData(void* newData) { data = newData; }
   inline const CAnnotationList* List() const { return myList; }
   inline const std::string LogMessage() const { return myLogMessage; }

   bool SetAnnotation(unsigned int lineNum, const std::string& line);
   void SetLogMessage(const std::string& msg);

   bool IsTextMatching(const std::string& searchString, bool caseSensitive) const;

protected:
   void Clear();
   bool Parse(const std::string& line);

   unsigned int         lineNum;
   CRevNumber           revNum;
   std::string          author;    
   struct tm            date;
   std::string          bugnumber;    
   std::string          text;
   CAnnotationList*     myList;
   void*                data;
   // Log message, extracted from "cvs log"
   std::string          myLogMessage;
};

class CAnnotationList
{
public:
   enum SortColumn
   {
      SORT_REV,
      SORT_AUTHOR,
      SORT_DATE,
      SORT_BUGNUMBER,
      SORT_LINE,
      SORT_TEXT
   };


   
   CAnnotationList();
   virtual ~CAnnotationList();

   inline unsigned int AnnotationCount() const { return static_cast<unsigned int>(annotationList.size()); }
   inline time_t MinDate() const { return minDate; }
   inline time_t MaxDate() const { return maxDate; }
   inline int DateCount() const { return static_cast<int>(dateMap.size()); }

   CAnnotation* operator[](unsigned int index)
   {
      if (index >= annotationList.size())
         return 0;
        
      return annotationList[index];
   }

   void Sort(SortColumn sortBy, bool ascending); 

   unsigned int AddAnnotation(const std::string& line);
    
protected:
   void Clear();
   // Compare annotations
   static bool CompareAnnotations(CAnnotation* item1, CAnnotation* item2);

   std::vector<CAnnotation*> annotationList;
   std::map<time_t, int> dateMap;
   time_t minDate;
   time_t maxDate;
   bool mySortAsc;
   SortColumn mySortCol;
};

#endif
