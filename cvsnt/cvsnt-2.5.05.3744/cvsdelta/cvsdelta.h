/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef CVSDELTA__H
#define CVSDELTA__H

#ifdef __cplusplus

#ifdef __hpux__
#include <stdarg.h> // yes, hpux is *that* broken
#endif
#include <vector>

#include <stdlib.h>

typedef unsigned char Byte;
class ByteArray
{ 
protected:
  Byte* buf; 
  size_t len;
  size_t reserved; 

public:
  ByteArray() { buf=NULL; len=reserved=0; }
  ByteArray(Byte *b, size_t l) { buf=b; len=reserved=l; }
  size_t size() const { return len; }
  size_t reserved_size() const { return reserved; }
  void resize(size_t l) { reserve(l); len=l; }
  void reserve(size_t l) { if(l>reserved) { buf=(Byte*)realloc(buf,l); reserved=l; } }
  Byte& operator[](size_t off) const { return buf[off]; }
  void clear() { free(buf); len=reserved=0; buf=NULL; }
};

class cvsdelta
{
public:
	bool diff(const ByteArray& file1, const ByteArray& file2, ByteArray& output);
	bool patch(const ByteArray& file1, const ByteArray& file2, ByteArray& output);

protected:
	typedef struct __checksum_t
	{
	unsigned short low,high;
	struct __checksum_t* next;
	const Byte *base;
	bool new_data;
	} checksum_t;

	typedef struct
	{
		unsigned short version; /* 16bit short */
		unsigned short block_size; /* 16bit short */
		unsigned control_length; /* 32bit int */
		unsigned data_length; /* 32bit int */
		unsigned file_size; /* 32bit int */
	} cvsd_header;

	Byte block_size;
	unsigned prime;
	int check2_count;
	checksum_t *check1;
	checksum_t *check2;
	checksum_t **hash;

	void calc_check(const Byte *data, checksum_t *check);
	void slide_check(checksum_t *check);
	unsigned calc_hash(const checksum_t *check);
	int next_prime(unsigned int size);
	int copy_buffer(const Byte *from, unsigned start, unsigned len, ByteArray& data_buffer);
	unsigned write_control(char region, unsigned start, unsigned len, const Byte *data, ByteArray& control_buffer, ByteArray& data_buffer, bool from_data);
	unsigned match(const Byte *p1, const Byte *p2, const Byte **pbase, int max_back, int max_forward);
};

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

#define CVSDELTA_BLOCKPAD 256 /* Size of largest block plus some safety */
int cvsdelta_diff(const void *buf1, size_t buf1_len, const void *buf2, size_t buf2_len, void **buf3, size_t *buf3_len);
int cvsdelta_patch(const void *buf1, size_t buf1_len, const void *buf2, size_t buf2_len, void **buf3, size_t *buf3_len, size_t *buf3_reserved);

#ifdef __cplusplus
}
#endif

#endif
