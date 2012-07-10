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
#include "cvsdelta.h"

int cvsdelta_diff(const void *buf1, size_t buf1_len, const void *buf2, size_t buf2_len, void **buf3, size_t *buf3_len)
{
  cvsdelta c;
  const ByteArray b1((Byte*)buf1, buf1_len);
  const ByteArray b2((Byte*)buf2, buf2_len);
  ByteArray b3((Byte*)NULL, 0);

  int ret = (c.diff(b1,b2,b3)==true)?0:1;
  *buf3=&b3[0];
  *buf3_len=b3.size();
  return ret;
}

int cvsdelta_patch(const void *buf1, size_t buf1_len, const void *buf2, size_t buf2_len, void **buf3, size_t *buf3_len, size_t *buf3_reserved)
{
  cvsdelta c;
  const ByteArray b1((Byte*)buf1, buf1_len);
  const ByteArray b2((Byte*)buf2, buf2_len);
  ByteArray b3((Byte*)*buf3, *buf3_reserved);

  int ret = (c.patch(b1,b2,b3)==true)?0:1;
  *buf3=&b3[0];
  *buf3_len=b3.size();
  *buf3_reserved=b3.reserved_size();
  return ret;
}
