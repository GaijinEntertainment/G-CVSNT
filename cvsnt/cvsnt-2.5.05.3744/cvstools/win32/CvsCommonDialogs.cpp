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
/* Win32 specific */
#include <cvsapi.h>
#include "../GlobalSettings.h"
#include "../RootSplitter.h"
#include "../ServerInfo.h"
#include "../ServerConnection.h"
#include "CvsCommonDialogs.h"
#include "resource.h"

#include <commctrl.h>

HMODULE g_hInstance;

BOOL WINAPI DllMain(HMODULE hModule, DWORD uReason, LPVOID lpReserved)
{
	g_hInstance = hModule;
	return TRUE;
}

CBrowserDialog::CBrowserDialog(HWND hParent)
{
	m_hParent = hParent;
	m_hWnd = NULL;
	m_bTagList = m_bTagMode = false;
}

CBrowserDialog::~CBrowserDialog()
{
}

bool CBrowserDialog::ShowDialog(unsigned flags /*= BDShowAll*/, const char *title /*= "Browse for repository"*/, const char *statusbar /*=""*/)
{
	m_flags = flags;
	m_szTitle = title;
	m_szStatus = statusbar;
	m_hLocalRoot=m_hGlobalRoot=m_hFtpRoot=NULL;
	if(DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_BROWSEDIALOG), m_hParent, _dlgProc, (LPARAM)this)==IDOK)
		return true;
	return false;
}

INT_PTR CALLBACK CBrowserDialog::_dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
// GetWindowLongPtr is #defined to GetWindowLong, which produces bogus 64bit
// compatibility warnings.  Disable these for this routine.
#pragma warning(push)
#pragma warning(disable:4312; disable:4244)

	CBrowserDialog* pthis = (CBrowserDialog*)GetWindowLongPtr(hWnd, DWLP_USER);
	if(uMsg == WM_INITDIALOG)
	{
		SetWindowLongPtr(hWnd, DWLP_USER, lParam);
		pthis=(CBrowserDialog*)lParam;
		pthis->m_hWnd = hWnd;
	}
	if(!pthis)
		return FALSE;
	return pthis->dlgProc(hWnd,uMsg,wParam,lParam);

#pragma warning(pop)
}

INT_PTR CBrowserDialog::dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_INITDIALOG:
			return OnInitDialog();
		case WM_SIZE:
			OnSize((int)wParam,(int)LOWORD(lParam),(int)HIWORD(lParam));
			return 0;
		case WM_COMMAND:
			switch(wParam)
			{
			case IDC_NEW:
				OnNew();
				return TRUE;
			case IDOK:
				if(!OnOk())
					break;
			case IDCANCEL:
				EndDialog(hWnd, wParam);
				return TRUE;
			}
			return 0;
		case WM_NOTIFY:
			OnNotify((int)wParam,(LPNMHDR)lParam);
			return 0;
	}
	return 0;
}

BOOL CBrowserDialog::OnInitDialog()
{
	m_hStatus = CreateWindowEx(NULL, STATUSCLASSNAME, cvs::wide(m_szStatus.c_str()),  WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,
							0, 0, 0, 0, m_hWnd, (HMENU)IDC_STATUS, g_hInstance, NULL);

	HIMAGELIST il = ImageList_Create(16,16,ILC_COLOR8|ILC_MASK,5,5);
	ImageList_AddIcon(il, LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_NETHOOD)));
	ImageList_AddIcon(il, LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_SERVER)));
	ImageList_AddIcon(il, LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_FLDRCLOSED)));
	ImageList_AddIcon(il, LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_FLDROPEN)));
	ImageList_AddIcon(il, LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_DOCUMENT)));
	ImageList_AddIcon(il, LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_BRANCH)));
	ImageList_AddIcon(il, LoadIcon(g_hInstance,MAKEINTRESOURCE(IDI_WORLD)));
	SendMessage(GetDlgItem(m_hWnd,IDC_TREE1),TVM_SETIMAGELIST,TVSIL_NORMAL,(LPARAM)il);

	if(m_flags&BDShowLocal)
		PopulateLocal(GetDlgItem(m_hWnd,IDC_TREE1),(m_flags&BDShowStatic)?true:false);
	if(m_flags&BDShowFtp)
		PopulateFtp(GetDlgItem(m_hWnd,IDC_TREE1));
	if(m_flags&BDShowGlobal)
		PopulateGlobal(GetDlgItem(m_hWnd,IDC_TREE1));
	if((m_flags&(BDAllowAdd|BDShowLocal))!=(BDAllowAdd|BDShowLocal))
		EnableWindow(GetDlgItem(m_hWnd,IDC_NEW),FALSE);

	RECT rect;
	GetClientRect(m_hWnd, &rect);
	OnSize(0,rect.right,rect.bottom);

	SetWindowText(m_hWnd,cvs::wide(m_szTitle.c_str()));

	EnableWindow(GetDlgItem(m_hWnd,IDOK),FALSE);

	return TRUE;
}

bool CBrowserDialog::OnOk()
{
	if(!m_activedir || m_activedir->invalid || m_activedir->level<1)
		return false;

	if(m_activedir->level==1 && !m_activedir->root.size())
	{
		CServerConnection conn;
		if(!conn.Connect("--utf8 rls -qe", m_activedir, this))
			return false;
	}

	if(!m_activedir->root.size())
		return false;

	m_root = m_activedir->root;
	m_module = m_activedir->module;
	m_description = m_activedir->srvname;
	m_tag = m_activedir->tag;

	return true;
}

void CBrowserDialog::OnNew()
{
	CBrowserRootDialog dlg(m_hWnd);
	ServerConnectionInfo *srv = new ServerConnectionInfo;
	srv->level=0;
	srv->user = true;
	if(!dlg.ShowDialog(srv, "User defined root", (m_flags&BDShowFtp)&~BDShowModule))
	{
		delete srv;
		return;
	}

	HWND hTree = GetDlgItem(m_hWnd,IDC_TREE1);
	if(strcmp(srv->protocol.c_str(),"ftp"))
	{
		srv->level=2;
		HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(srv->srvname.c_str()),1,1,srv,m_hLocalRoot);
		InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
	}
	else
	{
		HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(srv->srvname.c_str()),1,1,srv,m_hFtpRoot);
	}

	cvs::string key,rt;
	if(srv->root.size())
		rt = srv->root+"*"+srv->module;
	else
		cvs::sprintf(rt,80,"%s:%s",srv->server.c_str(),srv->port.c_str());
	cvs::sprintf(key,80,"Serv_%s",srv->srvname.c_str());
	CGlobalSettings::SetUserValue("WorkspaceManager","Servers",key.c_str(),rt.c_str());
}

void CBrowserDialog::OnSize(int nType, int x, int y)
{
	MoveWindow(m_hStatus,0,0,0,0,TRUE); // Status bar will self-size anyway

	RECT rect;
	int newx,newy;
	GetWindowRect(GetDlgItem(m_hWnd, IDOK),&rect);
	OffsetRect(&rect,-rect.left,-rect.top);
	newx=x/2-((rect.right*3)/2+2);
	newy=y-(rect.bottom+22);
	MoveWindow(GetDlgItem(m_hWnd, IDC_NEW),newx,newy,rect.right,rect.bottom,TRUE);
	MoveWindow(GetDlgItem(m_hWnd, IDOK),newx+rect.right+2,newy,rect.right,rect.bottom,TRUE);
	MoveWindow(GetDlgItem(m_hWnd, IDCANCEL),newx+rect.right*2+4,newy,rect.right,rect.bottom,TRUE);
	MoveWindow(GetDlgItem(m_hWnd, IDC_TREE1), 5, 5, x-10, newy-10, TRUE);
}

void CBrowserDialog::OnNotify(int nId, LPNMHDR nmhdr)
{
	LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)nmhdr;

	if(nId!=IDC_TREE1)
		return;

	switch(nmhdr->code)
	{
		case TVN_DELETEITEM:
			delete (ServerConnectionInfo*)pnmtv->itemOld.lParam;
			return;
		case TVN_ITEMEXPANDING:
			OnItemExpanding(GetDlgItem(m_hWnd, nId), pnmtv);
			return;
		case TVN_SELCHANGED:
			OnSelChanged(GetDlgItem(m_hWnd, nId), pnmtv);
			return;
		case NM_RCLICK:
			OnRClick(GetDlgItem(m_hWnd, nId), nmhdr);
			return;
		default:
			return;
	};
}

void CBrowserDialog::OnItemExpanding(HWND hTree, LPNMTREEVIEW pnmtv)
{
	if(pnmtv->action&TVE_COLLAPSE)
	{
		if(!(pnmtv->action&TVE_COLLAPSERESET) && pnmtv->itemNew.lParam)
			TreeView_Expand(hTree,pnmtv->itemNew.hItem,TVE_COLLAPSE|TVE_COLLAPSERESET);
		return;
	}

	UINT nMask = pnmtv->itemNew.state&TVIS_EXPANDEDONCE;
	if (nMask & TVIS_EXPANDEDONCE)
		return;

	HTREEITEM hParent = pnmtv->itemNew.hItem;
	ServerConnectionInfo *dir = (ServerConnectionInfo *)pnmtv->itemNew.lParam;

	if(!dir || !hParent || dir->level==-2)
		return;

	HTREEITEM hItem = TreeView_GetChild(hTree,hParent);
	while(hItem)
	{
		HTREEITEM hNextItem = TreeView_GetNextSibling(hTree,hItem);
		TreeView_DeleteItem(hTree,hItem);
		hItem = hNextItem;
	}

	if(dir->level == -1)
	{
		CDnsApi dns;
		std::map<cvs::string,int> dupMap;

		if(dns.Lookup(dir->module.c_str(), DNS_TYPE_PTR))
		{
			do
			{
				CDnsApi srvDns;
				const char *srvName = dns.GetRRPtr();
				const char *srvTxt = NULL, *srvPtr = NULL;
				CDnsApi::SrvRR *rr = NULL;

				if(!srvName || dupMap[srvName]++)
					continue;

				if(srvName && strchr(srvName,'.') && srvDns.Lookup(srvName,DNS_TYPE_ANY))
				{
					do
					{
						if(!srvTxt)
							srvTxt = srvDns.GetRRTxt();

						if(!rr)
							rr=srvDns.GetRRSrv();

						srvPtr = srvDns.GetRRPtr();

						if(srvPtr || (srvTxt && rr)) break;
					} while(srvDns.Next());

					if(srvPtr)
					{
						ServerConnectionInfo *srv = new ServerConnectionInfo;
						srv->level=-1;
						srv->user=false;

						srv->module = srvPtr;

						cvs::string title = srvName;
						title.resize(title.find_first_of('.'));
						srv->srvname = title;
						HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(title.c_str()),2,3,srv,hParent);
						InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
					}
					else
					{
						if(rr)
						{
							ServerConnectionInfo *srv = new ServerConnectionInfo;
							srv->level=0;
							srv->user=false;

							srv->server = rr->server;
							cvs::sprintf(srv->port,32,"%u",rr->port);

							cvs::string title = srvName;
							title.resize(title.find_first_of('.'));
							srv->srvname = title;
							HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(title.c_str()),1,1,srv,hParent);
							InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);

							if(srvTxt)
							{
								srv->level=1;
								srv->enumerated=false;
								srv->anonymous=false;
								srv->module="";
								srv->root = srvTxt;
							}
						}
					}
				}
			} while(dns.Next());
		}
		TreeView_SortChildren(hTree,hParent,FALSE);
		return;
	}

	if(!dir->level)
	{
		// Server level.  Initialise structure and get repository details.
		CServerInfo si;
		CServerInfo::remoteServerInfo rsi;
		cvs::string tempServ = dir->server + ":" + dir->port;

        if(dir->root.size() || !si.getRemoteServerInfo(tempServ.c_str(),rsi))
		{
			if(!dir->root.size())
				return;
			ServerConnectionInfo *newdir = new ServerConnectionInfo;
			*newdir = *dir;
			newdir->level = 1;
			newdir->user=false;
			newdir->enumerated=false;
			newdir->anonymous=false;
			cvs::string tmp = dir->directory;
			if(newdir->module.size())
			{
				tmp+=" (";
				tmp+=newdir->module;
				tmp+=")";
			}
			hItem=InsertTreeItem(hTree,cvs::wide(tmp.c_str()),2,3,newdir,hParent);
			InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem); 
		}
		else
		{
			bool first = true;
			for(CServerInfo::remoteServerInfo::repositories_t::const_iterator i = rsi.repositories.begin(); i!=rsi.repositories.end(); ++i)
			{
				ServerConnectionInfo *newdir = new ServerConnectionInfo;
				*newdir = *dir;
				newdir->level = 1;
				newdir->user=false;
				newdir->anonymous=false;
				newdir->enumerated=true;
				newdir->module="";
				newdir->anon_user = rsi.anon_username;
				newdir->anon_proto = rsi.anon_protocol;
				newdir->default_proto = rsi.default_protocol;
				newdir->directory = i->first;

				hItem=InsertTreeItem(hTree,cvs::wide(i->second.c_str()),2,3,newdir,hParent);
				InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
			}
		}
		return;
	}
	else if(dir->level>=1 && (dir->level==1 || m_flags&BDShowDirectories))
	{
		cvs::string tmp;

		if(dir->module.size() && !dir->tag.size() && (m_flags&(BDShowTags|BDShowBranches)))
		{
			m_tagList.clear();
			if(dir->module.size() && !dir->tag.size())
			{
				CServerConnection conn;
				cvs::sprintf(tmp,80,"--utf8 -d \"%s\" rlog -lh \"%s\"",dir->root.c_str(),dir->module.c_str());
				m_bTagList = true;
				m_bTagMode = false;
				if(conn.Connect(tmp.c_str(), dir, this))
				{
					for(std::map<cvs::string,int>::const_iterator i = m_tagList.begin(); i!=m_tagList.end(); ++i)
					{
						ServerConnectionInfo *tagdir = new ServerConnectionInfo;
						*tagdir = *dir;
						tagdir->tag = i->first;
						hItem=InsertTreeItem(hTree,cvs::wide(i->first.c_str()),5,5,tagdir,hParent);
						if(m_flags&BDShowDirectories)
							InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
					}
				}
				m_bTagList = false;
			}
		}

		CServerConnection conn;

		m_dirList.clear();

		if(dir->module.size())
			cvs::sprintf(tmp,80,"--utf8 rls -qe \"%s\"",dir->module.c_str());
		else
			tmp="--utf8 rls -qe";
		if(!conn.Connect(tmp.c_str(), dir, this))
			return;

		for(size_t n=0; n<m_dirList.size(); n++)
		{
			const char *p=m_dirList[n].c_str(),*q;
			bool isfile = false;
			if(*p!='D')
				isfile = true;
			else
				p++;

			if(isfile)
				continue;

			if(*(p++)!='/')
				continue;
			q=strchr(p,'/');
			if(!q)
				continue;
			cvs::filename fn;
			fn.assign(p,q-p);

			ServerConnectionInfo *newdir = new ServerConnectionInfo;
			*newdir = *dir;
			newdir->level++;
			if(newdir->module.size())
				newdir->module+="/";
			newdir->module += fn.c_str();
			newdir->isfile = isfile;

			hItem=InsertTreeItem(hTree,cvs::wide(fn.c_str()),isfile?4:2,isfile?4:3,newdir,hParent);
			if(!isfile && (m_flags&BDShowDirectories))
				InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
		}
	}
}

void CBrowserDialog::OnSelChanged(HWND hTree, LPNMTREEVIEW pnmtv)
{
	m_activedir = (ServerConnectionInfo*)pnmtv->itemNew.lParam;

	EnableWindow(GetDlgItem(m_hWnd, IDOK), (m_activedir && !m_activedir->invalid && m_activedir->level>0 && (!(m_flags&BDRequireModule) || m_activedir->module.size()))?TRUE:FALSE);
}

void CBrowserDialog::OnRClick(HWND hTree, LPNMHDR pnmhdr)
{
	DWORD pos = GetMessagePos();
	POINTS p(MAKEPOINTS(pos));
	TVHITTESTINFO ht = { {p.x,p.y} };
	TVITEM tv = { TVIF_PARAM };

	ScreenToClient(hTree,&ht.pt);
	HTREEITEM hItem = (HTREEITEM)SendMessage(hTree,TVM_HITTEST,0,(LPARAM)&ht);
	if(!hItem || !(ht.flags&TVHT_ONITEM))
		return;

	tv.hItem = hItem;

	if(!SendMessage(hTree,TVM_GETITEM,0,(LPARAM)&tv))
		return;

	ServerConnectionInfo *dir = (ServerConnectionInfo*)tv.lParam;
	if(!dir || !dir->user)
		return;

	HMENU hMenu = CreatePopupMenu();
	AppendMenu(hMenu,MF_STRING,100,_T("&Delete"));
	AppendMenu(hMenu,MF_STRING,101,_T("&Properties"));

	switch(TrackPopupMenu(hMenu,TPM_NONOTIFY|TPM_RETURNCMD|TPM_LEFTALIGN,p.x,p.y,0,m_hWnd,NULL))
	{
		case 100: /* Delete */
			{
				cvs::string tmp;
				cvs::sprintf(tmp,80,"Serv_%s",dir->srvname.c_str());

				CGlobalSettings::DeleteUserValue("WorkspaceManager","Servers",tmp.c_str());
				SendMessage(hTree,TVM_DELETEITEM,0,(LPARAM)hItem);
			}
			break;
		case 101: /* Properties */
			{
				CBrowserRootDialog dlg(m_hWnd);
				if(dlg.ShowDialog(dir,"Properties",(m_flags&BDShowFtp)&~BDShowModule))
				{
					cvs::string key,rt;
					if(dir->root.size())
						rt = dir->root+"*"+dir->module;
					else
						cvs::sprintf(rt,80,"%s:%s",dir->server.c_str(),dir->port.c_str());
					cvs::sprintf(key,80,"Serv_%s",dir->srvname.c_str());
					CGlobalSettings::SetUserValue("WorkspaceManager","Servers",key.c_str(),rt.c_str());
				}
			}
			break;
	}
	DestroyMenu(hMenu);
}

HTREEITEM CBrowserDialog::InsertTreeItem(HWND hTree, LPCTSTR szText, int nIcon, int nStateIcon, LPVOID param /* = NULL */, HTREEITEM hParentItem /*= TVI_ROOT*/)
{
	TVINSERTSTRUCT tvis = { hParentItem, TVI_LAST };
	tvis.item.mask=TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_TEXT|TVIF_PARAM;
	tvis.item.pszText=(LPTSTR)szText;
	tvis.item.iImage=nIcon;
	tvis.item.iSelectedImage=nStateIcon;
	tvis.item.lParam=(LPARAM)param;
	return (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tvis);
}


void CBrowserDialog::PopulateLocal(HWND hTree, bool bStatic)
{
	HTREEITEM hRoot = InsertTreeItem(hTree,_T("Local Servers"),0,0);

	m_hLocalRoot = hRoot;

	const char *pResp = NULL;
	char szResp[64];

	// New responder string
  	if(!CGlobalSettings::GetGlobalValue("cvsnt","PServer","ResponderName",szResp,sizeof(szResp)))
	{
		pResp = szResp;
		if(!strcmp(pResp,"none"))
			return;
		if(!strcmp(pResp,"default"))
			pResp = NULL;
	}

	CZeroconf zc(pResp, CGlobalSettings::GetLibraryDirectory(CGlobalSettings::GLDMdns));
	const CZeroconf::server_struct_t *serv;
	zc.BrowseForService("_cvspserver._tcp",CZeroconf::zcAddress);
	bool first = true;
	while((serv=zc.EnumServers(first))!=NULL)
	{
		ServerConnectionInfo *srv = new ServerConnectionInfo;
		srv->level=0;
		srv->user=false;

		srv->server = serv->server;
		srv->server.resize(srv->server.find_first_of('.'));
		cvs::sprintf(srv->port,32,"%u",serv->port);
		srv->srvname = serv->servicename;
		HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(serv->servicename.c_str()),1,1,srv,hRoot);
		InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
	}

	if(bStatic)
	{
		int n =0;
		char key[256];
		char val[256];
		while(!CGlobalSettings::EnumUserValues("WorkspaceManager","Servers",n++,key,sizeof(key),val,sizeof(val)))
		{
			CRootSplitter split;

			if(strncmp(key,"Serv_",5))
				continue;
			if(val[0]!=':' && !strchr(val,'/'))
			{
				char *p=strchr(val,':');
				if(p) *p='\0';
				split.m_server = val;
				if(p) split.m_port = p+1;
				else split.m_port="";
			}
			else if(!split.Split(val))
				continue;

			if(!strcmp(split.m_protocol.c_str(),"ftp"))
				continue;

			ServerConnectionInfo *srv = new ServerConnectionInfo;
			srv->level=0;
			srv->user = true;
			srv->root = split.m_root;
			srv->protocol = split.m_protocol;
			srv->keywords = split.m_keywords;
			srv->username = split.m_username;
			srv->password = split.m_password;
			srv->server = split.m_server;
			srv->port = split.m_port;
			srv->directory = split.m_directory;
			srv->module = split.m_module;
			srv->srvname = key+5;
			HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(key+5),1,1,srv,hRoot);
			InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
		}
	}

	SendMessage(GetDlgItem(m_hWnd,IDC_TREE1),TVM_SORTCHILDREN,0,(LPARAM)hRoot);
	SendMessage(GetDlgItem(m_hWnd,IDC_TREE1),TVM_EXPAND,TVE_EXPAND,(LPARAM)hRoot);
}

void CBrowserDialog::PopulateFtp(HWND hTree)
{
	HTREEITEM hRoot = InsertTreeItem(hTree,_T("FTP Servers"),0,0);

	m_hFtpRoot = hRoot;

	int n =0;
	char key[256];
	char val[256];
	while(!CGlobalSettings::EnumUserValues("WorkspaceManager","Servers",n++,key,sizeof(key),val,sizeof(val)))
	{
		CRootSplitter split;

		if(strncmp(key,"Serv_",5))
			continue;
		if(val[0]!=':' && !strchr(val,'/'))
		{
			char *p=strchr(val,':');
			if(p) *p='\0';
			split.m_server = val;
//			if(p) split.m_port = p+1;
			/*else */split.m_port="";
		}
		else if(!split.Split(val))
			continue;

		if(strcmp(split.m_protocol.c_str(),"ftp"))
			continue;

		ServerConnectionInfo *srv = new ServerConnectionInfo;
		srv->level=2;
		srv->user = true;
		srv->root = split.m_root;
		srv->protocol = split.m_protocol;
		srv->keywords = split.m_keywords;
		srv->username = split.m_username;
		srv->password = split.m_password;
		srv->server = split.m_server;
		srv->port = split.m_port;
		srv->directory = split.m_directory;
		srv->module = split.m_module;
		srv->srvname = key+5;
		HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(key+5),1,1,srv,hRoot);
	}

	SendMessage(GetDlgItem(m_hWnd,IDC_TREE1),TVM_SORTCHILDREN,0,(LPARAM)hRoot);
	SendMessage(GetDlgItem(m_hWnd,IDC_TREE1),TVM_EXPAND,TVE_EXPAND,(LPARAM)hRoot);
}

void CBrowserDialog::PopulateGlobal(HWND hTree)
{
	ServerConnectionInfo *srv = new ServerConnectionInfo;
	srv->level = -2;
	
	HTREEITEM hRoot = InsertTreeItem(hTree,_T("Global Directory"),6,6,srv);

	m_hGlobalRoot = hRoot;

	CDnsApi dns;
	std::map<cvs::string,int> dupMap;

	if(dns.Lookup("_cvspserver._tcp.cvsnt.org", DNS_TYPE_PTR))
	{
		do
		{
			CDnsApi srvDns;
			const char *srvName = dns.GetRRPtr();
			const char *srvTxt = NULL, *srvPtr = NULL;
			CDnsApi::SrvRR *rr = NULL;

			if(!srvName || dupMap[srvName]++)
				continue;

			if(srvName && strchr(srvName,'.') && srvDns.Lookup(srvName,DNS_TYPE_ANY))
			{
				do
				{
					if(!srvTxt)
						srvTxt = srvDns.GetRRTxt();

					if(!rr)
						rr=srvDns.GetRRSrv();

					srvPtr = srvDns.GetRRPtr();

					if(srvPtr || (srvTxt && rr)) break;
				} while(srvDns.Next());

				if(srvPtr)
				{
					ServerConnectionInfo *srv = new ServerConnectionInfo;
					srv->level=-1;
					srv->user=false;

					srv->module = srvName;

					cvs::string title = srvName;
					title.resize(title.find_first_of('.'));
					srv->srvname = title;
					HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(title.c_str()),2,3,srv,hRoot);
					InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);
				}
				else
				{
					if(rr)
					{
						ServerConnectionInfo *srv = new ServerConnectionInfo;
						srv->level=0;
						srv->user=false;

						srv->server = rr->server;
						cvs::sprintf(srv->port,32,"%u",rr->port);

						cvs::string title = srvName;
						title.resize(title.find_first_of('.'));
						srv->srvname = title;
						HTREEITEM hItem=InsertTreeItem(hTree,cvs::wide(title.c_str()),1,1,srv,hRoot);
						InsertTreeItem(hTree,_T("__@@__"),0,0,NULL,hItem);

						if(srvTxt)
						{
							srv->level=1;
							srv->enumerated=false;
							srv->anonymous=false;
							srv->module="";
							srv->root = srvTxt;
						}
					}
				}
			}
		} while(dns.Next());
	}
	SendMessage(GetDlgItem(m_hWnd,IDC_TREE1),TVM_SORTCHILDREN,0,(LPARAM)hRoot);
	SendMessage(GetDlgItem(m_hWnd,IDC_TREE1),TVM_EXPAND,TVE_EXPAND,(LPARAM)hRoot);
}

void CBrowserDialog::ProcessOutput(const char *line)
{
	if(!m_bTagList) // Standard directory list
		m_dirList.push_back(line);
	else
	{
		if(!strcmp(line,"symbolic names:"))
			m_bTagMode=true;
		else if(!strncmp(line,"keyword substitution:",21))
			m_bTagMode=false;
		else if(m_bTagMode)
		{
			const char *p = strchr(line,':');
			if(p)
			{
				cvs::string str,tag=p+2;
				str.assign(line,p-line);
				if(strstr(tag.c_str(),".0."))
				{
					if(m_flags&BDShowBranches)
						m_tagList[str]++;
				}
				else
				{
					if(m_flags&BDShowTags)
						m_tagList[str]++;
				}
			}
		}
	}
}

void CBrowserDialog::Error(ServerConnectionInfo *current, ServerConnectionError error)
{
	const TCHAR *message;
	switch(error)
	{
		case SCESuccessful:
			return;
		case SCEFailedNoAnonymous: message = _T("Logon failed an no anonymous user defined"); break;
		case SCEFailedBadExec: message = _T("Failed to execute cvs"); break;
		case SCEFailedConnection: message = _T("Connection to server failed"); break;
		case SCEFailedBadProtocol: message = _T("Connection to server failed due to bad protocol"); break;
		case SCEFailedNoSupport: message = _T("Server does not support remote repository listing"); break;
		case SCEFailedCommandAborted: message = _T("Command execution failed"); break;
		default: message = _T("Connection failed"); break;
	}
	MessageBox(m_hWnd, message, cvs::wide(m_szTitle.c_str()), MB_ICONSTOP|MB_OK);
}

bool CBrowserDialog::AskForPassword(ServerConnectionInfo *dir)
{
	CBrowserPasswordDialog dlg(m_hWnd);
	return dlg.ShowDialog(dir);
}

/*********************************/

CBrowserPasswordDialog::CBrowserPasswordDialog(HWND hParent)
{
	m_hParent = hParent;
	m_hWnd = NULL;
}

CBrowserPasswordDialog::~CBrowserPasswordDialog()
{
}

bool CBrowserPasswordDialog::ShowDialog(ServerConnectionInfo *info)
{
	m_activedir = info;
	return DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_PASSWORD), m_hParent, _dlgProc, (LPARAM)this)==IDOK?true:false;
}

INT_PTR CALLBACK CBrowserPasswordDialog::_dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
// GetWindowLongPtr is #defined to GetWindowLong, which produces bogus 64bit
// compatibility warnings.  Disable these for this routine.
#pragma warning(push)
#pragma warning(disable:4312; disable:4244)

	CBrowserPasswordDialog* pthis = (CBrowserPasswordDialog*)GetWindowLongPtr(hWnd, DWLP_USER);
	if(uMsg == WM_INITDIALOG)
	{
		SetWindowLongPtr(hWnd, DWLP_USER, lParam);
		pthis=(CBrowserPasswordDialog*)lParam;
		pthis->m_hWnd = hWnd;
	}
	if(!pthis)
		return FALSE;
	return pthis->dlgProc(hWnd,uMsg,wParam,lParam);

#pragma warning(pop)
}

INT_PTR CBrowserPasswordDialog::dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR tmp[256];

	switch(uMsg)
	{
		case WM_INITDIALOG:
			SetDlgItemText(hWnd,IDC_ROOT,cvs::wide(m_activedir->root.c_str()));
			SetDlgItemText(hWnd,IDC_USERNAME,cvs::wide(m_activedir->username.c_str()));
			SetDlgItemText(hWnd,IDC_PASSWORD,cvs::wide(m_activedir->password.c_str()));
			SetDlgItemText(hWnd,IDC_KEYWORDS,cvs::wide(m_activedir->keywords.c_str()));
			if(m_activedir->anon_proto.size() && m_activedir->anon_user.size())
			{
				EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),TRUE);
			}
			else
			{
				EnableWindow(GetDlgItem(hWnd,IDC_CHECK1),FALSE);
			}
			{
				RECT rect;
				GetWindowRect(GetDlgItem(hWnd,IDC_PROTOCOL),&rect);
				MapWindowPoints(NULL, hWnd, (LPPOINT)&rect, 2);
				m_splitPoint = rect.top;
				GetWindowRect(GetDlgItem(m_hWnd,IDOK),&rect);
				MapWindowPoints(NULL, hWnd, (LPPOINT)&rect, 2);
				m_growPoint = rect.top;
			}
			SendDlgItemMessage(m_hWnd,IDC_PROTOCOL, CB_ADDSTRING, 0, (LPARAM)(LPCWSTR)cvs::wide(m_activedir->protocol.c_str()));
			SendDlgItemMessage(m_hWnd,IDC_PROTOCOL, CB_SETCURSEL, 0, 0);
			Collapse();
			m_bcbFull=false;
			return 0;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
			case IDOK:
				m_activedir->anonymous = SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0)?true:false;
				if(m_activedir->anonymous)
				{
					m_activedir->username = m_activedir->anon_user;
					m_activedir->password = "";
					m_activedir->protocol = m_activedir->anon_proto;
					m_activedir->keywords = "";
				}
				else
				{
					GetDlgItemText(hWnd,IDC_PASSWORD,tmp,sizeof(tmp));
					if(_tcschr(tmp,'@') || _tcschr(tmp,':') || _tcschr(tmp,'"'))
					{
						MessageBox(hWnd,_T("Password contains invalid characters."),_T("Password Required"),MB_ICONSTOP|MB_OK);
						return 0;
					}
					m_activedir->password = cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_USERNAME,tmp,sizeof(tmp));
					m_activedir->username = cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_KEYWORDS,tmp,sizeof(tmp));
					m_activedir->keywords = cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_PROTOCOL,tmp,sizeof(tmp));
					m_activedir->protocol = cvs::narrow(tmp);
				}
			case IDCANCEL:
				EndDialog(hWnd, wParam);
				return TRUE;
			case IDC_CHECK1:
				switch(HIWORD(wParam))
				{
				case BN_CLICKED:
						if(SendDlgItemMessage(hWnd,IDC_CHECK1,BM_GETCHECK,0,0))
						{
							if(!m_bCollapsed)
								Collapse();
							EnableWindow(GetDlgItem(hWnd,IDC_USERNAME),FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_PASSWORD),FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_OPTIONS),FALSE);
						}
						else
						{
							EnableWindow(GetDlgItem(hWnd,IDC_USERNAME),TRUE);
							EnableWindow(GetDlgItem(hWnd,IDC_PASSWORD),TRUE);
							EnableWindow(GetDlgItem(hWnd,IDC_OPTIONS),TRUE);
						}
						return TRUE;
				}
				break;
			case IDC_PROTOCOL:
				switch(HIWORD(wParam))
				{
					case CBN_DROPDOWN:
						if(!m_bcbFull)
							GetProtocols(GetDlgItem(hWnd,IDC_PROTOCOL));
						m_bcbFull=true;
						return TRUE;
				}
				break;
			case IDC_OPTIONS:
				if(m_bCollapsed)
					Expand();
				else
					Collapse();
				return TRUE;
			}
			return 0;
	}
	return 0;
}

void CBrowserPasswordDialog::Collapse()
{
	RECT rect;
	int rectBtm;

	ShowWindow(GetDlgItem(m_hWnd,IDC_PROTOCOL),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PROTOCOL_TEXT),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS_TEXT),SW_HIDE);

	GetWindowRect(GetDlgItem(m_hWnd,IDOK),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_splitPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDOK),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	MapWindowPoints(m_hWnd, NULL, (LPPOINT)&rect, 2);
	rectBtm = rect.bottom + 10;

	GetWindowRect(GetDlgItem(m_hWnd,IDCANCEL),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_splitPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDCANCEL),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	GetWindowRect(GetDlgItem(m_hWnd,IDC_OPTIONS),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_splitPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDC_OPTIONS),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

    SetDlgItemText(m_hWnd,IDC_OPTIONS,_T("&Options >>"));
	m_bCollapsed = true;

	GetWindowRect(m_hWnd, &rect);
	MoveWindow(m_hWnd, rect.left, rect.top, rect.right - rect.left, rectBtm - rect.top, TRUE);
}

void CBrowserPasswordDialog::Expand()
{
	RECT rect;
	int rectBtm;

	GetWindowRect(GetDlgItem(m_hWnd,IDOK),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_growPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDOK),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	MapWindowPoints(m_hWnd, NULL, (LPPOINT)&rect, 2);
	rectBtm = rect.bottom + 10;

	GetWindowRect(GetDlgItem(m_hWnd,IDCANCEL),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_growPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDCANCEL),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	GetWindowRect(GetDlgItem(m_hWnd,IDC_OPTIONS),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_growPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDC_OPTIONS),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	ShowWindow(GetDlgItem(m_hWnd,IDC_PROTOCOL),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PROTOCOL_TEXT),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS_TEXT),SW_SHOW);

	SetDlgItemText(m_hWnd,IDC_OPTIONS,_T("&Options <<"));
	m_bCollapsed = false;

	GetWindowRect(m_hWnd, &rect);
	MoveWindow(m_hWnd, rect.left, rect.top, rect.right - rect.left, rectBtm - rect.top, TRUE);
}

void CBrowserPasswordDialog::GetProtocols(HWND hCombo)
{
	CServerInfo inf;
	CServerInfo::remoteServerInfo rsi;

	SendMessage(hCombo,CB_RESETCONTENT,0,0);
	if(!inf.getRemoteServerInfo(m_activedir->server.c_str(),rsi) || !rsi.protocols.size())
	{
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("pserver"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("sserver"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("sspi"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ssh"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("gserver"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ext"));
	}
	else
	{
		for(CServerInfo::remoteServerInfo::protocols_t::const_iterator i = rsi.protocols.begin(); i!=rsi.protocols.end(); ++i)
			SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)(LPCWSTR)cvs::wide(i->first.c_str()));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ssh"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ext"));
	}
	SendMessage(hCombo,CB_SETCURSEL,SendMessage(hCombo,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)(LPCWSTR)cvs::wide(m_activedir->protocol.c_str())),0);
}

/*********************************/

CBrowserRootDialog::CBrowserRootDialog(HWND hParent)
{
	m_hParent = hParent;
	m_hWnd = NULL;
}

CBrowserRootDialog::~CBrowserRootDialog()
{
}

bool CBrowserRootDialog::ShowDialog(ServerConnectionInfo *info, const char *title /*= "User defined root"*/, unsigned flags /*= 0*/)
{
	m_activedir = info;
	m_szTitle = title;
	m_flags = flags;
	return DialogBoxParam(g_hInstance, MAKEINTRESOURCE(IDD_NEWROOT), m_hParent, _dlgProc, (LPARAM)this)==IDOK?true:false;
}

INT_PTR CALLBACK CBrowserRootDialog::_dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
// GetWindowLongPtr is #defined to GetWindowLong, which produces bogus 64bit
// compatibility warnings.  Disable these for this routine.
#pragma warning(push)
#pragma warning(disable:4312; disable:4244)

	CBrowserRootDialog* pthis = (CBrowserRootDialog*)GetWindowLongPtr(hWnd, DWLP_USER);
	if(uMsg == WM_INITDIALOG)
	{
		SetWindowLongPtr(hWnd, DWLP_USER, lParam);
		pthis=(CBrowserRootDialog*)lParam;
		pthis->m_hWnd = hWnd;
	}
	if(!pthis)
		return FALSE;
	return pthis->dlgProc(hWnd,uMsg,wParam,lParam);

#pragma warning(pop)
}

INT_PTR CBrowserRootDialog::dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR tmp[256];

	switch(uMsg)
	{
		case WM_INITDIALOG:
			if(m_flags&BRDRequireModule)
				m_flags|=BRDShowModule;
			SetWindowText(hWnd,cvs::wide(m_szTitle.c_str()));
			SetDlgItemText(hWnd,IDC_TITLE,cvs::wide(m_activedir->srvname.c_str()));
			SetDlgItemText(hWnd,IDC_SERVER,cvs::wide(m_activedir->server.c_str()));
			SetDlgItemText(hWnd,IDC_USERNAME,cvs::wide(m_activedir->username.c_str()));
			SetDlgItemText(hWnd,IDC_PASSWORD,cvs::wide(m_activedir->password.c_str()));
			SetDlgItemText(hWnd,IDC_DIRECTORY,cvs::wide(m_activedir->directory.c_str()));
			if(m_flags&BRDShowModule)
				SetDlgItemText(hWnd,IDC_MODULE,cvs::wide(m_activedir->module.c_str()));
			SetDlgItemText(hWnd,IDC_KEYWORDS,cvs::wide(m_activedir->keywords.c_str()));
			SetDlgItemText(hWnd,IDC_PORT,cvs::wide(m_activedir->port.c_str()));
			{
				RECT rect;
				GetWindowRect(GetDlgItem(hWnd,IDC_DIRECTORY),&rect);
				MapWindowPoints(NULL, hWnd, (LPPOINT)&rect, 2);
				m_splitPoint = rect.top;
				GetWindowRect(GetDlgItem(m_hWnd,IDOK),&rect);
				MapWindowPoints(NULL, hWnd, (LPPOINT)&rect, 2);
				m_growPoint = rect.top;
			}
			if(!m_activedir->protocol.size())
				SendDlgItemMessage(m_hWnd,IDC_PROTOCOL, CB_ADDSTRING, 0, (LPARAM)_T("automatic"));
			else
				SendDlgItemMessage(m_hWnd,IDC_PROTOCOL, CB_ADDSTRING, 0, (LPARAM)(LPCWSTR)cvs::wide(m_activedir->protocol.c_str()));
			SendDlgItemMessage(m_hWnd,IDC_PROTOCOL, CB_SETCURSEL, 0, 0);
			if(!m_activedir->directory.size())
				Collapse();
			else
				Expand();
			if(!strcmp(m_activedir->protocol.c_str(),"ftp"))
			{
				SetDlgItemText(hWnd,IDC_KEYWORDS,_T(""));
				SetDlgItemText(hWnd,IDC_PORT,_T(""));
				SetDlgItemText(hWnd,IDC_MODULE,_T(""));
				EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS_TEXT),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_PORT),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_PORT_TEXT),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_MODULE),FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_MODULE_TEXT),FALSE);
				SetDlgItemText(hWnd,IDC_DIRECTORY_TEXT,_T("Directory"));
			}
			else
			{
				EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS),TRUE);
				EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS_TEXT),TRUE);
				if(!strcmp(m_activedir->protocol.c_str(),"ext"))
				{
					EnableWindow(GetDlgItem(hWnd,IDC_PORT),FALSE);
					EnableWindow(GetDlgItem(hWnd,IDC_PORT_TEXT),FALSE);
				}
				else
				{
					EnableWindow(GetDlgItem(hWnd,IDC_PORT),TRUE);
					EnableWindow(GetDlgItem(hWnd,IDC_PORT_TEXT),TRUE);
				}
				EnableWindow(GetDlgItem(hWnd,IDC_MODULE),m_flags&BRDShowModule?TRUE:FALSE);
				EnableWindow(GetDlgItem(hWnd,IDC_MODULE_TEXT),m_flags&BRDShowModule?TRUE:FALSE);
				SetDlgItemText(hWnd,IDC_DIRECTORY_TEXT,_T("Repository"));
			}
			return 0;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
			case IDOK:
				GetDlgItemText(hWnd,IDC_TITLE,tmp,sizeof(tmp));
				m_activedir->srvname = cvs::narrow(tmp);
				GetDlgItemText(hWnd,IDC_SERVER,tmp,sizeof(tmp));
				m_activedir->server = cvs::narrow(tmp);
				if(strcmp(m_activedir->protocol.c_str(),"ext"))
				{
					GetDlgItemText(hWnd,IDC_PORT,tmp,sizeof(tmp));
					m_activedir->port = cvs::narrow(tmp);
				}
				else
					m_activedir->port="";
				if(m_bCollapsed)
				{
					CRootSplitter split;
					if(split.Split(m_activedir->server.c_str()))
					{
						cvs::string keys;

						m_activedir->protocol = split.m_protocol;
						m_activedir->keywords = split.m_keywords;
						m_activedir->username = split.m_username;
						m_activedir->password = split.m_password;
						m_activedir->server = split.m_server;
						m_activedir->port = split.m_port;
						m_activedir->directory = split.m_directory;
						m_activedir->module = split.m_module;
						if(m_activedir->keywords.size())
						{
							keys=";"+m_activedir->keywords;
						}
						if(m_activedir->username.size())
							cvs::sprintf(m_activedir->root,80,":%s%s:%s%s%s@%s:%s:%s",m_activedir->protocol.c_str(),keys.c_str(),m_activedir->username.c_str(),m_activedir->password.size()?":":"",m_activedir->password.c_str(),m_activedir->server.c_str(),m_activedir->port.c_str(),m_activedir->directory.c_str());
						else
							cvs::sprintf(m_activedir->root,80,":%s%s:%s:%s:%s",m_activedir->protocol.c_str(),keys.c_str(),m_activedir->server.c_str(),m_activedir->port.c_str(),m_activedir->directory.c_str());
						m_activedir->enumerated = false;
						m_activedir->user = true;
					}
					else
					{
						CServerInfo inf;
						CServerInfo::remoteServerInfo rsi;
						if(!inf.getRemoteServerInfo(m_activedir->server.c_str(),rsi))
						{
							MessageBox(hWnd,_T("Server does not support automatic configuration.\nPlease choose a protocol manually."),_T("New server entry"),MB_ICONSTOP|MB_OK);
							return TRUE;
						}
						m_activedir->root="";
						m_activedir->protocol="";
						m_activedir->username="";
						m_activedir->password="";
						m_activedir->keywords="";
						m_activedir->directory="";
						m_activedir->module="";
						m_activedir->enumerated = false;
						m_activedir->user = true;
					}

				}
				else
				{
					GetDlgItemText(hWnd,IDC_PASSWORD,tmp,sizeof(tmp));
					if(_tcschr(tmp,'@') || _tcschr(tmp,':') || _tcschr(tmp,'"'))
					{
						MessageBox(hWnd,_T("Password contains invalid characters."),cvs::wide(m_szTitle.c_str()),MB_ICONSTOP|MB_OK);
						return 0;
					}
					m_activedir->password = cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_PROTOCOL,tmp,sizeof(tmp));
					m_activedir->protocol = cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_USERNAME,tmp,sizeof(tmp));
					m_activedir->username = cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_DIRECTORY,tmp,sizeof(tmp));
					if(tmp[0]!='/')
						m_activedir->directory="/";
					else
						m_activedir->directory="";
					m_activedir->directory += cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_MODULE,tmp,sizeof(tmp));
					m_activedir->module = cvs::narrow(tmp);
					GetDlgItemText(hWnd,IDC_KEYWORDS,tmp,sizeof(tmp));
					m_activedir->keywords = cvs::narrow(tmp);

					if(!strcmp(m_activedir->protocol.c_str(),"ftp"))
					{
						m_activedir->port = "";
						m_activedir->keywords = "";
						m_activedir->module = "";
					}

					cvs::string keys;
					if(m_activedir->keywords.size())
					{
						keys=";"+m_activedir->keywords;
					}
					if(m_activedir->username.size())
						cvs::sprintf(m_activedir->root,80,":%s%s:%s%s%s@%s:%s:%s",m_activedir->protocol.c_str(),keys.c_str(),m_activedir->username.c_str(),m_activedir->password.size()?":":"",m_activedir->password.c_str(),m_activedir->server.c_str(),m_activedir->port.c_str(),m_activedir->directory.c_str());
					else
						cvs::sprintf(m_activedir->root,80,":%s%s:%s:%s:%s",m_activedir->protocol.c_str(),keys.c_str(),m_activedir->server.c_str(),m_activedir->port.c_str(),m_activedir->directory.c_str());
					m_activedir->enumerated = false;
					m_activedir->user = true;
				}
				// Fall through
			case IDCANCEL:
				EndDialog(hWnd, wParam);
				return TRUE;
			case IDC_PROTOCOL:
				switch(HIWORD(wParam))
				{
					case CBN_DROPDOWN:
						GetProtocols(GetDlgItem(hWnd,IDC_PROTOCOL));
						return TRUE;
					case CBN_SELENDOK:
						GetDlgItemText(hWnd,IDC_PROTOCOL,tmp,sizeof(tmp));
						if(!_tcscmp(tmp,_T("automatic")))
						{
							if(!m_bCollapsed)
								Collapse();
						}
						else 
						{
							if(m_bCollapsed)
								Expand();
						}
						EnableOk();
						if(!_tcscmp(tmp,_T("ftp")))
						{
							EnableWindow(GetDlgItem(hWnd,IDC_MODULE),FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_MODULE_TEXT),FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS),FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS_TEXT),FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_PORT),FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_PORT_TEXT),FALSE);
							SetDlgItemText(hWnd,IDC_KEYWORDS,_T(""));
							SetDlgItemText(hWnd,IDC_PORT,_T(""));
							SetDlgItemText(hWnd,IDC_DIRECTORY_TEXT,_T("Directory"));
						}
						else
						{
							EnableWindow(GetDlgItem(hWnd,IDC_MODULE),m_flags&BRDShowModule?TRUE:FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_MODULE_TEXT),m_flags&BRDShowModule?TRUE:FALSE);
							EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS),TRUE);
							EnableWindow(GetDlgItem(hWnd,IDC_KEYWORDS_TEXT),TRUE);
							if(!_tcscmp(tmp,_T("ext")))
							{
								EnableWindow(GetDlgItem(hWnd,IDC_PORT),FALSE);
								EnableWindow(GetDlgItem(hWnd,IDC_PORT_TEXT),FALSE);
							}
							else
							{
								EnableWindow(GetDlgItem(hWnd,IDC_PORT),TRUE);
								EnableWindow(GetDlgItem(hWnd,IDC_PORT_TEXT),TRUE);
							}
							SetDlgItemText(hWnd,IDC_DIRECTORY_TEXT,_T("Repository"));
						}
						return TRUE;
				}
				break;
			}
			case IDC_TITLE:
			case IDC_SERVER:
			case IDC_DIRECTORY:
			case IDC_MODULE:
				switch(HIWORD(wParam))
				{
				case EN_CHANGE:
					EnableOk();
					break;
				}
				break;
			return 0;
	}
	return 0;
}

void CBrowserRootDialog::Collapse()
{
	RECT rect;
	int rectBtm;

	ShowWindow(GetDlgItem(m_hWnd,IDC_DIRECTORY),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_MODULE),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_USERNAME),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PASSWORD),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PORT),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_DIRECTORY_TEXT),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_MODULE_TEXT),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_USERNAME_TEXT),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PASSWORD_TEXT),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS_TEXT),SW_HIDE);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PORT_TEXT),SW_HIDE);

	GetWindowRect(GetDlgItem(m_hWnd,IDOK),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_splitPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDOK),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	MapWindowPoints(m_hWnd, NULL, (LPPOINT)&rect, 2);
	rectBtm = rect.bottom + 10;

	GetWindowRect(GetDlgItem(m_hWnd,IDCANCEL),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_splitPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDCANCEL),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	m_bCollapsed = true;

	GetWindowRect(m_hWnd, &rect);
	MoveWindow(m_hWnd, rect.left, rect.top, rect.right - rect.left, rectBtm - rect.top, TRUE);
}

void CBrowserRootDialog::Expand()
{
	RECT rect;
	int rectBtm;

	GetWindowRect(GetDlgItem(m_hWnd,IDOK),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_growPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDOK),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	MapWindowPoints(m_hWnd, NULL, (LPPOINT)&rect, 2);
	rectBtm = rect.bottom + 10;

	GetWindowRect(GetDlgItem(m_hWnd,IDCANCEL),&rect);
	MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rect, 2);
	OffsetRect(&rect,0,m_growPoint-rect.top);
	MoveWindow(GetDlgItem(m_hWnd,IDCANCEL),rect.left,rect.top,(rect.right-rect.left)/*+1*/,(rect.bottom-rect.top)/*+1*/,TRUE);

	ShowWindow(GetDlgItem(m_hWnd,IDC_DIRECTORY),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_MODULE),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_USERNAME),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PASSWORD),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PORT),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_DIRECTORY_TEXT),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_MODULE_TEXT),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_USERNAME_TEXT),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PASSWORD_TEXT),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_KEYWORDS_TEXT),SW_SHOW);
	ShowWindow(GetDlgItem(m_hWnd,IDC_PORT_TEXT),SW_SHOW);

	m_bCollapsed = false;

	GetWindowRect(m_hWnd, &rect);
	MoveWindow(m_hWnd, rect.left, rect.top, rect.right - rect.left, rectBtm - rect.top, TRUE);
}

void CBrowserRootDialog::GetProtocols(HWND hCombo)
{
	CServerInfo inf;
	CServerInfo::remoteServerInfo rsi;

	SendMessage(hCombo,CB_RESETCONTENT,0,0);
	if(!(m_flags&BRDRequireModule))
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("automatic"));
	if(m_flags&BRDShowFtp)
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ftp"));
	HCURSOR hOldCursor = GetCursor();
	SetCursor(LoadCursor(NULL,IDC_WAIT));
	if(!m_activedir->server.size() || !inf.getRemoteServerInfo(m_activedir->server.c_str(),rsi) || !rsi.protocols.size())
	{
		SetCursor(hOldCursor);
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("pserver"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("sserver"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("sspi"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ssh"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("gserver"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ext"));
	}
	else
	{
		SetCursor(hOldCursor);
		for(CServerInfo::remoteServerInfo::protocols_t::const_iterator i = rsi.protocols.begin(); i!=rsi.protocols.end(); ++i)
			SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)(LPCWSTR)cvs::wide(i->first.c_str()));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ssh"));
		SendMessage(hCombo,CB_ADDSTRING,0,(LPARAM)_T("ext"));
	}
	if(!m_activedir->protocol.size())
		SendMessage(hCombo,CB_SETCURSEL,0,0);
	else
		SendMessage(hCombo,CB_SETCURSEL,SendMessage(hCombo,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)(LPCWSTR)cvs::wide(m_activedir->protocol.c_str())),0);
}

void CBrowserRootDialog::EnableOk()
{
	BOOL bEnable = FALSE;
	int titleLen = (int)SendDlgItemMessage(m_hWnd,IDC_TITLE,WM_GETTEXTLENGTH,0,0);
	int serverLen = (int)SendDlgItemMessage(m_hWnd,IDC_SERVER,WM_GETTEXTLENGTH,0,0);
	int directoryLen = (int)SendDlgItemMessage(m_hWnd,IDC_DIRECTORY,WM_GETTEXTLENGTH,0,0);
	int moduleLen = (int)SendDlgItemMessage(m_hWnd,IDC_MODULE,WM_GETTEXTLENGTH,0,0);
	
	TCHAR tmp[32];
	SendDlgItemMessage(m_hWnd,IDC_PROTOCOL,WM_GETTEXT,sizeof(tmp)/sizeof(tmp[0]),(LPARAM)tmp);
	bool bFtp = !_tcscmp(tmp,_T("ftp"));
    
	if(titleLen && serverLen && (m_bCollapsed || (directoryLen && (bFtp || !(m_flags&BRDRequireModule) || moduleLen))))
		bEnable = TRUE;
	EnableWindow(GetDlgItem(m_hWnd,IDOK),bEnable);
}
