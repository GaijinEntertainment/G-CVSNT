/*
	CVSNT binary delta generator
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
#include "config.h"

#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#endif
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "cvsdelta.h"

#if defined(_WIN32)
  #include <winsock2.h>
#else
 #ifdef HAVE_ARPA_INET_H
  #include <arpa/inet.h> 
 #elif defined(HAVE_NETINET_IN_H)
  #include <netinet/in.h> 
 #endif
#endif

#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

/* Certain systems don't allow nonaligned word comparisons, which is a real
   performance drag on the cvsdelta code as you have to do it a byte at a time */
#if defined(__sparc__) || defined(__hppa__)
#define USE_BYTE_COMPARES
#endif

static const unsigned short single_hash[256] =
{
  /* Random numbers generated using SLIB's pseudo-random number
   *    * generator. */
  0xbcd1, 0xbb65, 0x42c2, 0xdffe, 0x9666, 0x431b, 0x8504, 0xeb46,
  0x6379, 0xd460, 0xcf14, 0x53cf, 0xdb51, 0xdb08, 0x12c8, 0xf602,
  0xe766, 0x2394, 0x250d, 0xdcbb, 0xa678, 0x02af, 0xa5c6, 0x7ea6,
  0xb645, 0xcb4d, 0xc44b, 0xe5dc, 0x9fe6, 0x5b5c, 0x35f5, 0x701a,
  0x220f, 0x6c38, 0x1a56, 0x4ca3, 0xffc6, 0xb152, 0x8d61, 0x7a58,
  0x9025, 0x8b3d, 0xbf0f, 0x95a3, 0xe5f4, 0xc127, 0x3bed, 0x320b,
  0xb7f3, 0x6054, 0x333c, 0xd383, 0x8154, 0x5242, 0x4e0d, 0x0a94,
  0x7028, 0x8689, 0x3a22, 0x0980, 0x1847, 0xb0f1, 0x9b5c, 0x4176,
  0xb858, 0xd542, 0x1f6c, 0x2497, 0x6a5a, 0x9fa9, 0x8c5a, 0x7743,
  0xa8a9, 0x9a02, 0x4918, 0x438c, 0xc388, 0x9e2b, 0x4cad, 0x01b6,
  0xab19, 0xf777, 0x365f, 0x1eb2, 0x091e, 0x7bf8, 0x7a8e, 0x5227,
  0xeab1, 0x2074, 0x4523, 0xe781, 0x01a3, 0x163d, 0x3b2e, 0x287d,
  0x5e7f, 0xa063, 0xb134, 0x8fae, 0x5e8e, 0xb7b7, 0x4548, 0x1f5a,
  0xfa56, 0x7a24, 0x900f, 0x42dc, 0xcc69, 0x02a0, 0x0b22, 0xdb31,
  0x71fe, 0x0c7d, 0x1732, 0x1159, 0xcb09, 0xe1d2, 0x1351, 0x52e9,
  0xf536, 0x5a4f, 0xc316, 0x6bf9, 0x8994, 0xb774, 0x5f3e, 0xf6d6,
  0x3a61, 0xf82c, 0xcc22, 0x9d06, 0x299c, 0x09e5, 0x1eec, 0x514f,
  0x8d53, 0xa650, 0x5c6e, 0xc577, 0x7958, 0x71ac, 0x8916, 0x9b4f,
  0x2c09, 0x5211, 0xf6d8, 0xcaaa, 0xf7ef, 0x287f, 0x7a94, 0xab49,
  0xfa2c, 0x7222, 0xe457, 0xd71a, 0x00c3, 0x1a76, 0xe98c, 0xc037,
  0x8208, 0x5c2d, 0xdfda, 0xe5f5, 0x0b45, 0x15ce, 0x8a7e, 0xfcad,
  0xaa2d, 0x4b5c, 0xd42e, 0xb251, 0x907e, 0x9a47, 0xc9a6, 0xd93f,
  0x085e, 0x35ce, 0xa153, 0x7e7b, 0x9f0b, 0x25aa, 0x5d9f, 0xc04d,
  0x8a0e, 0x2875, 0x4a1c, 0x295f, 0x1393, 0xf760, 0x9178, 0x0f5b,
  0xfa7d, 0x83b4, 0x2082, 0x721d, 0x6462, 0x0368, 0x67e2, 0x8624,
  0x194d, 0x22f6, 0x78fb, 0x6791, 0xb238, 0xb332, 0x7276, 0xf272,
  0x47ec, 0x4504, 0xa961, 0x9fc8, 0x3fdc, 0xb413, 0x007a, 0x0806,
  0x7458, 0x95c6, 0xccaa, 0x18d6, 0xe2ae, 0x1b06, 0xf3f6, 0x5050,
  0xc8e8, 0xf4ac, 0xc04c, 0xf41c, 0x992f, 0xae44, 0x5f1b, 0x1113,
  0x1738, 0xd9a8, 0x19ea, 0x2d33, 0x9698, 0x2fe9, 0x323f, 0xcde2,
  0x6d71, 0xe37d, 0xb697, 0x2c4f, 0x4373, 0x9102, 0x075d, 0x8e25,
  0x1672, 0xec28, 0x6acb, 0x86cc, 0x186e, 0x9414, 0xd674, 0xd1a5
};

#define CHEW(x) (single_hash[(unsigned)x])

static const unsigned primes[] =
{
//  11, 19, 37, 73, 109, 163, 251, 367, 557, 823, 1237,
  1861, 2777, 4177, 6247, 9371, 14057, 21089, 31627,
  47431, 71143, 106721, 160073, 240101, 360163, 540217,
  810343, 1215497, 1823231, 2734867, 4102283, 6153409,
  9230113, 13845163
};

#define NUM_PRIMES (sizeof(primes)/sizeof(primes[0]))

void cvsdelta::calc_check(const Byte *data, checksum_t *check)
{
  int n;
  unsigned short low=0,high=0;

  check->next=0;
  check->base=data;
  check->new_data=false;

  for (n=0; n<block_size; n++)
  {
       low  += CHEW(*data++);
       high += low;
   }
  check->low=low;
  check->high=high;
}

void cvsdelta::slide_check(checksum_t *check)
{
   const Byte *p=check->base;
   unsigned short d0 = CHEW(*p);
   unsigned short d1 = CHEW(*(p+block_size));
   check->base++;
   check->low-=d0;
   check->high-=d0*block_size;
   check->low+=d1;
   check->high+=check->low;
}

unsigned cvsdelta::calc_hash(const checksum_t *check)
{
    const unsigned high = check->high;
    const unsigned low = check->low;
    const unsigned it = (high >> 2) + (low << 3) + (high << 16);

    return (it ^ high ^ low);
}

int cvsdelta::next_prime(unsigned int size)
{
  int n;

  for (n = 0; n < NUM_PRIMES; n++)
    if (primes[n]>size)
      return primes[n];

  return primes[NUM_PRIMES-1];
}

int cvsdelta::copy_buffer(const Byte *from, unsigned start, unsigned len, ByteArray& data_buffer)
{
	Byte *p;
	size_t count,n;
	size_t s = data_buffer.size();
	data_buffer.resize(s+len);
	memcpy((Byte*)&data_buffer[s],from+start,len);

	/* Merge the buffer into the calculated checksum data */
	count=len/block_size;
	for(n=0, p=(Byte*)&data_buffer[s]+(count-1)*block_size; n<count; n++, p-=block_size)
	{
		checksum_t *c = check2+n+check2_count;
		unsigned h;
		calc_check(p,c);
		h=calc_hash(c)%prime;
		c->next=hash[h];
		c->new_data=true;
		hash[h]=c;
	}
	check2_count+=n;

	return s;
}

unsigned cvsdelta::write_control(char region, unsigned start, unsigned len, const Byte *data, ByteArray& control_buffer, ByteArray& data_buffer, bool from_data)
{
  unsigned control=control_buffer.size();
  Byte *cp;
  unsigned char control_bit = region?0x80:0;

  control_buffer.resize(control_buffer.size()+32);

  cp = (Byte*)&control_buffer[control];

  if(region && !from_data)
    start = copy_buffer(data,start,len,data_buffer);

  if(len<0x20)
  {
    *(cp++)=(unsigned char)len|control_bit;
    control+=1;
  }
  else if(len<0x2000)
  {
    *(cp++)=(unsigned char)(len>>8)+0x20|control_bit;
    *(cp++)=(unsigned char)(len&0xFF);
    control+=2;
  }
  else if(len<0x200000)
  {
    *(cp++)=(unsigned char)(len>>16)+0x40|control_bit;
    *(cp++)=(unsigned char)((len&0xFF00)>>8);
    *(cp++)=(unsigned char)(len&0xFF);
    control+=3;
  }
  else if(len<0x20000000)
  {
    *(cp++)=(unsigned char)(len>>24)+0x60|control_bit;
    *(cp++)=(unsigned char)((len&0xFFFF00)>>16);
    *(cp++)=(unsigned char)((len&0xFF00)>>8);
    *(cp++)=(unsigned char)(len&0xFF);
    control+=4;
  }
  else assert(0); /* Max. chunksize limit 768MB */

  if(start<0x40)
  {
    *(cp++)=(unsigned char)start;
    control+=1;
  }
  else if(start<0x4000)
  {
    *(cp++)=(unsigned char)(start>>8)+0x40;
    *(cp++)=(unsigned char)(start&0xFF);
    control+=2;
  }
  else if(start<0x400000)
  {
    *(cp++)=(unsigned char)(start>>16)+0x80;
    *(cp++)=(unsigned char)((start&0xFF00)>>8);
    *(cp++)=(unsigned char)(start&0xFF);
    control+=3;
  }
  else if(start<0x40000000)
  {
    *(cp++)=(unsigned char)(start>>24)+0xC0;
    *(cp++)=(unsigned char)((start&0xFF0000)>>16);
    *(cp++)=(unsigned char)((start&0xFF00)>>8);
    *(cp++)=(unsigned char)(start&0xFF);
    control+=4;
  }
  else assert(0); /* Max chunkstart 1.6GB */

  control_buffer.resize(control);
  return control;
}

unsigned cvsdelta::match(const Byte *p1, const Byte *p2, const Byte **pbase, int max_back, int max_forward)
{
	const Byte *startp1=p1,*startp2=p2;
	const unsigned long *ul1=(const unsigned long *)p1,*ul2=(const unsigned long *)p2;
	const Byte *minp1 = p1 - max_back;
	const Byte *maxp1 = p1 + max_forward;
	const Byte *minp2 = p2 - max_back;
	const Byte *maxp2 = p2 + max_forward;

#ifndef USE_BYTE_COMPARES
	ul1=(const unsigned long *)p1;
	ul2=(const unsigned long *)p2;
	ul1--;
	ul2--;
	while(((const Byte *)ul1)>=minp1 && ((const Byte *)ul2)>=minp2 && *ul1==*ul2)
	{
		ul1--;
		ul2--;
	}
	ul1++;
	ul2++;

	p1=(const Byte *)ul1;
	p2=(const Byte *)ul2; 
#endif

	p1--;
	p2--;
	while(p1>=minp1 && p2>=minp2 && *p1==*p2)
	{
		p1--;
		p2--;
	}
	p1++;
	p2++;

	*pbase = p1;

	p1=startp1+block_size;
	p2=startp2+block_size;

#ifndef USE_BYTE_COMPARES
	ul1=(const unsigned long *)p1;
	ul2=(const unsigned long *)p2;
	while(((const Byte *)ul1)<=maxp1 && ((const Byte *)ul2)<=maxp2 && *ul1==*ul2)
	{
		ul1++;
		ul2++;
	}
	ul1--;
	ul2--;
#endif

	p1=(const Byte *)ul1;
	p2=(const Byte *)ul2; 
	while(p1<=maxp1 && p2<=maxp2 && *p1==*p2)
	{
		p1++;
		p2++;
	}
	p1--;
	p2--;

	return p1 - (*pbase);
}


bool cvsdelta::diff(const ByteArray& file1, const ByteArray& file2, ByteArray& output)
{
  unsigned n;
  const Byte *p;
  unsigned count1;
  unsigned count2;
  unsigned miss,hit,control;
  unsigned match_len;
  const Byte *match_start,*last_match;
  ByteArray data_buffer;
  ByteArray control_buffer;
  checksum_t c = {0};
  unsigned int cl,dl;
  unsigned real_size;

  if(file2.size()<=(256*1024))
	  block_size=8;
  else if(file2.size()<=(512*1024))
	  block_size=16;
  else if(file2.size()<=(1024*1024))
	  block_size=32;
  else if(file2.size()<=(2048*1024))
	  block_size=64;
  else
	  block_size=128;

  count1 = file1.size()/block_size;
  count2 = file2.size()/block_size;

  real_size = file2.size();

  if(file1.size()%block_size)
	count1++;

  if(file2.size()%block_size)
	count2++;

  prime = next_prime(count1);
  check1 = (checksum_t*)malloc(count1*sizeof(checksum_t));
  check2 = (checksum_t*)malloc(count2*sizeof(checksum_t));
  check2_count=0;
  hash = (checksum_t**)calloc(prime,sizeof(checksum_t*));
  data_buffer.clear();
  data_buffer.reserve(file2.size()); /* Reserve some memory */
  control_buffer.clear();
  control_buffer.reserve(1024); /* Reserve some memory */

  for(n=0, p=(&file1[0])+((count1-1)*block_size); n<count1; n++, p-=block_size)
  {
     checksum_t *c = check1+n;
     unsigned h;
     calc_check(p,c);
     h=calc_hash(c)%prime;
     c->next=hash[h];
     hash[h]=c;
  }

  miss=hit=control=0;
  last_match = &file2[0]; 
  for(n=0, p=&file2[0]; n<file2.size();)
  {
     checksum_t *c1;
     unsigned h;

     if(c.base==p-1)
       slide_check(&c);
     else 
       calc_check(p,&c);
     h=calc_hash(&c)%prime;
     c1=hash[h];
     while(c1 && (c.low!=c1->low || c.high!=c1->high || memcmp(c.base,c1->base,block_size)))
		c1=c1->next;
     if(c1)
	 {
		size_t f2s = file2.size();
		size_t f2off = (p-&file2[0]);
		size_t f1s,f1off;
		if(c1->new_data)
		{
		  f1off = (c1->base-&data_buffer[0]);
		  f1s = data_buffer.size();
		}
		else
		{
		  f1off = (c1->base-&file1[0]);
		  f1s = file1.size();
		}
		
		match_len = match(c.base,c1->base, &match_start, min(p-last_match,f1off), min(f2s-f2off,f1s-f1off));
	 }
     else
		match_len = 0;
     if(match_len)
     {
		unsigned matched_minus = c.base-match_start;
		unsigned matched_plus = match_len - matched_minus;
		if(match_start>last_match)
		{
			/* insert */
			control+=write_control(1,last_match-&file2[0],match_start-last_match,&file2[0],control_buffer,data_buffer, 0);
			miss+=match_start-last_match;
		}
		/* copy */
		if(c1->new_data)
		{
			assert(((c1->base-matched_minus)-&data_buffer[0])<=data_buffer.size());
			control+=write_control(1,(c1->base-matched_minus)-&data_buffer[0],match_len,NULL,control_buffer,data_buffer, 1);
		}
		else
		{
			assert(((c1->base-matched_minus)-&file1[0])<=file1.size());
			control+=write_control(0,(c1->base-matched_minus)-&file1[0],match_len,NULL,control_buffer,data_buffer, 0);
		}

		if(!matched_plus)
			matched_plus++;
		p+=matched_plus;
		n+=matched_plus;
		last_match=p;
		hit+=match_len;
     }
     else
     {
		p++;
		n++;
     }
  }
  
  if(p>last_match)
  {
	control+=write_control(1,last_match-&file2[0],p-last_match,&file2[0],control_buffer,data_buffer, 0);
	miss+=p-last_match;
  }


  cl = control_buffer.size();
  dl = data_buffer.size();

  output.resize(cl+dl+sizeof(cvsd_header));
  cvsd_header *h = (cvsd_header*)&output[0];
  h->version=htons(1);
  h->control_length=htonl(cl);
  h->data_length=htonl(dl);
  h->block_size=htons(block_size);
  h->file_size=htonl(real_size);
  memcpy((Byte*)&output[0]+sizeof(cvsd_header),&control_buffer[0],cl);
  memcpy((Byte*)&output[0]+cl+sizeof(cvsd_header),&data_buffer[0],dl);
  free(check1);
  free(check2);
  free(hash);
  control_buffer.clear();
  data_buffer.clear();
  return true;
}

// file1 original file
// file2 patch
// file3 output buffer
bool cvsdelta::patch(const ByteArray& file1, const ByteArray& file2, ByteArray& output)
{
	const Byte *control_p,*data_p;
	Byte *dest_p;
	size_t control_l,data_l, total_size,blocksize;
	cvsd_header *h = (cvsd_header*)&file2[0];
	if(h->version!=htons(1))
		return false;
		
	control_p = &file2[0]+sizeof(cvsd_header);
	control_l = ntohl(h->control_length);
	data_p = control_p + control_l;
	data_l = ntohl(h->data_length);
	total_size = ntohl(h->file_size);
	blocksize = ntohl(h->block_size);

	output.resize(total_size);
	dest_p = &output[0];


	while(control_l)
	{
		int region = control_p[0]&0x80;
		size_t pos,len;

		if((control_p[0]&0x60)==0x60)
		{
			assert(control_l>=4);
			len = ((control_p[0]&0x1f)<<24)|(control_p[1]<<16)|(control_p[2]<<8)|control_p[3];
			control_p+=4;
			control_l-=4;
		} else if((control_p[0]&0x60)==0x40)
		{
			assert(control_l>=3);
			len = ((control_p[0]&0x1f)<<16)|(control_p[1]<<8)|control_p[2];
			control_p+=3;
			control_l-=3;
		} else if((control_p[0]&0x60)==0x20)
		{
			assert(control_l>=2);
			len = ((control_p[0]&0x1f)<<8)|control_p[1];
			control_p+=2;
			control_l-=2;
		}
		else
		{
			assert(control_l>=1);
			len = (control_p[0]&0x1f);
			control_p+=1;
			control_l-=1;
		}

		if((control_p[0]&0xC0)==0xC0)
		{
			assert(control_l>=4);
			pos = ((control_p[0]&0x3f)<<24)|(control_p[1]<<16)|(control_p[2]<<8)|control_p[3];
			control_p+=4;
			control_l-=4;
		} else if((control_p[0]&0xC0)==0x80)
		{
			assert(control_l>=3);
			pos = ((control_p[0]&0x3f)<<16)|(control_p[1]<<8)|control_p[2];
			control_p+=3;
			control_l-=3;
		} else if((control_p[0]&0xC0)==0x40)
		{
			assert(control_l>=2);
			pos = ((control_p[0]&0x3f)<<8)|control_p[1];
			control_p+=2;
			control_l-=2;
		}
		else
		{
			assert(control_l>=1);
			pos = (control_p[0]&0x3f);
			control_p+=1;
			control_l-=1;
		}
		
		if(control_l==0 && dest_p+len>&output[0]+total_size)
		{
			/* There can be a slight overflow due to block padding */
			/* This is normal.. fix it here */
			len=(&output[0]+total_size)-dest_p;
		}
		assert(dest_p+len<=&output[0]+total_size);
		if(region)
		{
			assert(data_p+pos+len<=data_p+data_l);
			if(len)
				memcpy(dest_p,data_p+pos,len);
		}
		else
		{	
			assert(pos+len<=file1.size());
			if(len)
				memcpy(dest_p,&file1[0]+pos,len);
		}
		dest_p+=len;
		assert(dest_p<=&output[0]+total_size);
	}
    
	return true;
}
