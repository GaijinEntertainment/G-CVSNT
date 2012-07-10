/**
	\file
	Common Dialogs.

	\author
	CVSNT Helper application API
    Copyright (C) 2004-5 Tony Hoyle and March-Hare Software Ltd
*/
/*
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
#ifndef CVSCOMMONDIALOGS__H
#define CVSCOMMONDIALOGS__H

#include <commctrl.h>

/// Repository Browse Dialog.
/// Display a list of all cvs and (optionallly) ftp servers, and returns
/// a cvs root string and a description.
class CBrowserDialog : public CServerConnectionCallback
{
public:
	enum _CBDFlags
	{
		BDShowStatic		= 0x000001, ///< Show user defined servers
		BDShowLocal			= 0x000002, ///< Show local (intranet) discovered servers
		BDShowGlobal		= 0x000004, ///< Show global directory servers
		BDShowDirectories	= 0x000008, ///< Show all directory levels, not just module
		BDAllowAdd			= 0x000010, ///< Allow user to add new servers
		BDShowModule		= 0x000020, ///< Show module selection option

		BDShowFtp			= 0x000100, ///< Show defined FTP servers
		BDRequireModule		= 0x000200, ///< Require a module name to be entered
		BDShowTags			= 0x000400, ///< List available tags
		BDShowBranches		= 0x000800, ///< List available branches

		BDShowDefault		= 0x0000FF  ///< Show default attributes
	};

	/// \param hParent Parent window, or NULL
	CVSTOOLS_EXPORT CBrowserDialog(HWND hParent);
	CVSTOOLS_EXPORT virtual ~CBrowserDialog();

	/// Display the dialog.
	/// \param flags Elements of dialog to show.
	/// \param title Dialog title.
	/// \param statusbar Text of status bar.
	/// \return true if user clicked OK, false otherwise.
	CVSTOOLS_EXPORT bool ShowDialog(unsigned flags = BDShowDefault, const char *title = "Browse for repository", const char *statusbar="");

	/// Get the root from the previous call to ShowDialog.
	CVSTOOLS_EXPORT const char *getRoot() { return m_root.c_str(); }

	/// Get the description from the previous call to ShowDialog.
	CVSTOOLS_EXPORT const char *getDescription() { return m_description.c_str(); }

	/// Get the module from the previous call to ShowDialog.
	CVSTOOLS_EXPORT const char *getModule() { return m_module.c_str(); }

	/// Get the tag/branch from the previous call to ShowDialog.
	CVSTOOLS_EXPORT const char *getTag() { return m_tag.c_str(); }

private:
	cvs::string m_root,m_description,m_module,m_tag;
	unsigned m_flags;
	std::vector<cvs::string> m_dirList;
	std::map<cvs::string,int> m_tagList;
	int m_error;
	bool m_bTagList, m_bTagMode;
	cvs::string m_szTitle, m_szStatus;
	ServerConnectionInfo *m_activedir;
	HTREEITEM m_hLocalRoot,m_hGlobalRoot,m_hFtpRoot;

	HWND m_hParent;
	HWND m_hWnd;
	HWND m_hStatus;

	static INT_PTR CALLBACK _dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	BOOL OnInitDialog();
	bool OnOk();
	void OnNew();
	void OnSize(int nType, int x, int y);
	void OnNotify(int nId, LPNMHDR nmhdr);
	void OnItemExpanding(HWND hTree, LPNMTREEVIEW pnmtv);
	void OnSelChanged(HWND hTree, LPNMTREEVIEW pnmtv);
	void OnRClick(HWND hTree, LPNMHDR pnmhdr);

	HTREEITEM InsertTreeItem(HWND hTree, LPCTSTR szText, int nIcon, int nStateIcon, LPVOID param  = NULL, HTREEITEM hParentItem = TVI_ROOT);

	void PopulateLocal(HWND hTree, bool bStatic);
	void PopulateFtp(HWND hTree);
	void PopulateGlobal(HWND hTree);
	int BuildDirList(const char *data,size_t len);

	// CServerConnectionCallback
	virtual bool AskForPassword(ServerConnectionInfo *dir);
	virtual void ProcessOutput(const char *line);
	virtual void Error(ServerConnectionInfo *current, ServerConnectionError error);
};

/// Repository Browse Password Dialog.
/// Ask for a username/password and allow the user to change the
/// connection details if required.
class CBrowserPasswordDialog
{
public:
	/// \param hParent Parent window, or NULL
	CVSTOOLS_EXPORT CBrowserPasswordDialog(HWND hParent);
	CVSTOOLS_EXPORT virtual ~CBrowserPasswordDialog();

	/// Display the dialog.
	/// \param info Server details.
	/// \return true if user clicked OK, false otherwise.
	CVSTOOLS_EXPORT bool ShowDialog(ServerConnectionInfo *info);

private:
	ServerConnectionInfo *m_activedir;
	HWND m_hParent;
	HWND m_hWnd;
	bool m_bCollapsed;
	int m_splitPoint, m_growPoint;
	bool m_bcbFull;

	static INT_PTR CALLBACK _dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void Collapse();
	void Expand();
	void GetProtocols(HWND hCombo);
};

/// Repository Browse Root Dialog.
/// Ask for details of a new server or to modify the properties of an old one.
class CBrowserRootDialog
{
public:
	// Kept in sync with _CBDFlags where necessary
	enum _CBRDFlags
	{
		BRDShowModule		= 0x000020, ///< Show module selection option
		BRDRequireModule	= 0x000200, ///< Require a module/directory name
		BRDShowFtp			= 0x000100, ///< Show FTP as an available protocol
	};

	/// \param hParent Parent window, or NULL
	CVSTOOLS_EXPORT CBrowserRootDialog(HWND hParent);
	CVSTOOLS_EXPORT virtual ~CBrowserRootDialog();

	/// Display the dialog.
	/// \param info Server details.
	/// \param title Dialog title.
	/// \param flags Flags from _CBRDFlags which control behaviour
	/// \return true if user clicked OK, false otherwise
	CVSTOOLS_EXPORT bool ShowDialog(ServerConnectionInfo *info, const char *title = "Add User defined root", unsigned flags = 0);

private:
	ServerConnectionInfo *m_activedir;
	HWND m_hParent;
	HWND m_hWnd;
	bool m_bCollapsed;
	int m_splitPoint, m_growPoint;
	unsigned m_flags;
	cvs::string m_szTitle;

	static INT_PTR CALLBACK _dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	INT_PTR dlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void Collapse();
	void Expand();
	void GetProtocols(HWND hCombo);
	void EnableOk();
};

#endif