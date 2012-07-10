##########################################################################
# Variables

# Taget name
TARGET=TortoiseAct

# Binary file
BIN=TortoiseAct.exe

# Source files
SOURCE=						\
	Annotation.cpp				\
	ConflictParser.cpp			\
	DirectoryGroup.cpp			\
	StdAfx.cpp				\
	ThreadSafeProgress.cpp			\
	TortoiseAct.cpp				\
	TortoiseActVerbs.cpp			\
	TortoisePreferences.cpp			\
	TortoiseActUtils.cpp			\
	TortoiseParams.cpp			\
	TortoiseTip.cpp				\
	WebLog.cpp				\
	../DialogsWxw/AboutDialog.cpp		\
	../DialogsWxw/AddFilesDialog.cpp	\
	../DialogsWxw/AnnotateDialog.cpp	\
	../DialogsWxw/AnnotateListCtrl.cpp	\
	../DialogsWxw/BranchTagDlg.cpp		\
	../DialogsWxw/CheckoutDialog.cpp	\
	../DialogsWxw/CheckoutOptionsPage.cpp	\
	../DialogsWxw/CheckQueryDlg.cpp		\
	../DialogsWxw/ChooseFile.cpp		\
	../DialogsWxw/CommandDialog.cpp		\
	../DialogsWxw/CommitDialog.cpp		\
	../DialogsWxw/ConflictDialog.cpp	\
	../DialogsWxw/ConflictListDialog.cpp	\
	../DialogsWxw/CreditsDialog.cpp		\
	../DialogsWxw/CvsRootListCtrl.cpp	\
	../DialogsWxw/EditDialog.cpp		\
	../DialogsWxw/EditorListDialog.cpp	\
	../DialogsWxw/EditorListListCtrl.cpp	\
	../DialogsWxw/ExplorerMenu.cpp		\
	../DialogsWxw/ExtComboBox.cpp		\
	../DialogsWxw/ExtDialog.cpp		\
	../DialogsWxw/ExtListCtrl.cpp		\
	../DialogsWxw/ExtSplitterWindow.cpp	\
	../DialogsWxw/ExtStaticText.cpp		\
	../DialogsWxw/ExtTextCtrl.cpp		\
	../DialogsWxw/FilenameText.cpp		\
	../DialogsWxw/FindDialog.cpp		\
	../DialogsWxw/GraphDialog.cpp		\
	../DialogsWxw/GraphSettingsDialog.cpp	\
	../DialogsWxw/GraphLogData.cpp		\
	../DialogsWxw/HistoryDialog.cpp		\
	../DialogsWxw/HistoryTreeCtrl.cpp	\
	../DialogsWxw/IconSetDialog.cpp		\
	../DialogsWxw/LogConfigDialog.cpp	\
	../DialogsWxw/LoginDialog.cpp		\
	../DialogsWxw/MakeModuleBasicsPage.cpp	\
	../DialogsWxw/MakeModuleDialog.cpp	\
	../DialogsWxw/MakePatchDialog.cpp	\
	../DialogsWxw/MergeDlg.cpp		\
	../DialogsWxw/MessageDialog.cpp		\
	../DialogsWxw/ModuleBasicsPage.cpp	\
	../DialogsWxw/PreferencesDialog.cpp	\
	../DialogsWxw/ProgressDialog.cpp	\
	../DialogsWxw/ReleaseDialog.cpp		\
	../DialogsWxw/RenameDialog.cpp		\
	../DialogsWxw/RepoTreeCtrl.cpp		\
	../DialogsWxw/ResizeDialog.cpp		\
	../DialogsWxw/RevOptions.cpp		\
	../DialogsWxw/TagValidator.cpp		\
	../DialogsWxw/treelistctrl.cpp		\
	../DialogsWxw/UpdateDlg.cpp		\
	../DialogsWxw/wxInitialTipTextCtrl.cpp	\
	../DialogsWxw/WxwHelpers.cpp		\
	../DialogsWxw/WxwMain.cpp		\
	../DialogsWxw/YesNoDialog.cpp		\
	../DialogsWxw/YesNoAllDialog.cpp	\
	../CVSGlue/CVSAction.cpp		\
	../CVSGlue/CVSFeatures.cpp		\
	../CVSGlue/CVSStatus.cpp		\
	../CVSGlue/CVSStatusShared.cpp		\
	../CVSGlue/CVSEntries.cpp		\
	../CVSGlue/CvsIgnore.cpp		\
	../CVSGlue/CVSRoot.cpp			\
	../CVSGlue/getline.cpp			\
	../CVSGlue/ndir.cpp			\
	../CVSGlue/Stat.cpp			\
	../CVSGlue/ustr.cpp			\
	../CVSGlue/MakeArgs.cpp			\
	../CVSGlue/RemoteLists.cpp		\
	../CvsGui/cvsgui_process.cpp		\
	../CvsGui/cvsgui_wire.cpp		\
	../Utils/getdate.cpp			\
	../Utils/Cache.cpp			\
	../Utils/ClockRackCon.cpp		\
	../Utils/FileTree.cpp			\
	../Utils/HttpSnaffle.cpp		\
	../Utils/Keyboard.cpp			\
	../Utils/LangUtils.cpp			\
	../Utils/LineReader.cpp			\
	../Utils/LogUtils.cpp			\
	../Utils/MultiMonUtils.cpp		\
	../Utils/OsVersion.cpp			\
	../Utils/PathUtils.cpp			\
	../Utils/Preference.cpp			\
	../Utils/ProcessUtils.cpp		\
	../Utils/RepoUtils.cpp			\
	../Utils/Serialize.cpp			\
	../Utils/ShellUtils.cpp			\
	../Utils/SoundUtils.cpp			\
	../Utils/StringUtils.cpp		\
	../Utils/SyncUtils.cpp			\
	../Utils/TimeUtils.cpp			\
	../Utils/TortoiseException.cpp		\
	../Utils/TortoiseRegistry.cpp		\
	../Utils/TortoiseStatus.cpp		\
	../Utils/TortoiseUtils.cpp		\
	../Utils/UnicodeStrings.cpp		\
	../Utils/Wildcard.cpp
ifeq ($(DEBUG),1)
SOURCE+=../Utils/TortoiseDebug.cpp
endif

# Resource script
RESOURCE=TortoiseAct.rc 


# Compiler flags
CXXFLAGS=$(STLP_CFLAGS) $(WX_CFLAGS) -I . -I .. -Wall -fno-rtti -DWIN32 -D_MBCS \
	-D_WIN32_IE=0x0400 -O2 
ifeq ($(DEBUG),1)
CXXFLAGS+=-D_DEBUG 
else
CXXFLAGS+=-DNDEBUG 
endif


# Linker flags
LDFLAGS=$(WX_LDFLAGS) \
	-L"../cvstree/$(RELEASE)" -lcvstree $(STLP_LDFLAGS) -lole32 	\
	-lkernel32 -luser32 -lgdi32 -lshlwapi -lwininet -lwinmm 	\
	-Wl,--subsystem,windows -mwindows

ifneq ($(DEBUG),1)
LDFLAGS+=-s
endif


# Resource compiler flags
RCFLAGS=--include-dir "$(WXDIR)/include" --define __WIN32__ \
	--define __WIN95__ --define __GNUWIN32__ --define _WINRESRC_

# TortoiseCVS base dir
TORTOISE_SRC=..

# Include Make environment
include ../Makeenv

# Include Make program
include ../Makeprog

../Utils/getdate.cpp : ../Utils/getdate.y
	$(SED) -e "s/\r//" < $(BISON_PKGDATADIR)/$(BISON_SKEL) > $(BISON_PKGDATADIR)/$(BISON_SKEL_TMP) ; \
	export BISON_PKGDATADIR=$(BISON_PKGDATADIR) ; \
	export M4=$(M4) ; \
	$(BISON) -o getdate.c -d $< -S $(BISON_SKEL_TMP) 2>&1 ; \
	rm $(BISON_PKGDATADIR)/$(BISON_SKEL_TMP) ; \
	$(SED) -e "s/getdate\.c/getdate\.cpp/g" <  getdate.c > $@ ; \
	rm -f getdate.c

