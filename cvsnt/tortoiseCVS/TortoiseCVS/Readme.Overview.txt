Hello!  Welcome to TortoiseCVS.

build/
  Used to make binary releases of TortoiseCVS.  Includes a batch file
  to do some of the work, and binary copies of various files that 
  TortoiseCVS requires.

po/
  Translations of text into different languages.

  Note: This folder is now a separate module. Check it out in the same
  folder where your TortoiseCVS folder is, then the build script will
  find it automatically.

src/
  The source code, with project and make files.

web/
  The TortoiseCVS web page, help files and change log.  There is
  some developer information here.

wxw/
  Precompiled binaries of wxWidgets, for your convenience.  The
  Visual C++ TortoiseCVS project will look first in a top level
  directory called wxWindows (in the same folder as this 
  TortoiseCVS code tree), and failing that will use the files in
  here.

docs/
  The User's Manual.

admin/
  General project administration files, such as publicity ideas
  and template case study questions.

tools/
  Tools - mainly Cygwin tools - that are required for compiling 
  and translating TortoiseCVS.
  

