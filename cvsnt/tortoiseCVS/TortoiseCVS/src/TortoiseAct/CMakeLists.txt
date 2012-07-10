# TortoiseCVS - a Windows shell extension for easy version control
#
# Copyright (C) 2008 - Torsten Martinsen
# <torsten@bullestock.net> - June 2008
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

PROJECT(TortoiseAct)

SET(USE_UNICODE 1)

INCLUDE_DIRECTORIES(
   ${WX_INCLUDE_PATH}
   ..
   .
)

ADD_DEFINITIONS(
   
)

SET(CPP_SOURCE
   ../DialogsWxw/AboutDialog.cpp
   ../DialogsWxw/AddFilesDialog.cpp
   ../DialogsWxw/AnnotateDialog.cpp
   ../DialogsWxw/AnnotateListCtrl.cpp
   ../DialogsWxw/BranchTagDlg.cpp
   ../DialogsWxw/CheckQueryDlg.cpp
   ../DialogsWxw/CheckoutDialog.cpp
   ../DialogsWxw/CheckoutOptionsPage.cpp
   ../DialogsWxw/ChooseFile.cpp
   ../DialogsWxw/CommandDialog.cpp
   ../DialogsWxw/CommitDialog.cpp
   ../DialogsWxw/ConflictListDialog.cpp
   ../DialogsWxw/CreditsDialog.cpp
   ../DialogsWxw/CvsRootListCtrl.cpp
   ../DialogsWxw/EditDialog.cpp
   ../DialogsWxw/EditorListDialog.cpp
   ../DialogsWxw/EditorListListCtrl.cpp
   ../DialogsWxw/ExplorerMenu.cpp
   ../DialogsWxw/ExtComboBox.cpp
   ../DialogsWxw/ExtDialog.cpp
   ../DialogsWxw/ExtListCtrl.cpp
   ../DialogsWxw/ExtSplitterWindow.cpp
   ../DialogsWxw/ExtStaticText.cpp
   ../DialogsWxw/ExtTextCtrl.cpp
   ../DialogsWxw/FilenameText.cpp
   ../DialogsWxw/FindDialog.cpp
   ../DialogsWxw/GraphDialog.cpp
   ../DialogsWxw/GraphLogData.cpp
   ../DialogsWxw/GraphSettingsDialog.cpp
   ../DialogsWxw/HistoryDialog.cpp
   ../DialogsWxw/HistoryTreeCtrl.cpp
   ../DialogsWxw/IconSetDialog.cpp
   ../DialogsWxw/InfoPanel.cpp
   ../DialogsWxw/LogConfigDialog.cpp
   ../DialogsWxw/LoginDialog.cpp
   ../DialogsWxw/MakeModuleBasicsPage.cpp
   ../DialogsWxw/MakeModuleDialog.cpp
   ../DialogsWxw/MakePatchDialog.cpp
   ../DialogsWxw/MergeDlg.cpp
   ../DialogsWxw/MessageDialog.cpp
   ../DialogsWxw/ModuleBasicsPage.cpp
   ../DialogsWxw/ModuleBasicsPageEventHandler.cpp
   ../DialogsWxw/PreferencesDialog.cpp
   ../DialogsWxw/ProgressDialog.cpp
   ../DialogsWxw/ReleaseDialog.cpp
   ../DialogsWxw/RenameDialog.cpp
   ../DialogsWxw/RepoTreeCtrl.cpp
   ../DialogsWxw/ResizeDialog.cpp
   ../DialogsWxw/RevOptions.cpp
   ../DialogsWxw/SwitchDialog.cpp
   ../DialogsWxw/TagValidator.cpp
   ../DialogsWxw/UpdateDlg.cpp
   ../DialogsWxw/WxwHelpers.cpp
   ../DialogsWxw/WxwMain.cpp
   ../DialogsWxw/YesNoAllDialog.cpp
   ../DialogsWxw/YesNoDialog.cpp
   ../DialogsWxw/treelistctrl.cpp
   ../DialogsWxw/wxInitialTipTextCtrl.cpp
   ../DialogsWxw/AboutDialog.h
   ../DialogsWxw/AddFilesDialog.h
   ../DialogsWxw/AnnotateDialog.h
   ../DialogsWxw/AnnotateListCtrl.h
   ../DialogsWxw/BranchTagDlg.h
   ../DialogsWxw/CheckoutDialog.h
   ../DialogsWxw/CheckoutOptionsPage.h
   ../DialogsWxw/CheckQueryDlg.h
   ../DialogsWxw/ChooseFile.h
   ../DialogsWxw/CommandDialog.h
   ../DialogsWxw/CommitDialog.h
   ../DialogsWxw/ConflictDialog.h
   ../DialogsWxw/ConflictListDialog.h
   ../DialogsWxw/CreditsDialog.h
   ../DialogsWxw/CvsRootListCtrl.h
   ../DialogsWxw/EditDialog.h
   ../DialogsWxw/EditorListDialog.h
   ../DialogsWxw/EditorListListCtrl.h
   ../DialogsWxw/ExplorerMenu.h
   ../DialogsWxw/ExtComboBox.h
   ../DialogsWxw/ExtDialog.h
   ../DialogsWxw/ExtListCtrl.h
   ../DialogsWxw/ExtSplitterWindow.h
   ../DialogsWxw/ExtStaticText.h
   ../DialogsWxw/ExtTextCtrl.h
   ../DialogsWxw/FilenameText.h
   ../DialogsWxw/FindDialog.h
   ../DialogsWxw/GraphDialog.h
   ../DialogsWxw/GraphLogData.h
   ../DialogsWxw/GraphSettingsDialog.h
   ../DialogsWxw/HistoryDialog.h
   ../DialogsWxw/HistoryTreeCtrl.h
   ../DialogsWxw/IconSetDialog.h
   ../DialogsWxw/InfoPanel.h
   ../DialogsWxw/LogConfigDialog.h
   ../DialogsWxw/LoginDialog.h
   ../DialogsWxw/MakeModuleBasicsPage.h
   ../DialogsWxw/MakeModuleDialog.h
   ../DialogsWxw/MakePatchDialog.h
   ../DialogsWxw/MergeDlg.h
   ../DialogsWxw/MessageDialog.h
   ../DialogsWxw/ModuleBasicsPage.h
   ../DialogsWxw/ModuleBasicsPageEventHandler.h
   ../DialogsWxw/PreferencesDialog.h
   ../DialogsWxw/ProgressDialog.h
   ../DialogsWxw/ReleaseDialog.h
   ../DialogsWxw/RenameDialog.h
   ../DialogsWxw/RepoTreeCtrl.h
   ../DialogsWxw/ResizeDialog.h
   ../DialogsWxw/RevOptions.h
   ../DialogsWxw/SwitchDialog.h
   ../DialogsWxw/TagValidator.h
   ../DialogsWxw/treelistctrl.h
   ../DialogsWxw/UpdateDlg.h
   ../DialogsWxw/wxInitialTipTextCtrl.h
   ../DialogsWxw/WxwHelpers.h
   ../DialogsWxw/WxwMain.h
   ../DialogsWxw/YesNoAllDialog.h
   ../DialogsWxw/YesNoDialog.h
   ../cvsgui/cvsgui_process.cpp
   ../cvsgui/cvsgui_wire.cpp
   Annotation.cpp
   Annotation.h
   ConflictParser.cpp
   ConflictParser.h
   DirectoryGroup.cpp
   DirectoryGroup.h
   StdAfx.cpp
   ThreadSafeProgress.cpp
   ThreadSafeProgress.h
   TortoiseAct.cpp
   TortoiseAct.h
   TortoiseActUtils.cpp
   TortoiseActUtils.h
   TortoiseActVerbs.cpp
   TortoiseActVerbs.h
   TortoiseParams.cpp
   TortoiseParams.h
   TortoisePreferences.cpp
   TortoisePreferences.h
   TortoiseTip.cpp
   TortoiseTip.h
   WebLog.cpp
   WebLog.h
)

SET(PRECOMP_SOURCE ${CPP_SOURCE})

SOURCE_GROUP(CVSGui
    REGULAR_EXPRESSION ../cvsgui/.*)

SOURCE_GROUP(Dialogs
    REGULAR_EXPRESSION ../DialogsWxw/.*)

LINK_LIBRARIES(
    CVSGlue
    Utils
)

SET_CFLAGS(TortoiseAct)

LINK_DIRECTORIES(
    /cvsbin/lib/
)

LINK_LIBRARIES(
    cvstree
    CVSGlue
    Utils
    ${WX_LIBS}
    comctl32.lib
    rpcrt4.lib
    shlwapi.lib
    wininet.lib
    winmm.lib
    ws2_32.lib
    gdiplus.lib
    ${CVSAPILIB}
)

ADD_EXECUTABLE(TortoiseAct WIN32 ${STDAFX_SOURCE} ${C_SOURCE} ${CPP_SOURCE} TortoiseAct.rc)

SET_TARGET_PROPERTIES(TortoiseAct PROPERTIES LINK_FLAGS_RELEASE "/NODEFAULTLIB:wxbase28u_net.lib /NODEFAULTLIB:wxbase28u_xml.lib /NODEFAULTLIB:wxexpat.lib /NODEFAULTLIB:wxjpeg.lib /NODEFAULTLIB:wxmsw28u_aui.lib /NODEFAULTLIB:wxmsw28u_html.lib /NODEFAULTLIB:wxmsw28u_media.lib /NODEFAULTLIB:wxmsw28u_qa.lib /NODEFAULTLIB:wxmsw28u_richtext.lib /NODEFAULTLIB:wxmsw28u_xrc.lib /NODEFAULTLIB:wxpng.lib /NODEFAULTLIB:wxregexu.lib /NODEFAULTLIB:wxtiff.lib /NODEFAULTLIB:wxzlib.lib")
SET_TARGET_PROPERTIES(TortoiseAct PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:wxbase28ud_net.lib /NODEFAULTLIB:wxbase28ud_xml.lib /NODEFAULTLIB:wxexpatd.lib /NODEFAULTLIB:wxjpegd.lib /NODEFAULTLIB:wxmsw28ud_aui.lib /NODEFAULTLIB:wxmsw28ud_html.lib /NODEFAULTLIB:wxmsw28ud_media.lib /NODEFAULTLIB:wxmsw28ud_qa.lib /NODEFAULTLIB:wxmsw28ud_richtext.lib /NODEFAULTLIB:wxmsw28ud_xrc.lib /NODEFAULTLIB:wxpngd.lib /NODEFAULTLIB:wxregexud.lib /NODEFAULTLIB:wxtiffd.lib /NODEFAULTLIB:wxzlibd.lib")

GET_FILENAME_COMPONENT(TORTOISE_ROOTDIR  "[HKEY_LOCAL_MACHINE\\SOFTWARE\\TortoiseCVS;RootDir]" ABSOLUTE CACHE)

ADD_CUSTOM_COMMAND(
   TARGET TortoiseAct
   POST_BUILD
   COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/TortoiseAct.exe" "${TORTOISE_ROOTDIR}"
)
