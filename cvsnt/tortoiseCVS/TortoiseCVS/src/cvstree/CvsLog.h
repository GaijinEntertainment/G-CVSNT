/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 1, or (at your option)
** any later version.

** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.

** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef CVSLOG_H
#define CVSLOG_H

#ifdef WIN32
#  ifdef _DEBUG
#     define qCvsDebug 1
#  else
#     define qCvsDebug 0
#  endif
#endif /* WIN32 */

#include <vector>
#include <time.h>
#include <stdio.h>

#include <wx/string.h>

#if defined(_MSC_VER) && _MSC_VER < 0x514 // VC7
#  include <iostream.h>
using namespace std;
#  define OSTREAM ::ostream
#else
#  include <iostream>
#  define OSTREAM std::ostream
#endif

// a C-string
class CLogStr
{
public:
   inline CLogStr() {}
   inline CLogStr(const char* newstr)
   {
      if (!newstr)
      {
         str.clear();
         return;
      }
      str = newstr;
   }
   inline CLogStr(const CLogStr& newstr)
   {
      str = newstr.str;
   }
   virtual ~CLogStr() {}

   inline bool empty() const { return str.empty(); }
   inline unsigned int length() const { return str.size(); }

   const char* operator=(const char* newstr);
   // set to a new C String (0L is OK)
   const CLogStr& operator=(const CLogStr& newstr);
   // set according to another CLogStr
   const CLogStr& set(const char* buf, unsigned int len);
   // set from a buffer

   bool operator<(const char* newstr) const;

   // as a C string
   inline const char* c_str() const { return str.c_str(); }

   CLogStr& operator<<(const char* addToStr);
   CLogStr& operator<<(char addToStr);
   CLogStr& operator<<(int addToStr);
   // concatenate

   inline bool endsWith(char c) const {return str.empty() ? false : str[str.size()-1] == c;}

   const CLogStr& replace(char which, char bywhich);
   // replace a character

protected:
   std::string str;
};

std::ostream& operator<<(std::ostream&, const CLogStr&);

// class to store a revision number
class CRevNumber
{
public:
   CRevNumber() {}
   CRevNumber(const char* p);
   virtual ~CRevNumber() { reset(); }

   void reset();
   inline int size() const { return static_cast<int>(allDigits.size()); }
   inline bool empty() const { return allDigits.empty(); }

   int cmp(const CRevNumber& arev) const;
   // return -1,0,1 "a la" qsort

   CRevNumber& operator+=(int adigit);
   CRevNumber& operator=(const CRevNumber& arev);
   bool operator==(const CRevNumber& arev) const;
   int operator[](int index) const;
   bool operator<(const CRevNumber& arev) const;

   bool ischildof(const CRevNumber& arev) const;
   // 1.4.2.2 is child of 1.4
   bool issamebranch(const CRevNumber& arev) const;
   // 1.1.1.2 is same branch than 1.1.1.3
   bool ispartof(const CRevNumber& arev) const;
   // 1.1.1.2 is part of 1.1.1
   // 1.4.2.3 is part of 1.4.0.2
   bool issubbranchof(const CRevNumber& arev) const;
   // 1.1.1 is subbranch of 1.1
   // 1.4.0.2 is subbranch of 1.4

   inline const std::vector<int>& IntList() const { return allDigits; }
   inline std::vector<int>& IntList() { return allDigits; }

   inline const CLogStr& Tag() const { return tagName; }
   inline CLogStr& Tag() { return tagName; }

   std::string str() const;
   // Set digits from string
   void SetDigits(const char *s);
protected:
   std::vector<int> allDigits;
   CLogStr tagName; // or an author
};

// a single revision for a file
class CRevFile
{
public:
   CRevFile();
   CRevFile(const CRevFile& afile);
   virtual ~CRevFile();

   CRevFile& operator=(const CRevFile& afile);
   bool operator<(const CRevFile& afile) const;
   bool operator==(const CRevFile& afile) const;

   void print(OSTREAM& out) const;
   void SetRevTime(const char *s);
   void SetLines(const char *s);
   void SetDescLog(const char* log) { descLog = log; }

   inline const CRevNumber& RevNum() const { return revNum; }
   inline CRevNumber& RevNum() { return revNum; }

   inline const struct tm& RevTime() const { return revTime; }
   inline struct tm& RevTime() { return revTime; }

   inline const CLogStr& Locker() const { return locker; }
   inline CLogStr& Locker() { return locker; }

   inline const std::vector<CRevNumber>& BranchesList() const { return branchesList; }
   inline std::vector<CRevNumber>& BranchesList() { return branchesList; }
   inline CRevNumber& LastBranch() { return branchesList[branchesList.size() - 1]; }

   inline const CLogStr& Author() const { return author; }
   inline CLogStr& Author() { return author; }

   inline const CLogStr& State() const { return state; }
   inline CLogStr& State() { return state; }

   inline int ChgPos() const { return chgPos; }
   inline int& ChgPos() { return chgPos; }

   inline int ChgNeg() const { return chgNeg; }
   inline int& ChgNeg() { return chgNeg; }

   inline const CLogStr& DescLog() const { return descLog; }
   inline CLogStr& DescLog() { return descLog; }

   inline const CLogStr& KeywordSubst() const { return keywordSubst; }
   inline CLogStr& KeywordSubst() { return keywordSubst; }

   inline const CLogStr& CommitID() const { return commitID; }
   inline CLogStr& CommitID() { return commitID; }

   inline const CLogStr& Filename() const { return filename; }
   inline CLogStr& Filename() { return filename; }

   inline const CRevNumber& MergePoint() const { return mergePoint; }
   inline CRevNumber& MergePoint() { return mergePoint; }

   inline const CLogStr& BugNumber() const { return bugNumber; }
   inline CLogStr& BugNumber() { return bugNumber; }

   inline bool HasTag() const { return hasTag; }

   inline bool IsMergePointTarget() const { return isMergePointTarget; }
   inline bool& IsMergePointTarget() { return isMergePointTarget; }

   // Check if this revision is associated with any of the tags in the list
   void CheckTags(const std::vector<CRevNumber>& taglist);

   // Return added/deleted lines as text
   std::string Changes() const;

   // Return date as text
   wxString DateAsString() const;

protected:
   CRevNumber   revNum;
   struct tm    revTime;
   CLogStr      locker;
   std::vector<CRevNumber> branchesList;
   CLogStr      author;
   CLogStr      state;
   int          chgPos;
   int          chgNeg;
   CLogStr      descLog;
   CLogStr      keywordSubst;
   CLogStr      commitID;
   CLogStr      filename;
   CRevNumber   mergePoint;
   CLogStr      bugNumber;
   bool         hasTag;
   bool         isMergePointTarget;
};

// RCS infos for a file
class CRcsFile
{
public:
   CRcsFile();
   CRcsFile(const CRcsFile& afile);
   virtual ~CRcsFile();

   CRcsFile& operator=(const CRcsFile& afile);
   bool operator<(const CRcsFile& afile) const;
   bool operator==(const CRcsFile& afile) const;

   void sort();

   void print(OSTREAM& out) const;

   inline const CLogStr& RcsFile() const { return rcsFile; }
   inline CLogStr& RcsFile() { return rcsFile; }

   inline const CLogStr& WorkingFile() const { return workingFile; }
   inline CLogStr& WorkingFile() { return workingFile; }

   inline const CRevNumber& HeadRev() const { return headRev; }
   inline CRevNumber& HeadRev() { return headRev; }

   inline const CRevNumber& BranchRev() const { return branchRev; }
   inline CRevNumber& BranchRev() { return branchRev; }

   inline const CLogStr& KeywordSubst() const { return keywordSubst; }
   inline CLogStr& KeywordSubst() { return keywordSubst; }

   inline int SelRevisions() const { return selRevisions; }
   inline int& SelRevisions() { return selRevisions; }

   inline int TotRevisions() const { return totRevisions; }
   inline int& TotRevisions() { return totRevisions; }

   inline const std::vector<CLogStr>& AccessList() const { return accessList; }
   inline std::vector<CLogStr>& AccessList() { return accessList; }
   inline CLogStr& LastAccess() { return accessList[accessList.size() - 1]; }

   inline const std::vector<CRevNumber>& SymbolicList() const { return symbolicList; }
   inline std::vector<CRevNumber>& SymbolicList() { return symbolicList; }
   inline CRevNumber& LastSymbName() { return symbolicList[symbolicList.size() - 1]; }

   inline const std::vector<CRevNumber>& LocksList() const { return locksList; }
   inline std::vector<CRevNumber>& LocksList() { return locksList; }
   inline CRevNumber& LastLock() { return locksList[locksList.size() - 1]; }

   inline bool LockStrict() const { return lockStrict; }
   inline bool& LockStrict() { return lockStrict; }

   inline const std::vector<CRevFile>& AllRevs() const { return allRevs; }
   inline std::vector<CRevFile>& AllRevs() { return allRevs; }
   inline CRevFile& LastRev() { return allRevs[allRevs.size() - 1]; }

   inline const CLogStr& DescLog() const { return descLog; }
   inline CLogStr& DescLog() { return descLog; }

protected:
   CLogStr rcsFile;
   CLogStr workingFile;
   CRevNumber headRev;
   CRevNumber branchRev;
   CLogStr keywordSubst;
   std::vector<CLogStr> accessList;
   std::vector<CRevNumber> symbolicList;
   std::vector<CRevNumber> locksList;
   int selRevisions;
   int totRevisions;
   bool lockStrict;
   std::vector<CRevFile> allRevs;
   CLogStr descLog;
};

// classes used to build a tree off the cvs log output

typedef enum
{
   kNodeHeader,
   kNodeBranch,
   kNodeRev,
   kNodeTag
} kLogNode;

class CLogNode;

class CLogNodeData
{
public:
   CLogNodeData(CLogNode* node) { fNode = node; }
   virtual ~CLogNodeData() {}

   inline CLogNode* Node() { return fNode; }
protected:
   CLogNode* fNode;
};

class CLogNode
{
public:
   CLogNode(CLogNode*  root = 0L);
   virtual ~CLogNode();

   virtual kLogNode GetType() const = 0;

   bool IsChildOf(const CLogNode* node) const;

   typedef std::vector<CLogNode*> ChildList;

   inline const ChildList& Childs() const { return childs; }
   inline ChildList& Childs() { return childs; }

   inline const CLogNode * Root() const { return root; }
   inline CLogNode *& Root() { return root; }

   inline const CLogNode*  Next() const { return next; }
   inline CLogNode *& Next() { return next; }

   inline void SetUserData(CLogNodeData *data) { user = data; }
   inline CLogNodeData *GetUserData() { return user; }

   const CLogNode* FindBranchNode(const std::string& branch, const CLogNode* parent);

protected:
   ChildList childs;
   CLogNode * root;
   CLogNode * next;
   CLogNodeData *user;
};

class CLogNodeHeader : public CLogNode
{
public:
   CLogNodeHeader(const CRcsFile& h, CLogNode * r = 0L) :
      CLogNode(r), header(h) {}

   virtual kLogNode GetType() const { return kNodeHeader; }

   inline const CRcsFile& RcsFile() const { return header; }
   inline CRcsFile& RcsFile() { return header; }

protected:
   CRcsFile header;
};

class CLogNodeRev : public CLogNode
{
public:
   CLogNodeRev(const CRevFile& r, CLogNode * root = 0)
      : CLogNode(root),
        rev(r),
        nextDeleted(false),
        prevDeleted(false),
        thisDeleted(false)
   {
   }

   virtual kLogNode GetType() const { return kNodeRev; }

   inline const CRevFile& Rev() const { return rev; }
   inline CRevFile& Rev() { return rev; }

   void SetNextDeleted(bool deleted) { nextDeleted = deleted; }
   bool GetNextDeleted() const       { return nextDeleted; }

   void SetPrevDeleted(bool deleted) { prevDeleted = deleted; }
   bool GetPrevDeleted() const       { return prevDeleted; }

   void SetThisDeleted(bool deleted) { thisDeleted = deleted; }
   bool GetThisDeleted() const       { return thisDeleted; }

protected:
   CRevFile     rev;
   bool         nextDeleted;
   bool         prevDeleted;
   bool         thisDeleted;
};

class CLogNodeTag : public CLogNode
{
public:
   CLogNodeTag(const CLogStr& t, CLogNode * r = 0L) :
      CLogNode(r), tag(t) {}

   virtual kLogNode GetType() const { return kNodeTag; }

   inline const CLogStr& Tag() const { return tag; }
   inline CLogStr& Tag() { return tag; }

protected:
   CLogStr tag;
};

class CLogNodeBranch : public CLogNode
{
public:
   CLogNodeBranch(const CLogStr& b, CLogNode * r = 0)
      : CLogNode(r),
        myBranch(b)
   {
   }

   virtual ~CLogNodeBranch() {}
   
   virtual kLogNode GetType() const { return kNodeBranch; }

   inline const CLogStr& Branch() const { return myBranch; }
   inline CLogStr& Branch() { return myBranch; }

protected:
   CLogStr myBranch;
};

// parse a file and return the set of RCS files
std::vector<CRcsFile>& CvsLogParse(FILE * file);

// free the memory used
void CvsLogReset();

struct CvsLogParameters
{
   CvsLogParameters(const std::string&                 filename,
                    bool                               currentBranchOnly,
                    bool                               collapseRevisions,
                    bool                               showtags,
                    const std::string&                 onlyShownBranch,
                    const std::vector<std::string>&    hiddenBranches,
                    const std::string&                 startDate,
                    const std::string&                 endDate)
      : myFilename(filename),
        myCurrentBranchOnly(currentBranchOnly),
        myCollapseRevisions(collapseRevisions),
        myShowtags(showtags),
        myOnlyShownBranch(onlyShownBranch),
        myHiddenBranches(hiddenBranches),
        myStartDate(startDate),
        myEndDate(endDate)
   {
   }

   std::string                          myFilename;
   bool                                 myCurrentBranchOnly;
   bool                                 myCollapseRevisions;
   bool                                 myShowtags;
   const std::string&                   myOnlyShownBranch;
   const std::vector<std::string>&      myHiddenBranches;
   const std::string&                   myStartDate;
   const std::string&                   myEndDate;
};

// build a tree (i.e. graph) of the history
CLogNode *CvsLogGraph(struct ParserData& parserData,
                      CvsLogParameters* settings = 0);

#endif
