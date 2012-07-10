#ifndef CP_STR_H
#define CP_STR_H

#include "ustr.h"
#define CStr UStr

// class to use when fed-up with realloc, malloc...
template <class T>
class CStaticAllocT
{
public:
	CStaticAllocT()
	{
		buf = 0L;
		numbuf = 0;
	}

	~CStaticAllocT()
	{
		if(buf != 0L)
			free(buf);
	}

	void AdjustSize(size_t size)
	{
		if(numbuf < size)
		{
			if(buf == 0L)
				buf = (T *)malloc(size * sizeof(T));
			else
				buf = (T *)realloc(buf, size * sizeof(T));
			numbuf = size;
		}
	}

	inline operator T *(void) const /* egcs const */ { return buf; }
	inline operator const T *(void) const { return buf; }
#ifdef __GNUC__
	inline const T & operator[](int i) const { return buf[i]; }
	inline T & operator[](int i) { return buf[i]; }
#endif

	inline size_t size() const { return numbuf; }
protected:
	T *buf;
	size_t numbuf;
};

#endif
