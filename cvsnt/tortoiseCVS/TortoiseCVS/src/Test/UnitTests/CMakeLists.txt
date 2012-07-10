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

PROJECT(UnitTests)

SET(USE_UNICODE 1)

INCLUDE_DIRECTORIES(
   ${WX_INCLUDE_PATH}
   ../../../tools/cppunit/include
   ../..
   .
)

ADD_DEFINITIONS(
   
)

SET(CPP_SOURCE
   ../../DialogsWxw/AboutDialog.cpp
   ../../DialogsWxw/AddFilesDialog.cpp
   ../../DialogsWxw/AnnotateDialog.cpp
   ../../DialogsWxw/AnnotateListCtrl.cpp
   ../../DialogsWxw/BranchTagDlg.cpp
   ../../DialogsWxw/CheckQueryDlg.cpp
   ../../DialogsWxw/CheckoutDialog.cpp
   ../../DialogsWxw/CheckoutOptionsPage.cpp
   ../../DialogsWxw/ChooseFile.cpp
   ../../DialogsWxw/CommandDialog.cpp
   ../../DialogsWxw/CommitDialog.cpp
   ../../DialogsWxw/ConflictDialog.cpp
   ../../DialogsWxw/ConflictListDialog.cpp
   ../../DialogsWxw/CreditsDialog.cpp
   ../../DialogsWxw/CvsRootListCtrl.cpp
   ../../DialogsWxw/EditDialog.cpp
   ../../DialogsWxw/EditorListDialog.cpp
   ../../DialogsWxw/EditorListListCtrl.cpp
   ../../DialogsWxw/ExplorerMenu.cpp
   ../../DialogsWxw/ExtComboBox.cpp
   ../../DialogsWxw/ExtDialog.cpp
   ../../DialogsWxw/ExtListCtrl.cpp
   ../../DialogsWxw/ExtSplitterWindow.cpp
   ../../DialogsWxw/ExtStaticText.cpp
   ../../DialogsWxw/ExtTextCtrl.cpp
   ../../DialogsWxw/FilenameText.cpp
   ../../DialogsWxw/FindDialog.cpp
   ../../DialogsWxw/GraphDialog.cpp
   ../../DialogsWxw/GraphLogData.cpp
   ../../DialogsWxw/GraphSettingsDialog.cpp
   ../../DialogsWxw/HistoryDialog.cpp
   ../../DialogsWxw/HistoryTreeCtrl.cpp
   ../../DialogsWxw/IconSetDialog.cpp
   ../../DialogsWxw/InfoPanel.cpp
   ../../DialogsWxw/LogConfigDialog.cpp
   ../../DialogsWxw/LoginDialog.cpp
   ../../DialogsWxw/MakeModuleBasicsPage.cpp
   ../../DialogsWxw/MakeModuleDialog.cpp
   ../../DialogsWxw/MakePatchDialog.cpp
   ../../DialogsWxw/MergeDlg.cpp
   ../../DialogsWxw/MessageDialog.cpp
   ../../DialogsWxw/ModuleBasicsPage.cpp
   ../../DialogsWxw/ModuleBasicsPageEventHandler.cpp
   ../../DialogsWxw/PreferencesDialog.cpp
   ../../DialogsWxw/ProgressDialog.cpp
   ../../DialogsWxw/ReleaseDialog.cpp
   ../../DialogsWxw/RenameDialog.cpp
   ../../DialogsWxw/RepoTreeCtrl.cpp
   ../../DialogsWxw/ResizeDialog.cpp
   ../../DialogsWxw/RevOptions.cpp
   ../../DialogsWxw/SwitchDialog.cpp
   ../../DialogsWxw/TagValidator.cpp
   ../../DialogsWxw/UpdateDlg.cpp
   ../../DialogsWxw/WxwHelpers.cpp
   ../../DialogsWxw/YesNoAllDialog.cpp
   ../../DialogsWxw/YesNoDialog.cpp
   ../../DialogsWxw/treelistctrl.cpp
   ../../DialogsWxw/wxInitialTipTextCtrl.cpp
   ../../TortoiseAct/Annotation.cpp
   ../../TortoiseAct/ConflictParser.cpp
   ../../TortoiseAct/DirectoryGroup.cpp
   ../../TortoiseAct/ThreadSafeProgress.cpp
   ../../TortoiseAct/TortoiseAct.cpp
   ../../TortoiseAct/TortoiseActUtils.cpp
   ../../TortoiseAct/TortoiseActVerbs.cpp
   ../../TortoiseAct/TortoiseParams.cpp
   ../../TortoiseAct/TortoisePreferences.cpp
   ../../TortoiseAct/TortoiseTip.cpp
   ../../TortoiseAct/WebLog.cpp
   ../../cvsgui/cvsgui_process.cpp
   ../../cvsgui/cvsgui_wire.cpp
   ../WxWMainStub.cpp
   StdAfx.cpp
   Suite.cpp
   TestCvsLogParser.cpp
   TestCVSRoot.cpp
   TestCVSStatus.cpp
   TestDirList.cpp
   TestDirectoryGroups.cpp
   TestTortoiseActUtils.cpp
   main.cpp
)

SET(PRECOMP_SOURCE ${CPP_SOURCE})

LINK_LIBRARIES(
    CVSGlue
    cvstree
    Utils
    cppunit
    ${WX_LIBS}
    comctl32.lib
    rpcrt4.lib
    shlwapi.lib
    wininet.lib
    winmm.lib
    ws2_32.lib
    gdiplus.lib
)

SET_CFLAGS(UnitTests)

ADD_EXECUTABLE(UnitTests ${STDAFX_SOURCE} ${CPP_SOURCE})

SET_TARGET_PROPERTIES(UnitTests PROPERTIES LINK_FLAGS_RELEASE "/NODEFAULTLIB:wxbase28u_net.lib /NODEFAULTLIB:wxbase28u_xml.lib /NODEFAULTLIB:wxexpat.lib /NODEFAULTLIB:wxjpeg.lib /NODEFAULTLIB:wxmsw28u_aui.lib /NODEFAULTLIB:wxmsw28u_html.lib /NODEFAULTLIB:wxmsw28u_media.lib /NODEFAULTLIB:wxmsw28u_qa.lib /NODEFAULTLIB:wxmsw28u_richtext.lib /NODEFAULTLIB:wxmsw28u_xrc.lib /NODEFAULTLIB:wxpng.lib /NODEFAULTLIB:wxregexu.lib /NODEFAULTLIB:wxtiff.lib /NODEFAULTLIB:wxzlib.lib")
SET_TARGET_PROPERTIES(UnitTests PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:wxbase28ud_net.lib /NODEFAULTLIB:wxbase28ud_xml.lib /NODEFAULTLIB:wxexpatd.lib /NODEFAULTLIB:wxjpegd.lib /NODEFAULTLIB:wxmsw28ud_aui.lib /NODEFAULTLIB:wxmsw28ud_html.lib /NODEFAULTLIB:wxmsw28ud_media.lib /NODEFAULTLIB:wxmsw28ud_qa.lib /NODEFAULTLIB:wxmsw28ud_richtext.lib /NODEFAULTLIB:wxmsw28ud_xrc.lib /NODEFAULTLIB:wxpngd.lib /NODEFAULTLIB:wxregexud.lib /NODEFAULTLIB:wxtiffd.lib /NODEFAULTLIB:wxzlibd.lib")
