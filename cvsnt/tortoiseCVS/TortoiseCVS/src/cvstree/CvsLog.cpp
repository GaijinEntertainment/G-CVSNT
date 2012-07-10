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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <algorithm>
#include <limits>
#include <sstream>

#include <Utils/TortoiseDebug.h>
#include <Utils/TimeUtils.h>
#include <Utils/Preference.h>

#include "common.h"
#include "CvsLog.h"


static bool KeepOnlyBranch(CLogNode* header, const std::string& onlyShownBranch);
static bool HideBranches(CLogNode* header, const std::vector<std::string>& hiddenBranches);
static void PurgeBoringRevisions(CLogNode* header, bool showtags);
static void TrimDateRange(CLogNode* header, const std::string& startDate, const std::string& enddate);
static bool TrimDateRange(CLogNode* header, time_t startTime, time_t endTime);

//
// CRevNumber
//

void CRevNumber::reset(void)
{
   allDigits.erase(allDigits.begin(), allDigits.end());
   tagName = "";
}

CRevNumber& CRevNumber::operator+=(int adigit)
{
   allDigits.push_back(adigit);
   return *this;
}

CRevNumber::CRevNumber(const char* p)
{
   SetDigits(p);
}

CRevNumber& CRevNumber::operator=(const CRevNumber& arev)
{
   reset();
   for(int i = 0; i < arev.size(); i++)
   {
      *this += arev[i];
   }
   tagName = arev.Tag();
   return *this;
}

int CRevNumber::operator[](int index) const
{
   if(index < 0 || index >= size())
      return -1;
   return allDigits[index];
}

std::string CRevNumber::str() const
{
   std::stringstream ss;
   if(allDigits.empty())
      return ss.str();
   std::vector<int>::const_iterator i = allDigits.begin();
   ss << *i;
   ++i;
   for(; i != allDigits.end(); ++i)
   {
      ss << '.';
      ss << *i;
   }
   return ss.str();
}

int CRevNumber::cmp(const CRevNumber& arev) const
{
   const std::vector<int>& rev1 = IntList();
   const std::vector<int>& rev2 = arev.IntList();

   std::vector<int>::const_iterator i = rev1.begin();
   std::vector<int>::const_iterator j = rev2.begin();
   while(i != rev1.end() && j != rev2.end())
   {
      if((*i) != (*j))
         return (*i) < (*j) ? -1 : 1;
      ++i;
      ++j;
   }
   if(i == rev1.end() && j != rev2.end())
      return -1;
   if(i != rev1.end() && j == rev2.end())
      return 1;
   return 0;
}

bool CRevNumber::operator==(const CRevNumber& arev) const
{
   if (empty() || (size() != arev.size()))
      return false;

   const std::vector<int>& rev1 = IntList();
   const std::vector<int>& rev2 = arev.IntList();
   std::vector<int>::const_iterator i = rev1.begin();
   for (std::vector<int>::const_iterator j = rev2.begin(); j != rev2.end(); ++i, ++j)
   {
      if (*i != *j)
         return false;
   }
   return true;
}

bool CRevNumber::ischildof(const CRevNumber& arev) const
{
   if (empty() || arev.empty())
      return false;

   if((arev.size() + 2) != size())
      return false;

   const std::vector<int>& rev1 = IntList();
   const std::vector<int>& rev2 = arev.IntList();
   std::vector<int>::const_iterator i = rev1.begin();
   std::vector<int>::const_iterator j = rev2.begin();
   for (std::vector<int>::const_iterator j = rev2.begin(); j != rev2.end(); ++i, ++j)
   {
      if (*i != *j)
         return false;
   }
   return true;
}

bool CRevNumber::issamebranch(const CRevNumber& arev) const
{
   if(size() != arev.size() || size() == 0)
      return false;
   const std::vector<int> & rev1 = IntList();
   const std::vector<int> & rev2 = arev.IntList();
   std::vector<int>::const_iterator i = rev1.begin();
   std::vector<int>::const_iterator j = rev2.begin();
   for (int idx = 0; idx < size() - 1; ++idx, ++i, ++j)
   {
      if (*i != *j)
         return false;
   }
   return true;
}

bool CRevNumber::ispartof(const CRevNumber& arev) const
{
   const std::vector<int> & rev1 = IntList();
   const std::vector<int> & rev2 = arev.IntList();
   std::vector<int>::const_iterator i = rev1.begin();
   std::vector<int>::const_iterator j = rev2.begin();

   if(size() == 0 || arev.size() == 0)
      return false;

   if((arev.size() & 1) != 0)
   {
      // special case for "1.1.1"
      if((arev.size() + 1) != size())
         return false;
      return memcmp(&*i, &*j, arev.size() * sizeof(int)) == 0;
   }

   // case for 1.1.1.1.2.4 is part of 1.1.1.1.0.2
   if(arev.size() != size() || size() < 2)
      return false;

   if(memcmp(&*i, &*j, (size() - 2) * sizeof(int)) != 0)
      return false;

   return arev.IntList()[size() - 2] == 0 &&
      IntList()[size() - 2] == arev.IntList()[size() - 1];
}

bool CRevNumber::issubbranchof(const CRevNumber& arev) const
{
   const std::vector<int> & rev1 = IntList();
   const std::vector<int> & rev2 = arev.IntList();
   std::vector<int>::const_iterator i = rev1.begin();
   std::vector<int>::const_iterator j = rev2.begin();

   if(size() == 0 || arev.size() == 0)
      return false;

   if((size() & 1) != 0)
   {
      // special case for "1.1.1"
      if((arev.size() + 1) != size())
         return false;
      return memcmp(&*i, &*j, arev.size() * sizeof(int)) == 0;
   }

   // case for 1.4.0.2 is subbranch of 1.4
   if((arev.size() + 2) != size() || IntList()[arev.size()] != 0)
      return false;

   return memcmp(&*i, &*j, arev.size() * sizeof(int)) == 0;
}


// Set digits from string
void CRevNumber::SetDigits(const char *s)
{
   const char *p = s;
   char la;
   char *b;
   char buf[10];
   int l = static_cast<int>(strlen(s));

   // Erase digits   
   allDigits.clear();
   if (l == 0)
      return;
      
   b = buf;
   
   // Iterate through the string
   while (*p)
   {
      if (b - buf >= 10)
         break;
      la = *(p + 1);
      *b = *p;
      b++;
      *b = 0;

      if (la == '.' || la == '\0')
      {
         *b = 0;
         allDigits.push_back(atoi(buf));
         b = buf;
         if (la == '.')
            p++;
      }
      p++;
   }
}


//
// CLogStr
//

const char *CLogStr::operator=(const char *newstr)
{
   if (!newstr)
   {
      str.clear();
      return 0;
   }
   str = newstr;
   return c_str();
}

const CLogStr & CLogStr::operator=(const CLogStr & newstr)
{
   str = newstr.str;
   return *this;
}

const CLogStr & CLogStr::set(const char *buf, unsigned int len)
{
   str.clear();
   if(len == 0)
      return *this;

   str.assign(buf, len);

   return *this;
}

const CLogStr & CLogStr::replace(char which, char bywhich)
{
   for (size_t i = 0; i < str.size(); ++i)
   {
      if (str[i] == which)
         str[i] = bywhich;
   }
   return *this;
}

CLogStr & CLogStr::operator<<(const char *addToStr)
{
   if(addToStr == 0L)
      return *this;

   str.append(addToStr);
   
   return *this;
}

CLogStr & CLogStr::operator<<(char addToStr)
{
   str.append(1, addToStr);
   return *this;
}

CLogStr & CLogStr::operator<<(int addToStr)
{
   char astr[50];
   sprintf(astr, "%d", addToStr);
   str.append(astr);
   return *this;
}

std::ostream& operator<<(std::ostream& stream, const CLogStr& s)
{
   stream << s.c_str();
   return stream;
}

//
// CRcsFile
//

CRcsFile::CRcsFile()
{
   selRevisions = 0;
   totRevisions = 0;
   lockStrict = false;
}

CRcsFile::CRcsFile(const CRcsFile & afile)
{
   *this = afile;
}

CRcsFile::~CRcsFile()
{
}

CRcsFile & CRcsFile::operator=(const CRcsFile & afile)
{
   rcsFile = afile.rcsFile;
   workingFile = afile.workingFile;
   headRev = afile.headRev;
   branchRev = afile.branchRev;
   keywordSubst = afile.keywordSubst;
   accessList = afile.accessList;
   symbolicList = afile.symbolicList;
   locksList = afile.locksList;
   selRevisions = afile.selRevisions;
   totRevisions = afile.totRevisions;
   lockStrict = afile.lockStrict;
   allRevs = afile.allRevs;
   descLog = afile.descLog;
   return *this;
}

void CRcsFile::print(OSTREAM & out) const
{
   out << "Rcs file : '" << RcsFile() << "'\n";
   out << "Working file : '" << WorkingFile() << "'\n";
   out << "Head revision : " << HeadRev().str() << '\n';
   out << "Branch revision : " << BranchRev().str() << '\n';

   out << "Locks :" << (LockStrict() ? " strict" : "") << '\n';
   std::vector<CRevNumber>::const_iterator s;
   for(s = LocksList().begin(); s != LocksList().end(); ++s)
   {
      const CRevNumber& lock = *s;
      out << '\t' << lock.str() << " : '" << lock.Tag() << "'\n";
   }

   out << "Access :\n";
   std::vector<CLogStr>::const_iterator a;
   for(a = AccessList().begin(); a != AccessList().end(); ++a)
   {
      out << "\t'" << *a << "'\n";
   }

   out << "Symbolic names :\n";
   std::vector<CRevNumber>::const_iterator n;
   for(n = SymbolicList().begin(); n != SymbolicList().end(); ++n)
   {
      const CRevNumber& symb = *n;
      out << '\t' << symb.str() << " : '" << symb.Tag() << "'\n";
   }

   out << "Keyword substitution : '" << KeywordSubst() << "'\n";
   out << "Total revisions : " << TotRevisions() << "\n";
   out << "Selected revisions : " << SelRevisions() << "\n";
   out << "Description :\n" << DescLog() << '\n';

   std::vector<CRevFile>::const_iterator i;
   for(i = AllRevs().begin(); i != AllRevs().end(); ++i)
   {
      i->print(out);
   }
}

// operator< for sorting
static int sortRevs(const CRevFile& rev1, const CRevFile& rev2)
{
   return rev1.RevNum().cmp(rev2.RevNum()) < 0;
}

void CRcsFile::sort()
{
   std::sort(AllRevs().begin(), AllRevs().end(), sortRevs);
}

//
// CRevFile
//

CRevFile::CRevFile()
   : hasTag(false),
     isMergePointTarget(false)
{
   chgPos = 0;
   chgNeg = 0;
   memset(&revTime, 0, sizeof(revTime));
}

CRevFile::CRevFile(const CRevFile & afile)
{
   *this = afile;
}

CRevFile::~CRevFile()
{
}

CRevFile & CRevFile::operator=(const CRevFile & afile)
{
   revNum = afile.revNum;
   revTime = afile.revTime;
   locker = afile.locker;
   branchesList = afile.branchesList;
   author = afile.author;
   state = afile.state;
   chgPos = afile.chgPos;
   chgNeg = afile.chgNeg;
   descLog = afile.descLog;
   keywordSubst = afile.keywordSubst;
   commitID = afile.commitID;
   mergePoint = afile.mergePoint;
   bugNumber = afile.bugNumber;
   filename = afile.filename;
   hasTag = afile.hasTag;
   isMergePointTarget = afile.isMergePointTarget;
   return *this;
}

void CRevFile::print(OSTREAM & out) const
{
   out << "----------------------------\n";
   out << "Revision : " << RevNum().str() << '\n';
   if(!Locker().empty())
      out << "Locked by : '" << Locker() << "'\n";
   out << "Date : " <<
      (RevTime().tm_year + 1900) << '/' <<
      RevTime().tm_mon << '/' <<
      RevTime().tm_mday << ' ' <<
      RevTime().tm_hour << ':' <<
      RevTime().tm_min << ':' <<
      RevTime().tm_sec << '\n';
   out << "Author : '" << Author() << "'\n";
   out << "State : '" << State() << "'\n";
   out << "Lines : +" << ChgPos() << ' ' << ChgNeg() << '\n';

   if(!BranchesList().empty())
   {
      out << "Branches :\n";
      std::vector<CRevNumber>::const_iterator s;
      for(s = BranchesList().begin(); s != BranchesList().end(); ++s)
      {
         const CRevNumber& branch = *s;
         out << '\t' << branch.str() << '\n';
      }
   }
   if(!KeywordSubst().empty())
      out << "Keyword : '" << KeywordSubst() << "'\n";
   if(!CommitID().empty())
      out << "CommitID : '" << CommitID() << "'\n";
   if(!MergePoint().empty())
      out << "MergePoint : '" << MergePoint().str() << "'\n";
   if(!BugNumber().empty())
      out << "BugID : '" << BugNumber() << "'\n";
   
   out << "Description :\n" << DescLog() << '\n';
}


// Set revision time
void CRevFile::SetRevTime(const char *s)
{
   int state;
   const char *p;
   char *b;
   char buf[30];

   p = s;
   b = buf;
   state = 0;
   while (*p && state < 6)
   {
      *b = *p;
      b++;
      p++;
      
      if (!isdigit(*p))
      {
         *b = '\0';
         switch (state)
         {
            case 0:
               revTime.tm_year = atoi(buf) - 1900;
               break;
               
            case 1:
               revTime.tm_mon = atoi(buf) - 1;
               break;
               
            case 2:
               revTime.tm_mday = atoi(buf);
               break;
               
            case 3:
               revTime.tm_hour = atoi(buf);
               break;
               
            case 4:
               revTime.tm_min = atoi(buf);
               break;
              
            case 5:
               revTime.tm_sec = atoi(buf);
               break;
               
            default:
               break;
         }
         b = buf;
         state++;     
            
         while (*p && !isdigit(*p))
            p++;
      }
   }
}


// Set lines
void CRevFile::SetLines(const char *s)
{
   int state;
   const char *p;
   char *b;
   char buf[30];

   // skip to '+'
   p = s;
   state = 0;
   
   while (*p && state < 2)
   {
      b = buf;
      while (*p && *p != '+' && *p != '-' && !isdigit(*p))
        p++;
      *b = *p;
      p++;
      b++;

      while(*p && isdigit(*p))
      {
         *b = *p;
         p++;
         b++;
      }
      *b = '\0';
      
      switch (state)
      {
         case 0:
            chgPos = atoi(buf);
            break;
         
         case 1:
            chgNeg = atoi(buf);
            break;
      }
      state++;
   }
}

void CRevFile::CheckTags(const std::vector<CRevNumber>& taglist)
{
   for (std::vector<CRevNumber>::const_iterator it = taglist.begin(); it != taglist.end(); ++it)
   {
      if (*it == revNum)
      {
         hasTag = true;
         break;
      }
   }
}


std::string CRevFile::Changes() const
{
   std::ostringstream ss;
   ss << "+" << chgPos << " " << (chgNeg == 0 ? "-" : "") << chgNeg;
   return ss.str();
}

wxString CRevFile::DateAsString() const
{
   wxString result;
   wxChar szDate[100];
   
   if (revTime.tm_year && tm_to_local_DateTimeFormatted(&revTime, szDate, sizeof(szDate), false))
      result = szDate;

   return result;
}

//
// tree nodes
//

CLogNode::CLogNode(CLogNode * r)
{
   root = r;
   next = 0L;
   user = 0L;
}

CLogNode::~CLogNode()
{
   delete Next();
   std::vector<CLogNode *>::iterator i;
   for(i = Childs().begin(); i != Childs().end(); ++i)
   {
      delete *i;
      *i = 0;
   }
   delete user;
}

const CLogNode* CLogNode::FindBranchNode(const std::string& branch, const CLogNode* /*!!*/)
{
   if (GetType() == kNodeBranch)
   {
      CLogNodeBranch* branchNode = static_cast<CLogNodeBranch*>(this);
      if (std::string(branchNode->Branch().c_str()) == branch)
         return branchNode;
   }
   for (ChildList::iterator it = childs.begin(); it != childs.end(); ++it)
   {
       const CLogNode* n = (*it)->FindBranchNode(branch, this);
       if (n)
          return n;
   }
   if (Next())
      return Next()->FindBranchNode(branch, 0);
   return 0;
}

bool CLogNode::IsChildOf(const CLogNode* node) const
{
   for (ChildList::const_iterator it = node->Childs().begin(); it != node->Childs().end(); ++it)
   {
      if (*it == this)
         return true;
      if (IsChildOf(*it))
         return true;
   }
   return false;
}


static bool insertSymbName(CLogNode *node, const CRevNumber& symb, int maxTags)
{
   std::vector<CLogNode *>::iterator i;
   if(node->GetType() == kNodeRev)
   {
      CLogNodeRev& rev = *((CLogNodeRev*) node);
      if(rev.Rev().RevNum() == symb)
      {
         int tagCount = 0;
         for(i = node->Childs().begin(); i != node->Childs().end(); ++i)
         {
            if( (*i)->GetType() == kNodeTag )
                ++tagCount;
         }

         if(maxTags == -1 || tagCount < maxTags)
         {
            // Insert the tag as a child of the node
            CLogNodeTag *tag = new CLogNodeTag(symb.Tag(), node);
            node->Childs().push_back(tag);
         }

         return true;
      }
   }

   for(i = node->Childs().begin(); i != node->Childs().end(); ++i)
   {
      if(insertSymbName(*i, symb, maxTags))
         return true;
   }
   if (node->Next() && insertSymbName(node->Next(), symb, maxTags))
      return true;

   return false;
}

static bool insertBranchName(CLogNode *node, const CRevNumber& symb)
{
   for (std::vector<CLogNode*>::iterator i = node->Childs().begin(); i != node->Childs().end(); ++i)
   {
      CLogNode* subnode = *i;
      if(subnode->GetType() == kNodeRev)
      {
         CLogNodeRev* rev = dynamic_cast<CLogNodeRev*>(subnode);
         if (rev->Rev().RevNum().ispartof(symb))
         {
            // insert the branch name as previous node of the
            // first node of that branch
            CLogNodeBranch* branch = new CLogNodeBranch(symb.Tag(), node);
            branch->Next() = subnode;
            *i = branch;
            return true;
         }
      }
      if (insertBranchName(subnode, symb))
         return true;
   }
   if(node->Next() && insertBranchName(node->Next(), symb))
      return true;

   // we didn't find to connect this branch because there is no
   // revision in this branch (empty branch) : so add it as a child of this node.
   if(node->GetType() == kNodeRev)
   {
      CLogNodeRev & rev = *(CLogNodeRev *)node;
      if(symb.issubbranchof(rev.Rev().RevNum()))
      {
         CLogNodeBranch *branch = new CLogNodeBranch(symb.Tag(), node);
         node->Childs().push_back(branch);
      }
   }

   return false;
}

CLogNode* CvsLogGraph(ParserData& parserData,
                      CvsLogParameters* settings)
{
   TDEBUG_ENTER("CvsLogGraph");

   CRcsFile& rcsfile = parserData.rcsFiles[0];

   // we sort the revisions in order to build the tree
   // using a stack-algorithm
   rcsfile.sort();

   CLogNodeHeader* header = new CLogNodeHeader(rcsfile);
   CLogNodeRev* curNode = 0L;

   // Compute mergepoint targets
   std::vector<CRevFile>::iterator i;
   for (i = rcsfile.AllRevs().begin(); i != rcsfile.AllRevs().end(); ++i)
   {
      TDEBUG_TRACE("Rev " << i->RevNum().str());
      std::vector<CRevFile>::const_iterator j;
      for (j = rcsfile.AllRevs().begin(); j != rcsfile.AllRevs().end(); ++j)
         if (i->RevNum() == j->MergePoint())
         {
            TDEBUG_TRACE("MP: " << j->MergePoint().str());
            i->IsMergePointTarget() = true;
            break;
         }
   }
   
   // Append the revisions to the tree
   for (i = rcsfile.AllRevs().begin(); i != rcsfile.AllRevs().end(); ++i)
   {
      const CRevFile& rev = *i;
      TDEBUG_TRACE("Rev " << rev.RevNum().str());
      
      if (!curNode)
      {
         CLogNodeRev* nrev = new CLogNodeRev(rev, header);
         header->Childs().push_back(nrev);
         curNode = nrev;
         continue;
      }

      do
      {
         const CRevNumber& curRev = curNode->Rev().RevNum();
         const CRevNumber& thisRev = rev.RevNum();

         if (thisRev.ischildof(curRev))
         {
            TDEBUG_TRACE("Child of " << curRev.str());
            CLogNodeRev *nrev = new CLogNodeRev(rev, curNode);
            curNode->Childs().push_back(nrev);
            curNode = nrev;
            break;
         }
         else if (thisRev.issamebranch(curRev))
         {
            TDEBUG_TRACE("Same branch as " << curRev.str());
            CLogNodeRev* nrev = new CLogNodeRev(rev, curNode);
            curNode->Next() = nrev;
            curNode = nrev;
            break;
         }
         if (curNode->Root() == header)
            curNode = 0;
         else
            curNode = (CLogNodeRev*) curNode->Root();
      } while (curNode);

      if (!curNode)
      {
         CLogNodeRev *nrev = new CLogNodeRev(rev, header);
         header->Childs().push_back(nrev);
         curNode = nrev;
      }
   }

   if (!settings || settings->myShowtags)
   {
       // append the tags
       std::string maxTagsString(GetStringPreference("GraphMaxTags", settings ? settings->myFilename : ""));
       int maxTags = -1;
       if (!maxTagsString.empty())
           maxTags = atoi(maxTagsString.c_str());
       for (std::vector<CRevNumber>::const_iterator s = rcsfile.SymbolicList().begin(); 
            s != rcsfile.SymbolicList().end(); ++s)
       {
           TDEBUG_TRACE("Add tag " << s->str());
           insertSymbName(header, *s, maxTags);
       }
   }

   // append the branch names
   for (std::vector<CRevNumber>::const_iterator s = rcsfile.SymbolicList().begin(); 
        s != rcsfile.SymbolicList().end(); ++s)
   {
      TDEBUG_TRACE("Add branch " << s->str());
      insertBranchName(header, *s);
   }
   
   if (settings)
   {
      if (!settings->myOnlyShownBranch.empty())
         KeepOnlyBranch(header, settings->myOnlyShownBranch);
      else if (!settings->myHiddenBranches.empty())
         HideBranches(header, settings->myHiddenBranches);
   
      if (settings->myCollapseRevisions)
         PurgeBoringRevisions(header, settings->myShowtags);

      if (!settings->myStartDate.empty() || !settings->myEndDate.empty())
      {
         TrimDateRange(header, settings->myStartDate, settings->myEndDate);
      }
   }
   return header;
}

// Remove all nodes *not* belonging to the specified branch.
static bool KeepOnlyBranch(CLogNode* header, const std::string& onlyShownBranch)
{
   TDEBUG_ENTER("KeepOnlyBranch");
   bool result = false;

   CLogNode* cur = header;
   CLogNode* prev = 0;

   while (cur)
   {
      switch (cur->GetType())
      {
      case kNodeHeader:
         TDEBUG_TRACE("Header");
         break;
      case kNodeBranch:
      {
         CLogNodeBranch* branchNode = (CLogNodeBranch*) cur;
         TDEBUG_TRACE("Branch " << branchNode->Branch().c_str());
         if (strcmp(onlyShownBranch.c_str(), branchNode->Branch().c_str()))
         {
            // This is not the branch we are looking for
            CLogNode* next = cur->Next();
            cur->Next() = 0;
            delete cur;
            result = true;
            cur = next;
            continue; // while ()
         }
         break;
      }
      case kNodeRev:
         TDEBUG_TRACE("Rev " << ((CLogNodeRev*) cur)->Rev().RevNum().str());
         break;
      case kNodeTag:
         TDEBUG_TRACE("Tag " << ((CLogNodeTag*) cur)->Tag().c_str());
         break;
      }
      // Use index instead of iterator to allow for deletion inside the loop
      CLogNode::ChildList& children = cur->Childs();
      for (size_t i = 0; i < children.size(); ++i)
         if (KeepOnlyBranch(children[i], onlyShownBranch))
            children.erase(children.begin() + i);
      prev = cur;
      cur = cur->Next();
   }
   return result;
}

// Remove all nodes belonging to any of the specified branches.
static bool HideBranches(CLogNode* header, const std::vector<std::string>& hiddenBranches)
{
   TDEBUG_ENTER("HideBranches");
   bool result = false;
   
   CLogNode* cur = header;
   CLogNode* prev = 0;

   while (cur)
   {
      switch (cur->GetType())
      {
      case kNodeHeader:
         TDEBUG_TRACE("Header");
         break;
      case kNodeBranch:
      {
         CLogNodeBranch* branchNode = (CLogNodeBranch*) cur;
         TDEBUG_TRACE("Branch " << branchNode->Branch().c_str());
         bool foundOne = false;
         for (std::vector<std::string>::const_iterator it = hiddenBranches.begin();
              it != hiddenBranches.end(); ++it)
            if (!strcmp(it->c_str(), branchNode->Branch().c_str()))
            {
               // This branch is hidden
               TDEBUG_TRACE("Delete!");
               CLogNode* next = cur->Next();
               cur->Next() = 0;
               delete cur;
               // Entire branch is gone, so this child node must be removed from its parent
               result = true;
               cur = next;
               foundOne = true;
               break;   // for ()
            }
         if (foundOne)
            continue;   // while ()
         break;         // switch ()
      }
      case kNodeRev:
         TDEBUG_TRACE("Rev " << ((CLogNodeRev*) cur)->Rev().RevNum().str());
         break;
      case kNodeTag:
         TDEBUG_TRACE("Tag " << ((CLogNodeTag*) cur)->Tag().c_str());
         break;
      }
      // Use index instead of iterator to allow for deletion inside the loop
      CLogNode::ChildList& children = cur->Childs();
      for (size_t i = 0; i < children.size(); ++i)
         if (HideBranches(children[i], hiddenBranches))
            children.erase(children.begin() + i);

      prev = cur;
      cur = cur->Next();
   }
   return result;
}

// Purge uninteresting revisions
static void PurgeBoringRevisions(CLogNode* header, bool showtags)
{
   TDEBUG_ENTER("PurgeBoringRevisions");
   CLogNode* cur = header;
   CLogNode* prev = 0;

   bool first = true;
   while (cur)
   {
      CLogNode::ChildList& children = cur->Childs();
      switch (cur->GetType())
      {
      case kNodeHeader:
         break;
      case kNodeBranch:
         break;
      case kNodeRev:
      {
         CLogNodeRev* rev = (CLogNodeRev*) cur;
         // Only consider revisions not attached to merge points for purging
         TDEBUG_TRACE("Rev " << rev->Rev().RevNum().str() << ": "
                      << rev->Rev().IsMergePointTarget()
                      << " - " << rev->Rev().MergePoint().empty());
         if (!rev->Rev().IsMergePointTarget() && rev->Rev().MergePoint().empty())
         {
            bool noKids = children.empty();
            if (!noKids && !showtags)
            {
               // Check if all children are tags
               noKids = true;
               for (CLogNode::ChildList::iterator it = children.begin(); it != children.end(); ++it)
                  if ((*it)->GetType() != kNodeTag)
                  {
                     noKids = false;
                     break;
                  }
            }
            if (!first && noKids && cur->Next() && !((CLogNodeRev*) cur)->Rev().IsMergePointTarget())
            {
               // Neither first nor last, and no non-tag kids: Delete this node
               CLogNode* next = cur->Next();
               prev->Next() = next;
               ((CLogNodeRev*) prev)->SetNextDeleted(true);
               if (next)
                  ((CLogNodeRev*) next)->SetPrevDeleted(true);
               cur->Next() = 0;
               delete cur;
               cur = next;
               continue;
            }
         }
         first = false;
      }
      break;
      case kNodeTag:
          break;
      }
      for (CLogNode::ChildList::iterator it = children.begin(); it != children.end(); ++it)
         PurgeBoringRevisions(*it, showtags);
      prev = cur;
      cur = cur->Next();
   }
}

// Remove all nodes *not* within the specified date range.
static void TrimDateRange(CLogNode* header, const std::string& startDate, const std::string& endDate)
{
   // Convert human-readable dates to time_t and call the worker function.
   time_t startTime = startDate.empty() ? 0 : get_date(startDate.c_str());
   time_t endTime = endDate.empty() ? std::numeric_limits<time_t>::max() : get_date(endDate.c_str());
   TrimDateRange(header, startTime, endTime);
}

// Remove all nodes *not* within the specified date range.
// Return true if the top node was deleted.
static bool TrimDateRange(CLogNode* header, time_t startTime, time_t endTime)
{
   TDEBUG_ENTER("TrimDateRange");
   TDEBUG_TRACE("Range " << startTime << " to " << endTime);

   bool result = false;
   bool deletedFirstNode = false;
   bool first = true;
   CLogNode* cur = header;
   CLogNode* prev = 0;

   while (cur)
   {
      switch (cur->GetType())
      {
      case kNodeHeader:
         TDEBUG_TRACE("Header");
         break;
      case kNodeBranch:
         TDEBUG_TRACE("Branch");
         break;
      case kNodeRev:
      {
         CLogNodeRev* rev = (CLogNodeRev*) cur;
         struct tm tm = rev->Rev().RevTime();
         time_t t = mktime(&tm);
         TDEBUG_TRACE("Rev " << rev->Rev().RevNum().str() << ": " << t);
         if ((t < startTime) || (t > endTime))
         {
            // Delete this node
            CLogNode* next = cur->Next();
            if (next)
               ((CLogNodeRev*) next)->SetPrevDeleted(true);
            if (prev)
               ((CLogNodeRev*) prev)->SetNextDeleted(true);
            if (first)
            {
               rev->SetThisDeleted(true);
               TDEBUG_TRACE("Mark as deleted");
               // Avoid losing this node
               prev = cur;
               // Remove all child nodes
               CLogNode::ChildList& children = cur->Childs();
               for (CLogNode::ChildList::iterator it = children.begin(); it != children.end(); ++it)
                  delete *it;
               children.clear();
            }
            else
            {
               TDEBUG_TRACE("Delete");
               if (prev)
               {
                  if (next)
                  {
                     TDEBUG_TRACE("Link prev " << ((CLogNodeRev*) prev)->Rev().RevNum().str()
                                  << " to " << ((CLogNodeRev*) next)->Rev().RevNum().str());
                  }
                  else
                     TDEBUG_TRACE("Link prev " << ((CLogNodeRev*) prev)->Rev().RevNum().str()
                                  << " to 0");
                  prev->Next() = next;
               }
               cur->Next() = 0;
               delete cur;
            }
            first = false;
            if (!prev)
               deletedFirstNode = true;
            if (!next && deletedFirstNode)
               result = true;
            cur = next;
            continue;
         }
      }
      break;
      case kNodeTag:
         break;
      }
      // Use index instead of iterator to allow for deletion inside the loop
      CLogNode::ChildList& children = cur->Childs();
      for (size_t i = 0; i < children.size(); ++i)
         if (TrimDateRange(children[i], startTime, endTime))
         {
            TDEBUG_TRACE("Dump child");
            children.erase(children.begin() + i);
         }
      prev = cur;
      cur = cur->Next();
   }
   return result;
}
