// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - February 2003

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
#ifndef SYNC_UTILS_H
#define SYNC_UTILS_H

#include <windows.h>
#include "FixWinDefs.h"


class CriticalSection
{
public:
   CRITICAL_SECTION m_CS;   

   CriticalSection();
   ~CriticalSection();
   void Enter();
   void Leave();
};



// Helper class for CRITICAL_SECTION
class CSHelper
{
public:
   // Constructors
   inline CSHelper(CRITICAL_SECTION &cs, bool bEnter = false) : m_cs(&cs)
   {
      m_Inside = false;
      if (bEnter)
         Enter();
   }

   // Constructors
   inline CSHelper(CriticalSection &cs, bool bEnter = false) : m_cs(&cs.m_CS)
   {
      m_Inside = false;
      if (bEnter)
         Enter();
   }

   inline CSHelper() : m_cs(0)
   {
      m_Inside = false;
   }

   // Destructor
   inline ~CSHelper()
   {
      if (m_Inside)
         Leave();
   }

   // Enter
   inline void Enter()
   {
      if (!m_Inside && m_cs)
      {
         EnterCriticalSection(m_cs);
         m_Inside = true;
      }
   }

   // Leave
   inline void Leave()
   {
      if (m_Inside && m_cs)
      {
         LeaveCriticalSection(m_cs);
         m_Inside = false;
      }
   }

   // Test whether inside
   inline bool IsInside()
   {
      return m_Inside && m_cs;
   }

   // Attach
   inline void Attach(CRITICAL_SECTION& cs)
   {
      if (m_cs != &cs)
      {
         Leave();
         m_cs = &cs;
      }
   }

   // Attach
   inline void Attach(CriticalSection &cs)
   {
      if (m_cs != &cs.m_CS)
      {
         Leave();
         m_cs = &cs.m_CS;
      }
   }

private:
   // Associated synchronizer
   CRITICAL_SECTION *m_cs;

   // flags
   bool m_Inside;
};



#endif
