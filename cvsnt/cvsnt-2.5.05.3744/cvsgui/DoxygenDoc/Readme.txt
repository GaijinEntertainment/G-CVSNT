Each platform has the specific target: Win32, Mac and Unix. 
Target can be specified as a command line script parameter.
Target options:
	o None - default for a given platform
	o Win32, Win, W or w - Win32
	o Mac, M or m - Mac
	o Unix, U or u - Unix
	o Any other parameter will simply display this readme file

You can create the documentation using the doxygen.
To get the DOXYGEN go to:
http://www.doxygen.org

For nice graphs you will also need a dot tool.
You can get GRAPHVIZ from: 
http://www.research.att.com/sw/tools/graphviz/

On MacOS X, you can use fink to install the doxygen & related tools.
See <http://fink.sourceforge.net/> for details on fink.
You may want to use FinkCommander as a GUI frontend for fink: http://finkcommander.sourceforge.net/

On Windows to create the compressed html file you need the html help compiler.
Get HHC from: 
http://msdn.microsoft.com/library/default.asp?url=/library/en-us/htmlhelp/html/hwMicrosoftHTMLHelpDownloads.asp
or try MSDN search if the link will break again:
http://msdn.microsoft.com/

Notes on the configuration file (Doxyfile):
You should run the script/batch file rather than running doxygen directly.

The script should set certain variables that are used by configuration file.
The names are corresponding to the configuration names but have the DOX_ prefix added. 

The variables are:
  1. DOX_PROJECT_NAME - The document name displayed on the main page (eg. cvsgui protocol).
  2. DOX_PROJECT_VERSION - The document version displayed on the main page (eg. 1.0).
  3. DOX_OUTPUT_DIRECTORY - The resulting documentation directory.
  4. DOX_PREDEFINED - The platform-specific predefined tags (WIN32, qUnix, macintosh etc.).
  5. DOX_INCLUDE_PATH - Includes directories, empty for now
  6. DOX_INPUT - The input files and directories, typically up-level directory: ..
  7. DOX_STRIP_FROM_PATH should contain the expanded form of the up-level directory name. 
     The expanded string is then used by doxygen to strip from the file names. 
  8. DOX_FILE_PATTERNS - The input file patterns, can be left blank.
  9. DOX_CASE_SENSE_NAMES - Case-sensitive file names. Set to NO on Windows.
 10. DOX_SHORT_NAMES - Short (and cryptic) or long (more descrptive) file names. Use short on Mac and Windows, 
     unless you want to place HTML output as a part of the website
 11. DOX_GENERATE_* - Turn on or off specific output type (like HTML, RTF, man, Latex or XML)
 12. DOX_SEARCHENGINE - Specifies whether or not a search engine should be used, yes if you want to
     use the html output on the website and add online search capacity
 13. DOX_EXCLUDE_PATTERNS - The exclude file pattern, typicaly all CVS-paths should be ignored (*/CVS/*)
 
Platforms:
   Windows (Win32):
     1. Run the batch file named: DoDox.bat.
     2. The resulting documentation is sent to directory: cvsgui_protocol_WinDoc
     3. The output of the batch file execution is sent to: DoDox.log
     4. The doxygen warnings/errors are send to: Warnings.log
     5. Please note that you might have to hard-code the strip path as not all
        Windows versions support the current directory variable.

   MacOS X (Mac):
     1. Run the script named: DoDox.sh.
     2. The resulting documentation is sent to directory: cvsgui_protocol_MacDoc
     3. The output of the script execution is sent to: DoDox.log
     4. The doxygen warnings/errors are send to: Warnings.log

   Linux (Unix):
     1. Run the script named: DoDox.sh.
     2. The resulting documentation is sent to directory: cvsgui_protocol_UniDoc
     3. The output of the script execution is sent to: DoDox.log
     4. The doxygen warnings/errors are send to: Warnings.log
