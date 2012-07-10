/*
	CVSNT Generic API
    Copyright (C) 2004 Tony Hoyle and March-Hare Software Ltd

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
/* _EXPORT */
#ifndef CVS_SMARTPTR__H
#define CVS_SMARTPTR__H

#include <assert.h>

#if 0
// gcc stackdump code.. compile with CFLAGS=-g -rdynamic
#include <execinfo.h>
static void bt()
{
  void *array[32];
  int c = backtrace(array,32);
  backtrace_symbols_fd(array,c,1);
}
#endif

namespace cvs
{
	template<typename _Typ>
	struct sp_delete
	{
		static void dealloc(_Typ* ptr) { delete ptr; }
	};

	template<typename _Typ>
	struct sp_delete_array
	{
		static void dealloc(_Typ* ptr) { delete[] ptr; }
	};

	template<typename _Typ>
	struct sp_free
	{
		static void dealloc(_Typ* ptr) { free(ptr); }
	};

	template<typename _Typ, typename _ArrayType = _Typ, typename _Dealloc = sp_delete<_Typ> >
	class smartptr
	{
	public:
		typedef smartptr<_Typ,_ArrayType,_Dealloc> my_type;

		smartptr() { pObj=NULL; }
		~smartptr() { deref(pObj); };
		smartptr(const my_type& other) { ref((stub_ptr_type)other.pObj); pObj=(stub_ptr_type)other.pObj; }
		smartptr(_Typ* other) { stub_ptr_type stub = alloc_ref(other); pObj=stub; }

		template<typename _tIx>
			_ArrayType operator[](const _tIx _Ix) const { assert(pObj); return pObj->obj->operator[](_Ix); }

		_Typ* Detach() { _Typ* p = pObj->obj; pObj->obj = NULL; deref(pObj); return p; }

		my_type& operator=(const my_type& other) { ref((stub_ptr_type)other.pObj); deref(pObj); pObj=(stub_ptr_type)other.pObj; return *this; }
		my_type& operator=(_Typ* other) { stub_ptr_type stub = alloc_ref(other); deref(pObj); pObj=stub; return *this; }

		_Typ* operator->() const { assert(pObj); if(!pObj) return NULL; return pObj->obj; }
		operator _Typ*() const { if(!pObj) return NULL; return pObj->obj; }
		_Typ* object() const { assert(pObj); if(!pObj) return NULL; return pObj->obj; }

		bool operator==(const my_type& other) const { if(!pObj && !other.pObj) return true; if(!pObj) return false; return pObj->obj==other.pObj; }
		bool operator==(const _Typ* obj) const { if(!pObj && !obj) return true; if(!pObj) return false; return pObj->obj==obj; }

	private:
		template<typename _Ty>
		struct smartptr_stub
		{
			unsigned long count;
			_Ty obj;
		};

		typedef smartptr_stub<_Typ*>* stub_ptr_type;

		stub_ptr_type pObj;

		void deref(stub_ptr_type& stub)
		{
			if(stub && stub->count && !--stub->count)
				dealloc_ref(stub);
			stub=NULL;
		}

		void ref(stub_ptr_type stub)
		{
			if(!stub)
				return;
			++stub->count;
		}

		// TODO: make these less time consuming - prealloc block of memory and use that
		stub_ptr_type alloc_ref(_Typ* obj)
		{
			stub_ptr_type stub = new smartptr_stub<_Typ*>;
			stub->count=1;
			stub->obj = obj;
			return stub;
		}

		void dealloc_ref(stub_ptr_type stub)
		{
			assert(!stub->count);
			if(stub->obj)
				_Dealloc::dealloc(stub->obj);
			delete stub;
		}
	};
};

#endif
