/*
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <cvsapi.h>
#include "export.h"
#include "Scramble.h"

/* The only source of this data is the original scramble.cpp.. it's
   undocumented otherwise.  Given that all cvs clients must be using
   this data I think we can gloss over the potential licensing problem
   of using it (it's just a list of 256 random numbers) */
const unsigned char CScramble::m_lookup[256] =
{
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
   16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
  114,120, 53, 79, 96,109, 72,108, 70, 64, 76, 67,116, 74, 68, 87,
  111, 52, 75,119, 49, 34, 82, 81, 95, 65,112, 86,118,110,122,105,
   41, 57, 83, 43, 46,102, 40, 89, 38,103, 45, 50, 42,123, 91, 35,
  125, 55, 54, 66,124,126, 59, 47, 92, 71,115, 78, 88,107,106, 56,
   36,121,117,104,101,100, 69, 73, 99, 63, 94, 93, 39, 37, 61, 48,
   58,113, 32, 90, 44, 98, 60, 51, 33, 97, 62, 77, 84, 80, 85,223,
  225,216,187,166,229,189,222,188,141,249,148,200,184,136,248,190,
  199,170,181,204,138,232,218,183,255,234,220,247,213,203,226,193,
  174,172,228,252,217,201,131,230,197,211,145,238,161,179,160,212,
  207,221,254,173,202,146,224,151,140,196,205,130,135,133,143,246,
  192,159,244,239,185,168,215,144,139,165,180,157,147,186,214,176,
  227,231,219,169,175,156,206,198,129,164,150,210,154,177,134,127,
  182,128,158,208,162,132,167,209,149,241,153,251,237,236,171,195,
  243,233,253,240,194,250,191,155,142,137,245,235,163,242,178,152 
};

/* Since the pserver protocol wasn't that well designed, we'll never
   be able to change the method (currently 'A') without breaking
   compatibility with other servers out there.  This sucks, but
   we're stuck with it. */
const char *CScramble::Scramble(const char *plaintext)
{
	const unsigned char *s = (unsigned char *)plaintext;
	unsigned char *d;

	m_scramble.resize(strlen(plaintext)+1);
	d=(unsigned char*)m_scramble.data();
	*(d++)='A';
	while(*s) *(d++)=m_lookup[*(s++)];

	return m_scramble.c_str();
}

const char *CScramble::Unscramble(const char *cypher)
{
	const unsigned char *s = (unsigned char *)(cypher+1);
	unsigned char *d;

	if(cypher[0]!='A')
		return NULL;

	m_scramble.resize(strlen(cypher)-1);
	d=(unsigned char*)m_scramble.data();
	while(*s) *(d++)=m_lookup[*(s++)];

	return m_scramble.c_str();
}

