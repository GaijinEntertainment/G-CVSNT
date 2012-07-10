Hello!  Welcome to TortoiseCVS.  This file gives an overview of the
source code.

TortoiseAct/
  .exe to perform TortoiseCVS actions (the main app).  Uses wxWidgets
  for its user interface, and calls cvs2ntlib.dll to call CVS.

TortoiseShell/
  .dll Explorer shell extension. Displays CVS status in Explorer, adds
  context menus, and invokes TortoiseAct to do the work.

TortoiseLib/
  .lib used by TortoiseAct and TortoiseShell.

ContextMenus/
  Menu description code used by TortoiseShell

CVSGlue/
  Code to actually run CVS commands, and check status of files in CVS

cvstree/
  Parser for "cvs log" output.

cvsgui/
  Library for communicating with CVS.

CvsNt/
  CVSNT executables and DLLs.

DialogsWxw/
  GUI code used by TortoiseAct.  Uses wxWidgets API for ease of coding.

Icons/
  The overlay icons that show CVS status in Explorer

Utils/
  Assorted utility classes

patch/
  Patch

PostInst/
  Postinstaller, for setting up various things after installation.

TortoisePlink/
  Slightly modified version of plink from the PuTTY suite.

TortoiseSetupHelper/
  Used by installer and uninstaller for terminating Explorer.

TortoiseSpawn/
  Helper for executing CVSNT; necessary for 'Abort' button to work properly.

Test/
  Unit tests.
