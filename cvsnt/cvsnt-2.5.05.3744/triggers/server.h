/*
	CVSNT Windows Scripting trigger handler
    Copyright (C) 2005 Tony Hoyle and March-Hare Software Ltd

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
#ifndef SERVER__H
#define SERVER__H

#include "server_h.h"

class CServer :
	   public CComObjectRootEx<CComMultiThreadModel>,
	   public CComCoClass<IServer, &CLSID_Server>,
	   public IDispatchImpl<IServer, &IID_IServer, &LIBID_Server>
{
    BEGIN_COM_MAP(CServer)
     COM_INTERFACE_ENTRY(IDispatch)
     COM_INTERFACE_ENTRY(IServer)
    END_COM_MAP()

public:
    STDMETHOD(Trace)(short Level, BSTR Text);
    STDMETHOD(Warning)(BSTR Text);
    STDMETHOD(Error)(BSTR Text);
};

class CChangeInfoStruct :
	   public CComObjectRootEx<CComMultiThreadModel>,
	   public CComCoClass<IChangeInfoStruct, &CLSID_Server>,
	   public IDispatchImpl<IChangeInfoStruct, &IID_IChangeInfoStruct, &LIBID_Server>
{
    BEGIN_COM_MAP(CChangeInfoStruct)
     COM_INTERFACE_ENTRY(IDispatch)
     COM_INTERFACE_ENTRY(IChangeInfoStruct)
    END_COM_MAP()
public:
	virtual BSTR STDMETHODCALLTYPE get_filename() { return SysAllocString(filename); }
	virtual BSTR STDMETHODCALLTYPE get_rev_new() { return SysAllocString(rev_new); }
	virtual BSTR STDMETHODCALLTYPE get_rev_old() { return SysAllocString(rev_old); }
	virtual BSTR STDMETHODCALLTYPE get_type() { return SysAllocString(type); }
	virtual BSTR STDMETHODCALLTYPE get_tag() { return SysAllocString(tag); }
	virtual BSTR STDMETHODCALLTYPE get_bugid() { return SysAllocString(bugid); }

	BSTR filename;
	BSTR rev_new;
	BSTR rev_old;
	BSTR type;
	BSTR tag;
	BSTR bugid;
};

class CItemListStruct :
	   public CComObjectRootEx<CComMultiThreadModel>,
	   public CComCoClass<IItemListStruct, &CLSID_Server>,
	   public IDispatchImpl<IItemListStruct, &IID_IItemListStruct, &LIBID_Server>
{
    BEGIN_COM_MAP(CItemListStruct)
     COM_INTERFACE_ENTRY(IDispatch)
     COM_INTERFACE_ENTRY(IItemListStruct)
    END_COM_MAP()

public:
	virtual BSTR STDMETHODCALLTYPE get_name() { return name.copy(); }
	virtual BSTR STDMETHODCALLTYPE get_value() { return value.copy(); }

	_bstr_t name;
	_bstr_t value;
};

template <class EnumType, class CollType>
	HRESULT CreateSTLEnumerator(IUnknown** ppUnk, IUnknown* pUnkForRelease, CollType& collection)
{
    if (ppUnk == NULL)
        return E_POINTER;
    *ppUnk = NULL;

    CComObject<EnumType>* pEnum = NULL;
    HRESULT hr = CComObject<EnumType>::CreateInstance(&pEnum);

    if (FAILED(hr))
        return hr;

    hr = pEnum->Init(pUnkForRelease, collection);

    if (SUCCEEDED(hr))
        hr = pEnum->QueryInterface(ppUnk);

    if (FAILED(hr))
        delete pEnum;

    return hr;

} // CreateSTLEnumerator
	
typedef CComEnumOnSTL<IEnumVARIANT, &IID_IEnumVARIANT, VARIANT,
                  _Copy<VARIANT>, std::vector<CComVariant> > VarVarEnum;

class ATL_NO_VTABLE CChangeInfoCollection :
	   public CComObjectRootEx<CComMultiThreadModel>,
	   public CComCoClass<IChangeInfoCollection, &CLSID_Server>,
	   public IDispatchImpl<IChangeInfoCollection, &IID_IChangeInfoCollection, &LIBID_Server>
{
    BEGIN_COM_MAP(CChangeInfoCollection)
     COM_INTERFACE_ENTRY(IDispatch)
     COM_INTERFACE_ENTRY(IChangeInfoCollection)
    END_COM_MAP()

public:
   std::vector<CComVariant> m_vec;

   CChangeInfoCollection() { }
   virtual ~CChangeInfoCollection() { }

   STDMETHOD(get__NewEnum)(IUnknown** ppUnk) { return CreateSTLEnumerator<VarVarEnum>(ppUnk, this, m_vec); }
   STDMETHOD(get_Item)(long Index, IDispatch **ppVal) 
       { 
       if(Index<0 || Index>(long)m_vec.size()) 
           return E_INVALIDARG; 
       *ppVal=m_vec[Index].pdispVal;
       (*ppVal)->AddRef ();
       return S_OK; 
       }
   STDMETHOD(get_Count)(long *pVal) { *pVal=m_vec.size(); return S_OK; }
};

class ATL_NO_VTABLE CItemListCollection :
	   public CComObjectRootEx<CComMultiThreadModel>,
	   public CComCoClass<IItemListCollection, &CLSID_Server>,
	   public IDispatchImpl<IItemListCollection, &IID_IItemListCollection, &LIBID_Server>
{
    BEGIN_COM_MAP(CItemListCollection)
     COM_INTERFACE_ENTRY(IDispatch)
     COM_INTERFACE_ENTRY(IItemListCollection)
    END_COM_MAP()

public:
   std::vector<CComVariant> m_vec;

   CItemListCollection() { }
   virtual ~CItemListCollection() { }

   STDMETHOD(get__NewEnum)(IUnknown** ppUnk) { return CreateSTLEnumerator<VarVarEnum>(ppUnk, this, m_vec); }
   STDMETHOD(get_Item)(long Index, IDispatch **ppVal) { if(Index<0 || Index>(long)m_vec.size()) return E_INVALIDARG; *ppVal=m_vec[Index].pdispVal; (*ppVal)->AddRef (); return S_OK; }
   STDMETHOD(get_Count)(long *pVal) { *pVal=m_vec.size(); return S_OK; }
};

#endif