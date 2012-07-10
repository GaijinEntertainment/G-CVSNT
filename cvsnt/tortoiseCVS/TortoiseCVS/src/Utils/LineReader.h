// TortoiseCVS - a Windows shell extension for easy version control

// Copyright (C) 2003 - Hartmut Honisch
// <Hartmut_Honisch@web.de> - March 2003

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


#ifndef _LINEREADER_H
#define _LINEREADER_H

#include <iostream>
#include "TortoiseDebug.h"

template <class T> class LineReader
{
public:
   typedef typename T::value_type value_type;
   typedef typename T::size_type size_type;
      
   enum CrlfStyle
   {
     CrlfStyleAutomatic = -1,
     CrlfStyleDos = 0,
     CrlfStyleUnix = 1,
     CrlfStyleMac = 2
   };

   // Constructor
   LineReader(std::istream *in, CrlfStyle crlfStyle = CrlfStyleAutomatic);

   // Destructor
   virtual ~LineReader();

   // Guess CRLF style
   void GuessCrlfStyle();

   // Read a line
   bool ReadLine(T &sLine);

   // Set crlf style
   void SetCrlfStyle(CrlfStyle crlfStyle);

   // Get crlf style
   CrlfStyle GetCrlfStyle();

   // Converter
   std::istream *m_in;

protected:
   // Find buffer content
   const value_type *MemFind(const value_type *buf, size_type count, 
              const value_type *findbuf, size_type findcount);

   // Buffer size
   static const size_type m_bufferSize;
   // Buffer
   value_type *m_buffer;
   // Current length
   size_type m_bufferLength;
   // Buffer pointer
   size_type m_bufferPtr;
   // EOF
   bool m_eof;

   // CRLF-Style
   CrlfStyle m_crlfStyle;
   // CRLF according to style
   const value_type *m_crlf;
   // CRLF length
   size_type m_crlfLength;

private:

};



template <class T> const typename LineReader<T>::size_type LineReader<T>::m_bufferSize = 32768;



// Constructor
template <class T> LineReader<T>::LineReader(std::istream *in, CrlfStyle crlfStyle)
{
   m_in = in;
   if (crlfStyle != CrlfStyleAutomatic)
   {
      SetCrlfStyle(crlfStyle);
   }
   else
   {
      m_crlfStyle = CrlfStyleAutomatic;
      m_crlf = 0;
      m_crlfLength = 0;
   }
   m_buffer = 0;
   m_bufferLength = 0;
   m_bufferPtr = 0;
   m_eof = false;
}


// Destructor
template <class T> LineReader<T>::~LineReader()
{
   if (m_buffer)
   {
      delete m_buffer;
   }
}



// Guess CRLF style
template <class T> void LineReader<T>::GuessCrlfStyle()
{
   if (m_buffer == 0)
   {
      m_buffer = new value_type[m_bufferSize];
   }

   m_in->read((char*) m_buffer, m_bufferSize * sizeof(value_type));
   m_bufferLength = m_in->gcount() / sizeof(value_type);

   if (m_crlfStyle == CrlfStyleAutomatic)
   {
      //  Try to determine current CRLF mode
      size_type I;
      for (I = 0; I < m_bufferSize - 1; I++)
      {
         if ((m_buffer[I] == L'\x0d') || (m_buffer[I] == L'\x0a'))
           break;
      }
      if (I == m_bufferSize - 1)
      {
         //  By default (or in the case of empty file), set DOS style
         m_crlfStyle = CrlfStyleDos;
      }
      else
      {
         //  Otherwise, analyse the first occurance of line-feed character
         if (m_buffer[I] == L'\x0a')
         {
            m_crlfStyle = CrlfStyleUnix;
         }
         else
         {
             if (I < m_bufferSize - 1 && m_buffer[I + 1] == L'\x0a')
               m_crlfStyle = CrlfStyleDos;
             else
               m_crlfStyle = CrlfStyleMac;
         }
      }
   }
   SetCrlfStyle(m_crlfStyle);
}




// Read a line
template <class T> bool LineReader<T>::ReadLine(T &sLine)
{
   const int capacityMul = 2;
   const int capacityDiv = 1;
   const size_type minCapacity = 2048;
   size_type currentCapacity;
   size_type newCapacity;

   size_type dwCharsLeft;
   const value_type *pw;
   sLine.erase();
   if (m_eof)
   {
      return false;
   }

   if (m_buffer == 0)
   {
      m_buffer = new value_type[m_bufferSize];
   }

   if (m_crlfStyle == CrlfStyleAutomatic)
   {
      GuessCrlfStyle();
   }

   // Set min capacity
   currentCapacity = sLine.capacity();
   if (currentCapacity < minCapacity)
   {
      sLine.reserve(minCapacity);
      currentCapacity = minCapacity;
   }

   while (true)
   {
      // load next chunk of characters if buffer is empty
      dwCharsLeft = m_bufferLength - m_bufferPtr;
      if ((dwCharsLeft < m_crlfLength) && !m_eof)
      {
         // Copy remaining bytes to beginning of buffer
         if (dwCharsLeft > 0)
         {
            memcpy(&m_buffer[0], &m_buffer[m_bufferPtr], dwCharsLeft * sizeof(value_type));
         }
         m_bufferPtr = 0;
         // Read new chars
         DWORD dwCharsRead;

         m_in->read((char*) &m_buffer[dwCharsLeft], (m_bufferSize - dwCharsLeft) * sizeof(value_type));
         dwCharsRead = static_cast<DWORD>(m_in->gcount() / sizeof(value_type));

         m_eof = (dwCharsRead == 0);
         m_bufferLength = dwCharsLeft + dwCharsRead;
      }

      // scan for crlf
      pw = MemFind(&m_buffer[m_bufferPtr], m_bufferLength - m_bufferPtr, m_crlf, m_crlfLength);

      if (pw != 0)
      {
         // We found the end of the line
         size_type dwLen = pw - &m_buffer[m_bufferPtr];
         sLine.append(&m_buffer[m_bufferPtr], dwLen);
         m_bufferPtr += (dwLen + m_crlfLength);
         return true;
      }
      else if (m_eof)
      {
         // We're at the end of the file => take everything
         size_type dwLen = m_bufferLength - m_bufferPtr;
         ASSERT(dwLen >= 0);
         sLine.append(&m_buffer[m_bufferPtr], dwLen);
         m_bufferPtr += dwLen;
         return true;
      }
      else
      {
         // No crlf found => take entire buffer

         size_type dwLen = m_bufferLength - m_bufferPtr;
         dwLen -= (m_crlfLength - 1);
         if (dwLen > 0)
         {
            if (sLine.length() + dwLen > currentCapacity)
            {
               newCapacity = std::max(currentCapacity, sLine.length() + dwLen);
               newCapacity = newCapacity / capacityDiv * capacityMul;
               newCapacity = std::min(newCapacity, sLine.max_size());
               sLine.reserve(newCapacity);
               currentCapacity = newCapacity;
            }
            sLine.append(&m_buffer[m_bufferPtr], dwLen);
            m_bufferPtr += dwLen;
         }
      }
   }
}



// Get crlf style
template <class T> typename LineReader<T>::CrlfStyle LineReader<T>::GetCrlfStyle()
{
   return m_crlfStyle;
}


#endif
